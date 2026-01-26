#include <stdio.h>
#include <curl/curl.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "weather/http.h"


// TODO: Parse arguments (intervals, url, route, output_path)
int main(void)
{
    bool is_running = true;
    int interval = 0;

    //char* output_folder;
    //char* request_url;
    //char* response;

    do
    {
        Http h;
        http_initialize(&h);

        char* response = NULL;
        http_get("https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&hourly=temperature_2m", &response, NULL);
        printf("[%s]\n", response);

        FILE* file = fopen("whatever.json", "w");
        fprintf(file, "%s", response);
        fclose(file);

        http_dispose(&h, NULL);

        if (interval == 0)
            break;
        
        sleep(interval);
    } while (is_running);
    


    // GET
    // interval
    // output folder


    return 0;
}