// 명시적 할당기 + 명시적 가용 리스트 = Explicit 
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


#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12) 

// Max 함수 정의
#define MAX(x,y) ((x)>(y)? (x):(y))

// Pack a size and allocated bit into a word
#define PACK(size, alloc) ((size)|(alloc))

// Read and write a word at address p
#define GET(p) (*(unsigned int *)(p)) // *p의 주소를 읽고 씀
#define PUT(p, val) (*(unsigned int *)(p)=(val)) // *p에 val 값을 할당

// Read the size and allocated fields from address p
#define GET_SIZE(p) (GET(p) & ~0x7) // 7의 2진법 : 111 여기서 not : 000 GET으로 얻은 p의 주소와 000과 and 하여서 사용 --> 즉, 마지막 비트 3개만 비우고 크기만 정확히 읽어오겠다라는 의미
#define GET_ALLOC(p) (GET(p) & 0x1) // 0x1 의 2진법 : 1 과 p의 주소와 1과 and 로 사용 - 할당 비트 -- 주소의 마지막 자리와 비교해서 allocate와 free를 확인 가능

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE) // Header : block pointer - 4  (pointer는 항상 payload의 시작점을 가리키기 때문에 -4를 해줌으로써 힙의 첫부분으로 이동)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //  block pointer + Header pointer의 size - 8 (해당 계산식을 통해서 footer의 시작점을 포인터가 가리키게됨)

// Given block ptr bp, compute address of next and previous blocks - 다음 블록과 이전 블록의 주소를 계산
#define NEXT_BLKP(bp) (((char *)(bp) + GET_SIZE((char *)(bp)-WSIZE))) // 다음 블록 : 다음 block의 bp를 가리킴
#define PREV_BLKP(bp) (((char *)(bp) - GET_SIZE((char *)(bp)-DSIZE))) // 이전 블록 : 이전 block의 bp를 가리킴

//explicit free list
#define PRED_PTR(bp) ((char *)(bp))
#define SUCC_PTR(bp) ((char *)(bp) + WSIZE)

//선언부 - 책에는 없지만 선언부가 없으면 동작에 문제가 생김 특히 heap_listp
static void *heap_listp = NULL;
static char *root = NULL; // root는 항상 list의 제일 앞을 보고 있음

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t a_size);
static void place(void *bp, size_t a_size);


static void __insert(char* bp){ // address order --> 이걸 쓰면 장점은 주소가 인접해있다 = 즉 delete하거나 coalesce 일 경우 인전한 주소를 바로 쓸 수 있다는 장점이 있음
    char *succ; // 후계자 포인터를 선언해주고
    for (succ = GET(root); succ != NULL; succ = GET(SUCC_PTR(succ))){ // 후계자 포인터에 root의 주소를 넣고, 그 주소가 NULL이 아니면 돌리고, 후계자 포인터에 + WSIZE 한만큼 더해감
        if(bp < succ){ // bp 즉 현재 pointer가 succ보다 작으면 -- succ 이전 ptr이면
            PUT(PRED_PTR(bp), GET(PRED_PTR(succ))); // bp 포인터에 succ의 주소를 넣음
            PUT(SUCC_PTR(bp), succ); // bp의 succ 포인터에 for문의 조건문을 통해 얻은 succ 주소를 넣음

            if(GET(PRED_PTR(succ)) != NULL) // succ의 주소가 NULL이 아니면 
                PUT(SUCC_PTR(GET(PRED_PTR(succ))), bp); // 해당 주소 + wsize (succ 위치에) bp 값을 넣음
            else // succ의 주소가 NULL 이면 Root 라는 얘기 root 앞은 아무것도 없으니깐 NULL
                PUT(root, bp); // 따라서 root에 bp 값을 부여해줌
            
            PUT(PRED_PTR(succ), bp); // succ의 주소에 bp의 값을 넣음
            return;
        }
        else if(GET(SUCC_PTR(succ)) == NULL){ // succ에 해당하는 주소가 비어있고 bp가 succ 보다 클때
            PUT(PRED_PTR(bp), succ);
            PUT(SUCC_PTR(bp), 0); // SUCC의 값을 지워줌 -- 뒤에 할당된 block이 없다
            PUT(SUCC_PTR(succ), bp);
            return;
        }
    }
    // 위 조건들에 다 걸리지 않을 경우 - bp가 succ보다 크면서 succ에 해당하는 주소도 차있따.
    // bp가 가용 리스트의 하나뿐인 블록의 주소일 경우
    PUT(root, bp); // root에 bp 주소를 넣고
    PUT(PRED_PTR(bp), 0); // PRED, SUCC 값을 지움
    PUT(SUCC_PTR(bp), 0);
}

