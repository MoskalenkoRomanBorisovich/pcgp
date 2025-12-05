#ifndef PCGP_H
#define PCGP_H

#include "types.h"
#include "pcgpmem.h"

bool next_lexicographic_step(Graph* g);

inline void adjVert(VertPair*p, Graph*g, int u, int i) {
    p->u = (u + g->s[i]) % g->n;
    p->v = (u - g->s[i] + g->n) % g->n;
}

// old function
void circulantBFS(Arena*arena, GraphProp* prop, Graph*g);

// old function without modulo operation
void circulantBFS_2(Arena*arena, GraphProp*prop, const Graph*g);

// new symmetry function with padding
int* initCirculantBFS_3(int n, int* const dist);
void circulantBFS_3_impl(int*dist, int*queue, GraphProp*prop, const Graph*g);

// new symmetry function without padding
void circulantBFS_4_impl(int*dist, int*queue, GraphProp*prop, const Graph*g);

// new symmetry function without padding with early stop
bool circulantBFS_5_impl(unsigned int*dist, int*queue, const IntGraphProp*best_prop, const Graph*g, IntGraphProp*prop);

bool circulantBFS_6(unsigned int*dist, int*queue, const Graph*g, IntGraphProp*best_prop, IntGraphProp*prop);

int circulantKernighanLin(Arena*arena, Graph*graph);

inline bool graphCheck_impl(int n, int k, int so) {
    if (n < 0 || k <= 0 || k > n / 2 || so < 0 || so > k) {
        return true;
    }
    else {
        return false;
    }
}

inline bool graphCheck(Graph* g) {
	return graphCheck_impl(g->n, g->k, g->so);
}


#endif
