// логер
#pragma once
#include <string>

class Logger{

    public:
        void log(const std::string& message);
        void err(const std::string& message);
};