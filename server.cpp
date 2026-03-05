#include "server.hpp"
#include <iostream>
#include <string>

Session::Session(tcp::socket socket_, Logger& logger_, GameManager& gm)
    : _socket(std::move(socket_)), _logger(logger_), _game_manager(gm) {}


ChessServer::ChessServer(boost::asio::io_context& io, const ServerSettings& config, Logger& logger)
    : _acceptor(io, tcp::endpoint(boost::asio::ip::make_address(config.host), config.port)), _logger(logger)
    {
        _logger.log("Server initialized on " + config.host + ":" + std::to_string(config.port));
        start_accept();
    }

tcp::socket& Session::socket(){
    return _socket;
}

void Session::start(){
    _logger.log("New session started! ");
    do_read();
}



// беск реккурсия (база реккурсия) (или цикл с break)

void Session::do_read() {
    auto self(shared_from_this());
    boost::asio::async_read_until(_socket, _buffer, '\n',
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                
                std::string data(
                    boost::asio::buffers_begin(_buffer.data()),
                    boost::asio::buffers_begin(_buffer.data()) + length
                );
                _buffer.consume(length); 

                
                _logger.debug("Received raw data: " + data);

                try {
                    
                    auto command = Protocol::parse(data);
                    
                    
                    _logger.log("Command processed: type=" + command.type + " from=" + std::to_string(command.player_id));

                    
                    handle_command(command);

                } catch (const std::exception& e) {
                    _logger.err("JSON Parse Error: " + std::string(e.what()));
                    do_write(Protocol::serialize("error", "Invalid JSON format"));
                }

                
                do_read();
            } else {
                if (ec == boost::asio::error::eof) {
                    _logger.log("Client disconnected gracefully.");
                } else {
                    _logger.err("Read error: " + ec.message());
                }
            }
        });
}


void Session::do_write(const std::string& message){

    auto self(shared_from_this());
    boost::asio::async_write(_socket, boost::asio::buffer(message),
[this,self](boost::system::error_code ec, std::size_t){
    if (ec){
        _logger.err("Session write error: " + ec.message());
    }
});
}


void Session::handle_command(const GameCommand& cmd) {
    if (cmd.type == "auth") {
        _player_id = cmd.player_id;
        _game_manager.reg_player(_player_id, shared_from_this());
        _logger.log("Player " + std::to_string(_player_id) + " registered.");
        do_write("{\"status\":\"ok\"}\n");
    } 
    else if (cmd.type == "move") {
        _logger.log("Move from " + std::to_string(_player_id));
        
        
        std::string msg = "{\"type\":\"opponent_move\", \"from\":\"" + cmd.from + "\", \"to\":\"" + cmd.to + "\"}\n";
        _game_manager.broadcast(msg, _player_id); 
    }
}
void ChessServer::start_accept() {
    auto new_session = std::make_shared<Session>(
        tcp::socket(static_cast<boost::asio::io_context&>(_acceptor.get_executor().context())), 
        _logger, 
        _game_manager
    );

    _acceptor.async_accept(new_session->socket(),
        [this, new_session](boost::system::error_code ec) {
            if (!ec) {
                _sessions.push_back(new_session);
                new_session->start();
            } else {
                _logger.err("Accept error: " + ec.message());
            }
            start_accept(); 
        });
}


