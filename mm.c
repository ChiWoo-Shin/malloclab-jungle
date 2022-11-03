// /*
//  * mm-naive.c - The fastest, least memory-efficient malloc package.
//  * 
//  * In this naive approach, a block is allocated by simply incrementing
//  * the brk pointer.  A block is pure payload. There are no headers or
//  * footers.  Blocks are never coalesced or reused. Realloc is
//  * implemented directly using mm_malloc and mm_free.
//  *
//  * NOTE TO STUDENTS: Replace this header comment with your own header
//  * comment that gives a high level description of your solution.
//  */
// 명시적 할당기 + 묵시적 리스트 + first fit
/*
Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.007653   744
 1       yes   99%    5848  0.005321  1099
 2       yes   99%    6648  0.009183   724
 3       yes  100%    5380  0.008238   653
 4       yes   66%   14400  0.000127113744
 5       yes   92%    4800  0.005618   854
 6       yes   92%    4800  0.004967   966
 7       yes   55%   12000  0.074936   160
 8       yes   51%   24000  0.252870    95
 9       yes   27%   14401  0.043129   334
10       yes   34%   14401  0.001634  8814
Total          74%  112372  0.413674   272

Perf index = 44 (util) + 18 (thru) = 63/100
*/
// #include <stdio.h>
// #include <stdlib.h>
// #include <assert.h>
// #include <unistd.h>
// #include <string.h>

// #include "mm.h"
// #include "memlib.h"

// /*********************************************************
//  * NOTE TO STUDENTS: Before you do anything else, please
//  * provide your team information in the following struct.
//  ********************************************************/
// team_t team = {
//     /* Team name */
//     "No.2",
//     /* First member's full name */
//     "ChiWoo Shin",
//     /* First member's email address */
//     "shin8037@naver.com",
//     /* Second member's full name (leave blank if none) */
//     "",
//     /* Second member's email address (leave blank if none) */
//     ""
// };

// /* single word (4) or double word (8) alignment */
// #define ALIGNMENT 8

// /* rounds up to the nearest multiple of ALIGNMENT */
// #define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// #define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// //Basic constants and macros
// #define WSIZE 4
// #define DSIZE 8
// #define CHUNKSIZE (1<<12) // 우리는 extend해서 하나의 chunksize를 받아서 그 구간을 malloc으로 나눠서 씀
// // 그런데 chunksize를 너무 작게 두면 extend를 너무 자주 호출해야해서 CPU에 과부하가 걸리고
// // 너무 chunksize를 너무 크게 주면 내부 단편화가 발생
// // 따라서 그 중간 합의적을 찾은 값이 4KB 이다 (페이지 블록이 하나에 4KB~2MB인 것도 관련이 있을 수 있음)
// /*
// 4 KiB만큼의 heap을 요청하는 이유는 1 page가 4 KiB이기 때문이라고 생각하시면 될 것 같습니다. 
// 컴퓨터 입장에서 페이지는 메모리를 묶어서 다루는 가장 자연스러운 단위입니다. 
// 추후 PintOS를 구현하면서 OS가 어떻게 하드웨어와 발을 맞춰 페이지 단위로 메모리를 관리하는지 배우게 될 것입니다.

// 그렇다면 1 page는 왜 4 KiB일까요? 컴퓨터가 사용할 수 있는 메모리는 정해져 있습니다. 
// 따라서 페이지가 너무 작으면 페이지의 개수가 매우 많아서 이들을 관리하는 데 불필요하게 많은 자원을 써야 할 것이고, 
// 페이지가 너무 크면 페이지를 사용하지 않고 남기는 부분이 많아 메모리가 낭비될 것입니다. 
// 따라서 컴퓨터를 설계할 때 적절한 페이지 크기를 골라야 합니다. 
// 페이지를 사용한 최초의 CPU인 인텔 80386은 1 page = 4 KiB를 골랐고, 이 크기가 x86 아키텍처에 남아 지금까지 유지되어 왔다고 생각할 수 있겠습니다.
// https://stackoverflow.com/questions/11543748/why-is-the-page-size-of-linux-x86-4-kb-how-is-that-calculated
// */

// // Max 함수 정의
// #define MAX(x,y) ((x)>(y)? (x):(y))

// // Pack a size and allocated bit into a word
// #define PACK(size, alloc) ((size)|(alloc)) // size와 alloc을 bit로 변환후 or로 합침

