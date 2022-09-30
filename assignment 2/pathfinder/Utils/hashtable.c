#include "hashtable.h"

u_long ht_computehash(const Hashtable *ht, const void *key) { return ht->hash_fn(key) % ht->capacity; }

Hashtable *ht_init(const size_t key_width, const size_t val_width, const ht_hash hash_fn, const ht_kcopy kcopy,
                   const ht_vcopy vcopy, const ht_kdtr kdtr, const ht_vdtr vdtr, const ht_kequal kequal,
                   const uint capacity) {

    Hashtable *ht = malloc(sizeof *ht);

    // starting field values
    *ht = (Hashtable){
        .table     = calloc(capacity, sizeof(Bucket *)),
        .size      = 0,
        .capacity  = capacity,
        .key_width = key_width,
        .val_width = val_width,
        .kcopy     = kcopy,
        .vcopy     = vcopy,
        .kdtr      = kdtr,
        .vdtr      = vdtr,
        .kequal    = kequal,
        .hash_fn   = hash_fn,
    };

    return ht;
}

Hashtable *ht_init_int_void() {
    return ht_init(sizeof(int), sizeof(void *), (ht_hash)ht_moduloHash, NULL, NULL, NULL, NULL,
                   (ht_kequal)ht_kequal_int, HT_START_SIZE);
}

Hashtable *ht_init_uint16_void() {
    return ht_init(sizeof(uint16_t), sizeof(void *), (ht_hash)ht_moduloHash_uint16, NULL, NULL, NULL, NULL,
                   (ht_kequal)ht_kequal_uint16, HT_START_SIZE);
}

bool ht_insert(Hashtable **ht, const void *key, const void *val) {

    // dynamic resizing
    if ((*ht)->capacity == (*ht)->size)
        *ht = ht_grow(*ht);

    (*ht)->size++;
    uint    pos = ht_computehash(*ht, key);
    Bucket *b   = malloc(sizeof *b);

    // prepare the bucket
    *b = (Bucket){
        .key = malloc((*ht)->key_width),
        .val = malloc((*ht)->val_width),
    };

    // copy the key value pair into the bucket
    if ((*ht)->kcopy)
        (*ht)->kcopy(b->key, key);
    else
        memcpy(b->key, key, (*ht)->key_width);
    if ((*ht)->vcopy)
        (*ht)->vcopy(b->val, val);
    else
        memcpy(b->val, val, (*ht)->val_width);

    // add to has(*ht)able, linearly probe when collisions occur
    for (uint i = 0; i < (*ht)->capacity; i++) {
        if ((*ht)->table[pos] == NULL) {
            (*ht)->table[pos] = b;
            return true;
        }

        pos = (pos + 1 == (*ht)->capacity) ? 0 : pos + 1; // avoid modulo
    }

    return false; // could not find space (shuold not happen...)
}

void *ht_lookup(const Hashtable *ht, const void *key) {

    uint pos = ht_computehash(ht, key);

    // compare keys
    for (int i = 0; i < ht->capacity; i++) {
        if (ht->table[pos] == NULL)
            return NULL;

        if (ht->kequal && ht->kequal(key, ht->table[pos]->key))
            return ht->table[pos]->val;
        else if (!ht->kequal && key == ht->table[pos]->key)
            return ht->table[pos]->val;

        pos = (pos + 1 == ht->capacity) ? 0 : pos + 1;
    }

    return NULL;
}

void *ht_lookupVal(const Hashtable *ht, const void *key) {
    void **res = ht_lookup(ht, key);
    if (res == NULL)
        return NULL;
    return *res;
}

void ht_free(Hashtable *ht) {

    for (int i = 0; i < ht->capacity; i++) {
        if (ht->table[i]) {

            // free key
            if (ht->kdtr)
                ht->kdtr(ht->table[i]->key);
            free(ht->table[i]->key);

            // free value
            if (ht->vdtr)
                ht->vdtr(ht->table[i]->val);
            free(ht->table[i]->val);

            // free bucket
            free(ht->table[i]);
        }
    }

    free(ht->table);
    free(ht);
}

Hashtable *ht_grow(Hashtable *ht) {

    // make a new hashtable twice as large
    Hashtable *ht_bigger = ht_init(ht->key_width, ht->val_width, ht->hash_fn, ht->kcopy, ht->vcopy, ht->kdtr, ht->vdtr,
                                   ht->kequal, ht->capacity * 2);

    // add all k-v pairs into the bigger hashtable
    for (uint i = 0; i < ht->capacity; i++) {
        if (ht->table[i])
            ht_insert(&ht_bigger, ht->table[i]->key, ht->table[i]->val);
    }

    // free the old hashtable
    ht_free(ht);

    return ht_bigger;
}

