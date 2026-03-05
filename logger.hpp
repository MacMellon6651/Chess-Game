// логер
#pragma once
#include <string>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>

class Logger{

    public:
        Logger();


        void log(const std::string& message);
        void err(const std::string& message);

        void debug(const std::string& message);
};