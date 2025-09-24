#include "notify_parser.hpp"
#include <iostream>
#include <boost/asio.hpp>
#include <string>

using boost::asio::ip::tcp;

int main() {
    const std::string pool_host = "solo-btg.2miners.com";
    const std::string pool_port = "4040";
    const std::string wallet = "Gb4V4a9Jk3p8aH6jkW3Aq3sq8rQCuJQ6S8";
    const std::string worker = "XBTGPUARC";
    const std::string password = "x";
    const std::string full_user = wallet + "." + worker;

    try {
        boost::asio::io_context io_context;

        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(pool_host, pool_port);
        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        std::cout << "📡 Verbunden mit " << pool_host << ":" << pool_port << "\n";

        // mining.subscribe senden
        std::string subscribe = "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": []}\n";
        std::string authorize = "{\"id\": 2, \"method\": \"mining.authorize\", \"params\": [\"" +
        full_user + "\", \"" + password + "\"]}\n";

// Empfangsschleife
std::string buffer;
for (;;) {
    char reply[4096];
    boost::system::error_code error;
    size_t len = socket.read_some(boost::asio::buffer(reply), error);
    buffer.append(reply, len);

    // Zeilenweise verarbeiten
    size_t pos = 0;
    while ((pos = buffer.find('\n')) != std::string::npos) {
        std::string line = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);

        auto job = parse_notify(line);
        if (job) {
            std::cout << "🎯 Job ID: " << job->job_id << std::endl;
            std::cout << "🧱 PrevHash: " << job->prevhash << std::endl;
        }

        std::cout << "🌐 Nachricht:\n" << line << "\n";
    }

    if (error == boost::asio::error::eof) break;
    else if (error) throw boost::system::system_error(error);
}

    return 0;
}
