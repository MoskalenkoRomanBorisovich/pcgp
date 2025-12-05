#include<pcgp_parallel.hpp>

#include <array>
#include <chrono>
#include <source_location>
#include <iostream>


[[noreturn]] inline void source_error(std::string_view text, const std::source_location& sl = std::source_location::current()) {
	throw std::logic_error(std::format("error: {}, at {}:{}", text, sl.file_name(), sl.line()));
}

inline void validate(bool expr, const std::source_location& sl = std::source_location::current()) {
	if (!expr) {
		source_error("validation failed", sl);
	}
}


void test1()
{
	try {
		const auto callbalk = [](const unsigned int id, const Graph& g, const IntGraphProp& prop, const std::atomic<IntGraphProp>& best_prop) {
			(void)id;
			(void)g; 
			(void)prop; 
			(void)best_prop;
		};
		IntGraphProp min_best_prop;
		IntGraphProp_infty(&min_best_prop);
		validate(!!pcgp::parallel_optimal_search_simple(10, 2, 0, min_best_prop, callbalk));
	}
	catch (const std::exception& e) {
		printf("Error: test1 failed\n");
		printf("Exception: %s\n", e.what());
	}
}

void test1_2()
{
	try {
		std::vector<int> s_res_graph;
		constexpr size_t n = 25;
		constexpr size_t k = 3;
		constexpr size_t so = 0;

		std::vector<std::vector<int>> s_results;
		IntGraphProp best_prop;
		IntGraphProp_infty(&best_prop);
		const auto callbalk = [&](const unsigned int id, const Graph& g, const IntGraphProp& prop, const std::atomic<IntGraphProp>& best_prop_) {
			(void)id;
			(void)best_prop_;
			if (IntGraphProp_less(&prop, &best_prop)) {
				s_results.clear();
			}
			s_results.emplace_back(g.s, g.s + k);
		};
		validate(!!pcgp::parallel_optimal_search_simple(n, k, so, best_prop, callbalk));

		for (const auto& s : s_results) {
			printf("Found graph with jumps: ");
			for (const auto& jump : s) {
				printf("%d ", jump);
			}
			printf("\n");
		}
		GraphProp prop;
		calcGraphProp(n, &best_prop, &prop);
		printf("Best graph: diam=%d aspl=%.6f\n", prop.diam, prop.aspl);
	}
	catch (const std::exception& e) {
		printf("Error: test1_2 failed\n");
		printf("Exception: %s\n", e.what());
	}
}

void test_pcgp_parallel(int n, int k, int so)
{
	try {
		const auto start = std::chrono::high_resolution_clock::now();

		std::vector<int> s_res_graph;
		const auto prop = pcgp::pcgp_parallel(s_res_graph, n, k, so);
		validate(!!prop);
		GraphProp gprop;
		calcGraphProp(n, &*prop, &gprop);
		pcgp::write_results_csv(n, k, gprop, s_res_graph, std::cout);

		const auto end = std::chrono::high_resolution_clock::now();
		const std::chrono::duration<double> elapsed = end - start;
		printf("time passed: %.3f seconds\n", elapsed.count());
	}
	catch (const std::exception& e) {
		printf("Error: test_pcgp_parallel failed\n");
		printf("Exception: %s\n", e.what());
	}
}

void test_pcgp_parallel_with_precalc(int n, int k, int so)
{
	try {
		const auto init_graph_prop = pcgp::parallel_optimal_search_simple(
			n,
			k - 1,
			so,
			IntGraphProp_inf(),
			[](const unsigned int id, const Graph& g, const IntGraphProp& prop, const std::atomic<IntGraphProp>& best_prop) {
				(void)id;
				(void)g;
				(void)prop;
				(void)best_prop;
			}
		);
		validate(!!init_graph_prop);
		printf("Initial graph for n=%d k=%d so=%d : dist_sum=%u diam=%u\n", n, k - 1, so, init_graph_prop->dist_sum, init_graph_prop->diam);
		//const IntGraphProp init_graph_prop{.diam = 4, .dist_sum = 716};
		const auto start = std::chrono::high_resolution_clock::now();

		std::vector<int> s_res_graph;
		const auto prop = pcgp::pcgp_parallel(s_res_graph, n, k, so, *init_graph_prop);
		validate(!!prop);
		GraphProp gprop;
		calcGraphProp(n, &*prop, &gprop);
		pcgp::write_results_csv(n, k, gprop, s_res_graph, std::cout);

		const auto end = std::chrono::high_resolution_clock::now();
		const std::chrono::duration<double> elapsed = end - start;
		printf("time passed: %.3f seconds\n", elapsed.count());
	}
	catch (const std::exception& e) {
		printf("Error: test_pcgp_parallel failed\n");
		printf("Exception: %s\n", e.what());
	}
}


std::chrono::duration<double> benchmark(size_t n_runs, auto&& fn){
	auto start = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; i < n_runs; ++i) {
		fn();
	}
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed = end - start;
	elapsed /= static_cast<double>(n_runs);
	return elapsed;
}

void benchmark_parallel_optimal_search(int n, int k, int so, size_t n_runs)
{
	IntGraphProp best_prop;
	IntGraphProp_infty(&best_prop);
	const auto callbalk = [&](const unsigned int id, const Graph& g, const IntGraphProp& prop, const std::atomic<IntGraphProp>& best_prop) {
		(void)id;
		(void)g;
		(void)prop;
		(void)best_prop;
	};
	const size_t n_cores = std::thread::hardware_concurrency();
	printf("Number of hardware threads: %zu\n", n_cores);
	for (unsigned int threads = 4; threads <= n_cores; threads *= 2) {
		const std::chrono::duration<double> elapsed = benchmark(n_runs, [&]() {
			validate(!!pcgp::parallel_optimal_search_simple(n, k, so, best_prop, callbalk, threads));
		});
		printf("Parallel optimal search %u threads time for n=%d k=%d so=%d : %.3f seconds\n", threads, n, k, so, elapsed.count());
	}
}

int main () {
	test1();
	//test1_2();
	//test_pcgp_parallel(64, 2, 1);
	//test_pcgp_parallel(26, 3, 1);
	//test_pcgp_parallel(20, 4, 1);
	test_pcgp_parallel(250, 6, 1);
	test_pcgp_parallel_with_precalc(250, 6, 1);

	//benchmark_parallel_optimal_search(64, 2, 0, 1);
	//benchmark_parallel_optimal_search(128, 2, 0, 1);
	//benchmark_parallel_optimal_search(256, 2, 0, 1);
	//benchmark_parallel_optimal_search(512, 2, 0, 1);
	//benchmark_parallel_optimal_search(1024, 2, 0, 1);
	//benchmark_parallel_optimal_search(2048, 2, 0, 1);

	//benchmark_parallel_optimal_search(64, 3, 0, 1);
	//benchmark_parallel_optimal_search(128, 3, 0, 1);
	//benchmark_parallel_optimal_search(256, 3, 0, 1);
	//benchmark_parallel_optimal_search(512, 3, 0, 1);
	//benchmark_parallel_optimal_search(1024, 3, 0, 1);
	//benchmark_parallel_optimal_search(2048, 3, 0, 1);

	//benchmark_parallel_optimal_search(150, 6, 0, 1);
	//benchmark_parallel_optimal_search(100, 3, 0, 1);
	//benchmark_parallel_optimal_search(100, 6, 0, 1);
	//benchmark_parallel_optimal_search(150, 5, 0, 1);
	//benchmark_parallel_optimal_search(50, 15, 0, 1);
	//benchmark_parallel_optimal_search(200, 5, 0, 1);
	return 0;
}