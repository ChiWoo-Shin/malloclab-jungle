// segregated list - 98점
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

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
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7) // ~0x7 = 1000 : 2진법 && size+7


// My additional Macros
#define WSIZE     4          // word and header/footer size (bytes)
#define DSIZE     8          // double word size (bytes)
#define INITCHUNKSIZE (1<<6) // 2**6
#define CHUNKSIZE (1<<12)//+(1<<7) 

#define LISTLIMIT     20      
#define REALLOC_BUFFER  (1<<7)    // 2**7 

#define MAX(x, y) ((x) > (y) ? (x) : (y)) 
#define MIN(x, y) ((x) < (y) ? (x) : (y)) 

// Pack a size and allocated bit into a word
#define PACK(size, alloc) ((size) | (alloc))

// Read and write a word at address p 
#define GET(p)            (*(unsigned int *)(p))
#define PUT(p, val)       (*(unsigned int *)(p) = (val) | GET_TAG(p)) // val or 10(GET_TAG 의 2진법)한 값을 넣음
#define PUT_NOTAG(p, val) (*(unsigned int *)(p) = (val))

// Store predecessor or successor pointer for free blocks 
#define SET_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))

// Read the size and allocation bit from address p 
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_TAG(p)   (GET(p) & 0x2)
#define SET_RATAG(p)   (GET(p) |= 0x2) // RATAG = realloc tag
#define REMOVE_RATAG(p) (GET(p) &= ~0x2) // RATAG = realloc tag

// Address of block's header and footer 
#define HDRP(ptr) ((char *)(ptr) - WSIZE)
#define FTRP(ptr) ((char *)(ptr) + GET_SIZE(HDRP(ptr)) - DSIZE)

// Address of (physically) next and previous blocks 
#define NEXT_BLKP(ptr) ((char *)(ptr) + GET_SIZE((char *)(ptr) - WSIZE))
#define PREV_BLKP(ptr) ((char *)(ptr) - GET_SIZE((char *)(ptr) - DSIZE))

// Address of free block's predecessor and successor entries 
#define PRED_PTR(ptr) ((char *)(ptr))
#define SUCC_PTR(ptr) ((char *)(ptr) + WSIZE)

// Address of free block's predecessor and successor on the segregated list 
#define PRED(ptr) (*(char **)(ptr))
#define SUCC(ptr) (*(char **)(SUCC_PTR(ptr)))


// End of my additional macros


// Global var
void *segregated_free_lists[LISTLIMIT]; // 총 20개의 공간을 가진 segregated_free_lists를 확보함


// Functions
static void *extend_heap(size_t size);
static void *coalesce(void *ptr);
static void *place(void *ptr, size_t asize);
static void insert_node(void *ptr, size_t size);
static void delete_node(void *ptr);

//static void checkheap(int verbose);


///////////////////////////////// Block information /////////////////////////////////////////////////////////
/*
 
A   : Allocated? (1: true, 0:false)
RA  : Reallocation tag (1: true, 0:false)
 
 < Allocated Block >
 
 
             31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Header :   |                              size of the block                                       |  |  | A|
    bp ---> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                                                                                               |
            |                                                                                               |
            .                              Payload and padding                                              .
            .                                                                                               .
            .                                                                                               .
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Footer :   |                              size of the block                                       |     | A|
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 
 
 < Free block >
 
             31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Header :   |                              size of the block                                       |  |RA| A|
    bp ---> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                        pointer to its predecessor in Segregated list                          |
bp+WSIZE--> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                        pointer to its successor in Segregated list                            |
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            .                                                                                               .
            .                                                                                               .
            .                                                                                               .
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Footer :   |                              size of the block                                       |     | A|
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 
 
*/
///////////////////////////////// End of Block information /////////////////////////////////////////////////////////

//////////////////////////////////////// Helper functions //////////////////////////////////////////////////////////
static void *extend_heap(size_t size)
{
    void *ptr;                   
    size_t asize;                // Adjusted size 
    
    asize = ALIGN(size); // 8바이트 배열을 맞추기위하여 ALIGN
    
    if ((ptr = mem_sbrk(asize)) == (void *)-1) // 더이상 반환할 크기가 없으면
        return NULL; // NULL을 return 함
    
    // Set headers and footer 
    PUT_NOTAG(HDRP(ptr), PACK(asize, 0));  // TAG정보가 없이 -- 할당하는것이 아닌 값을 처음 set이라서
    PUT_NOTAG(FTRP(ptr), PACK(asize, 0));   
    PUT_NOTAG(HDRP(NEXT_BLKP(ptr)), PACK(0, 1)); // epliogue block
    insert_node(ptr, asize); // 

    return coalesce(ptr);
}

