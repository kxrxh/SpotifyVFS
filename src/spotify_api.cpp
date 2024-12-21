#include "spotify_api.h"
#include <cpr/cpr.h>
#include <iostream>
#include <json/json.h>

#define DEBUG

SpotifyAPI *SpotifyAPI::instance = nullptr;

size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp) {
  ((std::string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
}

std::string handleCallback(std::string callback_url) {
  // Find start of access token
  size_t token_start = callback_url.find("access_token=") + 13;
  // Find end of access token (before next &)
  size_t token_end = callback_url.find("&", token_start);
  // Extract access token
  std::string access_token =
      callback_url.substr(token_start, token_end - token_start);
#ifdef DEBUG
  std::cout << "Access token: " << access_token << std::endl;
#endif
  return access_token;
}

SpotifyAPI *SpotifyAPI::getInstance() {
  if (instance == nullptr) {
    throw std::runtime_error("SpotifyAPI instance is null. Call init() first.");
  }
  return instance;
}

bool SpotifyAPI::init(std::string client_id) {
  if (instance == nullptr) {
    instance = new SpotifyAPI();
  }
  instance->client_id = client_id;
  instance->oauth();
  return true;
}

// OAuth flow
void SpotifyAPI::oauth() {
  std::string redirect_uri = "http://localhost:3000";

  // Generate random state string
  const int state_length = 16;
  std::string state;
  state.reserve(state_length);
  static const char alphanum[] = "0123456789"
                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "abcdefghijklmnopqrstuvwxyz";
  for (int i = 0; i < state_length; ++i) {
    state += alphanum[rand() % (sizeof(alphanum) - 1)];
  }

  // Build authorization URL
  std::string scope =
      "user-read-private user-read-email playlist-modify-private "
      "playlist-modify-public user-library-modify";
  std::string auth_url = "https://accounts.spotify.com/authorize";
  auth_url += "?response_type=token";
  auth_url += "&client_id=" + client_id;
  auth_url +=
      "&scope=" + std::string(curl_easy_escape(nullptr, scope.c_str(), 0));
  auth_url += "&redirect_uri=" +
              std::string(curl_easy_escape(nullptr, redirect_uri.c_str(), 0));
  auth_url +=
      "&state=" + std::string(curl_easy_escape(nullptr, state.c_str(), 0));

  std::cout << "login url:\n" << auth_url << std::endl;

  std::string redirect_url = "";

  std::cout << "Enter redirect url: ";

  std::cin >> redirect_url;
  access_token = handleCallback(redirect_url);
}

std::vector<Playlist> SpotifyAPI::getAllPlaylists() {
  std::string url = "https://api.spotify.com/v1/me/playlists";

  // Set up headers
  cpr::Header headers = {{"Authorization", "Bearer " + access_token}};

  // Make GET request
  auto response = cpr::Get(cpr::Url{url}, headers, cpr::VerifySsl{false});

  std::vector<Playlist> playlists;

  if (response.status_code == 200) {
    // Parse JSON response
    Json::Value root;
    Json::Reader reader;

    if (reader.parse(response.text, root)) {
      const Json::Value items = root["items"];
      std::cout << "Found " << items.size() << " playlists" << std::endl;
      playlists.reserve(items.size());

      for (const Json::Value &item : items) {
        std::cout << "Found playlist: " << item["name"].asString() << std::endl;
        Playlist playlist;
        playlist.id = item["id"].asString();
        playlist.name = item["name"].asString();
        playlist.owner = item["owner"]["display_name"].asString();
        playlists.push_back(playlist);
      }
    }
  } else {
    std::cerr << "Request failed with status code: " << response.status_code
              << std::endl;
    std::cerr << "Body: " << response.text << std::endl;
  }

  return playlists;
}

std::vector<Track> SpotifyAPI::getPlaylistTracks(std::string playlist_id) {
  std::vector<Track> tracks;
  int offset = 0;
  const int limit = 100;
  int total = 0;
  bool first_page = true;

  do {
    std::string url = "https://api.spotify.com/v1/playlists/" + playlist_id +
                      "/tracks?offset=" + std::to_string(offset) +
                      "&limit=" + std::to_string(limit);

    // Set up headers
    cpr::Header headers = {{"Authorization", "Bearer " + access_token}};

    // Make GET request
    auto response = cpr::Get(cpr::Url{url}, headers, cpr::VerifySsl{false});

    if (response.status_code == 200) {
      // Parse JSON response
      Json::Value root;
      Json::Reader reader;

      if (reader.parse(response.text, root)) {
        if (first_page) {
          total = root["total"].asInt();
          first_page = false;
          std::cout << "Found total of " << total << " tracks" << std::endl;
        }

        const Json::Value items = root["items"];
        std::cout << "Fetched " << items.size() << " tracks at offset "
                  << offset << std::endl;

        for (const Json::Value &item : items) {
          Track track;
          track.id = item["track"]["id"].asString();
          track.name = item["track"]["name"].asString();
          track.artist = item["track"]["artists"][0]["name"].asString();
          track.album = item["track"]["album"]["name"].asString();
          track.duration_ms = item["track"]["duration_ms"].asInt();
          track.uri = item["track"]["uri"].asString();
          tracks.push_back(track);
        }
      }
    } else {
      std::cerr << "Request failed with status code: " << response.status_code
                << std::endl;
      std::cerr << "Body: " << response.text << std::endl;
      break;
    }

    offset += limit;
  } while (offset < total);

  return tracks;
}

bool SpotifyAPI::addTrackToPlaylist(std::string playlist_id,
                                    std::string track_uri) {
  std::string url =
      "https://api.spotify.com/v1/playlists/" + playlist_id + "/tracks";

  // Set up headers
  cpr::Header headers = {{"Authorization", "Bearer " + access_token}};

  // Make POST request
  Json::Value body;
  body["uris"].append("spotify:track:" + track_uri);
  body["position"] = 0;
  auto response =
      cpr::Post(cpr::Url{url}, headers, cpr::Body{body.toStyledString()},
                cpr::VerifySsl{false});

  if (response.status_code == 201) {
    return true;
  } else {
    std::cerr << "Request failed with status code: " << response.status_code
              << std::endl;
    std::cerr << "Body: " << response.text << std::endl;
    return false;
  }
}

bool SpotifyAPI::removeTrackFromPlaylist(std::string playlist_id,
                                         std::string track_uri) {
  std::string url =
      "https://api.spotify.com/v1/playlists/" + playlist_id + "/tracks";

  // Set up headers
  cpr::Header headers = {{"Authorization", "Bearer " + access_token},
                         {"Content-Type", "application/json"}};

  // Create the request body with the track URI
  Json::Value trackObject;
  trackObject["uri"] = track_uri;

  Json::Value body;
  body["tracks"].append(trackObject);

  // Make DELETE request
  auto response =
      cpr::Delete(cpr::Url{url}, headers, cpr::Body{body.toStyledString()},
                  cpr::VerifySsl{false});

  if (response.status_code == 200) {
    return true;
  } else {
    std::cerr << "Request failed with status code: " << response.status_code
              << std::endl;
    std::cerr << "Body: " << response.text << std::endl;
    return false;
  }
}

std::string SpotifyAPI::getUserId() {
  std::string url = "https://api.spotify.com/v1/me";

  // Set up headers
  cpr::Header headers = {{"Authorization", "Bearer " + access_token}};

  // Make GET request
  auto response = cpr::Get(cpr::Url{url}, headers, cpr::VerifySsl{false});

  if (response.status_code == 200) {
    Json::Value root;
    Json::Reader reader;
    if (reader.parse(response.text, root)) {
      return root["id"].asString();
    }
  }
  return "";
}

bool SpotifyAPI::createPlaylist(std::string name, std::string description,
                                bool is_public) {
  std::string url =
      "https://api.spotify.com/v1/users/" + getUserId() + "/playlists";

  // Set up headers
  cpr::Header headers = {{"Authorization", "Bearer " + access_token}};

  // Make POST request
  Json::Value body;
  body["name"] = name;
  body["description"] = "New playlist created by SpotifyFS";
  body["public"] = is_public;

  auto response =
      cpr::Post(cpr::Url{url}, headers, cpr::Body{body.toStyledString()},
                cpr::VerifySsl{false});

  if (response.status_code == 201) {
    return true;
  } else {
    std::cerr << "Request failed with status code: " << response.status_code
              << std::endl;
    std::cerr << "Body: " << response.text << std::endl;
    return false;
  }
}

std::string SpotifyAPI::searchTrack(std::string query) {
  // URL encode the query parameter properly
  CURL *curl = curl_easy_init();
  char *encoded_query = curl_easy_escape(curl, query.c_str(), query.length());
  std::string url = "https://api.spotify.com/v1/search?q=" + 
                    std::string(encoded_query) + 
                    "&type=track";
  curl_free(encoded_query);
  curl_easy_cleanup(curl);

  // Set up headers
  cpr::Header headers = {{"Authorization", "Bearer " + access_token}};

  // Make GET request
  auto response = cpr::Get(cpr::Url{url}, headers, cpr::VerifySsl{false});

  if (response.status_code == 200) {
    Json::Value root;
    Json::Reader reader;
    if (reader.parse(response.text, root)) {
      const Json::Value& items = root["tracks"]["items"];
      if (!items.empty()) {
        return items[0]["id"].asString();
      }
    }
  } else {
    std::cerr << "Search failed with status code: " << response.status_code << std::endl;
    std::cerr << "Response: " << response.text << std::endl;
  }

  return "";
}

Track SpotifyAPI::getTrackInfo(std::string track_id) {
  std::string url = "https://api.spotify.com/v1/tracks/" + track_id;

  // Set up headers
  cpr::Header headers = {{"Authorization", "Bearer " + access_token}};

  // Make GET request
  auto response = cpr::Get(cpr::Url{url}, headers, cpr::VerifySsl{false});

  if (response.status_code == 200) {
    Json::Value root;
    Json::Reader reader;
    if (reader.parse(response.text, root)) {
      Track track;
      track.id = root["id"].asString();
      track.name = root["name"].asString();
      track.artist = root["artists"][0]["name"].asString();
      track.album = root["album"]["name"].asString();
      track.duration_ms = root["duration_ms"].asInt();
      track.uri = root["uri"].asString();
      return track;
    }
  }

  return Track();
}
