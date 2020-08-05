/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define LRU_HASHMAPSIZE 1024
struct LRUNode {
    int hash;
    char * data;
    int size;
    struct LRUNode * next,
                   * prev;
};

struct LRUHashListNode {
    const char * key;
    int hash;
    struct LRUHashListNode * next;
    struct LRUNode * data; // non null
};

struct LRUCache {
    int totalSize;
    struct LRUNode * front, 
                   * rear;
    struct LRUHashListNode * hashToLRUNode[LRU_HASHMAPSIZE];
};

void initCache(struct LRUCache * cache);
void freeCache(struct LRUCache * cache);

struct LRUNode* LRUCache_get(struct LRUCache * cache,  const char * key, void * metadata);
struct LRUHashListNode* LRUCache_Hash_Get(struct LRUCache * cache, const char * key);
void LRUCache_Hash_Put(struct LRUCache * cache, const char * key, struct LRUNode * newnode);
int LRUCache_Hash(const char * key);

// CALLBACK
struct LRUNode * createData(const char * key, void * metadata);

