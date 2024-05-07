#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define HEAP_SIZE 4096

#ifdef WIN32
#define NULL 0
#endif

/*
    mmap(NULL, HEAP_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
*/

struct Allocation_Header {
    size_t size;
    size_t padding;
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

void coalesce() {
    struct Free_List_Node * cursor = free_list.first_free;

    while (cursor != 0) {
        if ((char *)cursor + sizeof(struct Free_List_Node) + cursor->size == (char *)cursor->next) {
            if (cursor->next != 0) {
                struct Free_List_Node * target = cursor->next;
                size_t target_size = target->size;

                if (cursor->next != NULL)
                  cursor->next = cursor->next->next;

                cursor->size = cursor->size + target_size + sizeof(struct Free_List_Node);
                memset(target, 0, target_size + sizeof(struct Free_List_Node));
                continue;
            }
        } else {
            cursor = cursor->next;
        }
    }
}

// When the user requests memory, it searches in the linked list for a block where
// the data can fit. It then removes the element from the linked list and places an
// allocation header (which is required on free) just before the data.
// https://www.gingerbill.org/article/2021/11/30/memory-allocation-strategies-005/
void * mr_alloc(size_t size) {
    if (free_list.heap == NULL)
        init_allocator();

    struct Free_List_Node * current = free_list.first_free;
    struct Free_List_Node * next = NULL;
    struct Free_List_Node * previous = NULL;

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

        new_node->next = next;
        free_list.first_free = new_node;
    }

    // Give the people what they want
    void * ptr = (char *)current + sizeof(struct Allocation_Header);

    return ptr;
}

void mr_free(void * ptr) {
    // What the fuck?
    //struct Allocation_Header alloc_header = *(struct Allocation_Header *)((char *)ptr - sizeof(struct Allocation_Header));
    struct Allocation_Header alloc_header = *((struct Allocation_Header *)ptr - 1);
    struct Free_List_Node new_node = { .next = NULL };
    struct Free_List_Node * cursor = free_list.first_free;
    struct Free_List_Node * previous = NULL;

    while (cursor < (struct Free_List_Node *)ptr) {
        previous = cursor;
        cursor = cursor->next;
    }

    if (previous != NULL)
        previous->next = (struct Free_List_Node *)(struct Allocation_Header *)((char *)ptr - sizeof(struct Allocation_Header));
    new_node.next = cursor;
    new_node.size = alloc_header.size;

    memset(ptr - sizeof(struct Allocation_Header), 0, alloc_header.size + sizeof(struct Allocation_Header));
    
    free_list.heap_used -= new_node.size + sizeof(new_node);

    if ((struct Free_List_Node *)ptr -1 < free_list.first_free)
      free_list.first_free = (struct Free_List_Node *)((char *)ptr - sizeof(struct Free_List_Node));

    *((struct Free_List_Node *)ptr - 1) = new_node;

    coalesce();
}

int main(void) {
    int * alloc = mr_alloc(sizeof(int) * 2);
    *alloc = 1;

    int * new_alloc = mr_alloc(sizeof(int) * 4);
    *new_alloc = 1337;

    int * final_alloc = mr_alloc(sizeof(int) * 2);
    *final_alloc = 1337;

    mr_free(alloc);
    mr_free(final_alloc);
    mr_free(new_alloc);

    return 0;
}
