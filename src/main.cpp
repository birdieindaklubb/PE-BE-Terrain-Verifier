#include "terrain_source.hpp"
#include "end_point_source.hpp"
#include "pe061_terrain_source.hpp"
#include "pe090_cave_prefilter.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using mcpe::worldgen::Chunk;
using mcpe::worldgen::DimensionId;
using mcpe::worldgen::Pe090GeneratorType;
using mcpe::worldgen::Version;
using mcpe::terrain_verifier::CavePoint;
using mcpe::terrain_verifier::TerrainSource;

enum class Operation : std::uint8_t {
    verify,
    filter,
    scan,
};

// Nether population writes blocks, but its cache-driven invocation is not a
// substitute for base terrain.  Keep the two source stages explicit.
enum class NetherStage : std::uint8_t {
    base,
    populated,
};

struct Observation final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t z{};
    std::int32_t chunk_x{};
    std::int32_t chunk_z{};
    std::int32_t local_x{};
    std::int32_t local_z{};
    bool non_air{};
};

struct Options final {
    Operation operation{};
    Version version{};
    DimensionId dimension{DimensionId::overworld};
    Pe090GeneratorType pe090_source{Pe090GeneratorType::infinite};
    NetherStage nether_stage{NetherStage::base};
    std::optional<std::uint32_t> verify_seed;
    std::optional<std::string> candidates_path;
    std::optional<std::uint32_t> scan_start;
    std::optional<std::uint64_t> scan_count;
    std::string observation_path;
    std::optional<std::filesystem::path> output_path;
    unsigned int threads{};
};

struct Mismatch final {
    Observation observation;
    std::uint8_t actual_block{};
};

struct Pe061ColumnCheck final {
    std::int32_t chunk_x{};
    std::int32_t chunk_z{};
    std::vector<std::pair<std::int32_t, bool>> blocks;
};

struct Pe090CaveCheck final {
    std::int32_t chunk_x{};
    std::int32_t chunk_z{};
    std::vector<CavePoint> points;
};

struct FastPrefilterPlan final {
    std::vector<Pe061ColumnCheck> pe061_first_columns;
    std::vector<Pe090CaveCheck> pe090_cave_checks;
    std::optional<Observation> end_rare_solid;
};

[[nodiscard]] std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

[[nodiscard]] bool parse_u32(std::string_view text, std::uint32_t& output) noexcept {
    text = trim(text);
    if (text.empty()) {
        return false;
    }
    if (text.front() == '-') {
        std::int64_t parsed{};
        const auto [end, error] = std::from_chars(
            text.data(), text.data() + text.size(), parsed, 10);
        if (error != std::errc{} || end != text.data() + text.size()
            || parsed < std::numeric_limits<std::int32_t>::min()
            || parsed > std::numeric_limits<std::int32_t>::max()) {
            return false;
        }
        output = static_cast<std::uint32_t>(static_cast<std::int32_t>(parsed));
        return true;
    }
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text.remove_prefix(2);
    }
    std::uint64_t parsed{};
    const auto [end, error] = std::from_chars(
        text.data(), text.data() + text.size(), parsed, base);
    if (error != std::errc{} || end != text.data() + text.size()
        || parsed > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    output = static_cast<std::uint32_t>(parsed);
    return true;
}

[[nodiscard]] bool parse_u64(std::string_view text, std::uint64_t& output) noexcept {
    text = trim(text);
    if (text.empty() || text.front() == '-') {
        return false;
    }
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text.remove_prefix(2);
    }
    const auto [end, error] = std::from_chars(
        text.data(), text.data() + text.size(), output, base);
    return error == std::errc{} && end == text.data() + text.size();
}

[[nodiscard]] std::string next_argument(int& index, int argc, char** argv, std::string_view flag) {
    if (++index >= argc) {
        throw std::runtime_error("Missing value after " + std::string(flag) + ".");
    }
    return argv[index];
}

[[nodiscard]] Version parse_supported_version(std::string_view text) {
    const auto parsed = mcpe::worldgen::parse_version(text);
    if (!parsed.has_value() || (*parsed != Version::pe_0_6_1
        && *parsed != Version::pe_0_9_0 && *parsed != Version::pe_1_1_5_0)) {
        throw std::runtime_error(
            "This build supports PE 0.6.1/0.9.0 Overworld and PE 1.1.5.0 End/Nether terrain.");
    }
    return *parsed;
}

