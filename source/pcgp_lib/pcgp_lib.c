#include "pcgp.h"

#include <limits.h>
#include <float.h>
#include <stdlib.h>
#include <memory.h>


bool next_lexicographic_step(Graph* g)
{/* compute the next lexicographical S or stop */
    int n = g->n / 2;
    int k = g->k - g->so;
    int* s = g->s + g->so;
    bool changed = false;
    for (int i = 1; i <= k; ++i) {
        if (s[k - i] <= n - i) {
            ++s[k - i];
            for (int j = k - i + 1; j < k; ++j) {
                s[j] = s[j - 1] + 1;
            }
            changed = true;
            break;
        }
    }
    return changed;
}


void circulantBFS(Arena* restrict arena, GraphProp* restrict prop, const Graph* restrict g) {
    int* restrict dist = arenaAlloc(arena, g->n * sizeof(*dist));
    int* restrict queue = arenaAlloc(arena, g->n * sizeof(*queue));
    dist[0] = 0;
    for (int i = 1; i < g->n; i++) {
        dist[i] = INT_MAX;
    }
    queue[0] = 0;
    int qr = 0;
    int qw = 1;
    int vc = 1;
    while (vc < g->n && qr != qw) {
        int u = queue[qr++];
        int d = dist[u] + 1;
        for (int i = 0; i < g->k; i++) {
            VertPair uv = { 0 };
            adjVert(&uv, g, u, i);
            if (dist[uv.u] == INT_MAX) {
                queue[qw++] = uv.u;
                dist[uv.u] = dist[(g->n - uv.u) % g->n] = d;
                vc += 2;
            }
            if (dist[uv.v] == INT_MAX) {
                queue[qw++] = uv.v;
                dist[uv.v] = dist[(g->n - uv.v) % g->n] = d;
                vc += 2;
            }
        }
    }
    if (vc < g->n) {
        GraphProp_infty(prop);
        return;
    }
    int diam = 0;
    int dist_sum = 0;
    for (int i = 0; i < g->n; i++) {
        diam = diam > dist[i] ? diam : dist[i];
        dist_sum += dist[i];
    }
    prop->diam = diam;
    prop->aspl = (double)dist_sum / (g->n - 1);
}


void adjVertFast(VertPair* uv, int s, int u, int n) {
    uv->u = u + s;
    if (uv->u >= n)
        uv->u -= n;
    uv->v = u - s;
    if (uv->v < 0)
        uv->v += n;
}


int mirrVert(int v, int n) {
    return n - v;
}


void circulantBFS_2(Arena* restrict arena, GraphProp* restrict prop, const Graph* restrict g) {
    const int n = g->n;
    const int k = g->k;
    int* restrict dist = arenaAlloc(arena, n * sizeof(*dist));
    int* restrict queue = arenaAlloc(arena, n * sizeof(*queue));
    dist[0] = 0;
    for (int i = 1; i < n; i++) {
        dist[i] = INT_MAX;
    }
    queue[0] = 0;
    int qr = 0;
    int qw = 1;
    int vc = 1;
    while (vc < n && qr != qw) {
        const int u = queue[qr++];
        const int d = dist[u] + 1;
        for (int i = 0; i < k; i++) {
            VertPair uv = { 0 };
            const int s = g->s[i];
            adjVertFast(&uv, s, u, n);
            if (dist[uv.u] == INT_MAX) {
                queue[qw++] = uv.u;
                dist[uv.u] = dist[mirrVert(uv.u, n)] = d;
                vc += 2;
            }
            if (dist[uv.v] == INT_MAX) {
                queue[qw++] = uv.v;
                dist[uv.v] = dist[mirrVert(uv.v, n)] = d;
                vc += 2;
            }
        }
    }
    if (vc < n) {
        GraphProp_infty(prop);
        return;
    }
    int diam = 0;
    int dist_sum = 0;
    for (int i = 0; i < g->n; i++) {
        diam = diam > dist[i] ? diam : dist[i];
        dist_sum += dist[i];
    }
    prop->diam = diam;
    prop->aspl = (double)dist_sum / (g->n - 1);
}


