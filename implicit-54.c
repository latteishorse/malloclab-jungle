#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

team_t team = {
    /* Team name */
    "SWJungle",
    /* First member's full name */
    "latteishorse",
    /* First member's email address */
    "week06",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

// macro
#define ALIGNMENT 8

#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4  
#define DSIZE 8 
#define CHUNKSIZE (1<<12)

#define MAX(x,y) ((x) > (y) ? (x) : (y)) 
#define MIN(x,y) ((x) < (y) ? (x) : (y))

// Pack a size and allocated bit into a word
/*
alloc: 가용 여부
size: block size
*/
#define PACK(size,alloc) ((size)|(alloc)) 


// Read and write a word at address p 
#define GET(p) (*(unsigned int*)(p)) // 주소값 조회
#define PUT(p,val) (*(unsigned int*)(p)=(int)(val)) // 블록의 주소를 넣음

// Read the size and allocation bit from address p 
#define GET_SIZE(p) (GET(p) & ~0x7) 
// ~ -> 역수
// 헤더에서 블록 size만 조회

#define GET_ALLOC(p) (GET(p) & 0x1) 
// 00000001 -> 헤더 가용 여부 판단
// 1 -> allocated

// Address of block's header and footer 
#define HDRP(bp) ((char*)(bp) - WSIZE) 
// bp가 어디에 있던 상관없이 WSIZE 앞에 위치

#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)


// Address of (physically) next and previous blocks 
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp)-WSIZE))) 
// bp에서 현재 블록 크기만큼 더함 -> 그 다음 블록 헤더 다음으로 이동

#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE))) 

// global var
static char *heap_listp;

// functions
static void *extend_heap(size_t);
static void *coalesce(void *);
static void *find_fit(size_t);
static void place(void *, size_t);

/*
---------------------------
 패딩 다음에는 prolog 블록
    - 프롤로그는 8바이트 할당 블록

prolog는 스택의 프레임을 생성하기위한 과정
 - 다음 어셈블리어로 스택 프레임 구성
 - push ebp
 - mov ebp, esp

초기화 과정에서 생성되며, 반환하지 않는다.
프롤로그 다음 블록은 일반 블록
---------------------------
힙은 항상 에필로그로 끝남
 - 헤더만으로 구성된 크기가 0으로 할당된 블록

*/


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)

        return -1;

    PUT(heap_listp, 0); // padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // prolog -> double word size & allocated 1
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // prolog footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); // epilogue header
    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/*
* extend heap
* 1) 힙이 초기화 될 때
* 2) mm_malloc이 적당한 fit을 찾지 못했을 때 
*/
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // 인접한 힙을 2워드의 배수로 반올림, 그 후에 메모리 시스템으로부터 추가적인 힙 공간을 요청
    if  ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    // HDRP bp -> wsize 앞으로
    PUT(HDRP(bp), PACK(size, 0)); // free block header

    // FTRP footer의 시작 위치
    PUT(FTRP(bp), PACK(size, 0)); // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); //추가한 블럭, allocate 

    return coalesce(bp);
}
/*
*  coalesce - 블록 연결
*/
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 전 블록 가용 여부 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록 가용 여부 확인
    size_t size = GET_SIZE(HDRP(bp)); // 현재 블록 사이즈 확인

// Case 1
    // 이전과 다음 블록이 모두 할당되어 있음
    // 독립된 블록으로 유지
    if (prev_alloc && next_alloc)
    {
        return bp;
    }

// Case 2
    // 이전 블록 할당, 다음 블록 가용
    // 현재 블록 다음 블록과 합침
    else if (prev_alloc && !next_alloc) 
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

// Case 3
    // 이전 블록 가용, 다음 블록 할당
    // 앞 블록과 합침
    else if (!prev_alloc && next_alloc) 
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

// Case 4
    // 앞, 뒤 모두 가용
    // 3개의 블록을 합침
    else 
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
                GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
    {
        return NULL; // -> 무시
    }

    if (size <= DSIZE)
    {
        asize = 2*DSIZE;
    }
    else
    {
        // size보다 클 때 -> 큰 범위 내에서 만족하는 가장 작은 값 (8*n + (0~7))으로 분할 가능
        // size + (header) + (padding)
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }
    if ((bp = find_fit(asize)) != NULL) // 들어갈 free block이 있다면 넣어준다
    {
        place(bp, asize); // asize로 할당
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
* find_fit -> using first fit 
* search non
*/
static void *find_fit(size_t asize)
{
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) 
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) 
        {
            return bp;
        }
    }
    return NULL; 
}

/*
* place
*/
static void place(void *bp, size_t asize)
{ 
    size_t csize = GET_SIZE(HDRP(bp)); //현재 블록 사이즈
    if ( (csize-asize) >= (2 * DSIZE)) // 현재 블록에 asize만큼 넣어도 2 * DSIZE 이상 남는지를 확인
    // 남으면 다른 data 넣을 수 있다
    { 
        PUT(HDRP(bp), PACK(asize,1)); // 헤더 위치에 asize 만큼 넣음 and allocated flag
        PUT(FTRP(bp), PACK(asize,1)); // footer 위치에

        bp = NEXT_BLKP(bp); // 다음 블록으로 이동하여 bp 갱신
        PUT(HDRP(bp), PACK(csize-asize,0)); 
        PUT(FTRP(bp), PACK(csize-asize,0)); 
    }
    else
    {
        // 자리가 없다면
        // 현재 사이즈에서 헤더, 푸터 할당
        PUT(HDRP(bp), PACK(csize,1)); 
        PUT(FTRP(bp), PACK(csize,1));
    }
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
// CLRS code
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;

    copySize = GET_SIZE(HDRP(oldptr));

    if (size < copySize)
    {
        copySize = size;
    }

    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

