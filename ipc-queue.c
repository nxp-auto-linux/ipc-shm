/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018-2019,2022 NXP
 */
#include "ipc-os.h"
#include "ipc-queue.h"

/**
 * ipc_queue_pop() - removes element from queue
 * @queue:	[IN] queue pointer
 * @buf:	[OUT] pointer where to copy the removed element
 *
 * Element is removed from pop ring that is mapped in remote shared memory and
 * it corresponds to the remote push ring.
 *
 * Return:	0 on success, error code otherwise
 */
int ipc_queue_pop(struct ipc_queue *queue, void *buf)
{
	uint32_t write; /* cache write index for thread-safety */
	uint32_t read; /* cache read index for thread-safety */
	void *src;

	if ((queue == NULL) || (buf == NULL)) {
		return -EINVAL;
	}

	write = queue->pop_ring->write;

	/* read indexes of push/pop rings are swapped (interference freedom) */
	read = queue->push_ring->read;

	/* check if queue is empty */
	if (read == write) {
		return -ENOBUFS;
	}

	/* copy queue element in buffer */
	src = &queue->pop_ring->data[read * queue->elem_size];
	(void) memcpy(buf, src, queue->elem_size);

	/* increment read index with wrap around */
	queue->push_ring->read = (read + 1u) % queue->elem_num;

	return 0;
}

/**
 * ipc_queue_push() - pushes element into the queue
 * @queue:	[IN] queue pointer
 * @buf:	[IN] pointer to element to be pushed into the queue
 *
 * Element is pushed into the push ring that is mapped in local shared memory
 * and corresponds to the remote pop ring.
 *
 * Return:	0 on success, error code otherwise
 */
int ipc_queue_push(struct ipc_queue *queue, const void *buf)
{
	uint32_t write; /* cache write index for thread-safety */
	uint32_t read; /* cache read index for thread-safety */
	void *dst;

	if ((queue == NULL) || (buf == NULL)) {
		return -EINVAL;
	}

	write = queue->push_ring->write;

	/* read indexes of push/pop rings are swapped (interference freedom) */
	read = queue->pop_ring->read;

	/* check if queue is full ([write + 1 == read] because of sentinel) */
	if (((write + 1u) % queue->elem_num) == read) {
		return -ENOMEM;
	}

	/* copy element from buffer in queue */
	dst = &queue->push_ring->data[write * queue->elem_size];
	(void) memcpy(dst, buf, queue->elem_size);

	/* increment write index with wrap around */
	queue->push_ring->write = (write + 1u) % queue->elem_num;

	return 0;
}

/**
 * ipc_queue_init() - initializes queue and maps push/pop rings in memory
 * @queue:		[IN] queue pointer
 * @elem_num:		[IN] number of elements in queue
 * @elem_size:		[IN] element size in bytes (8-byte multiple)
 * @push_ring_addr:	[IN] local addr where to map the push buffer ring
 * @pop_ring_addr:	[IN] remote addr where to map the pop buffer ring
 *
 * Element size must be 8-byte multiple to ensure memory alignment.
 *
 * Queue will add one additional sentinel element to its size for lock-free
 * single-producer - single-consumer thread-safety.
 *
 * Return: 0 on success, error code otherwise
 */
int ipc_queue_init(struct ipc_queue *queue,
		   uint16_t elem_num, uint16_t elem_size,
		   uintptr_t push_ring_addr, uintptr_t pop_ring_addr)
{
	if ((queue == NULL) || (push_ring_addr == (uintptr_t)NULL) ||
		(pop_ring_addr == (uintptr_t)NULL) || (elem_num == 0u) ||
		(elem_size == 0u) || ((elem_size % 8u) != 0u)) {
		return -EINVAL;
	}

	/* add 1 sentinel element in queue for lock-free thread-safety */
	queue->elem_num = elem_num + 1u;

	queue->elem_size = elem_size;

	/* map and init push ring in local memory */
	queue->push_ring = (struct ipc_ring *) push_ring_addr;

	queue->push_ring->write = 0;
	queue->push_ring->read = 0;

	/* map pop ring in remote memory (init is done by remote) */
	queue->pop_ring = (struct ipc_ring *) pop_ring_addr;

	return 0;
}
