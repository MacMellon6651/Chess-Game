// main.cpp
#include "server.hpp"
#include "logging.hpp"
#include "websocket_server.hpp"

int main() {
    init_logging();

    try {
        boost::asio::io_context io;
        
        ServerSettings settings = ConfigReader::load("settings.json");

        // Запускаем TCP сервер для консольных клиентов
        ChessServer server(io, settings);
        
        // Запускаем WebSocket сервер для веб-клиентов
        auto ws_server = std::make_shared<WebSocketServer>(io, settings, server);
        ws_server->start();

        BOOST_LOG_TRIVIAL(info) << "TCP Server on " << settings.host << ":" << settings.port;
        BOOST_LOG_TRIVIAL(info) << "WebSocket Server on " << settings.host << ":" << settings.port + 1;

        io.run();
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << e.what();
    }
    return 0;
}