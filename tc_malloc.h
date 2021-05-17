
#define PAGEHEAPSIZE 256

#define PAGESIZE 4096

#define MAXBLOCK 1024 * 1024 * 512

#define MAXPAGE 131072

#define MAXSMALLOBJ 32768

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))


typedef struct list{

    void *list;

    size_t size;

    size_t threshold;

}FREELIST;


typedef struct span{

    size_t page_id;

    size_t page_size;

    size_t size_of_objects;

    size_t num_of_objects_taken;

    // add a pointer to lists 
    void* obj_ptr;

    struct span* next;

    struct span* prev;
}SPAN;


typedef struct central_array{
    
    size_t page_id;

    struct span* span_of_page_id;

}span_to_id_map;


void *tc_central_init();
void *tc_thread_init();
void *tc_malloc(size_t size);
void tc_free(void *ptr);

