#include "spotify_api.h"
#include "spotify_fs.h"
#include <fuse.h>
#include <iostream>

// Define the operations for our file system.
static struct fuse_operations spotify_oper = {
    .getattr = SpotifyFileSystem::getFileAttributes,
    .readdir = SpotifyFileSystem::listFiles,
    .open = SpotifyFileSystem::openFile,
    .read = SpotifyFileSystem::readFile,
    .mkdir = SpotifyFileSystem::createPlaylist,
    .rmdir = SpotifyFileSystem::removePlaylist,
    .create = SpotifyFileSystem::createFile,
};

// Main function.
int main(int argc, char *argv[]) {
  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  // Initialize Spotify filesystem with access token
  auto client_id = "a2b9e2d7538f4674bf704dc898c58564";
  auto client_secret = "9bb3bcc7ea0b4f6eacb12347af4e1412";
  if (!SpotifyAPI::init(client_id, client_secret)) {
    std::cerr << "Failed to initialize SpotifyAPI" << std::endl;
    return -1;
  }


  return ret;
}