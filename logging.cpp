#include "logging.hpp"

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

void init_logging() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    namespace expr = boost::log::expressions;

    boost::log::add_common_attributes();
    boost::log::add_console_log(
        std::clog,
        boost::log::keywords::format =
            (expr::stream
             << "[" << boost::log::trivial::severity << "] "
             << expr::smessage));
}

