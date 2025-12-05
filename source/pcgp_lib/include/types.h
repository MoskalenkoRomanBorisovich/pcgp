#ifndef TYPES_H
#define TYPES_H

#include <assert.h>
#include <stdbool.h>
#include <limits.h>

typedef struct {
    int n;      // order
    int k;      // number of jumps
    int so;     // number of static jumps
    int* s;     // jumps
} Graph;

typedef struct {
    int     diam;
    double  aspl;
} GraphProp;

typedef struct {
    unsigned int diam;
    unsigned int dist_sum;
} IntGraphProp;

inline void IntGraphProp_infty(IntGraphProp* prop) {
    prop->diam = UINT_MAX;
    prop->dist_sum = UINT_MAX;
}

inline bool IntGraphProp_equal(const IntGraphProp* a, const IntGraphProp* b) {
    if (a->dist_sum != b->dist_sum)
        return false;
    return a->diam == b->diam;
}

inline bool IntGraphProp_less(const IntGraphProp* a, const IntGraphProp* b) {
    if (a->dist_sum < b->dist_sum)
        return true;
    if (a->dist_sum > b->dist_sum)
        return false;
    return a->diam < b->diam;
}

inline bool IntGraphProp_greater(const IntGraphProp* a, const IntGraphProp* b) {
    if (a->dist_sum > b->dist_sum)
        return true;
    if (a->dist_sum < b->dist_sum)
        return false;
    return a->diam > b->diam;
}

inline void calcGraphProp(int n, const IntGraphProp* int_prop, GraphProp* prop) {
    assert(n > 1);
    prop->diam = int_prop->diam;
    prop->aspl = (double)int_prop->dist_sum / (n - 1);
}

typedef struct {
    int u;
    int v;
} VertPair;

#endif