static void insert_node(void *ptr, size_t size) {
    int list = 0; // list는 0부터 탐색을 시작할 것이기 때문에 0을 넣어줌
    void *search_ptr = ptr;
    void *insert_ptr = NULL;
    
    // Select segregated list -- 들어온 size에 적합한 블록을 찾기위해
    while ((list < LISTLIMIT - 1) && (size > 1)) {
        size >>= 1; // 크기는 이진법으로 Right shift 1씩 --> size를 2진법으로 오른쪽 shift 하면서 list를 1씩 늘리는거라고 생각하면됨
        list++; // list는 1씩 커짐
    }
    
    // Keep size ascending order and search - 크기 오름차순을 유지하고 검색
    search_ptr = segregated_free_lists[list]; // 위에서 얻은 list 값을 가진 segregated_free_list를 search_ptr에 넣어줌
    while ((search_ptr != NULL) && (size > GET_SIZE(HDRP(search_ptr)))) { // search_ptr 이 NULL이 아니고 && input 된 size가 search_ptr_의 size보다 크다면
        insert_ptr = search_ptr; // 해당 포인터를 insert_ptr에 저장해놓고
        search_ptr = PRED(search_ptr); // search_ptr의 앞 블록 주소를 search_ptr로 옮김
    } // 이를 반복하면 search_ptr이 지속적으로 앞으로 이동하면서 NULL 인 공간을 찾거나 size가 더 커지는 공간을 찾게됨
    // 앞으로 이동한다는건 리스트 앞으로 간다는거 즉 메모리를 좀더 알차게 채워서 쓰려고한다
    
    // Set predecessor and successor 
    if (search_ptr != NULL) { // 위 while문을 거치게 되면 size가 search_ptr 보다 크거나
        if (insert_ptr != NULL) { // 이부분은 만약 처음부터 search_ptr이 저 while문의 조건에 안걸린다면 insert_ptr이 NULL인지 아닌지 확인을 해봐야함
            SET_PTR(PRED_PTR(ptr), search_ptr); // ptr에 search_ptr을 저장함
            SET_PTR(SUCC_PTR(search_ptr), ptr); // search_ptr의 succ 위치에 ptr을 넣음
            SET_PTR(SUCC_PTR(ptr), insert_ptr); // ptr의 succ 위치에 insert_ptr 주소를 넣고
            SET_PTR(PRED_PTR(insert_ptr), ptr); // insert_ptr의 Pred 위치에 ptr 주소를 넣음
        } else { // insert_ptr == NULL
            SET_PTR(PRED_PTR(ptr), search_ptr); // ptr의 PRED 위치에 search_ptr 넣고
            SET_PTR(SUCC_PTR(search_ptr), ptr); // search_ptr 의 succ 위치에 ptr 넣고
            SET_PTR(SUCC_PTR(ptr), NULL); // ptr의 succ 위치에 NULL 을 넣는다 == 뒷블럭이 없다
            segregated_free_lists[list] = ptr;
        }
    } else { // search_ptr == NULL 일때 위와 동일한 작업을 수행
        if (insert_ptr != NULL) { 
            SET_PTR(PRED_PTR(ptr), NULL);
            SET_PTR(SUCC_PTR(ptr), insert_ptr);
            SET_PTR(PRED_PTR(insert_ptr), ptr);
        } else {
            SET_PTR(PRED_PTR(ptr), NULL);
            SET_PTR(SUCC_PTR(ptr), NULL);
            segregated_free_lists[list] = ptr;
        }
    }
    
    return;
}


