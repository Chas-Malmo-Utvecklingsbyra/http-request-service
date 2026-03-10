#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <ctime>
#include <cstdio>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

extern "C"
{
#include "weather/http.h"
#include "cli/cli.h"
#include "file_helper/file_helper.h"
}

constexpr int MAX_TIMESTAMP_BUFFER_SIZE = 20;

bool running = true;

int write_to_parent(int write_fd, const std::string &message)
{
    if (write_fd == -1)
        return -1;

    ssize_t bytes_written = write(write_fd, message.c_str(), message.size());

    if (bytes_written == -1)
    {
        perror("Failed to write to parent");
        return -1;
    }

    if ((size_t)bytes_written != message.size())
    {
        std::cerr << "Partial write to parent\n";
        return -1;
    }

    return 0;
}

int read_from_parent(int read_fd, char *buffer, size_t buffer_size)
{
    if (read_fd == -1)
        return -1;

    ssize_t bytes_read;

    while ((bytes_read = read(read_fd, buffer, buffer_size - 1)) == -1)
    {
        if (errno == EINTR)
            return -1;

        perror("Failed to read from parent");
        return -1;
    }

    if (bytes_read == 0)
    {
        std::cerr << "Parent closed the pipe\n";
        return -1;
    }

    buffer[bytes_read] = '\0';
    return 0;
}

void notify_parent(int write_fd)
{
    int result = write_to_parent(write_fd, "NEW_DATA");
    if (result != 0)
        std::cout << "Failed to write to parent process.\n";
}

void signal_handler(int signum)
{
    std::cout << "HTTP Request Service received signal: " << signum << "\n";
    running = false;
}

void setup_signal_handlers()
{
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
}

int main(int argc, char **argv)
{
    CLI cli;
    int interval = 0;

    std::string url;
    std::string route;
    std::string output_path;
    std::string name;

    bool use_pipe = false;
    int read_fd = -1;
    int write_fd = -1;

    char url_buffer[256] = {};
    char route_buffer[256] = {};
    char output_path_buffer[256] = {};
    char name_buffer[256] = {};

    CLI_Argument_Add(&cli, "--intervals", "-i", Argument_Option_Integer, &interval);
    CLI_Argument_Add(&cli, "--url", "-u", Argument_Option_String, url_buffer);
    CLI_Argument_Add(&cli, "--route", "-r", Argument_Option_String, route_buffer);
    CLI_Argument_Add(&cli, "--output", "-o", Argument_Option_String, output_path_buffer);
    CLI_Argument_Add(&cli, "--name", "-n", Argument_Option_String, name_buffer);
    CLI_Argument_Add(&cli, "--read-fd", "-rfd", Argument_Option_Integer, &read_fd);
    CLI_Argument_Add(&cli, "--write-fd", "-wfd", Argument_Option_Integer, &write_fd);

    setup_signal_handlers();

    if (!CLI_Parse(&cli, argc, argv))
    {
        std::cout << "Failed to parse arguments.\n";
        return -1;
    }

    url = url_buffer;
    route = route_buffer;
    output_path = output_path_buffer;
    name = name_buffer;

    if (url.empty())
        url = "https://api.open-meteo.com";

    if (route.empty())
        route = "/v1/forecast?latitude=55.6&longitude=13&current_weather=true";

    bool output_is_stdout = output_path.empty();

    use_pipe = (read_fd != -1 || write_fd != -1);

    do
    {
        auto full_path = url + route;

        std::cout << "Requesting URL: " << full_path << std::endl;

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
            std::cout << "\nWeather API response:\n";
            std::cout << "[" << response << "]\n";

            if (use_pipe)
                notify_parent(write_fd);
        }
        else
        {
            std::time_t current_time = std::time(nullptr);
            char file_name[512];

            auto tm_info = std::localtime(&current_time);

            if (!name.empty())
                std::snprintf(file_name, sizeof(file_name), "%s", name.c_str());
            else if (std::strftime(file_name, MAX_TIMESTAMP_BUFFER_SIZE, "%Y-%m-%d", tm_info) == 0)
                std::snprintf(file_name, MAX_TIMESTAMP_BUFFER_SIZE, "Unknown Time");

            File_Helper_Result res = File_Helper_Write(
                output_path.c_str(),
                file_name,
                response.c_str(),
                response.size(),
                FILE_HELPER_MODE_WRITE,
                true);

            if (res != FILE_HELPER_RESULT_SUCCESS)
                std::cout << "Failed to write to file code: " << res << "\n";

            if (use_pipe)
                notify_parent(write_fd);
        }

        if (use_pipe)
        {
            std::cout << "Waiting for parent process...\n";

            char response_buffer[128];

            int res = read_from_parent(read_fd, response_buffer, sizeof(response_buffer));

            if (res == -1)
                break;

            std::string parent_msg(response_buffer);

            if (parent_msg == "ACK")
                std::cout << "Received ACK from parent\n";
            else if (parent_msg == "QUIT")
            {
                std::cout << "Parent requested shutdown\n";
                break;
            }
        }

        if (interval == 0 || !running)
            break;

        std::this_thread::sleep_for(std::chrono::seconds(interval));

    } while (running);

    return 0;
}