#ifdef _WIN32
#define FSEEK _fseeki64
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#else
#define _FILE_OFFSET_BITS 64
#define FSEEK fseek
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <float.h>
#include <ctype.h>

#include <pcgpmem.h>
#include <pcgp.h>

#define LINE_MAX 4096

void die(const char* msg) {
    if (errno) {
        perror(msg);
    }
    else if (msg) {
        fprintf(stderr, "%s.\n", msg);
    }
    exit(EXIT_FAILURE);
}

bool str2int(int* out, char* str, int base) {
    char* end = NULL;
    if (str[0] == '\0' || isspace(str[0]))
        return 0;
    errno = 0;
    long l = strtol(str, &end, base);
    if (l > INT_MAX || l < INT_MIN
        || errno == ERANGE && (l == LONG_MAX || l == LONG_MIN)
        || *end != '\0') {
        return false;
    }
    else {
        *out = l;
        return true;
    }
}


enum PcgpStage {
    PCGP_STAGE_BFS,
    PCGP_STAGE_KL,
};

typedef struct {
    int                 stage;
    int                 best_diam;
    double              best_aspl;
    int                 best_bisect_cost;
    unsigned long long  graph_count;
    unsigned long long  graph_count_max;
    unsigned long long  graph_count_best;
    size_t              bfs_log_count;
    size_t              kl_log_count;
}   ScanState;

int stateFileWrite(ScanState* scan, Graph* g, FILE* file) {
    void* buf = g->s + g->so;
    size_t len = g->k - g->so;
    if (FSEEK(file, 0L, SEEK_SET)) {
        return 1;
    }
    else if (fwrite(&scan, sizeof(scan), 1, file) < 1) {
        return 1;
    }
    else if (fwrite(buf, sizeof(*g->s), len, file) < len) {
        return 1;
    }
    else {
        return 0;
    }
}

#define STATE_UPDATE_TIME 5.0

enum PcgpMode {
    PCGP_MODE_SCAN,
    PCGP_MODE_IMMEDIATE,
};