// // Read and write a word at address p
// #define GET(p) (*(unsigned int *)(p)) // 포인터에 형변환을 시켜서 특정 타입을 할당해줌 - void는 정확한 값을 불러올수 없기 때문에 
// #define PUT(p, val) (*(unsigned int *)(p)=(val)) // *p에 val 값을 할당

// // Read the size and allocated fields from address p
// #define GET_SIZE(p) (GET(p) & ~0x7) // 7의 2진법 : 111 여기서 not : 000 GET으로 얻은 p의 주소와 000과 and 하여서 사용 --> 즉, 마지막 비트 3개만 비우고 크기만 정확히 읽어오겠다라는 의미
// #define GET_ALLOC(p) (GET(p) & 0x1) // 0x1 의 2진법 : 1 과 p의 주소와 1과 and 로 사용 - 할당 비트 -- 주소의 마지막 자리와 비교해서 allocate와 free를 확인 가능

// // Given block ptr bp, compute address of its header and footer
// #define HDRP(bp) ((char *)(bp) - WSIZE) // Header : block pointer - 4  (pointer는 항상 payload의 시작점을 가리키기 때문에 -4를 해줌으로써 힙의 첫부분으로 이동)
// #define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //  block pointer + Header pointer의 size - 8 (해당 계산식을 통해서 footer의 시작점을 포인터가 가리키게됨)

// // Given block ptr bp, compute address of next and previous blocks - 다음 블록과 이전 블록의 주소를 계산
// #define NEXT_BLKP(bp) (((char *)(bp) + GET_SIZE((char *)(bp)-WSIZE))) // 다음 블록 : 다음 block의 bp를 가리킴
// #define PREV_BLKP(bp) (((char *)(bp) - GET_SIZE((char *)(bp)-DSIZE))) // 이전 블록 : 이전 block의 bp를 가리킴

// //선언부 - 책에는 없지만 선언부가 없으면 동작에 문제가 생김 특히 heap_listp
// static void *heap_listp;
// static void *extend_heap(size_t words);
// static void *coalesce(void *bp);
// static void *find_fit(size_t a_size);
// static void place(void *bp, size_t a_size);

// /* 
//  * mm_init - initialize the malloc package.
//  */
// int mm_init(void)
// {
//     //Create the inital empty heap
//     if ((heap_listp=mem_sbrk(4*WSIZE)) == (void *)-1) // heap_listq에 처음 메모리에서 4개의 워드를 요청하는데 그 값이 최대값이면 -1을 return 함
//         return -1;
//     PUT(heap_listp, 0); // heap_listp 포인터에 0을 할당
//     PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1)); // heap_listp +1 words에 1000 | 1 -->1001 할당 -- prologue block header
//     PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1)); // heap_listp +2 words에 1000 | 1 -->1001 할당 -- prologue block footer
//     PUT(heap_listp + (3*WSIZE), PACK(0,1)); // heap_listp +3 words에 0 | 1 --> 1 할당 -- epliogue block 이기 때문임
//     heap_listp += (2*WSIZE); // heap_listp에 2 words를 추가 -- 포인터의 위치를 prologue block의 footer로 이동시킴

//     //Extend the empty heap with a free block of CHUNKSIZE bytes
//     if (extend_heap(CHUNKSIZE/WSIZE) == NULL) // 더 이상 늘릴 공간이 없으면
//         return -1; // return -1
//     return 0; // 모두 잘 동작했으면 return 0;
// }

// static void *extend_heap(size_t words){
//     char *bp;
//     size_t size;

//     //Allocate an even number of words to maintain alignment
//     size = (words %2) ? (words +1)*WSIZE : words * WSIZE; // size가 홀수이면 TRUE 라서 +1하고 WSIZE를 곱하는거고 FLASE이면 짝수라서 바로 WSIZE를 곱함
//     if ((long)(bp = mem_sbrk(size)) == -1) // mem_sbrk에서 -1을 return 받았다는 것은 size 초과라는 의미 그거에 따라서 NULL을 return
//         return NULL;

//     // Initialize free block header/footer and the epilogue header
//     PUT(HDRP(bp), PACK(size,0)); // 위에서 return되지 않으면 size가 정상적으로 할당된 거임 따라서 할당된 size로 다시 Header와 Footer를 할당해줌
//     PUT(FTRP(bp), PACK(size,0));
//     PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); // 다음 블록의 header pointer에 1을 할당해줌 - 왜냐면 얘가 epliogue block이기 때문
//     // epliogue block은 사이즈는 0이지만 할당되어 있다는 뜻으로 1을 할당함

