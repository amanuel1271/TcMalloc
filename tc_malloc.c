#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "tc_malloc.h"

SPAN* central_page_heap[PAGEHEAPSIZE] = {0}; // make the central page heap global so that every function can access it
pthread_spinlock_t central_lock; //global variables
pthread_spinlock_t heap_lock; //global variables
pthread_spinlock_t central_array_lock;


__thread FREELIST thread_cache[159] = {0}; // 159 ALLOCATABLE SIZE CLASSES


void* central_array[100] = {0}; //everytime you request 256 pages of memory initialize a span to id map array and save the pointer here
SPAN* central_free_list[159] = {0}; // there are 159 allocatable size classes



void *tc_central_init();
void *tc_thread_init();
void *tc_malloc(size_t size);
void tc_free(void *ptr);



void *tc_central_init(){
    for (int j = 0;j < PAGEHEAPSIZE;j++)
        central_page_heap[j] = NULL;
    pthread_spin_init(&central_lock,1);
    pthread_spin_init(&heap_lock,1);
    pthread_spin_init(&central_array_lock,1);
    return (void *)central_page_heap;
}




SPAN* span_pop(SPAN** begin){
    
    assert(begin);
    if (*begin == NULL){
        fprintf(stderr,"usage: pop a non NULL pointer");
        return NULL;
    }
    SPAN* next = (*begin)->next;
    SPAN* cur = *begin;
    if (next){
        next->prev = cur->prev;
        *begin = next;
        cur->next = NULL;
        cur->prev = NULL;
        return cur;
    }
    *begin = NULL;
    cur->next = NULL;
    cur->prev = NULL;
    return cur;
}







void span_push(SPAN** span_ptr_index,SPAN* new_span){
    if (new_span == NULL){
        fprintf(stderr,"usage: push a non NULL pointer");
        return;
    }
    if (*span_ptr_index == NULL){
        *span_ptr_index = new_span;
        new_span->next = NULL;
        new_span->prev = NULL;
        return;
    }
    SPAN* next = *span_ptr_index;
    SPAN* prev = next->prev;
    *span_ptr_index = new_span;    
    new_span->next = next;
    next->prev = new_span;
    new_span->prev = prev;
    return;
}




void map_id_span(size_t num_of_pages,SPAN* span_ptr,size_t starting_id){
    span_to_id_map* find;
    for(int j = 0; j < 100;j++){
       if (central_array[j] == NULL)
            break;       
        find = (span_to_id_map*)central_array[j];
        if ((find->page_id <= starting_id) && (starting_id <= (find->page_id+ MAXPAGE - 1))){
            find = find + (starting_id - find->page_id);
            for(int k = 0;k < num_of_pages;k++)
                (find + k)->span_of_page_id = span_ptr;
            return;
        }
    }
}





SPAN* lookup(size_t page_id){   
    span_to_id_map* find;
    for(int j = 0; j < 100;j++){
        if (central_array[j] == NULL){
            break;
        }
        find = (span_to_id_map*)central_array[j];
        if ( (find->page_id <= page_id) && (page_id <= (find->page_id+ MAXPAGE - 1)) )
            return (find + (page_id - find->page_id))->span_of_page_id;
    }
    return NULL;
}



size_t calculate_pageid(void *ptr){

    uintptr_t number = (uintptr_t)ptr;
    return (size_t)number/PAGESIZE;
}



