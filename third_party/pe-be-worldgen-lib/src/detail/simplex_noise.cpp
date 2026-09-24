#include "detail/simplex_noise.hpp"

#include "detail/fp.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <bit>

namespace mcpe::worldgen::detail {
namespace {

// These are the exact binary32 constants installed by PE 0.9.0's static
// initializer.  The 3-D kernel uses the separately encoded 1/3 and 1/6.
constexpr float kOneThird = 0.33333334F;
constexpr float kOneSixth = 0.16666667F;
constexpr float kAttenuation = 0.6F;
constexpr float kOutputScale = 32.0F;
// Exact binary32 SimplexNoise::F2 and ::G2 globals observed in the
// symbol-rich matching Bedrock server build and used by the APK's 2-D path.
// Keep them as literals rather than deriving them from host math routines.
constexpr float kSimplexF2 = std::bit_cast<float>(0x3ebb67aeU);
constexpr float kSimplexG2 = std::bit_cast<float>(0x3e58658cU);
constexpr float kOutputScale2d = 70.0F;

constexpr std::array<std::array<std::int32_t, 3>, 12> kGradients{{
    {{1, 1, 0}}, {{-1, 1, 0}}, {{1, -1, 0}}, {{-1, -1, 0}},
    {{1, 0, 1}}, {{-1, 0, 1}}, {{1, 0, -1}}, {{-1, 0, -1}},
    {{0, 1, 1}}, {{0, -1, 1}}, {{0, 1, -1}}, {{0, -1, -1}},
}};

[[nodiscard]] constexpr std::uint32_t wrap_256(std::int32_t value) noexcept {
    return static_cast<std::uint32_t>(value) & 0xffU;
}

[[nodiscard]] std::int32_t wrapping_add(
    std::int32_t left,
    std::int32_t right) noexcept {
    return std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
}

[[nodiscard]] std::int32_t wrapping_sub(
    std::int32_t left,
    std::int32_t right) noexcept {
    return std::bit_cast<std::int32_t>(
        static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right));
}

// PE's SimplexNoise keeps the original fastfloor spelling: it subtracts one
// from every non-positive truncated value, including exactly zero.
[[nodiscard]] std::int32_t simplex_floor(float value) noexcept {
    const std::int32_t truncated = fp::trunc_to_i32(value);
    return value <= 0.0F ? wrapping_sub(truncated, 1) : truncated;
}

[[nodiscard]] float contribution3d(
    float x,
    float y,
    float z,
    float gradient) noexcept {
    // The ARM code subtracts z, then x, then y.
    float attenuation = fp::sub(kAttenuation, fp::mul(z, z));
    attenuation = fp::sub(attenuation, fp::mul(x, x));
    attenuation = fp::sub(attenuation, fp::mul(y, y));
    if (attenuation < 0.0F) {
        return 0.0F;
    }

    const float attenuation_square = fp::mul(attenuation, attenuation);
    const float attenuation_fourth =
        fp::mul(attenuation_square, attenuation_square);
    return fp::mul(attenuation_fourth, gradient);
}

[[nodiscard]] float contribution2d(
    float x,
    float z,
    float gradient) noexcept {
    // SimplexNoise::_getValue(Vec2) subtracts X squared from 0.5 before
    // subtracting Z squared.  Reordering this as 0.5 - (X2 + Z2) changes the
    // rounded attenuator at island-field boundaries.
    float attenuation = fp::sub(0.5F, fp::mul(x, x));
    attenuation = fp::sub(attenuation, fp::mul(z, z));
    if (attenuation < 0.0F) {
        return 0.0F;
    }

    const float attenuation_square = fp::mul(attenuation, attenuation);
    return fp::mul(fp::mul(attenuation_square, attenuation_square), gradient);
}

} // namespace

SimplexNoise::SimplexNoise(Mt19937& random) noexcept {
    // The APK constructs the three values in stream order Z, Y, X (its
    // stores are at offsets +8, +4, +0 respectively).  This is invisible to
    // the End's 2-D island field, but is observable to 3-D simplex callers.
    z_offset_ = fp::mul(random.next_float(), 256.0F);
    y_offset_ = fp::mul(random.next_float(), 256.0F);
    x_offset_ = fp::mul(random.next_float(), 256.0F);

    for (std::uint32_t index = 0; index < 256U; ++index) {
        permutation_[index] = index;
    }
    for (std::uint32_t index = 0; index < 256U; ++index) {
        const std::uint32_t selected = index + random.next_bounded(256U - index);
        std::swap(permutation_[index], permutation_[selected]);
        permutation_[index + 256U] = permutation_[index];
    }
}

