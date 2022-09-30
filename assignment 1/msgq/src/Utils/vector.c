#include "vector.h"

Vector *vec_init(const size_t width, const vec_copyctr copyctr, const vec_dtr dtr, const uint capacity) {

    Vector *v = malloc(sizeof *v);

    *v = (Vector){
        .vec      = calloc(capacity, sizeof(void *)),
        .size     = 0,
        .capacity = capacity,
        .width    = width,
        .copyctr  = copyctr,
        .dtr      = dtr,
    };

    return v;
}

Vector *vec_init_ptr() { return vec_init(sizeof(void *), NULL, NULL, VEC_START_SIZE); }

bool vec_isEmpty(const Vector *v) { return (v->size == 0); }

bool vec_pushBack(Vector *v, const void *e) {

    // dynamic resizing
    if (v->capacity == v->size)
        if (!vec_grow(v))
            return false; // could not resize

    // make a copy of the data
    void *e_copy = malloc(v->width);
    if (v->copyctr)
        v->copyctr(e_copy, e);
    else
        memcpy(e_copy, e, v->width);

    // add to end of vector
    v->vec[v->size++] = e_copy;

    return true;
}

bool vec_insertAt(Vector *v, const void *e, const uint pos) {

    // invalid index
    if (pos > v->size)
        return false;

    // dynamic resizing
    if (v->capacity == v->size)
        if (!vec_grow(v))
            return false; // could not resize

    // move all elements to make room
    for (uint i = v->size; i > pos; i--)
        v->vec[i] = v->vec[i - 1];

    // make a copy of the data
    void *e_copy = malloc(v->width);
    if (v->copyctr)
        v->copyctr(e_copy, e);
    else
        memcpy(e_copy, e, v->width);

    // add at pos
    v->vec[pos] = e_copy;
    v->size++;

    return true;
}

void *vec_getAt(const Vector *v, const uint pos) {

    // invalid index
    if (pos >= v->size)
        return NULL;

    return v->vec[pos];
}

void *vec_getValAt(const Vector *v, const uint pos) {
    void **res = vec_getAt(v, pos);
    if (res == NULL)
        return NULL;
    return *res;
}

bool vec_popBack(Vector *v) {

    if (vec_isEmpty(v))
        return false;

    // free data
    v->size--;
    if (v->dtr)
        v->dtr(v->vec[v->size]);
    free(v->vec[v->size]);

    return true;
}

bool vec_removeAt(Vector *v, const uint pos) {

    // invalid index
    if (pos >= v->size)
        return false;

    // free data
    v->size--;
    if (v->dtr)
        v->dtr(v->vec[pos]);
    free(v->vec[pos]);

    // move elements
    for (uint i = pos; i < v->size; i++)
        v->vec[i] = v->vec[i + 1];

    return true;
}

void vec_free(Vector *v) {

    // deep free
    for (uint i = 0; i < v->size; i++) {
        if (v->dtr)
            v->dtr(v->vec[i]);
        free(v->vec[i]);
    }

    free(v->vec);
    free(v);
}

bool vec_grow(Vector *v) {
    v->capacity *= 2;

    // return true on successful realloc
    return ((v->vec = realloc(v->vec, v->capacity * sizeof(void *))) != NULL);
}

/* int main() {

    // testing
    Vector *v = vec_init(sizeof(int), NULL, NULL, VEC_START_SIZE);

    int one   = 1;
    int two   = 2;
    int three = 3;
    int four  = 4;
    vec_pushBack(v, &one);
    vec_pushBack(v, &two);
    int *x1 = vec_getAt(v, 1);
    printf("Actual: %d\tExpected %d\n", *x1, two);
    vec_popBack(v);
    vec_pushBack(v, &two);
    int *x2 = vec_getAt(v, 1);
    printf("Actual: %d\tExpected %d\n", *x2, two);
    int *x3 = vec_getAt(v, 0);
    printf("Actual: %d\tExpected %d\n", *x3, one);
    int *x4 = vec_getAt(v, 3);
    if (!x4)
        printf("Actual: NULL\tExpected NULL\n");
    else
        printf("Actual: %d\tExpected: NULL\n", *x4);
    vec_removeAt(v, 0);
    int *x5 = vec_getAt(v, 0);
    printf("Actual: %d\tExpected %d\n", *x5, two);
    vec_pushBack(v, &two);
    vec_pushBack(v, &three);
    vec_insertAt(v, &four, 0);
    int *x6 = vec_getAt(v, 0);
    printf("Actual: %d\tExpected %d\n", *x6, four);
    int *x7 = vec_getAt(v, 3);
    printf("Actual: %d\tExpected %d\n", *x7, three);
    int *x8 = vec_getAt(v, 4);
    if (!x8)
        printf("Actual: NULL\tExpected NULL\n");
    else
        printf("Actual: %d\tExpected: NULL\n", *x8);
    vec_removeAt(v, 1);
    int *x9 = vec_getAt(v, 2);
    printf("Actual: %d\tExpected %d\n", *x9, three);

    vec_free(v);
} */
