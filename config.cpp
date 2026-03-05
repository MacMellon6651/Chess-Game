#include "config.hpp"
#include <fstream>

ServerSettings ConfigReader::load(const std::string& filename){

    std::ifstream file(filename);

    nlohmann::json jn;

    file >> jn;

    return { jn.at("host").get<std::string>() , jn.at("port").get<int>() };
}