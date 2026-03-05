#include "server.hpp"
#include <iostream>

Session::Session(tcp::socket socket_, Logger& logger_)
    : _socket(std::move(socket_)), _logger(logger_) {}


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
            std::string data;
            std::istream is(&_buffer);
            std::getline(is, data);

            GameCommand cmd = Protocol::parse(data);

            if (cmd.is_valid) {
                _logger.log("Valid command: " + cmd.type + " from " + cmd.from);
                
                std::string response = Protocol::serialize("ok", "Command received");
                do_write(response);
            } else {
                _logger.err("Invalid JSON received: " + data);
                do_write(Protocol::serialize("error", "Invalid format"));
            }
            do_read(); 
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


void ChessServer::start_accept(){

    auto new_session = std::make_shared<Session>(tcp::socket(static_cast<boost::asio::io_context&>(_acceptor.get_executor().context())), _logger);

    _acceptor.async_accept(new_session->socket(),
[this, new_session](boost::system::error_code ec){
    if (!ec){
        _sessions.push_back(new_session);
        new_session->start(); // посмотреть на переполнение  stack ! 
    }
    else{
        _logger.err("Accept error: " + ec.message());
    }

    start_accept(); 
});
}


