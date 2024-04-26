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
team_t team = {
    /* Team name */
    "None",
    /* First member's full name */
    "None",
    /* First member's email address */
    "None",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double word size (bytes) */
#define FSIZE 16
#define CHUNKSIZE (1 << 12) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p 第一个字节*/
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer 是p后面的bp*/
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

// 空闲块中的前一个和后一个偏移量
#define PREV_VAL(bp) ((int)(*bp))
#define NEXT_VAL(bp) ((int)(*(bp + 1)))

// 空闲块中的前一个和后一个指针
#define PREV(bp) ((char *)(bp))
#define NEXT(bp) ((char *)(bp + WSIZE))

static void *extend_heap(size_t words);
// static void *next_fit(size_t asize);
static void *first_fit(size_t asize);
static void place(void *bp, size_t asize);
static void *coalesce(void *bp);
static void fix_ptr(void *bp);
static void make_LIFO(char *bp);

static char *heap_listp;
static char *root;

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE + 2 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                            /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), 0);              //前驱大小
    PUT(heap_listp + (1 * WSIZE + 1 * WSIZE), 0);              //后继大小
    PUT(heap_listp + (1 * WSIZE + 2 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE + 2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE + 2 * WSIZE), PACK(0, 1));     /* Epilogue header */
    root = heap_listp + (1 * WSIZE);
    heap_listp += (4 * WSIZE);

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment 8字节对齐*/
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* 新的尾部 */

    PUT(PREV(bp), 0); //前驱置为0
    PUT(NEXT(bp), 0); //后继置为0

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

static void place(void *bp, size_t asize)
{
    size_t size = GET_SIZE(HDRP(bp)); //当前块的大小
    fix_ptr(bp); //修正指针

    if ((size - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(size - asize, 0));
        PUT(FTRP(bp), PACK(size - asize, 0));

        PUT(PREV(bp), 0); //前驱置为0
        PUT(NEXT(bp), 0); //后继置为0
        make_LIFO(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
    }
    // pre_listp = bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    if(bp == 0)
    {
        return;
    }

    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    PUT(PREV(bp), 0); //free的时候还用置为0吗？？？
    PUT(NEXT(bp), 0);

    coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); //前一个块的尾部
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); //后一个块的头部
    size_t size = GET_SIZE(HDRP(bp)); //当前块的大小

    if (prev_alloc && next_alloc) //都是已分配的情况
    { // Case 1
        // pre_listp = bp;
        // return bp;，这里不需要返回，因为还要LIFO
    }

    if (prev_alloc && !next_alloc) 
    { /* Case 2 下一个是空的*/
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); //合并大小
        fix_ptr(NEXT_BLKP(bp)); //修正指针
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc)
    { /* Case 3 上一个是空的*/
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        fix_ptr(PREV_BLKP(bp)); //修正指针
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp); //指针指向新的块
    }
    else
    { /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
                GET_SIZE(FTRP(NEXT_BLKP(bp)));
        fix_ptr(PREV_BLKP(bp)); //修正指针
        fix_ptr(NEXT_BLKP(bp)); //修正指针

        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // pre_listp = bp;
    make_LIFO(bp);

    return bp;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE) //调整为至少16个字节
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); //调整为8的倍数

    /* Search the free list for a fit */
    if ((bp = first_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* 放不下的话，扩展新空间，在新的块里面放置 */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
        return mm_malloc(size);
    if (size == 0)
    {
        mm_free(ptr);
        return NULL;
    }
    size_t adjust_size = ALIGN(size); 
    size_t old_size = GET_SIZE(HDRP(ptr));
    if (adjust_size < old_size)
    {
        return ptr;
    }
    // if(!GET_ALLOC(NEXT_BLKP(ptr)))
    // {
    //     void *next = NEXT_BLKP(ptr);
    //     size_t next_size = GET_SIZE(HDRP(next));
    //     size_t size = old_size + next_size + WSIZE;
    //     if(adjust_size <= size)
    //     {
    //         PUT(HDRP(ptr), PACK(size, 0));
    //         PUT(FTRP(next), PACK(size, 0));
    //         if(next == pre_listp)
    //         {
    //             pre_listp = ptr;
    //         }
    //         place(ptr, adjust_size);
    //         return ptr;
    //     }
    // }
    // else if(!GET_ALLOC(PREV_BLKP(ptr)))
    // {
    //     void *prev = PREV_BLKP(ptr);
    //     size_t pre_size = GET_SIZE(HDRP(prev));
    //     size_t size = old_size + pre_size + WSIZE;
    //     if(adjust_size <= size)
    //     {
    //         PUT(HDRP(prev), PACK(size, 0));
    //         PUT(FTRP(ptr), PACK(size, 0));
    //         if(ptr == pre_listp)
    //         {
    //             pre_listp = prev;
    //         }
    //         place(prev, adjust_size);
    //         return prev;
    //     }
    // }
    
    void *newptr;
    size_t copySize;
    newptr = mm_malloc(size); //malloc返回的是bp
    if (newptr == NULL)
        return NULL;
    size = GET_SIZE(HDRP(ptr));
    copySize = GET_SIZE(HDRP(newptr));
    if (size < copySize) //如果原来的块比新的块小,就只复制原来块的大小
        copySize = size;
    memmove(newptr, ptr, copySize - WSIZE); //复制原来块的内容到新的块,不包括头部
    mm_free(ptr);
    return newptr;
}

static void *first_fit(size_t asize)
{
    char *bp = root;
    while (bp != 0)
    {
        if (GET_SIZE(HDRP(bp)) >= asize)
        {
            return bp;
        }
        bp = bp + NEXT_VAL(bp);
    }
    return NULL;
}

static void fix_ptr(void *bp)
{
    if (bp == NULL || GET_ALLOC(HDRP(bp))) //如果是已分配的块，不需要修正
    {
        printf("bp is null or allocated\n"); //正常情况下不会进入这里
        return;
    }

    int prev_val = GET(PREV(bp));
    int next_val = GET(NEXT(bp));

    if (prev_val == 0) //前一个是root
    {
        int next_offset;
        if(next_val)
        {
            char* next = bp + next_val;
            next_offset = (int)(next - root);
            PUT(PREV(next), next_offset);
        }
        next_offset = prev_val + next_val;
        PUT(root, -next_offset);
    }
    else
    {
        char* prev = bp + prev_val;
        char* next = bp + next_val;
        if(next_val)
        {
            int next_offset = (int)(next - prev);
            PUT(PREV(next), next_offset);
        }
        int prev_offset = (int)(prev - next);
        PUT(NEXT(prev), prev_offset);
    }
    
}

static void make_LIFO(char *bp)
{
    int old_head_offset = GET(root);
    int new_head_offset = (int)(bp - root);
    if (old_head_offset != 0) //有后续空节点
    {
        char* old_head = root + old_head_offset;
        PUT(PREV(old_head), old_head_offset - new_head_offset); //旧的头节点的前驱指向新的头节点
    }
    PUT(NEXT(bp), new_head_offset - old_head_offset); //新的头节点的后继指向旧的头节点
    PUT(PREV(bp), new_head_offset); //新的头节点的前驱指向root
    PUT(root, new_head_offset); //root指向新的头节点

}