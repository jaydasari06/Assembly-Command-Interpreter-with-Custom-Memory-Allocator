#include "umalloc.h"
#include "csbrk.h"
#include <stdio.h>
#include <assert.h>
#include "ansicolors.h"

const char author[] = ANSI_BOLD ANSI_COLOR_RED "Jay Dasari" ANSI_RESET;

/*
 * The following helpers can be used to interact with the mem_block_header_t
 * struct, they can be adjusted as necessary.
 */

mem_block_header_t *free_heads[BIN_COUNT];

/*
 * select_bin - selects a free list bin to use based on the
 * block size.
 */

mem_block_header_t *select_bin(size_t size)
{
    // 4 bins with sizes <128, <512, <1024, and >1024
    if (size < 128)
    {
        return free_heads[0];
    }
    else if (size < 512)
    {
        return free_heads[1];
    }
    else if (size < 1024)
    {
        return free_heads[2];
    }
    else
    {
        return free_heads[3];
    }
}

/*
 * block_metadata - returns true if a block is marked as allocated.
 */
bool is_allocated(mem_block_header_t *block)
{
    if (block != NULL)
    {
        return (block->block_metadata & 1) == 1;
    }
    return false;
}

/*
 * allocate - marks a block as allocated.
 */
void allocate(mem_block_header_t *block)
{
    // only run if unallocated
    if (!is_allocated(block))
    {
        block->block_metadata += 1;
    }
}

/*
 * deallocate - marks a block as unallocated.
 */
void deallocate(mem_block_header_t *block)
{
    // only run if allocated
    if (is_allocated(block))
    {
        block->block_metadata -= 1;
    }
}

/*
 * get_size - gets the size of the block.
 */
size_t get_size(mem_block_header_t *block)
{
    if (block != NULL)
    {
        return block->block_metadata >> 4;
    }
    return 0;
}

/*
 * get_next - gets the next block.
 */
mem_block_header_t *get_next(mem_block_header_t *block)
{
    return block->next;
}

/*
 * set_block_metadata
 * Optional helper method that can be used to initialize the fields for the
 * memory block struct.
 */
void set_block_metadata(mem_block_header_t *block, size_t size, bool alloc)
{
    // add size block_metadata and then shift left to make space for alloc bit
    block->block_metadata = size;
    block->block_metadata = block->block_metadata << 4;
    block->block_metadata += (int)alloc;
}

/*
 * get_payload - gets the payload of the block.
 */
void *get_payload(mem_block_header_t *block)
{
    // 1 represents 16 bits due to void cast
    return (void *)(block + 1);
}

/*
 * get_header - given a payload, returns the block.
 */
mem_block_header_t *get_header(void *payload)
{
    return (mem_block_header_t *)payload - 1;
}

int select_bin_index(size_t size)
{
    if (size < 128)
    {
        return 0;
    }
    else if (size < 512)
    {
        return 1;
    }
    else if (size < 1024)
    {
        return 2;
    }
    else
    {
        return 3;
    }
}
/*
 * The following are helper functions that can be implemented to assist in your
 * design, but they are not required.
 */

/*
 * find - finds a free block that can satisfy the umalloc request.
 */
mem_block_header_t *find(size_t payload_size)
{
    int index = select_bin_index(payload_size);
    while (index < BIN_COUNT)
    {
        // find first free node with min space, remove from list and return
        // if none free in bin, try to move in next bin
        mem_block_header_t *bin = free_heads[index];
        mem_block_header_t *prev = NULL;
        mem_block_header_t *returnBin = NULL;
        while (bin != NULL)
        {
            if (get_size(bin) >= payload_size)
            {
                if (prev == NULL)
                {
                    returnBin = bin;
                    free_heads[index] = bin->next;
                }
                else
                {
                    returnBin = bin;
                    prev->next = bin->next;
                }
                return returnBin;
            }
            prev = bin;
            bin = get_next(bin);
        }
        index++;
    }
    return NULL;
}

/*
 * extend - extends the heap if more memory is required.
 */
mem_block_header_t *extend(size_t size)
{
    mem_block_header_t *extended = (mem_block_header_t *)csbrk(ALIGN(size + sizeof(mem_block_header_t)));
    if (extended == NULL)
    {
        return NULL;
    }
    set_block_metadata(extended, size, false);
    return extended;
}

