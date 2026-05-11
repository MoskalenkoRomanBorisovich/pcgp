#pragma once

#include <pcgp_wrap.h>
#include <span>

namespace pcgp{

/// @brief compute all gcd with all numbers from 0 to gcd_out.size()
/// @param n 
/// @param gcd_out 
void get_gcds(int n, std::span<int> gcd_out);


bool adam_fast_lexi_check(int n, int k, const int* s, const int* gcds);
bool adam_full_lexi_check(int n, int k, int np, const int* coprimes, const int* s, int* s_buf);

void adam_isomorphiс(int n, int k, int p, int* s);
void adam_isomorphiс(int n, int k, int p, const int* s, int* s_out);

void adam_canon(int n, int k, int np, const int* coprimes, const int *s, int* s_out, int* s_buf);


}