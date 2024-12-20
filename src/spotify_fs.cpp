#include "spotify_fs.h"
#include "spotify_api.h"
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <unistd.h>
#include <iostream>
std::unordered_map<std::string, struct spotify_file *> SpotifyFileSystem::files;

void SpotifyFileSystem::init() {
  SpotifyFileSystem::cleanup();
  std::vector<Playlist> playlists =
      SpotifyAPI::getInstance()->getAllPlaylists();
  for (const auto &playlist : playlists) {
    auto pl = new spotify_file();
    pl->id = playlist.id;
    pl->name = playlist.name;
    pl->is_playlist = true;
    files["/" + pl->name] = pl;
    
    // Load tracks for this playlist
    std::vector<Track> tracks = SpotifyAPI::getInstance()->getPlaylistTracks(playlist.id);
    for (const auto &track : tracks) {
      auto track_file = new spotify_file();
      track_file->id = track.id;
      track_file->name = track.artist + " -- " + track.name;
      track_file->is_playlist = false;
      track_file->duration_ms = track.duration_ms;
      track_file->uri = track.uri;
      // Store track with path: /playlist_name/track_name
      files["/" + pl->name + "/" + track_file->name] = track_file;
      
    }
  }
}

int SpotifyFileSystem::getFileAttributes(const char *path, struct stat *stbuf) {
  memset(stbuf, 0, sizeof(struct stat));
  

  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0777;
    stbuf->st_nlink = 2;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_mtime = time(NULL);
    return 0;
  }

  auto it = files.find(path);
  if (it != files.end()) {
    if (it->second->is_playlist) {
      stbuf->st_mode = S_IFDIR | 0777;
      stbuf->st_nlink = 2;
    } else {
      stbuf->st_mode = S_IFREG | 0666;
      stbuf->st_nlink = 1;
      stbuf->st_size = it->second->duration_ms > 0 ? it->second->duration_ms : 1024;
    }
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_mtime = time(NULL);
    return 0;
  }

  return -ENOENT;
}

int SpotifyFileSystem::listFiles(const char *path, void *buf,
                                 fuse_fill_dir_t filler, off_t offset,
                                 struct fuse_file_info *fi) {
  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  if (strcmp(path, "/") == 0) {
    // List all playlists in root
    for (const auto &pair : files) {
      if (pair.second->is_playlist) {
        std::string name = pair.second->name;
        filler(buf, name.c_str(), NULL, 0);
      }
    }
  } else {
    // List tracks in a playlist
    for (const auto &pair : files) {
      std::string parent_path = std::string(path) + "/";
      if (pair.first.find(parent_path) == 0 && !pair.second->is_playlist) {
        std::string name = pair.second->name;
        filler(buf, name.c_str(), NULL, 0);
      }
    }
  }
  return 0;
}

int SpotifyFileSystem::openFile(const char *path, struct fuse_file_info *fi) {
  auto it = files.find(path);
  if (it == files.end() || it->second->is_playlist) {
    return -ENOENT;
  }
  return 0;
}

int SpotifyFileSystem::readFile(const char *path, char *buf, size_t size,
                                off_t offset, struct fuse_file_info *fi) {
  auto it = files.find(path);
  if (it == files.end() || it->second->is_playlist) {
    return -ENOENT;
  }

  // Skip opening Spotify if we're in file creation or if it's a hidden file
  std::string path_str(path);
  std::string filename = path_str.substr(path_str.find_last_of('/') + 1);
  if (filename[0] != '.') {  // Only open Spotify for non-hidden files
    std::string uri = it->second->uri;
    std::string command;
    
    #ifdef __APPLE__
      command = "open spotify:track:" + uri;
    #else
      command = "xdg-open spotify:track:" + uri;
    #endif
    
    system(command.c_str());
  }

  std::string content = "Opening track in Spotify...\n";
  if (offset >= content.size()) {
    return 0;
  }

  size_t len = content.size() - offset;
  if (len > size) {
    len = size;
  }
  memcpy(buf, content.c_str() + offset, len);
  return len;
}

