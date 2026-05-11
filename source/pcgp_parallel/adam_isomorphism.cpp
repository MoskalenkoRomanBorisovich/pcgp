#include "adam_isomorphism.hpp"
#include "pcgp_parallel.hpp"

#include <numeric>
#include <cassert>
#include <algorithm>

namespace pcgp {
void get_gcds(int n, std::span<int> gcd_out)
{
	for (int i = 0; i < gcd_out.size(); ++i) {
		gcd_out[i] = std::gcd(n, i);
	}
}

bool adam_fast_lexi_check(int n, int k, const int* s, const int* gcds)
{
	for (int i = 0; i < k; ++i) {
		const int si = s[i];
		const int d = gcds[si];
		if (d < s[0] || d > n - s[0])
			return false;
	}
	return true;
}

bool adam_full_lexi_check(int n, int k, int np, const int* coprimes, const int* s, int* s_buf)
{
	for (int ip = 0; ip < np; ++ip) {
		const int p = coprimes[ip];
		adam_isomorphiс(n, k, p, s, s_buf);
		const bool is_less = std::lexicographical_compare(s_buf, s_buf + k, s, s + k);
		if (is_less) {
			return false;
		}
	}
	return true;
}


void adam_isomorphiс(int n, int k, int p, int* s)
{
	assert(std::gcd(n, p) == 1);
	for (int i = 0; i < k; ++i) {
		s[i] = (s[i] * p) % n;
	}

	normalize_jumps(n, k, s);
}

void adam_isomorphiс(int n, int k, int p, const int* s, int* s_out)
{
	assert(std::gcd(n, p) == 1);
	for (int i = 0; i < k; ++i) {
		s_out[i] = (s[i] * p) % n;
	}
	normalize_jumps(n, k, s_out);
}

void adam_canon(int n, int k, int np, const int* coprimes, const int* s, int* s_out, int* s_buf)
{
	std::copy(s, s + k, s_out);

	for (int ip = 0; ip < np; ++ip) {
		const int p = coprimes[ip];
		adam_isomorphiс(n, k, p, s, s_buf);
		const bool is_less = std::lexicographical_compare(s_buf, s_buf + k, s_out, s_out + k);
		if (is_less) {
			std::copy(s_buf, s_buf + k, s_out);
		}
	}

}


}
