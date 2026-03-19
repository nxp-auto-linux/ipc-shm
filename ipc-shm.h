/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright 2018-2025 NXP
 */
#ifndef IPC_SHM_H
#define IPC_SHM_H

#include "ipc-types.h"
#include "ipc-os.h"


/**
 * ipc_shm_init_instance() - Initialize the specified instance
 *                           of the IPC-Shm driver
 *
 * @instance: instance id
 * @cfg:      ipc-shm instance configuration
 *
 * Return:
 * * %0        - On success (all instances were initialized with success or have
 *   been previously initialized)
 * * %-EINVAL  - Invalid instance configuration or instance is initialized
 * * %-ENOMEM  - Buffer/pool doesn't fit into shared memory or channel/buffer
 *   pool queue is full
 * * %-EAGAIN  - Remote initialization is in progress
 */
int8_t ipc_shm_init_instance(uint8_t instance);

/**
 * ipc_shm_init() - initialize shared memory device
 * @cfg:              configuration parameters
 *
 * Function is non-reentrant.
 *
 * Return:
 * * %0        - On success (all instances were initialized with success or have
 *   been previously initialized)
 * * %-EINVAL  - Invalid instance configuration or instance is initialized
 * * %-ENOMEM  - Buffer/pool doesn't fit into shared memory or channel/buffer
 *   pool queue is full
 * * %-EAGAIN  - Remote initialization is in progress
 */
int8_t ipc_shm_init(const struct ipc_shm_instances_cfg *cfg);

/**
 * ipc_shm_free_instance() - Deinitialize the specified instance
 *                           of the IPC-Shm driver
 *
 * @instance: instance id
 *
 * Function is non-reentrant.
 */
void ipc_shm_free_instance(const uint8_t instance);

/**
 * ipc_shm_free() - release all instances of shared memory device
 *
 * Function is non-reentrant.
 */
void ipc_shm_free(void);

/**
 * ipc_shm_acquire_buf() - request a buffer for the given channel
 * @instance:       instance id
 * @chan_id:        channel index
 * @mem_size:       required size
 *
 * Function used only for managed channels where buffer management is enabled.
 * Function is thread-safe for different channels but not for the same channel.
 *
 * Return:
 * * %NULL                                - Buffer not found
 * * %Pointer to the buffer base address  - On success
 */
void *ipc_shm_acquire_buf(const uint8_t instance, uint8_t chan_id,
				uint32_t mem_size);

/**
 * ipc_shm_release_buf() - release a buffer for the given channel
 * @instance:       instance id
 * @chan_id:        channel index
 * @buf:            buffer pointer
 *
 * Function used only for managed channels where buffer management is enabled.
 * Function is thread-safe for different channels but not for the same channel.
 *
 * * %0               - On success
 * * %-EINVAL         - Invalid instance, channel or buffer
 * * %-ENOMEM         - Cannot release buffer
 * * %-EADDRNOTAVAIL  - Buffer is released to the pool and cannot release again
 */
int8_t ipc_shm_release_buf(const uint8_t instance, uint8_t chan_id,
				const void *buf);

/**
 * ipc_shm_tx() - send data on given channel and notify remote
 * @instance:       instance id
 * @chan_id:        channel index
 * @buf:            buffer pointer
 * @size:           size of data written in buffer
 *
 * Function used only for managed channels where buffer management is enabled.
 * Function is thread-safe for different channels but not for the same channel.
 *
 * * %0              - On success
 * * %-EINVAL        - Invalid instance, channel or buffer
 * * %-EADDRNOTAVAIL - Buffer is transmitted without being acquired
 */
int8_t ipc_shm_tx(const uint8_t instance, int chan_id, void *buf,
			uint32_t size);

/**
 * ipc_shm_unmanaged_acquire() - acquire the unmanaged channel local memory
 * @instance:       instance id
 * @chan_id:        channel index
 *
 * Function used only for unmanaged channels. The memory must be acquired only
 * once after the channel is initialized. There is no release function needed.
 * Function is thread-safe for different channels but not for the same channel.
 *
 * Return:
 * * %NULL                           - Invalid channel
 * * %Pointer to the channel memory  - On success
 */
void *ipc_shm_unmanaged_acquire(const uint8_t instance, uint8_t chan_id);

/**
 * ipc_shm_acquire_scmi() - acquire the scmi channel local memory
 * @instance:       instance id
 * @chan_id:        channel index
 *
 * Function used only for scmi channels. The memory must be acquired only
 * once after the channel is initialized.
 * Function is thread-safe for different channels but not for the same channel.
 *
 * Return:
 * * %Pointer to the channel memory - On success
 * * %NULL                          - Invalid channel
 */
void *ipc_shm_acquire_scmi(const uint8_t instance, uint8_t chan_id);

