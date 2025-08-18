/*
 * Copyright (c) 2025, horoni. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <linux/kernel.h>     /* For working with kernel                    */
#include <linux/module.h>     /* Necessary for kernel module                */
#include <linux/fs.h>         /* file, file_operations, inode, file_path()  */
#include <linux/miscdevice.h> /* miscdevice, misc_register()                */
#include <linux/mm_types.h>   /* mm_struct, page, vm_area_struct            */
#include <linux/pid.h>        /* find_get_pid, pid, pid_task                */
#include <linux/sched.h>      /* task_struct                                */
#include <linux/types.h>      /* phys_addr_t, pid_t                         */
#include <linux/uaccess.h>    /* copy_from_user(), copy_to_user()           */
#include <linux/hw_breakpoint.h>

#include <asm/io.h>             /* ioremap_cache, iounmap */
#include <asm/memory.h>         /* page_to_phys           */
#include <asm/page-def.h>       /* PAGE_MASK              */
#include <asm/pgtable-types.h>  /* pgd_t, pmd_t, pte_t    */

#include "kmemhack.h"

static int rw_mem(struct __memory_t *req, char flag);
static int read_maps(struct __maps_t *req);
static struct mm_struct *get_mm(pid_t pid);
static phys_addr_t get_pa(struct mm_struct *mm, unsigned long vaddr);

static long driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
  struct __maps_t map;
  struct __memory_t mem;

  switch (cmd) {
    case GET_MAPS:
      if (copy_from_user(&map, (typeof(map) *)arg, sizeof(map)))
        return -1;
      if (read_maps(&map))
        return -1;
      if (copy_to_user((typeof(map) *)arg, &map, sizeof(map)))
        return -1;
      break;
    case GET_MEM:
      if (copy_from_user(&mem, (typeof(mem) *)arg, sizeof(mem)))
        return -1;
      rw_mem(&mem, 0);
      break;
    case SET_MEM:
      if (copy_from_user(&mem, (typeof(mem) *)arg, sizeof(mem)))
        return -1;
      rw_mem(&mem, 1);
      break;
  }
  return 0;
}

/* flag == 0 -> Read; flag == 1 -> Write */
static int rw_mem(struct __memory_t *req, char flag)
{
  struct mm_struct *mm;
  void *pmap = NULL;
  phys_addr_t pa;
  int ret = -1;

  mm = get_mm(req->pid);
  if (!mm) {
    printk("[kmemhack]: rw_mem: mm is NULL");
    goto out;
  }

  /* Get physical address */
  pa = get_pa(mm, req->addr);
  if (!pa) {
    printk("[kmemhack]: rw_mem: get_pa() err");
    goto out;
  }

  /* Map physical memory */
  pmap = ioremap_cache(pa, req->n);
  if (!pmap) {
    printk("[kmemhack]: rw_mem: ioremap_cache() err");
    goto out;
  }

  if (flag) {
    /* pmap <- buf; Copy memory segment of N bytes from user buffer */
    if (copy_from_user(pmap, req->buf, req->n)) {
      printk("[kmemhack]: rw_mem: copy_from_user() err");
      goto out;
    }
  } else {
    /* pmap -> buf; Copy memory segment of N bytes to user buffer */
    if (copy_to_user(req->buf, pmap, req->n)) {
      printk("[kmemhack]: rw_mem: copy_to_user() err");
      goto out;
    }
  }

  ret = 0;
out:
  if (pmap)
    iounmap(pmap);
  return ret;
}

static int read_maps(struct __maps_t *req)
{
  struct mm_struct *mm;
  struct vm_area_struct *mmap;
  char buf[128];
  char *p;
  int ret = -1;

  mm = get_mm(req->pid);
  if (!mm) {
    printk("[kmemhack]: read_maps: mm is NULL");
    goto out;
  }

  mmap = mm->mmap;
  if (!mmap) {
    printk("[kmemhack]: read_maps: mmap is NULL");
    goto out;
  }

  /* Loop through all maps */
  for (; mmap; mmap = mmap->vm_next) {
    /* Skip anonymous map */
    if (!mmap->vm_file)
      continue;
    p = file_path(mmap->vm_file, buf, sizeof(buf));
    if (!strstr(p, req->name))
      continue;
    req->start = mmap->vm_start;
    req->end   = mmap->vm_end;
    req->flags = mmap->vm_flags;
    /* Break after first entry found */
    break;
  }
  ret = 0;
out:
  return ret;
}

static struct mm_struct *get_mm(pid_t pid_nr)
{
  struct pid *pid;
  struct task_struct *task;
  struct mm_struct *mm = NULL;

  pid = find_get_pid(pid_nr);
  if (!pid){
    printk("[kmemhack]: get_mm: pid is NULL");
    goto out;
  }

  task = pid_task(pid, PIDTYPE_PID);
  if (!task) {
    printk("[kmemhack]: get_mm: task is NULL");
    goto out;
  }

  mm = task->mm;
out:
  return mm;
}

static phys_addr_t get_pa(struct mm_struct *mm, unsigned long vaddr)
{
  /* PGD - Page Global Directory
   * +-> PUD - Page Upper Directory
   *     +-> PMD - Page Middle Directory
   *         +-> PTE - Page Table Entry
   */
  pgd_t *pgd;
  pud_t *pud;
  pmd_t *pmd;
  pte_t *pte;
  struct page *pg;
  phys_addr_t pga, pa = 0;

  pgd = pgd_offset(mm, vaddr);
  if (pgd_none(*pgd) || pgd_bad(*pgd)) {
    printk("[kmemhack]: get_pa: pgd error");
    goto out;
  }

  pud = pud_offset(pgd, vaddr);
  if (pud_none(*pud) || pud_bad(*pud)) {
    printk("[kmemhack]: get_pa: pud error");
    goto out;
  }

  pmd = pmd_offset(pud, vaddr);
  if (pmd_none(*pmd) || pmd_bad(*pmd)) {
    printk("[kmemhack]: get_pa: pmd error");
    goto out;
  }
  
  pte = pte_offset_map(pmd, vaddr);
  if (pte_none(*pte)) {
    printk("[kmemhack]: get_pa: pte error");
    goto out;
  }

  pg = pte_page(*pte);
  pte_unmap(pte);

  /* physical page address */
  pga = page_to_phys(pg);
  /* physical address */
  pa = pga | (vaddr & ~PAGE_MASK);
out:
  return pa;
}

static int driver_open(struct inode *node, struct file *file)
{
  return 0;
}

static int driver_close(struct inode *node, struct file *file)
{
  return 0;
}

static struct file_operations fops = {
  .owner          = THIS_MODULE,
  .open           = driver_open,
  .release        = driver_close,
  .unlocked_ioctl = driver_ioctl,
};

static struct miscdevice misc = {
  .name  = DEVICE_NAME,
  .minor = MISC_DYNAMIC_MINOR,
  .fops  = &fops,
};

static int __init driver_entry(void)
{
  int ret;

  printk("[kmemhack]: driver_init");
  ret = misc_register(&misc);
  if (ret) {
    printk("[kmemhack]: misc_register failed: %d", ret);
    return ret;
  }
  return 0;
}

static void __exit driver_exit(void)
{
  printk("[kmemhack]: driver_exit");
  misc_deregister(&misc);
}

module_init(driver_entry);
module_exit(driver_exit);

MODULE_DESCRIPTION("No description");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("horoni");
