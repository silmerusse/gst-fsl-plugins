/*
 * Copyright (c) 2008-2012, Freescale Semiconductor, Inc. All rights reserved.
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
 * Module Name:    mfw_gst_debug.h
 *
 * Description:    Head file of debug functions for Gstreamer plugins.
 *
 * Portability:    This code is written for Linux OS and Gstreamer
 */

/*
 * Changelog: 
 *
 * Aug, 22 2008 Sario HU<b01138@freescale.com>
 * - Initial version
 */


#ifndef __MFW_GST_DEBUG_H__
#define __MFW_GST_DEBUG_H__

#if 0

#ifndef MAX_SIZE_WARNING_THRESHOLD
#define MAX_SIZE_WARNING_THRESHOLD 1000000      //1M
#endif


typedef struct _memdesc
{
  struct _memdesc *next;
  guint size;
  void *mem;
  char *desstring;
  guint age;
} Mem_Desc;

typedef struct _mem_manager
{
  Mem_Desc *allocatedbuffer;
  Mem_Desc *freelist;
  Mem_Desc *head;
  Mem_Desc *tail;
  guint age;
  guint size;
  guint maxsize;
  guint num;
  guint maxnum;
  guint allocatednum;
  char *shortname;
} Mem_Mgr;


#define COPYMEMORYDESC(des, src) \
    do { \
        des->size = src->size; \
        des->mem = src->mem; \
        des->desstring = src->desstring;\
        des->age = src->age;\
    }while(0)

static Mem_Mgr mm_mgr = { 0 };



static Mem_Desc *
new_mem_desc ()
{
  Mem_Desc *newbuffer;
  if (mm_mgr.freelist) {
    newbuffer = mm_mgr.freelist;
    mm_mgr.freelist = newbuffer->next;
    return newbuffer;
  }
  if (mm_mgr.allocatednum)
    mm_mgr.allocatednum <<= 1;
  else
    mm_mgr.allocatednum = 4;
  if (newbuffer = g_malloc (sizeof (Mem_Desc) * mm_mgr.allocatednum)) {
    Mem_Desc *oldhead, *nb;
    int i = 0;

    oldhead = mm_mgr.head;
    nb = newbuffer;
    mm_mgr.freelist = mm_mgr.head = mm_mgr.tail = NULL;
    for (i = 0; i < (mm_mgr.allocatednum - 1); i++) {
      if (oldhead) {
        COPYMEMORYDESC (nb, oldhead);
        nb->next = NULL;
        if (mm_mgr.tail) {
          (mm_mgr.tail)->next = nb;
          mm_mgr.tail = nb;
        } else {
          mm_mgr.head = mm_mgr.tail = nb;
        }
        oldhead = oldhead->next;
      } else {
        nb->next = mm_mgr.freelist;
        mm_mgr.freelist = nb;
      }
      nb++;
    }
    if (mm_mgr.allocatedbuffer) {
      g_free (mm_mgr.allocatedbuffer);
    }
    mm_mgr.allocatedbuffer = newbuffer;
    return nb;
  } else {
    return newbuffer;
  }
}



static void
dbg_mem_init (char *shortname)
{
  memset (&mm_mgr, 0, sizeof (Mem_Mgr));
  if (shortname) {
    mm_mgr.shortname = shortname;
  } else {
    mm_mgr.shortname = __FILE__;
  }
}

static void
dbg_mem_print_nonfree (int detail)
{
  Mem_Desc *bt = mm_mgr.head;
  int tsize = 0, tnum = 0;
  int i = 0;

  if (detail)
    g_print (PURPLE_STR ("%s:\t Non-freed memory list:\n", mm_mgr.shortname));

  while (bt) {
    if (detail) {
      g_print ("[%03d]mem 0x%p,\tsize = %ld,\tage = %ld.", i++, bt->mem,
          bt->size, bt->age);
      if (bt->desstring) {
        g_print (" desc: %s\n", bt->desstring);
      } else {
        g_print ("\n");
      }
    }
    tnum++;
    tsize += bt->size;
    bt = bt->next;
  }
  if (detail)
    g_print (PURPLE_STR ("%s:\t End.\n", mm_mgr.shortname));
  g_print (PURPLE_STR ("Total non-free %d/%d(bytes)\n", tnum, tsize));

}

static void
dbg_mem_clear_nonfree ()
{
  Mem_Desc *bt = mm_mgr.head;
  while (bt) {
    g_free (bt->mem);
    bt = bt->next;
  }
}


