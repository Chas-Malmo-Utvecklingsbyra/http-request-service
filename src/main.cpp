#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <ctime>
#include <cstdio>
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
#define MINUTES_TO_SECONDS(x) ((x) * 60)

volatile sig_atomic_t running = 1;

// PIPE

int write_to_parent(int write_fd, const std::string &message)
{
    if (write_fd == -1)
        return -1;

    ssize_t total_written = 0;

    while (total_written < (ssize_t)message.size())
    {
        ssize_t n = write(write_fd,
                          message.c_str() + total_written,
                          message.size() - total_written);

        if (n == -1)
        {
            perror("Failed to write to parent");
            return -1;
        }

        total_written += n;
    }

    return 0;
}

int read_from_parent(int read_fd, std::string &out)
{
    if (read_fd == -1)
        return -1;

    char buffer[128];
    ssize_t bytes_read;

    while (true)
    {
        bytes_read = read(read_fd, buffer, sizeof(buffer) - 1);

        if (bytes_read == -1)
        {
            if (errno == EINTR)
                continue;

            perror("Failed to read from parent");
            return -1;
        }
        break;
    }

    if (bytes_read == 0)
    {
        std::cerr << "Parent closed the pipe\n";
        return -1;
    }

    buffer[bytes_read] = '\0';
    out = buffer;

    return 0;
}

void notify_parent(int write_fd)
{
    int result = write_to_parent(write_fd, "NEW_DATA");
    if (result != 0)
        std::cout << "Failed to write to parent process.\n";
}

// SIGNALS

void signal_handler(int signum)
{
    std::cout << "HTTP Request Service received signal: " << signum << "\n";
    running = 0;
}

void setup_signal_handlers()
{
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
}

// MAIN