[[nodiscard]] DimensionId parse_dimension(std::string_view text) {
    if (text == "overworld") {
        return DimensionId::overworld;
    }
    if (text == "end") {
        return DimensionId::end;
    }
    if (text == "nether") {
        return DimensionId::nether;
    }
    throw std::runtime_error("--dimension must be overworld, end, or nether.");
}

[[nodiscard]] Pe090GeneratorType parse_pe090_source(std::string_view text) {
    if (text == "infinite") {
        return Pe090GeneratorType::infinite;
    }
    if (text == "legacy") {
        return Pe090GeneratorType::legacy;
    }
    throw std::runtime_error("--pe090-source must be infinite or legacy.");
}

[[nodiscard]] NetherStage parse_nether_stage(std::string_view text) {
    if (text == "base") {
        return NetherStage::base;
    }
    if (text == "populated") {
        return NetherStage::populated;
    }
    throw std::runtime_error("--nether-stage must be base or populated.");
}

void print_usage() {
    std::cout
        << "MCPE terrain verifier\n\n"
        << "Usage:\n"
        << "  mcpe_terrain_verifier --verify VERSION SEED OBSERVATIONS.txt [--dimension overworld|end|nether]\n"
        << "  mcpe_terrain_verifier --filter VERSION CANDIDATES.txt OBSERVATIONS.txt [options]\n"
        << "  mcpe_terrain_verifier --scan VERSION START COUNT OBSERVATIONS.txt [options]\n\n"
        << "  mcpe_terrain_verifier --all VERSION OBSERVATIONS.txt [options]\n\n"
        << "Observation lines are: x y z value\n"
        << "  value 0 = exact air; value 1 = any non-air terrain block.\n"
        << "  0.6.1 observations must be inside its finite 256x256 Overworld.\n"
        << "  0.9.0 defaults to the infinite source; use --pe090-source legacy when needed.\n"
        << "  1.1.5.0 requires --dimension end or nether.\n"
        << "  options: --dimension DIMENSION --pe090-source infinite|legacy\n"
        << "           --nether-stage base|populated\n"
        << "           --threads N --output PATH\n"
        << "  --all scans all 2^32 seeds exactly and may take a long time.\n";
}

