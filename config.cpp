#include "config.hpp"
#include <fstream>

ServerSettings ConfigReader::load(const std::string& filename){

    std::ifstream file(filename);

    nlohmann::json jn;

    file >> jn;

    ServerSettings settings{};
    settings.host = jn.at("host").get<std::string>();
    settings.port = jn.at("port").get<int>();
    settings.ws_port = jn.value("ws_port", 18081);
    settings.wss_port = jn.value("wss_port", 18082);
    settings.ssl_enabled = jn.value("ssl_enabled", false);
    settings.ssl_cert_file = jn.value("ssl_cert_file", "certs/server.crt");
    settings.ssl_key_file = jn.value("ssl_key_file", "certs/server.key");
    return settings;
}