/**
 * ipc_shm_unmanaged_tx() - notify remote that data has been written in channel
 * @instance:       instance id
 * @chan_id:        channel index
 *
 * Function used only for unmanaged channels. It can be used after the channel
 * memory has been acquired whenever is needed to signal remote that new data
 * is available in channel memory.
 * Function is thread-safe for different channels but not for the same channel.
 *
 * Return:
 * * %0        - On success
 * * %-EINVAL  - Invalid instance, channel or buffer
 */
int8_t ipc_shm_unmanaged_tx(const uint8_t instance, uint8_t chan_id);

/**
 * ipc_shm_scmi_tx() - notify remote that data has been written in channel
 * @instance:       instance id
 * @chan_id:        channel index
 *
 * Function used only for scmi channels. It can be used after the channel
 * memory has been acquired whenever is needed to signal remote that new data
 * is available in channel memory.
 * Function is thread-safe for different channels but not for the same channel.
 *
 * Return:
 * * %0        - On success
 * * %-EINVAL  - Invalid channel or SCMI address is NULL
 */
int8_t ipc_shm_scmi_tx(const uint8_t instance, uint8_t chan_id);

/**
 * ipc_shm_is_remote_ready() - check whether remote is initialized
 * @instance:        instance id
 *
 * Function used to check if the remote is initialized and ready to receive
 * messages. It should be invoked at least before the first transmit operation.
 * Function is thread-safe.
 *
 * Return:
 * * %0        - Remote is ready and valid instance state
 * * %-EINVAL  - Invalid instance state
 * * %-EAGAIN  - Remote is not ready
 */
int8_t ipc_shm_is_remote_ready(const uint8_t instance);

/**
 * ipc_shm_poll_channels() - poll the channels for available messages to process
 * @instance:        instance id
 *
 * This function handles all channels using a fair handling algorithm: all
 * channels are treated equally and no channel is starving.
 * Function is thread-safe for different instances but not for same instance.
 *
 * Return:
 * * %Number of messages processed  - On success
 * * %-EINVAL                       - Invalid instance
 * * %-EOPNOTSUPP                   - Instance is not configuring for polling
 * * %-EAGAIN                       - Remote is not ready
 */
int8_t ipc_shm_poll_channels(const uint8_t instance);

/**
 * ipc_shm_free_scmi_channel() - free the scmi channel
 * @instance:        instance id
 * @chan_id:         channel index
 *
 * This function releases the scmi channel. The caller should release the
 * channel before populating the information in the shared memory area or can
 * be used in case of a timeout.
 * Function is thread-safe for different instances but not for same instance.
 *
 * Return:
 * * %0            - Free channel successfully
 * * %-EINVAL      - Invalid channel
 */
int8_t ipc_shm_free_scmi_channel(const uint8_t instance, const uint8_t chan_id);

/**
 * ipc_shm_is_channel_not_empty() - check if a channel has not processed
 *                                  messages
 * @instance:       instance id
 * @chan_id:        channel index
 * @not_empty:      [OUT] pointer that indicates if the channel is empty or not
 *                  TRUE   if channel has non processed messages
 *                  FALSE  if the channel has no messages
 *
 * Return:
 * * %0        - Check channel state succuessfully
 * * %-EINVAL  - Invalid instance, invalid channel or channel queue is not empty
 */
int8_t ipc_shm_is_channel_not_empty(const uint8_t instance, uint8_t chan_id,
			bool *not_empty);

/**
 * ipc_shm_get_message_from_channel() - retrieve a message from a channel to
 *                                      msg_buffer
 *
 * If the channel is managed, get the message from the first queue
 * If the channel is unmanaged, read from the shm directly
 * If the channel is scmi, get the message from the associated queue
 * The messages wil be mark as release/free, because will be copy in msg_buffer.
 *
 * @instance:   [IN] instance id
 * @chan_id:    [IN] channel id
 * @msg_buffer: [OUT] pointer to output message buffer
 *
 * Return:
 * * %0        - Get message succuessfully
 * * %-EINVAL  - Invalid instance or invalid channel
 */
int8_t ipc_shm_get_message_from_channel(uint8_t instance, uint8_t chan_id,
			void **msg_buffer);

/**
 * ipc_shm_get_channel_type() - retrieve the channel type for a channel
 * @instance:      [IN] instance id
 * @chan_id:       [IN] channel id
 * @chan_type:     [OUT] pointer to output the channel type
 *
 * Return:
 * * %0        - Get channel succuessfully
 * * %-EINVAL  - Invalid instance or invalid channel
 */
int8_t ipc_shm_get_channel_type(const uint8_t instance, uint8_t chan_id,
			enum ipc_shm_channel_type *chan_type);

#endif /* IPC_SHM_H */