[[nodiscard]] Options parse_options(int argc, char** argv) {
    if (argc == 1) {
        print_usage();
        throw std::runtime_error("No operation supplied.");
    }
    Options options;
    bool operation_seen = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            print_usage();
            std::exit(0);
        }
        if (argument == "--verify") {
            if (operation_seen) {
                throw std::runtime_error("Choose exactly one of --verify, --filter, or --scan.");
            }
            operation_seen = true;
            options.operation = Operation::verify;
            options.version = parse_supported_version(next_argument(index, argc, argv, argument));
            const std::string seed = next_argument(index, argc, argv, argument);
            std::uint32_t value{};
            if (!parse_u32(seed, value)) {
                throw std::runtime_error("Invalid 32-bit seed: " + seed);
            }
            options.verify_seed = value;
            options.observation_path = next_argument(index, argc, argv, argument);
        } else if (argument == "--filter") {
            if (operation_seen) {
                throw std::runtime_error("Choose exactly one of --verify, --filter, or --scan.");
            }
            operation_seen = true;
            options.operation = Operation::filter;
            options.version = parse_supported_version(next_argument(index, argc, argv, argument));
            options.candidates_path = next_argument(index, argc, argv, argument);
            options.observation_path = next_argument(index, argc, argv, argument);
        } else if (argument == "--scan") {
            if (operation_seen) {
                throw std::runtime_error("Choose exactly one of --verify, --filter, or --scan.");
            }
            operation_seen = true;
            options.operation = Operation::scan;
            options.version = parse_supported_version(next_argument(index, argc, argv, argument));
            const std::string start = next_argument(index, argc, argv, argument);
            const std::string count = next_argument(index, argc, argv, argument);
            std::uint32_t start_value{};
            std::uint64_t count_value{};
            if (!parse_u32(start, start_value) || !parse_u64(count, count_value)
                || count_value == 0 || count_value > (std::uint64_t{1} << 32U)
                || static_cast<std::uint64_t>(start_value) + count_value > (std::uint64_t{1} << 32U)) {
                throw std::runtime_error("--scan requires an in-range START and COUNT without 32-bit wrap.");
            }
            options.scan_start = start_value;
            options.scan_count = count_value;
            options.observation_path = next_argument(index, argc, argv, argument);
        } else if (argument == "--all") {
            if (operation_seen) {
                throw std::runtime_error("Choose exactly one of --verify, --filter, --scan, or --all.");
            }
            operation_seen = true;
            options.operation = Operation::scan;
            options.version = parse_supported_version(next_argument(index, argc, argv, argument));
            options.scan_start = 0U;
            options.scan_count = std::uint64_t{1} << 32U;
            options.observation_path = next_argument(index, argc, argv, argument);
        } else if (argument == "--pe090-source") {
            options.pe090_source = parse_pe090_source(next_argument(index, argc, argv, argument));
        } else if (argument == "--dimension") {
            options.dimension = parse_dimension(next_argument(index, argc, argv, argument));
        } else if (argument == "--nether-stage") {
            options.nether_stage = parse_nether_stage(
                next_argument(index, argc, argv, argument));
        } else if (argument == "--threads") {
            std::uint64_t value{};
            const std::string count = next_argument(index, argc, argv, argument);
            if (!parse_u64(count, value) || value == 0 || value > std::numeric_limits<unsigned int>::max()) {
                throw std::runtime_error("--threads requires a positive machine-sized integer.");
            }
            options.threads = static_cast<unsigned int>(value);
        } else if (argument == "--output") {
            options.output_path = next_argument(index, argc, argv, argument);
        } else {
            throw std::runtime_error("Unknown option: " + std::string(argument));
        }
    }
    if (!operation_seen) {
        throw std::runtime_error("Choose --verify, --filter, --scan, or --all.");
    }
    if (options.version == Version::pe_1_1_5_0
        && options.dimension != DimensionId::end
        && options.dimension != DimensionId::nether) {
        throw std::runtime_error("PE 1.1.5.0 terrain verification requires --dimension end or nether.");
    }
    if (options.version != Version::pe_1_1_5_0
        && options.dimension != DimensionId::overworld) {
        throw std::runtime_error("PE 0.6.1 and 0.9.0 support --dimension overworld only.");
    }
    if (options.nether_stage != NetherStage::base
        && (options.version != Version::pe_1_1_5_0
            || options.dimension != DimensionId::nether)) {
        throw std::runtime_error("--nether-stage populated requires PE 1.1.5.0 --dimension nether.");
    }
    return options;
}

void split_coordinate(std::int32_t coordinate, std::int32_t& chunk, std::int32_t& local) noexcept {
    chunk = coordinate / Chunk::width;
    local = coordinate % Chunk::width;
    if (local < 0) {
        --chunk;
        local += Chunk::width;
    }
}

[[nodiscard]] std::vector<Observation> load_observations(const Options& options) {
    std::ifstream input(options.observation_path);
    if (!input) {
        throw std::runtime_error("Cannot read observation file: " + options.observation_path);
    }
    std::map<std::tuple<std::int32_t, std::int32_t, std::int32_t>, bool> seen;
    std::vector<Observation> observations;
    std::string line;
    std::size_t line_number{};
    while (std::getline(input, line)) {
        ++line_number;
        if (const std::size_t comment = line.find('#'); comment != std::string::npos) {
            line.resize(comment);
        }
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream fields{line};
        std::int64_t x{};
        std::int64_t y{};
        std::int64_t z{};
        std::int64_t value{};
        std::string extra;
        if (!(fields >> x >> y >> z >> value)) {
            if (trim(line).empty()) {
                continue;
            }
            throw std::runtime_error("Expected `x y z value` at observation line " + std::to_string(line_number) + ".");
        }
        if ((fields >> extra) || x < std::numeric_limits<std::int32_t>::min()
            || x > std::numeric_limits<std::int32_t>::max()
            || z < std::numeric_limits<std::int32_t>::min()
            || z > std::numeric_limits<std::int32_t>::max()
            || y < 0 || y >= Chunk::height || (value != 0 && value != 1)) {
            throw std::runtime_error("Invalid terrain observation at line " + std::to_string(line_number)
                + "; y must be 0..127 and value must be 0 or 1.");
        }
        Observation observation{
            .x = static_cast<std::int32_t>(x),
            .y = static_cast<std::int32_t>(y),
            .z = static_cast<std::int32_t>(z),
            .non_air = value == 1,
        };
        split_coordinate(observation.x, observation.chunk_x, observation.local_x);
        split_coordinate(observation.z, observation.chunk_z, observation.local_z);
        if (options.version == Version::pe_0_6_1
            && (observation.chunk_x < 0 || observation.chunk_x >= 16
                || observation.chunk_z < 0 || observation.chunk_z >= 16)) {
            throw std::runtime_error("0.6.1 is a finite 16x16 chunk Overworld; observation line "
                + std::to_string(line_number) + " is outside x/z 0..255.");
        }
        const auto key = std::tuple{observation.x, observation.y, observation.z};
        const auto [found, inserted] = seen.emplace(key, observation.non_air);
        if (!inserted) {
            if (found->second != observation.non_air) {
                throw std::runtime_error("Conflicting observations at line " + std::to_string(line_number) + ".");
            }
            continue;
        }
        observations.push_back(observation);
    }
    if (observations.empty()) {
        throw std::runtime_error("The observation file contained no terrain blocks.");
    }
    return observations;
}

