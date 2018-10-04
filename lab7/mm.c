/* 516030910444   Li Ying

 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 *
 * I want to use the explicit free list as the strcture to save info.
 * My ideal practice is use 8 bytes next to the header to store the 
 * position of previous free block and next free block. 
 * The ideal structure of free block:
 * |header|pos of previous free block|pos of next free block|...|footer| 
 * The structure of allocted block is the same as the book:
 * |header|...|footer|
 *
 * However, segmentation fault always exists, so I have to give up my 
 * initial idea. Finally, the structure is elementary and the same as the book.
 * Although I don't use the structure of free list eventually,I still use some 
 * special methods to improve its utilization and speed, such as adding a branch
 * in the function of realloc to apply for more space instead of call malloc again.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define WSIZE 4
#define DSIZE 8
//#define CHUNKSIZE (1<<12)

#define MAX(x,y)((x) > (y)? (x):(y))
#define PACK(size,alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p,val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


static void *heap_listp;
static void *heap_tailp;
static void *temp_ptr;
static int trace = 0;
static int CHUNKSIZE;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

static int mm_check(void *bp);
static int mm_check_addr(void *bp);
static int mm_check_overlap(void);
static int mm_check_coalesce(void *bp);


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
	if (trace % 12 == 1)
		CHUNKSIZE = 1<<6 ;
	else
		CHUNKSIZE = 1<<20;
	trace ++;
	temp_ptr = NULL;
	if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1)
		return -1;
	PUT(heap_listp,0);
	PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1));
	PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1));
	PUT(heap_listp + (3*WSIZE), PACK(0,1));
	heap_listp += (2*WSIZE);
    heap_tailp = heap_listp + (2*WSIZE);
    return 0;
}


// this function is used to extend the heap, which is the same as that in the book.
static void *extend_heap(size_t words)
{
	char *bp;
	size_t size;

	size = (words % 2)?(words+1) * WSIZE : words * WSIZE;
	if ((long)(bp = mem_sbrk(size)) == -1)
		return NULL;

	PUT(HDRP(bp), PACK(size,0));
	PUT(FTRP(bp), PACK(size,0));
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));
	heap_tailp = NEXT_BLKP(bp);
	return coalesce(bp);
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * te a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
	size_t asize;
	size_t extendsize;
	char *bp = NULL;

	if (size == 0)
		return NULL;
	if (size <= DSIZE)
		asize = 2 * DSIZE;
	else
		asize = DSIZE*((size + (DSIZE) + (DSIZE -1)) / DSIZE);

	if((bp = find_fit(asize)) != NULL)
	{
		place(bp, asize);
		return bp;
	}
	extendsize = MAX(asize, CHUNKSIZE);
	if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
		return NULL;
	place(bp, asize);
	return bp;
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
	//if (ptr == NULL)
	//	return;
	size_t size = GET_SIZE(HDRP(ptr));
	PUT(HDRP(ptr), PACK(size,0));
	PUT(FTRP(ptr), PACK(size,0));
	coalesce(ptr);
}


// this function is used to coalesce the free blocks.
static void *coalesce(void *bp)
{
	size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
	size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
	size_t size = GET_SIZE(HDRP(bp));

	if (prev_alloc && next_alloc)
		return bp;

	else if (prev_alloc && !next_alloc)
	{
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		PUT(HDRP(bp), PACK(size,0));
		PUT(FTRP(bp), PACK(size,0));
	}

	else if(!prev_alloc && next_alloc)
	{
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(FTRP(bp), PACK(size,0));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
		bp = PREV_BLKP(bp);
	}

	else
	{
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
		bp = PREV_BLKP(bp);
	}
	return bp;
}


// this function is used to find the space that can be used to malloc 
// and if there doesn't exist a suitable space then return NULL

// I use a global variable temp_ptr to record the position of last malloc
// so that I can use the remaining space instead of search all blocks again. 
static void *find_fit(size_t asize)
{
	void *bp;
	for (bp = PREV_BLKP(heap_tailp); bp != heap_listp; bp = PREV_BLKP(bp))	
	{
		if (bp == temp_ptr)
			continue;
		if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
		{
			if (asize >= 128)
				temp_ptr = NULL;
			else 
				temp_ptr = bp;
			return bp;
		}
	}	
	temp_ptr = NULL;
	return NULL;
}


static void place(void *bp, size_t asize)
{
	size_t csize = GET_SIZE(HDRP(bp));
	if ((csize - asize) >= (2*DSIZE))
	{
		PUT(HDRP(bp), PACK(asize,1));
		PUT(FTRP(bp), PACK(asize,1));
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize - asize, 0));
		PUT(FTRP(bp), PACK(csize - asize, 0));
	}
	else
	{
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
	}
}


/*
// * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
// */
void *mm_realloc(void *ptr, size_t size)
{
	unsigned int old_size = GET_SIZE(HDRP(ptr));
	if (GET_SIZE(HDRP(NEXT_BLKP(ptr))) == 0)
	{
		if (size == 0)
			return NULL;
		if (size < 16)
			size = 16;
		else
			size = DSIZE*((size + (DSIZE) + (DSIZE -1)) / DSIZE);
		if (size > old_size)
		{
			mem_sbrk(size - old_size);
			PUT(HDRP(ptr),PACK(size,1));
			PUT(FTRP(ptr),PACK(size,1));
			PUT(HDRP(NEXT_BLKP(ptr)),PACK(0,1));
			heap_tailp = NEXT_BLKP(ptr);
		}
		return ptr;
	}

	void *oldptr = ptr;
	void *newptr;
	size_t copySize;

	newptr = mm_malloc(size);
	if (newptr == NULL)
		return NULL;
	copySize = *(size_t *)((char*)oldptr - SIZE_T_SIZE);
	if (size < copySize)
		copySize = size;
	memcpy(newptr,oldptr,copySize);
	mm_free(oldptr);
	return newptr;
}


