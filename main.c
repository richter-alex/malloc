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

void init_allocator() {
    free_list.heap = mmap(NULL, HEAP_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);

    if (free_list.heap == MAP_FAILED) {
        printf("mmap failed. errno: %d\n", errno);
        exit(-1);
    }

    free_list.heap_used = 0;
    free_list.first_free = (struct Free_List_Node *)free_list.heap;
    free_list.first_free->size = HEAP_SIZE - sizeof(struct Free_List_Node);
    free_list.first_free->next = NULL;
}

// When the user requests memory, it searches in the linked list for a block where
// the data can fit. It then removes the element from the linked list and places an
// allocation header (which is required on free) just before the data.
// https://www.gingerbill.org/article/2021/11/30/memory-allocation-strategies-005/
void * mr_alloc(size_t size) {
    if (free_list.heap == NULL)
        init_allocator();

    struct Free_List_Node * current = free_list.first_free;
    struct Free_List_Node * next;
    struct Free_List_Node * previous;

    size_t remaining;

    // Find a Free_List_Node with an appropriate size.
    while (current != NULL) {
        if (current->size >= size)
            break;

        if (next == NULL) {
            printf("Free list exhausted. Crash time.\n");
            exit(-1);
        }

        previous = current;
        current = current->next;
    }

    remaining = current->size - size;

    // Remove the node from the linked list.
    if (previous != NULL)
        previous->next = current->next;

    // Place an allocation header.
    struct Allocation_Header alloc_header;
    alloc_header.size = size;
    *(struct Allocation_Header *)current = alloc_header;

    memset((char *)current + sizeof(struct Allocation_Header), 0, size);
    free_list.heap_used += sizeof(struct Allocation_Header) + size;

    // Insert a new node if there's room.
    if (remaining > 0) {
        struct Free_List_Node * new_node = (struct Free_List_Node *)((char *)current + sizeof(struct Allocation_Header) + size);
        new_node->size = remaining;

        if (previous != NULL)
            previous->next = new_node;

        new_node-> next = next;
        free_list.first_free = new_node;
    }

    // Give the people what they want
    void * ptr = &alloc_header + sizeof(struct Allocation_Header);

    return ptr;
}

// Need to approach this one with a fresh mind. Consecutive free() calls and not leaving the linked list in
// the correct state.
void mr_free(void * ptr) {
        struct Allocation_Header * alloc_ptr = (struct Allocation_Header*)ptr - 1;
        size_t size = alloc_ptr->size;

        memset(alloc_ptr, 0, size + sizeof(struct Allocation_Header));

        struct Free_List_Node fl_node;
        fl_node.size = (size + sizeof(struct Allocation_Header)) - sizeof(struct Free_List_Node);
        fl_node.next = free_list.first_free;
        *(struct Free_List_Node *)alloc_ptr = fl_node;

        free_list.first_free = (struct Free_List_Node *)alloc_ptr;
}

int main(int argc, char **argv) {
    int * alloc = mr_alloc(sizeof(int) * 2);
    *alloc = 1;

    int * new_alloc = mr_alloc(sizeof(int) * 2);
    *new_alloc = 1337;

    int * final_alloc = mr_alloc(sizeof(int) * 2);
    *final_alloc = 1337;

    return 0;
}