// helper method to add block to freelist, used in split and ufree
void freelist_add(mem_block_header_t *block, size_t size)
{
    int index = select_bin_index(size);
    mem_block_header_t *bin = free_heads[index];
    mem_block_header_t *prev = NULL;
    while (bin != NULL && get_size(block) > get_size(bin))
    {
        prev = bin;
        bin = bin->next;
    }
    if (prev == NULL)
    {
        block->next = free_heads[index];
        free_heads[index] = block;
    }
    else
    {
        block->next = prev->next;
        prev->next = block;
    }
}

/*
 * split - splits a given block in parts, one allocated, one free.
 */
mem_block_header_t *split(mem_block_header_t *block, size_t new_block_size)
{
    if (get_size(block) >= ALIGN(new_block_size) + sizeof(mem_block_header_t) + 32)
    {
        size_t remaining_size = get_size(block) - ALIGN(new_block_size) - sizeof(mem_block_header_t);
        set_block_metadata(block, ALIGN(new_block_size), true);
        mem_block_header_t *freeBlock = (mem_block_header_t *)((uintptr_t)block + sizeof(mem_block_header_t) + ALIGN(new_block_size));
        set_block_metadata(freeBlock, remaining_size, false);
        freelist_add(freeBlock, remaining_size);
    }
    return block;
}

/*
 * coalesce - coalesces a free memory block with neighbors.
 */
mem_block_header_t *coalesce(mem_block_header_t *block)
{
    mem_block_header_t *current = block;
    bool coalesced;
    // do-while loop runs until there are no more blocks to coalesce in bin
    do
    {
        coalesced = false;
        int index = select_bin_index(get_size(current));
        mem_block_header_t *prev = NULL;
        mem_block_header_t *next = free_heads[index];
        while (next != NULL)
        {
            if (next != current)
            {
                // checks if memory addresses are right next to each other and combines if they are
                if ((uintptr_t)next + get_size(next) + sizeof(mem_block_header_t) == (uintptr_t)current)
                {
                    set_block_metadata(next, get_size(next) + get_size(current) + sizeof(mem_block_header_t), false);
                    if (prev == NULL)
                    {
                        free_heads[index] = next->next;
                    }
                    else
                    {
                        prev->next = next->next;
                    }
                    current = next;
                    coalesced = true;
                    break;
                }
                else if ((uintptr_t)current + get_size(current) + sizeof(mem_block_header_t) == (uintptr_t)next)
                {
                    set_block_metadata(current, get_size(current) + get_size(next) + sizeof(mem_block_header_t), false);
                    if (prev == NULL)
                    {
                        free_heads[index] = next->next;
                    }
                    else
                    {
                        prev->next = next->next;
                    }
                    coalesced = true;
                    break;
                }
            }
            prev = next;
            next = next->next;
        }
    } while (coalesced);
    return current;
}

/*
 * uinit - Used initialize metadata required to manage the heap
 * along with allocating initial memory.
 */
int uinit()
{
    // initializes bins with memory based on bin size
    size_t arr[] = {64, 256, 512, 2048};
    for (int i = 0; i < BIN_COUNT; i++)
    {
        mem_block_header_t *bin = free_heads[i];
        bin = extend(arr[i]);
        set_block_metadata(bin, arr[i], false);
    }
    return 0;
}

/*
 * umalloc -  allocates size bytes and returns a pointer to the allocated memory.
 */
void *umalloc(size_t size)
{
    // find free node, if none found, extend space and return
    mem_block_header_t *mem = find(size);
    if (mem == NULL)
    {
        mem = extend(size);
    }
    mem = split(mem, size);
    allocate(mem);
    return get_payload(mem);
}

/**
 * @param ptr the pointer to the memory to be freed,
 * must have been called by a previous malloc call
 * @brief frees the memory space pointed to by ptr.
 */
void ufree(void *ptr)
{
    if (ptr == NULL)
    {
        return; // Do nothing if the pointer is NULL
    }

    // adds block back into freelist after it has been deallocated
    mem_block_header_t *pointer = get_header(ptr);
    deallocate(pointer);
    pointer = coalesce(pointer);
    freelist_add(pointer, get_size(pointer));
}