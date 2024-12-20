# SpotifyVFS

SpotifyVFS is a virtual filesystem that allows you to interact with your Spotify playlists and tracks through your operating system's file explorer. It uses FUSE (Filesystem in Userspace) to mount Spotify as a local filesystem, enabling you to manage your music library using familiar file operations.

## Features

- 📁 Browse Spotify playlists as folders
- 🎵 View tracks as files within playlist folders
- ▶️ Play tracks by opening them (launches Spotify)
- ➕ Create new playlists by creating directories
- 📝 Add tracks to playlists by creating files
- 🗑️ Remove tracks from playlists by deleting files
- 🔄 Real-time synchronization with Spotify

## Prerequisites

- Linux or macOS
- FUSE library
- C++ compiler with C++11 support
- [CPR](https://github.com/libcpr/cpr) library for HTTP requests
- [JsonCpp](https://github.com/open-source-parsers/jsoncpp) library for JSON parsing
- Spotify account and registered application

## Building

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install libfuse-dev libcpr-dev libjsoncpp-dev

# Build the project
mkdir build
cd build
cmake ..
make
```

## Usage

1. Register a Spotify application at [Spotify Developer Dashboard](https://developer.spotify.com/dashboard)
2. Update the `client_id` in `src/main.cpp` with your application's client ID
3. Mount the filesystem:

```bash
./SpotifyFS /path/to/mount/point
```

4. Navigate to the mount point to interact with your Spotify library
5. Unmount when done:

```bash
fusermount -u /path/to/mount/point
```

## File Operations

- **List playlists**: Navigate to the root directory
- **View tracks**: Enter a playlist directory
- **Play track**: Open a track file (launches Spotify)
- **Create playlist**: Create a new directory
- **Add track**: Create a new file with the format "Artist - Song Name"
- **Remove track**: Delete the track file

## Implementation Details

SpotifyVFS is implemented using:
- FUSE for filesystem operations
- Spotify Web API for music library management
- OAuth 2.0 for authentication
- CPR for HTTP requests
- JsonCpp for JSON parsing

## Limitations

- Spotify API rate limits apply
- Some operations may have slight delays due to API communication
- Track content cannot be directly read/written
- Playlist deletion is not supported by Spotify's API

## Acknowledgments

- [FUSE](https://github.com/libfuse/libfuse)
- [Spotify Web API](https://developer.spotify.com/documentation/web-api/)
- [CPR](https://github.com/libcpr/cpr)
- [JsonCpp](https://github.com/open-source-parsers/jsoncpp)