float SimplexNoise::gradient(
    std::uint32_t hash,
    float x,
    float y,
    float z) const noexcept {
    const std::array<std::int32_t, 3>& gradient = kGradients[hash % 12U];
    float dot = fp::mul(x, static_cast<float>(gradient[0]));
    dot = fp::mul_add(dot, y, static_cast<float>(gradient[1]));
    return fp::mul_add(dot, z, static_cast<float>(gradient[2]));
}

float SimplexNoise::gradient2(
    std::uint32_t hash,
    float x,
    float z) const noexcept {
    const std::array<std::int32_t, 3>& gradient = kGradients[hash % 12U];
    // Native evaluates the Z product first, then accumulates the X product.
    return fp::mul_add(
        fp::mul(z, static_cast<float>(gradient[1])),
        x,
        static_cast<float>(gradient[0]));
}

float SimplexNoise::sample3d(
    float x,
    float y,
    float z,
    float skew) const noexcept {
    const std::int32_t cell_x = simplex_floor(fp::add(x, skew));
    const std::int32_t cell_y = simplex_floor(fp::add(y, skew));
    const std::int32_t cell_z = simplex_floor(fp::add(z, skew));

    const std::int32_t cell_sum = wrapping_add(
        wrapping_add(cell_x, cell_y), cell_z);
    const float unskew = fp::mul(static_cast<float>(cell_sum), kOneSixth);

    const float local_x = fp::sub(
        x, fp::sub(static_cast<float>(cell_x), unskew));
    const float local_y = fp::sub(
        y, fp::sub(static_cast<float>(cell_y), unskew));
    const float local_z = fp::sub(
        z, fp::sub(static_cast<float>(cell_z), unskew));

    std::int32_t first_x{};
    std::int32_t first_y{};
    std::int32_t first_z{};
    std::int32_t second_x{};
    std::int32_t second_y{};
    std::int32_t second_z{};

    if (local_x >= local_y) {
        if (local_y >= local_z) {
            first_x = 1;
            second_x = 1;
            second_y = 1;
        } else if (local_x >= local_z) {
            first_x = 1;
            second_x = 1;
            second_z = 1;
        } else {
            first_z = 1;
            second_x = 1;
            second_z = 1;
        }
    } else if (local_y < local_z) {
        first_z = 1;
        second_y = 1;
        second_z = 1;
    } else if (local_x < local_z) {
        first_y = 1;
        second_y = 1;
        second_z = 1;
    } else {
        first_y = 1;
        second_x = 1;
        second_y = 1;
    }

    const float local_x_1 = fp::sub(
        fp::add(local_x, kOneSixth), static_cast<float>(first_x));
    const float local_y_1 = fp::sub(
        fp::add(local_y, kOneSixth), static_cast<float>(first_y));
    const float local_z_1 = fp::sub(
        fp::add(local_z, kOneSixth), static_cast<float>(first_z));
    const float local_x_2 = fp::sub(
        fp::add(local_x, kOneThird), static_cast<float>(second_x));
    const float local_y_2 = fp::sub(
        fp::add(local_y, kOneThird), static_cast<float>(second_y));
    const float local_z_2 = fp::sub(
        fp::add(local_z, kOneThird), static_cast<float>(second_z));
    const float local_x_3 = fp::sub(local_x, 0.5F);
    const float local_y_3 = fp::sub(local_y, 0.5F);
    const float local_z_3 = fp::sub(local_z, 0.5F);

    const std::uint32_t px = wrap_256(cell_x);
    const std::uint32_t py = wrap_256(cell_y);
    const std::uint32_t pz = wrap_256(cell_z);
    const std::uint32_t hash0 = permutation_[
        px + permutation_[py + permutation_[pz]]];
    const std::uint32_t hash1 = permutation_[
        px + static_cast<std::uint32_t>(first_x)
        + permutation_[py + static_cast<std::uint32_t>(first_y)
            + permutation_[pz + static_cast<std::uint32_t>(first_z)]]];
    const std::uint32_t hash2 = permutation_[
        px + static_cast<std::uint32_t>(second_x)
        + permutation_[py + static_cast<std::uint32_t>(second_y)
            + permutation_[pz + static_cast<std::uint32_t>(second_z)]]];
    const std::uint32_t hash3 = permutation_[
        px + 1U + permutation_[py + 1U + permutation_[pz + 1U]]];

    const float sample0 = contribution3d(
        local_x, local_y, local_z, gradient(hash0, local_x, local_y, local_z));
    const float sample1 = contribution3d(
        local_x_1, local_y_1, local_z_1,
        gradient(hash1, local_x_1, local_y_1, local_z_1));
    const float sample2 = contribution3d(
        local_x_2, local_y_2, local_z_2,
        gradient(hash2, local_x_2, local_y_2, local_z_2));
    const float sample3 = contribution3d(
        local_x_3, local_y_3, local_z_3,
        gradient(hash3, local_x_3, local_y_3, local_z_3));
    return fp::add(fp::add(fp::add(sample1, sample0), sample2), sample3);
}