static void delete_node(void *ptr) {
    int list = 0;
    size_t size = GET_SIZE(HDRP(ptr));
    
    // Select segregated list 
    while ((list < LISTLIMIT - 1) && (size > 1)) {
        size >>= 1;
        list++;
    }
    
    if (PRED(ptr) != NULL) {
        if (SUCC(ptr) != NULL) {
            SET_PTR(SUCC_PTR(PRED(ptr)), SUCC(ptr));
            SET_PTR(PRED_PTR(SUCC(ptr)), PRED(ptr));
        } else {
            SET_PTR(SUCC_PTR(PRED(ptr)), NULL);
            segregated_free_lists[list] = PRED(ptr);
        }
    } else {
        if (SUCC(ptr) != NULL) {
            SET_PTR(PRED_PTR(SUCC(ptr)), NULL);
        } else {
            segregated_free_lists[list] = NULL;
        }
    }
    
    return;
}


static void *coalesce(void *ptr)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(ptr))); // 이전 block의 header 부분을 가리킴
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr))); // 다음 block의 header 를 가리킴
    size_t size = GET_SIZE(HDRP(ptr)); // 해당 block의 크기를 저장
    

    // Do not coalesce with previous block if the previous block is tagged with Reallocation tag
    if (GET_TAG(HDRP(PREV_BLKP(ptr)))) // 이전 block의 헤더에서 얻은 GET_tag 가 TRUE이면
        prev_alloc = 1; // 걔 사이즈를 1로 저장

    if (prev_alloc && next_alloc) {                         // Case 1 : 앞 뒤가 다 할당 되어있는 경우
        return ptr; // ptr을 반환 --> 할당에서 가용 상태로 
    }
    else if (prev_alloc && !next_alloc) {                   // Case 2 : 앞에는 할당 뒤에는 가용
        delete_node(ptr); // 가용 상태로 만들어주기 위해서 current block을 list에서 제거
        delete_node(NEXT_BLKP(ptr)); // next block도 list에서 제거
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr))); // 뒤에가 가용 상태이니깐 해당 블럭의 사이즈를 합쳐줌
        PUT(HDRP(ptr), PACK(size, 0)); // 그리고 크기만큼 배정해주고 할당 상태는 가용으로 체크
        PUT(FTRP(ptr), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {                 // Case 3 : 앞에는 가용 뒤에는 할당
        delete_node(ptr); // current list 제거해주고
        delete_node(PREV_BLKP(ptr)); // 이전 block도 제거해주고
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))); // 이전 블록의 사이즈를 더해주고
        PUT(FTRP(ptr), PACK(size, 0)); // current ptr에 업데이트된 사이즈를 넣어줌
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0)); // 이전 블록의 header에 업데이트 된 사이즈 넣어줌
        ptr = PREV_BLKP(ptr); // 포인터 이전 블록으로 이동
    } else {                                                // Case 4 : 둘다 가용
        delete_node(ptr); // 3개를 다 리스트에서 제거해주고
        delete_node(PREV_BLKP(ptr));
        delete_node(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(HDRP(NEXT_BLKP(ptr))); // 현재 + 앞 + 뒤 사이즈를 다 더해주고
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr); // 이전 블록의 header로 포인터 이동
    }
    
    insert_node(ptr, size); // 병합이 끝난 이후에 가용 리스트를 다시 넣어줌
    
    return ptr;
}

static void *place(void *ptr, size_t asize) // 배치
{
    size_t ptr_size = GET_SIZE(HDRP(ptr)); // current block 사이즈
    size_t remainder = ptr_size - asize; // current block - adjusted block size 
    
    delete_node(ptr); // current block을 list에서 제거해줌 - 가용여부와 할당 여부의 변경
    
    
    if (remainder <= DSIZE * 2) { //사이즈가 double word *2 (alignment 포함)보다 작으면
        // Do not split block - 나눌 필요가 없이
        PUT(HDRP(ptr), PACK(ptr_size, 1));  // 바로 배치
        PUT(FTRP(ptr), PACK(ptr_size, 1)); 
    }
    
    else if (asize >= 100) { // adjusted size가 100보다 크다면.. 근데 왜 100일까..? 100보다 크다는 것은 이미 할당된 공간이 충분히 크다??
        // Split block
        PUT(HDRP(ptr), PACK(remainder, 0)); // 그렇다면 remainder 크기를 header와 footer에 넣어줌 - TAG를 포함한
        PUT(FTRP(ptr), PACK(remainder, 0));
        PUT_NOTAG(HDRP(NEXT_BLKP(ptr)), PACK(asize, 1)); // 다음 block의 header에 할당된 사이즈를 넣고 할당되었다고 표시해줌
        PUT_NOTAG(FTRP(NEXT_BLKP(ptr)), PACK(asize, 1)); // 다음 block의 footer에도 동일하게 해줌
        insert_node(ptr, remainder); // remainder size에 맞는 공간을 찾아서 ptr을 넣어준다
        return NEXT_BLKP(ptr); // 그리고 ptr의 다음 블록을 return
        
    }
    
    else { // 그 외의 모든 경우에는
        // Split block
        PUT(HDRP(ptr), PACK(asize, 1)); // TAG를 포함한 정보
        PUT(FTRP(ptr), PACK(asize, 1)); 
        PUT_NOTAG(HDRP(NEXT_BLKP(ptr)), PACK(remainder, 0)); // Tag 정보가 없음 , ptr의 다음 블럭의 header에 remainder 값을 넣어줌 
        PUT_NOTAG(FTRP(NEXT_BLKP(ptr)), PACK(remainder, 0)); // Tag 정보가 없음 , ptr의 다음 블럭의 footer에 remainder 값을 넣어줌 
        insert_node(NEXT_BLKP(ptr), remainder); // 그리고 insert
    }
    return ptr;
}



