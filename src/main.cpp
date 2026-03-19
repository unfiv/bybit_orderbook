#include "bybit/client.hpp"

#include <boost/asio/signal_set.hpp>

#include <iostream>
#include <string_view>
#include <charconv>
#include <print>

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::print(stderr, "Usage: orderbook_test <messages_count>\n");
        return 1;
    }

    try
    {
        bybit::net::io_context ioc;

        uint32_t messages = 0;
        std::string_view arg = argv[1];
        auto [ptr, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), messages);
        if (ec != std::errc()) {
            std::print(stderr, "Error: Invalid number of messages!\n");
            return 1;
        }

        // Catch Ctrl+C (SIGINT) and closing (SIGTERM)
        bybit::net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto)
        {
            ioc.stop();
        });

        bybit::ssl::context ctx{bybit::ssl::context::tlsv12_client};
        ctx.set_verify_mode(bybit::ssl::verify_none);

        auto client = std::make_shared<bybit::Client>(ioc, ctx, messages);
        client->run();
        ioc.run();
    }
    catch (const std::exception& e)
    {
        std::print(stderr, "Exception: {}\n", e.what());
        return 1;
    }

    return 0;
}
