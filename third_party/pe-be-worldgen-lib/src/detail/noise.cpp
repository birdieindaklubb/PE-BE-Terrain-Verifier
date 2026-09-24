#include "detail/noise.hpp"

#include "detail/fp.hpp"

#include <algorithm>
#include <cassert>

namespace mcpe::worldgen::detail {
namespace {

[[nodiscard]] constexpr std::uint32_t wrap_256(std::int32_t value) noexcept {
    return static_cast<std::uint32_t>(value) & 0xffU;
}

// ImprovedNoise::_blendCubeCorners does not materialize each corner's dot
// product and then subtract it during the X lerp.  It keeps the two signed
// gradient terms separate and forms `(next_u - (current_u + current_v)) +
// next_v`.  That order is observable at binary32 density boundaries.
struct GradientTerms final {
    float u{};
    float v{};
};

[[nodiscard]] GradientTerms gradient_terms(
    std::uint32_t hash,
    float x,
    float y,
    float z) noexcept {
    const std::uint32_t h = hash & 15U;
    const float first = h < 8U ? x : y;
    const float second = h < 4U ? y : ((h == 12U || h == 14U) ? x : z);
    return {
        (h & 1U) == 0U ? first : -first,
        (h & 2U) == 0U ? second : -second,
    };
}

[[nodiscard]] float blend_x(
    GradientTerms current,
    GradientTerms next,
    float amount) noexcept {
    const float current_sum = fp::add(current.u, current.v);
    const float delta = fp::add(
        fp::sub(next.u, current_sum), next.v);
    return fp::mul_add(current_sum, amount, delta);
}

} // namespace

ImprovedNoise::ImprovedNoise(Mt19937& random) noexcept {
    x_offset_ = fp::mul(random.next_float(), 256.0F);
    y_offset_ = fp::mul(random.next_float(), 256.0F);
    z_offset_ = fp::mul(random.next_float(), 256.0F);

    for (std::uint32_t i = 0; i < 256U; ++i) {
        permutation_[i] = i;
    }
    for (std::uint32_t i = 0; i < 256U; ++i) {
        const std::uint32_t selected = i + random.next_bounded(256U - i);
        std::swap(permutation_[i], permutation_[selected]);
        permutation_[i + 256U] = permutation_[i];
    }
}

float ImprovedNoise::fade(float value) noexcept {
    const float square = fp::mul(value, value);
    const float cube = fp::mul(square, value);
    const float six_value = fp::mul(value, 6.0F);
    const float inner = fp::sub(six_value, 15.0F);
    const float polynomial = fp::mul_add(10.0F, value, inner);
    return fp::mul(cube, polynomial);
}

float ImprovedNoise::lerp(float amount, float a, float b) noexcept {
    return fp::mul_add(a, amount, fp::sub(b, a));
}

float ImprovedNoise::grad(
    std::uint32_t hash,
    float x,
    float y,
    float z) noexcept {
    const std::uint32_t h = hash & 15U;
    const float u = h < 8U ? x : y;
    const float v = h < 4U ? y : ((h == 12U || h == 14U) ? x : z);
    const float signed_u = (h & 1U) == 0U ? u : -u;
    const float signed_v = (h & 2U) == 0U ? v : -v;
    return fp::add(signed_u, signed_v);
}

float ImprovedNoise::grad2(std::uint32_t hash, float x, float z) noexcept {
    const std::uint32_t h = hash & 15U;
    const float u = h < 8U ? x : 0.0F;
    const float v = h < 4U ? 0.0F : ((h == 12U || h == 14U) ? x : z);
    const float signed_u = (h & 1U) == 0U ? u : -u;
    const float signed_v = (h & 2U) == 0U ? v : -v;
    return fp::add(signed_u, signed_v);
}

float ImprovedNoise::value(float x, float z) const noexcept {
    return value(x, z, 0.0F);
}

float ImprovedNoise::value(float x, float y, float z) const noexcept {
    const float shifted_x = fp::add(x, x_offset_);
    const float shifted_y = fp::add(y, y_offset_);
    const float shifted_z = fp::add(z, z_offset_);

    const std::int32_t floor_x = fp::floor_to_i32(shifted_x);
    const std::int32_t floor_y = fp::floor_to_i32(shifted_y);
    const std::int32_t floor_z = fp::floor_to_i32(shifted_z);

    const std::uint32_t x_index = wrap_256(floor_x);
    const std::uint32_t y_index = wrap_256(floor_y);
    const std::uint32_t z_index = wrap_256(floor_z);

    const float local_x = fp::sub(shifted_x, static_cast<float>(floor_x));
    const float local_y = fp::sub(shifted_y, static_cast<float>(floor_y));
    const float local_z = fp::sub(shifted_z, static_cast<float>(floor_z));
    const float u = fade(local_x);
    const float v = fade(local_y);
    const float w = fade(local_z);

    const std::uint32_t a = permutation_[x_index] + y_index;
    const std::uint32_t aa = permutation_[a] + z_index;
    const std::uint32_t ab = permutation_[a + 1U] + z_index;
    const std::uint32_t b = permutation_[x_index + 1U] + y_index;
    const std::uint32_t ba = permutation_[b] + z_index;
    const std::uint32_t bb = permutation_[b + 1U] + z_index;

    const float x_minus_one = fp::sub(local_x, 1.0F);
    const float y_minus_one = fp::sub(local_y, 1.0F);
    const float z_minus_one = fp::sub(local_z, 1.0F);

    const float low_y = lerp(
        u,
        grad(permutation_[aa], local_x, local_y, local_z),
        grad(permutation_[ba], x_minus_one, local_y, local_z));
    const float high_y = lerp(
        u,
        grad(permutation_[ab], local_x, y_minus_one, local_z),
        grad(permutation_[bb], x_minus_one, y_minus_one, local_z));
    const float low_z = lerp(v, low_y, high_y);

    const float low_y_next_z = lerp(
        u,
        grad(permutation_[aa + 1U], local_x, local_y, z_minus_one),
        grad(permutation_[ba + 1U], x_minus_one, local_y, z_minus_one));
    const float high_y_next_z = lerp(
        u,
        grad(permutation_[ab + 1U], local_x, y_minus_one, z_minus_one),
        grad(permutation_[bb + 1U], x_minus_one, y_minus_one, z_minus_one));
    const float high_z = lerp(v, low_y_next_z, high_y_next_z);
    return lerp(w, low_z, high_z);
}

void ImprovedNoise::add(
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

    const float inverse_amplitude = fp::div(1.0F, amplitude);
    std::size_t output_index = 0;

    if (y_size == 1) {
        for (std::int32_t ix = 0; ix < x_size; ++ix) {
            const float grid_x = fp::add(x, static_cast<float>(ix));
            const float shifted_x = fp::mul_add(x_offset_, grid_x, x_scale);
            const std::int32_t floor_x = fp::floor_to_i32(shifted_x);
            const std::uint32_t x_index = wrap_256(floor_x);
            const float local_x = fp::sub(shifted_x, static_cast<float>(floor_x));
            const float u = fade(local_x);

            for (std::int32_t iz = 0; iz < z_size; ++iz) {
                const float grid_z = fp::add(z, static_cast<float>(iz));
                const float shifted_z = fp::mul_add(z_offset_, grid_z, z_scale);
                const std::int32_t floor_z = fp::floor_to_i32(shifted_z);
                const std::uint32_t z_index = wrap_256(floor_z);
                const float local_z =
                    fp::sub(shifted_z, static_cast<float>(floor_z));
                const float w = fade(local_z);

                // The y-size-one specialization still walks the omitted Y=0
                // permutation level. In native terms these are
                // p[p[x]] + z and p[p[x + 1]] + z, not p[x] + z.
                const std::uint32_t a =
                    permutation_[permutation_[x_index]] + z_index;
                const std::uint32_t b =
                    permutation_[permutation_[x_index + 1U]] + z_index;
                const float x_minus_one = fp::sub(local_x, 1.0F);
                const float z_minus_one = fp::sub(local_z, 1.0F);

                const float low_z = lerp(
                    u,
                    grad2(permutation_[a], local_x, local_z),
                    grad(permutation_[b], x_minus_one, 0.0F, local_z));
                const float high_z = lerp(
                    u,
                    grad(permutation_[a + 1U], local_x, 0.0F, z_minus_one),
                    grad(
                        permutation_[b + 1U],
                        x_minus_one,
                        0.0F,
                        z_minus_one));
                const float sample = lerp(w, low_z, high_z);
                output[output_index] = fp::mul_add(
                    output[output_index], sample, inverse_amplitude);
                ++output_index;
            }
        }
        return;
    }

    for (std::int32_t ix = 0; ix < x_size; ++ix) {
        const float grid_x = fp::add(x, static_cast<float>(ix));
        const float shifted_x = fp::mul_add(x_offset_, grid_x, x_scale);
        const std::int32_t floor_x = fp::floor_to_i32(shifted_x);
        const std::uint32_t x_index = wrap_256(floor_x);
        const float local_x = fp::sub(shifted_x, static_cast<float>(floor_x));
        const float u = fade(local_x);

        for (std::int32_t iz = 0; iz < z_size; ++iz) {
            const float grid_z = fp::add(z, static_cast<float>(iz));
            const float shifted_z = fp::mul_add(z_offset_, grid_z, z_scale);
            const std::int32_t floor_z = fp::floor_to_i32(shifted_z);
            const std::uint32_t z_index = wrap_256(floor_z);
            const float local_z = fp::sub(shifted_z, static_cast<float>(floor_z));
            const float w = fade(local_z);

            // The APK caches the four X-axis gradient interpolants until the
            // integer Y lattice coordinate changes. Their dot products also
            // depend on local_y, so reusing them is mathematically unusual,
            // but it is observable in every low-frequency 3-D octave.
            std::int32_t previous_y_index = -1;
            float low_y = 0.0F;
            float high_y = 0.0F;
            float low_y_next_z = 0.0F;
            float high_y_next_z = 0.0F;

            for (std::int32_t iy = 0; iy < y_size; ++iy) {
                const float grid_y = fp::add(y, static_cast<float>(iy));
                const float shifted_y = fp::mul_add(y_offset_, grid_y, y_scale);
                const std::int32_t floor_y = fp::floor_to_i32(shifted_y);
                const std::uint32_t y_index = wrap_256(floor_y);
                const float local_y =
                    fp::sub(shifted_y, static_cast<float>(floor_y));
                const float v = fade(local_y);

                if (iy == 0
                    || static_cast<std::int32_t>(y_index)
                        != previous_y_index) {
                    const std::uint32_t a = permutation_[x_index] + y_index;
                    const std::uint32_t aa = permutation_[a] + z_index;
                    const std::uint32_t ab = permutation_[a + 1U] + z_index;
                    const std::uint32_t b =
                        permutation_[x_index + 1U] + y_index;
                    const std::uint32_t ba = permutation_[b] + z_index;
                    const std::uint32_t bb = permutation_[b + 1U] + z_index;
                    const float x_minus_one = fp::sub(local_x, 1.0F);
                    const float y_minus_one = fp::sub(local_y, 1.0F);
                    const float z_minus_one = fp::sub(local_z, 1.0F);

                    low_y = blend_x(
                        gradient_terms(
                            permutation_[aa], local_x, local_y, local_z),
                        gradient_terms(
                            permutation_[ba], x_minus_one, local_y, local_z),
                        u);
                    high_y = blend_x(
                        gradient_terms(
                            permutation_[ab], local_x, y_minus_one, local_z),
                        gradient_terms(
                            permutation_[bb],
                            x_minus_one,
                            y_minus_one,
                            local_z),
                        u);
                    low_y_next_z = blend_x(
                        gradient_terms(
                            permutation_[aa + 1U],
                            local_x,
                            local_y,
                            z_minus_one),
                        gradient_terms(
                            permutation_[ba + 1U],
                            x_minus_one,
                            local_y,
                            z_minus_one),
                        u);
                    high_y_next_z = blend_x(
                        gradient_terms(
                            permutation_[ab + 1U],
                            local_x,
                            y_minus_one,
                            z_minus_one),
                        gradient_terms(
                            permutation_[bb + 1U],
                            x_minus_one,
                            y_minus_one,
                            z_minus_one),
                        u);
                    previous_y_index = static_cast<std::int32_t>(y_index);
                }

                // Keep the APK's interpolation schedule, not merely its
                // trilinear formula.  The ARM routine first resolves the
                // Y blend at Z=0, then forms the Z delta from that rounded
                // value.  Computing an independent Z=1 blend and subtracting
                // it is algebraically equivalent but changes threshold cells
                // in End terrain.
                const float y_blend_at_z0 = lerp(v, low_y, high_y);
                const float z_delta = fp::add(
                    fp::sub(low_y_next_z, y_blend_at_z0),
                    fp::mul(
                        fp::sub(high_y_next_z, low_y_next_z), v));
                const float sample = fp::mul_add(y_blend_at_z0, w, z_delta);
                output[output_index] = fp::mul_add(
                    output[output_index], sample, inverse_amplitude);
                ++output_index;
            }
        }
    }
}

PerlinNoise::PerlinNoise(Mt19937& random, std::int32_t octaves) {
    assert(octaves >= 0);
    octaves_.reserve(static_cast<std::size_t>(octaves));
    for (std::int32_t octave = 0; octave < octaves; ++octave) {
        octaves_.emplace_back(random);
    }
}

float PerlinNoise::value(float x, float z) const noexcept {
    float result = 0.0F;
    float scale = 1.0F;
    for (const ImprovedNoise& octave : octaves_) {
        const float sample = octave.value(fp::mul(x, scale), fp::mul(z, scale));
        result = fp::add(result, fp::div(sample, scale));
        scale = fp::mul(scale, 0.5F);
    }
    return result;
}

float PerlinNoise::value(float x, float y, float z) const noexcept {
    float result = 0.0F;
    float scale = 1.0F;
    for (const ImprovedNoise& octave : octaves_) {
        const float sample = octave.value(
            fp::mul(x, scale),
            fp::mul(y, scale),
            fp::mul(z, scale));
        result = fp::add(result, fp::div(sample, scale));
        scale = fp::mul(scale, 0.5F);
    }
    return result;
}

std::span<float> PerlinNoise::region(
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
    for (const ImprovedNoise& octave : octaves_) {
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

std::span<float> PerlinNoise::region2d(
    std::vector<float>& storage,
    std::int32_t x,
    std::int32_t z,
    std::int32_t x_size,
    std::int32_t z_size,
    float x_scale,
    float z_scale,
    float amplitude) const {
    // The APK's 2-D overload is a thin wrapper around the 3-D path at Y=10.
    // The final float parameter is present in the ABI but is not read.
    (void)amplitude;
    return region(
        storage,
        static_cast<float>(x),
        10.0F,
        static_cast<float>(z),
        x_size,
        1,
        z_size,
        x_scale,
        1.0F,
        z_scale);
}

} // namespace mcpe::worldgen::detail
