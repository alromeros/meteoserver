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
static void setup_server(serverState_t **state, int argc, char **argv);

/**
* @brief Destructor in charge of releasing all the server's resources, mainly after receiving a TERM signal.
* @param state Struct that contains information from the program current state.
*/
static void teardown_server(serverState_t   *state);

/**
* @brief In charge of freeing resources before the program finishes its execution.
* @param state General struct that contains information from the program current state.
*/
static void free_server(serverState_t   *state);

/**
* @brief Function in charge of freeing the cache after receiving a USR1 signal.
* @param state General struct that contains information from the program current state.
*/
static void empty_cache(serverState_t *state);

/**
* @brief Function that sets up the server-side networking functions, mainly socket, bind and listen.
* @param state General struct that contains information from the program current state.
*/
static void setup_server_networking(serverState_t *state);

/**
* @brief Wrapper function that sets up the structures needed to modify signal behavior.
*/
static void signal_modifier();

/**
* @brief Signal handler used in sigaction.
* @param signal Signal received by the program.
*/
static void signal_handler(int signal);



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
static void setup_server(serverState_t **state, int argc, char **argv)
{
    *state = calloc(1, sizeof(serverState_t));

    if (*state == NULL)
        exit(ERROR);
    
    if (parse_arguments(&(*state)->settings, argc, argv) == false)
    {
        safe_free(*state);
        exit(ERROR);
    }

    (*state)->lruCache = lru_cache_init((*state)->settings.cacheSize);
    (*state)->requestQueue = linked_queue_init();
    (*state)->thread_pool = calloc((*state)->settings.threadNumber, sizeof(pthread_t));

    if ((*state)->lruCache == NULL ||
        (*state)->requestQueue == NULL ||
        (*state)->thread_pool == NULL)
    {
        free_server(*state);
        exit(ERROR);
    }
}

// In charge of freeing resources before the program finishes its execution
static void free_server(serverState_t   *state)
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
    // Releases the latest conditional wait
    pthread_cond_signal(&(state->requestQueue->available));

    // Waits for all the threads to finish their execution
    for (int i = 0; i < state->settings.threadNumber; i++)
        pthread_join(state->thread_pool[i], NULL);

    // Prints each one of the cached elements
    for (size_t i = 0; i < state->lruCache->currentCapacity; i++)
    {
        printf("Request: '%s' with hash: '%s'\n", state->lruCache->head->request, \
                state->lruCache->head->md5);
        state->lruCache->head = state->lruCache->head->next;
    }
    
    close(state->serverSocket);
    free_server(state);
    printf("Bye!\n");
}

// Function in charge of freeing the cache after receiving a USR1 signal
static void empty_cache(serverState_t *state)
{
    serverHandler &= ~(SERVER_SIGUSR1); 

    lru_cache_free(state->lruCache);
    safe_free(state->lruCache);
    state->lruCache = lru_cache_init(state->settings.cacheSize);
    printf("Done!\n");
}

// Function that sets up the server-side networking functions, mainly socket, bind and listen
static void setup_server_networking(serverState_t *state)
{
    struct timeval      tv;
    struct sockaddr_in  sockaddr;
    int                 errcode;
    int                 on = 1;

    // Fill struct to set a timeout in setsockopt
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    // Struct to configure socket binding
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    sockaddr.sin_port = htons(state->settings.port);

    // Create UNIX socket
    state->serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    check_socket_error(state->serverSocket);

    // Set up socket-related options
    errcode = setsockopt(state->serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    check_socket_error(errcode);
    errcode = setsockopt(state->serverSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    check_socket_error(errcode);
    errcode = setsockopt(state->serverSocket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    check_socket_error(errcode);

    // Bind the socket to a TCP port
    errcode = bind(state->serverSocket, (struct sockaddr*)&sockaddr, sizeof(sockaddr));
    check_socket_error(errcode);

    // Start listening
    errcode = listen(state->serverSocket, state->settings.cacheSize);
    check_socket_error(errcode);
}


/* main */

int main(int argc, char **argv)
{
    serverState_t   *state;
    int             connection;

    // Modify the behavior of SIGUSR1, SIGTERM and SIGINT signals
    signal_modifier();

    // Initialize the server's main data structures
    setup_server(&state, argc, argv);

    // Setup socket, bind and listen functions
    setup_server_networking(state);

    // Initialize thread pool in charge of processing client's requests
    pthread_mutex_init(&(state->queueMutex), NULL);
    for (int i = 0; i < state->settings.threadNumber; i++)
        pthread_create(&(state->thread_pool[i]), NULL, request_monitor, state);

    // Main loop
    while (serverHandler & SERVER_ENABLED)
    {    
        if (serverHandler & SERVER_SIGUSR1)
            empty_cache(state);

        if ((connection = accept(state->serverSocket, NULL, NULL)) < 0)
            continue;

        linked_queue_push_ex(state->requestQueue, &connection);
    }

    teardown_server(state);
    return SUCCESS;
}


/* Signal-handling functions */

// Wrapper function that sets up the structures needed to modify signal behavior
static void signal_modifier()
{
    struct sigaction handler = {0};

    handler.sa_handler = signal_handler;
    handler.sa_flags = 0;

    sigaction(SIGUSR1, &handler, NULL);
    sigaction(SIGTERM, &handler, NULL);
    sigaction(SIGINT, &handler, NULL);
}

// Signal handler used in sigaction
static void signal_handler(int signal)
{
    if (signal == SIGUSR1)
    {
        serverHandler |= SERVER_SIGUSR1;
    }
    else if (signal == SIGTERM || signal == SIGINT)
    {
        serverHandler &= ~(SERVER_ENABLED);
        serverHandler |= SERVER_SIGTERM;
    }
}
