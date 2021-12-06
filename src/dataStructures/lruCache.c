/*
 * [meteoserver]
 * lruCache.c
 * November 28, 2021.
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
* @brief Function in charge of searching for elements in the cache,
*        instead of the standard hash table.
* @param cachePool Double pointer that contains each one of the nodes of the cache.
* @param request Request that's searched in the cache.
* @param currentCapacity Current capacity of the cache.
* @return If exists, returns a node containing the requested element, NULL if it doesn't.
*/
lruCacheNode_t *lru_find_element(lruCacheNode_t **cachePool, char *request, size_t currentCapacity);

/**
* @brief Allocs and initializes a new lruCache_t structure.
* @param capacity Number of nodes that'll contain the cache.
* @return Initialized cache.
*/
lruCache_t *lru_cache_init(int capacity);

/**
* @brief Function in charge of freeing the data assigned to the cache.
* @param cache Cache to be freed.
*/
void lru_cache_free(lruCache_t *cache);

/**
* @brief Function in charge of updating and searching for cached elements.
* @param cache Cache that stores the elements.
* @param request Request to be searched in the queue.
* @return If exists, returns the cached element corresponding to the request. NULL if it doesn't.
*/
char    *lru_cache_get_element(lruCache_t *cache, char *request);

/**
* @brief Function in charge of updating the cache with a new element.
* @param cache Cache to be updated.
* @param request Request to be added to the queue.
* @param value Value to be cached along the request.
*/
void lru_cache_update_node(lruCache_t *cache, char *request, char *value);



/* Definitions */


// Function in charge of searching for elements in the cache, instead of the standard hash table
lruCacheNode_t *lru_find_element(lruCacheNode_t **cachePool, char *request, size_t currentCapacity)
{
    lruCacheNode_t *ret = NULL;

    for (size_t i = 0; i < currentCapacity; i++)
    {
        if (cachePool[i]->request && request && !strcmp(request, cachePool[i]->request)) 
        {
            ret = cachePool[i];
            break;
        }
    }

    return ret;
}

// Allocs and initializes a new lruCache_t structure
lruCache_t *lru_cache_init(int capacity)
{
    lruCache_t *cache = NULL;

    if (capacity <= 0)
        return NULL;

    cache = calloc(1, sizeof(lruCache_t));
    cache->cachePool = calloc(capacity, sizeof(lruCacheNode_t *));

    for (int i = 0; i < capacity; i++)
        cache->cachePool[i] = calloc(1, sizeof(lruCacheNode_t));

    cache->head = cache->cachePool[0];
    cache->head->next = cache->head;
    cache->head->prev = cache->head;
    cache->totalCapacity = capacity;
    cache->currentCapacity = 0;
    pthread_mutex_init(&(cache->mutex), NULL);

    return cache;
}

// Function in charge of freeing the data assigned to the cache
void lru_cache_free(lruCache_t *cache)
{
    pthread_mutex_lock(&(cache->mutex));
    for (size_t i = 0; i < cache->totalCapacity; i++)
    {
        safe_free(cache->cachePool[i]->request);
        safe_free(cache->cachePool[i]->md5);
        safe_free(cache->cachePool[i]);
    }

    safe_free(cache->cachePool);
    cache->totalCapacity = 0;
    cache->currentCapacity = 0;
    pthread_mutex_unlock(&(cache->mutex));
}

// Function in charge of updating and searching for cached elements
char    *lru_cache_get_element(lruCache_t *cache, char *request)
{
    lruCacheNode_t *tmpNode;

    pthread_mutex_lock(&(cache->mutex));
    tmpNode = lru_find_element(cache->cachePool, request, cache->currentCapacity);
    
    // Move the node to the head of the list if "request" is present in the cache
    if (tmpNode && tmpNode != cache->head)
    {
	    tmpNode->prev->next = tmpNode->next;
	    tmpNode->next->prev = tmpNode->prev;
	    tmpNode->next = cache->head;
	    tmpNode->prev = cache->head->prev;
	    tmpNode->prev->next = tmpNode;
	    tmpNode->next->prev = tmpNode;

        cache->head = tmpNode;
    }
    pthread_mutex_unlock(&(cache->mutex));

    return tmpNode ? tmpNode->md5 : NULL;
}

// Function in charge of updating the cache with a new element
void lru_cache_update_node(lruCache_t *cache, char *request, char *value)
{
    lruCacheNode_t *tmpNode = NULL;

    pthread_mutex_lock(&(cache->mutex));
    // When the cache is not full, sets a new node from the pool
    if (cache->currentCapacity < cache->totalCapacity)
    {
	    tmpNode = cache->cachePool[cache->currentCapacity];
	    tmpNode->request = strdup(request);
	    tmpNode->md5 = value;
	    tmpNode->next = cache->head;
	    tmpNode->prev = cache->head->prev;
	    tmpNode->prev->next = tmpNode;
	    tmpNode->next->prev = tmpNode;
        cache->currentCapacity++;
    }
    // Update oldest node when the cache is full
    else
    {
	    tmpNode = cache->head->prev;
        safe_free(tmpNode->request);
        safe_free(tmpNode->md5);
	    tmpNode->request = strdup(request);
	    tmpNode->md5 = value;
    }

    cache->head = tmpNode;
    pthread_mutex_unlock(&(cache->mutex));
}
