#include <cstdlib>
#include <cstring>
#include <errno.h>
#include "spotify_fs.h"
#include "spotify_api.h"
#include <iostream>
std::unordered_map<std::string, struct spotify_file *> SpotifyFileSystem::files;

void SpotifyFileSystem::init() {
    SpotifyFileSystem::cleanup();
    std::vector<Playlist> playlists = SpotifyAPI::getInstance()->getAllPlaylists();
    for (const auto& playlist : playlists) {
        auto pl = new spotify_file();
        pl->id = playlist.id;
        pl->name = playlist.name;
        pl->is_playlist = true;
        files["/" + pl->name] = pl;
        std::cout << "Added playlist: " << pl->name << std::endl;
    }
}

int SpotifyFileSystem::getFileAttributes(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        return 0;
    }

    auto it = files.find(path);
    if (it != files.end()) {
        if (it->second->is_playlist) {
            stbuf->st_mode = S_IFDIR | 0777;
            stbuf->st_nlink = 2;
        } else {
            stbuf->st_mode = S_IFREG | 0777;
            stbuf->st_nlink = 1;
            stbuf->st_size = 1024;  // Default file size for now
        }
        return 0;
    }

    return -ENOENT;
}

int SpotifyFileSystem::listFiles(const char *path, void *buf, fuse_fill_dir_t filler,
                               off_t offset, struct fuse_file_info *fi) {
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    if (strcmp(path, "/") == 0) {
        // List all playlists and files in root
        for (const auto& pair : files) {
            // Remove leading '/' from the path to get just the name
            std::string name = pair.second->name;
            filler(buf, name.c_str(), NULL, 0);
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

    std::string content = "Dummy file content\n";
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
    auto pl = new spotify_file();
    pl->name = std::string(path).substr(1);  // Remove leading '/'
    pl->is_playlist = true;
    files[path] = pl;
    return 0;
}

int SpotifyFileSystem::removeFolder(const char *path) {
    auto it = files.find(path);
    if (it == files.end() || !it->second->is_playlist) {
        return -ENOENT;
    }
    
    delete it->second;
    files.erase(it);
    return 0;
}

int SpotifyFileSystem::createFile(const char *path, mode_t mode, struct fuse_file_info *fi) {
    auto file = new spotify_file();
    file->name = std::string(path).substr(1);  // Remove leading '/'
    file->is_playlist = false;
    file->duration_ms = 0;
    files[path] = file;  // Store with full path including leading '/'
    return 0;
}

int SpotifyFileSystem::removeFile(const char *path) {
    auto it = files.find(path);
    if (it == files.end() || it->second->is_playlist) {
        return -ENOENT;
    }
    
    delete it->second;
    files.erase(it);
    return 0;
}

int SpotifyFileSystem::cleanup() {
    for (auto& pair : files) {
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