int* initCirculantBFS_3(int n, int* const dist) {
    int n_2 = (n + 2) / 2;

    for (int i = 0; i < n_2; i++) {
        dist[i] = 0;
    }

    for (int i = n_2 * 2; i < n_2 * 4; i++) {
        dist[i] = 0;
    }
    return dist + n_2;
}


void circulantBFS_3_impl(int* restrict dist, int* restrict queue, GraphProp* restrict prop, const Graph* restrict g) {
    const int n = g->n;
    const int k = g->k;
    const int n_h_verts = (n + 2) / 2;
    const bool is_even = (n % 2 == 0);
    dist[0] = 0;
    for (int i = 1; i < n_h_verts; i++) {
        dist[i] = INT_MAX;
    }
    int q_size = 1;
    int q_cur = 0;
    queue[q_cur] = 0;
    while (q_cur < q_size && q_size < n_h_verts) {
        const int v = queue[q_cur++];
        for (int i = 0; i < k; i++) {
            const int s = g->s[i];
            int u = v + s;
            if (dist[u] == INT_MAX) {
                queue[q_size++] = u;
                dist[u] = dist[v] + 1;
            }
            u = v - s;
            if (dist[u] == INT_MAX) {
                queue[q_size++] = u;
                dist[u] = dist[v] + 1;
            }
            u = -v + s;
            if (dist[u] == INT_MAX) {
                queue[q_size++] = u;
                dist[u] = dist[v] + 1;
            }
            u = n - v - s;
            if (dist[u] == INT_MAX) {
                queue[q_size++] = u;
                dist[u] = dist[v] + 1;
            }
        }
    }
    if (q_size < n_h_verts) {
        prop->diam = INT_MAX;
        prop->aspl = DBL_MAX;
        return;
    }
    int diam = 0;
    int dist_sum = 0;
    int n_unique_verts = (n + 1) / 2;
    for (int i = 0; i < n_unique_verts; i++) {
        int d = dist[i];
        diam = diam > d ? diam : d;
        dist_sum += d << 1;
    }
    if (is_even) {
        int d = dist[n_unique_verts];
        diam = diam > d ? diam : d;
        dist_sum += d;
    }
    prop->diam = diam;
    prop->aspl = (double)dist_sum / (g->n - 1);
}


void circulantBFS_4_impl(int* restrict dist, int* restrict queue, GraphProp* restrict prop, const Graph* restrict g) {
    const int n = g->n;
    const int k = g->k;
    const int n_h_verts = (n + 2) / 2;
    const bool is_even = (n % 2 == 0);
    dist[0] = 0;
    for (int i = 1; i < n_h_verts; i++) {
        dist[i] = INT_MAX;
    }
    int q_size = 1;
    int q_cur = 0;
    queue[q_cur] = 0;
    while (q_cur < q_size && q_size < n_h_verts) {
        const int v = queue[q_cur++];
        const int d = dist[v] + 1;
        for (int i = 0; i < k; i++) {
            const int s = g->s[i];
            int u = v + s;
            if (u < n_h_verts && dist[u] == INT_MAX) {
                queue[q_size++] = u;
                dist[u] = d;
            }
            u = v - s;
            if (0 <= u && dist[u] == INT_MAX) {
                queue[q_size++] = u;
                dist[u] = d;
            }
            u = -v + s; // ((n - v) + s) - n
            if (0 <= u && dist[u] == INT_MAX) {
                queue[q_size++] = u;
                dist[u] = d;
            }
            u = n - v - s;
            if (u < n_h_verts && dist[u] == INT_MAX) {
                queue[q_size++] = u;
                dist[u] = d;
            }
        }
    }
    if (q_size < n_h_verts) {
        prop->diam = INT_MAX;
        prop->aspl = DBL_MAX;
        return;
    }
    int diam = 0;
    int dist_sum = 0;
    int n_unique_verts = (n + 1) / 2;
    for (int i = 0; i < n_unique_verts; i++) {
        int d = dist[i];
        diam = diam > d ? diam : d;
        dist_sum += d << 1;
    }
    if (is_even) {
        int d = dist[n_unique_verts];
        diam = diam > d ? diam : d;
        dist_sum += d;
    }
    prop->diam = diam;
    prop->aspl = (double)dist_sum / (g->n - 1);
}


