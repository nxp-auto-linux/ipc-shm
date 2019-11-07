/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018-2019 NXP
 */
#ifndef IPC_QUEUE_H
#define IPC_QUEUE_H

/**
 * struct ipc_ring - memory mapped circular buffer ring
 * @write:	write index, position used to store next byte in the buffer
 * @read:	read index, read next byte from this position
 * @data:	circular buffer
 */
struct ipc_ring {
	volatile uint32_t write;
	volatile uint32_t read;
	uint8_t data[];
};

/**
 * struct ipc_queue - Dual-Ring Shared-Memory Lock-Free FIFO Queue
 * @elem_num:	number of elements in queue
 * @elem_size:  element size in bytes (8-byte multiple)
 * @push_ring:	push buffer ring mapped in local shared memory
 * @pop_ring:	pop buffer ring mapped in remote shared memory
 *
 * This queue has two buffer rings one for pushing data and one for popping
 * data and works in conjunction with a complementary queue configured by
 * another IPC device (called remote) where the push/pop rings are reversed:
 *     local push_ring == remote pop_ring
 *     local pop_ring == remote push_ring
 *
 * The queue has freedom from interference between local and remote memory
 * domains by executing all write operations only in local memory (push_ring).
 * Read indexes of push_ring and pop_ring are swapped to avoid writing read
 * index in remote memory when doing pop operations.
 *
 * The queue is thread safe as long as only one thread is pushing and only one
 * thread is popping: Single-Producer - Single-Consumer. This thread safety
 * is lock-free and needs one additional sentinel element in rings between
 * write and read index that is never written.
 */
struct ipc_queue {
	uint16_t elem_num;
	uint16_t elem_size;
	struct ipc_ring *push_ring;
	struct ipc_ring *pop_ring;
};

int ipc_queue_init(struct ipc_queue *queue, uint16_t elem_num,
	uint16_t elem_size, uintptr_t push_ring_addr, uintptr_t pop_ring_addr);
int ipc_queue_push(struct ipc_queue *queue, const void *buf);
int ipc_queue_pop(struct ipc_queue *queue, void *buf);

/**
 * ipc_queue_mem_size() - return queue footprint in local mapped memory
 * @queue:	[IN] queue pointer
 *
 * Return local mapped memory footprint: local ring control data + ring buffer.
 *
 * Return:	size of local mapped memory occupied by queue
 */
static inline uint32_t ipc_queue_mem_size(struct ipc_queue *queue)
{
	/* local ring control room + ring size */
	return (uint32_t)sizeof(struct ipc_ring)
		+ ((uint32_t)queue->elem_num * (uint32_t)queue->elem_size);
}

#endif /* IPC_QUEUE_H */
