#pragma once

#include<algorithm>
#include<ranges>
#include<execution>
#include<optional>
#include<concepts>
#include<format>
#include<functional>

#include<pcgp_wrap.h>

namespace pcgp {

void normalize_jumps(int n, int k, int* s);

/// @brief calculate properties of a single circulant graph
/// @param g graph
/// @return properties
std::optional<IntGraphProp> calc_single_prop(const Graph& g);


/// @brief calculate properties of a single circulant graph
/// @param n number of nodes
/// @param s jumps
/// @return properties
inline std::optional<IntGraphProp> calc_single_prop(int n, std::span<int> s) {
	const Graph g{
		.n = n,
		.k = static_cast<int>(s.size()),
		.so = 0,
		.s = s.data(),
	};
	return calc_single_prop(g);
}


/// @brief finds property of the best circulant graph
std::optional<IntGraphProp> pcgp_prop(
	int n, int k, int so,
	IntGraphProp init_prop = IntGraphProp_infty(),
	const unsigned int n_threads = std::thread::hardware_concurrency()
);

/// @brief finds circulants with least mean distance and diameter
/// @param s_res output jumps of found graphs
/// @param n number of nodes
/// @param k number of jumps
/// @param so number of fixed jumps
/// @param init_prop guarantied minimum best prop (for optimal BFS pruning)
/// @param n_threads number of concurrent threads
/// @return properties of the best graphs
std::optional<IntGraphProp> pcgp_parallel(
	std::vector<int>& s_res, 
	int n, int k, int so, 
	IntGraphProp init_prop = IntGraphProp_infty(), 
	const unsigned int n_threads = std::thread::hardware_concurrency()
);

/// @brief finds circulants with least mean distance and diameter
/// @param s_res output jumps of found graphs
/// @param n number of nodes
/// @param k number of jumps
/// @param so number of fixed jumps
/// @param init_prop guarantied minimum best prop (for optimal BFS pruning)
/// @param n_threads number of concurrent threads
/// @return properties of the best graphs
std::optional<IntGraphProp> pcgp_parallel_isomorph(
	std::vector<int>& s_res,
	int n, int k, int so,
	IntGraphProp init_prop = IntGraphProp_infty(),
	const unsigned int n_threads = std::thread::hardware_concurrency()
);

/// @brief finds circulants with least mean distance and diameter
/// @param s_res output jumps of found graphs
/// @param n number of nodes
/// @param k number of jumps
/// @param so number of fixed jumps
/// @param init_prop guarantied minimum best prop (for optimal BFS pruning)
/// @param n_threads number of concurrent threads
/// @return properties of the best graphs
std::optional<IntGraphProp> pcgp_parallel_isomorph_full(
	std::vector<int>& s_res,
	int n, int k, int so,
	IntGraphProp init_prop = IntGraphProp_infty(),
	const unsigned int n_threads = std::thread::hardware_concurrency()
);


/// @brief finds circulants acceptable by predicate with least mean distance and diameter
/// @param s_res output jumps of found graphs
/// @param n number of nodes
/// @param k number of jumps
/// @param so number of fixed jumps
/// @param pred predicate function, on
/// @param init_prop guarantied minimum best prop (for optimal BFS pruning)
/// @param n_threads number of concurrent threads
/// @return properties of the best graphs
std::optional<IntGraphProp> pcgp_parallel_pred(
	std::vector<int>& s_res,
	int n, int k, int so,
	std::function<bool(const Graph&)> pred,
	IntGraphProp init_prop = IntGraphProp_infty(),
	const unsigned int n_threads = std::thread::hardware_concurrency()
);




inline void write_signature(int n, std::span<const int> s, auto&& o_stream)
{
	const int k = static_cast<int>(s.size());
	o_stream << "C(" << n << ";";
	for (size_t j = 0; j < k; ++j) {
		o_stream << s[j];
		if (j + 1 < k)
			o_stream << ";";
	}
	o_stream << ")";
}

/// @brief writes results in csv format
/// @param n number of nodes
/// @param k number of jumps
/// @param best_prop properties of graphs
/// @param s_res jumps of graphs 
/// @param header if true adds header
/// @param o_stream output stream
inline void write_results_csv(
	int n,
	int k,
	const GraphProp& best_prop,
	const std::span<const int> s_res,
	bool header,
	auto& o_stream
) {
	if (header) {
		o_stream << "N,K,S,diameter,averageShortestPathLength,edges\n";
	}
	const size_t n_graphs = s_res.size() / k;
	for (size_t i = 0; i < n_graphs; ++i) {
		o_stream << n << "," << k << ",";
		write_signature(n, std::span(s_res.begin() + k * i, k), o_stream);
		o_stream << ",";
		o_stream << std::format("{},{:.16f},", best_prop.diam, best_prop.aspl);
		o_stream << n_circulant_edges(n, std::span( s_res.data() + i * k, k ));
		o_stream << '\n';
	}
};

}