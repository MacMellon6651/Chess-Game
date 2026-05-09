//десериализатор 

#pragma once 
#include <string>
#include "json.hpp"

struct ServerSettings{

    std::string host;
    int port;
    int ws_port;
    int wss_port;
    bool ssl_enabled;
    std::string ssl_cert_file;
    std::string ssl_key_file;
};

class ConfigReader {

    public:
        static ServerSettings load(const std::string& filename);
};