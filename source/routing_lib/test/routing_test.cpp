#include <routing.h>

#include <pcgp_parallel.hpp>

#include <array>
#include <source_location>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <utility>
#include <format>
#include <ranges>
#include <random>
#include <numeric>

namespace {


[[noreturn]] inline void source_error(std::string_view text, const std::source_location& sl = std::source_location::current()) {
	throw std::logic_error(std::format("error: {}, at {}:{}", text, sl.file_name(), sl.line()));
}

inline void validate(bool expr, const std::source_location& sl = std::source_location::current()) {
	if (!expr) {
		source_error("validation failed", sl);
	}
}

}


namespace routing {
void simple_routing_validation(auto&& routing_fn, int n, std::span<const int> s) {
	const int k = static_cast<int>(s.size());
	for (int i = 0; i < n; ++i) {
		for (int sid = 0; sid < k; ++sid) {
			const int si = s[sid];
			validate(si < n && si >= 0);
			for (int dir : {1, -1}) {
				int j = i + dir * si;
				if (j < 0)
					j += n;
				if (j >= n)
					j -= n;

				const int res = routing_fn(n, s, j, i);
				validate(i == res);
			}
		}
	}
}


void clockwise_test1() {
	constexpr int n_min = 3, n_max = 8;
	for (int n = n_min; n < n_max; ++n) {
		const int k_max = n / 2;
		for (int k = 1; k <= k_max; ++k) {
			std::vector<int> s(k);
			for (int si = 0; si < k; ++si) {
				s[si] = si + 1;
			}
			simple_routing_validation(clockwise_next_step, n, s);
		}
	}
}

void simple_zero_tree_validation(std::span<const int> s, std::span<const int> tree) {
	const int n = static_cast<int>(tree.size());
	for (int si : s) {
		validate(tree[si] == 0);
		validate(tree[n - si] == 0);
	}
}

void clockwise_test2() {
	constexpr int n_min = 3, n_max = 8;
	for (int n = n_min; n < n_max; ++n) {
		const int k_max = n / 2;
		std::vector<int> tree(n);
		for (int k = 1; k <= k_max; ++k) {
			std::vector<int> s(k);
			for (int si = 0; si < k; ++si) {
				s[si] = si + 1;
			}
			validate(map_jumps_to_zero(n, s, clockwise_next_step, tree.data()));
			simple_zero_tree_validation(s, tree);
		}
	}
}

void clockwise_validate_diam(int n, std::span<const int> s, unsigned cw_diam, unsigned acw_diam) {
	(void)acw_diam;
	{
		const auto prop = circulant_routing_stats(n, s, clockwise_next_step);
		validate(!!prop);
		validate(prop->diam == cw_diam);
	}
	//{
	//	const auto prop = circulant_routing_stats(n, s, advanced_clockwise_next_step);
	//	validate(!!prop);
	//	validate(prop->diam == acw_diam);
	//}
}

void prop_test() {
	clockwise_validate_diam(9, std::vector{ 1, 3, 9 }, 2, 2);
	clockwise_validate_diam(16, std::vector{ 1, 4, 8 }, 4, 3);
	clockwise_validate_diam(25, std::vector{ 1, 6, 10 }, 5, 4);
	clockwise_validate_diam(36, std::vector{ 1, 8, 15 }, 7, 5);
}


void test_tree_dist(int n, std::span<const int> tree, const std::span<const int> correct_dist) {
	std::vector<int> dist(n);
	calc_tree_dist(n, tree.data(), dist.data());
	validate(std::ranges::equal(dist, correct_dist));
}

void generate_tree(std::span<int> tree, std::default_random_engine& rng)
{
	const int n = static_cast<int>(tree.size());
	tree[0] = 0; // root


	for (int i = 1; i < n; ++i) {
		std::uniform_int_distribution<int> dist(0, i - 1);
		tree[i] = dist(rng);   // parent < i → no cycles
	}
}


int simple_tree_dist(std::span<const int> tree, int i) {
	const int n = static_cast<int>(tree.size());
	for (int dist = 0; dist < n; ++dist){
		if (i == 0) {
			return dist;
		}
		i = tree[i];
	}
	validate(false);
	return 0;
}

void validate_tree_dist(std::span<const int> tree, std::span<const int> dist) {
	const int n = static_cast<int>(tree.size());
	for (int i = 0; i < n; ++i) {
		const int true_dist = simple_tree_dist(tree, i);
		const int d = dist[i];
		validate(d == true_dist);
	}
}


void tree_dist_test1() {
	constexpr int n_runs = 100;
	constexpr int n = 100;
	std::default_random_engine rng(1234567);
	std::vector<int> tree(n);
	std::vector<int> dist(n);
	for (int run = 0; run < n_runs; ++run) {
		generate_tree(tree, rng);
		calc_tree_dist(n, tree.data(), dist.data());
		validate_tree_dist(tree, dist);
	}
}




void search_test1(int n, int k, int so) {
	const auto [intprop, s_arr] = search_for_best_graph(n, k, so, clockwise_next_step);

	GraphProp prop;
	calcGraphProp(n, &intprop, &prop);

	std::cout << "true routing res" << std::endl;
	pcgp::write_results_csv(n, k, prop, s_arr, true, std::cout);

	std::vector<int> pcgp_s;
	const auto pcgp_res = pcgp::pcgp_parallel(pcgp_s, n, k, so);
	validate(!!pcgp_res);
	GraphProp pcgp_prop;
	calcGraphProp(n, &(*pcgp_res), &pcgp_prop);
	std::cout << "pcgp res" << std::endl;
	pcgp::write_results_csv(n, k, pcgp_prop, pcgp_s, true, std::cout);

	std::cout << "routing on pcgp" << std::endl;
	const int n_graphs = static_cast<int>(pcgp_s.size()) / k;
	for (int i = 0; i < n_graphs; ++i) {
		const std::optional<IntGraphProp> r_prop_ = circulant_routing_stats(n, std::span(pcgp_s.begin() + i * k, k), clockwise_next_step);
		if (!r_prop_) {
			std::cout << "graph " << i << " is unroutable\n";
			continue;
		}
		const IntGraphProp& r_prop = *r_prop_;
		GraphProp p;
		calcGraphProp(n, &r_prop, &p);
		std::cout << "graph " << i << " diam: " << p.diam << " aspl: " << p.aspl << std::endl;
	}
}

void explore_best_rounting_graphs(int n, int k, int so, const auto&& routing_fn
	, IntGraphProp& optimal_on_optimal, IntGraphProp& routing_on_optimal, IntGraphProp& routing_on_routing) {
	// find optimal graph
	std::vector<int> pcgp_s;
	const auto pcgp_res = pcgp::pcgp_parallel(pcgp_s, n, k, so);
	validate(!!pcgp_res);
	optimal_on_optimal = *pcgp_res;

	const auto [intprop, s_arr] = search_for_best_graph(n, k, so, routing_fn);
	routing_on_routing = intprop;

	const int n_graphs = static_cast<int>(pcgp_s.size()) / k;
	IntGraphProp best_rounting_on_optimal = IntGraphProp_infty();
	for (int i = 0; i < n_graphs; ++i) {
		const std::optional<IntGraphProp> r_prop_ = circulant_routing_stats(n, std::span(pcgp_s.begin() + i * k, k), routing_fn);
		if (!r_prop_) {
			continue;
		}
		const IntGraphProp& r_prop = *r_prop_;
		if (IntGraphProp_less(&r_prop, &best_rounting_on_optimal)) {
			best_rounting_on_optimal = r_prop;
		}
	}
	routing_on_optimal = best_rounting_on_optimal;
}

void print_explore_clockwise_routing_header() {
	std::cout << "n ; k ; opt_opt ; cw_on_opt ; cw_on_cw ; acw_on_opt ; acw_on_acw\n";
}

void explore_clockwise_routings(int n, int k, int so) {
	IntGraphProp opt_opt, cw_opt, cw_cw, acw_opt, acw_acw;
	explore_best_rounting_graphs(n, k, so, clockwise_next_step, opt_opt, cw_opt, cw_cw);
	explore_best_rounting_graphs(n, k, so, advanced_clockwise_next_step, opt_opt, acw_opt, acw_acw);
	
	GraphProp o_o, c_o, c_c, ac_o, ac_ac;
	calcGraphProp(n, &opt_opt, &o_o);
	calcGraphProp(n, &cw_opt, &c_o);
	calcGraphProp(n, &cw_cw, &c_c);
	calcGraphProp(n, &acw_opt, &ac_o);
	calcGraphProp(n, &acw_acw, &ac_ac);

	std::cout << std::format("{} ; {} ; {} ; {} ; {} ; {} ; {}\n", n, k, o_o.aspl, c_o.aspl, c_c.aspl, ac_o.aspl, ac_ac.aspl);
}

void print_explore_second_order_rouing() {
	std::cout << "n ; k ; opt_opt ; adapt_on_opt ; adapt_on_adapt\n";
}

void explore_second_order_rouing(int n, int so) {
	constexpr int k = 2;
	IntGraphProp opt_opt, ad_opt, ad_ad;
	explore_best_rounting_graphs(n, k, so, adaptive_next_step_wrap, opt_opt, ad_opt, ad_ad);

	GraphProp o_o, a_o, a_a;
	calcGraphProp(n, &opt_opt, &o_o);
	calcGraphProp(n, &ad_opt, &a_o);
	calcGraphProp(n, &ad_ad, &a_a);
	std::cout << std::format("{} ; {} ; {} ; {} ; {}\n", n, k, o_o.aspl, a_o.aspl, a_a.aspl);
}

void print_explore_third_order_rouing() {
	std::cout << "n ; k ; opt_opt ; dir_select_on_opt ; dir_select_on_dir_select\n";
}

void explore_third_order_rouing(int n, int so) {
	constexpr int k = 3;
	IntGraphProp opt_opt, ad_opt, ad_ad;
	explore_best_rounting_graphs(n, k, so, direction_selection_next_step_wrap, opt_opt, ad_opt, ad_ad);

	GraphProp o_o, a_o, a_a;
	calcGraphProp(n, &opt_opt, &o_o);
	calcGraphProp(n, &ad_opt, &a_o);
	calcGraphProp(n, &ad_ad, &a_a);
	std::cout << std::format("{} ; {} ; {} ; {} ; {}\n", n, k, o_o.aspl, a_o.aspl, a_a.aspl);
}



void search_for_optimal_graphs(int n, int k, int so) {
	// find optimal graph
	std::vector<int> pcgp_s;
	const auto pcgp_res = pcgp::pcgp_parallel(pcgp_s, n, k, so);
	validate(!!pcgp_res);
	GraphProp pcgp_prop;
	calcGraphProp(n, &(*pcgp_res), &pcgp_prop);
	{ // proces clockwise routing
		const auto [intprop, s_arr] = search_for_best_graph(n, k, so, clockwise_next_step);

		GraphProp prop;
		calcGraphProp(n, &intprop, &prop);

		std::cout << "\"clockwise\" routing specific results" << std::endl;
		pcgp::write_results_csv(n, k, prop, s_arr, true, std::cout);
	}
	{ // process advanced clockwise routing
		const auto [intprop, s_arr] = search_for_best_graph(n, k, so, advanced_clockwise_next_step);
		GraphProp prop;
		calcGraphProp(n, &intprop, &prop);
		std::cout << "\"advanced clockwise\" routing specific results" << std::endl;
		pcgp::write_results_csv(n, k, prop, s_arr, true, std::cout);

	}
	std::cout << "pcgp res" << std::endl;
	pcgp::write_results_csv(n, k, pcgp_prop, pcgp_s, true, std::cout);

	std::cout << "routing on pcgp" << std::endl;
	const int n_graphs = static_cast<int>(pcgp_s.size()) / k;
	for (int i = 0; i < n_graphs; ++i) {
		const std::optional<IntGraphProp> r_prop_ = circulant_routing_stats(n, std::span(pcgp_s.begin() + i * k, k), clockwise_next_step);
		if (!r_prop_) {
			std::cout << "graph " << i << " is unroutable\n";
			continue;
		}
		const IntGraphProp& r_prop = *r_prop_;
		GraphProp p;
		calcGraphProp(n, &r_prop, &p);
		std::cout << "graph " << i << " diam: " << p.diam << " aspl: " << p.aspl << std::endl;
	}
}

void test_blocked_table(int n, std::vector<int> s) {
	const int k = static_cast<int>(s.size());
	std::vector<unsigned> dist(n);
	std::vector<int> queue(n);
	Graph g;
	g.n = n;
	g.k = k;
	g.s = s.data();
	g.so = 0;
	IntGraphProp prop;
	const bool success = circulantBFS_7(dist.data(), queue.data(), &g, &prop);
	validate(success);
	blocked_table bt;
	std::vector<int> block_end_data(n);
	std::vector<int> block_v_data(n);
	bt.block_end = block_end_data.data();
	bt.block_v_next = block_v_data.data();
	blocked_table_routing(n, k, s.data(), dist.data(), bt);
	std::cout << bt.n_blocks << std::endl;
	std::cout << "block_end next_vert" << std::endl;
	for (int block = 0; block < bt.n_blocks; ++block) {
		std::cout << bt.block_end[block] << " " << bt.block_v_next[block] << std::endl;
	}
}

void test_blocked_table(int n, int k) {

	std::vector<int> s_res;
	const auto prop_ = pcgp::pcgp_parallel(s_res, n, k, 0, IntGraphProp_infty());
	validate(!!prop_);
	const size_t n_graphs = s_res.size() / k;
	std::vector<unsigned> dist(n);
	std::vector<int> queue(n);
	std::vector<int> block_end_data(n);
	std::vector<int> block_v_data(n);
	int min_blocks_size = std::numeric_limits<int>::max();
	int best_graph_id = 0;
	for (int graph_id = 0; graph_id < n_graphs; ++graph_id) {
		Graph g;
		g.n = n;
		g.k = k;
		g.s = s_res.data() + k * graph_id;
		g.so = 0;
		IntGraphProp prop;
		const bool success = circulantBFS_7(dist.data(), queue.data(), &g, &prop);
		validate(success);
		blocked_table bt;
		bt.block_end = block_end_data.data();
		bt.block_v_next = block_v_data.data();
		blocked_table_routing(n, k, g.s, dist.data(), bt);
		if (bt.n_blocks < min_blocks_size) {
			min_blocks_size = bt.n_blocks;
			best_graph_id = graph_id;
		}
	}
	pcgp::write_signature(n, std::span(s_res.begin() + k * best_graph_id, k), std::cout);
	std::cout << " min_block_count: " << min_blocks_size << std::endl;
}


void find_best_aspl_to_block_size_ratio_graph(int n, int k, int n_blocks_required) {
	std::vector<int> s_data(k);
	Graph g;
	g.n = n;
	g.k = k;
	g.s = s_data.data();
	g.so = 0;
	std::iota(s_data.begin(), s_data.end(), 1);

	std::vector<unsigned int> dist(n);
	std::vector<int> queue(n);
	std::vector<int> block_end_data(n);
	std::vector<int> block_v_data(n);


	int block_best = std::numeric_limits<int>::max();
	IntGraphProp prop_best;
	int best_total = std::numeric_limits<int>::max();
	std::vector<int> s_best(k);
	do {
		IntGraphProp prop;
		const bool success = circulantBFS_7(dist.data(), queue.data(), &g, &prop);
		if (!success)
			continue;
		
		blocked_table bt;
		bt.block_end = block_end_data.data();
		bt.block_v_next = block_v_data.data();
		blocked_table_routing(n, k, g.s, dist.data(), bt);
		if (n_blocks_required != bt.n_blocks)
			continue;
		const int total = prop.dist_sum;
		if (total < best_total) {
			best_total = total;
			prop_best = prop;
			block_best = bt.n_blocks;
			std::copy(s_data.begin(), s_data.end(), s_best.begin());
		}
	} while (next_lexicographic_step(&g));

	GraphProp p;
	calcGraphProp(n, &prop_best, &p);
	std::vector<int> s_res;
	const auto prop_opt = pcgp::pcgp_parallel(s_res, n, k, 0, IntGraphProp_infty());
	validate(!!prop_opt);
	GraphProp p_opt;
	calcGraphProp(n, &*prop_opt, &p_opt);

	const auto [intprop, _] = search_for_best_graph(n, k, 0, clockwise_next_step);
	GraphProp p_clock;
	calcGraphProp(n, &intprop, &p_clock);

	
	//std::cout << "best_blocked: ";
	//pcgp::write_signature(n, s_best, std::cout);
	std::cout << std::format(", n_blocks: {:2}, block_aspl: {:4.3}", block_best, p.aspl);
	std::cout << std::format(", optimal_aspl: {:4.3}", p_opt.aspl);
	std::cout << std::format(", clock_aspl: {:4.3}", p_clock.aspl);
	std::cout << std::endl;
}

}

