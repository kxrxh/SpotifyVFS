#include "spotify_api.h"
#include <iostream>

#define DEBUG

SpotifyAPI *SpotifyAPI::instance = nullptr;

// Add this callback function for CURL
size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userp) {
  ((std::string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
}

bool SpotifyAPI::init(std::string client_id, std::string client_secret) {
  if (instance == nullptr) {
    instance = new SpotifyAPI();
  }
  instance->client_id = client_id;
  instance->client_secret = client_secret;
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

SpotifyAPI *SpotifyAPI::getInstance() {
  if (instance == nullptr) {
    throw std::runtime_error("SpotifyAPI instance is null. Call init() first.");
  }
  return instance;
}
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


