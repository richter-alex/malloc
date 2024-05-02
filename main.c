#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define HEAP_SIZE 4096

/*
    mmap(NULL, HEAP_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
*/

struct Allocation_Header {
    size_t size;
};

struct Free_List_Node {
    size_t size;
    struct Free_List_Node * next;
};

struct Free_List {
    void * heap;
    struct Free_List_Node * first_free;
    int heap_used;
};

struct Free_List free_list = { .heap = NULL, .first_free = NULL, .heap_used = 0 };
struct Free_List * free_list_ptr = &free_list;

void init_allocator() {
    free_list_ptr->heap = mmap(NULL, HEAP_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
        
    if (free_list_ptr->heap == MAP_FAILED) {
        printf("mmap failed. errno: %d\n", errno);
        exit(-1);
    }
    
    free_list_ptr->heap_used = 0;
    free_list_ptr->first_free = (struct Free_List_Node *)free_list_ptr->heap;
    free_list_ptr->first_free->size = HEAP_SIZE - sizeof(struct Free_List_Node);
    free_list_ptr->first_free->next = NULL;
}

void * mr_alloc(size_t size) {
    if (free_list_ptr->heap == NULL)
        init_allocator();

    struct Free_List_Node * current = free_list_ptr->first_free;

    while (current->size < size) {
        if (current->next == NULL) {
            printf("Free list exhausted. Crash time.\n");
            exit(-1);
        }
        current = current->next;
    }

    struct Allocation_Header * alloc_header = (struct Allocation_Header *)current;
    alloc_header->size = size;
    memset(current + sizeof(struct Allocation_Header), 0, size );
    free_list_ptr->heap_used += sizeof(struct Allocation_Header) + size;

    free_list_ptr->first_free = (struct Free_List_Node *)((char *)alloc_header + sizeof(struct Allocation_Header) + size);
    free_list_ptr->first_free->size = HEAP_SIZE - free_list_ptr->heap_used;

    void * ptr = (char *)alloc_header + sizeof(struct Allocation_Header);

    return ptr;
}

void mr_free(void * ptr) {
        struct Allocation_Header * alloc_ptr = (struct Allocation_Header*)ptr - 1;
        size_t size = alloc_ptr->size;

        memset(alloc_ptr, 0, size + sizeof(struct Allocation_Header));

        struct Free_List_Node fl_node;
        fl_node.size = (size + sizeof(struct Allocation_Header)) - sizeof(struct Free_List_Node);
        fl_node.next = free_list_ptr->first_free;
        *(struct Free_List_Node *)alloc_ptr = fl_node;
        free_list_ptr->first_free = (struct Free_List_Node *)alloc_ptr;

        return;
}

int main(int argc, char **argv) {
    unsigned char * new_alloc = (unsigned char *)mr_alloc(sizeof(unsigned char) * 10);
    *new_alloc = 1;
    unsigned char * another_alloc = (unsigned char *)mr_alloc(sizeof(unsigned char) * 10);
    *another_alloc = 1;
    unsigned int * yet_another_alloc  = (unsigned int *)mr_alloc(sizeof(unsigned int) * 5);
    *yet_another_alloc = 128;

    return 0;
}