bool pcgp_scan_main(int argc, char** argv)
{
    Graph g = { 0 };
    if (argc != 4 || !(str2int(&g.n, argv[1], 10) && str2int(&g.k, argv[2], 10) && str2int(&g.so, argv[3], 10)))
        die("Invalid arguments");
    if (graphCheck(&g))
        die("Invalid argument values");
    g.s = malloc(g.k * sizeof(*g.s));
    if (!g.s)
        return false;
    const size_t GS_SIZE = (g.k - g.so) * sizeof(*g.s);
    ScanState scan = { 0 };
    scan.stage = PCGP_STAGE_BFS;
    scan.best_diam = INT_MAX;
    scan.best_aspl = DBL_MAX;
    scan.best_bisect_cost = 0;
    unsigned long long graph_count_prev = 0;
    FILE* state_file = NULL;
    FILE* bfs_file = NULL;
    FILE* kl_file = NULL;
    {/* restore scan state from file or initialize */
        bool restored = false;
        char bfs_file_name[0x100] = { 0 };
        {
            int len = snprintf(bfs_file_name, sizeof(bfs_file_name), "%d-%d-%d.bfs.bin", g.n, g.k, g.so);
            if (len < 0 || (unsigned)len >= sizeof(bfs_file_name))
                return false;
        }
        char kl_file_name[0x100] = { 0 };
        {
            int len = snprintf(kl_file_name, sizeof(kl_file_name), "%d-%d-%d.kl.bin", g.n, g.k, g.so);
            if (len < 0 || (unsigned)len >= sizeof(kl_file_name))
                return false;
        }
        char state_file_name[0x100] = { 0 };
        {
            int len = snprintf(state_file_name, sizeof(state_file_name), "%d-%d-%d.state.bin", g.n, g.k, g.so);
            if (len < 0 || (unsigned)len >= sizeof(state_file_name))
                return false;
        }
        state_file = fopen(state_file_name, "rb+");
        if (state_file) {
            if (fread(&scan, sizeof(scan), 1, state_file) == 1) {
                for (int i = 0; i < g.so; i++) {
                    g.s[i] = i + 1;
                }
                void* buf = g.s + g.so;
                size_t len = g.k - g.so;
                if (fread(buf, sizeof(*g.s), len, state_file) == len) {
                    restored = true;
                }
            }
        }
        if (restored) {
            bfs_file = fopen(bfs_file_name, "rb+");
            if (!bfs_file)
                return false;
            if (FSEEK(bfs_file, scan.bfs_log_count * GS_SIZE, SEEK_SET))
                return false;
            kl_file = fopen(kl_file_name, "rb+");
            if (!kl_file)
                return false;
            if (FSEEK(kl_file, scan.kl_log_count * GS_SIZE, SEEK_SET))
                return false;
            graph_count_prev = scan.graph_count;
        }
        else {/* initialize scan state */
            bfs_file = fopen(bfs_file_name, "wb+");
            if (!bfs_file)
                return false;
            kl_file = fopen(kl_file_name, "wb+");
            if (!kl_file)
                return false;
            state_file = fopen(state_file_name, "wb+");
            if (!state_file)
                return false;
            {/* compute max graph count as C(n,k) (overflow not tested) */
                int n = g.n / 2 - g.so;
                int k = g.k - g.so;
                if (k * 2 > n) k = n - k;
                if (k > n) {
                    scan.graph_count_max = 0;
                }
                else if (k == 0) {
                    scan.graph_count_max = 1;
                }
                else {
                    long long c = n;
                    for (long long i = 2; i <= k; i++) {
                        c *= (n - i + 1);
                        c /= i;
                    }
                    scan.graph_count_max = c;
                }
            }
            for (int i = 0; i < g.k; i++) {
                g.s[i] = i + 1;
            }
            if (stateFileWrite(&scan, &g, state_file))
                return false;
        }
        fprintf(stderr, "N=%d K=%d C=%d (%llu graphs)\n", g.n, g.k, g.so, scan.graph_count_max);
        if (restored) {
            fprintf(stderr, "Restored scan state at stage %d graph %zu\n", scan.stage, scan.graph_count);
        }
    }
    FILE* log_file = NULL;
    {
        char file_name[0x100] = { 0 };
        int len = snprintf(file_name, sizeof(file_name), "%d-%d-%d.log.csv", g.n, g.k, g.so);
        if (len < 0 || (unsigned)len >= sizeof(file_name))
            return false;
        log_file = fopen(file_name, "w");
        if (!log_file)
            return false;
        if (fputs("graph_count,elapsed,eta,best_diam,best_aspl\n", log_file) == EOF)
            return false;
    }
    Arena arena = { 0 };
    {
        size_t arena_size = 64 * g.n * sizeof(*g.s);
        void* arena_mem = malloc(arena_size);
        if (!arena_mem)
            return false;
        arenaInit(&arena, arena_mem, arena_size);
    }
    clock_t clock_start = 0;
    clock_t clock_prev = 0;
    switch (scan.stage) {
    case PCGP_STAGE_BFS: {
        clock_start = clock_prev = clock();
        bool running = 1;
        while (running) {
            {/* compute and write values for S */
                GraphProp prop = { 0 };
                circulantBFS_2(&arena, &prop, &g);
                bool write = false;
                if (prop.aspl < scan.best_aspl
                    || prop.aspl == scan.best_aspl && prop.diam < scan.best_diam
                    ) {
                    scan.best_aspl = prop.aspl;
                    scan.best_diam = prop.diam;
                    scan.graph_count_best = 1;
                    write = true;
                }
                else if (prop.aspl == scan.best_aspl && prop.diam == scan.best_diam) {
                    scan.graph_count_best++;
                    write = true;
                }
                if (write) {/* write only the variable part of S */
                    scan.bfs_log_count++;
                    void* buf = g.s + g.so;
                    size_t len = g.k - g.so;
                    if (fwrite(buf, sizeof(*g.s), len, bfs_file) < len)
                        return false;
                }
            }
            running = next_lexicographic_step(&g);
            scan.graph_count++;
            clock_t clock_now = clock();
            double elapsed = (clock_now - clock_prev) / CLOCKS_PER_SEC;
            if (!running || elapsed >= STATE_UPDATE_TIME) {
                {/* print logs */
                    clock_prev = clock_now;
                    unsigned long long graph_count_rel = scan.graph_count - graph_count_prev;
                    graph_count_prev = scan.graph_count;
                    double elapsed_total = (double)(clock_now - clock_start) / CLOCKS_PER_SEC;
                    double gps = elapsed != 0 ? (double)graph_count_rel / elapsed : graph_count_rel;
                    double eta = (double)(scan.graph_count_max - scan.graph_count) / gps;
                    double scan_prc = (double)scan.graph_count / scan.graph_count_max * 100.0;
                    int rc = 0;
                    rc = fprintf(stderr, "[Stage 0] %.2f%% (%lld/%lld) ETA=%.fs Elapsed=%gs GPS=%g Diam=%d ASPL=%g\n",
                        scan_prc, scan.graph_count, scan.graph_count_max,
                        eta, elapsed_total, gps, scan.best_diam, scan.best_aspl);
                    if (rc < 0)
                        return false;
                    rc = fprintf(log_file, "%llu,%.f,%.f,%d,%g\n",
                        scan.graph_count, elapsed_total, eta, scan.best_diam, scan.best_aspl);
                    if (rc < 0)
                        return false;
                }
                {/* checkpoint the state file */
                    if (FSEEK(state_file, 0L, SEEK_SET))
                        return false;
                    if (fwrite(&scan, sizeof(scan), 1, state_file) < 1)
                        return false;
                    {
                        void* buf = g.s + g.so;
                        size_t len = g.k - g.so;
                        if (fwrite(buf, sizeof(*g.s), len, state_file) < len)
                            return false;
                    }
                }
            }
            arenaFree(&arena);
        }
        {/* write BFS pass results */
            FILE* output_file = NULL;
            {
                char file_name[0x100] = { 0 };
                int len = snprintf(file_name, sizeof(file_name), "%d-%d-%d.output-stage0.txt", g.n, g.k, g.so);
                if (len < 0 || (unsigned)len >= sizeof(file_name))
                    return false;
                output_file = fopen(file_name, "w");
                if (!output_file)
                    return false;
                fprintf(stderr, "Saving stage 0 output to \"%s\"\n", file_name);
            }
            fprintf(output_file, "# Version PCGP 1.0\n");
            fprintf(output_file, "# Stage 0\n");
            fprintf(output_file, "# Parameters N=%d K=%d C=%d\n", g.n, g.k, g.so);
            fprintf(output_file, "# Values ASPL=%g Diameter=%d\n", scan.best_aspl, scan.best_diam);
            if (FSEEK(bfs_file, (scan.bfs_log_count - scan.graph_count_best) * GS_SIZE, SEEK_SET))
                return false;
            {
                void* buf = g.s + g.so;
                size_t len = g.k - g.so;
                size_t ele_read = 0;
                while ((ele_read = fread(buf, sizeof(*g.s), len, bfs_file))) {
                    if (ele_read == len) {
                        for (int i = 0; i < g.k - 1; i++) {
                            fprintf(output_file, "%d ", g.s[i]);
                        }
                        fprintf(output_file, "%d\n", g.s[g.k - 1]);
                    }
                    else {
                        return false;
                    }
                }
                if (fwrite(buf, sizeof(*g.s), len, state_file) < len)
                    return false;
            }
        }
        scan.stage = PCGP_STAGE_KL;
        scan.graph_count = 0;
        scan.graph_count_max = scan.graph_count_best;
        scan.graph_count_best = 0;
        graph_count_prev = 0;
        if (FSEEK(state_file, 0L, SEEK_SET))
            return false;
        if (fwrite(&scan, sizeof(scan), 1, state_file) < 1)
            return false;
    }
    case PCGP_STAGE_KL: {
        if (FSEEK(bfs_file, (scan.bfs_log_count - scan.graph_count_max + scan.graph_count) * GS_SIZE, SEEK_SET))
            return false;
        {
            void* buf = g.s + g.so;
            size_t len = g.k - g.so;
            size_t ele_read = 0;
            clock_start = clock_prev = clock();
            while ((ele_read = fread(buf, sizeof(*g.s), len, bfs_file))) {
                if (ele_read == len) {
                    int bisect_cost = circulantKernighanLin(&arena, &g);
                    if (bisect_cost >= scan.best_bisect_cost) {
                        if (bisect_cost > scan.best_bisect_cost) {
                            scan.best_bisect_cost = bisect_cost;
                            scan.graph_count_best = 1;
                        }
                        else {
                            scan.graph_count_best++;
                        }
                        scan.kl_log_count++;
                        {
                            void* buf_kl = g.s + g.so;
                            size_t len_kl = g.k - g.so;
                            if (fwrite(buf_kl, sizeof(*g.s), len_kl, kl_file) < len)
                                return false;
                        }
                    }
                    scan.graph_count++;
                    clock_t clock_now = clock();
                    double elapsed = (clock_now - clock_prev) / CLOCKS_PER_SEC;
                    if ((scan.graph_count == scan.graph_count_max) || elapsed >= STATE_UPDATE_TIME) {
                        {/* print logs */
                            clock_prev = clock_now;
                            unsigned long long graph_count_rel = scan.graph_count - graph_count_prev;
                            graph_count_prev = scan.graph_count;
                            double elapsed_total = (double)(clock_now - clock_start) / CLOCKS_PER_SEC;
                            double gps = (double)graph_count_rel / elapsed;
                            double eta = (double)(scan.graph_count_max - scan.graph_count) / gps;
                            double scan_prc = (double)scan.graph_count / scan.graph_count_max * 100.0;
                            int rc = 0;
                            rc = fprintf(stderr, "[Stage 1] %.2f%% (%lld/%lld) ETA=%.fs Elapsed=%gs GPS=%g BISECT_COST=%d\n",
                                scan_prc, scan.graph_count, scan.graph_count_max,
                                eta, elapsed_total, gps, scan.best_bisect_cost);
                            if (rc < 0)
                                return false;
                        }
                        {/* checkpoint the state file */
                            if (FSEEK(state_file, 0L, SEEK_SET))
                                return false;
                            if (fwrite(&scan, sizeof(scan), 1, state_file) < 1)
                                return false;
                        }
                    }
                }
                else {
                    return false;
                }
                arenaFree(&arena);
            }
        }
        {/* write KL pass results */
            FILE* output_file = NULL;
            {
                char file_name[0x100] = { 0 };
                int len = snprintf(file_name, sizeof(file_name), "%d-%d-%d.output-stage1.txt", g.n, g.k, g.so);
                if (len < 0 || (unsigned)len >= sizeof(file_name))
                    return false;
                output_file = fopen(file_name, "w");
                if (!output_file)
                    return false;
                fprintf(stderr, "Saving stage 1 output to \"%s\"\n", file_name);
            }
            fprintf(output_file, "# Version PCGP 1.0\n");
            fprintf(output_file, "# Stage 1\n");
            fprintf(output_file, "# Parameters N=%d K=%d C=%d\n", g.n, g.k, g.so);
            fprintf(output_file, "# Values ASPL=%g Diameter=%d BisectCost=%d\n", scan.best_aspl, scan.best_diam, scan.best_bisect_cost);
            if (FSEEK(kl_file, (scan.kl_log_count - scan.graph_count_best) * GS_SIZE, SEEK_SET))
                return false;
            {
                void* buf = g.s + g.so;
                size_t len = g.k - g.so;
                size_t ele_read = 0;
                while ((ele_read = fread(buf, sizeof(*g.s), len, kl_file))) {
                    if (ele_read == len) {
                        for (int i = 0; i < g.k - 1; i++) {
                            fprintf(output_file, "%d ", g.s[i]);
                        }
                        fprintf(output_file, "%d\n", g.s[g.k - 1]);
                    }
                    else {
                        return false;
                    }
                }
            }
        }
    }
    }
    fprintf(stderr, "Done\n");
    return true;
}


