//десериализатор 

#pragma once 
#include <string>
#include "json.hpp"

struct ServerSettings{

    std::string host;
    int port;
};

class ConfigReader {

    public:
        static ServerSettings load(const std::string& filename);
};