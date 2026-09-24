cmake_minimum_required(VERSION 3.20)

project(mcpe_terrain_verifier LANGUAGES CXX)

include(CTest)

if(MSVC)
    # A release executable links the Microsoft runtime statically as well as
    # the terrain core below. There is no companion runtime DLL to ship.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(MCPE_WORLDGEN_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../mcpe-worldgen"
    CACHE PATH
    "Path to the bundled observable world-generation sources"
)

if(NOT EXISTS "${MCPE_WORLDGEN_SOURCE_DIR}/include/mcpe/worldgen/chunk.hpp")
    message(FATAL_ERROR
        "MCPE_WORLDGEN_SOURCE_DIR does not point to the observable "
        "world-generation source tree: ${MCPE_WORLDGEN_SOURCE_DIR}")
endif()

set(_worldgen_src "${MCPE_WORLDGEN_SOURCE_DIR}/src")

add_executable(mcpe_terrain_verifier
    src/main.cpp
    src/terrain_source.cpp
    src/pe061_terrain_source.cpp
    src/pe090_cave_prefilter.cpp
    src/end_terrain_source.cpp
    src/end_point_source.cpp
    ${_worldgen_src}/detail/mt19937.cpp
    ${_worldgen_src}/detail/noise.cpp
    ${_worldgen_src}/detail/simplex_noise.cpp

    ${_worldgen_src}/v0_6_1/legacy_math.cpp
    ${_worldgen_src}/v0_6_1/biome.cpp

    ${_worldgen_src}/v0_9_0/layer_rng.cpp
    ${_worldgen_src}/v0_9_0/biome_registry.cpp
    ${_worldgen_src}/v0_9_0/biome_layers.cpp
    ${_worldgen_src}/v0_9_0/base_terrain.cpp
    ${_worldgen_src}/v0_9_0/caves.cpp
    ${_worldgen_src}/v0_9_0/lighting.cpp
    ${_worldgen_src}/v0_9_0/mesa_surface.cpp
    ${_worldgen_src}/v0_9_0/surface.cpp
    ${_worldgen_src}/v0_9_0/surface_overrides.cpp
    ${_worldgen_src}/v0_9_0/structure_piece.cpp
    ${_worldgen_src}/v0_9_0/terrain_generator.cpp
    ${_worldgen_src}/v0_9_0/terrain_noise.cpp
    ${_worldgen_src}/v0_9_0/world.cpp

    ${_worldgen_src}/v1_1_5_0/end_terrain.cpp
    ${_worldgen_src}/v1_1_5_0/end_features.cpp

    # Required only by the explicitly selected populated-Nether mode.
    # Overworld and End dispatch never construct or call these stateful
    # World/feature/fortress sources.
    ${_worldgen_src}/v0_9_0/features.cpp
    ${_worldgen_src}/v0_9_0/liquid.cpp
    ${_worldgen_src}/v1_1_5_0/hell_caves.cpp
    ${_worldgen_src}/v1_1_5_0/hell_features.cpp
    ${_worldgen_src}/v1_1_5_0/hell_fortress.cpp
    ${_worldgen_src}/v1_1_5_0/hell_random_level_source.cpp
    ${_worldgen_src}/v1_1_5_0/hell_terrain.cpp
)

target_compile_features(mcpe_terrain_verifier PRIVATE cxx_std_20)
# Measured MSVC LTCG builds are substantially slower for this strict
# binary32-noise workload. Keep ordinary Release optimization even if a
# parent project enables IPO globally.
set_property(
    TARGET mcpe_terrain_verifier
    PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE)
target_include_directories(mcpe_terrain_verifier PRIVATE
    "${MCPE_WORLDGEN_SOURCE_DIR}/include"
    "${_worldgen_src}"
)

# The target APKs use non-fused IEEE-754 operations at terrain thresholds.
if(MSVC)
    target_compile_options(mcpe_terrain_verifier PRIVATE /fp:strict /Gy)
