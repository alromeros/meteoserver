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

#define SERVER_ENABLED      0x01
#define SERVER_SIGUSR1      0x02
#define SERVER_SIGTERM      0x04
#define ERROR               1
#define SUCCESS             0
#define THREAD_POOL_SIZE    8
#define SOCKETERR          -1
#define MAXREQUESTSIZE      4096
#define REQUEST_FIELDS      3
#define SEND_TIMEOUT        "Timeout.\n"
#define SEND_LONG_REQUEST   "Request is too long.\n"
#define SEND_INVALID_REQUEST "Request is not valid.\n"

#define print_error()           fprintf(stderr, "Error '%d': '%s'", errno, strerror(errno))
#define safe_free(x)            if(x){free(x);x=NULL;}
#define check_socket_error(x)   if(x == SOCKETERR){print_error();exit(ERROR);} x;


extern volatile sig_atomic_t serverHandler;


//double-linked list
typedef struct          lruCacheNode
{
	char                *request;
    char                *md5;
    struct lruCacheNode *next;
    struct lruCacheNode *prev;
}                       lruCacheNode_t;

typedef struct          lruCache
{
    lruCacheNode_t      *head;
    lruCacheNode_t      **cachePool;
    pthread_mutex_t     mutex;
    size_t              currentCapacity;
    size_t              totalCapacity;
}                       lruCache_t;

typedef struct          arguments {
    int                 cacheSize;
    int                 port;
    int                 threadNumber;
}                       arguments_t;

typedef struct          queue_node_t {
    void                *data;                       ///< pointer to the node data
    struct queue_node_t *next; ///< pointer to next node
    struct queue_node_t *prev; ///< pointer to prev node
}                       queue_node_t;

typedef struct          linked_queue_t {
    pthread_cond_t      available;     ///< condition variable when queue is empty
    unsigned int        elements;        ///< counts the number of elements stored in the queue
    pthread_mutex_t     condMutex;        ///< mutex for mutual exclusion
    queue_node_t        *first; ///< points to the first node that would go out the queue
    queue_node_t        *last;  ///< pointer to the last node in the queue
}                       linked_queue_t;

typedef struct          request {
    char                *msg;
    time_t              mseconds;
}                       request_t;

typedef struct          MD5Context {
	uint64_t            size;        // Size of input in bytes
	uint32_t            buffer[4];   // Current accumulation of hash
	uint8_t             input[64];    // Input to be used in the next step
	uint8_t             digest[16];   // Result of algorithm
}                       MD5Context_t;

typedef struct          serverState {
    linked_queue_t      *requestQueue;
    lruCache_t          *lruCache;
    arguments_t         settings;
    pthread_t           *thread_pool;
    pthread_mutex_t     queueMutex;
    pthread_mutex_t     cacheMutex;
    int                 serverSocket;
}                       serverState_t;

void    *request_monitor(void *state);


uint32_t            F(uint32_t X, uint32_t Y, uint32_t Z);
uint32_t            G(uint32_t X, uint32_t Y, uint32_t Z);
uint32_t            H(uint32_t X, uint32_t Y, uint32_t Z);
uint32_t            I(uint32_t X, uint32_t Y, uint32_t Z);
char               *md5String(char *input);

lruCache_t          *lru_cache_init(int capacity);
void                linked_queue_free(linked_queue_t *queue);
char                *lru_cache_get_element(lruCache_t *cache, char *request);
void                lru_cache_update_node(lruCache_t *cache, char *request, char *value);
linked_queue_t      *linked_queue_init();
lruCacheNode_t      *find_element(lruCacheNode_t **cachePool, char *request, size_t currentCapacity);

queue_node_t        *linked_queue_push(linked_queue_t * queue, void * data);
queue_node_t        *linked_queue_push_ex(linked_queue_t * queue, void * data);
void                *linked_queue_pop(linked_queue_t * queue);
void                *linked_queue_pop_ex(linked_queue_t * queue);
void                linked_queue_unlink_and_push_node(linked_queue_t * queue, queue_node_t *node);
static void         linked_queue_append_node(linked_queue_t *queue, queue_node_t *node);
static void         linked_queue_pop_node(linked_queue_t *queue);
void                lru_cache_free(lruCache_t *cache);

#endif
