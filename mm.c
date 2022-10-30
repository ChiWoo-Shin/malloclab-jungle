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
    "No.2",
    /* First member's full name */
    "ChiWoo Shin",
    /* First member's email address */
    "shin8037@naver.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//Basic constants and macros
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

// Max 함수 정의
#define MAX(x,y) ((x)>(y)? (x):(y))

// Pack a size and allocated bit into a word
#define PACK(size, alloc) ((size)|(alloc)) // size와 alloc을 bit로 변환후 or로 합침

// Read and write a word at address p
#define GET(p) (*(unsigned int *)(p)) // *p의 주소를 읽고 씀
#define PUT(p, val) (*(unsigned int *)(p)=(val)) // *p에 val 값을 할당

// Read the size and allocated fields from address p
#define GET_SIZE(p) (GET(p) & ~0x7) // 7의 2진법 : 111 여기서 not : 000 GET으로 얻은 p의 주소와 000과 and 하여서 사용
#define GET_ALLOC(p) (GET(p) & 0x1) // 0x1 의 2진법 : 1 과 p의 주소와 1과 and 로 사용 - 할당 비트 -- 주소의 마지막 자리와 비교해서 allocate와 free를 확인 가능

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE) // Header : block pointer - 4  (pointer는 항상 payload의 시작점을 가리키기 때문에 -4를 해줌으로써 힙의 첫부분으로 이동)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //  block pointer + Header pointer의 size - 8 (해당 계산식을 통해서 footer의 시작점을 포인터가 가리키게됨)

// Given block ptr bp, compute address of next and previous blocks - 다음 블록과 이전 블록의 주소를 계산
#define NEXT_BLKP(bp) (((char *)(bp) + GET_SIZE((char *)(bp)-WSIZE))) // 다음 블록 : 다음 block의 bp를 가리킴
#define PREV_BLKP(bp) (((char *)(bp) - GET_SIZE((char *)(bp)-DSIZE))) // 이전 블록 : 이전 block의 bp를 가리킴

//선언부 - 책에는 없지만 선언부가 없으면 동작에 문제가 생김 특히 heap_listp
static void *heap_listp;
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t a_size);
static void place(void *bp, size_t a_size);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    //Create the inital empty heap
    if ((heap_listp=mem_sbrk(4*WSIZE)) == (void *)-1) // heap_listq에 처음 메모리에서 4개의 워드를 요청하는데 그 값이 최대값이면 -1을 return 함
        return -1;
    PUT(heap_listp, 0); // heap_listp 포인터에 0을 할당
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1)); // heap_listp +1 words에 1000 | 1 -->1001 할당 -- prologue block header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1)); // heap_listp +2 words에 1000 | 1 -->1001 할당 -- prologue block footer
    PUT(heap_listp + (3*WSIZE), PACK(0,1)); // heap_listp +3 words에 0 | 1 --> 1 할당 -- epliogue block 이기 때문임
    heap_listp += (2*WSIZE); // heap_listp에 2 words를 추가 -- 포인터의 위치를 prologue block의 footer로 이동시킴

    //Extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) // 더 이상 늘릴 공간이 없으면
        return -1; // return -1
    return 0; // 모두 잘 동작했으면 return 0;
}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    //Allocate an even number of words to maintain alignment
    size = (words %2) ? (words +1)*WSIZE : words * WSIZE; // size가 홀수이면 TRUE 라서 +1하고 WSIZE를 곱하는거고 FLASE이면 짝수라서 바로 WSIZE를 곱함
    if ((long)(bp = mem_sbrk(size)) == -1) // mem_sbrk에서 -1을 return 받았다는 것은 size 초과라는 의미 그거에 따라서 NULL을 return
        return NULL;

    // Initialize free block header/footer and the epilogue header
    PUT(HDRP(bp), PACK(size,0)); // 위에서 return되지 않으면 size가 정상적으로 할당된 거임 따라서 할당된 size로 다시 Header와 Footer를 할당해줌
    PUT(FTRP(bp), PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); // 다음 블록의 header pointer에 1을 할당해줌 - 왜냐면 얘가 epliogue block이기 때문
    // epliogue block은 사이즈는 0이지만 할당되어 있다는 뜻으로 1을 할당함

    //Coalesce if the previous block was free
    return coalesce(bp); // 위 동작이 끝나고 이전 block이 가용 가능하면 병합 시작
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) // free를 하는 방법
{
    size_t size = GET_SIZE(HDRP(bp)); // size에 current block의 크기를 할당해줌

    PUT(HDRP(bp), PACK(size,0)); // header pointer에 사이즈는 할당하지만 할당을 해지함 free로오
    PUT(FTRP(bp), PACK(size,0)); // footer도 위 header와 동일
    coalesce(bp); // 이전 블록과 연결해서 계속 확인
}

