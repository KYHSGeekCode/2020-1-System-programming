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


static void writeFooter(memhdr_tree_node * node);
static int mm_check();
#define GET_SIZE(hdrptr) ((hdrptr)->size & ~0x7)
#define GET_ALLOC(hdrptr) ((hdrptr)->size & 0x1)
#define SET_FREE(hdrptr) ((hdrptr)->size &= ~0x7)
#define SET_ALLOC(hdrptr) ((hdrptr)->size |= 0x1)
#define MAKE_TOTAL_SIZE(size) (ALIGN(size+ 2* sizeof(memhdr_tree_node))) 

#define HDRP(ptr) ((memhdr_tree_node *)((char*)(ptr) - sizeof(memhdr_tree_node)))
#define FTRP(ptr) ((char*)(ptr) +  GET_SIZE(HDRP(ptr)) - sizeof(memhdr_tree_node))
#define HDR2PTR(hdrptr) (((char*)hdrptr)+sizeof(memhdr_tree_node))
#define HDR2FTR(hdrptr) (((char*)hdrptr) + GET_SIZE(hdrptr) - sizeof(memhdr_tree_node))
// physically next header
//#define PHS_NEXT_HDR(hdrptr) ((memhdr_tree_node *)((char*)hdrptr + (hdrptr)->size))
// physically previous header
//#define PHS_PREV_HDR(hdrptr) ((memhdr_tree_node* )(((char*)hdrptr) - ((memhdr_tree_node*)((char*) hdrptr - sizeof(memhdr_tree_node)))->size))
static memhdr_tree_node * PHS_PREV_HDR(memhdr_tree_node * hdrptr) {
	memhdr_tree_node * prevFooter = (memhdr_tree_node *) (((char*)hdrptr)-sizeof(memhdr_tree_node));
	if(prevFooter == NULL || prevFooter > mem_heap_hi() || prevFooter < mem_heap_lo())
		return NULL;
	return (memhdr_tree_node *)(((char*)hdrptr) - GET_SIZE(prevFooter));
}

static memhdr_tree_node * PHS_NEXT_HDR(memhdr_tree_node * hdrptr) {
	if(hdrptr == NULL || hdrptr > mem_heap_hi() || hdrptr < mem_heap_lo())
		return NULL;
	memhdr_tree_node * result =  ((memhdr_tree_node *)((char*)hdrptr + GET_SIZE(hdrptr)));
	if(result == NULL || result > mem_heap_hi() || result < mem_heap_lo())
		return NULL;
	return result;
}

memhdr_tree_node * deleteMin(memhdr_tree_node * node) {
	if(node -> left == NULL) {
		return node->right;
	} else {
		node -> left = deleteMin(node->left);
		return node;
	}
}

memhdr_tree_node * deleteNode(memhdr_tree_node * node) {
	if(node->right == NULL && node -> left == NULL) {
		return NULL;
	} else if(node->left == NULL){
		return node->right;
	} else if(node -> right == NULL) {
		return node->left;
	} else {
		node -> right = deleteMin(node->right);
		return node;
	}
}

// Try coercing with physically next block
void right_coerce(memhdr_tree_node * hdr) {
	printf("right_coerce: %p\n", hdr);
//	printf("Coerce before: %d\n", GET_SIZE(hdr));
	memhdr_tree_node * nextNode =  PHS_NEXT_HDR(hdr);
	// no next node
	if(!nextNode)
		return;
	// Cannot coerce if reserved
	if(GET_ALLOC(nextNode))
		return;
	// while nextnode is  free 
	while(nextNode && !GET_ALLOC(nextNode)) {
		hdr->size = GET_SIZE(hdr) + GET_SIZE(nextNode); //coerce blocks
		nextNode -> size = 1; // mark as allocated
		// update parent & l & r
		int isInLeft = (nextNode -> parent -> left) == nextNode;
		if(nextNode -> left && nextNode -> right) {
			if(isInLeft) {
				nextNode->parent->left = nextNode -> left;
				nextNode->parent->left->right = nextNode->right;
				nextNode->left->parent = nextNode->parent;
				nextNode->right->parent = nextNode->parent->left;
				
			} else {
				nextNode->parent->right = nextNode -> left;
				nextNode->parent->right->right = nextNode->right;
				nextNode->left->parent = nextNode -> parent;
				nextNode->right->parent = nextNode-> parent->right;
			}
		} else if(nextNode -> left) {
			if(isInLeft) {
				nextNode->parent->left = nextNode->left;
				nextNode->left->parent = nextNode->parent;
			} else {
				nextNode->parent->right = nextNode -> left;
				nextNode->left->parent = nextNode->parent;
			}
		} else if(nextNode -> right) {
			if(isInLeft) {
				nextNode->parent->left = nextNode->right;
				nextNode->right->parent = nextNode->parent;
			} else {
				nextNode->parent->right = nextNode -> right;
				nextNode->right->parent = nextNode->parent;
			}
		} else {
			
		}
		// deleteNode(nextNode);
		writeFooter(nextNode->parent);
		writeFooter(nextNode);
		nextNode = PHS_NEXT_HDR(nextNode);
	}
//	printf("Coerce result: %d\n", GET_SIZE(hdr));
	writeFooter(hdr);
	printf("Coerce finished\n");
}


