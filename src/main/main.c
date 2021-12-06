/*
 * [meteoserver]
 * main.c
 * November 23, 2021.
 *
 * Created by Álvaro Romero <alvromero96@gmail.com>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "meteoserver.h"


/* Global flag in charge of keeping track of the server state */
volatile sig_atomic_t serverHandler = SERVER_ENABLED;


/**
* @brief Function in charge of printing a help message with usage information.
* @param argv Pointer containing an array with all the program's arguments.
*/
static void print_help_message(char **argv);

/**
* @brief In charge of parsing the in-line arguments.
* @param args Struct that'll hold all the relevant information from the in-line arguments.
* @param argc Number of arguments.
* @param argv Pointer containing an array with all the program's arguments.
*/
static bool argument_parser(arguments_t *args, int argc, char **argv);

/**
* @brief In charge of initializing all the data structures needed to start the server.
* @param state Pointer with the struct that contains information from the program current state.
* @param argc Number of in-line arguments.
* @param argv Pointer containing an array with all the program's arguments.
* @return Error/success code for proper error handling.
*/
static void initialize_server_data(serverState_t **state, int argc, char **argv);

/**
* @brief Destructor in charge of releasing all the server's resources, mainly after receiving a TERM signal.
* @param state Struct that contains information from the program current state.
*/
static void teardown_server(serverState_t   *state);

/**
* @brief In charge of freeing the program's currently allocated resources.
* @param state General struct that contains information from the program current state.
*/
static void free_current_data(serverState_t   *state);

/**
* @brief Function in charge of:
*   - Initializing the thread pool, in charge of monitoring and processing client requests.
*   - Starting the loop in charge of accepting connections.
* @param state General struct that contains information from the program current state.
*/
static void start_server(serverState_t *state);


/* Definitions */


// Function in charge of printing a help message with information about this program
static void print_help_message(char **argv)
{
    printf("\n");
    printf("Usage: %s [-p port] [-C amount] [-t amount]\n", argv[0]);
    printf("    -p  <port>          Port.\n");
    printf("    -C  <amount>        Cache size.\n");
    printf("    -t  <amount>        Number of threads used as thread pool (8 by default).\n");
    printf("    -h                  Show this help message.\n");
    printf("\n");
}

// In charge of parsing the in-line arguments
static bool parse_arguments(arguments_t *args, int argc, char **argv)
{
    const char  *short_opt = "p:C:ht:";
    int         c;

    while ((c = getopt(argc, argv, short_opt)) != -1)
    {
        switch (c)
        {
            case 'p':
                args->port = atoi(optarg);
                break;
            case 'C':
                args->cacheSize = atoi(optarg);
                break;
            case 't':
                args->threadNumber = atoi(optarg);
                break;
            default:
                print_help_message(argv);
                return false;
        }
    }

    if (args->port <= 0)
    {
        fprintf(stderr, "Error: A valid '-p' (port) argument is obligatory.\n");
        return false;
    }

    if (args->cacheSize <= 0)
    {
        fprintf(stderr, "Error: A valid '-C' (cache size) argument is obligatory.\n");
        return false;
    }

    if (args->threadNumber <= 0 || args->threadNumber >= 1000)
        args->threadNumber = THREAD_POOL_SIZE;

    return true;
}

// In charge of initializing all the data structures needed to start the server
static void initialize_server_data(serverState_t **state, int argc, char **argv)
{
    *state = calloc(1, sizeof(serverState_t));

    if (*state == NULL)
        exit(ERROR);

    // Parse in-line arguments
    if (parse_arguments(&(*state)->settings, argc, argv) == false)
    {
        safe_free(*state);
        exit(ERROR);
    }

    // Initialize the required data structures
    (*state)->lruCache = lru_cache_init((*state)->settings.cacheSize);
    (*state)->requestQueue = linked_queue_init();
    (*state)->thread_pool = calloc((*state)->settings.threadNumber, sizeof(pthread_t));

    // Error handling
    if ((*state)->lruCache == NULL || (*state)->requestQueue == NULL || (*state)->thread_pool == NULL)
    {
        free_current_data(*state);
        exit(ERROR);
    }
}

// In charge of freeing resources before the program finishes its execution
static void free_current_data(serverState_t   *state)
{
    lru_cache_free(state->lruCache);
    linked_queue_free(state->requestQueue);
    safe_free(state->thread_pool);
    safe_free(state->lruCache);
    safe_free(state);
}

// Destructor in charge of releasing all the server's resources, mainly after receiving a TERM signal
static void teardown_server(serverState_t   *state)
{
    // Release the latest conditional wait
    pthread_cond_signal(&(state->requestQueue->available));

    // Wait for all the threads to finish their execution
    for (int i = 0; i < state->settings.threadNumber; i++)
        pthread_join(state->thread_pool[i], NULL);

    // Print each one of the cached elements
    for (size_t i = 0; i < state->lruCache->currentCapacity; i++)
    {
        printf("Request: '%s' with hash: '%s'\n", state->lruCache->head->request,
                state->lruCache->head->md5);
        state->lruCache->head = state->lruCache->head->next;
    }
    
    close(state->serverSocket);
    free_current_data(state);
    printf("Bye!\n");
}

// In charge of running the two main sections of the server
static void start_server(serverState_t *state)
{
    int connection;

    // Initialize thread pool in charge of processing client requests
    pthread_mutex_init(&(state->queueMutex), NULL);
    for (int i = 0; i < state->settings.threadNumber; i++)
        pthread_create(&(state->thread_pool[i]), NULL, request_monitor, state);

    // Main loop in charge of accepting connections
    while (serverHandler & SERVER_ENABLED)
    {    
        if (serverHandler & SERVER_SIGUSR1)
            empty_cache(state);

        if ((connection = accept(state->serverSocket, NULL, NULL)) < 0)
            continue;

        // Add connection to the thread-safe linked queue
        linked_queue_push_ex(state->requestQueue, &connection);
    }
}


/* main */

int main(int argc, char **argv)
{
    serverState_t   *state;

    // Modify the behavior of SIGUSR1, SIGTERM and SIGINT signals
    signal_modifier();

    // Parse arguments and initialize the server main data structures
    initialize_server_data(&state, argc, argv);

    // Setup socket, bind and listen functions
    setup_server_networking(state);

    /*
    * In charge of running the two main sections of the server:
    *   - The thread pool, in charge of monitoring and processing client requests.
    *   - The loop in charge of accepting connections.
    */
    start_server(state);

    // Teardown server after receiving a TERM/INT signal
    teardown_server(state);
    return SUCCESS;
}