else()
    target_compile_options(mcpe_terrain_verifier PRIVATE
        -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
    target_link_options(mcpe_terrain_verifier PRIVATE -Wl,--gc-sections)
endif()

if(MSVC)
    target_link_options(mcpe_terrain_verifier PRIVATE /STACK:8388608)
elseif(MINGW)
    target_link_options(mcpe_terrain_verifier PRIVATE -Wl,--stack,8388608)
endif()

if(BUILD_TESTING)
    add_test(
        NAME pe061_exact_surface_regression
        COMMAND mcpe_terrain_verifier
            --verify 0.6.1 1788183909
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/pe061-surface-regression.txt"
    )
    add_test(
        NAME pe061_first_column_fast_path_regression
        COMMAND mcpe_terrain_verifier
            --scan 0.6.1 1788183909 1
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/pe061-first-column-regression.txt"
            --threads 1
            --output "${CMAKE_CURRENT_BINARY_DIR}/pe061-fast-path-results.txt"
    )
    set_tests_properties(
        pe061_first_column_fast_path_regression
        PROPERTIES PASS_REGULAR_EXPRESSION
            "Terrain filter complete: 1 candidate\\(s\\)\\."
    )
    add_test(
        NAME pe090_exact_cave_regression
        COMMAND mcpe_terrain_verifier
            --verify 0.9.0 1788587961
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/pe090-cave-regression.txt"
    )
    add_test(
        NAME pe090_legacy_selector_regression
        COMMAND mcpe_terrain_verifier
            --verify 0.9.0 1788587961
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/pe090-legacy-selector-regression.txt"
            --pe090-source legacy
    )
    add_test(
        NAME pe090_legacy_selector_rejects_infinite
        COMMAND mcpe_terrain_verifier
            --verify 0.9.0 1788587961
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/pe090-legacy-selector-regression.txt"
    )
    set_tests_properties(
        pe090_legacy_selector_rejects_infinite
        PROPERTIES WILL_FAIL TRUE
    )
    add_test(
        NAME pe115_exact_outer_end_island_regression
        COMMAND mcpe_terrain_verifier
            --verify 1.1.5.0 2714024796
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/pe115-end-outer-islands-regression.txt"
            --dimension end
    )
    add_test(
        NAME pe115_end_point_fast_path_regression
        COMMAND mcpe_terrain_verifier
            --scan 1.1.5.0 2714024796 1
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/pe115-end-point-regression.txt"
            --dimension end
            --threads 1
            --output "${CMAKE_CURRENT_BINARY_DIR}/pe115-end-point-results.txt"
    )
    set_tests_properties(
        pe115_end_point_fast_path_regression
        PROPERTIES PASS_REGULAR_EXPRESSION
            "Terrain filter complete: 1 candidate\\(s\\)\\."
    )
    add_test(
        NAME pe115_nether_terrain_and_cave_regression
        COMMAND mcpe_terrain_verifier
            --verify 1.1.5.0 305419896
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/pe115-nether-terrain-regression.txt"
            --dimension nether
    )
    add_test(
        NAME pe115_nether_population_halo_regression
        COMMAND mcpe_terrain_verifier
            --verify 1.1.5.0 305419896
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/pe115-nether-populated-regression.txt"
            --dimension nether
            --nether-stage populated
    )

    add_test(
        NAME pe061_overworld_filter_regression
        COMMAND mcpe_terrain_verifier
            --filter 0.6.1
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/regression-candidates.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/pe061-surface-regression.txt"
            --threads 2
            --output "${CMAKE_CURRENT_BINARY_DIR}/pe061-filter-results.txt"
    )
    set_tests_properties(
        pe061_overworld_filter_regression
        PROPERTIES PASS_REGULAR_EXPRESSION
            "Terrain filter complete: 1 candidate\\(s\\)\\."
    )

    add_test(
        NAME pe090_overworld_filter_regression
        COMMAND mcpe_terrain_verifier
            --filter 0.9.0
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/regression-candidates.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/pe090-cave-regression.txt"
            --threads 2
            --output "${CMAKE_CURRENT_BINARY_DIR}/pe090-filter-results.txt"
    )
    set_tests_properties(
        pe090_overworld_filter_regression
        PROPERTIES PASS_REGULAR_EXPRESSION
            "Terrain filter complete: 1 candidate\\(s\\)\\."
    )

    add_test(
        NAME pe115_end_filter_regression
        COMMAND mcpe_terrain_verifier
            --filter 1.1.5.0
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/regression-candidates.txt"
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/pe115-end-outer-islands-regression.txt"
            --dimension end
            --threads 2
            --output "${CMAKE_CURRENT_BINARY_DIR}/pe115-end-filter-results.txt"
    )
    set_tests_properties(
        pe115_end_filter_regression
        PROPERTIES PASS_REGULAR_EXPRESSION
            "Terrain filter complete: 1 candidate\\(s\\)\\."
    )

    add_test(
        NAME scan_progress_regression
        COMMAND mcpe_terrain_verifier
            --scan 0.6.1 1788183909 1
            "${CMAKE_CURRENT_SOURCE_DIR}/examples/pe061-surface-regression.txt"
            --threads 2
            --output "${CMAKE_CURRENT_BINARY_DIR}/scan-progress-results.txt"
    )
    set_tests_properties(
        scan_progress_regression
        PROPERTIES PASS_REGULAR_EXPRESSION
            "Progress: 100\\.00% \\(1/1\\)"
    )
endif()
