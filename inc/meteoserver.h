/*
 * [meteoserver]
 * meteoserver.h
 * November 23, 2021.
 *
 * Created by Álvaro Romero <alvromero96@gmail.com>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#ifndef METEOLOGICA_H
#define METEOLOGICA_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include <stdint.h>

// Global defines
#define SERVER_ENABLED          0x01
#define SERVER_SIGUSR1          0x02
#define SERVER_SIGTERM          0x04
#define SOCKETERR               -1
#define ERROR                   1
#define SUCCESS                 0
#define THREAD_POOL_SIZE        8
#define MAXREQUESTSIZE          4096
#define REQUEST_FIELDS          3

// Formatting
#define SEND_TIMEOUT            "Timeout.\n"
#define SEND_LONG_REQUEST       "Request is too long.\n"
#define SEND_INVALID_REQUEST    "Request is not valid.\n"

// Useful macros
#define print_error()           fprintf(stderr, "Error '%d': '%s'", errno, strerror(errno))
#define safe_free(x)            if(x){free(x);x=NULL;}
#define check_socket_error(x)   if(x == SOCKETERR){print_error();exit(ERROR);} x;

// Global flag in charge of keeping track of the server state
extern volatile sig_atomic_t serverHandler;


// Struct containing an individual node of the LRU cache
typedef struct          lruCacheNode
{
	char                *request;
    char                *md5;
    struct lruCacheNode *next;
    struct lruCacheNode *prev;
}                       lruCacheNode_t;

// General struct for LRU cache
typedef struct          lruCache
{
    lruCacheNode_t      *head;
    lruCacheNode_t      **cachePool;
    pthread_mutex_t     mutex;
    size_t              currentCapacity;
    size_t              totalCapacity;
}                       lruCache_t;

// Struct to keep track of the command line arguments
typedef struct          arguments {
    int                 cacheSize;
    int                 port;
    int                 threadNumber;
}                       arguments_t;

// Struct that contains an individual node of the linked queue
typedef struct          queue_node_t {
    void                *data;
    struct queue_node_t *next;
    struct queue_node_t *prev;
}                       queue_node_t;

// General struct for the linked queue
typedef struct          linked_queue_t {
    pthread_cond_t      available;
    unsigned int        elements;
    pthread_mutex_t     condMutex;
    queue_node_t        *first;
    queue_node_t        *last;
}                       linked_queue_t;

// Struct that contains data from a client request
typedef struct          request {
    char                *msg;
    time_t              mseconds;
}                       request_t;

// Struct used by the MD5 algorithm
typedef struct          MD5Context {
	uint64_t            size;
	uint32_t            buffer[4];
	uint8_t             input[64];
	uint8_t             digest[16];
}                       MD5Context_t;

// Main server struct, in charge of keeping track of the program state
typedef struct          serverState {
    linked_queue_t      *requestQueue;
    lruCache_t          *lruCache;
    arguments_t         settings;
    pthread_t           *thread_pool;
    pthread_mutex_t     queueMutex;
    int                 serverSocket;
}                       serverState_t;


/* Definitions */

// Global definitions
void                *request_monitor(void *state);
void                signal_modifier();
void                empty_cache(serverState_t *state);
void                setup_server_networking(serverState_t *state);

// MD5-related definitions
uint32_t            F(uint32_t X, uint32_t Y, uint32_t Z);
uint32_t            G(uint32_t X, uint32_t Y, uint32_t Z);
uint32_t            H(uint32_t X, uint32_t Y, uint32_t Z);
uint32_t            I(uint32_t X, uint32_t Y, uint32_t Z);
char               *md5String(char *input);

// LRU cache-related definitions
lruCache_t          *lru_cache_init(int capacity);
char                *lru_cache_get_element(lruCache_t *cache, char *request);
void                lru_cache_update_node(lruCache_t *cache, char *request, char *value);
void                lru_cache_free(lruCache_t *cache);

// Queue-related definitions
linked_queue_t      *linked_queue_init();
queue_node_t        *linked_queue_push_ex(linked_queue_t * queue, void * data);
void                *linked_queue_pop_ex(linked_queue_t * queue);
void                linked_queue_free(linked_queue_t *queue);

#endif
