#include "pcgpp.hpp"


#include <iostream>
#include <string>
#include <string_view>
#include <optional>
#include <thread>
#include <chrono>

#include <pcgp_parallel.hpp>


namespace {


struct Options {
    int n = 0;
    int k_start = 0, k_end = 0;
    int so = 0;
    int threads = std::thread::hardware_concurrency();
	bool benchmark = false;
    bool show_help = false;
};

bool validate_options(const Options& opts) {
    if (opts.n <= 0) {
        std::cerr << "Error: n must be positive.\n";
        return false;
    }
    if (opts.k_start <= 0 || opts.k_end <= 0 || opts.k_end < opts.k_start) {
        std::cerr << "Error: invalid k range.\n";
        return false;
    }
    if (opts.so < 0) {
        std::cerr << "Error: start offset must be non-negative.\n";
        return false;
    }
    if (opts.threads <= 0) {
        std::cerr << "Error: threads must be positive.\n";
        return false;
    }

    if (graphCheck_impl(opts.n, opts.k_start, opts.so)
        || graphCheck_impl(opts.n, opts.k_end, opts.so)) {
		std::cerr << "Error: invalid combination of n, k, so.\n";
        return false;
    }
    return true;
}

void print_help() {
    std::cout <<
        "Usage:\n"
        "  pcgpp -n <num> -k <num|start-end> [-so <num>] [-threads <num>] [--bench] \n\n"
        "Options:\n"
        "  -n <num>               value for n (required)\n"
        "  -k <num|start-end>     value for k, single value or range of values (required)\n"
        "  -so <num>              number of fixed jumps (default: 0)\n"
        "  -t, --threads <num>    thread count (default: maximum available)\n"
        "  -b, --bench            measure execution time\n"
        "  -h, --help             show this message\n"
        "Examples:\n"
        "  pcgpp_app -n 10 -k 3\n"
        "  pcgpp_app -n 80 -k 2-4 -so 1 -t 8\n"
        "  pcgpp_app -n 500 -k 3 --bench\n";
}

std::optional<std::pair<int, int>> parse_range(std::string_view sv) {
    // look for '-'
    size_t dash = sv.find('-');

    if (dash == std::string_view::npos) {
        // single value
        int v = std::stoi(std::string(sv));
        if (v <= 0) return {};
        return std::pair{ v, v };
    }

    // range form: a-b
    std::string start_str(sv.substr(0, dash));
    std::string end_str(sv.substr(dash + 1));

    if (start_str.empty() || end_str.empty())
        return {};

    int s = std::stoi(start_str);
    int e = std::stoi(end_str);

    if (s <= 0 || e <= 0 || e < s)
        return {}; // require positive and valid order

    return std::pair{ s, e };
}


std::optional<Options> parse_args(int argc, char* argv[]) {
    Options opts;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        const auto require_value = [&](std::string_view name) -> std::string_view {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << "\n";
                return {};
            }
            return argv[++i];
        };

        // help
        if (arg == "-h" || arg == "--help") {
            opts.show_help = true;
            continue;
        }

        // integer args
        if (arg == "-n") {
            auto val = require_value(arg);
            if (val.empty()) return {};
            opts.n = std::stoi(std::string(val));
            continue;
        }
        if (arg == "-k") {
            auto val = require_value(arg);
            if (val.empty()) return {};

            auto rng = parse_range(val);
            if (!rng) {
                std::cerr << "Invalid format for -k, expected number or range like 3-10\n";
                return {};
            }
            opts.k_start = rng->first;
            opts.k_end= rng->second;
            continue;
        }
        if (arg == "-so" || arg == "--start-offset") {
            auto val = require_value(arg);
            if (val.empty()) return {};
            opts.so = std::stoi(std::string(val));
            continue;
        }
        if (arg == "-t" || arg == "--threads") {
            auto val = require_value(arg);
            if (val.empty()) return {};
            opts.threads = std::stoi(std::string(val));
            continue;
        }
        if (arg == "-b" || arg == "--bench") {
            opts.benchmark = true;
            continue;
		}

        std::cerr << "Unknown option: " << arg << "\n";
        return {};
    }
    
    if (opts.show_help) {
		return opts;
    }
    if (!validate_options(opts))
        return {};
    return opts;
}


bool run_pcgpp(const Options& opt) {
    const auto t_start = std::chrono::high_resolution_clock::now();
    IntGraphProp best_ptop = IntGraphProp_infty();
    std::vector<int> s_res;
    for (int k = opt.k_start, k_end = opt.k_end + 1; k < k_end; ++k) {
        s_res.clear();
        const auto new_best_prop = pcgp::pcgp_parallel(
            s_res,
            opt.n,
            k,
            opt.so,
            best_ptop,
            opt.threads
        );
        if (!new_best_prop) [[unlikely]] {
            return false;
        }
		
        best_ptop = *new_best_prop;
        const bool write_header = k == opt.k_start;
        GraphProp prop;
        calcGraphProp(opt.n, &best_ptop, &prop);
        pcgp::write_results_csv(opt.n, k, prop, s_res, write_header, std::cout);
    }
    const auto t_end = std::chrono::high_resolution_clock::now();
    if (opt.benchmark) {
        const std::chrono::duration<double> elapsed = t_end - t_start;
        std::cout << "Total time: " << elapsed.count() << " seconds\n";
    }
    return true;
}


}


bool pcgpp_app_main(int argc, char* argv[])
{
	const auto opt = parse_args(argc, argv);
    if (!opt || opt->show_help) {
        print_help();
        return false;
	}

    

    if (!run_pcgpp(*opt)) {
        std::cerr << "pcgpp application failed\n";
        return false;
	}
    
    return true;
}
