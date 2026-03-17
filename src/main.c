#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <curl/curl.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#include "weather/http.h"
#include "cli/cli.h"
#include "core/file_helper/file_helper.h"

/* TODO: Right now we use curl so any urls with & need to be \&, should not be a problem when we use our own http stuff */
#define MAX_TIMESTAMP_BUFFER_SIZE 20

#define MINUTES_TO_SECONDS(x) (x*60)

bool running = true;

int write_to_parent(int write_fd, const char *message)
{
    if (write_fd == -1)
        return -1;

    size_t message_len = strlen(message);
    ssize_t bytes_written = write(write_fd, message, message_len);
    if (bytes_written == -1)
    {
        perror("Failed to write to parent");
        return -1;
    }
    else if ((size_t)bytes_written != message_len)
    {
        fprintf(stderr, "Partial write to parent: expected %zu bytes, wrote %zd bytes\n", message_len, bytes_written);
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
        {
            return -1; // Interrupted by signal, caller should check if should quit
        }
        perror("Failed to read from parent");
        return -1;
    }
    
    if (bytes_read == 0)
    {
        fprintf(stderr, "Parent closed the pipe\n");
        return -1;
    }

    buffer[bytes_read] = '\0';
    return 0;
}

void signal_handler(int signum)
{    
    (void)signum;
    printf("HTTP Request Service received signal to terminate: %d\n", signum);
    running = false;
}

void setup_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  // Don't use SA_RESTART - allow interruption of system calls
    
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}

int main(int argc, char **argv)
{
    CLI cli;
    bool output_is_stdout = false;
    bool use_pipe = false;
    bool specified_time = false;
    int interval = 0;
    int read_fd = -1;
    int write_fd = -1;
    int specific_time = 0;
    
    char url_buffer[512];
    char route_buffer[512];
    char output_path_buffer[512];
    char name_buffer[512];
    
    memset(url_buffer, 0, sizeof(url_buffer));
    memset(route_buffer, 0, sizeof(route_buffer));
    memset(output_path_buffer, 0, sizeof(output_path_buffer));
    memset(name_buffer, 0, sizeof(name_buffer));

    
    CLI_Argument_Add(&cli, "--intervals", "-i", Argument_Option_Integer, &interval);
    CLI_Argument_Add(&cli, "--url", "-u", Argument_Option_String, url_buffer);
    CLI_Argument_Add(&cli, "--route", "-r", Argument_Option_String, route_buffer);
    CLI_Argument_Add(&cli, "--output", "-o", Argument_Option_String, output_path_buffer);
    CLI_Argument_Add(&cli, "--name", "-n", Argument_Option_String, name_buffer);
    CLI_Argument_Add(&cli, "--read-fd", "-fd", Argument_Option_Integer, &read_fd);
    CLI_Argument_Add(&cli, "--write-fd", "-fd", Argument_Option_Integer, &write_fd);
    CLI_Argument_Add(&cli, "--quarter", "-q", Argument_Option_Integer, &specific_time);

    setup_signal_handlers();

    if (!CLI_Parse(&cli, argc, argv))
    {
        printf("Failed to parse arguments.\n");
        return -1;
    }
    if (url_buffer[0] == 0)
    {
        printf("Empty url.\n");
        return -2;
    }

    if (specific_time != 0)
    {
        specified_time = true;
    }

    if (output_path_buffer[0] == 0)
    {
        output_is_stdout = true;
    }
    if (read_fd != -1 || write_fd != -1)
    {
        use_pipe = true;
    }

    do
    {
        char* response = NULL;
        char full_path[1024];

        snprintf(full_path, sizeof(full_path) + 1, "%s%s", url_buffer, route_buffer);
        http_get(full_path, &response, NULL);
        if(!output_is_stdout)
        {
            time_t current_time = time(NULL);
            char file_name[512];
            struct tm *tm_info = localtime(&current_time);

            if (name_buffer[0] != 0)
            {
                snprintf(file_name, sizeof(file_name), "%s", name_buffer);
            }
            else if (strftime(file_name, MAX_TIMESTAMP_BUFFER_SIZE, "%Y-%m-%d", tm_info) == 0)
            {
                snprintf(file_name, MAX_TIMESTAMP_BUFFER_SIZE, "Unknown Time");
            }
            /* TODO, be able to choose FILE_HELPER_MODE WRITE/APPEND */
            if (File_Helper_Write(output_path_buffer, file_name, response, strlen(response), FILE_HELPER_MODE_WRITE, true) == FILE_HELPER_RESULT_SUCCESS)
            {
                if (use_pipe)
                {
                    // int result = write_to_parent(write_fd, response); //TODO: test this
                    int result = write_to_parent(write_fd, "NEW_DATA");
                    if (result != 0)
                        printf("Failed to write to parent process.");
                }
            }
            else
            {
                printf("Failed to write to file.\n");
            }
        }
        else
        {
            printf("[%s]\n", response);
            if (use_pipe)
            {
                // int result = write_to_parent(write_fd, response); //TODO: test this
                int result = write_to_parent(write_fd, "NEW_DATA");
                if (result != 0)
                    printf("Failed to write to parent process.");
            }
        }

        free(response);

        if (use_pipe)
        {
            printf("Waiting for parent process to acknowledge...\n");
            char response_buffer[128];
            int res = read_from_parent(read_fd, response_buffer, sizeof(response_buffer));
            if (res == -1)
            {
                break;
            }
            else
            {
                if (strcmp(response_buffer, "ACK") == 0)
                {
                    printf("Received acknowledgment from parent: %s\n", response_buffer);
                }
                else if (strcmp(response_buffer, "QUIT") == 0)
                {
                    printf("Received QUIT from parent process. Exiting...\n");
                    break;
                }
                else
                {
                    printf("Received unexpected message from parent process: %s\n", response_buffer);
                }
            }
        }
        
        if (interval == 0 && !specified_time)
            break;

        if (!running)
            break;

        while (specified_time)
        {
            time_t current_time = time(NULL);
            struct tm *tm_info = localtime(&current_time);

            int minutes = tm_info->tm_min;
            int seconds_to_sleep = 0;
            int minutes_in_seconds = MINUTES_TO_SECONDS(minutes);

            if (minutes == 0 || minutes == 15 || minutes == 30 || minutes == 45)
            {
                break;
            }

            if (minutes > 0 && minutes < 15)
            {
                seconds_to_sleep = MINUTES_TO_SECONDS(15) - minutes_in_seconds;
            }
            else if (minutes > 15 && minutes < 30)
            {
                seconds_to_sleep = MINUTES_TO_SECONDS(30) - minutes_in_seconds;
            }
            else if (minutes > 30 && minutes < 45)
            {
                seconds_to_sleep = MINUTES_TO_SECONDS(45) - minutes_in_seconds;
            }
            else if (minutes > 45 && minutes <= 59)
            {
                seconds_to_sleep = MINUTES_TO_SECONDS(59) - minutes_in_seconds;
            }
            
            printf("Seconds to sleep: %d\r\n", seconds_to_sleep);


            if (!running)
                return 0;

            sleep(seconds_to_sleep);
        }

    
        if (!specified_time)
        {
            struct timespec ts;
            ts.tv_sec = interval;
            ts.tv_nsec = 0;
            int sleep_result = nanosleep(&ts, NULL);
            if (sleep_result == -1)
                break;
        }

    } while (running);

    return 0;
}