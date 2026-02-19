#include <iostream>
#include <curl/curl.h>
#include <sys/time.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>


extern "C" {
    #include "weather/http.h"
    #include "cli/cli.h"
    #include "core/file_helper/file_helper.h"
}


constexpr int MAX_TIMESTAMP_BUFFER_SIZE = 20;

int main(int argc, char** argv)
{
    CLI cli;
    int interval = 0;
    char url_buffer[256];
    char route_buffer[256];
    char output_path_buffer[256];

    std::memset(url_buffer, 0, sizeof(url_buffer));
    std::memset(route_buffer, 0, sizeof(route_buffer));
    std::memset(output_path_buffer, 0, sizeof(output_path_buffer));

    CLI_Argument_Add(&cli, "--intervals", "-i", Argument_Option_Integer, &interval);
    CLI_Argument_Add(&cli, "--url", "-u", Argument_Option_String, url_buffer);
    CLI_Argument_Add(&cli, "--route", "-r", Argument_Option_String, route_buffer);
    CLI_Argument_Add(&cli, "--output", "-o", Argument_Option_String, output_path_buffer);

    if (!CLI_Parse(&cli, argc, argv))
    {
        std::cout << "Failed to parse arguments.\n";
        return -1;
    }

    if (url_buffer[0] == 0)
    {
        std::cout << "Empty url.\n";
        return -2;
    }

    bool output_is_stdout = false;
    if (output_path_buffer[0] == 0)
    {
        output_is_stdout = true;
    }

    bool is_running = true;
    do
    {
        char* response = nullptr;
        char full_path[512];
        // http_get("https://api.open-meteo.com + /v1/forecast?latitude=52.52&longitude=13.41&hourly=temperature_2m", &response, NULL);
        std::snprintf(full_path, sizeof(full_path) + 1, "%s%s", url_buffer, route_buffer);
        http_get(full_path, &response, nullptr);

        if (output_is_stdout)
        {
            std::cout << "[" << response << "]\n";
        }
        else
        {
            std::time_t current_time = std::time(nullptr);
            char file_name[MAX_TIMESTAMP_BUFFER_SIZE];
            std::tm* tm_info = std::localtime(&current_time);
        if (std::strftime(file_name, MAX_TIMESTAMP_BUFFER_SIZE, "%Y-%m-%d", tm_info) == 0)
    {
            std::snprintf(file_name, MAX_TIMESTAMP_BUFFER_SIZE, "Unknown Time");
    }

            int res = File_Helper_Write(output_path_buffer, file_name, response, std::strlen(response), FILE_HELPER_MODE_WRITE, true);
        if (res != FILE_HELPER_RESULT_SUCCESS)
            std::cout << "Failed to write to file code: " << res << "\n";
        }

        std::free(response);

        if (interval == 0)
            break;

        sleep(interval);
    }   while (is_running);

    return 0;
}