//     //Coalesce if the previous block was free
//     return coalesce(bp); // 위 동작이 끝나고 이전 block이 가용 가능하면 병합 시작
// }


// /*
//  * mm_free - Freeing a block does nothing.
//  */
// void mm_free(void *bp) // free를 하는 방법
// {
//     size_t size = GET_SIZE(HDRP(bp)); // size에 current block의 크기를 할당해줌

//     PUT(HDRP(bp), PACK(size,0)); // header pointer에 사이즈는 할당하지만 할당을 해지함 free로오
//     PUT(FTRP(bp), PACK(size,0)); // footer도 위 header와 동일
//     coalesce(bp); // 이전 블록과 연결해서 계속 확인
// }

// static void *coalesce(void *bp) // 병합 함수
// {
//     size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 block의 footer에서 할당 여부를 prev_alloc에 저장하고
//     size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 block의 header를 통해서 할당 여부를 next_alloc에 저장
//     size_t size = GET_SIZE(HDRP(bp)); // 현재 header pointer의 크기를 size에 넣어줌

//     if (prev_alloc && next_alloc){ //case 1 : current block 기준 이전과 다음 block 전부 할당
//         return bp; // 둘다 할당 상태이면 현재 block만 가용으로 return 해주면 됨
//     }
//     else if(prev_alloc && !next_alloc){ // case 2 : current block 기준으로 이전 block은 할당 다음 block은 가용
//         size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 현재 header pointer의 크기에 다음 블럭의 header pointer 사이즈를 더해줌
//         PUT(HDRP(bp), PACK(size, 0)); // 이때 얻은 size로 Header와 footer를 재할당
//         PUT(FTRP(bp), PACK(size, 0));
//     }
//     else if(!prev_alloc && next_alloc){ // case 3 : current block 기준으로 이전 block은 가용 다음 block은 할당
//         size += GET_SIZE(HDRP(PREV_BLKP(bp))); // 현재 header pointer 의 크기에 이전 블럭의 header pointer 사이즈를 더해주고
//         PUT(FTRP(bp), PACK(size, 0)); // footer를 새로 얻은 size로 초기화 하고
//         PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블럭의 header pointer를 새로 얻은 size로 초기화
//         bp = PREV_BLKP(bp); // block pointer에 이전 block pointer를 넣어서 이동 시켜줌 --> 이렇게 되면 이전 block pointer를 기준으로 두개 블록 사이즈가 연결됨 (prev block + current block)
//     }
//     else{ // case 4 : current block 기준 이전과 다음 모두 가용 
//         size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 위아래 block size 전부를 더해줌 --> 즉 3개 block의 크기
//         PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 block의 header pointer
//         PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); // 다음 block의 footer pointer를 새로 할당된 size로 초기화함
//         bp = PREV_BLKP(bp); // pointer 이동해주고
//     }
//     return bp; // 마무리로 변경된 bp를 return 함
// }

// /* 
//  * mm_malloc - Allocate a block by incrementing the brk pointer.
//  *     Always allocate a block whose size is a multiple of the alignment.
//  */
// void *mm_malloc(size_t size) // malloc 함수
// {
//     size_t asize; // Adjusted block size
//     size_t extendsize; // Abount to extend heap if no fit
//     char *bp;

//     // ignore spurious requests
//     if (size == 0) // size가 0이라는건 할당을 요청한 크기가 없으니 NULL return
//         return NULL;

//     // Adjust block size to include overhead and alignment reqs.
//     if (size<=DSIZE) // 크기가 Double words 보다 작으면
//         asize = 2*DSIZE; // double words에 곱하기 2를 해서 alignment 를 진행하는 것
//     else // 크기가 double words 보다 크면
//         asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/DSIZE); // 오버헤드 바이트(size)를 추가하고 인접 8의 배수로 반올림 + alignment 포함
    
//     // Search the free list for a fit
//     if ((bp = find_fit(asize)) != NULL){ // 위에서 새로구한 aszie를 find_fit에 넣고 적합한 공간이 있는지를 찾음
//         place(bp, asize); // 적절한 공간이 있으면 배치를 하고 분할하고
//         return bp;
//     }
//   //No fit found. Get more memory and lace the block
//     extendsize = MAX(asize, CHUNKSIZE); // 만일 위에서 남은 공간이 더이상 없다면 할당하려는 공간 vs heap의 남은 공간중 큰 값을 extendsize에 저장하고
//     if ((bp = extend_heap(extendsize/WSIZE)) == NULL) // extendsize는 메모리 사이즈이기 때문에 이를 WSIZE로 나눠서 word의 배수로 만들어줌
//         return NULL; // NULL 이 나온다는건 더이상 공간을 할당해 줄수 없다 or 니가 음수다 임
    
