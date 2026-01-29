#include <stdio.h>
#include <curl/curl.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/time.h>

#include "weather/http.h"
#include "cli/cli.h"
#include "core/file_helper/file_helper.h"

/* TODO: Right now we use curl so any urls with & need to be \&, should not be a problem when we use our own http stuff */
#define MAX_TIMESTAMP_BUFFER_SIZE 20

int main(int argc, char** argv)
{
    CLI cli;

    int interval = 0;
    char url_buffer[256];
    char route_buffer[256];
    char output_path_buffer[256];
    
    memset(url_buffer, 0, sizeof(url_buffer));
    memset(route_buffer, 0, sizeof(route_buffer));
    memset(output_path_buffer, 0, sizeof(output_path_buffer));
    
    CLI_Argument_Add(&cli, "--intervals", "-i", Argument_Option_Integer, &interval);
    CLI_Argument_Add(&cli, "--url", "-u", Argument_Option_String, url_buffer);
    CLI_Argument_Add(&cli, "--route", "-r", Argument_Option_String, route_buffer);
    CLI_Argument_Add(&cli, "--output", "-o", Argument_Option_String, output_path_buffer);
    
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
    int output_is_stdout = false;
    if (output_path_buffer[0] == 0)
    {
        output_is_stdout = true;
    }
    bool is_running = true;

    do
    {
        char* response = NULL;
        char full_path[512];

        // http_get("https://api.open-meteo.com + /v1/forecast?latitude=52.52&longitude=13.41&hourly=temperature_2m", &response, NULL);
        snprintf(full_path, sizeof(full_path) + 1, "%s%s", url_buffer, route_buffer);
        http_get(full_path, &response, NULL);

        if(output_is_stdout)
        {
            printf("[%s]\n", response);
        }
        else
        {
            time_t current_time = time(NULL);
            char file_name[MAX_TIMESTAMP_BUFFER_SIZE];
            struct tm* tm_info = localtime(&current_time);
            if (strftime(file_name, MAX_TIMESTAMP_BUFFER_SIZE, "%Y-%m-%d", tm_info) == 0)
            {
                snprintf(file_name, MAX_TIMESTAMP_BUFFER_SIZE, "Unknown Time");
            }

            int res = File_Helper_Write(output_path_buffer, file_name, response, strlen(response), FILE_HELPER_MODE_WRITE, true);
            if (res != FILE_HELPER_RESULT_SUCCESS)
                printf("Failed to write to file code: %d\n", res);
        }
        free(response);

        if (interval == 0)
            break;
        
        sleep(interval);
    } while (is_running);

    return 0;
}