#include "routing.h"

#include <assert.h>
#include <limits>


#include <pcgp_parallel.hpp>


namespace routing {

int clockwise_next_step(int n, std::span<const int> s, int i, int j)
{
	assert(0 <= i && i < n);
	assert(0 <= j && j < n);

	int d = j - i;
	if (d == 0)
		return i;
	if (d < 0)
		d += n;
	const unsigned int k = static_cast<int>(s.size());

	if (d <= n / 2) {
		for (unsigned int sid = 1; sid <= k; ++sid) {
			const int si = s[k - sid];
			if (d < si)
				continue;
			int res = i + si;
			if (res >= n)
				res -= n;
			return res;
		}
	}
	else {
		d = n - d;
		for (unsigned int sid = 1; sid <= k; ++sid) {
			const int si = s[k - sid];
			if (d < si)
				continue;
			int res = i - si;
			if (res < 0)
				res += n;
			return res;
		}
	}
	return -1;
}

int advanced_clockwise_next_step(int n, std::span<const int> s, int i, int j)
{
	assert(0 <= i && i < n);
	assert(0 <= j && j < n);
	const int k = static_cast<int>(s.size());
	if (k == 0) [[unlikely]]
		return -1;

	int d = normalize_vert_fast(j - i, n);
	int sign = 1; // fast enough for me
	if (d > n / 2) {
		d = n - d;
		sign = -1; 
	}
	const int d2 = d + d;
	for (int id = k - 2; id >= 0; --id) {
		const int s_min = s[id];
		const int s_max = s[id + 1];
		if (d2 <= s_min + s_max)
			continue;

		int res = i + sign * s_max;
		res = normalize_vert_fast(res, n);
		return res;
	}

	int res = i + sign * s[0];
	res = normalize_vert_fast(res, n);
	return res;
}



void calc_tree_dist(int n, const int* tree, int* dist) {
	std::fill_n(dist, n, -1);
	dist[0] = 0;   // root

	for (int i = 1; i < n; ++i) {

		if (dist[i] != -1)
			continue;

		int v = i;
		int depth = 0;
		// climb up until known
		bool reached_max_iter = true;
		for (int iter = 0; iter <= n; ++iter) {
			if (dist[v] != -1) {
				reached_max_iter = false;
				break;
			}
			v = tree[v];
			++depth;
		}
		if (reached_max_iter) [[unlikely]]
			continue; // root is unreachable from current vertex
		depth += dist[v];

		// assign back
		v = i;
		while (dist[v] == -1) {
			dist[v] = depth--;
			v = tree[v];
		}
	}
}

IntGraphProp calc_graph_stats(std::span<const int> dist)
{
	unsigned int sum = 0;
	unsigned int diam = 0;
	for (unsigned int d : dist) {
		sum += static_cast<unsigned>(d);
		diam = std::max(static_cast<unsigned>(d), diam);
	}
	return IntGraphProp{ .diam = diam, .dist_sum = sum};
}


int count_blocks(int n, int k, const int* s, const unsigned* dist)
{
	int n_blocks = 0;
	int cur = 1;
	while (cur < n) {
		int best_block_size = 0;
		int best_jump = 0;
		// iterate over all possible jumps
		for (int i = 0; i < k; ++i) for (int side : {-1, 1}) {
			const int jump = side * s[i];
			int block_size = n - cur;
			for (int next = cur; next < n; ++next) {
				const int nei = normalize_vert_fast(next + jump, n);
				assert(0 <= nei && nei < n);
				unsigned next_dist = dist[next];
				unsigned nei_dist = dist[nei];
				if (nei_dist >= next_dist) {
					block_size = next - cur;
					break;
				}
			}
			if (block_size > best_block_size) {
				best_block_size = block_size;
				best_jump = jump;
			}
		}
		assert(best_block_size != 0);
		cur += best_block_size;
		n_blocks++;
	}
	return n_blocks;
}

void blocked_table_routing(int n, int k, const int* s, const unsigned* dist, blocked_table& out)
{
	out.n_blocks = 0;
	int cur = 1;
	while (cur < n) {
		int best_block_size = 0;
		int best_jump = 0;
		// iterate over all possible jumps
		for (int i = 0; i < k; ++i) for (int side : {-1, 1}) {
			const int jump = side * s[i];
			int block_size = n - cur;
			for (int next = cur; next < n; ++next){
				const int nei = normalize_vert_fast(next + jump, n);
				assert(0 <= nei && nei < n);
				unsigned next_dist = dist[next];
				unsigned nei_dist = dist[nei];
				if (nei_dist >= next_dist) {
					block_size = next - cur;
					break;
				}
			}
			if (block_size > best_block_size) {
				best_block_size = block_size;
				best_jump = jump;
			}
		}
		assert(best_block_size != 0);
		cur += best_block_size;
		out.block_v_next[out.n_blocks] = normalize_vert_fast(-best_jump, n);
		out.block_end[out.n_blocks] = cur;
		out.n_blocks++;
	}
}


namespace {
int Step_Cycles(int startNode, int endNode, int N, int s1, int s2) {
	int bestWayR = 0;
	int stepR = 0;
	int bestWayL = 0;
	int stepL = 0;
	int S = endNode - startNode;
	const int R1 = S / s2 + (S % s2);
	const int R2 = S / s2 - (S % s2) + s2 + 1;
	if (S % s2 == 0) {
		bestWayR = R1;
		stepR = s2;
	}
	else {
		if (R1 < R2) {
			bestWayR = R1;
			stepR = s1;
		}
		else {
			bestWayR = R2;
			stepR = s2;
		}
	}
	const int R5 = (S + N) / s2 + ((S + N) % s2);
	const int R6 = (S + N) / s2 - ((S + N) % s2) + s2 + 1;

	if (R5 < bestWayR) {
		bestWayR = R5;
		stepR = s2;
	}
	if (R6 < bestWayR) {
		bestWayR = R6;
		stepR = s2;
	}
	const int R9  = (S + N + N) / s2 + ((S + N + N) % s2);
	const int R10 = (S + N + N) / s2 + ((S + N + N) % s2) + s2 + 1;
	if (R9 < bestWayR) {
		bestWayR = R9;
		stepR = s2;
	}
	if (R10 < bestWayR) {
		bestWayR = R10;
		stepR = s2;
	}
	S = endNode - startNode + N;
	const int L1 = S / s2 + (S % s2);
	const int L2 = S / s2 - (S % s2) + s2 + 1;
	if (S % s2 == 0) {
		bestWayL = L1;
		stepL = -s2;
	}
	else {
		if (L1 < L2) {
			bestWayL = L1;
			stepL = -s1;
		}
		else {
			bestWayL = L2;
			stepL = -s2;
		}
	}
	const int R7 = (S + N) / s2 + ((S + N) % s2);
	const int R8 = (S + N) / s2 + ((S + N) % s2) + s2 + s2 + 1;

	if (R7 < bestWayL) {
		bestWayL = R7;
		stepL = -s2;
	}
	if (R8 < bestWayL) {
		bestWayL = R8;
		stepL = -s2;
	}
	const int R11 = (S + N + N) / s2 + ((S + N + N) % s2);
	const int R12 = (S + N + N) / s2 - ((S + N + N) % s2) + s2 + 1;

	if (R11 < bestWayL) {
		bestWayL = R11;
		stepL = -s2;
	}
	if (R12 < bestWayL) {
		bestWayL = R12;
		stepL = -s2;
	}

	if (bestWayR < bestWayL)
		return stepR;
	
	return stepL;
}

}

int adaptive_next_step(int startNode, int endNode, int N, int s1, int s2) {
	startNode += 1; // original algorithm's indices start from 1
	endNode += 1;
	if (startNode > endNode) {
		startNode = startNode - Step_Cycles(endNode, startNode, N, s1, s2);
	}
	else {
		startNode = startNode + Step_Cycles(startNode, endNode, N, s1, s2);
	}

	if (startNode > N) {
		startNode = startNode - N;
	}
	else if (startNode <= 0) {
		startNode = startNode + N;
	}

	startNode -= 1; // return to 0 based indexing
	return startNode ;
}

namespace {

int direction_selection_internal_step(int startNode, int endNode, int N, int s1, int s2, int s3) {
	int bestTurnR = 0;
	int stepR = 0;
	int bestTurnL = 0;
	int stepL = 0;
	int S = endNode - startNode;

	const int R1 = (S - 1) / s2 + S % N;
	const int R2 = (S - 1) / s2 - S % s2 + s2 + 1;
	int R3 = (S - 1) / s2 - (s2 * (S / s2 + 1) - S) + s2 + 1;
	const int R4 = (S - 1) / s3 - S % s3 + s3 + 1;
	int R5 = (S - 1) / s3 - (s3 * (S / s3 + 1) - S) + s3 + 1;
	if (R3 > R2)
		R3 = R2;
	if (R5 > R4)
		R5 = R4;

	if (R1 < R3) {
		if (R1 < R5) {
			bestTurnR = R1;
			stepR = s1;
		}
		else {
			bestTurnR = R5;
			stepR = s3;
		}
	}
	else if (R3 < R5) {
		bestTurnR = R3;
		stepR = s2;
	}
	else {
		bestTurnR = R5;
		stepR = s3;
	}
	S = endNode - startNode + N;

	const int L1 = (S - 1) / s2 + S % N;
	const int L2 = (S - 1) / s2 - S % s2 + s2 + 1;
	int L3 = (S - 1) / s2 - (s2 * (S / s2 + 1) - S) + s2 + 1;
	const int L4 = (S - 1) / s3 - S % s3 + s3 + 1;
	int L5 = (S - 1) / s3 - (s3 * (S / s3 + 1) - S) + s3 + 1;

	if (L3 > L2)
		L3 = L2;
	if (L5 > L4)
		L5 = L4;

	if (L1 < L3) {
		if (L1 < L5) {
			bestTurnL = L1;
			stepL = -s1;
		}
		else {
			bestTurnL = L5;
			stepL = -s3;
		}
	}
	else if (L3 < L5) {
		bestTurnL = L3;
		stepL = -s2;
	}
	else {
		bestTurnL = L5;
		stepL = -s3;
	}

	if (bestTurnR < bestTurnL) {
		return stepR;
	}
	return stepL;
 }

} // namespace

int direction_selection_next_step(int startNode, int endNode, int N, int s1, int s2, int s3){
	startNode += 1; // original algorithm's indices start from 1
	endNode += 1;

	if (startNode > endNode) {
		startNode = startNode - direction_selection_internal_step(endNode, startNode, N, s1, s2, s3);
	}
	else {
		startNode = startNode + direction_selection_internal_step(startNode, endNode, N, s1, s2, s3);
	}

	if (startNode > N) {
		startNode = startNode - N;
	}
	else if (startNode <= 0) {
		startNode = startNode + N;
	}

	startNode -= 1; // return to 0 based indexing
	return startNode;
}


}