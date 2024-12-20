#include "spotify_api.h"
#include <iostream>

#define DEBUG

SpotifyAPI *SpotifyAPI::instance = nullptr;

size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp) {
  ((std::string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
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

std::string handleCallback(std::string callback_url) {
  // Find start of access token
  size_t token_start = callback_url.find("access_token=") + 13;
  // Find end of access token (before next &)
  size_t token_end = callback_url.find("&", token_start);
  // Extract access token
  std::string access_token = callback_url.substr(token_start, token_end - token_start);
  #ifdef DEBUG
  std::cout << "Access token: " << access_token << std::endl;
  #endif
  return access_token;
}

// OAuth flow
void SpotifyAPI::oauth() {
  std::string redirect_uri = "http://localhost:3000";
  
  // Generate random state string
  const int state_length = 16;
  std::string state;
  state.reserve(state_length);
  static const char alphanum[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
  for (int i = 0; i < state_length; ++i) {
    state += alphanum[rand() % (sizeof(alphanum) - 1)];
  }

  // Build authorization URL
  std::string scope = "user-read-private user-read-email";
  std::string auth_url = "https://accounts.spotify.com/authorize";
  auth_url += "?response_type=token";
  auth_url += "&client_id=" + client_id;
  auth_url += "&scope=" + std::string(curl_easy_escape(nullptr, scope.c_str(), 0));
  auth_url += "&redirect_uri=" + std::string(curl_easy_escape(nullptr, redirect_uri.c_str(), 0));
  auth_url += "&state=" + std::string(curl_easy_escape(nullptr, state.c_str(), 0));

  std::cout << "login url:\n" << auth_url << std::endl;

  std::string redirect_url = "";
  
  std::cout << "Enter redirect url: ";

  std::cin >> redirect_url;
  access_token = handleCallback(redirect_url);
}
std::vector<Playlist> SpotifyAPI::getAllPlaylists() {
  std::string url = "https://api.spotify.com/v1/me/playlists";
  std::string headers = "Authorization: Bearer " + access_token;
  std::string response;

  CURL *curl = curl_easy_init();
  struct curl_slist *header_list = NULL;
  header_list = curl_slist_append(header_list, headers.c_str());

  // Set options to follow redirects and handle VPN issues
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Enable verbose output for debugging
  curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
  }

  curl_slist_free_all(header_list);
  curl_easy_cleanup(curl);

  // Parse JSON response
  Json::Value root;
  Json::Reader reader;
  std::vector<Playlist> playlists;

  std::cout << "response: " << response << std::endl;

  if (reader.parse(response, root)) {
    const Json::Value items = root["items"];
    std::cout << "Found " << items.size() << " playlists" << std::endl;
    playlists.reserve(items.size());
    for (const Json::Value& item : items) {
      std::cout << "Found playlist: " << item["name"].asString() << std::endl;
      Playlist playlist;
      playlist.id = item["id"].asString();
      playlist.name = item["name"].asString();
      playlist.owner = item["owner"]["display_name"].asString();
      playlists.push_back(playlist);
    }
  }

  return playlists;
}
