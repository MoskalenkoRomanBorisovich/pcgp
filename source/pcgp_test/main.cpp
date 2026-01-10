#include<pcgp_parallel.hpp>

#include <array>
#include <source_location>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <utility>

[[noreturn]] inline void source_error(std::string_view text, const std::source_location& sl = std::source_location::current()) {
	throw std::logic_error(std::format("error: {}, at {}:{}", text, sl.file_name(), sl.line()));
}


inline void validate(bool expr, const std::source_location& sl = std::source_location::current()) {
	if (!expr) [[unlikely]] {
		source_error("validation failed", sl);
	}
}


inline bool are_close(const GraphProp& a, const GraphProp& b, double tol) {
	if (std::abs(a.aspl - b.aspl) > tol)
		return false;
	if (a.diam != b.diam)
		return false;
	return true;
}

// wrapper for test
GraphProp run_bfs(const Graph& g) {
	Arena arena;
	const size_t mem_size = g.n * 100 * sizeof(int);
	std::unique_ptr<char[]> mem = std::make_unique<char[]>(mem_size);
	arenaInit(&arena, mem.get(), mem_size);
	GraphProp prop;
	circulantBFS(&arena, &prop, &g);
	return prop;
}

// wrapper for test
GraphProp run_bfs_2(const Graph& g) {
	Arena arena;
	const size_t mem_size = g.n * 100 * sizeof(int);
	std::unique_ptr<char[]> mem = std::make_unique<char[]>(mem_size);
	arenaInit(&arena, mem.get(), mem_size);
	GraphProp prop;
	circulantBFS_2(&arena, &prop, &g);
	return prop;
}

// wrapper for test
GraphProp run_bfs_6(const Graph& g) {
	std::unique_ptr<unsigned int[]> dist_buf = std::make_unique<unsigned int[]>(g.n);
	std::unique_ptr<int[]> queue = std::make_unique<int[]>(g.n);
	const IntGraphProp best_prop = IntGraphProp_infty();
	IntGraphProp prop;
	GraphProp res;
	if (circulantBFS_6(dist_buf.get(), queue.get(), &g, &best_prop, &prop))
		calcGraphProp(g.n, &prop, &res);
	else
		GraphProp_infty(&res);
	return res;
}

GraphProp run_bfs(const int n, std::span<int> s) {
	const Graph g{
		.n = n,
		.k = static_cast<int>(s.size()),
		.so = 0,
		.s = s.data()
	};
	return run_bfs(g);
}

void validate_bfs(const int n, std::vector<int> s, const GraphProp& expected_prop, const double tol = 1e-6) {
	const GraphProp res = run_bfs(n, s);
	validate(are_close(res, expected_prop, tol));
}

// test some nkown circulants
void bfs_test1() {
	// results from Optimal circulant graphs as low-latency network topologies
	validate_bfs(32, { 1, 7 }, { 4, 2.71 }, 1e-2); 
	validate_bfs(32, { 1, 6, 16 }, { 3, 2.29 }, 1e-2);
	validate_bfs(64, { 1, 14 }, { 6, 3.78 }, 1e-2);
	validate_bfs(64, { 1, 4, 25}, { 4, 2.60}, 1e-2);

	// results from https://github.com/RomeoMe5/circulantGraphs/blob/master/Dataset/
	validate_bfs(676, { 1, 55, 250 }, { 9, 5.934814814814815 });
	validate_bfs(718, { 1, 132, 350}, { 9, 6.06415620641562 });
	validate_bfs(137, { 1, 5, 14, 53 }, { 4, 2.7794117647058822 });
	validate_bfs(201, { 1, 6, 64, 87}, { 4, 3.12 });
}

// check taht results match
void bfs_test2() {
	constexpr int n_min = 3, n_max = 10;
	constexpr double tol = 1e-6;
	std::unique_ptr<int[]> s_data = std::make_unique<int[]>(n_max);
	for (int n = n_min; n < n_max; ++n) {
		Graph g;
		g.s = s_data.get();
		g.n = n;
		g.so = 0;
		const int k_max = n / 2;
		for (int k = 1; k <= k_max; ++k) {
			g.k = k;
			for (int i = 0; i < k; ++i) {
				g.s[i] = i + 1;
			}
		}
		do {
			const GraphProp p1 = run_bfs(g);
			const GraphProp p2 = run_bfs_2(g);
			const GraphProp p6 = run_bfs_6(g);
			validate(are_close(p1, p2, tol));
			validate(are_close(p1, p6, tol));
		} while (next_lexicographic_step(&g));
	}
}


GraphProp search_prop_only(int n, int k, int so, unsigned int threads) {
	const auto callbalk = [](const unsigned int id, const Graph& g, const IntGraphProp& prop, const std::atomic<IntGraphProp>& best_prop) {
		(void)id;
		(void)g;
		(void)prop;
		(void)best_prop;
	};
	const IntGraphProp min_best_prop = IntGraphProp_infty();
	const auto res = pcgp::parallel_optimal_search_simple(n, k, so, min_best_prop, callbalk, threads);
	validate(!!res);
	GraphProp out;
	calcGraphProp(n, &(*res), &out);
	return out;
}


