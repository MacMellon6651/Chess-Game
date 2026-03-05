#include "server.hpp"

int main(){

    Logger logger;

    try{
        boost::asio::io_context io;
        
        ServerSettings settings = ConfigReader::load("settings.json");

        ChessServer server(io, settings, logger);

        io.run();
    }
    catch (const std::exception& _err){
        logger.err(_err.what());
    }
    return 0;
}