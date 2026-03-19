#pragma once

#include <string_view>

namespace bybit::config {
    inline constexpr std::string_view host     = "stream.bybit.com";
    inline constexpr std::string_view port     = "443";
    inline constexpr std::string_view endpoint = "/v5/public/linear";
    inline constexpr std::string_view heartbeat_msg = R"({"op":"ping"})";
    inline constexpr int depth                 = 200;

    static_assert(depth == 50 || depth == 200, "Unsupported Bybit depth");
}