[[nodiscard]] std::vector<std::uint32_t> load_candidates(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot read candidate file: " + path);
    }
    std::vector<std::uint32_t> candidates;
    std::string line;
    std::size_t line_number{};
    while (std::getline(input, line)) {
        ++line_number;
        const std::string_view view = trim(line);
        if (view.empty() || view.front() == '#') {
            continue;
        }
        constexpr std::string_view prefix = "unsigned=";
        const std::size_t marker = view.find(prefix);
        std::string_view token = marker == std::string_view::npos ? view : view.substr(marker + prefix.size());
        token = token.substr(0, token.find_first_of(" \t\r\n"));
        std::uint32_t seed{};
        if (!parse_u32(token, seed)) {
            throw std::runtime_error("Expected a 32-bit seed at candidate line " + std::to_string(line_number) + ".");
        }
        candidates.push_back(seed);
    }
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    if (candidates.empty()) {
        throw std::runtime_error("The candidate file contained no seeds.");
    }
    return candidates;
}

[[nodiscard]] FastPrefilterPlan build_fast_prefilter_plan(
    const Options& options,
    const std::vector<Observation>& observations) {
    FastPrefilterPlan plan;
    if (options.version == Version::pe_0_6_1) {
        std::map<std::pair<std::int32_t, std::int32_t>,
                 std::vector<std::pair<std::int32_t, bool>>> columns;
        for (const Observation& observation : observations) {
            if (observation.local_x == 0 && observation.local_z == 0) {
                columns[{observation.chunk_x, observation.chunk_z}].push_back(
                    {observation.y, observation.non_air});
            }
        }
        for (auto& [chunk, blocks] : columns) {
            plan.pe061_first_columns.push_back({
                chunk.first, chunk.second, std::move(blocks)});
        }
    } else if (options.version == Version::pe_0_9_0) {
        std::map<std::pair<std::int32_t, std::int32_t>,
                 std::vector<CavePoint>> chunks;
        for (const Observation& observation : observations) {
            if (!observation.non_air
                && observation.y >= 11
                && observation.y < 63) {
                chunks[{observation.chunk_x, observation.chunk_z}].push_back({
                    observation.local_x,
                    observation.y,
                    observation.local_z,
                });
            }
        }
        for (auto& [chunk, points] : chunks) {
            plan.pe090_cave_checks.push_back({
                chunk.first, chunk.second, std::move(points)});
        }
    } else if (options.version == Version::pe_1_1_5_0
               && options.dimension == DimensionId::end) {
        for (const Observation& observation : observations) {
            if (observation.non_air
                && (!plan.end_rare_solid.has_value()
                    || observation.y > plan.end_rare_solid->y)) {
                plan.end_rare_solid = observation;
            }
        }
    }
    return plan;
}

[[nodiscard]] bool use_populated_nether(const Options& options) noexcept {
    return options.version == Version::pe_1_1_5_0
        && options.dimension == DimensionId::nether
        && options.nether_stage == NetherStage::populated;
}

