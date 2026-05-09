#include "server.hpp"
#include "logging.hpp"
#include "config.hpp"

int main() {
    init_logging();
    try {
        boost::asio::io_context io;
        ServerSettings settings = ConfigReader::load("settings.json");

        ChessServer server(io, settings);
        server.start_accept_tcp();
        server.start_websocket_server();

        BOOST_LOG_TRIVIAL(info) << "TCP Server on " << settings.host << ":" << settings.port;
        BOOST_LOG_TRIVIAL(info) << "WebSocket Server on " << settings.host << ":" << settings.ws_port;
        if (settings.ssl_enabled) {
            BOOST_LOG_TRIVIAL(info) << "Secure WebSocket Server on " << settings.host << ":" << settings.wss_port;
        }

        io.run();
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << e.what();
    }
    return 0;
}