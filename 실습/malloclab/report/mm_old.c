/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

typedef struct memhdr_tree_node_t{
	size_t size;
	struct memhdr_tree_node_t *parent;
	struct memhdr_tree_node_t *left;
	struct memhdr_tree_node_t *right;
} __attribute__((aligned(ALIGNMENT))) memhdr_tree_node;

static char* heap_start = NULL;
static char * heap_end = NULL;
static memhdr_tree_node * root = NULL;

#define GET_SIZE(hdrptr) ((hdrptr)->size & ~0x7)
#define GET_ALLOC(hdrptr) ((hdrptr)->size & 0x1)
#define SET_FREE(hdrptr) ((hdrptr)->size &= ~0x7)
#define SET_ALLOC(hdrptr) ((hdrptr)->size |= 0x1)
#define MAKE_TOTAL_SIZE(size) (ALIGN(size+ 2* sizeof(memhdr_tree_node))) 

#define HDRP(ptr) ((memhdr_tree_node *)((char*)(ptr) - sizeof(memhdr_tree_node)))
#define FTRP(ptr) ((char*)(ptr) +  GET_SIZE(HDRP(ptr)) - sizeof(memhdr_tree_node))
#define HDR2PTR(hdrptr) (((char*)hdrptr)+sizeof(memhdr_tree_node))
// physically next header
//#define PHS_NEXT_HDR(hdrptr) ((memhdr_tree_node *)((char*)hdrptr + (hdrptr)->size))
// physically previous header
//#define PHS_PREV_HDR(hdrptr) ((memhdr_tree_node* )(((char*)hdrptr) - ((memhdr_tree_node*)((char*) hdrptr - sizeof(memhdr_tree_node)))->size))
static memhdr_tree_node * PHS_PREV_HDR(memhdr_tree_node * hdrptr) {
	memhdr_tree_node * prevFooter = (memhdr_tree_node *) (((char*)hdrptr)-sizeof(memhdr_tree_node));
	if(prevFooter == NULL || prevFooter > mem_heap_hi() || prevFooter < mem_heap_lo())
		return NULL;
	return (memhdr_tree_node * )(((char*)hdrptr) - GET_SIZE(prevFooter));
}

static memhdr_tree_node * PHS_NEXT_HDR(memhdr_tree_node * hdrptr) {
	memhdr_tree_node * result =  ((memhdr_tree_node *)((char*)hdrptr + GET_SIZE(hdrptr)));
	if(result == NULL || result > mem_heap_hi() || result < mem_heap_lo())
		return NULL;
	return result;
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	heap_start = mem_heap_lo();
	heap_end = mem_heap_hi();
	//mem_sbrk(sizeof(memhdr_tree_node)*2);
	root = NULL; //(memhdr_tree_node *) heap_start;
	return 0;
}

static void writeFooter(memhdr_tree_node * node) {
	if(node > mem_heap_hi() || node < mem_heap_lo())
		return;
	char* footerAddr = ((char*)node)+GET_SIZE(node)-sizeof(memhdr_tree_node);
	if(footerAddr > mem_heap_hi() || footerAddr < mem_heap_lo())
		return;
	memcpy(footerAddr, node, sizeof(memhdr_tree_node));
}