int main() {
	try {
		routing::clockwise_test1();
		routing::tree_dist_test1();
		routing::clockwise_test2();
		routing::prop_test();
		routing::search_test1(49, 3, 1);
		//for (int k : {2, 3, 4}) {
		//	for (int n = 10; n < 50; ++n) {
		//		std::cout << "n: " << n << ", k:" << k;
		//		routing::find_best_aspl_to_block_size_ratio_graph(n, k, std::max(8, k * 2));
		//		//routing::test_blocked_table(n, k);
		//	}
		//	std::cout << "\n";
		//}
		{ // clockwise 
			routing::print_explore_clockwise_routing_header();
			for (int n = 10; n <= 100; n += 10) {
				routing::explore_clockwise_routings(n, 2, 1);
			}
			for (int n = 10; n <= 100; n += 10) {
				routing::explore_clockwise_routings(n, 3, 1);
			}
			for (int n = 10; n <= 100; n += 10) {
				routing::explore_clockwise_routings(n, 4, 1);
			}
			for (int n = 10; n <= 100; n += 10) {
				routing::explore_clockwise_routings(n, 5, 1);
			}
		}
		//{ // second order
		//	routing::print_explore_second_order_rouing();
		//	for (int n = 10; n <= 100; n += 10) {
		//		routing::explore_second_order_rouing(n, 1);
		//	}
		//}
		//{ // third order
		//	routing::print_explore_third_order_rouing();
		//	for (int n = 10; n <= 100; n += 10) {
		//		routing::explore_third_order_rouing(n, 1);
		//	}
		//}
	}
	catch (const std::exception& e) {
		printf("Error: test_pcgp_parallel failed\n");
		printf(e.what());
		printf("Exception: %s\n", e.what());
		return 1;
	}
}
