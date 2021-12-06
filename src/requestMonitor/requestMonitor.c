/*
 * [meteoserver]
 * requestMonitor.c
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
* @brief Function in charge of tokenizing the received request.
* @param str String containing the request.
* @param request Data structure that'll hold the tokenized data.
* @return Error code for proper error handling.
*/
static int  tokenize_request(char *str, request_t *request);

/**
* @brief Function in charge of handling the whole reading process.
* @param request Data structure that'll hold the data from the request.
* @param connection Client socket.
* @return Error code
*/
static bool read_client_request(request_t *request, int *connection);

/**
* @brief Function in charge of processing the information extracted from the connection.
* @param connection Client socket.
* @param serverState Data structure containing the global server information.
* @param request Data structure that holds the data from the request.
*/
static void process_client_request(int connection, serverState_t *serverState, request_t *request);

/**
* @brief Function in charge of monitoring and handling connection with clients.
* @param state Data structure containing the global server information.
*/
void    *request_monitor(void *state);



/* Definitions */


// Function in charge of tokenizing the received request
static int  tokenize_request(char *str, request_t *request)
{
    int requestIterator = 0;

    if (!str)
        return ERROR;

    for (char *token = strtok(str, " "); token && *token; token = strtok(NULL, " "))
    {
        switch (++requestIterator)
        {
            // The first element has to be a 'get' method
            case 1:
                if (strcmp(token, "get"))
                    return ERROR;
                else
                    break;
            // The second element is the string to be hashed
            case 2:
                request->msg = strdup(token);
                break;
            // The first element is the timeout value
            case 3:
                request->mseconds = strtoul(token, NULL, 10);
                break;
            default:
                return ERROR;
        }
    }

    // Condition to check if the request has the expected number of fields
    if (requestIterator != REQUEST_FIELDS)
        return ERROR;

    return SUCCESS;
}

// Function in charge of handling the whole reading process
static bool read_client_request(request_t *request, int *connection)
{
    char    buffer[MAXREQUESTSIZE + 1] = {0};
    ssize_t bytesRead;
    int     recvSocket;

    if (!connection)
        return false;

    // Read from the client's socket
    recvSocket = *connection;
    bytesRead = recv(recvSocket, buffer, MAXREQUESTSIZE + 1, 0);

    // Error handling
    if (bytesRead == SOCKETERR)
    {
        // Timeout
        if (errno == EAGAIN)
            send(recvSocket, SEND_TIMEOUT, strlen(SEND_TIMEOUT), 0);
        close(recvSocket);
        return false;
    }
    else if (bytesRead > MAXREQUESTSIZE)
    {
        // Clear the client's input before sending the error message
        do
            memset(buffer, 0, MAXREQUESTSIZE + 1);
        while (recv(recvSocket, buffer, MAXREQUESTSIZE + 1, 0) > 0);
        send(recvSocket, SEND_LONG_REQUEST, strlen(SEND_LONG_REQUEST), 0);
        close(recvSocket);
        return false;
    }

    // Extract each one of the fields from the request
    if (tokenize_request(buffer, request) == ERROR)
    {
        safe_free(request->msg);
        send(recvSocket, SEND_INVALID_REQUEST, strlen(SEND_INVALID_REQUEST), 0);
        close(recvSocket);
        return false;
    }

    return true;
}

// Function in charge of processing the request received from the client
static void process_client_request(int connection, serverState_t *serverState, request_t *request)
{
    uint8_t *md5 = NULL;

    // Condition to check if the request message is already present in the cache
    if ((md5 = lru_cache_get_element(serverState->lruCache, request->msg)), !md5)
    {
        md5 = md5String(request->msg);
        usleep(request->mseconds * 1000);
        lru_cache_update_node(serverState->lruCache, request->msg, md5);
    }

    send(connection, md5, 32, 0);
    send(connection, "\n", 1, 0);

    request->mseconds = 0;
    safe_free(request->msg);
    close(connection);
}

// Function in charge of monitoring and handling connection with clients
void    *request_monitor(void *state)
{
    serverState_t   *serverState = (serverState_t *)state;
    linked_queue_t  *queue = (linked_queue_t *)serverState->requestQueue;
    int             *clientSocket;
    request_t       request;
    
    request.msg = NULL;
    request.mseconds = 0;

    while (serverHandler & SERVER_ENABLED)
    {
        // When available, obtain accepted connections
        pthread_mutex_lock(&serverState->queueMutex);
        clientSocket = linked_queue_pop_ex(queue);
        pthread_mutex_unlock(&serverState->queueMutex);

        // Read the request from the client socket 
        if (read_client_request(&request, clientSocket) == false)
            continue;

        // Function in charge of processing the request
        process_client_request(*clientSocket, state, &request);
    }

    pthread_exit(NULL);
}
