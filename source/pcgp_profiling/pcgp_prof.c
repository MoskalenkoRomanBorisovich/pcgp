#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#include <pcgpmem.h>
#include <pcgp.h>
#include <search.h>


void init_graph(Graph* g, int n, int k) {
    g->n = n;
    g->k = k;
    for (int i = 0; i < k; i++) {
        g->s[i] = i + 1;
    }
}

void profile_bfs(
    size_t n_runs,
    int n_min,
    int n_max,
    int k_min,
    int k_max
) {
    Arena arena = { 0 };
    {
        const size_t arena_size = (n_max * 100) * sizeof(int);
        void* arena_mem = malloc(arena_size);
        if (!arena_mem)
            return;
        arenaInit(&arena, arena_mem, arena_size);
    }
    Graph g;
    GraphProp prop;
    g.s = malloc(n_max * sizeof(int));
    g.so = 0;
    clock_t clock_start = 0;
    clock_t clock_end = 0;
    double elapsed_time = 0;

    int* dist_3 = (int*)malloc(4 * n_max * sizeof(int));
    int* queue_3 = (int*)malloc(2 * n_max * sizeof(int));

    unsigned int* dist_5 = (unsigned int*)malloc(n_max * sizeof(unsigned int));

    for (int n = n_min; n < n_max; n++) {
        for (int k = k_min; k < min(n / 2, k_max); k++) {
            printf("Profiling BFS with N=%d K=%d\n", n, k);
            clock_start = clock();
            for (int run = 0; run < n_runs; run++) {
                init_graph(&g, n, k);
                do {
                    arenaFree(&arena);
                    circulantBFS(&arena, &prop, &g);
                } while (next_lexicographic_step(&g));
            }
            clock_end = clock();
            elapsed_time = (double)(clock_end - clock_start) / CLOCKS_PER_SEC;
            printf("BFS Elapsed time: %f seconds\n", elapsed_time);

            clock_start = clock();
            for (int run = 0; run < n_runs; run++) {
                init_graph(&g, n, k);
                do {
                    arenaFree(&arena);
                    circulantBFS_2(&arena, &prop, &g);
                } while (next_lexicographic_step(&g));
            }
            clock_end = clock();
            elapsed_time = (double)(clock_end - clock_start) / CLOCKS_PER_SEC;
            printf("BFS_2 Elapsed time: %f seconds\n", elapsed_time);

            clock_start = clock();
            for (int run = 0; run < n_runs; run++) {
                int* dist_3_begin = initCirculantBFS_3(n, dist_3);
                init_graph(&g, n, k);
                do {
                    circulantBFS_3_impl(dist_3_begin, queue_3, &prop, &g);
                } while (next_lexicographic_step(&g));
            }
            clock_end = clock();
            elapsed_time = (double)(clock_end - clock_start) / CLOCKS_PER_SEC;
            printf("BFS_3 Elapsed time: %f seconds\n", elapsed_time);

            // clock_start = clock();
            // for (int run = 0; run < n_runs; run++) {
            //     int* dist_3_begin = initCirculantBFS_3(n, dist_3);
            //     init_graph(&g, n, k);
            //     do {
            //         circulantBFS_4_impl(dist_3_begin, queue_3, &prop, &g);
            //     } while (next_lexicographic_step(&g));
            // }
            // clock_end = clock();
            // elapsed_time = (double)(clock_end - clock_start) / CLOCKS_PER_SEC;
            // printf("BFS_4 Elapsed time: %f seconds\n", elapsed_time);

            // clock_start = clock();
            // for (int run = 0; run < n_runs; run++) {
            //     IntGraphProp prop5_best;
            //     prop5_best.diam = UINT_MAX;
            //     prop5_best.dist_sum = UINT_MAX;
            //     IntGraphProp prop5;
            //     init_graph(&g, n, k);
            //     do {
            //         if (circulantBFS_5_impl(dist_5, queue_3, &prop5_best, &g, &prop5)) {
            //             if (IntGraphProp_less(&prop5, &prop5_best)) {
            //                 prop5_best = prop5;
            //             }
            //         }
            //         arenaFree(&arena);
            //     } while (next_lexicographic_step(&g));
            // }
            // clock_end = clock();
            // elapsed_time = (double)(clock_end - clock_start) / CLOCKS_PER_SEC;
            // printf("BFS_5 Elapsed time: %f seconds\n", elapsed_time);

            clock_start = clock();
            for (int run = 0; run < n_runs; run++) {
                IntGraphProp prop6_best;
                prop6_best.diam = UINT_MAX;
                prop6_best.dist_sum = UINT_MAX;
                IntGraphProp prop6;
                init_graph(&g, n, k);
                do {
                    if (circulantBFS_6(dist_5, queue_3, &g, &prop6_best, &prop6)) {
                        if (IntGraphProp_less(&prop6, &prop6_best)) {
                            prop6_best = prop6;
                        }
                    }
                } while (next_lexicographic_step(&g));
            }
            clock_end = clock();
            elapsed_time = (double)(clock_end - clock_start) / CLOCKS_PER_SEC;
            printf("BFS_6 Elapsed time: %f seconds\n", elapsed_time);


        }
    }
    free(dist_3);
    free(queue_3);
    free(dist_5);
    free(g.s);
    free(arena.start);
}