[[nodiscard]] constexpr std::int32_t wrapping_offset(
    std::int32_t value,
    std::int32_t offset) noexcept {
    return static_cast<std::int32_t>(
        static_cast<std::uint32_t>(value)
        + static_cast<std::uint32_t>(offset));
}

template <typename MismatchHandler>
[[nodiscard]] bool visit_observations(
    std::uint32_t seed,
    const Options& options,
    const std::vector<Observation>& observations,
    MismatchHandler&& handler) {
    TerrainSource generator{
        seed,
        {options.version, options.dimension, options.pe090_source}};
    if (use_populated_nether(options)) {
        // Native chunk population is cache-backed and decorators can write
        // across a chunk edge. Populate the immediate source halo first, in a
        // stable ChunkPos order (X, then Z), then sample observed chunks only.
        // A saved region's actual request order is runtime state rather than
        // seed data; this is the explicitly reproducible canonical order.
        std::set<std::pair<std::int32_t, std::int32_t>> source_chunks;
        std::set<std::pair<std::int32_t, std::int32_t>> observed_chunks;
        for (const Observation& observation : observations) {
            observed_chunks.emplace(observation.chunk_x, observation.chunk_z);
            for (std::int32_t offset_x = -1; offset_x <= 1; ++offset_x) {
                for (std::int32_t offset_z = -1; offset_z <= 1; ++offset_z) {
                    source_chunks.emplace(
                        wrapping_offset(observation.chunk_x, offset_x),
                        wrapping_offset(observation.chunk_z, offset_z));
                }
            }
        }
        for (const auto& key : source_chunks) {
            generator.populate_chunk(key.first, key.second);
        }
        std::map<std::pair<std::int32_t, std::int32_t>, const Chunk*> chunks;
        for (const auto& key : observed_chunks) {
            const Chunk* const chunk = generator.find_chunk(key.first, key.second);
            if (chunk == nullptr) {
                throw std::logic_error("Populated Nether chunk was not retained by the generator.");
            }
            chunks.emplace(key, chunk);
        }
        for (const Observation& observation : observations) {
            const Chunk* const chunk = chunks.at(
                {observation.chunk_x, observation.chunk_z});
            const std::uint8_t block = chunk->block(
                observation.local_x, observation.y, observation.local_z);
            const bool matches = observation.non_air ? block != 0U : block == 0U;
            if (!matches && !std::invoke(handler, Mismatch{observation, block})) {
                return false;
            }
        }
        return true;
    }
    std::map<std::pair<std::int32_t, std::int32_t>, Chunk> chunks;
    for (const Observation& observation : observations) {
        const auto key = std::pair{observation.chunk_x, observation.chunk_z};
        auto found = chunks.find(key);
        if (found == chunks.end()) {
            Chunk chunk = generator.terrain_chunk(
                observation.chunk_x, observation.chunk_z);
            found = chunks.emplace(key, chunk).first;
        }
        const std::uint8_t block = found->second.block(
            observation.local_x, observation.y, observation.local_z);
        const bool matches = observation.non_air ? block != 0U : block == 0U;
        if (!matches && !std::invoke(handler, Mismatch{observation, block})) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool matches(
    std::uint32_t seed,
    const Options& options,
    const std::vector<Observation>& observations,
    const FastPrefilterPlan& plan) {
    if (!plan.pe061_first_columns.empty()) {
        mcpe::terrain_verifier::Pe061TerrainSource source(seed);
        for (const Pe061ColumnCheck& check : plan.pe061_first_columns) {
            const auto column = source.generate_first_column(
                check.chunk_x, check.chunk_z);
            for (const auto [y, expected_non_air] : check.blocks) {
                const bool non_air = column[static_cast<std::size_t>(y)] != 0U;
                if (non_air != expected_non_air) {
                    return false;
                }
            }
        }
    }
    if (!plan.pe090_cave_checks.empty()
        && options.pe090_source == Pe090GeneratorType::legacy) {
        return false;
    }
    for (const Pe090CaveCheck& check : plan.pe090_cave_checks) {
        // Below sea level raw PE 0.9 terrain is rock or water and every
        // surface path remains non-air. At block Y 11..62, only the cave
        // pass can produce final air (Y <= 10 becomes cave lava).
        if (!mcpe::terrain_verifier::pe090_caves_may_reach_all(
                seed, check.chunk_x, check.chunk_z, check.points)) {
            return false;
        }
    }
    if (plan.end_rare_solid.has_value()) {
        mcpe::terrain_verifier::EndPointSource source(seed);
        if (!source.non_air(
                plan.end_rare_solid->x,
                plan.end_rare_solid->y,
                plan.end_rare_solid->z)) {
            return false;
        }
    }
    return visit_observations(seed, options, observations, [](const Mismatch&) { return false; });
}

[[nodiscard]] std::vector<std::uint32_t> filter_candidates(
    const std::vector<std::uint32_t>& candidates,
    const Options& options,
    const std::vector<Observation>& observations,
    const FastPrefilterPlan& plan) {
    std::vector<unsigned char> accepted(candidates.size(), 0U);
    const unsigned int requested_workers = options.threads == 0U
        ? std::max(1U, std::thread::hardware_concurrency())
        : options.threads;
    const unsigned int worker_count = static_cast<unsigned int>(std::min(
        static_cast<std::size_t>(requested_workers), candidates.size()));
    std::atomic_size_t next{};
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for (unsigned int worker = 0; worker < worker_count; ++worker) {
        workers.emplace_back([&] {
            constexpr std::size_t batch_size = 32U;
            for (;;) {
                const std::size_t begin = next.fetch_add(
                    batch_size, std::memory_order_relaxed);
                if (begin >= candidates.size()) {
                    return;
                }
                const std::size_t end = std::min(
                    begin + batch_size, candidates.size());
                for (std::size_t index = begin; index < end; ++index) {
                    accepted[index] = matches(
                        candidates[index], options, observations, plan)
                        ? 1U : 0U;
                }
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }
    std::vector<std::uint32_t> result;
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        if (accepted[index] != 0U) {
            result.push_back(candidates[index]);
        }
    }
    return result;
}

[[nodiscard]] std::vector<std::uint32_t> scan_range(
    std::uint32_t start,
    std::uint64_t count,
    const Options& options,
    const std::vector<Observation>& observations,
    const FastPrefilterPlan& plan) {
    const unsigned int requested_workers = options.threads == 0U
        ? std::max(1U, std::thread::hardware_concurrency())
        : options.threads;
    const unsigned int worker_count = static_cast<unsigned int>(std::min(
        static_cast<std::uint64_t>(requested_workers), count));
    std::atomic_uint64_t next{};
    std::atomic_uint64_t completed{};
    std::atomic_uint active_workers{worker_count};
    std::mutex progress_mutex;
    std::condition_variable progress_changed;
    std::vector<std::vector<std::uint32_t>> matches_by_worker(worker_count);
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for (unsigned int worker = 0; worker < worker_count; ++worker) {
        workers.emplace_back([&, worker] {
            std::vector<std::uint32_t>& local_matches = matches_by_worker[worker];
            constexpr std::uint64_t batch_size = 64U;
            for (;;) {
                const std::uint64_t begin = next.fetch_add(
                    batch_size, std::memory_order_relaxed);
                if (begin >= count) {
                    break;
                }
                const std::uint64_t end = std::min(begin + batch_size, count);
                for (std::uint64_t offset = begin; offset < end; ++offset) {
                    const std::uint32_t seed = static_cast<std::uint32_t>(
                        static_cast<std::uint64_t>(start) + offset);
                    if (matches(seed, options, observations, plan)) {
                        local_matches.push_back(seed);
                    }
                }
                completed.fetch_add(
                    end - begin, std::memory_order_relaxed);
            }
            if (active_workers.fetch_sub(1U, std::memory_order_acq_rel) == 1U) {
                progress_changed.notify_one();
            }
        });
    }

    const auto print_progress = [count](std::uint64_t done, bool final) {
        const long double percentage = 100.0L
            * static_cast<long double>(done)
            / static_cast<long double>(count);
        std::ostringstream line;
        line << "Progress: " << std::fixed << std::setprecision(2)
             << static_cast<double>(percentage) << "% ("
             << done << '/' << count << ')';
        std::cerr << '\r' << line.str();
        if (final) {
            std::cerr << '\n';
        }
        std::cerr.flush();
    };

    print_progress(0U, false);
    {
        std::unique_lock lock{progress_mutex};
        while (active_workers.load(std::memory_order_acquire) != 0U) {
            progress_changed.wait_for(lock, std::chrono::milliseconds{250}, [&] {
                return active_workers.load(std::memory_order_acquire) == 0U;
            });
            if (active_workers.load(std::memory_order_acquire) != 0U) {
                print_progress(completed.load(std::memory_order_relaxed), false);
            }
        }
    }
    for (std::thread& worker : workers) {
        worker.join();
    }
    print_progress(completed.load(std::memory_order_relaxed), true);
    std::vector<std::uint32_t> result;
    for (const auto& local_matches : matches_by_worker) {
        result.insert(result.end(), local_matches.begin(), local_matches.end());
    }
    std::sort(result.begin(), result.end());
    return result;
}

[[nodiscard]] std::filesystem::path default_result_path(Version version) {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::ostringstream name;
    name << "terrain-" << mcpe::worldgen::to_string(version) << '-'
         << std::put_time(&local, "%y%m%d-%H%M%S") << ".txt";
    return std::filesystem::path{"results"} / name.str();
}

void save_candidates(
    const std::vector<std::uint32_t>& candidates,
    const Options& options) {
    const std::filesystem::path requested = options.output_path.value_or(
        default_result_path(options.version));
    std::filesystem::create_directories(
        requested.parent_path().empty() ? "." : requested.parent_path());
    std::filesystem::path path = requested;
    for (unsigned int suffix = 1; std::filesystem::exists(path); ++suffix) {
        path = requested.parent_path()
            / (requested.stem().string() + "-" + std::to_string(suffix)
                + requested.extension().string());
    }
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Cannot create result file: " + path.string());
    }
    for (const std::uint32_t seed : candidates) {
        output << "unsigned=" << seed
               << " signed=" << static_cast<std::int32_t>(seed)
               << " hex=0x" << std::hex << std::uppercase << seed << std::dec << '\n';
    }
    std::cout << "Saved " << candidates.size() << " candidate(s) to " << path.string() << '\n';
}

void print_candidates(const std::vector<std::uint32_t>& candidates) {
    for (const std::uint32_t seed : candidates) {
        std::cout << "unsigned=" << seed
                  << " signed=" << static_cast<std::int32_t>(seed)
                  << " hex=0x" << std::hex << std::uppercase << seed << std::dec << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        const std::vector<Observation> observations = load_observations(options);
        if (options.operation == Operation::verify) {
            std::vector<Mismatch> mismatches;
            static_cast<void>(visit_observations(*options.verify_seed, options, observations,
                [&mismatches](const Mismatch& mismatch) {
                    mismatches.push_back(mismatch);
                    return true;
                }));
            std::cout << "MCPE " << mcpe::worldgen::to_string(options.version)
                      << (options.dimension == DimensionId::end ? " End"
                          : options.dimension == DimensionId::nether ? " Nether"
                          : " Overworld")
                      << " terrain verification: " << observations.size()
                      << " observation(s), " << mismatches.size() << " mismatch(es).\n";
            for (const Mismatch& mismatch : mismatches) {
                std::cout << "mismatch x=" << mismatch.observation.x
                          << " y=" << mismatch.observation.y
                          << " z=" << mismatch.observation.z
                          << " expected=" << (mismatch.observation.non_air ? 1 : 0)
                          << " actual_block=" << static_cast<unsigned int>(mismatch.actual_block) << '\n';
            }
            return mismatches.empty() ? 0 : 1;
        }

        std::vector<std::uint32_t> result;
        const FastPrefilterPlan prefilter_plan = build_fast_prefilter_plan(
            options, observations);
        if (options.operation == Operation::filter) {
            const std::vector<std::uint32_t> candidates = load_candidates(*options.candidates_path);
            std::cout << "Filtering " << candidates.size() << " candidate(s) with "
                      << observations.size() << " terrain observation(s).\n";
            result = filter_candidates(
                candidates, options, observations, prefilter_plan);
        } else {
            std::cout << "Scanning " << *options.scan_count << " seed(s) with "
                      << observations.size() << " terrain observation(s).\n";
            result = scan_range(
                *options.scan_start,
                *options.scan_count,
                options,
                observations,
                prefilter_plan);
        }
        std::cout << "Terrain filter complete: " << result.size() << " candidate(s).\n";
        print_candidates(result);
        save_candidates(result, options);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 2;
    }
}
