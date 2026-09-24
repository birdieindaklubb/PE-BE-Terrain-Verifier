#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace mcpe::worldgen {

enum class Version : std::uint32_t {
    pe_0_6_1 = 0x0006'0001,
    pe_0_9_0 = 0x0009'0000,
    pe_1_1_5_0 = 0x0101'0500,
    be_1_6_0_15 = 0x0106'000f,

    // Source-compatibility alias for releases which incorrectly labelled
    // this Bedrock Dedicated Server target as Pocket Edition.
    pe_1_6_0_15 = be_1_6_0_15,
};

[[nodiscard]] constexpr std::string_view to_string(Version version) noexcept {
    switch (version) {
    case Version::pe_0_6_1:
        return "0.6.1";
    case Version::pe_0_9_0:
        return "0.9.0";
    case Version::pe_1_1_5_0:
        return "1.1.5.0";
    case Version::be_1_6_0_15:
        return "1.6.0.15";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::optional<Version>
parse_version(std::string_view text) noexcept {
    if (text == "0.6.1" || text == "pe-0.6.1" || text == "pe_0_6_1") {
        return Version::pe_0_6_1;
    }
    if (text == "0.9.0" || text == "pe-0.9.0" || text == "pe_0_9_0") {
        return Version::pe_0_9_0;
    }
    if (text == "1.1.5.0" || text == "pe-1.1.5.0"
        || text == "pe_1_1_5_0") {
        return Version::pe_1_1_5_0;
    }
    if (text == "1.6.0.15" || text == "be-1.6.0.15"
        || text == "be_1_6_0_15" || text == "pe-1.6.0.15"
        || text == "pe_1_6_0_15") {
        return Version::be_1_6_0_15;
    }
    return std::nullopt;
}

} // namespace mcpe::worldgen
