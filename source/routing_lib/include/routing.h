#pragma once

#include <span>
#include <pcgp_wrap.h>
#include <concepts>
#include <memory>
#include <iterator>
#include <vector>
#include <numeric>
#include <optional>
#include <thread>


namespace routing {
/*
all functions called ***_next_step  take in 
n - number of verts
s - jumps in ascending order
i - id of current vertex
j - id of target vertex

return - index of the next vertex, or -1 if step is impossible

*/


// returns -1 if there is no acceptible step
[[nodiscard]] int clockwise_next_step(int n, std::span<const int> s, int i, int j);

// Algorithm from "Routing in triple loop circulants: A case of networks-on-chip"
[[nodiscard]] int advanced_clockwise_next_step(int n, std::span<const int> s, int i, int j);

// Algorithm from "Development of routing algorithms in networks-on-chip based on ring circulant topologies"
[[nodiscard]] int adaptive_next_step(int startNode, int endNode, int N, int s1, int s2);

/// @brief  wraper with the same format as other algorithms. 
/// @param n 
/// @param s - s.size() == 2 required
/// @param i 
/// @param j 
/// @return 
[[nodiscard]] inline int adaptive_next_step_wrap(int n, std::span<const int> s, int i, int j){
	if (s.size() != 2) [[unlikely]]
		return -1;
	return adaptive_next_step(i, j, n, s[0], s[1]);
}

// Algorithm from "Routing in triple loop circulants: A case of networks-on-chip"
[[nodiscard]] int direction_selection_next_step(int startNode, int endNode, int N, int s1, int s2, int s3);

/// @brief wraper with the same format as other algorithms
/// @param n 
/// @param s - s.size() == 3 required
/// @param i 
/// @param j 
/// @return 
[[nodiscard]] inline int direction_selection_next_step_wrap(int n, std::span<const int> s, int i, int j) {
	if (s.size() != 3) [[unlikely]]
		return -1;
	return direction_selection_next_step(i, j, n, s[0], s[1], s[2]);
}

/// @brief calculate distance from root to each vertex of the tree
/// @param n - number of nodes
/// @param tree - array of size n - parent ids for each node
/// @param dist - array of size n - output distances, -1 means node is unreachable
void calc_tree_dist(int n, const int* tree, int* dist);

/// @brief calculate circulant properties using distance from node zero to other nodes
/// @param dist 
/// @return 
IntGraphProp calc_graph_stats(std::span<const int> dist);

[[nodiscard]] inline bool map_jumps_to_zero(int n, std::span<const int> s, auto&& routing_fn, int* out_next_vert) {
	for (int i = 0; i < n; ++i) {
		const int j = routing_fn(n, s, i, 0);
		if (j == -1) [[unlikely]]
			return false;
		out_next_vert[i] = j;
	}
	return true;
}


inline std::optional<IntGraphProp> circulant_routing_stats(int n, std::span<const int> s, auto&& routing_fn, int* tree_buf, int* dist_buf) {
	if (!map_jumps_to_zero(n, s, routing_fn, tree_buf))
		return {};
	calc_tree_dist(n, tree_buf, dist_buf);
	for (int i = 0; i < n; ++i) {
		if (dist_buf[i] == -1) [[unlikely]]
			return {};
	}
	return calc_graph_stats(std::span(dist_buf, n));
}

inline std::optional<IntGraphProp> circulant_routing_stats(int n, std::span<const int> s, auto&& routing_fn) {
	std::unique_ptr<int[]> tree = std::make_unique<int[]>(n);
	std::unique_ptr<int[]> dist = std::make_unique<int[]>(n);
	return circulant_routing_stats(n, s, routing_fn, tree.get(), dist.get());
}

inline std::pair<IntGraphProp, std::vector<int>> search_for_best_graph(int n, int k, int so, auto&& routing_fn) {
	std::unique_ptr<int[]> s_data = std::make_unique<int[]>(k);
	Graph g{
		.n = n,
		.k = k,
		.so = so,
		.s = s_data.get() 
	};
	std::iota(g.s, g.s + k, 1);
	std::unique_ptr<int[]> tree = std::make_unique<int[]>(n);
	std::unique_ptr<int[]> dist = std::make_unique<int[]>(n);

	std::vector<int> s_res;
	s_res.reserve(k);

	IntGraphProp best_prop = IntGraphProp_infty();
	do {
		const auto prop_ = circulant_routing_stats(n, std::span(g.s, k), routing_fn, tree.get(), dist.get());
		if (!prop_)
			continue;
		const IntGraphProp& prop = *prop_;
		if (IntGraphProp_greater(&prop, &best_prop))
			continue;
		if (IntGraphProp_less(&prop, &best_prop)) {
			best_prop = prop;
			s_res.clear();
		}
		std::copy_n(s_data.get(), k, std::back_inserter(s_res));

	} while (next_lexicographic_step(&g));
	return { best_prop, s_res };
}


struct blocked_table {
	int n_blocks;
	int* block_end; // size = n_blocks, ascenging order
	int* block_v_next; // size = n_blocks
		
};

/// @brief constructs optimal blocked table routing
/// @param n number of nodes
/// @param k number of jumps
/// @param s set of jumps
/// @param dist distance to each node from node 0
/// @param out resulting table
void blocked_table_routing(int n, int k, const int* s, const unsigned* dist, blocked_table& out);




} // routing