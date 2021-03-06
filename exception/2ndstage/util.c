/*  util.c

DESCRIPTION

    Yeah, everyone needs a utility module.

*/

#include "vars.h"
#include "voot.h"

#include "util.h"

static uint8 *malloc_root;
static uint32 malloc_fail_count;
/* DEPRECATED: This was is malloc()'s key.
static const uint8 malloc_key[] = { 0xE6, 0x2F, 0xFC, 0x7F, 0x02, 0x00 }; */
static const uint8 malloc_key[] = { 0xe6, 0x2f, 0xc6, 0x2f, 0xfc, 0x7f, 0x02, 0x00 };

/* CREDIT: Borrowed from Dan Potter's libc */
void* memmove(void *dest, const void *src, uint32 count)
{
    uint8 *tmp, *s;

    if (dest <= src)
    {
        tmp = (uint8 *) dest;
        s = (uint8 *) src;

        while (count--)
            *tmp++ = *s++;
    }
    else
    {
        tmp = (uint8 *) dest + count;
        s = (uint8 *) src + count;

        while (count--)
            *--tmp = *--s;
    }

    return dest;
}

/* CREDIT: Borrowed from Dan Potter's libdream. */
void vid_waitvbl(void)
{
    volatile uint32 *vbl = ((volatile uint32 *) 0xa05f8000) + 0x010c / sizeof(uint32);

    while (!(*vbl & 0x01ff));
    while (*vbl & 0x01ff);
}

uint8* search_sysmem(const uint8 *key, uint32 key_size)
{
    return search_sysmem_at(key, key_size, SYS_MEM_START, SYS_MEM_END);
}

uint8* search_sysmem_at(const uint8 *key, uint32 key_size, const uint8 *start_loc, const uint8 *end_loc)
{
    const uint8 *cur_loc;

    for (cur_loc = start_loc; cur_loc < end_loc; cur_loc++)
        if (*cur_loc == key[0])
            if(!memcmp((const uint8 *) cur_loc, key, key_size))
                return (uint8 *) cur_loc;     /* So we have a match. Report it. */

    return NULL;
}

void grep_memory(const uint8 *key, uint32 key_size)
{
    uint8 *mem_loc;

    voot_debug("Grepping memory for '%s' ...", key);

    mem_loc = SYS_MEM_START;

    while ((mem_loc = search_sysmem_at(key, key_size, mem_loc, SYS_MEM_END)))
    {
        voot_debug("Match @ %x", mem_loc);
        mem_loc++;
    }

    voot_debug("Grep done!");
}

/* CREDIT: Accessor to Katana syMalloc() */
void malloc_init(void)
{
    if (!malloc_root)
        malloc_root = search_sysmem(malloc_key, sizeof(malloc_key));
}

void malloc_stat(uint32 *freesize, uint32 *max_freesize)
{
    if (malloc_root)
        return (*(void (*)()) malloc_root)(freesize, max_freesize);
}

void* malloc(uint32 size)
{
    void *mem;

    if (malloc_root)
    {
        mem = (*(void* (*)()) (malloc_root + MALLOC_MALLOC_INDEX))(size);
        if (!mem)
            malloc_fail_count++;
        return mem;
    }
    else
        return NULL;
}

void free(void *data)
{
    if (malloc_root)
        return (*(void (*)()) (malloc_root + MALLOC_FREE_INDEX))(data);
}

/* CREDIT: thanks to AndrewK - store queue accessor */

#define QACR0 (*(volatile uint32 *) 0xff000038)
#define QACR1 (*(volatile uint32 *) 0xff00003c)

/* copies n bytes from src to dest, dest must be 32-byte aligned */
void* sq_cpy(void *dest, const uint32 *src, uint32 n)
{
    uint32 *d = (uint32 *) (0xe0000000 | (((uint32) dest) & 0x03ffffc0));
    
    /* Set store queue memory area as desired */
    QACR0 = ((((uint32) dest) >> 26) << 2) & 0x1c;
    QACR1 = ((((uint32) dest) >> 26) << 2) & 0x1c;
    
    /* fill/write queues as many times necessary */
    n >>= 5;
    while (n--)
    {
        d[0] = *(src++);
        d[1] = *(src++);
        d[2] = *(src++);
        d[3] = *(src++);
        d[4] = *(src++);
        d[5] = *(src++);
        d[6] = *(src++);
        d[7] = *(src++);

        asm("pref @%0" : : "r" (d));

        d += 8;
    }

    /* Wait for both store queues to complete */
    d = (uint32 *) 0xe0000000;
    d[0] = d[8] = 0;

    return dest;
}

/* Returns seconds since 1/1/1950 */
uint32 time(void)
{
    uint32 vals[2];

    vals[0] = *((volatile uint32 *) (0xA0710000));
    vals[1] = *((volatile uint32 *) (0xA0710004));

    return (((vals[0] & 0x0000FFFF) << 16) | (vals[1] & 0x0000FFFF));
}
