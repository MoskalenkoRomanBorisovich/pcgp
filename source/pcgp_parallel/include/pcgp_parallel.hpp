#pragma once

#include<algorithm>
#include<ranges>
#include<execution>
#include<optional>
#include<concepts>
#include<format>

#include<pcgp_wrap.h>

namespace pcgp {

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


/// @brief searches for circulants with least mean distances and diameters
/// @param n number of nodes
/// @param k number of jumps
/// @param so number of fixed jumps
/// @param start_prop initial guess for best graph props (for optimal BFS pruning)
/// @param found_callback handler of newly found graphs. Takes in id of the thread, graph, properties, best properties over all threads
/// @param num_threads number of concurrent threads to run
/// @return properties of the best found graphs
template<std::invocable<unsigned int, const Graph&, const IntGraphProp&, const std::atomic<IntGraphProp>&> F>
std::optional<IntGraphProp> parallel_optimal_search_simple(
	int n,
	int k,
	int so,
	const IntGraphProp& start_prop,
	F&& found_callback,
	const unsigned int num_threads = std::thread::hardware_concurrency()
) {

	if (graphCheck_impl(n, k, so)) [[unlikely]] {
		assert(false);
		return {};
	}

	const size_t ks = k - so;
	const size_t n_2 = (n + 1) / 2;

	// range of first variable jump
	const std::ranges::iota_view s1_range((int)so + 1, (int)(n_2 - ks + 2));
	const auto init_s = [](int s1, Graph& g) {
		// set fixed jumps
		for (int i = 0; i < g.so; ++i) {
			g.s[i] = i + 1;
		}
		g.s[g.so] = s1;
		for (size_t i = g.so + 1; i < g.k; ++i) {
			g.s[i] = g.s[i - 1] + 1;
		}
	};

	std::atomic<IntGraphProp> best_prop(start_prop);
	static_assert(best_prop.is_always_lock_free);

	std::atomic<size_t> next_index(0);
	const auto get_next_index = [&next_index]() {return next_index.fetch_add(1, std::memory_order_relaxed); };
	const size_t index_end = s1_range.size();

	std::exception_ptr e_ptr;
	std::mutex e_ptr_mutex;

	const auto thread_fn = [&](const unsigned int thread_id_) {
	try{
		const unsigned int thread_id = thread_id_;
		Graph g;
		g.n = n;
		g.k = k;
		g.so = so;
		const std::unique_ptr<int[]> s_data = std::make_unique<int[]>(k);
		g.s = s_data.get();

		// init bfs data
		const std::unique_ptr<unsigned int[]> dist_buf = std::make_unique<unsigned int[]>(n);
		const std::unique_ptr<int[]> queue_buf = std::make_unique<int[]>(n);

		for (size_t index = get_next_index(); index < index_end; index = get_next_index()) {
			const int s1 = s1_range[index]; // simple work stealing
			init_s(s1, g);

			do {
				IntGraphProp prop;
				IntGraphProp best_prop_local = best_prop.load(std::memory_order_relaxed);
				if (!circulantBFS_6(dist_buf.get(), queue_buf.get(), &g, &best_prop_local, &prop))
					continue;

				while (IntGraphProp_less(&prop, &best_prop_local)) {
					if (best_prop.compare_exchange_weak(
						best_prop_local,
						prop,
						std::memory_order_relaxed,
						std::memory_order_relaxed
					)) {
						break;
					}
				}

				if (IntGraphProp_greater(&prop, &best_prop_local))
					continue;
				found_callback(thread_id, g, prop, best_prop);
			} while (next_lexicographic_step(&g) && g.s[so] == s1);
		}
	}
	catch (...) {
		std::scoped_lock lock(e_ptr_mutex);
		if (!e_ptr)
			e_ptr = std::current_exception();
	}
	};

	{
		std::unique_ptr<std::jthread[]> threads = std::make_unique<std::jthread[]>(num_threads);
		for (unsigned int t = 0; t < num_threads; ++t) {
			threads[t] = std::jthread([t, &thread_fn]() {thread_fn(t); });
		}
	}

	if (e_ptr) [[unlikely]] {
		std::rethrow_exception(e_ptr);
	}

	return best_prop.load();
}

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

/// @brief writes results in csv format
/// @param n number of nodes
/// @param k number of jumps
/// @param best_prop properties of graphs
/// @param s_res jumps of graphs 
/// @param header if true adds header
/// @param o_stream output stream
void write_results_csv(
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
		o_stream << "C(" << n << ";";
		for (size_t j = 0; j < k; ++j) {
			o_stream << s_res[i * k + j];
			if (j + 1 < k)
				o_stream << ";";
		}
		o_stream << "),";
		o_stream << std::format("{},{:.16f},", best_prop.diam, best_prop.aspl);
		o_stream << n_circulant_edges(n, std::span( s_res.data() + i * k, k ));
		o_stream << '\n';
	}
};

}