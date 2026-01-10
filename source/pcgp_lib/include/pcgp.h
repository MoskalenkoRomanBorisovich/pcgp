#ifndef PCGP_H
#define PCGP_H

#include "types.h"
#include "pcgpmem.h"

/// @brief next circulant graph in lexicographic order
/// @param g current graph, changed inplace
/// @return false if current graph is last in lexicographic order
bool next_lexicographic_step(Graph* g);

/// @brief get points adjacent by jump
/// @param p output pair of points
/// @param g graph
/// @param u current vert
/// @param i id of the jump
inline void adjVert(VertPair*p, Graph*g, int u, int i) {
    p->u = (u + g->s[i]) % g->n;
    p->v = (u - g->s[i] + g->n) % g->n;
}

// old function
void circulantBFS(Arena*arena, GraphProp* prop, const Graph*g);

// old function without modulo operation
void circulantBFS_2(Arena*arena, GraphProp*prop, const Graph*g);

// new symmetry function with padding
int* initCirculantBFS_3(int n, int* const dist);
void circulantBFS_3_impl(int*dist, int*queue, GraphProp*prop, const Graph*g);

// new symmetry function without padding
void circulantBFS_4_impl(int*dist, int*queue, GraphProp*prop, const Graph*g);

// new symmetry function without padding with early stop
bool circulantBFS_5_impl(unsigned int*dist, int*queue, const IntGraphProp*best_prop, const Graph*g, IntGraphProp*prop);

/// @brief find circulant graph properties 
/// @param dist distance buffer of size g.n
/// @param queue queue buffer of size g.n
/// @param g circulant graph
/// @param best_prop minimum required properties (for BFS pruning)
/// @param prop output found properties
/// @return false if search was pruned due to exceeding best_prop values
bool circulantBFS_6(unsigned int*dist, int*queue, const Graph*g, const IntGraphProp*best_prop, IntGraphProp*prop);


/// @brief Krninghan Lin graph partition
/// @param arena arena alocator
/// @param graph graph
/// @return width of partition
int circulantKernighanLin(Arena*arena, Graph*graph);

/// @brief check if graph parameters are incorrect
/// @param n number of nodes 
/// @param k number of jumps
/// @param so number of fixed jumps
/// @return true if graph is incorrect
inline bool graphCheck_impl(int n, int k, int so) {
    if (n < 0 || k <= 0 || k > n / 2 || so < 0 || so > k) {
        return true;
    }
    else {
        return false;
    }
}

/// @brief check if graph parameters are incorrect
/// @param g graph
/// @return true if graph is incorrect
inline bool graphCheck(Graph* g) {
	return graphCheck_impl(g->n, g->k, g->so);
}


#endif