static char * find_fit(memhdr_tree_node ** parent, size_t size) {
	//printf("%p\n", *parent);
	// printf("Find_fit %d\n", size);
	size_t total_size = MAKE_TOTAL_SIZE(size);
	// printf("Total_size %d\n", total_size);
	if(*parent == NULL) { // new node created
		*parent = (memhdr_tree_node *) mem_sbrk(total_size);
		if(*parent == (void *)-1)
			return NULL;
		(*parent) -> size = total_size | 0x1; // Allocated
		(*parent) -> left = NULL;
		(*parent) -> right = NULL;
//		PHS_NEXT_HDR(*parent)-> size=0;
		writeFooter(*parent);
		return HDR2PTR(*parent);
	} else if(!GET_ALLOC(*parent)) {
		if(GET_SIZE(*parent) >= total_size) {
			(*parent) -> size = total_size | 0x1;
			writeFooter(*parent);
			return HDR2PTR(*parent);
		}  else {
			
			//free, but not enough space
		}
	} 
	// already allocated
	char * result; // = *parent; // test for *
	// MAGIC
	//(*parent)->left = NULL;
	//(*parent)->right = NULL;
	if((*parent)->right &&  GET_SIZE((*parent)->right) == 0) {
		(*parent)-> right = NULL;
	}
	if((*parent)->left && GET_SIZE((*parent)->left) == 0) {
		(*parent)-> left = NULL;
	}
	if(total_size > GET_SIZE(*parent)) {
		result = find_fit(&((*parent)->right), size); // allocate new and set as right if this node did not have data
	} else {
		result = find_fit(&((*parent)->left), size);		
	}
	writeFooter(*parent);
	//HDRP(result) -> left = NULL;
	//HDRP(result) -> right = NULL;
	return result;

	
//	 if((!GET_ALLOC(*parent)) && ) {
//		size_t old_parent_size = GET_SIZE(*parent);
//		SET_ALLOC(*parent);
		
		//(*parent) -> left = NULL;
		//(*parent) -> right = NULL;
//		// split leftover as a new block if available
//		if(old_parent_size - total_size > 2*sizeof(memhdr_tree_node) + ALIGNMENT) {
//			memhdr_tree_node * phy_next = PHS_NEXT_HDR(*parent);
//			phy_next->size = (old_parent_size - total_size) & ~0x7;
//			phy_next->left = NULL;
//			phy_next->right = NULL;
//			find_fit(&root, phy_next->size - 2*sizeof(memhdr_tree_node));
//		}
//		// reorder tree           
//		writeFooter(*parent);
//		return HDR2PTR(*parent);
//	}
//	 else if(!GET_ALLOC(*parent)){
//		memhdr_tree_node * hdr = *parent;
//		while(hdr != NULL && ((char*)hdr) < mem_heap_hi() - 2*sizeof(memhdr_tree_node) &&  !(GET_ALLOC(hdr))) {
//			(*parent)->size += GET_SIZE(hdr);
//			(*parent)->size &= ~0x7;
//			hdr = PHS_NEXT_HDR(hdr);
//		}
//		writeFooter(*parent);
//	}
////	
//	//if(!GET_ALLOC(*parent)) {
////		(*parent)->right = NULL;
////		(*parent)->left = NULL;
//	//}
//	char * result;
//	if(total_size > GET_SIZE(*parent)) {
//		result = find_fit(&((*parent)->right), size);
//	} else {
//		result = find_fit(&((*parent)->left), size);		
//	}
//		HDRP(result) -> parent = *parent;
	//return result;
	
}

/* 
 * mm_malloc - to find the best fit, use always sorted data structure like tree.
 * search the best fit place from the tree, and increase heap size if not found. 
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	if(size == 0)
		return NULL;
		
	char * block = find_fit(&root, size);
	if(block != NULL) {
		HDRP(block)->parent = root;
	}
	
//	if(block != NULL) {
		// place_block(block, size);
		return block;
//	}
//	return NULL;
//    int newsize = ALIGN(size + SIZE_T_SIZE);
//    void *p = mem_sbrk(newsize);
//    if (p == (void *)-1)
//		return NULL;
//    else {
//        *(size_t *)p = size;
//        return (void *)((char *)p + SIZE_T_SIZE);
//    }
}

/*
 * mm_free - Freeing a block does nothing but mark the block as free
 */
void mm_free(void *ptr)
{
	memhdr_tree_node * header = HDRP(ptr);
	SET_FREE(header);
	writeFooter(header);
	// Coerce right
	memhdr_tree_node * phy_next = PHS_NEXT_HDR(header);
//	printf("next:%p\n", phy_next);
//	if(phy_next &&  !GET_ALLOC(phy_next)) { // not allocated and exists
//		header-> size = GET_SIZE(header) + GET_SIZE(phy_next);
//		phy_next-> size = 1; // fake
//		// TODO: update tree
//		writeFooter(header);
//	}
//	memhdr_tree_node * phy_prev = PHS_PREV_HDR(header);
////	printf("prev:%p\n", phy_prev);
//	if(phy_prev &&!GET_ALLOC(phy_prev)) {
//		phy_prev -> size = GET_SIZE(header) + GET_SIZE(phy_prev);
//		header->size = 1; // fake
////		if(header->parent!=NULL) {
////			if(header->parent->left == header) {
////				header->parent->left = phy_prev;
////			} else if(header->parent->right == header) {
////				header->parent->right = phy_prev;
////			}
////			phy_prev->parent = header->parent;
////		}
//		// TODO: Update tree   
//		writeFooter(phy_prev);
//	}
	// reorder tree
	if(header -> left == NULL) {
		if(header -> right == NULL) {
			
		}
	}
	//header -> left = NULL;
	//header -> right = NULL;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
//	printf("realloc");
	if(ptr == NULL)
		return mm_malloc(size);
	if(size == 0) {
		mm_free(ptr);
		return NULL;
	}
	
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(ptr)) - 2* sizeof(memhdr_tree_node);//*(size_t *)((char *)oldptr - 2*sizeof(memhdr_tree_node));
    if (size < copySize)
      copySize = size;
    if(newptr != oldptr)
	    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