// this function is used to check whether there are faults
static int mm_check(void *bp)
{
	int a1 = mm_check_addr(bp);
	int a2 = mm_check_overlap();
	int a3 = mm_check_coalesce(bp);
	if (a1 && a2 && a3)
		return 1;
	return 0;
}


// this function is used to check whether the address is valid
static int mm_check_addr(void *bp)
{
	long addr = (long)bp;
	long low_addr = (long)heap_listp;
	long high_addr = low_addr;
	for (bp = heap_listp; GET_SIZE(HDRP(bp))>0; bp = NEXT_BLKP(bp))
		high_addr += GET_SIZE(HDRP(bp));
	if ((addr > low_addr) && (high_addr > addr))
		return 1;
	else
	{
		printf("Invalid address!");
		return 0;
	}
}


// this function is used to check whether blocks are overlapped
static int mm_check_overlap()
{
	void *bp;
	for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
	{
		if (NEXT_BLKP(bp) != NULL)
		{
			if (GET_SIZE(HDRP(bp)) + (HDRP(bp)) != (HDRP(NEXT_BLKP(bp))))
			{
				printf("Blocks are overlapped!");
				return 0;
			}
		}
		if (PREV_BLKP(bp) != NULL)
		{
			if (GET_SIZE(HDRP(PREV_BLKP(bp))) + (HDRP(PREV_BLKP(bp))) != (HDRP(bp)))
			{
				printf("Blocks are overlapped!");
				return 0;
			}
		}
	}
	return 1;
}


// this function is used to check whether free blocks are not coalesced
static int mm_check_coalesce(void *bp)
{
	size_t block_size = GET_SIZE(HDRP(bp));
	bp += block_size;
	if (GET_ALLOC(HDRP(bp)) == 0)
	{
		printf("Free blocks are not coalesced!");
		return 0;
	}	
	return 1;
}







