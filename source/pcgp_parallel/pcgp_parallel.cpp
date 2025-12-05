#include"pcgp_parallel.hpp"

#include<format>

namespace pcgp {
std::optional<IntGraphProp> pcgp_parallel(std::vector<int>& s_res, int n, int k, int so, IntGraphProp init_prop, const unsigned int n_threads) {
	std::vector<std::vector<int>> thread2_s_res(n_threads);
	std::vector<IntGraphProp> thread2_best_prop(n_threads, init_prop);

	// collect best graphs from each thread
	const auto callback = [&](const unsigned int id, const Graph& g, const IntGraphProp& prop, const std::atomic<IntGraphProp>& best_prop_) {
		(void)best_prop_; // not used here
		IntGraphProp& best_prop = thread2_best_prop[id];
		if (IntGraphProp_greater(&prop, &best_prop))
			return;

		std::vector<int>& s_res = thread2_s_res[id];
		if (IntGraphProp_less(&prop, &best_prop)) {
			s_res.clear();
			best_prop = prop;
		}
		for (int i = 0; i < k; ++i) {
			s_res.push_back(g.s[i]);
		}
	};
	// collect results from all threads
	const auto res_prop = parallel_optimal_search_simple(
		n,
		k,
		so,
		init_prop,
		callback,
		n_threads
	);

	if (!res_prop)
		return {};

	const IntGraphProp& best_prop = *res_prop;

	// merge results from all threads
	std::vector<int> res;
	for (unsigned int t = 0; t < n_threads; ++t) {
		assert(!IntGraphProp_less(&thread2_best_prop[t], &best_prop));
		if (!IntGraphProp_equal(&thread2_best_prop[t], &best_prop)) {
			continue;
		}
		const size_t cur_size = res.size();
		res.resize(cur_size+ thread2_s_res[t].size());
		std::copy(
			std::make_move_iterator(thread2_s_res[t].begin()),
			std::make_move_iterator(thread2_s_res[t].end()),
			res.begin() + cur_size
		);
	}

	// sort results
	assert(res.size() % k == 0);
	const size_t n_graphs = res.size() / k;
	std::vector<size_t> order(n_graphs);
	std::iota(order.begin(), order.end(), 0);
	std::sort(std::execution::par_unseq, order.begin(), order.end(), [&](size_t a, size_t b) {
		return std::lexicographical_compare(
			res.begin() + a * k,
			res.begin() + (a + 1) * k,
			res.begin() + b * k,
			res.begin() + (b + 1) * k
		);
	});
	s_res.clear();
	s_res.resize(res.size());
	for (size_t i = 0; i < n_graphs; ++i) {
		const size_t idx = order[i];
		std::copy(
			res.begin() + idx * k,
			res.begin() + (idx + 1) * k,
			s_res.begin() + i * k
		);
	}

	return res_prop;
}

}