//////////////////////////////////////// End of Helper functions ////////////////////////////////////////






/*
 * mm_init - initialize the malloc package.
 * Before calling mm_malloc, mm_realloc, or mm_free, 
 * the application program calls mm_init to perform any necessary initializations,
 * such as allocating the initial heap area.
 *
 * Return value : -1 if there was a problem, 0 otherwise.
 */
int mm_init(void)
{
    int list;         
    char *heap_start; // Pointer to beginning of heap
    
    // Initialize segregated free lists
    for (list = 0; list < LISTLIMIT; list++) { // 초기에는 list의 길이가 0 ~ listlimit-1 까지의 길이를 갖는 list의 개수가 생김
        segregated_free_lists[list] = NULL; // 내부는 NULL로 초기화 되어 있음
    }
    
    // Allocate memory for the initial empty heap 
    if ((long)(heap_start = mem_sbrk(4 * WSIZE)) == -1) // 만약 heap start가 꽉 차있으면 -1로 return
        return -1;
    
    PUT_NOTAG(heap_start, 0);                            /* Alignment padding */
    PUT_NOTAG(heap_start + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT_NOTAG(heap_start + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT_NOTAG(heap_start + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
    
    if (extend_heap(INITCHUNKSIZE) == NULL) // extend_heap에 initchunk 를 넣으면
        return -1;
    
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 *
 * Role : 
 * 1. The mm_malloc routine returns a pointer to an allocated block payload.
 * 2. The entire allocated block should lie within the heap region.
 * 3. The entire allocated block should overlap with any other chunk.
 * 
 * Return value : Always return the payload pointers that are alligned to 8 bytes.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    void *ptr = NULL;  /* Pointer */
    
    // Ignore size 0 cases
    if (size == 0)
        return NULL;
    
    // Align block size
    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = ALIGN(size+DSIZE);
    }
    
    int list = 0; 
    size_t searchsize = asize;
    // Search for free block in segregated list
    while (list < LISTLIMIT) {
        if ((list == LISTLIMIT - 1) || ((searchsize <= 1) && (segregated_free_lists[list] != NULL))) {
            ptr = segregated_free_lists[list];
            // Ignore blocks that are too small or marked with the reallocation bit
            while ((ptr != NULL) && ((asize > GET_SIZE(HDRP(ptr))) || (GET_TAG(HDRP(ptr)))))
            {
                ptr = PRED(ptr);
            }
            if (ptr != NULL) // segregated_free_lists 가 비어있다면
                break;
        }
        
        searchsize >>= 1; // right shift 1씩
        list++; // searchsize를 옮기면서 list를 1개씩 올린다는건 list와 matching이 되는 size를 찾을 수 있다는 것
    }
    
    // if free block is not found, extend the heap
    if (ptr == NULL) { // 위의 return에 걸리지 않았거나 ptr이 채워지지 않았다면 NULL이다 = 즉 heap이 부족하다
        extendsize = MAX(asize, CHUNKSIZE); // 둘 중 최대 값을 extendsize에 넣고
        
        if ((ptr = extend_heap(extendsize)) == NULL) // 그걸로 extend heap을 요청해서 값을 공간을 더 할당받을건데 그게 만약 NULL 이면 더이상 받지 못하면
            return NULL; // NULL을 return 한다
    }
    
    // Place and divide block
    ptr = place(ptr, asize);
    
    
    // Return pointer to newly allocated block 
    return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 *
 * Role : The mm_free routine frees the block pointed to by ptr
 *
 * Return value : returns nothing
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
 
    REMOVE_RATAG(HDRP(NEXT_BLKP(ptr))); // realloc tag도 지우고
    PUT(HDRP(ptr), PACK(size, 0)); // header의 tag도 지우고
    PUT(FTRP(ptr), PACK(size, 0)); // footer tag도 지움
    
    insert_node(ptr, size); // 다 지운걸 insert로 너놓고
    coalesce(ptr); // 가용 가능한 것들을 병합시킴
    
    return;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 *
 * Role : The mm_realloc routine returns a pointer to an allocated 
 *        region of at least size bytes with constraints.
 *
 *  I used https://github.com/htian/malloc-lab/blob/master/mm.c source idea to maximize utilization
 *  by using reallocation tags
 *  in reallocation cases (realloc-bal.rep, realloc2-bal.rep)
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *new_ptr = ptr;    /* Pointer to be returned */
    size_t new_size = size; /* Size of new block */
    int remainder;          /* Adequacy of block sizes */
    int extendsize;         /* Size of heap extension */
    int block_buffer;       /* Size of block buffer */
    
    // Ignore size 0 cases
    if (size == 0)
        return NULL;
    
    // Align block size
    if (new_size <= DSIZE) {
        new_size = 2 * DSIZE;
    } else {
        new_size = ALIGN(size+DSIZE);
    }
    
    /* Add overhead requirements to block size */
    new_size += REALLOC_BUFFER; // new size = new_size + 2**7으로 버퍼 크기만큼을 더 늘려줌 
    
    /* Calculate block buffer */
    block_buffer = GET_SIZE(HDRP(ptr)) - new_size; // 블록 버퍼 = 포인터 블록 크기 - 위에서 얻은 사이즈를 뺌
    
    /* Allocate more space if overhead falls below the minimum */
    if (block_buffer < 0) { // 값이 음수라면 크기가 더 필요하다 라는 말임
        /* Check if next block is a free block or the epilogue block */
        if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) || !GET_SIZE(HDRP(NEXT_BLKP(ptr)))) { //다음블록의 주소 또는 크기가 0이 아니라면
            remainder = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - new_size; // 현재 블록의 크기 + 다음 블록의 크기 - new_size 의 크기, 즉 더 필요한 크기
            if (remainder < 0) { // 이게 음수면
                extendsize = MAX(-remainder, CHUNKSIZE); // 커널에 추가 heap을 요청합시다
                if (extend_heap(extendsize) == NULL)
                    return NULL;
                remainder += extendsize;
            }
            
            delete_node(NEXT_BLKP(ptr)); // 다음 블록을 리스트에서 제거해주고
            
            // Do not split block - 나누지 않고
            PUT_NOTAG(HDRP(ptr), PACK(new_size + remainder, 1)); // header와 footer 에 현재블록크기 + 다음 블록의 크기를 할당
            PUT_NOTAG(FTRP(ptr), PACK(new_size + remainder, 1)); 
        } else { // 둘다 0이면
            new_ptr = mm_malloc(new_size - DSIZE); // new_ptr에 말록을 통해 할당한 크기만큼(새로운 사이즈에서 8바이트만큼 뺀걸 -- header와 footer 를 제거한 크기) 넣어줌
            memcpy(new_ptr, ptr, MIN(size, new_size)); // 새로운 ptr에 이전 정보를 복사해 넣어주자
            mm_free(ptr); // 이전 ptr 공간 날리고
        }
        block_buffer = GET_SIZE(HDRP(new_ptr)) - new_size; // block buffer의 크기를 재설정해줌
    }
    
    // Tag the next block if block overhead drops below twice the overhead 
    if (block_buffer < 2 * REALLOC_BUFFER) // 블록 버퍼가 2**7 *2 (overhead의 두배) 보다 작다면
        SET_RATAG(HDRP(NEXT_BLKP(new_ptr))); // 새로운 포인터의 다음블록의 header 에 realloc tag를 붙어줌 -- 왜냐면 realloc으로 사이즈를 재조정해야하니깐
    
    // Return the reallocated block 
    return new_ptr; // new_ptr을 return 함
}
