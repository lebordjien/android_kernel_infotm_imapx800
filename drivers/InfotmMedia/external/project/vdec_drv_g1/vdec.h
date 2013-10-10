/*
 * vdec.h
 *
 * Copyright (c) 2009~2014 ShangHai Infotm Ltd all rights reserved.
 *
 * Use of Infotm's code is governed by terms and conditions
 * stated in the accompanying licensing statement.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Author:
 *	ayakashi<eirc.xu@infotmic.com.cn>.
 *
 * Revision History:
 * 1.0  05/15/2012 ayakashi.
 * 	Create this file.
 */

#ifndef __VDEC_H__
#define __VDEC_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/mman.h>

#include <linux/semaphore.h>

#include <asm/io.h>
#include <asm/atomic.h>
#if (TS_VER_MAJOR == 2 && TS_VER_MINOR == 6)
#include <plat/imapx.h>
#endif 

#include <InfotmMedia.h>
#include <IM_buffallocapi.h>
#include <IM_devmmuapi.h>
#include <pmm_lib.h>

#define VDEC_DEFAULT_MAJOR		100
#define VDEC_DEFAULT_MINOR		101
#define VDEC_MODULE_NAME "vdec_drv_g1"
#define VDEC_RET_OK		0
#define VDEC_RET_ERROR	-1000


/*
 * macros about irq
 * irq register location and someothers
 */
#define VDEC_IRQ_STAT_DEC	1
#define VDEC_IRQ_STAT_PP	60

#define VDEC_IRQ_BIT_DEC	0x100
#define VDEC_IRQ_BIT_PP		0x100

#if (TS_VER_MAJOR == 3 && TS_VER_MINOR == 0)
#define VDEC_BASE_REG_PA 		(0x25000000)
#define VDEC_MEMPOOL_BASE_PA 	(0x21E00000)
#define VDEC_MEMPOOL_SIZE 		(0x100000)
#define IRQ_VDEC          		(128) /*Infotm video decoder interrupt */
#endif 

/*
 * there reserved 4KB for
 * Decode register, but actually only first 404 bytes used, 
 * 4KB is a page size, so reserve 4KB much larger than vdec
 * needs does make sense
 */
#define VDEC_ACT_REG_SIZE	(101 * 4)

/*
 * ioctl commands
 */
#define VDEC_MAGIC		   'k'
#define VDEC_MAX_CMD		14

#define VDEC_PP_INSTANCE        _IO(VDEC_MAGIC, 1)
#define VDEC_HW_PERFORMANCE     _IO(VDEC_MAGIC, 2)
#define VDEC_REG_BASE           _IOR(VDEC_MAGIC, 3, unsigned long *)
#define VDEC_REG_SIZE           _IOR(VDEC_MAGIC, 4, unsigned int *)
#define VDEC_IRQ_DISABLE        _IO(VDEC_MAGIC, 5)
#define VDEC_IRQ_ENABLE         _IO(VDEC_MAGIC, 6)
#define VDEC_INSTANCE_LOCK      _IOW(VDEC_MAGIC, 7, int)
#define VDEC_INSTANCE_UNLOCK    _IO(VDEC_MAGIC, 8)
#define VDEC_CHECK_MMU_INIT     _IOR(VDEC_MAGIC, 9, unsigned int *)
#define VDEC_NOTIFY_HW_ENABLE   _IO(VDEC_MAGIC, 10)
#define VDEC_NOTIFY_HW_DISABLE  _IO(VDEC_MAGIC, 11)
#define VDEC_RESET              _IO(VDEC_MAGIC, 12)
#define VDEC_UPDATE_CLK         _IO(VDEC_MAGIC, 13)
#define VDEC_GETDVFS            _IOR(VDEC_MAGIC, 14,unsigned int *)

#define VDEC_MMU_ON  1
#define VDEC_MMU_OFF 0
#define VDEC_DVFS_ON  1
#define VDEC_DVFS_OFF 0

#define DVFS_ENABLE_ITEM	"dvfs.enable"
#define DVFS_VPU_ENABLE_ITEM	"dvfs.vpu.enable"


/*
 * global variables
 */
typedef struct 
{
	struct resource         *resource_mem;
	void __iomem            *reg_base_virt_addr;
	void __iomem            *mempool_virt_addr;
	void __iomem            *mmu_virt_addr;
	unsigned int            reg_base_phys_addr;
	unsigned int            reg_reserved_size;
#ifdef CONFIG_VDEC_SIGNAL_MODE
	struct fasync_struct    *async_queue_dec;
	struct fasync_struct    *async_queue_pp;
#endif	/* CONFIG_IMAP_VDEC_SIGNAL_MODE */
	unsigned int            dec_instance;
	unsigned int            pp_instance;
	unsigned int            wait_mmu_instance;
#ifdef CONFIG_VDEC_HW_PERFORMANCE
	struct timeval          end_time;
#endif
	struct semaphore        dec_sema;
	struct semaphore        pp_sema;
	struct semaphore        mmu_sema;

	pmm_handle_t            pmm_handle;  
	int                     mmu_state;
	int                     mmu_inited;
	int                     dec_working;
	int                     wait_mmu;
	
	struct clk *	busClock;
	int		dvfsEna;
}vdec_param_t;

#endif  /* __VDEC_H__ */
