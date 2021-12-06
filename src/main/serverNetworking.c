/*
 * [meteoserver]
 * serverNetworking.c
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
* @brief Initializes the server socket and sets the required options.
* @return An initialized socket descriptor.
*/
static int setup_socket();

/**
* @brief Function that sets up the server-side networking functions, mainly socket, bind and listen.
* @param state General struct that contains information from the program current state.
*/
void    setup_server_networking(serverState_t *state);



/* Definitions */


// Initializes the server socket and sets the required options
static int setup_socket()
{
    int             serverSocket;
    struct timeval  tv;
    int             on = 1;
    int             errcode;

    /*
    * Fill struct to set a timeout in setsockopt,
    * mainly to avoid DoS attacks.
    */
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    check_socket_error(serverSocket);

    /*
    * Set up socket-related options:
    * - SO_REUSEADDR: To avoid blocking the socket after using the server.
    * - SO_RECVTIMEO: To avoid receiving a DoS attack without resorting to
    *                 functions like select or poll.
    * - SO_SNDTIMEO: To avoid blocking client's connection if the send fails.
    */
    errcode = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    check_socket_error(errcode);
    errcode = setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    check_socket_error(errcode);
    errcode = setsockopt(serverSocket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    check_socket_error(errcode);

    return serverSocket;
}

// Function that sets up the server-side networking functions: mainly socket, bind and listen
void    setup_server_networking(serverState_t *state)
{
    struct sockaddr_in  sockaddr;
    int                 errcode;

    // Fill struct to configure socket binding
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    sockaddr.sin_port = htons(state->settings.port);

    // Create UNIX socket
    state->serverSocket = setup_socket();

    // Bind the socket to a TCP port
    errcode = bind(state->serverSocket, (struct sockaddr*)&sockaddr, sizeof(sockaddr));
    check_socket_error(errcode);

    // Start listening
    errcode = listen(state->serverSocket, state->settings.cacheSize);
    check_socket_error(errcode);
}
