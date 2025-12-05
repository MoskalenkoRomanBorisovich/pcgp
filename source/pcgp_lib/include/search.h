#ifndef SEARCH_H
#define SEARCH_H

#include "types.h"

//// Search functions

// process found graphs
// return false to finish the search
typedef bool (*found_graph_cb)(void* userdata, const Graph* new_optimal_graph, const IntGraphProp* new_optimal_prop);

void optimal_search_6(
    Graph* start_graph,
    IntGraphProp* restrict best_prop, // best already found property
    found_graph_cb found_graph, void* fg_userdata
);

#endif