void test_bfs() {
    const double aspl_tol = 1e-5;
    const int n_min = 1;
    const int n_max = 22;
    const int k_min = 1;
    const int k_max = 10;
    Arena arena = { 0 };
    {
        const size_t arena_size = 1 << 26;
        void* arena_mem = malloc(arena_size);
        if (!arena_mem)
            return;
        arenaInit(&arena, arena_mem, arena_size);
    }
    Graph g;
    g.s = malloc(n_max * sizeof(int));
    g.so = 0;

    int* dist_3 = (int*)malloc(2 * n_max * sizeof(int));
    int* queue_3 = (int*)malloc(n_max * sizeof(int));
    unsigned int* dist_5 = (unsigned int*)malloc(n_max * sizeof(unsigned int));
    bool error = false;
    for (int n = n_min; n < n_max; n++) {
        for (int k = k_min; k < min(n / 2, k_max); k++) {
            init_graph(&g, n, k);
            do {
                GraphProp prop1;
                arenaFree(&arena);
                circulantBFS(&arena, &prop1, &g);

                GraphProp prop2;
                arenaFree(&arena);
                circulantBFS_2(&arena, &prop2, &g);

                GraphProp prop3;
                int* dist_3_begin = initCirculantBFS_3(n, dist_3);
                circulantBFS_3_impl(dist_3_begin, queue_3, &prop3, &g);

                GraphProp prop4;
                int* dist_4_begin = initCirculantBFS_3(n, dist_3);
                circulantBFS_4_impl(dist_4_begin, queue_3, &prop4, &g);

                IntGraphProp prop5;
                IntGraphProp prop_best;
                prop_best.diam = UINT_MAX;
                prop_best.dist_sum = UINT_MAX;
                const bool success_5 = circulantBFS_5_impl(dist_5, queue_3, &prop_best, &g, &prop5);
                if (success_5) {
                    IntGraphProp p;
                    IntGraphProp prop5_copy = prop5;
                    bool _success = circulantBFS_5_impl(dist_5, queue_3, &prop5_copy, &g, &p);
                    if (!_success) {
                        printf("error in bfs_5 early termination\n");
                        error = true;
                    }
                    prop5_copy.diam = 1;
                    prop5_copy.dist_sum = 1;
                    _success = circulantBFS_5_impl(dist_5, queue_3, &prop5_copy, &g, &p);
                    if (_success) {
                        printf("error in bfs_5 early termination\n");
                        error = true;
                    }
                }
                IntGraphProp prop6;
                IntGraphProp prop_best6;
                prop_best6.diam = UINT_MAX;
                prop_best6.dist_sum = UINT_MAX;
                const bool success_6 = circulantBFS_6(dist_5, queue_3, &g, &prop_best6, &prop6);
                if (success_6) {
                    IntGraphProp p;
                    IntGraphProp prop6_copy = prop6;
                    bool _success = circulantBFS_6(dist_5, queue_3, &g, &prop6_copy, &p);
                    if (!_success) {
                        printf("error in bfs_6 early termination\n");
                        error = true;
                    }
                    prop6_copy.diam = 1;
                    prop6_copy.dist_sum = 1;
                    _success = circulantBFS_6(dist_5, queue_3, &g, &prop6_copy, &p);
                    if (_success) {
                        printf("error in bfs_6 early termination\n");
                        error = true;
                    }
                }

                if (fabs(prop1.aspl - prop2.aspl) > aspl_tol || prop1.diam != prop2.diam) {
                    printf("error in bfs_2 %f, %d, %f, %d\n", prop1.aspl, prop1.diam, prop2.aspl, prop2.diam);
                    error = true;
                }

                if (fabs(prop1.aspl - prop3.aspl) > aspl_tol || prop1.diam != prop3.diam) {
                    printf("error in bfs_3 %f, %d, %f, %d\n", prop1.aspl, prop1.diam, prop3.aspl, prop3.diam);
                    error = true;
                }

                if (fabs(prop1.aspl - prop4.aspl) > aspl_tol || prop1.diam != prop4.diam) {
                    printf("error in bfs_4 %f, %d, %f, %d\n", prop1.aspl, prop1.diam, prop4.aspl, prop4.diam);
                    error = true;
                }

                if (success_5) {
                    GraphProp prop5_res;
                    calcGraphProp(n, &prop5, &prop5_res);
                    if (fabs(prop1.aspl - prop5_res.aspl) > aspl_tol || prop1.diam != prop5_res.diam) {
                        printf("error in bfs_5 %f, %d, %f, %d\n", prop1.aspl, prop1.diam, prop5_res.aspl, prop5_res.diam);
                        error = true;
                    }
                }
                else {
                    if (prop1.diam != INT_MAX) {
                        printf("error in bfs_5 %f, %d, %d, %d\n", prop1.aspl, prop1.diam, prop5.dist_sum, prop5.diam);
                        error = true;
                    }
                }

                if (success_6) {
                    GraphProp prop6_res;
                    calcGraphProp(n, &prop6, &prop6_res);
                    if (fabs(prop1.aspl - prop6_res.aspl) > aspl_tol || prop1.diam != prop6_res.diam) {
                        printf("error in bfs_6 %f, %d, %f, %d\n", prop1.aspl, prop1.diam, prop6_res.aspl, prop6_res.diam);
                        error = true;
                    }
                }
                else {
                    if (prop1.diam != INT_MAX) {
                        printf("error in bfs_6 %f, %d, %d, %d\n", prop1.aspl, prop1.diam, prop6.dist_sum, prop6.diam);
                        error = true;
                    }
                }

            } while (next_lexicographic_step(&g));
        }
    }
    if (error) {
        printf("BFS test failed\n");
    }
    else {
        printf("BFS test passed\n");
    }
    free(g.s);
    free(arena.start);
}