bool pcgp_immediate_main()
{
    Arena arena = { 0 };
    size_t arena_size = 1 << 26;
    {
        void* arena_mem = malloc(arena_size);
        if (!arena_mem)
            return false;
        arenaInit(&arena, arena_mem, arena_size);
    }
    int graph_buf[BUFSIZ] = { 0 };
    char line_buf[LINE_MAX] = { 0 };
    while (fgets(line_buf, LINE_MAX, stdin)) {
        if (ferror(stdin))
            return false;
        Graph g = { 0 };
        {
            int n, len, i = 0;
            char* line = line_buf;
            while (sscanf(line, "%d%n", &n, &len) == 1) {
                graph_buf[i] = n;
                line += len;
                i++;
            }
            g.n = graph_buf[0];
            g.k = i - 1;
            g.s = graph_buf + 1;
        }
        if (graphCheck(&g)) {
            fprintf(stdout, "Invalid circulant parameters\n");
            continue;
        }
        {
            GraphProp prop = { 0 };
            circulantBFS_2(&arena, &prop, &g);
            int bisect_cost = circulantKernighanLin(&arena, &g);
            fprintf(stdout, "%d %g %d\n", prop.diam, prop.aspl, bisect_cost);
        }
        arenaFree(&arena);
    }
    return true;
}

int main(int argc, char** argv) {
    int mode = -1;
    if (argc > 1) {
        if (strcmp(argv[1], "s") == 0) {
            mode = PCGP_MODE_SCAN;
        }
        else if (strcmp(argv[1], "i") == 0) {
            mode = PCGP_MODE_IMMEDIATE;
        }
    }
    else {
        mode = PCGP_MODE_IMMEDIATE;
    }
    argc--;
    argv++;

    switch (mode) {
    case PCGP_MODE_SCAN:
        if (!pcgp_scan_main(argc, argv))
            die("Scan mode failed");
        break;
    case PCGP_MODE_IMMEDIATE:
        if (!pcgp_immediate_main())
            die("Immediate mode failed");
        break;
    default:
        die("Invalid mode argument");
    }
    return EXIT_SUCCESS;
}
