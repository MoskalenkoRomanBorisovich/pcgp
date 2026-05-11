#include"pcgp_parallel.hpp"

#include"adam_isomorphism.hpp"

#include<format>
#include<exception>
#include<array>
#include<unordered_set>

namespace {

template<class T>
struct alignas(std::hardware_destructive_interference_size) cache_aligned {
	T value;
	// ergonomic access
	constexpr T& get()							noexcept { return value; }
	constexpr T const& get() const				noexcept { return value; }

	constexpr T* operator->()					noexcept { return &value; }
	constexpr T const* operator->() const		noexcept { return &value; }

	constexpr T& operator*()					noexcept { return value; }
	constexpr T const& operator*() const		noexcept { return value; }
};


}

namespace pcgp {

void normalize_jumps(int n, int k, int* s) {
	const int n_2 = n / 2;
	for (int i = 0; i < k; ++i) {
		if (s[i] > n_2) {
			s[i] = n - s[i];
		}
	}
	std::sort(s, s + k);
	assert(std::ranges::all_of(std::views::iota(0, k - 1), [&](int i) {
		return s[i] != s[i + 1]; 
	}));
}

std::optional<IntGraphProp> calc_single_prop(const Graph& g) {
	auto dist = std::make_unique<unsigned[]>(g.n);
	auto queue = std::make_unique<int[]>(g.n);
	IntGraphProp best_prop = IntGraphProp_infty();
	IntGraphProp prop;
	if (!circulantBFS_6(dist.get(), queue.get(), &g, &best_prop, &prop))
		return {};
	return prop;
}

namespace {


/// @brief searches for circulants with least mean distances and diameters
/// @param n number of nodes
/// @param k number of jumps
/// @param so number of fixed jumps
/// @param start_prop initial guess for best graph props (for optimal BFS pruning)
/// @param found_callback handler of newly found graphs. Takes in id of the thread, graph, properties, best properties over all threads
/// @param num_threads number of concurrent threads to run
/// @return properties of the best found graphs
template<std::invocable<unsigned int, const Graph&, const IntGraphProp&, const std::atomic<IntGraphProp>&> F,
	std::predicate<const Graph&> Fp
>
std::optional<IntGraphProp> parallel_optimal_search_simple(
	int n,
	int k,
	int so,
	const IntGraphProp& start_prop,
	F&& found_callback,
	Fp&& pred = [](const Graph&) {return true; },
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
		try {
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
					if (!pred(g))
						continue;
					IntGraphProp prop;
					IntGraphProp best_prop_local = best_prop.load(std::memory_order_relaxed);
					if (!circulantBFS_8(dist_buf.get(), queue_buf.get(), &g, &best_prop_local, &prop))
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



void sort_s_array(int k, std::span<const int> s_arr, std::span<int> s_res) {
	// sort results
	assert(s_arr.size() % k == 0);
	assert(s_arr.size() == s_res.size());
	const size_t n_graphs = s_arr.size() / k;
	std::vector<size_t> order(n_graphs);
	std::iota(order.begin(), order.end(), 0);
	std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
		return std::lexicographical_compare(
			s_arr.begin() + a * k,
			s_arr.begin() + (a + 1) * k,
			s_arr.begin() + b * k,
			s_arr.begin() + (b + 1) * k
		);
	});
	for (size_t i = 0; i < n_graphs; ++i) {
		const size_t idx = order[i];
		std::copy(
			s_arr.begin() + idx * k,
			s_arr.begin() + (idx + 1) * k,
			s_res.begin() + i * k
		);
	}
}

template<std::predicate<const Graph&> Fp>
[[nodiscard]] std::optional<IntGraphProp> pcgp_parallel_filter(std::vector<int>& s_res, int n, int k, int so, IntGraphProp init_prop, Fp&& predicate, const unsigned int n_threads) {
	s_res.clear();
	if (so == k) {
		s_res.resize(k);
		std::iota(s_res.begin(), s_res.end(), 1);
		return calc_single_prop(n, s_res);
	}
	static_assert(sizeof(cache_aligned<IntGraphProp>) >= std::hardware_destructive_interference_size);

	std::vector<cache_aligned<std::vector<int>>> thread2_s_res(n_threads);
	std::vector<cache_aligned<IntGraphProp>> thread2_best_prop(n_threads, cache_aligned(init_prop));

	for (auto& ts : thread2_s_res) {
		ts->reserve(k);
	}
	// collect best graphs from each thread
	const auto callback = [&](const unsigned int id, const Graph& g, const IntGraphProp& prop, const std::atomic<IntGraphProp>& best_prop_) {
		(void)best_prop_; // not used here
		IntGraphProp& best_prop = thread2_best_prop[id].get();
		if (IntGraphProp_greater(&prop, &best_prop))
			return;

		std::vector<int>& s_res = thread2_s_res[id].get();
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
		predicate,
		n_threads
	);

	if (!res_prop) [[unlikely]]
		return {};

	const IntGraphProp& best_prop = *res_prop;

	// merge results from all threads
	s_res.clear();
	for (unsigned int t = 0; t < n_threads; ++t) {
		assert(!IntGraphProp_less(&*thread2_best_prop[t], &best_prop));
		if (!IntGraphProp_equal(&*thread2_best_prop[t], &best_prop)) {
			continue;
		}
		const size_t cur_size = s_res.size();
		s_res.resize(cur_size + thread2_s_res[t].get().size());
		std::copy(
			std::make_move_iterator(thread2_s_res[t].get().begin()),
			std::make_move_iterator(thread2_s_res[t].get().end()),
			s_res.begin() + cur_size
		);
	}

	// sort results
	return res_prop;
}

struct jumps_hash {
	constexpr static size_t operator()(const std::span<const int> s) {
		std::size_t seed = s.size();
		for (auto& i : s) {
			seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}
		return seed;
	}
};
struct jump_equal {
	constexpr static bool operator()(const std::span<const int> s1, const std::span<const int> s2) {
		if (s1.size() != s2.size())
			return false;
		return std::equal(s1.begin(), s1.end(), s2.begin());
	}
};

}

std::optional<IntGraphProp> pcgp_prop(
	int n, int k, int so,
	IntGraphProp init_prop,
	const unsigned int n_threads
) {
	return parallel_optimal_search_simple(n, k, so, init_prop, [](auto&&, auto&&, auto&&, auto&&) {return true; }, [](auto&&) {return true; }, n_threads);
}

std::optional<IntGraphProp> pcgp_parallel(std::vector<int>& s_res, int n, int k, int so, IntGraphProp init_prop, const unsigned int n_threads) {
	std::vector<int> s_local;
	const auto res_local = pcgp_parallel_filter(s_local, n, k, so, init_prop, [](auto&&) {return true; }, n_threads);
	if (!res_local) [[unlikely]]
		return {};
	s_res.resize(s_local.size());
	sort_s_array(k, s_local, s_res);
	return res_local;
}


std::optional<IntGraphProp> pcgp_parallel_isomorph(std::vector<int>& s_res, int n, int k, int so, IntGraphProp init_prop, const unsigned int n_threads) {
	std::vector<int> gcds(n);
	get_gcds(n, gcds);
	const auto pred = [&gcds](const Graph& g) -> bool {
		return adam_fast_lexi_check(g.n, g.k, g.s, gcds.data());
	};
	std::vector<int> s_local;
	const auto res_local = pcgp_parallel_filter(s_local, n, k, so, init_prop, pred, n_threads);
	if (!res_local) [[unlikely]]
		return {};
	
	std::unordered_set<std::vector<int>, jumps_hash, jump_equal> found_s;

	const size_t n_graphs = s_local.size() / k;
	for (int i = 0; i < n_graphs; ++i){
		std::span<const int> s_span{ s_local.begin() + i * k, (size_t)k };
		const bool success = found_s.emplace(s_span.begin(), s_span.end()).second;
		assert(success);
	}

	std::vector<int> coprime;
	for (int i = 2; i < n; ++i) {
		if (gcds[i] == 1) {
			coprime.push_back(i);
		}
	}

	std::vector<int> s_isomorph(k);
	for (int i = 0; i < n_graphs; ++i) {
		for (const int p : coprime) {
			std::span<const int> s_span{ s_local.begin() + i * k, (size_t)k };
			adam_isomorphiс(n, k, p, s_span.data(), s_isomorph.data());
			
			const bool so_match = std::ranges::all_of(std::views::iota(0, so), [&](int so_i) {
				return s_isomorph[so_i] == so_i + 1; 
			});
			if (!so_match)
				continue;

			if (found_s.contains(s_isomorph))
				continue;

			const auto s_begin = s_local.insert(s_local.end(), s_isomorph.begin(), s_isomorph.end());
			found_s.emplace(s_begin, s_begin + k);
		}
	}

	s_res.resize(s_local.size());
	sort_s_array(k, s_local, s_res);
	return res_local;
}



std::optional<IntGraphProp> pcgp_parallel_isomorph_full(std::vector<int>& s_res, int n, int k, int so, IntGraphProp init_prop, const unsigned int n_threads) {
	std::vector<int> gcds(n);
	get_gcds(n, gcds);

	std::vector<int> coprime;
	for (int i = 2; i < n; ++i) {
		if (gcds[i] == 1) {
			coprime.push_back(i);
		}
	}
	
	const auto pred = [&coprime, &k](const Graph& g) -> bool {
		thread_local std::vector<int> s_buf(k);
		return adam_full_lexi_check(g.n, g.k, coprime.size(), coprime.data(), g.s, s_buf.data());
	};
	std::vector<int> s_local;
	const auto res_local = pcgp_parallel_filter(s_local, n, k, so, init_prop, pred, n_threads);
	if (!res_local) [[unlikely]]
		return {};

	std::unordered_set<std::vector<int>, jumps_hash, jump_equal> found_s;

	const size_t n_graphs = s_local.size() / k;
	for (int i = 0; i < n_graphs; ++i) {
		std::span<const int> s_span{ s_local.begin() + i * k, (size_t)k };
		const bool success = found_s.emplace(s_span.begin(), s_span.end()).second;
		assert(success);
	}


	std::vector<int> s_isomorph(k);
	for (int i = 0; i < n_graphs; ++i) {
		for (const int p : coprime) {
			std::span<const int> s_span{ s_local.begin() + i * k, (size_t)k };
			adam_isomorphiс(n, k, p, s_span.data(), s_isomorph.data());

			const bool so_match = std::ranges::all_of(std::views::iota(0, so), [&](int so_i) {
				return s_isomorph[so_i] == so_i + 1;
			});
			if (!so_match)
				continue;

			if (found_s.contains(s_isomorph))
				continue;

			const auto s_begin = s_local.insert(s_local.end(), s_isomorph.begin(), s_isomorph.end());
			found_s.emplace(s_begin, s_begin + k);
		}
	}

	s_res.resize(s_local.size());
	sort_s_array(k, s_local, s_res);
	return res_local;
}

std::optional<IntGraphProp> pcgp_parallel_pred(
	std::vector<int>& s_res,
	int n, int k, int so,
	std::function<bool(const Graph&)> pred,
	IntGraphProp init_prop,
	const unsigned int n_threads 
){
	std::vector<int> s_local;
	const auto res_local = pcgp_parallel_filter(s_local, n, k, so, init_prop, pred, n_threads);
	if (!res_local) [[unlikely]]
		return {};
	s_res.resize(s_local.size());
	sort_s_array(k, s_local, s_res);
	return res_local;

}


}