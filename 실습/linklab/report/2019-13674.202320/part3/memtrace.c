//------------------------------------------------------------------------------
//
// memtrace
//
// trace calls to the dynamic memory manager
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>
#include "callinfo.h"

//
// function pointers to stdlib's memory management functions
//
static void *(*mallocp)(size_t size) = NULL;
static void (*freep)(void *ptr) = NULL;
static void *(*callocp)(size_t nmemb, size_t size);
static void *(*reallocp)(void *ptr, size_t size);

//
// statistics & other global variables
//
static unsigned long n_malloc  = 0;
static unsigned long n_calloc  = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb  = 0;
static unsigned long n_freeb   = 0;
static item *list = NULL;

void * malloc(size_t size)
{
   n_allocb += size;
   n_malloc++;
   void * resultP = mallocp(size);
   LOG_MALLOC(size, resultP);
   item * allocated = alloc(list, resultP, size);
   return resultP;   
}

void free(void * ptr)
{
   LOG_FREE(ptr);
   item * deallocated = dealloc(list, ptr);
   n_freeb += deallocated->size; 
   freep(ptr);
}

void * calloc(size_t nmemb, size_t size)
{
   n_calloc++;
   void * resultP = callocp(nmemb, size);
   LOG_CALLOC(nmemb, size, resultP);
   if(resultP) {
     n_allocb += size*nmemb;
     item * allocated = alloc(list, resultP, nmemb * size); 
   }
   return resultP;
}
void * realloc(void *ptr, size_t new_size)
{
   n_realloc++;
   void * resultP = reallocp(ptr, new_size);
   item * deallocated = dealloc(list, ptr);
   n_freeb += deallocated->size;
   LOG_REALLOC(ptr, new_size, resultP);
   if(resultP) {
      n_allocb+= new_size;
      alloc(list, resultP, new_size);
   }
   return resultP; 
}   
 
//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void)
{
  char *error;

  LOG_START();

  // initialize a new list to keep track of all memory (de-)allocations
  // (not needed for part 1)
  list = new_list();

  // ...
  mallocp = dlsym(RTLD_NEXT, "malloc");
  if ((error = dlerror()) != NULL) {
     fputs(error,stderr);
     exit(1);
  }
  freep = dlsym(RTLD_NEXT, "free");
  if ((error = dlerror()) != NULL) {
     fputs(error,stderr);
     exit(1);
  }
callocp = dlsym(RTLD_NEXT, "calloc");
  if ((error = dlerror()) != NULL) {
     fputs(error,stderr);
     exit(1);
  }
  reallocp = dlsym(RTLD_NEXT, "realloc");
  if ((error = dlerror()) != NULL) {
     fputs(error,stderr);
     exit(1);
  }
 

}

//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void)
{
  // ...

  LOG_STATISTICS(n_allocb, n_allocb/(n_malloc+n_calloc+n_realloc), n_freeb);
  LOG_NONFREED_START();
  //LOG_BLOCK();
  item * i=list->next;
  while(i) {
    if(i->cnt >0) {
       LOG_BLOCK(i->ptr, i->size, i->cnt, i->fname, i->ofs);
    }
    i=i->next;
  }
  
  LOG_STOP();

  // free list (not needed for part 1)
  free_list(list);
}

// ...