SPAN* give_span_to_central_cache_or_fetch_from_system(size_t num_of_pages){
    SPAN* old_span;
    void *ptr;
    if (central_page_heap[num_of_pages-1] != NULL){      
        SPAN* x = span_pop(&(central_page_heap[num_of_pages-1]));
        return x;
    }
    for (size_t i = num_of_pages; i < PAGEHEAPSIZE; i++){
        if (central_page_heap[i] != NULL){
            old_span = span_pop(&(central_page_heap[i]));
            ptr = sbrk(sizeof(SPAN));
            if (ptr == (void*)-1){
                fprintf(stderr,"The system couldn't allocate memory\n");
                return NULL;
            }
            SPAN* new_span = (SPAN *)ptr;
            new_span->page_id = old_span->page_id - num_of_pages + old_span->page_size;
            new_span->page_size = num_of_pages;
            old_span->page_size = old_span->page_size - num_of_pages;
            char* ptr20 = (char *)old_span->obj_ptr;
            new_span->obj_ptr = (void *)(ptr20 + (old_span->page_size * PAGESIZE)); // This is a risky practice my friend
            new_span->next = NULL;
            new_span->prev = NULL;
            new_span->size_of_objects = 0;
            span_push(&(central_page_heap[old_span->page_size-1]),old_span);
            map_id_span(new_span->page_size,new_span,new_span->page_id);
            return new_span;
        }
    }
        void *ptr2 = sbrk(MAXBLOCK);
        if (ptr2 == (void*)-1){
            fprintf(stderr,"The system couldn't allocate memory\n");
            fflush(stdout);
            return NULL;

        }
        void * pp = sbrk(sizeof(SPAN));
        SPAN* another_span = (SPAN*)pp;
        another_span->page_id = calculate_pageid(ptr2);
        another_span->page_size = MAXPAGE;
        void *ptr4;       
        for(int j = 0; j < 100;j++){
             if (central_array[j] == NULL){
                 ptr4 = sbrk(MAXPAGE * sizeof(span_to_id_map));
                 span_to_id_map* find = (span_to_id_map*)ptr4;
                 central_array[j] = ptr4;
                 for (int k = 0; k < MAXPAGE;k++)
                     (find + k)->page_id = another_span->page_id + k;
                 break;
             }
        }
         SPAN* large_span;
         void* ptr3;
         char* an_ptr = (char *)ptr2;
         for (int k = 0;  k < 512;  k++){                
                ptr3 = sbrk(sizeof(SPAN));
                if (ptr3 == (void*)-1){
                    fprintf(stderr,"The system couldn't allocate memory\n");
                    fflush(stdout);
                    return NULL;
                }
                large_span = (SPAN *)ptr3;
                large_span->page_id = another_span->page_id + (k * 256);
                large_span->page_size = PAGEHEAPSIZE;
                large_span->size_of_objects = 0;
                large_span->obj_ptr = (void *)(an_ptr + (k * 256 * PAGESIZE));
                span_push(&(central_page_heap[PAGEHEAPSIZE -1]),large_span);
                map_id_span(large_span->page_size,large_span,large_span->page_id);
        }
        return give_span_to_central_cache_or_fetch_from_system(num_of_pages);

}

void Free_Span_to_Central_Page_heap(SPAN ** span){
    // look up the prev page's span   
    pthread_spin_lock(&heap_lock);
    size_t comb_page_size;
    pthread_spin_lock(&central_array_lock);
    SPAN* prev_span = lookup( ((*span)->page_id) - 1);
    pthread_spin_unlock(&central_array_lock);
    if (prev_span == *span){
        pthread_spin_unlock(&heap_lock);
        return;
    }
    SPAN** find;
    SPAN *search_for_span,*ptr;
    if (prev_span != NULL && prev_span->size_of_objects == 0 && prev_span->obj_ptr){    
        comb_page_size = prev_span->page_size + (*span)->page_size;
        if ( !prev_span->num_of_objects_taken && (comb_page_size <= PAGEHEAPSIZE) ){
            find = central_page_heap + prev_span->page_size -1;
            search_for_span = *find;
            ptr = search_for_span;
                if (ptr != NULL){                  
                    if (prev_span == ptr){ //If the span is the first span
                        if (*find)
                            span_pop(find);
                    }
                    else{
                        while(ptr != NULL){
                            if (ptr == prev_span){                             
                               if (ptr->prev->next){
                                    span_pop(&(ptr->prev->next));
                                    break;
                                }
                            }
                            ptr = ptr->next;
                        }
                    }
                    prev_span->page_size += (*span)->page_size;
                    *span = prev_span; // I passed in a double pointer to now change the span pointer given in the argument for merge
                }
         }
    }

    pthread_spin_lock(&central_array_lock);
    SPAN* next_span = lookup((*span)->page_id + (*span)->page_size);
    pthread_spin_unlock(&central_array_lock);
    if (next_span == *span){
        pthread_spin_unlock(&heap_lock);
        return;
    }
     if (next_span != NULL && next_span->size_of_objects == 0) {
        comb_page_size = next_span->page_size + (*span)->page_size;
        if ( !next_span->num_of_objects_taken && (comb_page_size <= PAGEHEAPSIZE) && next_span->obj_ptr){
            find = central_page_heap + next_span->page_size -1;
            search_for_span = *find;
            ptr = search_for_span;
            if (ptr != NULL){                
                    if (next_span == ptr){ //If the span is the first span                        
                        if (*find)
                            span_pop(find);
                    }
                    else{                        
                        while(ptr != NULL){                            
                            if (ptr == next_span){
                                if (ptr->prev->next){
                                    span_pop(&(ptr->prev->next));
                                    break;
                                }
                            }
                            ptr = ptr->next;
                        }
                    }                
                    if (ptr != NULL)
                        (*span)->page_size += next_span->page_size;//Merged
            }
        }
     }    
    pthread_spin_unlock(&heap_lock);
}





