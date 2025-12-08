#pragma once

#include<algorithm>
#include<ranges>
#include<execution>
#include<optional>
#include<concepts>
#include<format>

#include<pcgp_wrap.h>

namespace pcgp {

std::optional<IntGraphProp> calc_single_prop(const Graph& g);

inline std::optional<IntGraphProp> calc_single_prop(int n, std::span<int> s) {
	const Graph g{
		.n = n,
		.k = static_cast<int>(s.size()),
		.so = 0,
		.s = s.data(),
	};
	return calc_single_prop(g);
}


// simple parallel optimal search for circulant graphs
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
	std::ranges::iota_view s1_range((int)so + 1, (int)(n_2 - ks + 2));
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




//// simple parallel optimal search for circulant graphs
//std::optional<IntGraphProp> parallel_optimal_search_simple(
//	int n,
//	int k,
//	int so, // number of fiexed jumps
//	const IntGraphProp& start_prop,
//	auto&& found_callback// always called under mutex when a new best graph is found
//) {
//	if (graphCheck_impl(n, k, so)) [[unlikely]] {
//		assert(false);
//		return {};
//	}
//	const size_t ks = k - so;
//	const size_t n_2 = (n + 1) / 2;
//
//	// range of first variable jump
//	std::ranges::iota_view s1_range ((int)so + 1, (int)(n_2 - ks + 2));
//	const auto init_graph = [&](int s1, Graph& g) {
//		g.n = n;
//		g.k = k;
//		g.so = so;
//		// set fixed jumps
//		for (int i = 0; i < so; ++i) {
//			g.s[i] = i + 1;
//		}
//		g.s[so] = s1;
//		for (size_t i = so + 1; i < k; ++i) {
//			g.s[i] = g.s[i - 1] + 1;
//		}
//	};
//
//	IntGraphProp best_prop = start_prop;
//	std::mutex prop_mutex;
//	// for each value of the first variable jump, generate all circulants with that first jump and perform parallel optimal search
//	std::for_each(std::execution::par, s1_range.begin(), s1_range.end(), [&](const int s1) {
//		const std::unique_ptr<int[]> s_data = std::make_unique<int[]>(k);
//		Graph g;
//		g.s = s_data.get();
//		init_graph(s1, g);
//		// init bfs data
//		const std::unique_ptr<unsigned int[]> dist_buf = std::make_unique<unsigned int[]>(n);
//		const std::unique_ptr<int[]> queue_buf = std::make_unique<int[]>(n);
//		IntGraphProp prop;
//		IntGraphProp best_prop_local;
//		{
//			std::lock_guard lock(prop_mutex);
//			best_prop_local = best_prop;
//		}
//		do {
//			if (!circulantBFS_6(dist_buf.get(), queue_buf.get(), &g, &best_prop_local, &prop))
//				continue;
//
//			if (IntGraphProp_greater(&prop, &best_prop_local))
//				continue;
//			{
//				std::lock_guard lock(prop_mutex);
//				if (IntGraphProp_greater(&prop, &best_prop)) {
//					best_prop_local = best_prop;
//					continue;
//				}
//				found_callback(g, prop);
//				best_prop = prop;
//			}
//			best_prop_local = prop;
//		} while (next_lexicographic_step(&g) && g.s[so] == s1);
//	});
//	
//	return best_prop;
//}
//
//
//std::optional<IntGraphProp> parallel_optimal_search_simple_2(
//	int n,
//	int k,
//	int so,
//	const IntGraphProp& start_prop,
//	auto&& found_callback,
//	size_t num_threads = std::thread::hardware_concurrency()
//) {
//	if (graphCheck_impl(n, k, so)) [[unlikely]] {
//		assert(false);
//		return {};
//	}
//	
//	const size_t ks = k - so;
//	const size_t n_2 = (n + 1) / 2;
//
//	// range of first variable jump
//	std::ranges::iota_view s1_range((int)so + 1, (int)(n_2 - ks + 2));
//	const auto init_s = [](int s1, Graph& g) {
//		// set fixed jumps
//		for (int i = 0; i < g.so; ++i) {
//			g.s[i] = i + 1;
//		}
//		g.s[g.so] = s1;	
//		for (size_t i = g.so + 1; i < g.k; ++i) {
//			g.s[i] = g.s[i - 1] + 1;
//		}
//	};
//
//	IntGraphProp best_prop = start_prop;
//	std::mutex prop_mutex;
//
//	std::atomic<size_t> next_index(0);
//
//	const auto thread_fn = [&]() {
//		Graph g;
//		g.n = n;
//		g.k = k;
//		g.so = so;
//		const std::unique_ptr<int[]> s_data = std::make_unique<int[]>(k);
//		g.s = s_data.get();
//
//		// init bfs data
//		const std::unique_ptr<unsigned int[]> dist_buf = std::make_unique<unsigned int[]>(n);
//		const std::unique_ptr<int[]> queue_buf = std::make_unique<int[]>(n);
//		IntGraphProp prop;
//		IntGraphProp best_prop_local; // local best to minimize locking
//		
//		for (size_t index = next_index.fetch_add(1), index_end = s1_range.size(); index < index_end; index = next_index.fetch_add(1)) {
//			const int s1 = s1_range[index]; // simple work stealing
//			{ // regularly update local best
//				std::lock_guard lock(prop_mutex);
//				best_prop_local = best_prop;
//			}
//			init_s(s1, g);
//			do {
//				if (!circulantBFS_6(dist_buf.get(), queue_buf.get(), &g, &best_prop_local, &prop))
//					continue;
//
//				if (IntGraphProp_greater(&prop, &best_prop_local))
//					continue;
//				{
//					std::lock_guard lock(prop_mutex);
//					if (IntGraphProp_greater(&prop, &best_prop)) {
//						best_prop_local = best_prop;
//						continue;
//					}
//					found_callback(g, prop);
//					best_prop = prop;
//				}
//				best_prop_local = prop; // updated when a new global best is found
//			} while (next_lexicographic_step(&g) && g.s[so] == s1);
//		}
//	};
//
//	std::unique_ptr<std::jthread[]> threads = std::make_unique<std::jthread[]>(num_threads);
//	for (size_t t = 0; t < num_threads; ++t) {
//		threads[t] = std::jthread(thread_fn);
//	}
//	
//	return best_prop;
//}


std::optional<IntGraphProp> pcgp_parallel(
	std::vector<int>& s_res, 
	int n, int k, int so, 
	IntGraphProp init_prop = IntGraphProp_infty(), 
	const unsigned int n_threads = std::thread::hardware_concurrency()
);


void write_results_csv(
	int n,
	int k,
	const GraphProp& best_prop,
	const std::span<const int> s_res,
	bool header,
	auto&& o_stream
) {
	if (header) {
		o_stream << "N,K,S,diameter,averageShortestPathLength,edges\n";
	}
	const size_t n_graphs = s_res.size() / k;
	for (size_t i = 0; i < n_graphs; ++i) {
		o_stream << n << "," << k << ",";
		for (size_t j = 0; j < k; ++j) {
			o_stream << s_res[i * k + j];
			if (j + 1 < k)
				o_stream << ";";
		}
		o_stream << ",";
		o_stream << std::format("{},{:.16f},", best_prop.diam, best_prop.aspl);
		o_stream << n_circulant_edges(n, std::span( s_res.data() + i * k, k ));
		o_stream << '\n';
	}
};

}