static void *coalesce(void *bp) // 병합 함수
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 block의 footer에서 할당 여부를 prev_alloc에 저장하고
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 block의 header를 통해서 할당 여부를 next_alloc에 저장
    size_t size = GET_SIZE(HDRP(bp)); // 현재 header pointer의 크기를 size에 넣어줌

    if (prev_alloc && next_alloc){ //case 1 : current block 기준 이전과 다음 block 전부 할당
        return bp; // 둘다 할당 상태이면 현재 block만 가용으로 return 해주면 됨
    }
    else if(prev_alloc && !next_alloc){ // case 2 : current block 기준으로 이전 block은 할당 다음 block은 가용
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 현재 header pointer의 크기에 다음 블럭의 header pointer 사이즈를 더해줌
        PUT(HDRP(bp), PACK(size, 0)); // 이때 얻은 size로 Header와 footer를 재할당
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if(!prev_alloc && next_alloc){ // case 3 : current block 기준으로 이전 block은 가용 다음 block은 할당
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); // 현재 header pointer 의 크기에 이전 블럭의 header pointer 사이즈를 더해주고
        PUT(FTRP(bp), PACK(size, 0)); // footer를 새로 얻은 size로 초기화 하고
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블럭의 header pointer를 새로 얻은 size로 초기화
        bp = PREV_BLKP(bp); // block pointer에 이전 block pointer를 넣어서 이동 시켜줌 --> 이렇게 되면 이전 block pointer를 기준으로 두개 블록 사이즈가 연결됨 (prev block + current block)
    }
    else{ // case 4 : current block 기준 이전과 다음 모두 가용 
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 위아래 block size 전부를 더해줌 --> 즉 3개 block의 크기
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 block의 header pointer
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // 다음 block의 footer pointer를 새로 할당된 size로 초기화함
        bp = PREV_BLKP(bp); // pointer 이동해주고
    }
    return bp; // 마무리로 변경된 bp를 return 함
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) // malloc 함수
{
    size_t asize; // Adjusted block size
    size_t extendsize; // Abount to extend heap if no fit
    char *bp;

    // ignore spurious requests
    if (size == 0) // size가 0이라는건 할당을 요청한 크기가 없으니 NULL return
        return NULL;

    // Adjust block size to include overhead and alignment reqs.
    if (size<=DSIZE) // 크기가 Double words 보다 작으면
        asize = 2*DSIZE; // double words에 곱하기 2를 해서 alignment 를 진행하는 것
    else // 크기가 double words 보다 크면
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/DSIZE); // 오버헤드 바이트(size)를 추가하고 인접 8의 배수로 반올림 + alignment 포함
    
    // Search the free list for a fit
    if ((bp = find_fit(asize)) != NULL){ // 위에서 새로구한 aszie를 find_fit에 넣고 적합한 공간이 있는지를 찾음
        place(bp, asize); // 적절한 공간이 있으면 배치를 하고 분할하고
        return bp;
    }

    //No fit found. Get more memory and lace the block
    extendsize = MAX(asize, CHUNKSIZE); // 만일 위에서 남은 공간이 더이상 없다면 할당하려는 공간 vs heap의 남은 공간중 큰 값을 extendsize에 저장하고
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) // extendsize는 메모리 사이즈이기 때문에 이를 WSIZE로 나눠서 word의 배수로 만들어줌
        return NULL; // NULL 이 나온다는건 더이상 공간을 할당해 줄수 없다 or 니가 음수다 임
    
    place(bp, asize); // 배치하고 분할하고
    return bp; // 새롭게 할당한 블록의 포인터를 return

    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    
    // if (p == (void *)-1)
	//     return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) // 공간 재할당
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size); // newptr에 size의 크기로 만든 동적 메모리를 넣음
    if (newptr == NULL) // 근데 size가 0이라서 즉 재할당할 사이즈가 0이라면
      return NULL; // 그냥 return NULL
    // copySize = GET_SIZE(HDRP(oldptr));
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE); // oldptr- 8(header와 footer를 뺀 크기) 을 한거에 int형 씌운다
    if (size < copySize) // 만약 size가 copySize보다 작다면
      copySize = size; // copySize에 더 작은 값을 넣어줌 -- 작은거 위주로 넣어야함
    memcpy(newptr, oldptr, copySize); // newptr에 oldptr을 copySize 만큼 복사해 넣고
    mm_free(oldptr); // oldptr을 날리고
    return newptr; // newptr을 return 해줌
}

static void *find_fit(size_t asize){ // First-fit search로 적합한 공간을 찾는 방법
    //First-fit search
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){ // bp에는 heap_listp 의 주소를 넣고 (시작점) ; bp의 크기만큼 돌릴거고 ; offset은 bp에서 다음 블럭으로 가는 크기만큼
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){ // 만약 할당된 주소가 아니고 크기가 가용 크기보다 작을 경우
                return bp; // bp를 return 함
        }
    }
    return NULL; // No fit 맞는게 없으면 NULL을 return
}

static void place(void *bp, size_t asize){ // 배치 함수 asize는 alignment를 포함한 block --> 결국 malloc은 메모리 주소를 더블워드의 배수로 반환한다는 것
    size_t csize = GET_SIZE(HDRP(bp)); // 현재 블록의 size를 csize에 넣고

    if ((csize - asize) >= (2*DSIZE)){ // csize - asize 즉 남은 공간이 더블워드*2 보다 크다면
        PUT(HDRP(bp), PACK(asize, 1)); // 할당 됐다는 표시 1과 함께 값을 header와 footer에 할당해줌
        PUT(FTRP(bp), PACK(asize, 1));
        
        bp = NEXT_BLKP(bp); // 그리고 bp에는 다음 블록의 bp를 넣고--> 블록을 할당된 부분과 가용가능한 부분을 나눔
        PUT(HDRP(bp), PACK(csize - asize, 0)); // 그 bp의 Header에는 차이 만큼은 가용 가능하니 csize-asize 만큼 가용 블록을 넣어주고 가용 상태로 표기
        PUT(FTRP(bp), PACK(csize - asize, 0)); // footer에도 동일하게 해줌
    }
    else{ // csize - asize가 더블워드를 *2 한거보다 작다면
        PUT(HDRP(bp), PACK(csize, 1)); // 그냥 배치하면 됨
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

