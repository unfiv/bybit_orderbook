#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>

#include <string>

#include "bookbuilder.hpp"
#include "constants.hpp"

namespace bybit
{
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace websocket = beast::websocket;
    namespace net = boost::asio;
    namespace ssl = boost::asio::ssl;
    namespace ip = boost::asio::ip;

    class Client: public std::enable_shared_from_this<Client>
    {
    public:
        Client(net::io_context& ioc, ssl::context& ctx, uint32_t messages);
    
        void run();
    
    private:
        void on_resolve(beast::error_code ec, ip::tcp::resolver::results_type results);
        void on_connect(beast::error_code ec, ip::tcp::resolver::results_type::endpoint_type ep);
        void on_ssl_handshake(beast::error_code ec);
        void on_handshake(beast::error_code ec);
        void on_write_subscribe(beast::error_code ec, std::size_t bytes_transferred);
        
        void on_read(beast::error_code ec, std::size_t bytes_transferred);
        void on_close(beast::error_code ec);
        void on_error(beast::error_code ec, char const* what);

        void wait_and_send_heartbeat();
        void on_heartbeat_timer(beast::error_code ec);
        void on_heartbeat_answer(beast::error_code ec, std::size_t bytes_transferred);
        
        void stop();
    
        void handleMessage(std::string_view message);
    
    private:
        net::any_io_executor strand_;
        ip::tcp::resolver resolver_;
        websocket::stream<ssl::stream<beast::tcp_stream>> ws_;
        boost::asio::steady_timer heartbeat_timer;
        beast::flat_buffer buffer_;
        std::string topic_;

        std::string subscribe_msg_;        

        uint32_t num_messages_;
    
        Book<bybit::config::depth> book_;
        Bookbuilder<bybit::config::depth> builder_;
    };
} // namespace bybit
