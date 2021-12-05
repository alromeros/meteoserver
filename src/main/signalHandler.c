/*
 * [meteoserver]
 * signalHandler.c
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


/**
* @brief Wrapper function that sets up the structures needed to modify signal behavior.
*/
void signal_modifier();

/**
* @brief Signal handler used in sigaction.
* @param signal Signal received by the program.
*/
static void signal_handler(int signal);

/**
* @brief Function in charge of freeing the cache after receiving a USR1 signal.
* @param state General struct that contains information from the program current state.
*/
void empty_cache(serverState_t *state);



/* Definitions */


// Function in charge of freeing the cache after receiving a USR1 signal
void empty_cache(serverState_t *state)
{
    serverHandler &= ~(SERVER_SIGUSR1); 

    lru_cache_free(state->lruCache);
    safe_free(state->lruCache);
    state->lruCache = lru_cache_init(state->settings.cacheSize);
    printf("Done!\n");
}

// Wrapper function that sets up the structures needed to modify signal behavior
void signal_modifier()
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
