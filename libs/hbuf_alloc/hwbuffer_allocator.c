/*
 * Copyright (c) 2010, 2012, Freescale Semiconductor, Inc. All rights reserved.
 *
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
/*
 * Module Name:    buffer_allocator.c
 *
 * Description:    Implementation physical buffer allocator
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */  
 
/*
 * Changelog: 
 *
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>


#include "linux/ipu.h"


#if defined (IPU_ALLOC)
typedef int mem_desc;
#define PHY_MEM_ALLOC_IOCTL_ID IPU_ALLOC
#define PHY_MEM_FREE_IOCTL_ID IPU_FREE
#define MEMORY_DEVICE_NAME "/dev/mxc_ipu"
#define DESC2PHYADDRESS(memdesc) ((int)(*memdesc))
#define DESC2SIZE(memdesc) (*(memdesc))
#else
typedef ipu_mem_info mem_desc;
#define PHY_MEM_ALLOC_IOCTL_ID IPU_ALOC_MEM
#define PHY_MEM_FREE_IOCTL_ID IPU_FREE_MEM
#define MEMORY_DEVICE_NAME "/dev/mxc_ipu"
#define DESC2PHYADDRESS(memdesc) ((memdesc)->paddr)
#define DESC2SIZE(memdesc) ((memdesc)->size)
#endif



#define MAX_DIFF_SIZE_SHIFT 12 /* 4096 = 4k */
#define MAX_DIFF_SIZE (1<<MAX_DIFF_SIZE_SHIFT)

#define ROUND_UP_SIZE(size) ((((size)+(MAX_DIFF_SIZE-1))>>MAX_DIFF_SIZE_SHIFT)<<MAX_DIFF_SIZE_SHIFT)

