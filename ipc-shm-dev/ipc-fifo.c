/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (C) 2018 NXP Semiconductors
 */
#include <linux/module.h>
#include "ipc-fifo.h"

/** increment() - increment queue index and wrap around when size is exceeded */
static inline uint16_t increment(uint16_t idx, uint16_t step, uint16_t size)
{
	return (idx + step) % size;
}

/** get_count() - get number of elements in queue */
static inline uint16_t get_count(uint16_t size, uint16_t w, uint16_t r)
{
	return w < r ? size - (r - w) : w - r;
}

/** get_free() - get number of free elements in queue */
static inline uint16_t get_free(uint16_t size, uint16_t w, uint16_t r)
{
	return size - get_count(size, w, r) - 1;
}

/**
 * fifo_pop() - removes data from queue
 * @f:		[IN] queue pointer
 * @buf:	[IN] target buffer to pop the data to
 * @nbytes:	[IN] number of bytes to pop from the queue
 * Return:	number of bytes successfully removed from the queue
 */
uint16_t ipc_fifo_pop(struct ipc_fifo *f, void *buf, uint16_t nbytes)
{
	uint16_t n = 0;
	uint16_t partial_len = 0;
	uint8_t *tmp = (uint8_t *) buf;
	uint16_t w = 0; /* caches the write index */
	uint16_t r = 0; /* caches the read index */


	if ((f == NULL) || (buf == NULL) || (nbytes == 0) || (f->buf == NULL)) {
		goto out;
	}

	/* caching is needed because of multithreading */
	w = f->w;
	r = f->r;

	/* pop minimum between input parameter and number of bytes
	 * present in the queue
	 */
	n = min(nbytes, get_count(f->size, w, r));

	/*|x| = used, | | = free, r = read index, w = write index
	 * buffer index =>  0 1 2 3 4 5 6 7 8 9
	 * r, w indices =>        r       w
	 * ring buffer  => | | | |x|x|x|x| | | |
	 */
	if (r < w || n <= f->size - r) {
		/* r < w or no roll over*/
		memcpy(tmp, &f->buf[r], n);
	} else {
		/*|x| = used, | | = free, r = read index, w = write index
		 * buffer index =>  0 1 2 3 4 5 6 7 8 9
		 * r, w indices =>        w       r
		 * ring buffer  => |x|x|x| | | | |x|x|x|
		 */

		/* copy with roll over */
		/* 1. copy from the read index to end of buffer */
		partial_len = f->size - r;
		memcpy(tmp, &f->buf[r], partial_len);

		/* 2. copy remaining bytes from the buffer start */
		memcpy(&tmp[partial_len], f->buf, n - partial_len);
	}

	f->r = increment(r, n, f->size);

out:
	return n;
}

/**
 * fifo_discard() - discard bytes from queue
 * @f:		[IN] queue pointer
 * @nbytes:	[IN] number of bytes to discard from fifo
 *
 * Return:	number of bytes successfully discarded from the queue
 */
uint16_t ipc_fifo_discard(struct ipc_fifo *f, uint16_t nbytes)
{
	int len = 0;
	uint16_t r = 0;

	if ((f == NULL) || (nbytes == 0)) {
		goto out;
	}

	r = f->r; /* caching is needed because of multithreading */

	/* discard nbytes but no more than existing bytes in queue */
	len = min(nbytes, get_count(f->size, f->w, r));
	f->r = increment(f->r, len, f->size);

out:
	return len;
}

/**
 * ipc_fifo_push() - pushes input data into the queue
 * @f:		[IN] queue pointer
 * @buf:	[IN] pointer to buffer to be pushed into the queue
 * @nbytes	[IN] number of bytes from buffer to be pushed into the queue
 * Return:	the number of bytes successfully pushed into the queue
 *
 * The caller must check that the returned value is equal to the number of
 * elements intended to be pushed into the queue (nbytes)
 */
uint16_t ipc_fifo_push(struct ipc_fifo *f, const void *buf, uint16_t nbytes)
{
	uint16_t n = 0;
	uint16_t partial_len = 0;
	uint8_t *tmp = NULL;
	uint16_t w = 0; /* caches the write index */
	uint16_t r = 0; /* caches the read index */

	if ((f == NULL) || (buf == NULL) || (nbytes == 0) || (f->buf == NULL)) {
		goto out;
	}

	/* caching is needed because of multithreading */
	w = f->w;
	r = f->r;

	/* check if enough space */
	if (get_free(f->size, w, r) < nbytes)
		goto out;

	tmp = (uint8_t *) buf;

	/*|x| = used, | | = free, r = read index, w = write index
	 * buffer index =>  0 1 2 3 4 5 6 7 8 9
	 * r, w indices =>        w       r
	 * ring buffer  => |x|x|x| | | | |x|x|x|
	 */
	if (r > w || nbytes <= f->size - w) {
		memcpy(&f->buf[w], tmp, nbytes);
	} else {

		/*|x| = used, | | = free, r = read index, w = write index
		 * buffer index =>  0 1 2 3 4 5 6 7 8 9
		 * r, w indices =>        r       w
		 * ring buffer  => | | | |x|x|x|x| | | |
		 */

		/* copy with roll over */
		/* 1. copy from the write index to end of buffer */
		partial_len = f->size - w;
		memcpy(&f->buf[w], tmp, partial_len);

		/* 2. copy remaining bytes from the buffer start */
		memcpy(f->buf, &tmp[partial_len], nbytes - partial_len);
	}

	n = nbytes; /* number of pushed elements */
	f->w = increment(w, nbytes, f->size);

out:
	return n;
}

/**
 * ipc_fifo_init() - initializes the queue (head, tail, size, count, buffer
 * @f:		[IN] queue pointer
 * @buf:	[IN] buffer pointer to be managed by this queue
 * @size:	[IN] the length of the buffer to be managed
 * Return:	0 for success, non zero otherwise
 */
int ipc_fifo_init(struct ipc_fifo *f, uint8_t *buf, uint16_t size)
{
	int16_t status = -1;

	if (f != NULL && buf != NULL && size != 0) {
		f->buf = buf;
		f->size = size;
		f->r = 0;
		f->w = 0;
		status = 0;
	}
	return status;
}