//     place(bp, asize); // 배치하고 분할하고
//     return bp; // 새롭게 할당한 블록의 포인터를 return

// }

// /*
//  * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
//  */
// void *mm_realloc(void *ptr, size_t size) // 공간 재할당
// {
//     void *oldptr = ptr;
//     void *newptr;
//     size_t copySize;
    
//     newptr = mm_malloc(size); // newptr에 size의 크기로 만든 동적 메모리를 넣음
//     if (newptr == NULL) // 근데 size가 0이라서 즉 재할당할 사이즈가 0이라면
//       return NULL; // 그냥 return NULL
    
//     copySize = GET_SIZE(HDRP(oldptr)); // 이거도 사용 가능 아래와 이거 둘다 가능
//     // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE); // oldptr- 8(header와 footer를 뺀 크기) 을 한거에 int형 씌운다
//     if (size < copySize) // 만약 size가 copySize보다 작다면
//       copySize = size; // copySize에 더 작은 값을 넣어줌 -- 작은거 위주로 넣어야함
//     memcpy(newptr, oldptr, copySize); // newptr에 oldptr을 copySize 만큼 복사해 넣고
//     mm_free(oldptr); // oldptr을 날리고
//     return newptr; // newptr을 return 해줌
// }

// static void *find_fit(size_t asize){ // First-fit search로 적합한 공간을 찾는 방법
//     //First-fit search
//     void *bp;

//     for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){ // bp에는 heap_listp 의 주소를 넣고 (시작점) ; bp의 크기만큼 돌릴거고 ; offset은 bp에서 다음 블럭으로 가는 크기만큼
//         if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){ // 만약 할당된 주소가 아니고 크기가 가용 크기보다 작을 경우
//                 return bp; // bp를 return 함
//         }
//     }
//     return NULL; // No fit 맞는게 없으면 NULL을 return
// }

// static void place(void *bp, size_t asize){ // 배치 함수 asize는 alignment를 포함한 block --> 결국 malloc은 메모리 주소를 더블워드의 배수로 반환한다는 것
//     size_t csize = GET_SIZE(HDRP(bp)); // 현재 블록의 size를 csize에 넣고

//     if ((csize - asize) >= (2*DSIZE)){ // csize - asize 즉 남은 공간이 더블워드*2 보다 크다면
//         PUT(HDRP(bp), PACK(asize, 1)); // 할당 됐다는 표시 1과 함께 값을 header와 footer에 할당해줌
//         PUT(FTRP(bp), PACK(asize, 1));
       
//         bp = NEXT_BLKP(bp); // 그리고 bp에는 다음 블록의 bp를 넣고--> 블록을 할당된 부분과 가용가능한 부분을 나눔
//         PUT(HDRP(bp), PACK(csize - asize, 0)); // 그 bp의 Header에는 차이 만큼은 가용 가능하니 csize-asize 만큼 가용 블록을 넣어주고 가용 상태로 표기
//         PUT(FTRP(bp), PACK(csize - asize, 0)); // footer에도 동일하게 해줌
//     }
//     else{ // csize - asize가 더블워드를 *2 한거보다 작다면
//         PUT(HDRP(bp), PACK(csize, 1)); // 그냥 배치하면 됨
//         PUT(FTRP(bp), PACK(csize, 1));
//     }
// }

/***************************************************************************************************************************************************************/
// // 명시적 할당기 + 묵시적 가용리스트 + next fit 사용

/*
Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   89%    5694  0.001926  2956
 1       yes   91%    5848  0.001045  5595
 2       yes   95%    6648  0.003790  1754
 3       yes   97%    5380  0.005158  1043
 4       yes   66%   14400  0.000196 73357
 5       yes   92%    4800  0.004140  1159
 6       yes   90%    4800  0.003894  1233
 7       yes   55%   12000  0.009894  1213
 8       yes   51%   24000  0.009175  2616
 9       yes   76%   14401  0.000122117655
10       yes   46%   14401  0.000094153202
Total          77%  112372  0.039435  2850

Perf index = 46 (util) + 40 (thru) = 86/100
*/