size_t index_of_central_free_list(size_t block_size){
    if ( (block_size > 8 * PAGESIZE) || (block_size == 0)){

        fprintf(stderr,"usage: 0 <= blocksize <= 32K:  block size given is not indexable\n");
        return -100;
    }    
    size_t save;
    if (block_size <= 64){
        save = (64 - (int)block_size)/8;
        return 7 - save;
    }    
    else if (block_size <= 2048){
        save = (2048 - (int)block_size)/64;
        return (30 - save) + 8;
    }    
    else{
        save = (32768 - (int)block_size)/256;
        return (119 - save) + 39;

    }
}




size_t move_size(size_t size){
    if (!size)
        return 1;    
    int num_elem = (int)(MAXSMALLOBJ/size);
    if (num_elem < 3)
        num_elem = 3;
    else if (num_elem > 576)
        num_elem = 576;
    return num_elem;
}



size_t obj_size_to_page(size_t size){   
    size_t num_elem = move_size(size);
    size_t num_of_pages = (num_elem * size)/PAGESIZE;
    if (num_of_pages == 0)
        num_of_pages = 1;
    return num_of_pages;
}



SPAN* get_a_span_with_objsize(SPAN** list, size_t size ){
    

    
    SPAN* search = *list;

    while(search != NULL){

        if (search->obj_ptr != NULL){
            return search;
        }
        search = search->next;
    }

    size_t num_of_pages = obj_size_to_page(size);
    pthread_spin_lock(&heap_lock);
    SPAN* fetch_span = give_span_to_central_cache_or_fetch_from_system(num_of_pages);

    pthread_spin_unlock(&heap_lock);


    char *begin = (char *)fetch_span->obj_ptr;
    char *end = begin + (fetch_span->page_size * PAGESIZE);
    char* start = begin,*next;

    while ((next = start + size) < end){

        *((void**)start) = next;
        start = next;

    }

    if ( (end - start)  < 8)
        start = start-size;


    *((void**)start) = NULL;

    fetch_span->obj_ptr = (void *)begin;
    fetch_span->size_of_objects = size;
    fetch_span->num_of_objects_taken = 0;

    span_push(list,fetch_span);

    return fetch_span;

}

size_t Fetchnumobj(void** begin,void**end,size_t num_of_fetch,size_t size_of_obj){
   
    

    
    size_t index = index_of_central_free_list(size_of_obj);
    
    SPAN* search_span = central_free_list[index];
    SPAN* myspan = get_a_span_with_objsize(&central_free_list[index],size_of_obj);
    
    void* cur = myspan->obj_ptr;
    void *prev = cur;
    size_t fetch_num = 0;

    for( ; cur != NULL && fetch_num < num_of_fetch; fetch_num++){
        prev = cur;
        cur = *( (void**)cur );

    }

    *begin = myspan->obj_ptr;
    *end = prev;
    *( (void**)(*end) ) = NULL;

    myspan->obj_ptr = cur;
    myspan->num_of_objects_taken += fetch_num;


    return fetch_num;

}



