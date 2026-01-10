#ifndef TYPES_H
#define TYPES_H

#include <assert.h>
#include <stdbool.h>
#include <limits.h>


/// @brief circulant graph description
typedef struct {
    int n;      // order
    int k;      // number of jumps
    int so;     // number of static jumps
    int* s;     // jumps
} Graph;

/// @brief graph properties
typedef struct {
    int     diam; // diameter
    double  aspl; // avarage shortest path length
} GraphProp;


/// @brief integer graph properties
typedef struct {
    unsigned int diam; // diameter
    unsigned int dist_sum; // sump of surtest paths from one node to all others
} IntGraphProp;


/// @brief initialize maximum possible property values
/// @param prop output
inline void IntGraphProp_infty(IntGraphProp* prop) {
    prop->diam = UINT_MAX;
    prop->dist_sum = UINT_MAX;
}

/// @brief return true if props are equal
inline bool IntGraphProp_equal(const IntGraphProp* a, const IntGraphProp* b) {
    if (a->dist_sum != b->dist_sum)
        return false;
    return a->diam == b->diam;
}

/// @brief compare props
inline bool IntGraphProp_less(const IntGraphProp* a, const IntGraphProp* b) {
    if (a->dist_sum < b->dist_sum)
        return true;
    if (a->dist_sum > b->dist_sum)
        return false;
    return a->diam < b->diam;
}

/// @brief compare props
inline bool IntGraphProp_greater(const IntGraphProp* a, const IntGraphProp* b) {
    if (a->dist_sum > b->dist_sum)
        return true;
    if (a->dist_sum < b->dist_sum)
        return false;
    return a->diam > b->diam;
}

/// @brief convert integer properties to real properties
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