// link a block to an appropriate position
memhdr_tree_node * insert_tree(memhdr_tree_node * parent, memhdr_tree_node * newchild) {
//	printf("Insert_tree: parent %p", parent);
//	printf(" child %p", newchild);
//	printf("left %p right %p\n", parent->left, parent->right);
//	//	if(parent == NULL || newchild == NULL) {
	//		printf("Parent is %p, child is %p \n", parent, newchild);
	//	}
	if(parent == NULL || parent > mem_heap_hi() || parent < mem_heap_lo() /*|| GET_SIZE(parent) == 0*/) {
		parent = newchild;
	} else {
		size_t parentSize = GET_SIZE(parent);
		size_t childSize = GET_SIZE(newchild);
		if(parent->right &&  GET_SIZE(parent->right) == 0) {
			parent-> right = NULL;
		}
		if(parent->left && GET_SIZE(parent->left) == 0) {
			parent-> left = NULL;
		}
		if(childSize < parentSize) {
			parent->left = insert_tree(parent->left, newchild);
			parent->left->parent = parent;
		} else {
			memhdr_tree_node * tmp =insert_tree(parent->right, newchild);
			parent->right = tmp;
			parent->right->parent = parent;
		}
	}
	writeFooter(parent);
	return parent;
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
	printf("Writefooter %p to %p\n", node, footerAddr);
	memcpy(footerAddr, node, sizeof(memhdr_tree_node));
}

