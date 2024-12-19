#include <cstdlib>
#include <cstring>
#include <errno.h>
#include "spotify_fs.h"

std::string SpotifyFileSystem::access_token;
std::unordered_map<std::string, struct spotify_file *> SpotifyFileSystem::files;

void SpotifyFileSystem::init(std::string& token) {
    access_token = token;
    refreshPlaylists();
}

int SpotifyFileSystem::getFileAttributes(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;  // read-write-execute directory
        stbuf->st_nlink = 2;
        return 0;
    }

    auto file = files.find(path);
    if (file != files.end()) {
        if (file->second->is_playlist) {
            stbuf->st_mode = S_IFDIR | 0777;  // read-write-execute directory
            stbuf->st_nlink = 2;
        } else {
            stbuf->st_mode = S_IFREG | 0777;  // read-write-execute file
            stbuf->st_nlink = 1;
            // Size could be metadata about the song in JSON format
            stbuf->st_size = file->second->duration_ms;
        }
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
        // List all playlists at root
        for (const auto &entry : files) {
            if (entry.second->is_playlist) {
                filler(buf, entry.second->name.c_str(), NULL, 0);
            }
        }
    } else {
        // List all tracks in the playlist
        std::string playlist_id = getSpotifyId(path);
        if (playlist_id.empty()) return -ENOENT;
        
        loadPlaylistTracks(playlist_id);
        // List tracks without .mp3 extension
        for (const auto &entry : files) {
            if (!entry.second->is_playlist && 
                entry.first.find(path) == 0) {
                std::string display_name = entry.second->name + 
                                         " - " + 
                                         entry.second->artist;
                filler(buf, display_name.c_str(), NULL, 0);
            }
        }
    }

    return 0;
}

int SpotifyFileSystem::createPlaylist(const char *path, mode_t mode) {
    // Only allow creation in root directory
    std::string pathStr(path);
    size_t slashCount = std::count(pathStr.begin(), pathStr.end(), '/');
    if (slashCount != 1) {
        return -EPERM; // Operation not permitted
    }

    // Extract playlist name from path
    std::string playlistName = pathStr.substr(pathStr.find_last_of('/') + 1);
    
    if (!isValidPlaylistName(playlistName)) {
        return -EINVAL; // Invalid argument
    }

    // TODO: Call Spotify API to create playlist
    // spotify_api.createPlaylist(playlistName, access_token);
    
    // Create new spotify_file entry
    auto playlist = new spotify_file();
    playlist->name = playlistName;
    playlist->is_playlist = true;
    // playlist->id will be set from Spotify API response
    
    files[path] = playlist;
    
    return 0;
}

int SpotifyFileSystem::removePlaylist(const char *path) {
    auto file = files.find(path);
    if (file == files.end() || !file->second->is_playlist) {
        return -ENOENT; // No such file or directory
    }

    // TODO: Call Spotify API to delete playlist
    // spotify_api.deletePlaylist(file->second->id, access_token);
    
    // Remove all tracks from this playlist from our cache
    std::string prefix(path);
    auto it = files.begin();
    while (it != files.end()) {
        if (it->first.find(prefix) == 0) {
            delete it->second;
            it = files.erase(it);
        } else {
            ++it;
        }
    }
    
    return 0;
}

bool SpotifyFileSystem::isValidPlaylistName(const std::string& name) {
    // Check if name is not empty and doesn't contain invalid characters
    if (name.empty() || name.length() > 100) {
        return false;
    }
    
    // Check for invalid characters (you might want to adjust these)
    const std::string invalidChars = "\\/:*?\"<>|";
    if (name.find_first_of(invalidChars) != std::string::npos) {
        return false;
    }
    
    return true;
}