int SpotifyFileSystem::createFolder(const char *path, mode_t mode) {
  std::string name = std::string(path).substr(1); // Remove leading '/'
  bool success = SpotifyAPI::getInstance()->createPlaylist(name, "Created via SpotifyFS", true);
  if (!success) {
    return -EACCES;
  }
  
  auto pl = new spotify_file();
  pl->name = name;
  pl->is_playlist = true;
  files[path] = pl;
  return 0;
}

int SpotifyFileSystem::createFile(const char *path, mode_t mode,
                                  struct fuse_file_info *fi) {
  // Ignore macOS metadata files
  std::string path_str(path);
  if (path_str.find("/._") != std::string::npos) {
    return -EACCES;  // Deny creation of ._ files
  }
  
  auto existing_file = files.find(path);
  if (existing_file != files.end()) {
    return -EEXIST;
  }

  std::string filename = path_str.substr(path_str.find_last_of('/') + 1);
  
  // URL encode spaces in the search query
  std::string track_query = filename;
  size_t pos = 0;
  while ((pos = track_query.find(" ", pos)) != std::string::npos) {
    track_query.replace(pos, 1, "%20");
    pos += 3;
  }

  // Find the playlist
  auto playlist_it = files.find(path_str.substr(0, path_str.find_last_of('/')));
  if (playlist_it == files.end() || !playlist_it->second->is_playlist) {
    return -ENOENT;
  }

  // Search for track
  SpotifyAPI* api = SpotifyAPI::getInstance();
  std::string track_id = api->searchTrack(track_query);
  if (track_id.empty()) {
    return -ENOENT;
  }

  // Get track info
  Track track = api->getTrackInfo(track_id);
  if (track.id.empty()) {
    return -ENOENT;
  }

  // Add track to playlist
  bool success = api->addTrackToPlaylist(playlist_it->second->id, track.id);
  if (!success) {
    return -EACCES;
  }

  // Create file entry
  auto file = new spotify_file();
  file->id = track.id;
  file->name = track.artist + " -- " + track.name;
  file->is_playlist = false;
  file->duration_ms = track.duration_ms;
  file->uri = track.uri;
  files[path] = file;

  return 0;
}

int SpotifyFileSystem::removeFile(const char *path) {
  auto it = files.find(path);
  if (it == files.end()) {
    return -ENOENT;
  }

  if (it->second->is_playlist) {
    // TODO: Add deletePlaylist to SpotifyAPI
    return -EACCES;
  } else {
    // Find parent playlist
    std::string path_str(path);
    size_t slash_pos = path_str.find_last_of('/');
    std::string playlist_path = path_str.substr(0, slash_pos);
    
    auto playlist_it = files.find(playlist_path);
    if (playlist_it == files.end() || !playlist_it->second->is_playlist) {
      return -ENOENT;
    }

    bool success = SpotifyAPI::getInstance()->removeTrackFromPlaylist(
      playlist_it->second->id, it->second->uri);
    if (!success) {
      return -EACCES;
    }
  }

  delete it->second;
  files.erase(it);
  return 0;
}

int SpotifyFileSystem::cleanup() {
  for (auto &pair : files) {
    delete pair.second;
  }
  files.clear();
  return 0;
}

int SpotifyFileSystem::writeFile(const char *path, const char *buf, size_t size,
                                 off_t offset, struct fuse_file_info *fi) {
  auto it = files.find(path);
  if (it == files.end() || it->second->is_playlist) {
    return -ENOENT;
  }

  // For now, just acknowledge the write
  return size;
}

int SpotifyFileSystem::truncateFile(const char *path, off_t size) {
  auto it = files.find(path);
  if (it == files.end() || it->second->is_playlist) {
    return -ENOENT;
  }
  return 0;
}
