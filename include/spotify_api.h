#pragma once

#include <curl/curl.h>
#include <json/json.h>
#include <string>
#include <vector>
struct Playlist {
  std::string id;
  std::string name;
  std::string owner;
};

class SpotifyAPI {
public:
  static SpotifyAPI *getInstance();

  static bool init(std::string client_id);

  std::vector<Playlist> getAllPlaylists();

private:
  static SpotifyAPI *instance;

  std::string client_id;

  std::string access_token;

  SpotifyAPI() = default;             // Constructor is private and defaulted
  SpotifyAPI(SpotifyAPI const &);     // Prevent copies
  void operator=(SpotifyAPI const &); // Prevent assignments

  void oauth();
};
