#include <stdio.h>
#include <curl/curl.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "weather/http.h"
#include "cli/cli.h"

/* TODO: Right now we use curl so any urls with & need to be \&, should not be a problem when we use our own http stuff */

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

    bool is_running = true;

    do
    {
        char* response = NULL;
        // http_get("https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&hourly=temperature_2m", &response, NULL);
        http_get(url_buffer, &response, NULL);
        printf("[%s]\n", response);


        FILE* file = fopen(output_path_buffer, "w");
        fprintf(file, "%s", response);
        fclose(file);

        if (interval == 0)
            break;
        
        sleep(interval);
    } while (is_running);

    return 0;
}