bool circulantBFS_5_impl(unsigned int* restrict dist, int* restrict queue, const IntGraphProp* restrict best_prop, const Graph* restrict g, IntGraphProp* restrict prop) {
    const int n = g->n;
    const int k = g->k;
    const int n_h_verts = (n + 2) / 2;
    dist[0] = 0;
    for (int i = 1; i < n_h_verts; i++) {
        dist[i] = UINT_MAX;
    }
    int q_size = 1;
    int q_cur = 0;
    queue[q_cur] = 0;
    IntGraphProp cur_prop = { 0 };
    int n_unique_verts = (n + 1) / 2;
    while (q_cur < q_size && q_size < n_h_verts) {
        const int v = queue[q_cur++];
        const unsigned int d = dist[v] + 1;
        cur_prop.diam = d;
        if (IntGraphProp_greater(&cur_prop, best_prop))
            return false;
        for (int i = 0; i < k; i++) {
            const int s = g->s[i];
            int u = v + s;
            if (u < n_h_verts && dist[u] == UINT_MAX) {
                queue[q_size++] = u;
                dist[u] = d;
                cur_prop.dist_sum += d << (u != n_unique_verts);
            }
            u = v - s;
            if (0 <= u && dist[u] == UINT_MAX) {
                queue[q_size++] = u;
                dist[u] = d;
                cur_prop.dist_sum += d << (u != n_unique_verts);
            }
            u = -v + s; // ((n - v) + s) - n
            if (0 <= u && dist[u] == UINT_MAX) {
                queue[q_size++] = u;
                dist[u] = d;
                cur_prop.dist_sum += d << (u != n_unique_verts);
            }
            u = n - v - s;
            if (u < n_h_verts && dist[u] == UINT_MAX) {
                queue[q_size++] = u;
                dist[u] = d;
                cur_prop.dist_sum += d << (u != n_unique_verts);
            }
        }
    }
    if (q_size < n_h_verts) {
        return false;
    }
    *prop = cur_prop;
    return true;
}


bool circulantBFS_6(unsigned int* restrict dist, int* restrict queue, const Graph* restrict g, const IntGraphProp* restrict best_prop, IntGraphProp* restrict prop) {
    const int n = g->n;
    const int k = g->k;
    dist[0] = 0;
    for (unsigned int i = 1; i < n; i++) {
        dist[i] = UINT_MAX;
    }
    queue[0] = 0;
    unsigned int qr = 0;
    unsigned int qw = 1;
    unsigned int vc = 1;
    IntGraphProp cur_prop = { 0 };
    while (vc < n && qr != qw) {
        const int u = queue[qr++];
        const unsigned int d = dist[u] + 1;
		if (cur_prop.diam != d) {// attempt pruning when new depth is reached
			// minimal possible property
			const IntGraphProp temp_prop = { .diam = d, .dist_sum = cur_prop.dist_sum + d * (n - vc)};
            if (IntGraphProp_greater(&temp_prop, best_prop)) {
                return false;
            }
        }
		cur_prop.diam = d;
        for (int i = 0; i < k; i++) {
            VertPair uv = { 0 };
            const int s = g->s[i];
            adjVertFast(&uv, s, u, n);
            if (dist[uv.u] == UINT_MAX) {
                queue[qw++] = uv.u;
                dist[uv.u] = dist[mirrVert(uv.u, n)] = d;
                vc += 2;
                cur_prop.dist_sum += d << 1;
            }
            if (dist[uv.v] == UINT_MAX) {
                queue[qw++] = uv.v;
                dist[uv.v] = dist[mirrVert(uv.v, n)] = d;
                vc += 2;
                cur_prop.dist_sum += d << 1;
            }
        }
    }
    if (vc < n) { // not all vertices were reached
        return false;
    }
	if (n % 2 == 0) { // adjust for middle vertex counted twice
        const int middle_vert = n / 2;
		cur_prop.dist_sum -= dist[middle_vert];
    }
    *prop = cur_prop;
    return true;
}