// #include <stdio.h>
// #include <stdlib.h>
// #include <assert.h>
// #include <unistd.h>
// #include <string.h>

// #include "mm.h"
// #include "memlib.h"

// /*********************************************************
//  * NOTE TO STUDENTS: Before you do anything else, please
//  * provide your team information in the following struct.
//  ********************************************************/
// team_t team = {
//     /* Team name */
//     "No.2",
//     /* First member's full name */
//     "ChiWoo Shin",
//     /* First member's email address */
//     "shin8037@naver.com",
//     /* Second member's full name (leave blank if none) */
//     "",
//     /* Second member's email address (leave blank if none) */
//     ""
// };

// /* single word (4) or double word (8) alignment */
// #define ALIGNMENT 8

// /* rounds up to the nearest multiple of ALIGNMENT */
// #define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// #define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// //Basic constants and macros
// #define WSIZE 4
// #define DSIZE 8
// #define CHUNKSIZE (1<<12) 


// // Max 함수 정의
// #define MAX(x,y) ((x)>(y)? (x):(y))

// // Pack a size and allocated bit into a word
// #define PACK(size, alloc) ((size)|(alloc)) 

// // Read and write a word at address p
// #define GET(p) (*(unsigned int *)(p)) 
// #define PUT(p, val) (*(unsigned int *)(p)=(val))

// // Read the size and allocated fields from address p
// #define GET_SIZE(p) (GET(p) & ~0x7) 
// #define GET_ALLOC(p) (GET(p) & 0x1) 

// // Given block ptr bp, compute address of its header and footer
// #define HDRP(bp) ((char *)(bp) - WSIZE) 
// #define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// // Given block ptr bp, compute address of next and previous blocks - 다음 블록과 이전 블록의 주소를 계산
// #define NEXT_BLKP(bp) (((char *)(bp) + GET_SIZE((char *)(bp)-WSIZE)))
// #define PREV_BLKP(bp) (((char *)(bp) - GET_SIZE((char *)(bp)-DSIZE)))

// //declaration 
// static void *heap_listp;
// static char *next_fit_ptr; // 이전 검색이 종료된 지점을 기억하기 위한 포인터

// static void *extend_heap(size_t words);
// static void *coalesce(void *bp);
// static void *find_fit(size_t a_size);
// static void place(void *bp, size_t a_size);


// /* 
//  * mm_init - initialize the malloc package.
//  */
// int mm_init(void)
// {
//     //Create the inital empty heap
//     if ((heap_listp=mem_sbrk(4*WSIZE)) == (void *)-1)
//         return -1;

//     PUT(heap_listp, 0); 
//     PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1)); 
//     PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1)); 
//     PUT(heap_listp + (3*WSIZE), PACK(0,1)); 
//     heap_listp += (2*WSIZE); 
   
//     //Extend the empty heap with a free block of CHUNKSIZE bytes
//     if (extend_heap(CHUNKSIZE/WSIZE) == NULL) 
//         return -1;
    
//     next_fit_ptr = (char *)heap_listp; // 초기 next_fit_ptr 의 위치는 heap_listp 의 위치와 동일하게 시작 (첫 prologue block의 footer 위치)

//     return 0;
// }

// static void *extend_heap(size_t words){
//     char *bp;
//     size_t size;

//     //Allocate an even number of words to maintain alignment
//     size = (words %2) ? (words +1)*WSIZE : words * WSIZE; 
//     if ((long)(bp = mem_sbrk(size)) == -1)
//         return NULL;

//     // Initialize free block header/footer and the epilogue header
//     PUT(HDRP(bp), PACK(size,0)); 
//     PUT(FTRP(bp), PACK(size,0));
//     PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); 

//     //Coalesce if the previous block was free
//     return coalesce(bp); 
// }


// /*
//  * mm_free - Freeing a block does nothing.
//  */
// void mm_free(void *bp) 
// {
//     size_t size = GET_SIZE(HDRP(bp)); 

//     PUT(HDRP(bp), PACK(size,0)); 
//     PUT(FTRP(bp), PACK(size,0)); 
//     coalesce(bp); 
// }

// static void *coalesce(void *bp) // 병합 함수
// {
//     size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 block의 footer에서 할당 여부를 prev_alloc에 저장하고
//     size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 block의 header를 통해서 할당 여부를 next_alloc에 저장
//     size_t size = GET_SIZE(HDRP(bp)); // 현재 header pointer의 크기를 size에 넣어줌

