#include "logger.hpp"
#include <iostream>

void Logger::log(const std::string& message){
    std::cout << "[INFO]: " << message << std::endl;
}

void Logger::err(const std::string& message){
    std::cout << "[ERROR]: " << message << std::endl;
}