u_long ht_polyRollingHash(const char **key) {
    u_long hash_value = 0;
    // compute hash of string key using a polynomial rolling hash
    const uint p    = 53;
    const uint m    = 1e9 + 9;
    ulong      ppow = 1;
    for (size_t i = 0; i < strlen(*key); i++) {
        hash_value = (hash_value + ((*key)[i] - 'a' + 1) * ppow) % m;
        ppow       = (ppow * p) % m;
    }

    return hash_value;
}

u_long ht_moduloHash(int *key) { return *key % (1000000009); }
u_long ht_moduloHash_uint16(uint16_t *key) { return *key % (1000000009); }

bool ht_kequal_int(int *k1, int *k2) { return (*k1 == *k2); }
bool ht_kequal_uint16(uint16_t *k1, uint16_t *k2) { return (*k1 == *k2); }

/* int main() {

    // testing
    char *w1 = "one";
    char *w2 = "two";
    char *w3 = "three";
    char *w4 = "four";
    char *w5 = "five";
    char *w6 = "six";
    char *w7 = "seven";
    char *w8 = "eight";
    int   v1 = 1;
    int   v2 = 2;
    int   v3 = 3;
    int   v4 = 4;
    int   v5 = 5;
    int   v6 = 6;
    int   v7 = 7;
    int   v8 = 8;

    Hashtable *ht = ht_init(sizeof(char *), sizeof(int), (ht_hash)ht_polyRollingHash, (ht_kcopy)str_cpyctr, NULL,
                            (ht_kdtr)str_dtr, NULL, (ht_kequal)str_equal, HT_START_SIZE);

    ht_insert(&ht, &w1, &v1);
    ht_insert(&ht, &w2, &v2);
    ht_insert(&ht, &w3, &v3);
    ht_insert(&ht, &w4, &v4);

    int *l1 = ht_lookup(ht, &w1);
    int *l2 = ht_lookup(ht, &w2);
    int *l3 = ht_lookup(ht, &w7);
    int *l4 = ht_lookup(ht, &w8);

    if (l1)
        printf("Actual: Key found %d\tExpected: Key found %d\n", *l1, v1);
    else
        printf("Actual: Key not found\tExpected: key found %d\n", v1);

    if (l2)
        printf("Actual: Key found %d\tExpected: Key found %d\n", *l2, v2);
    else
        printf("Actual: Key not found\tExpected: key found %d\n", v2);

    if (l3)
        printf("Actual: Key found %d\tExpected: Key not found\n", *l3);
    else
        printf("Actual: Key not found\tExpected: key not found\n");

    if (l4)
        printf("Actual: Key found %d\tExpected: Key not found\n", *l4);
    else
        printf("Actual: Key not found\tExpected: key not found\n");

    ht_insert(&ht, &w1, &v1);
    ht_insert(&ht, &w5, &v5);
    ht_insert(&ht, &w8, &v8);
    ht_insert(&ht, &w7, &v7);
    ht_insert(&ht, &w6, &v6);

    int *l5 = ht_lookup(ht, &w1);
    int *l6 = ht_lookup(ht, &w7);
    int *l7 = ht_lookup(ht, &w2);

    if (l5)
        printf("Actual: Key found %d\tExpected: Key found %d\n", *l5, v1);
    else
        printf("Actual: Key not found\tExpected: key found %d\n", v1);

    if (l6)
        printf("Actual: Key found %d\tExpected: Key found %d\n", *l6, v7);
    else
        printf("Actual: Key not found\tExpected: key found %d\n", v7);

    if (l7)
        printf("Actual: Key found %d\tExpected: Key found %d\n", *l7, v2);
    else
        printf("Actual: Key not found\tExpected: key found %d\n", v2);

    ht_free(ht);

    Hashtable *h2 =
        ht_init(sizeof(char *), sizeof(char *), (ht_hash)ht_polyRollingHash, (ht_kcopy)str_cpyctr,
                (ht_vcopy)str_cpyctr, (ht_kdtr)str_dtr, (ht_vdtr)str_dtr, (ht_kequal)str_equal, HT_START_SIZE);

    ht_insert(&h2, &w1, &w2);
    char **l8 = ht_lookup(h2, &w1);
    char **l9 = ht_lookup(h2, &w2);

    if (l8)
        printf("Actual: %s\t\tExpected: %s\n", *l8, w2);
    else
        printf("Actual: not found\tExpected: %s\n", w2);
    if (l9)
        printf("Actual: %s\tExpected: not found\n", *l9);
    else
        printf("Actual: not found\tExpected: not found\n");

    ht_insert(&h2, &w2, &w3);
    char **l10 = ht_lookup(h2, &w2);
    if (l10)
        printf("Actual: %s\t\tExpected: %s\n", *l10, w3);
    else
        printf("Actual: not found\tExpected: %s\n", w3);

    ht_free(h2);

    return 0;
} */
