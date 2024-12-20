#include "spotify_fs.h"
#include "spotify_api.h"
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fuse/fuse_lowlevel.h>
#include <iostream>
#include <thread>
#include <unistd.h>

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
    std::vector<Track> tracks =
        SpotifyAPI::getInstance()->getPlaylistTracks(playlist.id);
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
      stbuf->st_size =
          it->second->duration_ms > 0 ? it->second->duration_ms : 1024;
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
        // Use original name for playlists
        std::string name = pair.second->name;
        filler(buf, name.c_str(), NULL, 0);
      }
    }
  } else {
    // List tracks in a playlist
    for (const auto &pair : files) {
      std::string parent_path = std::string(path) + "/";
      if (pair.first.find(parent_path) == 0 && !pair.second->is_playlist) {
        // Use the original filename instead of the formatted name
        std::string filename =
            pair.first.substr(pair.first.find_last_of('/') + 1);
        filler(buf, filename.c_str(), NULL, 0);
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
  if (filename[0] != '.') { // Only open Spotify for non-hidden files
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
  bool success = SpotifyAPI::getInstance()->createPlaylist(
      name, "Created via SpotifyFS", true);
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
  std::string path_str(path);

  // Ignore macOS metadata files
  if (path_str.find("/._") != std::string::npos) {
    return -EACCES;
  }

  std::string filename = path_str.substr(path_str.find_last_of('/') + 1);
  std::string dir_path = path_str.substr(0, path_str.find_last_of('/'));

  // Find the playlist
  auto playlist_it = files.find(dir_path);
  if (playlist_it == files.end() || !playlist_it->second->is_playlist) {
    return -ENOENT;
  }

  // URL encode spaces in the search query
  std::string track_query = filename;
  size_t pos = 0;
  while ((pos = track_query.find(" ", pos)) != std::string::npos) {
    track_query.replace(pos, 1, "%20");
    pos += 3;
  }

  // Search for track and get info
  SpotifyAPI *api = SpotifyAPI::getInstance();
  std::string track_id = api->searchTrack(track_query);
  if (track_id.empty()) {
    return -ENOENT;
  }

  Track track = api->getTrackInfo(track_id);
  if (track.id.empty()) {
    return -ENOENT;
  }

  // Add track to playlist
  bool success = api->addTrackToPlaylist(playlist_it->second->id, track.id);
  if (!success) {
    return -EACCES;
  }

  std::cout << "Added track: " << track.artist << " -- " << track.name
            << std::endl;

  // Create file entry
  auto file = new spotify_file();
  file->id = track_id;
  file->name = track.artist + " -- " + track.name;
  file->is_playlist = false;
  file->duration_ms = track.duration_ms;
  file->uri = track.uri;

  // First store with original path to let touch complete
  files[path_str] = file;

  // Create custom path and store the file there
  std::string custom_path = dir_path + "/" + file->name;
  files[custom_path] = file;

  // Schedule removal of the original path entry
  // We need to do this after a short delay to ensure touch completes
  std::thread([path_str]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    files.erase(path_str);
  }).detach();

  return 0;
}

int SpotifyFileSystem::removeFile(const char *path) {
  auto it = files.find(path);
  if (it == files.end()) {
    return -ENOENT;
  }

  if (it->second->is_playlist) {
    // SpotifyAPI doesn't support deleting playlists
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

void SpotifyFileSystem::spotify_ll_create(fuse_req_t req, fuse_ino_t parent,
                                          const char *name, mode_t mode,
                                          struct fuse_file_info *fi) {
  std::string path_str(name);

  // Ignore macOS metadata files
  if (path_str.find("/._") != std::string::npos) {
    fuse_reply_err(req, EACCES);
    return;
  }

  // Check if file already exists
  auto existing_file = files.find(path_str);
  if (existing_file != files.end()) {
    fuse_reply_err(req, EEXIST);
    return;
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
    fuse_reply_err(req, ENOENT);
    return;
  }

  // Search for track and get info
  SpotifyAPI *api = SpotifyAPI::getInstance();
  std::string track_id = api->searchTrack(track_query);
  if (track_id.empty()) {
    fuse_reply_err(req, ENOENT);
    return;
  }

  Track track = api->getTrackInfo(track_id);
  if (track.id.empty()) {
    fuse_reply_err(req, ENOENT);
    return;
  }

  // Add track to playlist
  bool success = api->addTrackToPlaylist(playlist_it->second->id, track.id);
  if (!success) {
    fuse_reply_err(req, EACCES);
    return;
  }

  std::cout << "Added track: " << track.artist << " -- " << track.name
            << std::endl;

  // Create file entry
  auto file = new spotify_file();
  file->id = track.id;
  file->name = track.artist + " -- " + track.name;
  file->original_name = filename;
  file->is_playlist = false;
  file->duration_ms = track.duration_ms;
  file->uri = track.uri;

  // Set up entry attributes
  struct fuse_entry_param e;
  memset(&e, 0, sizeof(e));
  e.ino = random(); // You might want to implement proper inode allocation
  e.attr.st_mode = S_IFREG | 0666;
  e.attr.st_nlink = 1;
  e.attr.st_uid = getuid();
  e.attr.st_gid = getgid();
  e.attr.st_size = track.duration_ms > 0 ? track.duration_ms : 1024;
  e.attr.st_mtime = time(NULL);
  e.generation = 1;
  e.attr_timeout = 1.0;
  e.entry_timeout = 1.0;

  files[path_str] = file;

  // Reply with the entry parameters
  fuse_reply_create(req, &e, fi);
}