static void
dbg_mem_deinit ()
{

  dbg_mem_print_nonfree (1);
  dbg_mem_clear_nonfree ();
  if (mm_mgr.allocatedbuffer) {
    g_free (mm_mgr.allocatedbuffer);
  }
  memset (&mm_mgr, 0, sizeof (Mem_Mgr));
}


static void *
dbg_mem_alloc (guint size, char *desc)
{
  Mem_Desc *bt;
  void *buf = NULL;
  if (size > MAX_SIZE_WARNING_THRESHOLD)
    g_print (RED_STR ("%s: try to allocate large memory %ld bytes %s\n",
            mm_mgr.shortname, size, desc ? desc : "no description"));
  if ((buf = g_malloc (size)) && (bt = new_mem_desc ())) {
    mm_mgr.age++;
    mm_mgr.size += size;
    mm_mgr.num++;
    if ((mm_mgr.num > mm_mgr.maxnum) || (mm_mgr.size > mm_mgr.maxsize)) {
      if (mm_mgr.num > mm_mgr.maxnum) {
        mm_mgr.maxnum = mm_mgr.num;
      }
      if (mm_mgr.size > mm_mgr.maxsize) {
        mm_mgr.maxsize = mm_mgr.size;
      }
#ifdef MEMORY_DEBUG_LIMITATION

      g_print ("%s:\t mem exceed %ld:%ld / %ld:%ld (pcs/bytes)\n",
          mm_mgr.shortname, mm_mgr.num, mm_mgr.maxnum, mm_mgr.size,
          mm_mgr.maxsize);
#endif
    }
    bt->size = size;
    bt->age = mm_mgr.age;
    bt->mem = buf;
    bt->desstring = desc;
    bt->next = NULL;

    if (mm_mgr.tail) {
      (mm_mgr.tail)->next = bt;
      mm_mgr.tail = bt;
    } else {
      mm_mgr.head = mm_mgr.tail = bt;
    }
  } else {
    if (buf) {
      g_free (buf);
      buf = NULL;
    } else {
      g_print (RED_STR ("%s:\t Can not allocate %ld bytes - %s\n",
              mm_mgr.shortname, size, desc));
    }
    g_print (PURPLE_STR
        ("FATAL ERROR: Can not allocate memory for memmanager!!\n", 0));
  }
  return buf;
}

static void
dbg_mem_free (void *mem, char *desc)
{
  Mem_Desc *bt = mm_mgr.head, *btpr = NULL;
  while (bt) {
    if (bt->mem == mem) {
      mm_mgr.size -= bt->size;
      mm_mgr.num--;
      g_free (mem);
      if (btpr) {
        btpr->next = bt->next;
        if (mm_mgr.tail == bt) {
          mm_mgr.tail = btpr;
        }
      } else {                  //head
        mm_mgr.head = bt->next;
        if (mm_mgr.head == NULL) {
          mm_mgr.tail = NULL;
        }
      }
      bt->next = mm_mgr.freelist;
      mm_mgr.freelist = bt;
      return;
    }
    btpr = bt;
    bt = bt->next;
  }
  g_print (PURPLE_STR ("%s Non-exist mem to free %p", mm_mgr.shortname, mem));

  if (desc) {
    g_print (PURPLE_STR ("- %s\n", desc));
  } else {
    g_print ("\n");
  }
}

#else

static void
dbg_print_buffer (char *buf, int len)
{
  int i;
  for (i = 0; i < len; i++) {
    if ((i & 0xf) == 0) {
      printf ("\n%06x: ", i);
    }
    g_print ("%02x ", buf[i]);

  }
  g_print ("\n");
}

typedef guint32 fsl_osal_u32;
typedef void *fsl_osal_ptr;



#define MAX_FILE_NAME_LEN 31

typedef struct _Res_Desc
{
  struct _Res_Desc *prev;
  struct _Res_Desc *next;
#ifdef   COPY_FILE_NAME
  char filename[MAX_FILE_NAME_LEN + 1];
#else
  char *filename;
#endif
  fsl_osal_u32 line;
  fsl_osal_u32 size;
  Fsl_Resource_Type type;
  void *key;
  fsl_osal_u32 age;
} Res_Desc;

typedef struct
{
  unsigned int age;
  Res_Desc *head;
  fsl_osal_u32 max_num;
  fsl_osal_u32 cur_num;
  fsl_osal_u32 max_size;
  fsl_osal_u32 cur_size;
  int init;
  //pthread_mutex_t lock;
} Memory_Manager;