int main(int argc, char **argv)
{
    CLI cli;

    int interval = 0;
    int read_fd = -1;
    int write_fd = -1;
    int specific_time = 0;

    bool specified_time = false;
    bool use_pipe = false;

    // CLI buffers
    char url_buf[512] = {};
    char route_buf[512] = {};
    char output_buf[512] = {};
    char name_buf[512] = {};
    char timestamp_buf[512] = {};

    CLI_Argument_Add(&cli, "--intervals", "-i", Argument_Option_Integer, &interval);
    CLI_Argument_Add(&cli, "--url", "-u", Argument_Option_String, url_buf);
    CLI_Argument_Add(&cli, "--route", "-r", Argument_Option_String, route_buf);
    CLI_Argument_Add(&cli, "--output", "-o", Argument_Option_String, output_buf);
    CLI_Argument_Add(&cli, "--name", "-n", Argument_Option_String, name_buf);
    CLI_Argument_Add(&cli, "--read-fd", "-fd", Argument_Option_Integer, &read_fd);
    CLI_Argument_Add(&cli, "--write-fd", "-fd", Argument_Option_Integer, &write_fd);
    CLI_Argument_Add(&cli, "--quarter", "-q", Argument_Option_Integer, &specific_time);
    CLI_Argument_Add(&cli, "--timestamp", "-ts", Argument_Option_String, timestamp_buf);

    setup_signal_handlers();

    if (!CLI_Parse(&cli, argc, argv))
    {
        std::cout << "Failed to parse arguments.\n";
        return -1;
    }

    if (url_buf[0] == 0)
    {
        std::cout << "Empty url.\n";
        return -2;
    }

    if (specific_time != 0)
        specified_time = true;

    bool output_is_stdout = (output_buf[0] == 0);

    if (read_fd != -1 || write_fd != -1)
        use_pipe = true;

    do
    {
        std::string full_path = std::string(url_buf) + route_buf;

        char *raw = nullptr;
        http_get(full_path.c_str(), &raw, nullptr);

        if (!raw)
        {
            std::cout << "HTTP request failed.\n";
            return -3;
        }

        std::string response = raw;
        std::free(raw);

        // OUTPUT

        if (!output_is_stdout)
        {
            std::time_t current_time = std::time(nullptr);

            std::tm tm_buf{};
            localtime_r(&current_time, &tm_buf);

            char file_name[512];

            if (name_buf[0] != 0)
                std::snprintf(file_name, sizeof(file_name), "%s", name_buf);
            else if (std::strftime(file_name, MAX_TIMESTAMP_BUFFER_SIZE, "%Y-%m-%d", &tm_buf) == 0)
                std::snprintf(file_name, MAX_TIMESTAMP_BUFFER_SIZE, "Unknown Time");

            if (File_Helper_Write(
                    output_buf,
                    file_name,
                    response.c_str(),
                    response.size(),
                    FILE_HELPER_MODE_WRITE,
                    true) == FILE_HELPER_RESULT_SUCCESS)
            {
                if (use_pipe)
                    notify_parent(write_fd);
            }
            else
            {
                std::cout << "Failed to write to file.\n";
            }
        }
        else
        {
            std::cout << "[" << response << "]\n";

            if (use_pipe)
                notify_parent(write_fd);
        }

        // PIPE

        if (use_pipe)
        {
            std::cout << "Waiting for parent process to acknowledge...\n";

            std::string msg;

            int res = read_from_parent(read_fd, msg);

            if (res == -1)
                break;

            if (msg == "ACK")
            {
                std::cout << "Received acknowledgment from parent: " << msg << "\n";
            }
            else if (msg == "QUIT")
            {
                std::cout << "Received QUIT from parent process. Exiting...\n";
                break;
            }
            else
            {
                std::cout << "Received unexpected message from parent process: " << msg << "\n";
            }
        }

        // EXIT CONDITIONS

        if (interval == 0 && !specified_time && timestamp_buf[0] == 0)
            break;

        if (!running)
            break;

        // QUARTER LOGIC (IDENTICAL)

        while (specified_time)
        {
            std::time_t current_time = std::time(nullptr);

            std::tm tm_buf{};
            localtime_r(&current_time, &tm_buf);

            int minutes = tm_buf.tm_min;
            int seconds_to_sleep = 0;
            int minutes_in_seconds = MINUTES_TO_SECONDS(minutes);

            if (minutes == 0 || minutes == 15 || minutes == 30 || minutes == 45)
            {
                sleep(60);
                break;
            }

            if (minutes > 0 && minutes < 15)
                seconds_to_sleep = MINUTES_TO_SECONDS(15) - minutes_in_seconds;
            else if (minutes > 15 && minutes < 30)
                seconds_to_sleep = MINUTES_TO_SECONDS(30) - minutes_in_seconds;
            else if (minutes > 30 && minutes < 45)
                seconds_to_sleep = MINUTES_TO_SECONDS(45) - minutes_in_seconds;
            else if (minutes > 45 && minutes <= 59)
                seconds_to_sleep = MINUTES_TO_SECONDS(60) - minutes_in_seconds;

            std::cout << "Seconds to sleep: " << seconds_to_sleep << "\n";

            if (!running)
                return 0;

            while (sleep(seconds_to_sleep) != 0 && running)
                ;
        }

        // TIMESTAMP LOGIC (IDENTICAL)

        if (timestamp_buf[0] != 0)
        {
            std::cout << "Timestamp buffer: " << timestamp_buf << "\n";

            std::time_t current_time = std::time(nullptr);

            std::tm tm_buf{};
            localtime_r(&current_time, &tm_buf);

            int hours = -1;
            int minutes = -1;

            sscanf(timestamp_buf, "%d:%d", &hours, &minutes);

            int remaining_hours = (tm_buf.tm_hour - hours) - 1;
            int remaining_minutes = (tm_buf.tm_min - minutes);

            std::cout << "Remaining time to next update: "
                      << remaining_hours << ":" << remaining_minutes << "\n";

            std::cout << hours << ":" << minutes << "\n";

            sleep(60);
        }

        // INTERVAL
        if (!specified_time)
        {
            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }

    } while (running);

    return 0;
}