/*
 * [meteoserver]
 * requestQueue.c
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
 * @brief Allocs and initializes a new queue structure.
 * @return Initialized queue structure.
 **/
linked_queue_t *linked_queue_init();

/**
 * @brief Frees an existent queue.
 * @param queue Queue to be freed.
 **/
void linked_queue_free(linked_queue_t *queue);

/** 
 * @brief Inserts an element into the queue.
 * @param queue Queue that'll receive the element.
 * @param data Data to be inserted.
 * @return Node pushed to the queue.
 **/
queue_node_t *linked_queue_push(linked_queue_t *queue, void *data);

/** 
 * @brief Wrapper for queue_push that adds mutual exclusion.
 * @param queue Queue that'll receive the element.
 * @param data Data to be inserted.
 * @return Node structure pushed to the queue.
 **/
queue_node_t *linked_queue_push_ex(linked_queue_t *queue, void *data);

/**
 * @brief Retrieves next item in the queue.
 * @param queue Queue to be searched.
 * @return Element if queue has a next, NULL if queue is empty.
 **/
void *linked_queue_pop(linked_queue_t *queue);

/**
 * @brief Wrapper for queue_push that adds mutual exclusion.
 * @param queue Queue that'll receive the element.
 * @return Next element in the queue.
 **/
void *linked_queue_pop_ex(linked_queue_t *queue);

/**
 * @brief Appends a node to the end of the queue.
 * @param queue Queue where the node will be appended.
 * @param node  Node that will be appended.
 **/
static void linked_queue_append_node(linked_queue_t *queue, queue_node_t *node);

/**
 * @brief Unlinks the last node of the queue.
 * @param queue Queue to be modified.
 **/
static void linked_queue_pop_node(linked_queue_t *queue);



/* Definitions */


// Allocs and initializes a new queue structure
linked_queue_t *linked_queue_init()
{
    linked_queue_t *queue;

    queue = calloc(1, sizeof(linked_queue_t));
    queue->elements = 0;
    queue->first = NULL;
    queue->last = NULL;
    pthread_mutex_init(&queue->condMutex, NULL);
    pthread_cond_init(&queue->available, NULL);

    return queue;
}

// Frees an existent queue
void linked_queue_free(linked_queue_t *queue)
{
    if (queue)
    {
        pthread_mutex_destroy(&queue->condMutex);
        pthread_cond_destroy(&queue->available);
        safe_free(queue);
    }
}

// Inserts an element into the queue
queue_node_t *linked_queue_push(linked_queue_t *queue, void *data)
{
    queue_node_t *node;
    node = calloc(1, sizeof(queue_node_t));
    node->data = data;
    node->next = NULL;
    linked_queue_append_node(queue, node);
    return node;
}

// Wrapper for queue_push that adds mutual exclusion
queue_node_t *linked_queue_push_ex(linked_queue_t *queue, void *data)
{
    queue_node_t *node;
    
    node = calloc(1, sizeof(queue_node_t));
    node->data = data;
    node->next = NULL;
    pthread_mutex_lock(&queue->condMutex);
    linked_queue_append_node(queue, node);
    pthread_cond_signal(&queue->available);
    pthread_mutex_unlock(&queue->condMutex);
    
    return node;
}

// Retrieves next item in the queue
void *linked_queue_pop(linked_queue_t *queue)
{
    void *data = NULL;

    if (queue->first)
    {
        data = queue->first->data;
        linked_queue_pop_node(queue);
    }

    return data;
}

// Wrapper for queue_push that adds mutual exclusion 
void *linked_queue_pop_ex(linked_queue_t *queue)
{
    void *data = NULL;

    pthread_mutex_lock(&queue->condMutex);
    while (data = linked_queue_pop(queue), !data)
    {
        // Break the loop when the server receives a TERM signal
        if (serverHandler & SERVER_SIGTERM)
            break;

        pthread_cond_wait(&queue->available, &queue->condMutex);
    }
    pthread_mutex_unlock(&queue->condMutex);

    return data;
}

// Appends a node to the end of the queue
static void linked_queue_append_node(linked_queue_t *queue, queue_node_t *node)
{
    if (!queue->first)
    {
        queue->first = node;
        queue->last = node;
        node->prev = NULL;
        node->next = NULL;
    }
    else 
    {
        queue->last->next = node;
        node->prev = queue->last;
        node->next = NULL;
        queue->last = node;
    }
    queue->elements++;
}

// Unlinks the last node of the queue
static void linked_queue_pop_node(linked_queue_t *queue)
{
    queue_node_t *tmp = queue->first;
    queue->first = queue->first->next;

    if (queue->first)
        queue->first->prev = NULL;
    else
        queue->last = NULL;

    queue->elements--;
    safe_free(tmp);
}