//     if(prev_alloc && !next_alloc){ 
//         size += GET_SIZE(HDRP(NEXT_BLKP(bp))); 
//         PUT(HDRP(bp), PACK(size, 0)); 
//         PUT(FTRP(bp), PACK(size, 0));
//     }
//     else if(!prev_alloc && next_alloc){ 
//         size += GET_SIZE(HDRP(PREV_BLKP(bp))); 
//         PUT(FTRP(bp), PACK(size, 0)); 
//         PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
//         bp = PREV_BLKP(bp); 
//     }
//     else if(!prev_alloc && !next_alloc){ // case 4 : current block 기준 이전과 다음 모두 가용 
//         size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
//         PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
//         PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
//         bp = PREV_BLKP(bp);
//     }
//     next_fit_ptr = bp; // 연결된 payload의 bp의 위치리를 next_fit_ptr에 너놓음
//     // next_fit은 결국 malloc의 값이 저장된 제일 마지막 block을 가리키게 되어 있음
//     // 따라서 병합이 일어나게 되면 bp의 위치와 맞춰줘야함
//     // 만일 그렇지 않으면 next_fit이 가리키는 위치는 bp 를 지나서 이후의 위치를 가리키게 됨
//     return bp; // 마무리로 변경된 bp를 return 함
// }

// /* 
//  * mm_malloc - Allocate a block by incrementing the brk pointer.
//  *     Always allocate a block whose size is a multiple of the alignment.
//  */
// void *mm_malloc(size_t size) // malloc 함수
// {
//     size_t asize; // Adjusted block size
//     size_t extendsize; // Abount to extend heap if no fit
//     char *bp;

//     // ignore spurious requests
//     if (size == 0)
//         return NULL;

//     // Adjust block size to include overhead and alignment reqs.
//     if (size<=DSIZE) 
//         asize = 2*DSIZE; 
//     else 
//         asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/DSIZE); 
    
//     // Search the free list for a fit
//     if ((bp = find_fit(asize)) != NULL){ 
//         place(bp, asize); 
//         next_fit_ptr = bp; // next_fit_ptr은 search하고 place 한 이후의 bp 위치로 옮겨줌 
//         //-- 안그러면 next_fit_ptr만 두고 bp가 떠난다... 그럼 다음 search할때 next_fit_ptr부터 시작해야하는데 문제가 생기겠지?
//         return bp;
//     }
//   //No fit found. Get more memory and lace the block
//     extendsize = MAX(asize, CHUNKSIZE); 
//     if ((bp = extend_heap(extendsize/WSIZE)) == NULL) 
//         return NULL; 
    
//     place(bp, asize); 
//     next_fit_ptr = bp;
//     return bp; 

// }

// /*
//  * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
//  */
// void *mm_realloc(void *bp, size_t size) // 공간 재할당
// {
//     size_t old_size = GET_SIZE(HDRP(bp)); // 현재 bp의 블럭 사이즈를 old_size에 너놓고
//     size_t new_size = size + (2 * WSIZE); // new_size 는 할당 요청 받은 size + 2 * word size(alignment 포함) 로 할당해 줌

//     if (new_size <= old_size) // 새로 할당할 size가 이전 할당 size보다 작으면
//         return bp; // 그냥 그대로 return
//     else{
//         size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 할당 크기 = 다음 블록의 헤드 포인트의 주소를 넣음
//         size_t current_size = old_size + GET_SIZE(HDRP((NEXT_BLKP(bp)))); // 현재 크기는 = 현재 블럭 size + 다음 블럭 사이즈

//         if(!next_alloc && current_size >=new_size){ // 만약 다음 블록이 할당되어 있지 않고 현재 사이즈가 new size보다 크다면
//             PUT(HDRP(bp), PACK(current_size, 1)); // 할당해주고
//             PUT(FTRP(bp), PACK(current_size, 1));
//             return bp; // return 하면 됨
//         }
//         else{
//             void *new_bp = mm_malloc(new_size); // 새로운 포인터에 new_size를 할당하고
//             place(new_bp, new_size); // 새로운 bp에 새로운 size를 넣어주고
//             memcpy(new_bp, bp, new_size); // 새로운 bp의 이전 bp들을 new_size 크기만큼 복사해넣음
//             mm_free(bp); // 그리고 이전 bp free로 날려줌
//             return new_bp;
//         }
//     }
// }