float SimplexNoise::value(float x, float z) const noexcept {
    const float skew = fp::mul(kSimplexF2, fp::add(x, z));
    const std::int32_t cell_x = simplex_floor(fp::add(x, skew));
    const std::int32_t cell_z = simplex_floor(fp::add(z, skew));
    const float unskew = fp::mul(
        static_cast<float>(wrapping_add(cell_x, cell_z)), kSimplexG2);
    const float local_x = fp::sub(
        x, fp::sub(static_cast<float>(cell_x), unskew));
    const float local_z = fp::sub(
        z, fp::sub(static_cast<float>(cell_z), unskew));

    const std::int32_t offset_x = local_x > local_z ? 1 : 0;
    const std::int32_t offset_z = local_x > local_z ? 0 : 1;
    const float local_x_1 = fp::sub(
        fp::add(local_x, kSimplexG2), static_cast<float>(offset_x));
    const float local_z_1 = fp::sub(
        fp::add(local_z, kSimplexG2), static_cast<float>(offset_z));
    const float final_offset = fp::sub(
        fp::add(kSimplexG2, kSimplexG2), 1.0F);
    const float local_x_2 = fp::add(local_x, final_offset);
    const float local_z_2 = fp::add(local_z, final_offset);

    const std::uint32_t px = wrap_256(cell_x);
    const std::uint32_t pz = wrap_256(cell_z);
    const std::uint32_t hash0 = permutation_[px + permutation_[pz]];
    const std::uint32_t hash1 = permutation_[
        px + static_cast<std::uint32_t>(offset_x)
        + permutation_[pz + static_cast<std::uint32_t>(offset_z)]];
    const std::uint32_t hash2 = permutation_[px + 1U + permutation_[pz + 1U]];

    const float sample0 = contribution2d(
        local_x, local_z, gradient2(hash0, local_x, local_z));
    const float sample1 = contribution2d(
        local_x_1, local_z_1, gradient2(hash1, local_x_1, local_z_1));
    const float sample2 = contribution2d(
        local_x_2, local_z_2, gradient2(hash2, local_x_2, local_z_2));
    return fp::mul(fp::add(fp::add(sample1, sample0), sample2), kOutputScale2d);
}

float SimplexNoise::value(float x, float y, float z) const noexcept {
    const float skew = fp::mul(
        fp::add(fp::add(y, z), x), kOneThird);
    return fp::mul(sample3d(x, y, z, skew), kOutputScale);
}

