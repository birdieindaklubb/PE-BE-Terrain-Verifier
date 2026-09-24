#pragma once

#include <cstdint>

namespace mcpe::worldgen::v1_1_5_0::block {

constexpr std::uint8_t air = 0;
constexpr std::uint8_t bedrock = 7;
constexpr std::uint8_t flowing_lava = 10;
constexpr std::uint8_t still_lava = 11;
constexpr std::uint8_t gravel = 13;
constexpr std::uint8_t brown_mushroom = 39;
constexpr std::uint8_t red_mushroom = 40;
constexpr std::uint8_t obsidian = 49;
constexpr std::uint8_t torch = 50;
constexpr std::uint8_t fire = 51;
constexpr std::uint8_t mob_spawner = 52;
constexpr std::uint8_t chest = 54;
constexpr std::uint8_t netherrack = 87;
constexpr std::uint8_t soul_sand = 88;
constexpr std::uint8_t glowstone = 89;
constexpr std::uint8_t nether_brick = 112;
constexpr std::uint8_t nether_brick_fence = 113;
constexpr std::uint8_t nether_brick_stairs = 114;
constexpr std::uint8_t nether_wart = 115;
constexpr std::uint8_t iron_bars = 101;
constexpr std::uint8_t end_portal = 119;
constexpr std::uint8_t end_stone = 121;
constexpr std::uint8_t quartz_ore = 153;
// PE 1.1.5 registers the chorus plant after the glazed-terracotta range at
// ID 240.  ID 199 is ItemFrameBlock in this native registry; do not use the
// later Java/Bedrock-era numeric association here.
constexpr std::uint8_t chorus_plant = 240;
constexpr std::uint8_t chorus_flower = 200;
constexpr std::uint8_t end_gateway = 209;
constexpr std::uint8_t magma_block = 213;

} // namespace mcpe::worldgen::v1_1_5_0::block
