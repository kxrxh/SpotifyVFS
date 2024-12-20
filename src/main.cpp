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
    .mkdir = SpotifyFileSystem::createFolder,
    .unlink = SpotifyFileSystem::removeFile,
    .write = SpotifyFileSystem::writeFile,
    .truncate = SpotifyFileSystem::truncateFile,
    .create = SpotifyFileSystem::createFile,
};

// Main function.
int main(int argc, char *argv[]) {
    int ret;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    // Initialize Spotify filesystem with access token
    auto client_id = "a2b9e2d7538f4674bf704dc898c58564";
    if (!SpotifyAPI::init(client_id)) {
        std::cerr << "Failed to initialize SpotifyAPI" << std::endl;
        return -1;
    }

    SpotifyFileSystem::init();
    ret = fuse_main(args.argc, args.argv, &spotify_oper, nullptr);
    fuse_opt_free_args(&args);

    return ret;
}