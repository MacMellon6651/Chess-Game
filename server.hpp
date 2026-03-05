// ядро сервера (tcp)

#pragma once 

#include <boost\asio.hpp>
#include <memory>
#include <vector>
#include "logger.hpp"
#include "config.hpp"
#include "command.hpp"
#include "gamemanager.hpp"

using boost::asio::ip::tcp;


class Session: public std::enable_shared_from_this<Session>{

    public:
        explicit Session(tcp::socket socket, Logger& logger, GameManager& gm);

        void do_write(const std::string& message);

        tcp::socket& socket();


        void start();

    private:

        int _player_id = 0;

        int _id;

        void do_read();

        void handle_command(const GameCommand& cmd);

        tcp::socket _socket;
        Logger& _logger;
        GameManager& _game_manager;
        boost::asio::streambuf _buffer;
        

};

class ChessServer{


    public:
        ChessServer(boost::asio::io_context& io, const ServerSettings& config, Logger& logger);

    private:
        void start_accept();

        void handle_accept(std::shared_ptr<Session> new_session, const boost::system::error_code& err); // shared_ptr разобраться с рвботой (шприная память )

        tcp::acceptor _acceptor;
        Logger& _logger;

        GameManager _game_manager;

        std::vector<std::shared_ptr<Session>> _sessions;
};