// static void *find_fit(size_t asize){ // Next-fit search로 적합한 공간을 찾는 방법
//     char *bp = next_fit_ptr; // next_fit_ptr을 bp에 저장해서 이제 사용할거야
    
//     for (bp = NEXT_BLKP(bp); GET_SIZE(HDRP(bp)) != 0 ; bp = NEXT_BLKP(bp)){ // bp는 다음블럭 주소부터 시작함 ; 다음 블록의 크기가 0이 아니라면 동작 ; offset은 다음블럭 주소만큼 간다
//         if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){ // bp의 다음 블럭 주소가 0이 아니고 사이즈가 adjust size보다 크다면 - 즉 할당 가능한 위치이면
//             next_fit_ptr = bp; // 해당 주소를 next_fit_ptr에 넣어줌
//             return bp; 
//         }
//     }
//     //위에서 도는 조건에 걸리지 못했다면 = 남은 구간에는 적합한 공간이 없다

//     bp=heap_listp; // 그래서 초기 위치를 bp에 넣고
//     while( bp < next_fit_ptr ){ // 적합한 주소를 찾을때까지 돌립니다.
//         bp = NEXT_BLKP(bp);
//         if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){ // 적합한 위치를 찾으면
//             next_fit_ptr = bp; // 해당 주소를 넣어서 기억시키고
//             return bp; // return 합니다
//         }
//     }
//     return NULL; // 위 두 곳에서 다 걸리지 못했다는 것은 적합한 공간이 없다라는 의미이기때문에 NULL로 return 합니다.
// }

// static void place(void *bp, size_t asize){ 
//     size_t csize = GET_SIZE(HDRP(bp)); 

//     if ((csize - asize) >= (2*DSIZE)){ 
//         PUT(HDRP(bp), PACK(asize, 1)); 
//         PUT(FTRP(bp), PACK(asize, 1));
       
//         bp = NEXT_BLKP(bp); 
//         PUT(HDRP(bp), PACK(csize - asize, 0)); 
//         PUT(FTRP(bp), PACK(csize - asize, 0)); 
//     }
//     else{ 
//         PUT(HDRP(bp), PACK(csize, 1)); 
//         PUT(FTRP(bp), PACK(csize, 1));
//     }
// }




// /***************************************************************************************************************************************************************/
// // 명시적 할당기 + 명시적 가용 리스트 = Explicit
// // 이중 연결 리스트를 사용하여 block을 allocate 하고 free함
// /* implicit 가용 리스트를 사용할 때와의 차이는 
// coalesce, place, insert에서 차이가 있고
// delete 부분을 새로 추가해준다 (free와는 다른 동작을 함)

// insert - PRED와 SUCC 까지 넣어서 앞 뒤 블록을 연결해줘야함
// coalesce - 각각의 케이스에 맞춰서 진행
// place - 할당시에 가용 list에서 빼와서 끊고 또 이어붙임
// delete - 가용 list에서 빼오는 작업, 빼오면서 앞 뒤의 연결을 이어주고 해당 block은 제거됨

// */

/*
Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.000162 35148
 1       yes   99%    5848  0.000126 46450
 2       yes   99%    6648  0.000187 35608
 3       yes  100%    5380  0.000142 37887
 4       yes   66%   14400  0.000159 90395
 5       yes   92%    4800  0.002538  1891
 6       yes   92%    4800  0.002394  2005
 7       yes   55%   12000  0.034186   351
 8       yes   51%   24000  0.129684   185
 9       yes   27%   14401  0.044330   325
10       yes   34%   14401  0.001687  8536
Total          74%  112372  0.215595   521

Perf index = 44 (util) + 35 (thru) = 79/100
*/

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
    "solo_team",
    /* First member's full name */
    "Daeell",
    /* First member's email address */
    "daelkdev@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4             // word and header footer size (byte)
#define DSIZE 8             // double word size (byte)
#define QSIZE 16            // quad word size (byte)
#define CHUNKSIZE (1 << 12) // initial free block size and default size for expanding the heap (byte)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc)) // size와 alloc 여부를 1word로 return

/* address p위치에 words를 read와 write를 한다. */
#define GET(p) (*(unsigned int *)(p))              // return p값, (unsigned int *) 형변환으로 한 칸당 단위.
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // (unsigned int *) 형변환으로 한 칸당 단위.

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