void search_test1()
{
	constexpr int n_min = 5, n_max = 20;
	for (int n = n_min; n < n_max; ++n) {
		const int k_max = n / 2;
		for (int k = 1; k <= k_max; ++k) {
			const GraphProp res = search_prop_only(n, k, 0, 1);
			const GraphProp res_parallel = search_prop_only(n, k, 0, std::thread::hardware_concurrency());
			validate(are_close(res, res_parallel, 1e-6));
		}
	}
}


void validate_search_props(int n, int k, int so, GraphProp expected_result, double tol = 1e-6) {
	const GraphProp res = search_prop_only(n, k, so, std::thread::hardware_concurrency());
	validate(are_close(res, expected_result, tol));
}


void search_test2()
{
	validate_search_props(718, 3, 1, { 9, 6.06415620641562 });
	validate_search_props(160, 4, 1, { 4, 2.9056603773584904 });
	validate_search_props(279, 4, 1, { 5, 3.4100719424460433 });

	// results from Optimal circulant graphs as low-latency network topologies
	validate_search_props(32, 2, 0, { 4, 2.71 }, 1e-2);
	validate_search_props(64, 2, 0, { 6, 3.78 }, 1e-2);
	validate_search_props(64, 3, 0, { 4, 2.60 }, 1e-2);

	// results from https://github.com/RomeoMe5/circulantGraphs/blob/master/Dataset/
	validate_search_props(676, 3, 0, { 9, 5.934814814814815 });
	validate_search_props(718, 3, 0, { 9, 6.06415620641562 });
	validate_search_props(137, 4, 0, { 4, 2.7794117647058822 });
	validate_search_props(201, 4, 0, { 4, 3.12 });
}


void search_test3() {
	constexpr int min_n = 5, max_n = 20;
	for (int n = min_n; n < max_n; ++n) {
		const int k_max = n / 2;
		for (int k = 1; k <= k_max; ++k) {
			std::vector<int> res;
			const auto res_prop = pcgp::pcgp_parallel(res, n, k, 0, IntGraphProp_infty(), 1);
			validate(!!res_prop);
			validate(!res.empty());
			std::vector<int> res_parallel;
			const auto res_prop_parallel = pcgp::pcgp_parallel(res_parallel, n, k, 0, IntGraphProp_infty(), std::thread::hardware_concurrency());
			validate(!!res_prop_parallel);
			validate(IntGraphProp_equal(&(*res_prop), &(*res_prop_parallel)));
			validate(res_parallel.size() == res.size());
			validate(std::ranges::equal(res, res_parallel));
			std::vector<int> res_pruned;
			const auto res_prop_pruned = pcgp::pcgp_parallel(res_pruned, n, k, 0, *res_prop, std::thread::hardware_concurrency());
			validate(!!res_prop_pruned);
			validate(IntGraphProp_equal(&(*res_prop), &(*res_prop_pruned)));
			validate(std::ranges::equal(res, res_pruned));
		}
	}
}


template<int k>
void validate_search_graphs(int n, int so, GraphProp expected_prop, std::set<std::array<int, k>> expected_graphs, double tol = 1e-6) {
	std::vector<int> res_graphs;
	const auto res_int_prop =pcgp::pcgp_parallel(res_graphs, n, k, so);
	validate(!!res_int_prop);
	GraphProp res_prop;
	calcGraphProp(n, &(*(res_int_prop)), &res_prop);
	validate(are_close(res_prop, expected_prop, tol));

	const size_t n_graphs = res_graphs.size() / k;
	validate(n_graphs == expected_graphs.size());
	for (size_t i = 0; i < n_graphs; ++i) {
		std::array<int, k> res_jumps;
		std::copy(
			res_graphs.begin() + i * k,
			res_graphs.begin() + (i + 1) * k,
			res_jumps.begin()
		);
		validate(expected_graphs.contains(res_jumps));
	}
}


void search_test4() {
	// results from https://github.com/RomeoMe5/circulantGraphs/blob/master/Dataset/
	validate_search_graphs(279, 1, { 5, 3.4100719424460433 }, std::set{
		std::array{1, 33, 96, 110},
		std::array{1, 60, 84, 104},
	});
	validate_search_graphs(125, 1, { 4, 2.693548387096774 }, std::set{
		std::array{1, 9, 20, 34},
		std::array{1, 11, 26, 30},
		std::array{1, 14, 24, 30}
	});
	validate_search_graphs(617, 1, { 9, 5.762987012987013 }, std::set{
		std::array{1, 62, 211},
		std::array{1, 193, 243},
		std::array{1, 209, 292}
	});
	validate_search_graphs(559, 1, { 8, 5.573476702508961 }, std::set{
		std::array{1, 115, 145},
		std::array{1, 155, 266},
		std::array{1, 175, 220}
	});
}


int main() {
	try {
		bfs_test1();
		bfs_test2();
		search_test1();
		search_test2();
		search_test3();
		search_test4();
	}
	catch (const std::exception& e) {
		printf("Error: test_pcgp_parallel failed\n");
		printf(e.what());
		printf("Exception: %s\n", e.what());
		return 1;
	}
	return 0;
}