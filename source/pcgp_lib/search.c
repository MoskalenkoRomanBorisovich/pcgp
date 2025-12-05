#ifndef SEARCH_CPP
#define SEARCH_CPP

#include "search.h"

#include "pcgp.h"

#include <malloc.h>


void optimal_search_6(
    Graph* start_graph,
    IntGraphProp* restrict best_prop,
    found_graph_cb found_graph, void* fg_userdata
) {
    assert(start_graph != NULL);
    assert(best_prop != NULL);
    assert(found_graph != NULL);
    unsigned int* dist = (unsigned int*)malloc(start_graph->n * sizeof(unsigned int));
    int* queue = (int*)malloc(start_graph->n * sizeof(int));
    IntGraphProp cur_prop;
    do {
        if (!circulantBFS_6(dist, queue, start_graph, best_prop, &cur_prop))
            continue;
        if (IntGraphProp_greater(&cur_prop, best_prop))
            continue;
        *best_prop = cur_prop;
        if (!found_graph(fg_userdata, start_graph, &cur_prop)) {
            break;
        }

    } while (next_lexicographic_step(start_graph));
}

#endif

