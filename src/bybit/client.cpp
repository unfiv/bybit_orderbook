#include "client.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <format>
#include <print>

using json = nlohmann::json;


namespace bybit {

Client::Client(net::io_context& ioc, ssl::context& ctx, uint32_t messages):
    strand_(net::make_strand(ioc)),
    resolver_(strand_),
    ws_(strand_, ctx),
    heartbeat_timer(strand_),
    num_messages_(messages),
    builder_(book_)
{
    topic_ = std::format("orderbook.{}.BTCUSDT", bybit::config::depth);
    subscribe_msg_ = std::format(R"({{"op": "subscribe", "args": ["{}"]}})", topic_);
}

void Client::run()
{
    if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), bybit::config::host.data())) {
        beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
        std::print(stderr, "{}\n", ec.message());
        return;
    }

    ws_.next_layer().set_verify_callback(ssl::host_name_verification(bybit::config::host.data()));

    resolver_.async_resolve(bybit::config::host, bybit::config::port, beast::bind_front_handler(
                &Client::on_resolve,
                shared_from_this()));
}

void Client::on_error(beast::error_code ec, char const* what)
{
    if (ec != net::error::operation_aborted) {
        std::print(stderr, "{}: {}\n", what, ec.message());
    }
}

void Client::on_resolve(beast::error_code ec, ip::tcp::resolver::results_type results)
{
    if (ec) {
        return on_error(ec, "resolve");
    }

    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

    beast::get_lowest_layer(ws_).async_connect(results, beast::bind_front_handler(
                &Client::on_connect,
                shared_from_this()));
}

void Client::on_connect(beast::error_code ec, ip::tcp::resolver::results_type::endpoint_type ep)
{
    if (ec) {
        return on_error(ec, "connect");
    }

    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
 
    ws_.next_layer().async_handshake(ssl::stream_base::client, beast::bind_front_handler(
                &Client::on_ssl_handshake,
                shared_from_this()));
}

void Client::on_ssl_handshake(beast::error_code ec)
{
    if (ec) {
        return on_error(ec, "ssl_handshake");
    }

    beast::get_lowest_layer(ws_).expires_never();

    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

    ws_.async_handshake(bybit::config::host, bybit::config::endpoint, beast::bind_front_handler(
                &Client::on_handshake,
                shared_from_this()));
}

void Client::on_handshake(beast::error_code ec)
{
    if (ec) {
        return on_error(ec, "handshake");
    }

    ws_.async_write(net::buffer(subscribe_msg_), beast::bind_front_handler(
                &Client::on_write_subscribe,
                shared_from_this()));
}

void Client::on_write_subscribe(beast::error_code ec, std::size_t bytes_transferred)
{
    if (ec) {
        return on_error(ec, "write_subscribe");
    }

    wait_and_send_heartbeat();

    ws_.async_read(buffer_, beast::bind_front_handler(
                &Client::on_read,
                shared_from_this()));
}

void Client::wait_and_send_heartbeat()
{
    heartbeat_timer.expires_after(boost::asio::chrono::seconds(20));
    heartbeat_timer.async_wait(beast::bind_front_handler(&Client::on_heartbeat_timer, shared_from_this()));
}

void Client::on_heartbeat_timer(beast::error_code ec)
{
    if (ec) return on_error(ec, "on_heartbeat_timer");
    ws_.async_write(net::buffer(bybit::config::heartbeat_msg), beast::bind_front_handler(&Client::on_heartbeat_answer, shared_from_this()));
}

void Client::on_heartbeat_answer(beast::error_code ec, std::size_t bytes_transferred)
{
    if (ec) return on_error(ec, "on_heartbeat_answer");
    wait_and_send_heartbeat();
}

void Client::on_read(beast::error_code ec, std::size_t bytes_transferred)
{
    if (ec) {
        return on_error(ec, "read");
    }

    auto data = buffer_.data();
    std::string_view sv{static_cast<char const*>(data.data()), bytes_transferred};

    handleMessage(sv);

    buffer_.consume(bytes_transferred);

    if (--num_messages_ > 0) {
        ws_.async_read(buffer_, beast::bind_front_handler(
                    &Client::on_read,
                    shared_from_this()));
    } else {
        stop();
    }
}

void Client::on_close(beast::error_code ec)
{
    if (ec) {
        return on_error(ec, "close");
    }
}

void Client::handleMessage(std::string_view message)
{
    try {
        auto js = json::parse(message);

        // check protocol answers
        if (js.contains("op") && js["op"] == "subscribe") {
            if (!(js.contains("success") && js["success"].get<bool>())) {
                std::print(stderr, "Subscription FAILED: {}\n", js["ret_msg"].get<std::string>());
                stop();
            }
            return; 
        }

        if (!js.contains("topic"))
            return;
        if (js["topic"] != topic_)
            return;

        const auto& type_ref = js.at("type");
        if (type_ref == "snapshot") {
            builder_.clear();
        }

        auto it_data = js.find("data");
        if (it_data != js.end() && it_data->is_object()) {
            const auto& data = *it_data;
            
            auto process_levels = [&](auto it, Side side) {
                if (it != data.end() && it->is_array()) {
                    for (const auto& kv : *it) {
                        // take refs to strings directly inside JSON
                        const std::string& p_s = kv[0].get_ref<const std::string&>();
                        const std::string& a_s = kv[1].get_ref<const std::string&>();

                        double px, amt;

                        // fastest parsing: no locales, no allocations
                        // TODO: consider the result
                        std::from_chars(p_s.data(), p_s.data() + p_s.size(), px);
                        std::from_chars(a_s.data(), a_s.data() + a_s.size(), amt);

                        builder_.applyDelta(side, px, amt, amt == 0);
                    }
                }
            };

            process_levels(data.find("a"), Side::Sell);
            process_levels(data.find("b"), Side::Buy);

            const char* GREEN = "\033[32m";
            const char* RED   = "\033[31m";
            const char* RESET = "\033[0m";

            std::print("{}{:>10.2f} ({:<10.4f}){}  |  {}{:>10.2f} ({:<10.4f}){}\n", 
                GREEN,
                book_.level(Side::Buy, 0).px,
                book_.level(Side::Buy, 0).amt,
                RESET,
                RED,
                book_.level(Side::Sell, 0).px, book_.level(Side::Sell, 0).amt,
                RESET);
        }
    } catch (const std::exception& e) {
        std::print(stderr, "JSON Error: {}\n", e.what());
        stop();
    }
}

void Client::stop()
{
    heartbeat_timer.cancel();
    ws_.async_close(websocket::close_code::normal, beast::bind_front_handler(&Client::on_close, shared_from_this()));

    // TODO: consider stopping later
    net::get_associated_executor(strand_).context().stop();
}

} // namespace bybit
