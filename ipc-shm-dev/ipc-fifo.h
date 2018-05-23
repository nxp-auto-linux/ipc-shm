/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2018 NXP Semiconductors
 */
#ifndef IPC_FIFO_H_
#define IPC_FIFO_H_

/**
 * struct ipcf_fifo - Memory Mapped Lock-Free Circular Queue
 * (Single-Producer - Single-Consumer)
 *
 * @size:	size of the queue
 * @w:		write index, position used to store next byte in the buffer
 * @r:		read index, read next byte from this position
 * @buf_addr:	circular buffer address
 *
 * The queue is thread safe as long as only one thread is pushing and only one
 * thread is popping: Single-Producer - Single-Consumer
 */
struct ipc_fifo {
	uint32_t size;
	volatile uint16_t w;
	volatile uint16_t r;
	uint64_t buf_addr;
};

struct ipc_fifo *ipc_fifo_init(void *base_addr, uint16_t buf_size);
uint16_t ipc_fifo_push(struct ipc_fifo *f, const void *buf, uint16_t nbytes);
uint16_t ipc_fifo_pop(struct ipc_fifo *f, void *buf, uint16_t nbytes);
uint16_t ipc_fifo_discard(struct ipc_fifo *f, uint16_t nbytes);

#endif /* IPC_FIFO_H_ */
