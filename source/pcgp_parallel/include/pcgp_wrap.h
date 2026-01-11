#pragma once

extern "C" {
#include <pcgp.h>
}

inline IntGraphProp IntGraphProp_infty() noexcept {
	IntGraphProp prop;
	IntGraphProp_infty(&prop);
	return prop;
}

inline int n_circulant_edges(int n, const std::span<const int> s) {
	const int k = static_cast<int>(s.size());
	if (k == 0 || n == 0) [[unlikely]]
		return 0;	
	if (n % 2 == 1)
		return n * k;

	const bool has_half = s.back() == n / 2;
	if (!has_half)
		return n * k;
	
	return n * k - n / 2;
}