int SpotifyFileSystem::createFile(const char *path, mode_t mode, struct fuse_file_info *fi) {
    std::string pathStr(path);
    
    size_t lastSlash = pathStr.find_last_of('/');
    if (lastSlash == std::string::npos) {
        return -EINVAL;
    }
    
    std::string playlistPath = pathStr.substr(0, lastSlash);
    std::string songName = pathStr.substr(lastSlash + 1);
    
    // Verify this is a valid playlist
    auto playlistEntry = files.find(playlistPath);
    if (playlistEntry == files.end() || !playlistEntry->second->is_playlist) {
        return -ENOENT;  // Playlist not found
    }
    
    if (!isValidSongName(songName)) {
        return -EINVAL;
    }
    
    // Parse song name and artist
    size_t separatorPos = songName.find(" - ");
    if (separatorPos == std::string::npos) {
        return -EINVAL;
    }
    
    std::string trackName = songName.substr(0, separatorPos);
    std::string artistName = songName.substr(separatorPos + 3);
    
    // TODO: Search Spotify API for the track
    // SpotifyTrack track = spotify_api.searchTrack(trackName, artistName, access_token);
    
    // TODO: Add track to playlist
    // spotify_api.addTrackToPlaylist(playlistEntry->second->id, track.id, access_token);
    
    // Create new spotify_file entry
    auto track = new spotify_file();
    track->name = trackName;
    track->artist = artistName;
    track->is_playlist = false;
    // These will be set from Spotify API response:
    // track->id = track.id;
    // track->album = track.album;
    // track->duration_ms = track.duration_ms;
    
    files[path] = track;
    
    return 0;
}

bool SpotifyFileSystem::isValidSongName(const std::string& name) {
    if (name.empty() || name.length() > 200) {
        return false;
    }
    
    // Must contain " - " separator
    if (name.find(" - ") == std::string::npos) {
        return false;
    }
    
    // Check for invalid characters
    const std::string invalidChars = "\\/:*?\"<>|";
    if (name.find_first_of(invalidChars) != std::string::npos) {
        return false;
    }
    
    return true;
}

int SpotifyFileSystem::readFile(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    auto file = files.find(path);
    if (file == files.end() || file->second->is_playlist) {
        return -ENOENT;
    }

    // Create a JSON string with track information
    std::string content = "{\n"
        "  \"name\": \"" + file->second->name + "\",\n"
        "  \"artist\": \"" + file->second->artist + "\",\n"
        "  \"album\": \"" + file->second->album + "\",\n"
        "  \"duration_ms\": " + std::to_string(file->second->duration_ms) + ",\n"
        "  \"spotify_id\": \"" + file->second->id + "\",\n"
        "  \"spotify_url\": \"spotify:track:" + file->second->id + "\",\n"
        "  \"web_url\": \"https://open.spotify.com/track/" + file->second->id + "\"\n"
        "}";

    if (offset >= content.length()) {
        return 0;
    }
    size_t len = content.length() - offset;
    if (len > size) {
        len = size;
    }
    memcpy(buf, content.c_str() + offset, len);
    return len;
}

std::string SpotifyFileSystem::getSpotifyId(const char *path) {
    auto file = files.find(path);
    if (file != files.end()) {
        return file->second->id;
    }
    return "";
}

int SpotifyFileSystem::refreshPlaylists() {
    // TODO: Call Spotify API to get user's playlists
    // Example:
    // auto playlists = spotify_api.getUserPlaylists(access_token);
    // for (const auto& playlist : playlists) {
    //     auto pl = new spotify_file();
    //     pl->id = playlist.id;
    //     pl->name = playlist.name;
    //     pl->is_playlist = true;
    //     files["/" + pl->name] = pl;
    // }
    return 0;
}

int SpotifyFileSystem::loadPlaylistTracks(const std::string& playlist_id) {
    // TODO: Call Spotify API to get playlist tracks
    // Example:
    // auto tracks = spotify_api.getPlaylistTracks(playlist_id, access_token);
    // for (const auto& track : tracks) {
    //     auto t = new spotify_file();
    //     t->id = track.id;
    //     t->name = track.name;
    //     t->artist = track.artist;
    //     t->album = track.album;
    //     t->duration_ms = track.duration_ms;
    //     t->is_playlist = false;
    //     files[playlist_path + "/" + t->name + " - " + t->artist] = t;
    // }
    return 0;
}

int SpotifyFileSystem::openFile(const char *path, struct fuse_file_info *fi) {
    auto file = files.find(path);
    if (file == files.end() || file->second->is_playlist) {
        return -ENOENT;
    }
    return 0;
}

void SpotifyFileSystem::cleanup() {
    // Free all allocated spotify_file objects
    for (auto& entry : files) {
        delete entry.second;
    }
    files.clear();
}