#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <curl/curl.h>

size_t write_callback(void* data, size_t size, size_t nmemb, void* userp)
{
  auto total = size * nmemb;
  static_cast<std::string*>(userp)
      ->append(static_cast<char*>(data), total);
    return total;
}

    std::string http_get(const std::string& url)
{

    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("CURL init failed");

  std::string response;

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    if (curl_easy_perform(curl) != CURLE_OK)
{

    curl_easy_cleanup(curl);
    throw std::runtime_error("HTTP request failed");
}

  curl_easy_cleanup(curl);
  return response;
}

int main(int argc, char** argv)
{
  try
{

    int interval = 0;
    std::string url   = "https://api.open-meteo.com";
    std::string route = "/v1/forecast?latitude=52.52&longitude=13.41&hourly=temperature_2m";
    std::string output;

    for (int i = 1; i < argc; ++i)
{
        std::string arg = argv[i];

    if ((arg == "-i" || arg == "--intervals") && i + 1 < argc) interval = std::stoi(argv[++i]);

    else if ((arg == "-u" || arg == "--url") && i + 1 < argc) url = argv[++i];

    else if ((arg == "-r" || arg == "--route") && i + 1 < argc) route = argv[++i];

    else if ((arg == "-o" || arg == "--output") && i + 1 < argc) output = argv[++i];
}

    if (url.empty())
      throw std::runtime_error("URL is required");

    do
{
        std::string response = http_get(url + route);

    if (output.empty())
{
        std::cout << "[" << response << "]\n";
}
    else
{
        auto now = std::chrono::system_clock::now();
        std::time_t t =
        std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << std::put_time(std::localtime(&t), "%Y-%m-%d");

        std::filesystem::create_directories(output);

        std::ofstream file(
        std::filesystem::path(output) /
        (ss.str() + ".txt")
);

    if (!file)
{
    throw std::runtime_error("File open failed");
}

    file << response;
}

    if (interval == 0)
{
    break;
}

        std::this_thread::sleep_for(
        std::chrono::seconds(interval)
);

}   while (true);
}
    catch (const std::exception& e)
{
        std::cerr << "Error: " << e.what() << "\n";
    return -1;
}

    return 0;
}