void Release_Obj_to_Span_in_Central_FreeList(void* start,size_t obj_size){

    pthread_spin_lock(&central_lock);
    size_t index = index_of_central_free_list(obj_size);

     SPAN* search_span = central_free_list[index],*span_ptr;
     void *next;

     while (start != NULL){

         next = *( (void **)start );
         size_t page_id = calculate_pageid(start);

         pthread_spin_lock(&central_array_lock);
         SPAN* corresponding_span = lookup(page_id);

         pthread_spin_unlock(&central_array_lock);
         *( (void **)start) = corresponding_span->obj_ptr;

         corresponding_span->obj_ptr = start;
         corresponding_span->num_of_objects_taken--;

        if (!corresponding_span->num_of_objects_taken){

            if (central_free_list[index] == corresponding_span)
                span_pop(&(central_free_list[index]));

            else{

                for ( span_ptr = central_free_list[index] ; span_ptr->next != NULL ; span_ptr = span_ptr->next ){

                    if (span_ptr->next == corresponding_span){

                        span_pop(&(span_ptr->next));
                        break;
                    }

                }
            }

            corresponding_span->size_of_objects = 0;
            corresponding_span->next = NULL;
            corresponding_span->prev = NULL;    


            Free_Span_to_Central_Page_heap(&corresponding_span);

        }
        start = next;

     }
    pthread_spin_unlock(&central_lock);

}


void* tc_thread_init(){
    return (void *)*(&thread_cache);

}



void * fetch_from_central_cache(size_t byte){

    void *begin, *end;
    size_t bytes;
    size_t index = index_of_central_free_list(byte);

    if (index <= 7){
        bytes = (8 * index)+ 8;
    }

    else if (index <= 38){
        bytes = ((index - 7)* 64) + 64;
    }
    else{
        bytes =  ((index - 31)* 256) + 256;
    }


    FREELIST* freelist = &thread_cache[index];
    
    if (!freelist->threshold)
        freelist->threshold = 1;

    size_t num_to_move = MIN(move_size(bytes),freelist->threshold);
    size_t fetch_num = Fetchnumobj(&begin,&end,num_to_move,bytes); // keyr byte yalkewn shiba


    if (fetch_num && begin != NULL){
        
        *((void **)end) = freelist->list;
        freelist->list = *( (void**)begin);
        freelist->size += fetch_num - 1;
    }

    if (freelist->threshold == MIN(move_size(bytes),freelist->threshold))
        freelist->threshold += 1;

    return begin;


}

void* Allocate_from_ThreadCache(size_t size){

    void *fetch;
    size_t index = index_of_central_free_list(size);
    FREELIST* freelist = &thread_cache[index];

    void *object = freelist->list;

    if (freelist->list == NULL){

    
        pthread_spin_lock(&central_lock);
        fetch = fetch_from_central_cache(size);
        pthread_spin_unlock(&central_lock);

        return fetch;

    }

    else{

        object = freelist->list;
        freelist->list = *( (void**)object);
        freelist->size -= 1;

        return object;
    }

}

void free_threadcache_to_central(FREELIST * freelist,size_t byte){

    void *begin = freelist->list;
    freelist->size = 0;
    freelist->list = NULL;
    Release_Obj_to_Span_in_Central_FreeList(begin,byte);

    return;
}



void deallocate_to_thread(void* obj,size_t byte){


    
    size_t index = index_of_central_free_list(byte);
    FREELIST *freelist = &thread_cache[index];
    *((void **)obj) = freelist->list;

    freelist->list = obj;
    freelist->size += 1;

    if (freelist->size >= freelist->threshold){
        free_threadcache_to_central(freelist,byte);

    }
    return;


}


void* tc_malloc(size_t size){

    void* obj;
    SPAN* obj2;
    size_t num_of_pages = size/PAGESIZE;
    
    if (size == 0){
        return NULL;
    }


    if (size <= MAXSMALLOBJ){

        obj = Allocate_from_ThreadCache(size);
        return obj;


    }

    else {


        if ( size%PAGESIZE != 0){ // If size doesn't divide pagesize add 1 pages
            num_of_pages += 1;
        }

        pthread_spin_lock(&heap_lock);
        obj2 = give_span_to_central_cache_or_fetch_from_system(num_of_pages);
        pthread_spin_unlock(&heap_lock);
        
        return obj2->obj_ptr;

    }


}




void tc_free(void* ptr){

    if (ptr == NULL)
        return;

    size_t page_id = calculate_pageid(ptr);


    pthread_spin_lock(&central_array_lock);
    SPAN* find = lookup(page_id); // It might not be able to find the pointer
    pthread_spin_unlock(&central_array_lock);
    
    if (find == NULL){
        return;

    }


    if (find->size_of_objects == 0){ 
        
        Free_Span_to_Central_Page_heap(&find);//(When I comment this out it works, look at this function bruh)
        return;


    }

    else{ //  small object with size find->size_of_objects

        deallocate_to_thread(ptr,find->size_of_objects);

    }
    return;

}
