#pragma once


#include <pcgp_wrap.h>

namespace pcgp {

size_t id_from_graph(GraphClass& g) {
	size_t id = 0;
	for (int i = 0; i < g.k; ++i) {
		id *= g.n / 2;
		id += (g.s[i] > 0) ? (g.s[i] - 1) : (g.s[i] + g.n / 2 - 1);
	}
	return id;
}

}