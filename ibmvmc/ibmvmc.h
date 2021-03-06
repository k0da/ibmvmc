/*
 * IBM Power Systems Virtual Management Channel Support.
 *
 * Copyright (c) 2004, 2015 IBM Corp.
 *   Dave Engebretsen engebret@us.ibm.com
 *   Steven Royer seroyer@linux.vnet.ibm.com
 *   Adam Reznechek adreznec@linux.vnet.ibm.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#ifndef IBMVMC_H
#define IBMVMC_H

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/cdev.h>

#include <asm/vio.h>

#define IBMVMC_PROTOCOL_VERSION    0x0101

#define LOG_LEVEL_MIN     1
#define LOG_LEVEL_NORM    3
#define LOG_LEVEL_TRACE   7
#define LOG_LEVEL_MAX     10
#define DEFAULT_LOG_LEVEL LOG_LEVEL_NORM

#define MIN_BUF_POOL_SIZE 16
#define MIN_HMCS          1
#define MIN_MTU           4096
#define MAX_BUF_POOL_SIZE 64
#define MAX_HMCS          2
#define MAX_MTU           (4*4096)
#define DEFAULT_BUF_POOL_SIZE 32
#define DEFAULT_HMCS          1
#define DEFAULT_MTU           4096
#define HMC_ID_LEN        32

#define VMC_INVALID_BUFFER_ID 0xFFFF

#define VMC_NUM_MINORS	1

/* ioctl numbers */
#define VMC_BASE	     0xCC
#define VMC_IOCTL_SETHMCID   _IOW(VMC_BASE, 0x00, unsigned char *)
#define VMC_IOCTL_QUERY      _IOR(VMC_BASE, 0x01, struct ibmvmc_query_struct)
#define VMC_IOCTL_REQUESTVMC _IOR(VMC_BASE, 0x02, u32)

#define VMC_MSG_CAP          0x01
#define VMC_MSG_CAP_RESP     0x81
#define VMC_MSG_OPEN         0x02
#define VMC_MSG_OPEN_RESP    0x82
#define VMC_MSG_CLOSE        0x03
#define VMC_MSG_CLOSE_RESP   0x83
#define VMC_MSG_ADD_BUF      0x04
#define VMC_MSG_ADD_BUF_RESP 0x84
#define VMC_MSG_REM_BUF      0x05
#define VMC_MSG_REM_BUF_RESP 0x85
#define VMC_MSG_SIGNAL       0x06

#define VMC_MSG_SUCCESS 0
#define VMC_MSG_INVALID_HMC_INDEX 1
#define VMC_MSG_INVALID_BUFFER_ID 2
#define VMC_MSG_CLOSED_HMC        3
#define VMC_MSG_INTERFACE_FAILURE 4
#define VMC_MSG_NO_BUFFER         5

#define VMC_BUF_OWNER_ALPHA 0
#define VMC_BUF_OWNER_HV    1

/* TODO(adreznec) Remove when H_REQUEST_VMC added to hvcall.h */
#define H_REQUEST_VMC           0x360

enum ibmvmc_states {
	ibmvmc_state_initial      = 0,
	ibmvmc_state_crqinit      = 1,
	ibmvmc_state_capabilities = 2,
	ibmvmc_state_ready        = 3,
	ibmvmc_state_failed       = 4,
};

enum ibmhmc_states {
	/* HMC connection not established */
	ibmhmc_state_free    = 0,

	/* HMC connection established (open called) */
	ibmhmc_state_initial = 1,

	/* open msg sent to HV, due to ioctl(1) call */
	ibmhmc_state_opening = 2,

	/* HMC connection ready, open resp msg from HV */
	ibmhmc_state_ready   = 3,

	/* HMC connection failure */
	ibmhmc_state_failed  = 4,
};

struct ibmvmc_buffer {
	u8 valid;    /* 1 when DMA storage allocated to buffer          */
	u8 free;     /* 1 when buffer available for the Alpha Partition */
	u8 owner;
	u16 id;
	u32 size;
	u32 msg_len;
	dma_addr_t dma_addr_local;
	dma_addr_t dma_addr_remote;
	void *real_addr_local;
};

struct crq_msg_ibmvmc_admin {
	u8 valid;     /* RPA Defined           */
	u8 type;      /* ibmvmc msg type       */
	u8 status;    /* Response msg status   */
	u8 rsvd[2];
	u8 max_hmc;
	__be16 pool_size;
	__be32 max_mtu;
	__be16 crq_size;
	__be16 version;
};

struct crq_msg_ibmvmc {
	u8 valid;     /* RPA Defined           */
	u8 type;      /* ibmvmc msg type       */
	u8 status;    /* Response msg status   */
	union {
		u8 rsvd;  /* Reserved              */
		u8 owner;
	} var1;
	u8 hmc_session;
	u8 hmc_index;
	union {
		__be16 rsvd;
		__be16 buffer_id;
	} var2;
	__be32 rsvd;
	union {
		__be32 rsvd;
		__be32 lioba;
		__be32 msg_len;
	} var3;
};

/* an RPA command/response transport queue */
struct crq_queue {
	struct crq_msg_ibmvmc *msgs;
	int size, cur;
	dma_addr_t msg_token;
	spinlock_t lock;
};

/* VMC server adapter settings */
struct crq_server_adapter {
	char name[16];
	struct device *dev;
	struct crq_queue queue;
	u32 liobn;
	u32 riobn;
	struct work_struct work;
	struct workqueue_struct *work_queue;
};

/* Driver wide settings */
struct ibmvmc_struct {
	u32 state;
	u32 max_mtu;
	u32 max_buffer_pool_size;
	u32 max_hmc_index;
	struct crq_server_adapter *adapter;
	struct cdev cdev;
	u32 vmc_drc_index;
};

struct ibmvmc_file_session;

/* Connection specific settings */
struct ibmvmc_hmc {
	u8 session;
	u8 index;
	u32 state;
	struct crq_server_adapter *adapter;
	spinlock_t lock;
	unsigned char hmc_id[HMC_ID_LEN];
	struct ibmvmc_buffer buffer[MAX_BUF_POOL_SIZE];
	unsigned short queue_outbound_msgs[MAX_BUF_POOL_SIZE];
	int queue_head, queue_tail;
	struct ibmvmc_file_session *pSession;
};

struct ibmvmc_file_session {
	struct file *file;
	struct ibmvmc_hmc *hmc;
	bool valid;
};

struct ibmvmc_query_struct {
	int have_vmc;
	int state;
	int vmc_drc_index;
};

#endif