#define FSL_MM_IOCTL(devfd, request, errorroute, ...)\
    do{\
        int ret;\
        if ((ret = ioctl((devfd), (request), ##__VA_ARGS__))<0){\
            printf("%s:%d ioctl error, return %d\n", __FILE__, __LINE__, ret);\
            goto errorroute;\
        }\
    }while(0)

#define LOCK() pthread_mutex_lock(&g_hwallocator_lock);
#define UNLOCK() pthread_mutex_unlock(&g_hwallocator_lock);


#define LIST2_ADD(head, item) \
    do{\
        (item)->prev = NULL;\
        (item)->next = (head);\
        if (head){\
            (head)->prev = item;\
        }\
        (head) = (item);\
    }while(0)

#define LIST2_REMOVE(head, item) \
    do{\
        if ((item)->next){\
            (item)->next->prev = (item)->prev;\
        }\
        if ((item)->prev){\
            (item)->prev->next = (item)->next;\
        }\
        if ((head)==(item)){\
            (head)=(item)->next;\
        }\
    }while(0)

#define LIST_PUSH(head, item) \
    do{\
        (item)->link = (head);\
        (head)=(item);\
    }while(0)

#define LIST_POP(head, item) \
    do{\
        (head)=(item)->link;\
    }while(0)



typedef struct _HWBufAllocator HWBufAllocator;
typedef struct _HWBufZone HWBufZone;


typedef struct _HWBufDesc{
    mem_desc memdesc;
    void * virt_addr;
    void * phy_addr;
    struct _HWBufDesc * prev; /* link in zone all pool */
    struct _HWBufDesc * next; /* link in zone all pool */
    struct _HWBufDesc * link; /* link in zone free pool */
    HWBufZone * zone;
}HWBufDesc;


struct _HWBufZone{
    int cnt;
    int freecnt;
    HWBufDesc * free;
    HWBufDesc * all;
    int size;
    struct _HWBufZone * prev; /* link in allocator */
    struct _HWBufZone * next; /* link in allocator */
};

struct _HWBufAllocator{
    int devfd;                 /* file descriptor for hw buffer allocator */
    struct _HWBufZone * zones;  /* zone for specific size hw buffer */
};

static HWBufAllocator g_hwallocator = 
    {
        0,      /* device */
        NULL   /* zones */
    };

static pthread_mutex_t g_hwallocator_lock = PTHREAD_MUTEX_INITIALIZER;



static HWBufZone *
find_or_create_zone_by_size(HWBufAllocator * allocator, int size)
{
    HWBufZone * zone = allocator->zones;
    int zone_size = ROUND_UP_SIZE(size);
    while(zone){
        
        if (zone_size==zone->size){
            return zone;
        }
        zone=zone->next;
    }
    zone = malloc(sizeof(HWBufZone));
    if (zone){
        zone->cnt = zone->freecnt = 0;
        zone->all = zone->free = NULL;
        zone->size = zone_size;
    }
    printf("hwbuf allocator zone(%d) created\n", zone_size);

    LIST2_ADD(allocator->zones, zone);
    return zone;
}

static void
destory_zone(HWBufAllocator * allocator, HWBufZone * zone)
{
    HWBufDesc * bufdesc = zone->all, * nextdesc;
    while(bufdesc){
        nextdesc = bufdesc->next;
        munmap(bufdesc->virt_addr, zone->size);
        FSL_MM_IOCTL(allocator->devfd, PHY_MEM_FREE_IOCTL_ID, error, &bufdesc->memdesc);
error:
        free(bufdesc);
        bufdesc=nextdesc;
    };

    printf("hwbuf allocator zone(%d) destroied.\n", zone->size);

    LIST2_REMOVE(allocator->zones, zone);
    free(zone);
}

static void
recycle_zone(HWBufAllocator * allocator, HWBufZone * zone)
{

    HWBufDesc * bufdesc = zone->free, * nextdesc;
    while(bufdesc){
        nextdesc = bufdesc->link;
        munmap(bufdesc->virt_addr, zone->size);
        FSL_MM_IOCTL(allocator->devfd, PHY_MEM_FREE_IOCTL_ID, error, &bufdesc->memdesc);
error:
        LIST2_REMOVE(zone->all,bufdesc);
        free(bufdesc);
        zone->cnt--;
        zone->freecnt--;
        bufdesc=nextdesc;
    };
    if (zone->all==NULL){
        LIST2_REMOVE(allocator->zones, zone);
        free(zone);
    }
}


static void
recycle_exclude(HWBufAllocator * allocator, HWBufZone * ex_zone)
{

    HWBufZone * zone = allocator->zones, *zonenext;
    while(zone){
        zonenext = zone->next;
        if (zone!=ex_zone){
            recycle_zone(allocator, zone);
        }
        zone=zonenext;
    }
}

static HWBufDesc * 
create_hwbuf(HWBufAllocator * allocator, HWBufZone * zone)
{

    HWBufDesc * bufdesc = NULL;
    int try_recycle = 1;
    bufdesc = malloc(sizeof(HWBufDesc));
    if (bufdesc==NULL){
        goto error;
    }
    mem_desc * memdesc =  &bufdesc->memdesc;
allochwbuf:
    memset(memdesc, 0, sizeof(mem_desc));
    DESC2SIZE(memdesc) = zone->size;
    FSL_MM_IOCTL(allocator->devfd, PHY_MEM_ALLOC_IOCTL_ID, recycle, memdesc);
    bufdesc->phy_addr = (void *)DESC2PHYADDRESS(memdesc);
    bufdesc->virt_addr =  mmap(NULL, zone->size,
			    PROT_READ | PROT_WRITE, MAP_SHARED,
			    allocator->devfd, bufdesc->phy_addr);
    if ((int)bufdesc->virt_addr==-1){
        printf("can not map virtaddr for size %d address %p\n", zone->size, bufdesc->phy_addr);
        FSL_MM_IOCTL(allocator->devfd, PHY_MEM_FREE_IOCTL_ID, error, memdesc);
        goto error;
    }
    bufdesc->zone = zone;
    LIST2_ADD(zone->all,bufdesc);
    zone->cnt++;
    return bufdesc;
recycle:
    if (try_recycle){
        try_recycle=0;
        recycle_exclude(allocator, zone);
        goto allochwbuf;
    }
error:
    if (bufdesc){
        free(bufdesc);
        bufdesc = NULL;
    }
    return bufdesc;
}

void * 
mfw_new_hw_buffer(int size, void **phy_addr, void **virt_addr, int flags)
{
    HWBufAllocator * allocator = &g_hwallocator;
    HWBufDesc * bufdesc = NULL;
    HWBufZone * zone = NULL;
    LOCK();

    if (allocator->devfd==0){
        allocator->devfd = open(MEMORY_DEVICE_NAME, O_RDWR);
        if (allocator->devfd<=0){
            allocator->devfd=0;
            printf("can not open memory device %s\n", MEMORY_DEVICE_NAME);
            goto error;
        }
    }
    
    zone = find_or_create_zone_by_size(allocator, size);
    if (zone==NULL){
        printf("can not create zone for size %d\n", size);
        goto error;
    }

    if (bufdesc=zone->free){
        LIST_POP(zone->free, bufdesc);
        zone->freecnt--;    
    }else{
        bufdesc = create_hwbuf(allocator, zone);
        if (bufdesc==NULL){
            printf("can not create hwbuf for size %d\n", size);
            goto error;
        }
        
    }
    (*phy_addr) = bufdesc->phy_addr;
    (*virt_addr) = bufdesc->virt_addr;
    UNLOCK();
    return bufdesc;

error:
    if ((zone) && (zone->all==NULL)){
        destory_zone(allocator, zone);
    }
    UNLOCK();
    return bufdesc;
}

void 
mfw_free_hw_buffer(void * handle)
{
    HWBufAllocator * allocator = &g_hwallocator;
    HWBufDesc * bufdesc;
    HWBufZone * zone;
    if (handle==NULL)
        return;
    bufdesc = (HWBufDesc *)handle;
    zone = bufdesc->zone;

    LOCK();
    LIST_PUSH(zone->free, bufdesc);
    zone->freecnt++;

    if (zone->freecnt==zone->cnt){
        destory_zone(allocator,zone);
    }
    UNLOCK();
}



void __attribute__ ((destructor)) hwbuffer_deconstructor(void);

void hwbuffer_deconstructor(void)
{

    HWBufAllocator * allocator = &g_hwallocator;
    HWBufZone * zone;
    LOCK();
    while(zone=allocator->zones){
        destory_zone(allocator, zone);
    }
    UNLOCK();
}



