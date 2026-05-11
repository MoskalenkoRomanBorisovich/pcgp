#include "pcgpp.hpp"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <optional>
#include <thread>
#include <chrono>
#include <future>

#include <pcgp_parallel.hpp>


namespace {


struct Options {
    int n_start = 0, n_end = 0;
    int k_start = 0, k_end = 0;
    int so = 0;
    int threads = std::thread::hardware_concurrency();
	bool benchmark = false;
    bool show_help = false;
    std::filesystem::path out_dir = {};
};

void validate_options(const Options& opts) {
    if (opts.show_help) {
        return;
	}
    if (opts.n_start <= 0 || opts.n_end <= 0 || opts.n_start > opts.n_end) {
		throw std::invalid_argument("Invalid n range.");
    }
    if (opts.k_start <= 0 || opts.k_end <= 0 || opts.k_start > opts.k_end) {
		throw std::invalid_argument("Invalid k range.");
    }
    if (opts.so < 0) {
		throw std::invalid_argument("Start offset must be non-negative.");
    }
    if (opts.threads <= 0) {
		throw std::invalid_argument("Thread count must be positive.");
    }

    if (opts.n_end / 2 < opts.k_start
        || opts.so > opts.k_end
        ) {
        throw std::invalid_argument("Error: no valid combinations of n, k, so.\n");
    }
}

void print_help() {
    std::cout <<
        "Usage:\n"
        "  pcgpp -n <num|start-end> -k <num|start-end> [-so <num>] [--out_dir <path>] [-threads <num>] [--bench] \n\n"
        "Options:\n"
        "  -n <num|start-end>     value for n, single value or range of values (required)\n"
        "  -k <num|start-end>     value for k, single value or range of values (required)\n"
        "  -so <num>              number of fixed jumps (default: 0)\n"
        "  --out_dir <path>       output directory, if set will create/truncate file {n}_{k}.csv in said directory for each pair."
        "If not set will print to the console\n"
        "  -t, --threads <num>    thread count (default: maximum available)\n"
        "  -b, --bench            measure execution time\n"
        "  -h, --help             show this message\n"
        "Examples:\n"
        "  pcgpp_app -n 10 -k 3\n"
        "  pcgpp_app -n 80 -k 2-4 -so 1 -t 8\n"
        "  pcgpp_app -n 500 -k 3 --out_dir \"out results\" --bench \n";
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
            if (i + 1 >= argc) [[unlikely]] {
				throw std::invalid_argument("Missing value for " + std::string(name));
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
            std::string_view val = require_value(arg);
            if (val.empty()) return {};
            auto rng = parse_range(val);
            if (!rng) [[unlikely]] {
				throw std::invalid_argument("Invalid format for -n, expected number or range like 3-10");
            }
			opts.n_start = rng->first;
			opts.n_end = rng->second;
            continue;
        }
        if (arg == "-k") {
            std::string_view val = require_value(arg);
            if (val.empty()) return {};

            auto rng = parse_range(val);
            if (!rng) {
                throw std::invalid_argument("Invalid format for -k, expected number or range like 3-10\n");
            }
            opts.k_start = rng->first;
            opts.k_end= rng->second;
            continue;
        }
        if (arg == "-so" || arg == "--start-offset") {
            std::string_view val = require_value(arg);
            if (val.empty()) return {};
            opts.so = std::stoi(std::string(val));
            continue;
        }
        if (arg == "--out_dir") {
            std::string_view val = require_value(arg);
            if (val.empty()) return {};
            opts.out_dir = std::string(val);
            continue;
		}
        if (arg == "-t" || arg == "--threads") {
            std::string_view val = require_value(arg);
            if (val.empty()) return {};
            opts.threads = std::stoi(std::string(val));
            continue;
        }
        if (arg == "-b" || arg == "--bench") {
            opts.benchmark = true;
            continue;
		}

		throw std::invalid_argument("Unknown argument: " + std::string(arg));
    }
    
    validate_options(opts);
    
    return opts;
}


void check_and_create_outdir(const std::filesystem::path& p) {
    namespace fs = std::filesystem;

    // Check if path exists
    if (fs::exists(p)) {

        // Exists but not a directory?
        if (!fs::is_directory(p)) {

            throw std::runtime_error("Output path exists but is not a directory: " + p.string());
        }

        // Directory already exists
        return;
    }
    // Create directory (recursive)
    if (!fs::create_directories(p)) {
        // Failed to create
		throw std::runtime_error("Failed to create output directory: " + p.string());
    }
}

std::filesystem::path get_file_path(const std::filesystem::path& dir_path, int n, int k) {
    std::filesystem::path fpath = dir_path;
    const std::string fname = std::format("{}_{}.csv", n, k);
    fpath.append(fname);
    return fpath;
}

void write_results(int n, int k, const IntGraphProp& best_prop, const std::vector<int>& s_res, const Options& opt) {
    GraphProp prop;
    calcGraphProp(n, &best_prop, &prop);
    if (opt.out_dir.empty()) {
        // write header only once in console 
        const bool write_header = k == opt.k_start && n == opt.n_start;
        pcgp::write_results_csv(n, k, prop, s_res, write_header, std::cout);
    }
    else {
        const std::filesystem::path fpath = get_file_path(opt.out_dir, n, k);
        std::ofstream fs(fpath, std::ios_base::trunc | std::ios_base::out);
        if (!fs) [[unlikely]] {
            throw std::runtime_error("Failed to open output file: " + fpath.string());
        }
        pcgp::write_results_csv(n, k, prop, s_res, true, fs);
    }
}




bool run_pcgpp(const Options& opt) {
    if (!opt.out_dir.empty()) {
        check_and_create_outdir(opt.out_dir);
	}
    std::future<void> write_future;


    const auto t_start = std::chrono::high_resolution_clock::now();
    for (int n = opt.n_start; n <= opt.n_end; ++n) {
        IntGraphProp best_prop = IntGraphProp_infty();
        for (int k = opt.k_start, k_end = opt.k_end + 1; k < k_end; ++k) {
            if (graphCheck_impl(n, k, opt.so)) {
                // invalid combination
                continue;
            }
            std::vector<int> s_res;
            //const auto new_best_prop = pcgp::pcgp_parallel(
            const auto new_best_prop = pcgp::pcgp_parallel_isomorph(
            //const auto new_best_prop = pcgp::pcgp_parallel_isomorph_full(
                s_res,
                n,
                k,
                opt.so,
                best_prop,
                opt.threads
            );
            if (!new_best_prop) [[unlikely]] {
                return false;
            }
            // update best prop
            best_prop = *new_best_prop;

            if (write_future.valid()) {
                write_future.get();
            }
            write_future = std::async(
                std::launch::async,
                write_results,
                n,
                k,
                best_prop,
                std::move(s_res),
                std::cref(opt)
            );
        }
    }
    // Wait for the last write to finish
    if (write_future.valid()) {
        write_future.get();
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
