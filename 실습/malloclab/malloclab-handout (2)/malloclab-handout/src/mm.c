/*
 * mm-naive.c - The fast, memory-efficient malloc package.
 * 
 * In this approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. Blocks are coalesced or reused.
 * Realloc is implemented directly using mm_malloc and mm_free.
 *
 *
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t = {
	"user102",
	"¾çÇö¼­"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MAKE_SIZE(size) (ALIGN(SIZE_T_SIZE*2+size))

#define GET(p) (*(unsigned int *)(p))
#define SET(p, value) (*(unsigned int*)(p) = value)
#define HDRP(p) (p-SIZE_T_SIZE)
#define GET_ALLOC(hdr) (GET(hdr) & 0x7)
#define SET_ALLOC(hdr) (*(unsigned int *)hdr |= 0x1)

#define GET_SIZE(hdr) (GET(hdr) & ~0x7)
#define FTRP(hdr) (hdr+GET_SIZE(hdr)-SIZE_T_SIZE)
#define NEXT_HDR(hdr) (hdr+GET_SIZE(hdr))
#define PREV_HDR(hdr) (hdr-GET_SIZE(hdr-SIZE_T_SIZE))
#define HDR2PTR(hdr) (hdr+SIZE_T_SIZE)
#define HDR2FTR(hdr) (hdr+ GET_SIZE_FROM_HDR(hdr) - SIZE_T_SIZE)
#define SET_SIZE(hdr, size) SET(hdr, (size) & ~0x7)
#define SET_FREE(hdr) (*(unsigned int *) hdr &= ~0x7)
static void writeFooter(char * header) {
	char * ftrp = FTRP(header);
	memcpy(ftrp, header, SIZE_T_SIZE);
}
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
char * search_start = NULL;
void *mm_malloc(size_t size)
{
    int newSize = MAKE_SIZE(size);
    if(search_start <= (void *)0) {
//    	printf("Initializing search_start\n");
    	search_start = mem_sbrk(newSize);
    	if (search_start == (void *)-1)
			return NULL;
		SET(search_start, newSize);
		SET_ALLOC(search_start);
		writeFooter(search_start);
//		printf("Initialized search_start: %p with size %d\n", search_start, size);
		return HDR2PTR(search_start);
	}
	char * iterator = search_start;
	char * max_heap = mem_heap_hi();
	while(iterator <= max_heap-SIZE_T_SIZE*2 - newSize) {
		if(!GET_ALLOC(iterator)) {
			size_t freeSize = GET_SIZE(iterator);
			int leftSize = freeSize - newSize;
			if(leftSize >= 0) {
//				printf("Found free space from %p to %p size %d, and %d needed\n", iterator, iterator+freeSize, freeSize,  newSize);
				if(leftSize > 2 * SIZE_T_SIZE) { // allocate leftover as free
//					printf("Allocated leftover: %d \n", newSize);
					SET(iterator, newSize);
					SET_ALLOC(iterator);
					writeFooter(iterator);
					char * nextHdr = NEXT_HDR(iterator);
					SET(nextHdr, leftSize);
					writeFooter(nextHdr);	
				} else {
//					printf("Allocated the full space\n");
					SET(iterator, freeSize);
					SET_ALLOC(iterator);
					writeFooter(iterator);
				}
				return HDR2PTR(iterator);
			}
		}
		iterator = NEXT_HDR(iterator);
	}
	iterator = mem_sbrk(newSize);
//	printf("Could not find free space. Created: %p\n", iterator);
    if (iterator == (void *)-1)
		return NULL;
	SET(iterator, newSize);
	SET_ALLOC(iterator);
	writeFooter(iterator);
	return HDR2PTR(iterator);
}

void coalesce_right(char * header) {
//	printf("Coalesce right %p\n", header);
	int alloc = GET_ALLOC(header);
	char * max_heap = mem_heap_hi();
	char * iterator = NEXT_HDR(header);
	while(iterator<=max_heap-SIZE_T_SIZE*2 && !GET_ALLOC(iterator)) {
		size_t size = GET_SIZE(iterator);
		SET_SIZE(header, GET_SIZE(header) + size);
		if(alloc)
			SET_ALLOC(header);
		iterator = NEXT_HDR(iterator);
//		printf("right Iterator: %p\n", iterator);
	}
	writeFooter(header);
//	printf("Coerce right finished\n");
}

char * coalesce_left(char * header) {
//	printf("Colaesce left\n");
	if(GET_ALLOC(header)) {
		return header;
	}
	char * min_heap = mem_heap_lo();
	char * iterator = PREV_HDR(header);
//	printf("header: %p, iter %p\n", header, iterator);
	if(header == iterator)
		return header;
	char * last_valid_iter = NULL;
	while(iterator >= min_heap && !GET_ALLOC(iterator)) {
		size_t size = GET_SIZE(iterator);
//		printf("Coalesce left iterator %p, Size = %d\n", iterator, size);
		SET_SIZE(iterator, GET_SIZE(header) + size);
		last_valid_iter = iterator;
//		writeFooter(iterator);
		iterator = PREV_HDR(iterator);
		if(last_valid_iter == iterator)
			break;
	}
	if(last_valid_iter) {
		writeFooter(last_valid_iter);
	}
//	printf("Colaesce left finished");
//	mm_check();
	return last_valid_iter;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
//	printf("Free %p called \n", ptr );
	SET_FREE(HDRP(ptr));
	writeFooter(HDRP(ptr));
	coalesce_right(HDRP(ptr));
	coalesce_left(HDRP(ptr));
//	printf("Free finished\n");
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
//	printf("Realloc %p size %d\n", ptr, size);
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    char * hdr = HDRP(ptr);
    int leftSize= 0;
    
    size_t realSize=  MAKE_SIZE(size);
    
	size_t curSize = GET_SIZE(hdr);
	leftSize = curSize - realSize;
	if(leftSize >=0) {
//		printf("First try tried %p\n", ptr);
		if(leftSize >= 2*SIZE_T_SIZE) { // use left space
//			printf("First try success %p\n", ptr);
			SET(hdr, realSize);
			SET_ALLOC(hdr);
			writeFooter(hdr);
			char * nextHdr = NEXT_HDR(hdr);
			SET(nextHdr, leftSize);
			writeFooter(nextHdr);
			return ptr;	
		} else {
//			printf("First try no leftover %p\n", ptr);
			return ptr; // Nothing to do
		}
	}
	
	coalesce_right(hdr);
	
    curSize = GET_SIZE(hdr);
    leftSize= curSize - realSize;
    if(leftSize >= 0){
	    if(leftSize >= 2*SIZE_T_SIZE) {
	    	SET(hdr, realSize);
			SET_ALLOC(hdr);
			writeFooter(hdr);
			char * nextHdr = NEXT_HDR(hdr);
			SET(nextHdr, leftSize);
			writeFooter(nextHdr);
			return ptr;
		} else {
			return ptr;
		}
	}
	
	copySize = GET_SIZE(hdr) - 2*SIZE_T_SIZE;
	if(copySize > size)
		copySize = size;
		
	newptr = coalesce_left(hdr);

	if(newptr != hdr) {
		size_t newSize = GET_SIZE(newptr);
		leftSize = newSize - realSize;
		if(leftSize >= 0){
		    if(leftSize >= 2*SIZE_T_SIZE) {
		    	SET(newptr, realSize);
				SET_ALLOC(newptr);
				writeFooter(newptr);
				memmove(HDR2PTR(newptr), ptr, copySize);
				char * nextHdr = NEXT_HDR(newptr);
				SET(nextHdr, leftSize);
				writeFooter(nextHdr);
				return HDR2PTR(newptr);
			} else {
				SET(newptr, newSize);
				SET_ALLOC(newptr);
				writeFooter(newptr);
				memmove(HDR2PTR(newptr), ptr, copySize);
				return HDR2PTR(newptr);
			}
		}
	}
	
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
//    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
//    if (size < copySize)
//      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

int mm_check() {
	char * iterator = search_start;
	char * max_heap = mem_heap_hi();
	while(iterator<=max_heap-SIZE_T_SIZE*2) {
		// printf("Checking block %p.. Alloc: %d, Size: %d\n", iterator, GET_ALLOC(iterator), GET_SIZE(iterator));
		if(memcmp(iterator, FTRP(iterator), SIZE_T_SIZE)) {
			printf("node %p and footer %p differ\n", iterator, FTRP(iterator));
			for(int i=0;i<SIZE_T_SIZE; i++) {
				if(iterator[i] != FTRP(iterator)[i]) {
					printf("node[%d]:%x footer[%d]:%x\n", i, (int)node[i] , i,  (int)footer[i]);
				}
			}
			return 0;
		}
		iterator = NEXT_HDR(iterator);
	}
	printf("Last iterator: %p\n", iterator);
	return 1;
}





