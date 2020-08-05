#include "lrucache.h"
#include "csapp.h"
#include <stddef.h> // NULL
#include <stdlib.h> // malloc, free
#include <string.h> // strcmp

sem_t semaphore;

// effectively, singleton
void initCache(struct LRUCache * cache) {
    cache->totalSize = 0;
    cache->front = NULL;
    cache->rear = NULL;
    bzero((char *)cache->hashToLRUNode, LRU_HASHMAPSIZE);
    Sem_init(&semaphore, 0, 1);
}

void freeCache(struct LRUCache * cache) {
    P(&semaphore);
    struct LRUNode *iterator = cache->front;
    while(iterator) {
        struct LRUNode * next = iterator->next;
        free(iterator -> data);
        iterator -> data = NULL;
        free(iterator);
        iterator = next;
    }
    for(int i=0;i<LRU_HASHMAPSIZE; i++) {
        struct LRUHashListNode * listNode =  cache->hashToLRUNode[i];
        struct LRUHashListNode * iterator = listNode;
        while(iterator) {
            struct LRUHashListNode * next = iterator -> next;
            free(iterator -> key);
            free(iterator); 
            iterator = next;
        }
    }
    V(&semaphore);
}

struct LRUNode * LRUCache_get(struct LRUCache *cache, const char * key, void * metadata) {
    struct LRUHashListNode * listNode = LRUCache_Hash_Get(cache, key);
    P(&semaphore);
    if(listNode) { // pop the node to front of the list
        struct LRUNode * node = listNode -> data;
        if(node -> prev) {
            node -> prev -> next = node -> next; // remove from list
            node -> next -> prev = node -> prev;
        }
        // insert front
        node -> next = cache->front;
        node -> prev = NULL;
        cache -> front -> prev = node;
        cache -> front = node;
        V(&semaphore);
        return node;
    } else {
        V(&semaphore);
        // create data and insert
        struct LRUNode * newNode = createData(key, metadata);
        P(&semaphore);
        newNode -> next = cache -> front;
        newNode -> prev = NULL;
        newNode -> hash = LRUCache_Hash(key);
        if(cache -> front) {
            cache -> front -> prev = newNode;
        }
        cache -> front = newNode;
        cache -> totalSize += newNode -> size;
        while(cache -> totalSize > MAX_CACHE_SIZE) {
            // evict oldest data
            struct LRUNode * rearIterator = cache -> front;
            while(rearIterator -> next) {
                rearIterator = rearIterator -> next;
            }
            cache -> rear = rearIterator;
            struct LRUNode * toDelete = cache -> rear;
            // remove from hashmap
            int hash = toDelete -> hash;
            struct LRUHashListNode * iterator =  cache -> hashToLRUNode[hash];
            struct LRUHashListNode * previt = NULL; // &(cache -> hashToLRUNode[hash]);
            while(iterator) {
                struct LRUHashListNode * next = iterator -> next;
                if(iterator -> data == toDelete) {
                    if(previt != NULL)
                        previt -> next = iterator -> next;
                    else
                        cache -> hashToLRUNode[hash] = iterator -> next;
                    free(iterator);
                    break;
                }
                previt = iterator;
                iterator = next;
            }

            free(toDelete -> data);
            cache -> totalSize -= toDelete->size;
            if(toDelete -> prev)
                toDelete -> prev -> next = NULL;
            cache -> rear = toDelete -> prev;
            free(toDelete);
        }
        V(&semaphore);
        LRUCache_Hash_Put(cache, key, newNode);
        return newNode;
    }
}

struct LRUHashListNode * LRUCache_Hash_Get(struct LRUCache *cache, const char * key) {
    int hash = LRUCache_Hash(key);
    P(&semaphore);
    struct LRUHashListNode * listNode = cache->hashToLRUNode[hash];
    if(!listNode) {
        V(&semaphore);
        return NULL;
    }
    struct LRUHashListNode * iterator = listNode;
    while(iterator) {
        if(listNode->key && key &&  strcmp(key, listNode->key) == 0) {
            V(&semaphore);
            return listNode;
        }
        iterator = iterator -> next;
    }
    V(&semaphore);
    return NULL;
}

void LRUCache_Hash_Put(struct LRUCache * cache, const char * key, struct LRUNode * data) {
    int hash = LRUCache_Hash(key);
    struct LRUHashListNode * newListNode = malloc(sizeof(struct LRUHashListNode));
    // iterator -> next = newListNode;
    newListNode -> data = data;
    newListNode -> key = strdup(key);
    newListNode -> hash = hash;
    newListNode -> next = NULL;
    P(&semaphore);
    struct LRUHashListNode * temp = cache -> hashToLRUNode[hash];
    if(temp) {
        struct LRUHashListNode * iterator = temp;
        while(iterator -> next)
            iterator = iterator -> next;
        iterator -> next = newListNode;
    } else {
        cache -> hashToLRUNode[hash] = newListNode;
    }
    V(&semaphore);
}

/* key: uri */
int LRUCache_Hash(const char * key) {
    int hash = 0;
    while(*key) {
        hash = ((((hash << 5) - hash))  + *key) % LRU_HASHMAPSIZE;
        key++;
    }
    return hash;
}