typedef struct {
    IntGraphProp best_prop;
    unsigned int count;
} Userdata1;

void Userdata1_init(Userdata1* userdata)
{
    IntGraphProp_infty(&userdata->best_prop);
    userdata->count = 0;
}

bool found_graph1(void* ar, const Graph* g, const IntGraphProp* p)
{
    (void)g;
    Userdata1* data = (Userdata1*)ar;
    if (IntGraphProp_less(p, &data->best_prop)) {
        data->best_prop = *p;
        data->count = 1;
    }
    else if (IntGraphProp_equal(p, &data->best_prop)) {
        data->count++;
    }
    return true;
}


void test_search2(int n, int k, int so) {
    Graph g;
    g.s = (int*)malloc(k * sizeof(int));
    g.so = so;
    init_graph(&g, n, k);
    IntGraphProp start_prop;
    IntGraphProp_infty(&start_prop);

    Graph g_check;
    g_check.s = (int*)malloc(k * sizeof(int));
    g_check.so = so;
    init_graph(&g_check, n, k);

    const size_t arena_size = 1024 * 1024;
    char* arena_mem = (char*)malloc(arena_size);
    Arena arena;
    arenaInit(&arena, arena_mem, arena_size);
    GraphProp prop;
    GraphProp best_prop_check;
    best_prop_check.aspl = FLT_MAX;
    best_prop_check.diam = INT_MAX;

    do {
        arenaFree(&arena);
        circulantBFS(&arena, &prop, &g_check);
        if (prop.aspl > best_prop_check.aspl
            || prop.aspl == best_prop_check.aspl && prop.diam > best_prop_check.diam
            ) {
            continue;
        }
        if (prop.aspl < best_prop_check.aspl
            || prop.aspl == best_prop_check.aspl && prop.diam < best_prop_check.diam
            ) {
            best_prop_check = prop;
        }
    } while (next_lexicographic_step(&g_check));
    free(g.s);
    free(arena_mem);
}

int main() {
    test_bfs();

    for (int n = 10; n < 20; ++n) {
        for (int k = 2; k < 5; ++k) {
            for (int so = 0; so < k; ++so) {
                test_search2(n, k, so);
            }
        }
    }

    profile_bfs(100000, 19, 20, 2, 7);

    profile_bfs(1, 99, 100, 2, 6);
    return 0;
}