static char * find_fit(memhdr_tree_node ** parent, size_t size) {
	//printf("%p\n", *parent);
	// printf("Find_fit %d\n", size);
	size_t total_size = MAKE_TOTAL_SIZE(size);
	// printf("Total_size %d\n", total_size);
	if(*parent == NULL) { // new node created
		printf("\n[Find_fit]: parent is NULL\n");
		*parent = (memhdr_tree_node *) mem_sbrk(total_size);
		if(*parent == (void *)-1)
			return NULL;
		(*parent) -> size = total_size | 0x1; // Allocated
		(*parent) -> left = NULL;
		(*parent) -> right = NULL;
		(*parent) -> parent = *parent; // self
//		PHS_NEXT_HDR(*parent)-> size=0;
		writeFooter(*parent);
		return HDR2PTR(*parent);
	} else if(!GET_ALLOC(*parent)) { // found a free block allocated previously
		size_t origSize = GET_SIZE(*parent);
		if(origSize >= total_size) { // have enough space 
			(*parent) -> size = total_size | 0x1; // use it
			writeFooter(*parent); 
			if(origSize - total_size >= 2*sizeof(memhdr_tree_node)) { // create a block for leftover
				// printf("Empty size: %d\n", origSize - total_size);
				memhdr_tree_node * nextHdr =  PHS_NEXT_HDR(*parent);
				nextHdr-> size = origSize - total_size;
				nextHdr-> left = NULL;
				nextHdr-> right = NULL;
				nextHdr -> parent = *parent;
				insert_tree(*parent, nextHdr); // and enlist it
				writeFooter(nextHdr);
			}
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
		(*parent)->right->parent = *parent;
		writeFooter((*parent)->right);
	} else {
		result = find_fit(&((*parent)->left), size);
		(*parent)->left->parent = *parent;
		writeFooter((*parent)->left);		
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
//	if(block != NULL) {
//		HDRP(block)->parent = root;
//	}
//	if(block != NULL) {
		// place_block(block, size);
//	if(!mm_check()) {
//		printf("mm_check from mm_malloc(%d) failed. block=%p\n", size, block);
//		exit(0);
//	}
	
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
 * mm_free - Mark the block as free and try coercing with other blocks
 */
void mm_free(void *ptr)
{
	memhdr_tree_node * header = HDRP(ptr);
	printf("Free %p hdr %p\n", ptr, header);
	SET_FREE(header);
	writeFooter(header);
	// Coerce right
	//memhdr_tree_node * phy_next = PHS_NEXT_HDR(header);
	right_coerce(header);
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
    
    size_t oldSize = GET_SIZE(HDRP(ptr));
    size_t newSize = MAKE_TOTAL_SIZE(size);
	size_t copySize;
    
    if(oldSize >= newSize) {
    	HDRP(ptr)->size = newSize | 0x1;
    	writeFooter(HDRP(ptr));
		return ptr; 
	}
    
    right_coerce(HDRP(ptr));
    if(GET_SIZE(HDRP(ptr)) >= newSize) {
    	printf("Coerce success : %d\n", newSize - GET_SIZE(HDRP(ptr)));
    	HDRP(ptr)->size = newSize | 0x1;
    	writeFooter(HDRP(ptr));
		return ptr;
	}
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

int mm_check() {
	int err = 0;
	if(root == NULL) {
		printf("Root is NULL\n");
		return 0;
	}
	if(root > mem_heap_hi() || root < mem_heap_lo()) {
		printf("Root out of range :%p\n", root);
		return 0;
	}
	if(root->left != NULL) {
		if(!mm_check_sub(root->left)) {
			printf("Check for left node %p failed\n", root->left);
			err = 1;
		}
	}
	if(root->right != NULL) {
		if(!mm_check_sub(root->right)) {
			printf("Check for right node %p failed\n", root->right);
			err =1;
		}
	}
	if(root->parent!=NULL && root->parent != root ) {
		printf("Root parent is wrong: %p, root: %p\n", root->parent, root);
		err = 1;
	}
	if(err)
		return 0;
	return 1;
}

int mm_check_sub(memhdr_tree_node *node) {
	int err = 0;
	printf("Checking node %p ===============\n", node);
	printf("Heap lo: %p, Heap hi: %p\n", mem_heap_lo(), mem_heap_hi());
	if(node == NULL) {
		printf("Checker is 0: node is NULL\n");
		return 0;
	}
	if(node > mem_heap_hi() || node < mem_heap_lo()) {
		printf("Node out of range: %p\n");
		return 0;
	}
	if(GET_ALLOC(node)) {
		printf("The node %p is allocated: size = %d\n", node ,GET_SIZE(node));
	} else {
		printf("The node %p is not allocated: size = %d\n", node, GET_SIZE(node));
	}
		
	if(node->parent == NULL) {
		printf("parent of %p is NULL\n", node);
		return 0;
	}
	if(node->parent > mem_heap_hi() || node->parent < mem_heap_lo()) {
		printf("invalid parent %p for node %p\n", node->parent, node);
		return 0;
	}
	if(node->parent->left== node) {
		printf("The node %p is a left node of parent %p\n", node, node->parent);
	}
	if(node->parent->right==node) {
		printf("The node %p is a right node of parent %p\n", node, node->parent);
	}
	
	if(node->left != NULL) {
		if(node->left > mem_heap_hi() || node->left < mem_heap_lo()) {
			printf("The node %p has wrong childs: left: %p right: %p\n", node, node->left, node->right);
			return 0;
		}
		printf("The node %p has left: %p, size = %d\n", node, node->left, GET_SIZE(node->left));
		if(!mm_check_sub(node->left)) {
			printf("Check for left node %p failed\n", node->left);
			err = 1;
		}
	}
	if(node->right != NULL) {
		if(node->right > mem_heap_hi() || node->right < mem_heap_lo()) {
			printf("The node %p has wrong childs: left: %p right: %p\n", node, node->left, node->right);
			return 0;
		}
		printf("The node %p has right: %p, size = %d\n", node, node->right, GET_SIZE(node->right));
		if(!mm_check_sub(node->right)) {
			printf("Check for right node %p failed\n", node->right);
			err = 1;
		}
	}
	memhdr_tree_node * footer = HDR2FTR(node);
	if(memcmp(node, footer, sizeof(memhdr_tree_node))) {
		printf("node %p and footer %p differ\n", node, footer);
		for(int i=0;i<sizeof(memhdr_tree_node); i++) {
			if(((char*)node)[i] != ((char*)footer)[i]) {
				printf("node[%d]:%x footer[%d]:%x\n", i, (int)((char*)node)[i] , i,  (int)((char*)footer)[i]);
			}
		}
		return 0;
	}

	printf("=====End for node %p======\n", node);
	if(err)
		return 0;
	return 1;
}
