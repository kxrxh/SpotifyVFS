#ifndef SPOTIFY_FS_H
#define SPOTIFY_FS_H

#include <fuse.h>
#include <string>
#include <unordered_map>

// Spotify-specific file structure
struct spotify_file {
  std::string id;           // Spotify ID (track ID or playlist ID)
  std::string name;         // Display name
  std::string artist;       // Artist name (for tracks)
  std::string album;        // Album name (for tracks)
  size_t duration_ms;       // Track duration in milliseconds
  bool is_playlist;         // true if playlist, false if track
};

class SpotifyFileSystem {
public:
  static void init(std::string& access_token);
  static int getFileAttributes(const char *path, struct stat *stbuf);
  static int listFiles(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi);
  static int openFile(const char *path, struct fuse_file_info *fi);
  static int readFile(const char *path, char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi);
  
  // Spotify-specific operations
  static int refreshPlaylists();
  static int loadPlaylistTracks(const std::string& playlist_id);
  static std::string getSpotifyId(const char *path);
  static void cleanup();
  static int createPlaylist(const char *path, mode_t mode);
  static int removePlaylist(const char *path);
  static int createFile(const char *path, mode_t mode, struct fuse_file_info *fi);

private:
  static std::string access_token;
  static std::unordered_map<std::string, struct spotify_file *> files;
  
  // Helper method to validate playlist name
  static bool isValidPlaylistName(const std::string& name);
  static bool isValidSongName(const std::string& name);
};

#endif // SPOTIFY_FS_H