// explicit 추가
#define PREV_FREE(bp) (*(void **)(bp))
#define NEXT_FREE(bp) (*(void **)(bp + WSIZE))

static void *heap_listp = NULL;
static void *free_listp = NULL; // explicit 추가 - free list 시작 부분

static void *coalesce(void *bp);           // free block 연결
static void *extend_heap(size_t words);    // heap size 추가
static void *find_fit(size_t asize);       // fit 방식
static void place(void *bp, size_t asize); // block 분할

/* explicit 추가 */
static void removeblock(void *bp);
static void putblock(void *bp);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    heap_listp = mem_sbrk(6 * WSIZE);
    if (heap_listp == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (WSIZE), PACK(QSIZE, 1));     // prologue header
    PUT(heap_listp + (2 * WSIZE), NULL);           // prev
    PUT(heap_listp + (3 * WSIZE), NULL);           // next
    PUT(heap_listp + (4 * WSIZE), PACK(QSIZE, 1)); // prologue footer
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));     // epilogue header

    free_listp = heap_listp + DSIZE;

    /*Extend the empty heap with a free blcok if CHUNKING bytes*/
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

static void *find_fit(size_t asize)
{
    void *bp;

    for (bp = free_listp; GET_ALLOC(HDRP(bp)) != 1; bp = NEXT_FREE(bp))
    {
        if (GET_SIZE(HDRP(bp)) >= asize)
        {
            return bp;
        }
    }
    return NULL;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* 크기 조정 */
    size_t extendsize; /* 맞는 size의 block이 없을 때 확장 */
    char *bp;

    /* 유효하지 않은 요청 */
    if (size <= 0)
        return NULL;
    /* 최소 사이즈보다 작은 block*/
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = ALIGN(size + SIZE_T_SIZE);

    /* first fit */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{
    if (size <= 0)
    {
        mm_free(bp);
        return 0;
    }
    if (bp == NULL)
    {
        return mm_malloc(size);
    }
    void *newp = mm_malloc(size);
    if (newp == NULL)
    {
        return 0;
    }
    size_t oldsize = GET_SIZE(HDRP(bp));
    if (size < oldsize)
    {
        oldsize = size;
    }
    // 메모리의 특정한 부분으로부터 얼마까지의 부분을 다른 메모리 영역으로
    // 복사해주는 함수(bp로부터 oldsize만큼의 문자를 newp로 복사해라)
    memcpy(newp, bp, oldsize);
    mm_free(bp);
    return newp;
}

/*
 * Util functions
 */

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // previous block status
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // next block status
    size_t size = GET_SIZE(HDRP(bp));                   // current block size

    /* 할당 해제 과정
        1. 할당을 해제할 block의 주소를 받는다.
        2. 할당을 해제할 block의 prev, next block을 확인한다.
        3. 기존의 free block이 있다면 해당 free block을 free list에서 제거하고 할당을 해제할 block과 결합하여 free list의 앞에 추가한다.
    */
    if (prev_alloc && next_alloc)
    {
        /* Case 1 : both of two allocated */
        putblock(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc)
    { /* Case 2 : prev block allocated only */
        removeblock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (!prev_alloc && next_alloc)
    { /* Case 3 : next block allocated only */
        removeblock(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else
    { /* Case 4 : both of two free */
        removeblock(PREV_BLKP(bp));
        removeblock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
                GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    putblock(bp);
    return bp;
}

/*
 * extend_heap - extens the heap with a new free block
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* 정렬 기준을 유지하기 위해 짝수 word로 정렬한다. */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));         // Free block header
    PUT(FTRP(bp), PACK(size, 0));         // Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // New epilogue header

    /* 이전에 free block이 있다면 연결한다. */
    return coalesce(bp);
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    removeblock(bp);
    if ((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        // split block
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        putblock(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* free list에 block을 배치한다. */
void putblock(void *bp)
{
    NEXT_FREE(bp) = free_listp;
    PREV_FREE(bp) = NULL;
    PREV_FREE(free_listp) = bp;
    free_listp = bp;
}

/* free list에 block을 제거한다. */
void removeblock(void *bp)
{
    if (bp == free_listp)
    {
        PREV_FREE(NEXT_FREE(bp)) = NULL;
        free_listp = NEXT_FREE(bp);
    }
    else
    {
        PREV_FREE(NEXT_FREE(bp)) = PREV_FREE(bp);
        NEXT_FREE(PREV_FREE(bp)) = NEXT_FREE(bp);
    }
}