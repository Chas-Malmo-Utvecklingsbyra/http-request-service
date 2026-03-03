#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <ctime>
#include <cstdio>
#include <memory>

extern "C"
{
#include "weather/http.h"
#include "cli/cli.h"
#include "file_helper/file_helper.h"
}

constexpr int MAX_TIMESTAMP_BUFFER_SIZE = 20;

int main(int argc, char **argv)
{
    CLI cli;
    int interval = 0;

    std::string url;
    std::string route;
    std::string output_path;

    char url_buffer[256] = {};
    char route_buffer[256] = {};
    char output_path_buffer[256] = {};

    CLI_Argument_Add(&cli, "--intervals", "-i", Argument_Option_Integer, &interval);
    CLI_Argument_Add(&cli, "--url", "-u", Argument_Option_String, url_buffer);
    CLI_Argument_Add(&cli, "--route", "-r", Argument_Option_String, route_buffer);
    CLI_Argument_Add(&cli, "--output", "-o", Argument_Option_String, output_path_buffer);

    if (!CLI_Parse(&cli, argc, argv))
    {
        std::cout << "Failed to parse arguments.\n";
        return -1;
    }

    url = url_buffer;
    route = route_buffer;
    output_path = output_path_buffer;

    if (url.empty())
    {
        std::cout << "Empty url.\n";
        return -2;
    }

    bool output_is_stdout = output_path.empty();

    do
    {
        std::string full_path = url + route;

        char *raw_response = nullptr;
        http_get(full_path.c_str(), &raw_response, nullptr);

        if (!raw_response)
        {
            std::cout << "HTTP request failed.\n";
            return -3;
        }

        std::string response = raw_response;
        std::free(raw_response);

        if (output_is_stdout)
        {
            std::cout << "[" << response << "]\n";
        }
        else
        {
            std::time_t current_time = std::time(nullptr);
            char file_name[MAX_TIMESTAMP_BUFFER_SIZE];
            std::tm *tm_info = std::localtime(&current_time);

            if (std::strftime(file_name, MAX_TIMESTAMP_BUFFER_SIZE, "%Y-%m-%d", tm_info) == 0)
            {
                std::snprintf(file_name, MAX_TIMESTAMP_BUFFER_SIZE, "Unknown Time");
            }

            File_Helper_Result res = File_Helper_Write(
                output_path.c_str(),
                file_name,
                response.c_str(),
                response.size(),
                FILE_HELPER_MODE_WRITE,
                true);

            if (res != FILE_HELPER_RESULT_SUCCESS)
            {
                std::cout << "Failed to write to file code: " << res << "\n";
            }
        }

        std::free(raw_response);

        if (interval == 0)
        {
            break;
        }

        std::this_thread::sleep_for(std::chrono::seconds(interval));

    } while (true);

    return 0;
}