#define MM_PREFIX "[MM] %s:%d "
#define MM_PREARG file, line

#define MMMALLOC g_malloc
#define MMMFREE g_free
#define MMMREALLOC g_realloc


static Memory_Manager gmm = { 0 };

static Memory_Manager *pmm = &gmm;
static gchar *g_mmmodulename = "Unknown";

static void
setfilename (Res_Desc * memdesc, char *fn)
{
  int i = 0;
  int len = strlen (fn);
  char *buf = &(fn[len - 1]);
  while ((buf[0] != '/') && buf >= fn) {
    i++;
    buf--;
  }

  if (i) {
    buf++;
#ifdef   COPY_FILE_NAME
    memcpy (memdesc->filename, buf, i);
    memdesc->filename[i] = '\0';
#else
    memdesc->filename = buf;
#endif
  }
#ifndef COPY_FILE_NAME
  else {
    memdesc->filename = NULL;
  }
#endif
}

#define INIT_RESDESC(memdesc, pbuffer, length, rest)\
    do{\
        (memdesc)->prev=(memdesc)->next=NULL;\
        (memdesc)->size = (length);\
        (memdesc)->key = (pbuffer);\
        (memdesc)->type = (rest);\
        (memdesc)->line = line;\
        (memdesc)->age = pmm->age++;\
        if (file)\
            setfilename((memdesc), file);\
    }while(0)


static void
init_mm_mutex ()
{

#if 0
  pthread_mutexattr_t attr;


  pthread_mutexattr_init (&attr);
  pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_TIMED_NP);

  pthread_mutex_init (&pmm->lock, &attr);
#endif
}

static void
lock_mm ()
{
#if 0
  pthread_mutex_lock (&pmm->lock);
#endif
}

static void
unlock_mm ()
{
#if 0
  pthread_mutex_unlock (&pmm->lock);
#endif
}

static void
insert_Res_Desc (Res_Desc * memdesc)
{

  lock_mm ();
  if (pmm->head) {
    pmm->head->prev = memdesc;
    memdesc->next = pmm->head;
    pmm->head = memdesc;
  } else {
    pmm->head = memdesc;
  }

  if (memdesc->type == RES_MEM) {
    pmm->cur_num++;
    pmm->cur_size += memdesc->size;

    if (pmm->max_num < pmm->cur_num) {
      pmm->max_num = pmm->cur_num;
    }

    if (pmm->max_size < pmm->cur_size) {
      pmm->max_size = pmm->cur_size;
    }
  }
  unlock_mm ();
}

static Res_Desc *
check_Res_Desc (fsl_osal_ptr ptr, Fsl_Resource_Type type)
{
  Res_Desc *memdesc;
  lock_mm ();
  memdesc = pmm->head;
  while (memdesc) {
    if ((memdesc->key == ptr) && (memdesc->type == type)) {
      break;
    }
    memdesc = memdesc->next;
  }
  unlock_mm ();
  return memdesc;

}

static void
plug_Res_Desc (Res_Desc * memdesc)
{
  lock_mm ();
  if (pmm->head != memdesc) {
    memdesc->prev->next = memdesc->next;
    if (memdesc->next)
      memdesc->next->prev = memdesc->prev;
  } else {
    pmm->head = memdesc->next;
    if (memdesc->next)
      memdesc->next->prev = memdesc->prev;
  }

  if (memdesc->type == RES_MEM) {
    pmm->cur_num--;
    pmm->cur_size -= memdesc->size;
  }
  unlock_mm ();
}


static void
print_non_free_res_by_type (Fsl_Resource_Type restype)
{
  Res_Desc *resdesc = pmm->head;
  int i = 0;
  printf (CYAN_STR ("Non-free list for type%d:\n", restype));
  while (resdesc) {
    if (resdesc->type == restype) {
      printf (CYAN_STR ("%03d key %p: size %d, age %d, at %s:%d\n", ++i,
              resdesc->key, resdesc->size, resdesc->age, resdesc->filename,
              resdesc->line));
    }
    resdesc = resdesc->next;
  }
}

static void
print_non_free_resource ()
{
  Fsl_Resource_Type restype;
  if (pmm->head) {
    printf (CYAN_STR ("Module: %s\n", g_mmmodulename));
    printf (CYAN_STR ("Memory status: (cur/max) num %d:%d size %d:%d\n",
            pmm->cur_num, pmm->max_num, pmm->cur_size, pmm->max_size));
    for (restype = RES_MEM; restype < RES_TYPE_MAX; restype++) {
      print_non_free_res_by_type (restype);
    }
  } else {
    printf (GREEN_STR ("Module: %s\n", g_mmmodulename));
    printf (GREEN_STR ("All Resources are freed!\n", 0));
    printf (GREEN_STR ("Memory status: (max) num %d size %d\n", pmm->max_num,
            pmm->max_size));
  }
}