static void delete_node(char *bp){
    if(GET(PRED_PTR(bp)) == NULL && GET(SUCC_PTR(bp)) == NULL) // bp가 root 이면서 혼자 있을때
        PUT(root, GET(SUCC_PTR(bp)));
    else if (GET(PRED_PTR(bp)) != NULL && GET(SUCC_PTR(bp)) == NULL) // bp가 root 가 아니면서 list의 마지막일때
        PUT(SUCC_PTR(GET(PRED_PTR(bp))), GET(SUCC_PTR(bp)));
    else if (GET(PRED_PTR(bp)) == NULL && GET(SUCC_PTR(bp)) != NULL){ // bp가 root 면서 뒤에 다른 할당된 block이 있을때
        PUT(PRED_PTR(GET(SUCC_PTR(bp))), GET(PRED_PTR(bp)));
        PUT(root, GET(SUCC_PTR(bp)));
    }
    else{ // bp의 앞, 뒤에 할당된 block이 다 있을 때
        PUT(PRED_PTR(GET(SUCC_PTR(bp))), GET(PRED_PTR(bp))); // 현재 정보에 등록된 애들을 앞뒤로 이어서 붙여주는 작업
        PUT(SUCC_PTR(GET(PRED_PTR(bp))), GET(SUCC_PTR(bp)));
    }
    // bp는 이제 가용 블록이 아니므로 PRED, SUCC 정보를 지워줌
    PUT(PRED_PTR(bp), 0);
    PUT(SUCC_PTR(bp), 0);
}

static void *find_fit(size_t asize){  // 적합한 곳을 찾는다 first fit
    for (char *bp = GET(root); bp != NULL; bp = GET(SUCC_PTR(bp))){  // bp는 root부터 시작해서, bp 가 NULL이 아닐때 돌고, offset은 bp 에 WSIZE 만큼 이동한 주소
        if (asize <= GET_SIZE(HDRP(bp))){ // asize가 블록의 크기보다 작으면
            return bp; // 그냥 bp를 넣으면 되니깐 바로 return
        }
    }
    return NULL; // 장소가 없으면 NULL
}

static void *coalesce(void *bp) // 병합 함수
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 block의 footer에서 할당 여부를 prev_alloc에 저장하고
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 block의 header를 통해서 할당 여부를 next_alloc에 저장
    size_t size = GET_SIZE(HDRP(bp)); // 현재 header pointer의 크기를 size에 넣어줌

    if (prev_alloc && next_alloc){ //case 1 : current block 기준 이전과 다음 block 전부 할당
        __insert(bp); // 해당하는 bp를 insert하고  --> insert는 주소의 빈 공간을 찾아서 넣게됨 --주소에 따라 배열된 방법이니깐
        return bp; // 둘다 할당 상태이면 현재 block만 가용으로 return 해주면 됨
    }
    else if(prev_alloc && !next_alloc){ // case 2 : current block 기준으로 이전 block은 할당 다음 block은 가용
        delete_node(NEXT_BLKP(bp)); // 다음 블록을 비우고 -- 다음블록은 가용 상태이니깐  
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 현재 header pointer의 크기에 다음 블럭의 header pointer 사이즈를 더해줌 -- 둘을 합쳐서
        PUT(HDRP(bp), PACK(size, 0)); // 이때 얻은 size로 Header와 footer를 재할당
        PUT(FTRP(bp), PACK(size, 0));
        __insert(bp); // 다시 insert를 해줌
    }
    else if(!prev_alloc && next_alloc){ // case 3 : current block 기준으로 이전 block은 가용 다음 block은 할당
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); // 현재 header pointer 의 크기에 이전 블럭의 header pointer 사이즈를 더해주고
        PUT(FTRP(bp), PACK(size, 0)); // footer를 새로 얻은 size로 초기화 하고
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블럭의 header pointer를 새로 얻은 size로 초기화
        bp = PREV_BLKP(bp); // block pointer에 이전 block pointer를 넣어서 이동 시켜줌 --> 이렇게 되면 이전 block pointer를 기준으로 두개 블록 사이즈가 연결됨 (prev block + current block)
    }
    else{ // case 4 : current block 기준 이전과 다음 모두 가용 
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 위아래 block size 전부를 더해줌 --> 즉 3개 block의 크기
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 block의 header pointer
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // 다음 block의 footer pointer를 새로 할당된 size로 초기화함
        bp = PREV_BLKP(bp); // pointer 이동해주고
    }
    return bp; // 마무리로 변경된 bp를 return 함
}