typedef struct {
    int a, b;
    int g;
} KLPair;

static int KernighanLinC(Graph* graph, const int* part, unsigned a, unsigned b) {
    for (int i = 0; i < graph->k; i++) {
        VertPair uv = { 0 };
        adjVert(&uv, graph, a, i);
        if (uv.v == (int)b || uv.u == (int)b) {
            return part[a] != part[b] ? 1 : -1;
        }
    }
    return 0;
}

static int KernighanLinPartitionCost(Graph* graph, const int* part) {
    int cost = 0;
    for (int u = 0; u < graph->n; u++) {
        for (int i = 0; i < graph->k; i++) {
            const int v = (u + graph->s[i]) % graph->n;
            if (part[u] != part[v]) {
                cost++;
            }
        }
    }
    return cost;
}

int circulantKernighanLin(Arena* restrict arena, Graph* restrict graph) {
    int passes = 0;
    int* restrict V = arenaAlloc(arena, graph->n * sizeof(*V));
    int* restrict P = arenaAlloc(arena, graph->n * sizeof(*P));
    int* restrict D = arenaAlloc(arena, graph->n * sizeof(*D));
    int* restrict G_sum = arenaAlloc(arena, graph->n * sizeof(*G_sum));
    int G_sum_max = 0;
    int GAB_size = 0;
    KLPair* restrict GAB = arenaAlloc(arena, graph->n * sizeof(*GAB));
    for (int i = 0; i < graph->n / 2; i++) {
        P[i] = 0;
    }
    for (int i = graph->n / 2; i < graph->n; i++) {
        P[i] = 1;
    }
    do {
        GAB_size = 0;
        for (int u = 0; u < graph->n; u++) {
            V[u] = 0;
            D[u] = 0;
            for (int i = 0; i < graph->k; i++) {
                VertPair uv = { 0 };
                adjVert(&uv, graph, u, i);
                D[u] += (P[u] != P[uv.u] ? 1 : -1);
                if (uv.u != uv.v) {
                    D[u] += (P[u] != P[uv.v] ? 1 : -1);
                }
            }
        }
        for (int i = 0; i < graph->n / 2; i++) {
            int g_max = INT_MIN;
            int a_max = 0, b_max = 0;
            for (int a = 0; a < graph->n; a++) {
                if (!V[a] && (P[a] == 0)) {
                    for (int b = 0; b < graph->n; b++) {
                        if (!V[b] && (P[b] == 1)) {
                            int g = D[a] + D[b] - 2 * KernighanLinC(graph, P, a, b);
                            if (g > g_max) {
                                g_max = g;
                                a_max = a;
                                b_max = b;
                            }
                        }
                    }
                }
            }
            V[a_max] = V[b_max] = 1;
            KLPair* gab = &GAB[GAB_size++];
            gab->a = a_max;
            gab->b = b_max;
            gab->g = g_max;
            for (int u = 0; u < graph->n; u++) {
                if (V[u]) continue;
                else if (P[u]) {
                    int c_yb = KernighanLinC(graph, P, u, b_max);
                    int c_ab = KernighanLinC(graph, P, u, a_max);
                    D[u] += 2 * c_yb - 2 * c_ab;
                }
                else {
                    int c_xa = KernighanLinC(graph, P, u, a_max);
                    int c_xb = KernighanLinC(graph, P, u, b_max);
                    D[u] += 2 * c_xa - 2 * c_xb;
                }
            }
        }
        int k = 0;
        G_sum_max = G_sum[0] = GAB[0].g;
        for (int i = 1; i < GAB_size; i++) {
            G_sum[i] = G_sum[i - 1] + GAB[i].g;
            if (G_sum[i] > G_sum_max) {
                G_sum_max = G_sum[i];
                k = i;
            }
        }
        if (G_sum_max > 0) {
            for (int i = 0; i <= k; i++) {
                KLPair* uv = &GAB[i];
                int tmp = P[uv->a];
                P[uv->a] = P[uv->b];
                P[uv->b] = tmp;
            }
        }
        passes++;
    } while (G_sum_max > 0);
    return KernighanLinPartitionCost(graph, P);
}