static fsl_osal_ptr
fsl_alloc_dbg (int size, const char *file, int line)
{

  Res_Desc *memdesc = NULL;
  fsl_osal_ptr ptr = NULL;
  ptr = MMMALLOC (size);
  memdesc = MMMALLOC (sizeof (Res_Desc));
  if ((ptr == NULL) || (memdesc == NULL)) {
    goto ErrAlloc;
  }
  INIT_RESDESC (memdesc, ptr, size, RES_MEM);
  insert_Res_Desc (memdesc);

  return ptr;
ErrAlloc:
  if (ptr)
    MMMFREE (ptr);
  if (memdesc)
    MMMFREE (memdesc);

  printf (RED_STR (MM_PREFIX "Can not allocate (size%d)\n", MM_PREARG, size));

  return NULL;
}

static fsl_osal_ptr
fsl_realloc_dbg (fsl_osal_ptr ptr, int size, const char *file, int line)
{

  Res_Desc *memdesc = NULL;

  if (ptr) {
    if ((memdesc = check_Res_Desc (ptr, RES_MEM)) == NULL) {
      printf (RED_STR (MM_PREFIX "realloc: memdesc does not exist(%p)\n",
              MM_PREARG, ptr));
      return NULL;
    }
    plug_Res_Desc (memdesc);
    MMMFREE (memdesc);

  }

  ptr = MMMREALLOC (ptr, size);
  memdesc = MMMALLOC (sizeof (Res_Desc));
  if ((ptr == NULL) || (memdesc == NULL)) {
    goto ErrAlloc;
  }
  INIT_RESDESC (memdesc, ptr, size, RES_MEM);
  insert_Res_Desc (memdesc);

  return ptr;
ErrAlloc:
  if (ptr)
    MMMFREE (ptr);
  if (memdesc)
    MMMFREE (memdesc);

  printf (RED_STR (MM_PREFIX "Can not reallocate (size%d)\n", MM_PREARG, size));

  return NULL;
}





static void
fsl_dealloc_dbg (fsl_osal_ptr ptr, const char *file, int line)
{

  Res_Desc *memdesc;
  if ((memdesc = check_Res_Desc (ptr, RES_MEM)) == NULL) {
    printf (RED_STR (MM_PREFIX "dealloc: memdesc does not exist(%p)\n",
            MM_PREARG, ptr));
    return;
  }
  plug_Res_Desc (memdesc);
  MMMFREE (ptr);
  MMMFREE (memdesc);
}

static int
fsl_check_res_debug (void *reskey, Fsl_Resource_Type restype, const char *file,
    int line)
{
  if (check_Res_Desc (reskey, restype) == NULL) {
    printf (RED_STR (MM_PREFIX "Can not find res (key%p)\n", MM_PREARG,
            reskey));
    return 1;
  } else {
    return 0;
  }
}

static void
fsl_reg_res_debug (void *reskey, Fsl_Resource_Type restype, const char *file,
    int line)
{
  Res_Desc *resdesc;
  if (reskey == NULL) {
    printf (RED_STR ("Reg null pointer refused at %s:%d\n", file, line));
    return;
  }

  resdesc = MMMALLOC (sizeof (Res_Desc));
  if (resdesc) {
    INIT_RESDESC (resdesc, reskey, 0, restype);
    insert_Res_Desc (resdesc);
  } else {
    printf (RED_STR (MM_PREFIX "Can not register resource\n", MM_PREARG));
  }

}

static void
fsl_unreg_res_debug (void *reskey, Fsl_Resource_Type restype, const char *file,
    int line)
{
  Res_Desc *resdesc;
  if (reskey == NULL) {
    printf (RED_STR ("Unreg null pointer refused at %s:%d\n", file, line));
    return;
  }
  if (resdesc = check_Res_Desc (reskey, restype)) {
    plug_Res_Desc (resdesc);
    MMMFREE (resdesc);
  } else {
    printf (RED_STR (MM_PREFIX "Can not find resource(key%p)\n", MM_PREARG,
            reskey));
  }

}
#endif

#endif //__MFW_GST_DEBUG_H__
