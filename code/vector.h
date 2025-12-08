#ifndef _VECTOR_H_
#define _VECTOR_H_

/**
 * @author Sascha Paez
 *
 * @paragraph This single-header library intends to implement a simple to use vector functionality. The memory will be managed outside
 * this library as it permits the user to handle memory as they intend to, only making the library as an interface to manage the data they want to
 * handle
 */

typedef struct vector vector;
struct vector {
    void* data;
    u32   len;
    u32   offset;
    u32   capacity;
    u32   data_size;
};

/**
 * @author Sascha Paez
 * @brief  Returns a vector struct initialized with the data passed
 *
 * @param buffer    Buffer memory handled and initiaized by the user
 * @param len       Starting length of the vector
 * @param capacity  The maximum capacity of the vector
 * @param data_size The size of the type we want to handle in the vector
 *
 * @return vector   Initialized vector
 * @note For sake for user manageability, the buffer will be initialized and destroyed by them
 */
fn_internal vector VectorInit(void* buffer, u32 len, u32 capacity, u32 data_size);

/**
 * @author Sascha Paez
 * @brief  Resets the struct passed by the user
 *
 * @note For sake for user manageability, the buffer will never be freed by the function
 */
fn_internal void VectorReset(vector* v);

/**
 * @author Sascha Paez
 * @brief  Appends a value into the vector
 */
fn_internal void VectorAppend(vector* v, void* val);

/**
 * @author Sascha Paez
 * @brief  Sets a value into a given idx
 */
fn_internal void VectorSet(vector* v, void* val, u32 idx);

/**
 * @author Sascha Paez
 * @brief  Gets the pointer to the value from a given idx
 */
fn_internal void* VectorGet(vector* v, u32 idx);

/**
 * @author Sascha Paez
 * @brief  Resizes the vector into a new capacity
 */
fn_internal void VectorResize(vector* v, void* buffer, u32 new_len);

/**
 * @author Sascha Paez
 * @brief  Pops the last value in the vector
 *
 * @note It is important to note that, as it does not manage the memory from the buffer, until the buffer is freed it will always
 * occupy the same amount of memory
 */
fn_internal void VectorPop(vector* v);

/**
 * @author Sascha Paez
 *
 * @brief Makes it nicer to create the vector, passing the type the user wants to use
 */
#define VectorNew(buffer, len, capacity, type) VectorInit(buffer, len, capacity, sizeof(type))

#endif

#ifdef VECTOR_IMPL

fn_internal vector
VectorInit(void* buffer, u32 len, u32 capacity, u32 data_size) {
	vector v = {};
	v.data = buffer;
	v.len = len;
	v.offset = len * data_size;
	v.capacity = capacity;
	v.data_size = data_size;

    return v;
}

fn_internal void
VectorReset(vector* v) {
    memset(v, 0, sizeof(vector));
}

fn_internal void
VectorClear(vector* v) {
    v->len = 0;
    v->offset = 0;
}

fn_internal void
VectorAppend(vector* v, void* val) {
    if( v->len < v->capacity) {
        memcpy(((u8*)v->data + v->offset), val, v->data_size);
        v->len += 1;
        v->offset += v->data_size;
    } else {
        /**
         * @todo Is it better to handle it, to assert it or to reset the last value into this one?
         */
    }
}

fn_internal void
VectorPop(vector* v) {
    if(v->len > 0) {
        v->len -= 1;
        v->offset -= v->data_size;
    }
}

fn_internal void
VectorResize(vector* v, void* buffer, u32 new_len) {
    u32 len       = v->len;
    u32 offset    = v->offset;
    u32 data_size = v->data_size;

    memcpy((u8*)buffer, (u8*)v->data , v->offset);
    VectorReset(v);
    *v = VectorInit(buffer, 0, new_len, data_size);
    v->offset = offset;
    v->len    = len;
}

fn_internal void
VectorSet(vector* v, void* val, u32 idx) {
    if( v == NULL ) { return; }
    if( v->data == NULL ) { return; }
    if( idx > v->len ) { return; }

    u32 idx_offset = idx * v->data_size;

    memcpy(((u8*)v->data + idx_offset), val, v->data_size);
}

fn_internal void*
VectorGet(vector* v, u32 idx) {
    if( v == NULL ) { return NULL; }
    if( v->data == NULL ) { return NULL; }
    if( idx > v->len ) { return NULL; }

    u32 idx_offset = idx * v->data_size;

    return (u8*)v->data + idx_offset;
}

#endif