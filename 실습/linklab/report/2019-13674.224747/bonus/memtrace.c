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
   item * tobedeallocated = find(list, ptr);
   if(tobedeallocated) {
      if(tobedeallocated -> cnt <=0) {
         LOG_DOUBLE_FREE();
	 return;
      }
      n_freeb += tobedeallocated->size; 
      freep(ptr);
      dealloc(list, ptr);
   } else {
     LOG_ILL_FREE();
   }
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
   void * resultP = NULL;
   int double_free = 0;
   int illegal_free = 0;
   n_realloc++;
   item * tobedeallocated = find(list, ptr);
   if(tobedeallocated) {
      if(tobedeallocated->cnt<=0) {
         double_free = 1;
	 resultP = reallocp(NULL, new_size);
	 // LOG_DOUBLE_FREE();
      } else {
         n_freeb += tobedeallocated->size;
         dealloc(list, ptr);
         resultP = reallocp(ptr, new_size);
         // LOG_REALLOC(ptr, new_size, resultP);
      }
   } else {
     illegal_free = 1;
     resultP = reallocp(NULL, new_size);
   }
   LOG_REALLOC(ptr, new_size, resultP);
   if(resultP) {
      n_allocb += new_size;
      alloc(list, resultP, new_size);
   }
   if(double_free) {
      LOG_DOUBLE_FREE();
   }
   if(illegal_free) {
      LOG_ILL_FREE();
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
  int n = n_malloc + n_calloc + n_realloc;
  int avg = n? n_allocb / n : 0;
  LOG_STATISTICS(n_allocb, avg, n_freeb);
  item * i=list->next;
  if(i && i->cnt >0) {
    LOG_NONFREED_START();
  }
  //LOG_BLOCK();
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
