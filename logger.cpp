#include "logger.hpp"
#include <iostream>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/support/date_time.hpp>

namespace logging = boost::log;
namespace keywords = boost::log::keywords;


Logger::Logger(){

    logging::add_console_log(
        std::cout,
        keywords::format = (
            logging::expressions::stream
            << "[" << logging::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S") << "]"
            << "[" << logging::trivial::severity << "]: "
            << logging::expressions::smessage
        )
    );

    logging::add_common_attributes();

}

void Logger::log(const std::string& message){
    BOOST_LOG_TRIVIAL(info) << message;
}

void Logger::err(const std::string& message){
    BOOST_LOG_TRIVIAL(error) << message;
}
void Logger::debug(const std::string& message){
    BOOST_LOG_TRIVIAL(debug) << message;
}