void SimplexNoise::add(
    std::span<float> output,
    float x,
    float y,
    float z,
    std::int32_t x_size,
    std::int32_t y_size,
    std::int32_t z_size,
    float x_scale,
    float y_scale,
    float z_scale,
    float amplitude) const noexcept {
    assert(x_size >= 0 && y_size >= 0 && z_size >= 0);
    const std::size_t required = static_cast<std::size_t>(x_size)
        * static_cast<std::size_t>(y_size)
        * static_cast<std::size_t>(z_size);
    assert(output.size() >= required);

    // The VFP implementation multiplies this factor before entering the
    // loops, then uses VMLA to add each finished simplex sample.
    const float scaled_amplitude = fp::mul(amplitude, kOutputScale);
    std::size_t output_index = 0;

    for (std::int32_t ix = 0; ix < x_size; ++ix) {
        const float scaled_x = fp::mul_add(
            x_offset_, fp::add(x, static_cast<float>(ix)), x_scale);

        for (std::int32_t iz = 0; iz < z_size; ++iz) {
            const float scaled_z = fp::mul_add(
                z_offset_, fp::add(z, static_cast<float>(iz)), z_scale);

            for (std::int32_t iy = 0; iy < y_size; ++iy) {
                const float scaled_y = fp::mul_add(
                    y_offset_, fp::add(y, static_cast<float>(iy)), y_scale);

                const float skew = fp::mul(
                    fp::add(fp::add(scaled_z, scaled_x), scaled_y), kOneThird);
                const float sample = sample3d(
                    scaled_x, scaled_y, scaled_z, skew);
                output[output_index] = fp::mul_add(
                    output[output_index], sample, scaled_amplitude);
                ++output_index;
            }
        }
    }
}

PerlinSimplexNoise::PerlinSimplexNoise(Mt19937& random, std::int32_t octaves) {
    assert(octaves >= 0);
    octaves_.reserve(static_cast<std::size_t>(octaves));
    for (std::int32_t octave = 0; octave < octaves; ++octave) {
        octaves_.emplace_back(random);
    }
}

float PerlinSimplexNoise::value(float x, float z) const noexcept {
    float value = 0.0F;
    float octave_scale = 1.0F;
    for (const SimplexNoise& octave : octaves_) {
        const float sample = octave.value(
            fp::mul(octave_scale, x), fp::mul(octave_scale, z));
        value = fp::add(value, fp::div(sample, octave_scale));
        octave_scale = fp::mul(octave_scale, 0.5F);
    }
    return value;
}

float PerlinSimplexNoise::value(float x, float y, float z) const noexcept {
    float value = 0.0F;
    float octave_scale = 1.0F;
    for (const SimplexNoise& octave : octaves_) {
        const float sample = octave.value(
            fp::mul(octave_scale, x),
            fp::mul(octave_scale, y),
            fp::mul(octave_scale, z));
        value = fp::add(value, fp::div(sample, octave_scale));
        octave_scale = fp::mul(octave_scale, 0.5F);
    }
    return value;
}

std::span<float> PerlinSimplexNoise::region(
    std::vector<float>& storage,
    float x,
    float y,
    float z,
    std::int32_t x_size,
    std::int32_t y_size,
    std::int32_t z_size,
    float x_scale,
    float y_scale,
    float z_scale) const {
    const std::size_t size = static_cast<std::size_t>(x_size)
        * static_cast<std::size_t>(y_size)
        * static_cast<std::size_t>(z_size);
    storage.assign(size, 0.0F);

    float octave_scale = 1.0F;
    for (const SimplexNoise& octave : octaves_) {
        octave.add(
            storage,
            x,
            y,
            z,
            x_size,
            y_size,
            z_size,
            fp::mul(x_scale, octave_scale),
            fp::mul(y_scale, octave_scale),
            fp::mul(z_scale, octave_scale),
            octave_scale);
        octave_scale = fp::mul(octave_scale, 0.5F);
    }
    return storage;
}

std::span<float> PerlinSimplexNoise::region2d(
    std::vector<float>& storage,
    std::int32_t x,
    std::int32_t z,
    std::int32_t x_size,
    std::int32_t z_size,
    float x_scale,
    float z_scale,
    float unused_amplitude) const {
    // This final ABI parameter is never read by PE 0.9.0.  Its two-dimensional
    // overload forwards to the 3-D path at y=0 with a single y sample.
    (void)unused_amplitude;
    return region(
        storage,
        static_cast<float>(x),
        0.0F,
        static_cast<float>(z),
        x_size,
        1,
        z_size,
        x_scale,
        1.0F,
        z_scale);
}

} // namespace mcpe::worldgen::detail
