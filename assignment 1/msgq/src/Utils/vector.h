#ifndef VECTOR_H
#define VECTOR_H

/**
 * Generic Vector ADT
 *
 * (type unsafe!)
 *
 */

#define VEC_START_SIZE 8

#include "utils.h"

typedef void (*vec_copyctr)(void *dest, const void *src); // copy constructor
typedef void (*vec_dtr)(void *p);                         // destructor

/**
 * Generic Vector implementation.
 * Doubles capacity when full.
 * Supports optional copy constructors and
 * destructors.
 */
typedef struct Vector {
    void      **vec;      // dynamic array of generic type
    uint        size;     // number of elements in the vector
    uint        capacity; // max number of elements before resizing
    size_t      width;    // sizeof the data type
    vec_copyctr copyctr;  // optional copy constructor function ptr
    vec_dtr     dtr;      // optional destructor function ptr
} Vector;

/**
 * Initialize a vector.
 * Requres sizeof the data type stored.
 * Copy constructors and destructional are optional.
 * Recommended capacity is VEC_START_SIZE.
 */
Vector *vec_init(const size_t width, const vec_copyctr copyctr, const vec_dtr dtr, const uint capacity);

/**
 * Initializes a void* vector.
 */
Vector *vec_init_ptr();

/**
 * Checks for empty vector.
 */
bool vec_isEmpty(const Vector *v);

/**
 * Adds an element to the end of the vector.
 * Resizing is done if required.
 *
 * Returns true on successful insertion.
 */
bool vec_pushBack(Vector *v, const void *e);

/**
 * Add an element at the given position.
 *
 * Returns true on successful insertion.
 * Returns false if failed or invalid position.
 */
bool vec_insertAt(Vector *v, const void *e, const uint pos);

/**
 * Returns a pointer to the element at given position.
 * Returns NULL if position is invalid.
 */
void *vec_getAt(const Vector *v, const uint pos);

/**
 * Returns the value at a position (for void * vectors)
 * Returns NULL if position is invalid
 */
void *vec_getValAt(const Vector *v, const uint pos);

/**
 * Remove the last element from vector and returns true.
 * Returns false if the vector is empty.
 */
bool vec_popBack(Vector *v);

/**
 * Removes the element at the specified position.
 * Returns false if invalid position.
 */
bool vec_removeAt(Vector *v, const uint pos);

/**
 * Cleans up all allocations.
 */
void vec_free(Vector *v);

/**
 * Doubles the vector capacity.
 */
bool vec_grow(Vector *v);

#endif
