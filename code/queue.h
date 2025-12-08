#ifndef _QUEUE_H_
#define _QUEUE_H_

#include "types.h"

typedef struct queue queue;
struct queue {
    void* data;
    u32   len;
    u32   idx_start;
    u32   idx_front;
    u32   capacity;
    u32   data_size;
};

/**
 * @author Sascha Paez
 * @brief  Returns a queue struct initialized with the data passed
 * 
 * @param buffer    Buffer memory handled and initiaized by the user
 * @param len       Starting length of the vector
 * @param capacity  The maximum capacity of the vector 
 * @param data_size The size of the type we want to handle in the vector
 * 
 * @return queue   Initialized queue   
 * @note For sake for user manageability, the buffer will be initialized and destroyed by them
 */
fn_internal queue QueueInit(void* buffer, u32 len, u32 capacity, u32 data_size);

/**
 * @author Sascha Paez
 * @brief  Resets the struct passed by the user
 * 
 * @note For sake for user manageability, the buffer will never be freed by the function
 */
fn_internal void QueueClear(queue* q);

/**
 * @author Sascha Paez
 * @brief  Pushes the new value (by copy) to the struct passed by the user
 * 
 * @note For sake for user manageability, the buffer will never be freed by the function
 * @return Pointer to the value stored in the queue, NULL if it could not be stored
 */
fn_internal void* QueuePush(queue* q, void* data);

/**
 * @author Sascha Paez
 * @brief  Pops the front value (by pointer)
 * 
 * @note For sake for user manageability, the buffer will never be freed by the function
 */
fn_internal void* QueuePopRaw(queue* q);

/**
 * @author Sascha Paez
 * @brief  Pops the front value (by pointer) and casts it to the type pointed by the user
 * 
 * @note For sake for user manageability, the buffer will never be freed by the function
 */
#define QueuePop(q, type) ( (type*)QueuePopRaw(q) ) 

/**
 * @author Sascha Paez
 * @brief  Returns the front value (by pointer)
 * 
 * @note For sake for user manageability, the buffer will never be freed by the function
 */
fn_internal void* QueueGetFrontRaw(queue* q);

/**
 * @author Sascha Paez
 * @brief  Returns the front value (by pointer) and casts it to the type pointed by the user
 * 
 * @note For sake for user manageability, the buffer will never be freed by the function
 */
#define QueueGetFront(q, type) ( (type*)QueueGetFrontRaw(q) ) 

#endif 

#ifdef QUEUE_IMPL

fn_internal queue 
QueueInit(void* buffer, u32 len, u32 capacity, u32 data_size) {
	queue q = {};
	q.data      = buffer;
    q.len       = len;
    q.idx_start = 0;
    q.idx_front = 0;
    q.capacity  = capacity;
    q.data_size = data_size;


    return q;
}

fn_internal void 
QueueClear(queue* q) {
    q->len = 0;
    q->idx_front = 0;
}

fn_internal void* 
QueuePush(queue* q, void* data) {
    if( q->len == q->capacity ) { return NULL; }

    q->idx_front = (q->idx_front + 1) % q->capacity;
    
    u8* current_offset = (u8*)q->data + (q->idx_front * q->data_size);
    memcpy(current_offset, data, q->data_size);

    q->len += 1;

    return current_offset;
}

fn_internal void* 
QueuePopRaw(queue* q) {
    if( q->len == 0 ) { return NULL; }

    if( q->idx_front == 0 ) {
        q->idx_front = q->capacity - 1;

        return q->data;
    }

    u8* data = (u8*)q->data + (q->idx_front * q->data_size);
    q->idx_front -= 1;

    q->len -= 1;

    return (void*)data;
}

fn_internal void* 
QueueGetFrontRaw(queue* q) {
    if( q->len == 0 ) { return NULL; }

    u8* data = (u8*)q->data + (q->idx_front * q->data_size);

    return (void*)data;
}
#endif