#pragma once

#include <curl/curl.h>
#include <json/json.h>
#include <string>
#include <vector>

// Represents a Spotify playlist
struct Playlist {
  std::string id;    // Unique identifier for the playlist
  std::string name;  // Name of the playlist
  std::string owner; // Owner of the playlist
};

// Represents a track in Spotify
struct Track {
  std::string id;     // Unique identifier for the track
  std::string name;   // Name of the track
  std::string artist; // Artist of the track
  std::string album;  // Album of the track
  std::string uri;    // URI for the track
  size_t duration_ms; // Duration of the track in milliseconds
};

// Class to interact with the Spotify API
class SpotifyAPI {
public:
  // Returns the singleton instance of SpotifyAPI
  static SpotifyAPI *getInstance();

  // Initializes the SpotifyAPI with the given client ID
  static bool init(std::string client_id);

  // Retrieves all playlists for the authenticated user
  std::vector<Playlist> getAllPlaylists();

  // Retrieves all tracks in a specified playlist
  std::vector<Track> getPlaylistTracks(std::string playlist_id);

  // Adds a track to a specified playlist
  bool addTrackToPlaylist(std::string playlist_id, std::string track_uri);

  // Removes a track from a specified playlist
  bool removeTrackFromPlaylist(std::string playlist_id, std::string track_uri);

  // Creates a new playlist with the specified name and description
  bool createPlaylist(std::string name, std::string description,
                      bool is_public);

  // Retrieves the user ID of the authenticated user
  std::string getUserId();

  // Retrieves information about a track
  Track getTrackInfo(std::string track_id);

  // Searches for a track by name and artist
  std::string searchTrack(std::string query);

private:
  static SpotifyAPI *instance; // Singleton instance

  std::string client_id;    // Client ID for Spotify API
  std::string access_token; // Access token for authentication

  SpotifyAPI() = default;             // Constructor is private and defaulted
  SpotifyAPI(SpotifyAPI const &);     // Prevent copies
  void operator=(SpotifyAPI const &); // Prevent assignments

  void oauth(); // Handles the OAuth authentication process
};