static void place(void *bp, size_t asize){ 
    size_t csize = GET_SIZE(HDRP(bp)); 
    delete_node(bp); // current block에 해당하는 비우고

    if ((csize - asize) >= (2*DSIZE)){  // 더블워드 정렬보다 크다면
        PUT(HDRP(bp), PACK(asize, 1)); //할당해주고
        PUT(FTRP(bp), PACK(asize, 1));
       
        bp = NEXT_BLKP(bp); //다음 블록 포인트로 이동하고
        PUT(HDRP(bp), PACK(csize - asize, 0)); // 남은 칸은 할당하기 위해서 분할
        PUT(FTRP(bp), PACK(csize - asize, 0));
        PUT(PRED_PTR(bp), 0);
        PUT(SUCC_PTR(bp), 0);
        coalesce(bp); // 그리고 병합
    }
    else{ 
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    size = (words %2) ? (words +1)*WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) 
        return NULL;

    // Initialize free block header/footer and the epilogue header
    PUT(HDRP(bp), PACK(size, 0)); 
    PUT(FTRP(bp), PACK(size, 0));
    PUT(PRED_PTR(bp), 0);
    PUT(SUCC_PTR(bp), 0);
    
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    //Coalesce if the previous block was free
    return coalesce(bp); 
}

int mm_init(void)
{
    //Create the inital empty heap
    if ((heap_listp=mem_sbrk(4*WSIZE)) == (void *)-1) 
        return -1;
    
    PUT(heap_listp, 0); 
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); 
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); 
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); 
    root = heap_listp; // 초기 시작 값을 즉 top 값을 root에 저장
    heap_listp += (2*WSIZE); 

    //Extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) 
        return -1;

    return 0; 
}


void *mm_malloc(size_t size) // malloc 함수
{
    size_t asize; // Adjusted block size
    size_t extendsize; // Abount to extend heap if no fit
    char *bp;

    // ignore spurious requests
    if (size == 0) 
        return NULL;

    if (size<=DSIZE) 
        asize = 2*DSIZE; 
    else 
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/DSIZE); 
    

    if ((bp = find_fit(asize)) != NULL){ 
        place(bp, asize); 
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE); 
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL; 
    
    place(bp, asize); 
    return bp; 

}

void mm_free(void *bp) // free를 하는 방법
{
    size_t size = GET_SIZE(HDRP(bp)); // size에 current block의 크기를 할당해줌

    PUT(HDRP(bp), PACK(size,0)); // header pointer에 사이즈는 할당하지만 할당을 해지함 free로오
    PUT(FTRP(bp), PACK(size,0)); // footer도 위 header와 동일
    PUT(PRED_PTR(bp), 0);
    PUT(SUCC_PTR(bp), 0);
    coalesce(bp); // 이전 블록과 연결해서 계속 확인
}

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
      copySize = size; 
    memcpy(newptr, oldptr, copySize); 
    mm_free(oldptr); 
    return newptr; 
}

