#include "spotify_fs.h"
#include <fuse.h>

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
    std::string access_token = "YOUR_SPOTIFY_TOKEN"; // You'll need to implement token acquisition
    SpotifyFileSystem::init(access_token);

    ret = fuse_main(args.argc, args.argv, &spotify_oper, NULL);

    SpotifyFileSystem::cleanup(); // Clean up resources
    
    fuse_opt_free_args(&args);
    return ret;
}