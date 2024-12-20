#ifndef SPOTIFY_FS_H
#define SPOTIFY_FS_H

#include <fuse.h>
#include <string>
#include <unordered_map>
#include <fuse/fuse_lowlevel.h>

// Spotify-specific file structure
struct spotify_file {
  std::string id;           // Spotify ID (track ID or playlist ID)
  std::string name;         // Display name
  std::string artist;       // Artist name (for tracks)
  std::string album;        // Album name (for tracks)
  size_t duration_ms;       // Track duration in milliseconds
  std::string uri;          // Spotify URI
  bool is_playlist;         // true if playlist, false if track
  std::string original_name; // Original name of the file
};

class SpotifyFileSystem {
public:
  static void init();
  static int getFileAttributes(const char *path, struct stat *stbuf);
  static int listFiles(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi);
  static int openFile(const char *path, struct fuse_file_info *fi);
  static int readFile(const char *path, char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi);
  static int createFolder(const char *path, mode_t mode);
  static int createFile(const char *path, mode_t mode, struct fuse_file_info *fi);
  static int removeFile(const char *path);
  static int cleanup();
  static int writeFile(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi);
  static int truncateFile(const char *path, off_t size);

  static void spotify_ll_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi);
private:
  static std::unordered_map<std::string, struct spotify_file *> files;
};

#endif // SPOTIFY_FS_H