#include "server.hpp"
#include "logging.hpp"

int main(){

    init_logging();

    try{
        boost::asio::io_context io;
        
        ServerSettings settings = ConfigReader::load("settings.json");

        ChessServer server(io, settings);

        io.run();
    }
    catch (const std::exception& _err){
        BOOST_LOG_TRIVIAL(error) << _err.what();
    }
    return 0;
}