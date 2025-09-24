#include <boost/asio.hpp>
#include <CL/cl.h>
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>
#include <mutex>

#include "notify_parser.hpp"
#include "mining_job.hpp"
#include "miner_loop.hpp"

void stop_mining();

void connect_to_pool(const std::string& host, int port,
                     const std::string& wallet,
                     const std::string& worker,
                     const std::string& password,
                     int platform_index,
                     int device_index,
                     int intensity) {
    using boost::asio::ip::tcp;

    try {
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        std::cout << "📡 Verbunden mit " << host << ":" << port << "\n";

        std::string subscribe = R"({"id":1,"method":"mining.subscribe","params":[]})" "\n";
        std::string full_user = wallet + "." + worker;
        std::string authorize = R"({"id":2,"method":"mining.authorize","params":[")" + full_user + R"(",")" + password + R"("]})" "\n";

        boost::asio::write(socket, boost::asio::buffer(subscribe));
        boost::asio::write(socket, boost::asio::buffer(authorize));

        std::string buffer;

        static std::thread mining_thread;
        static std::mutex miner_mutex;

        for (;;) {
            char reply[4096];
            boost::system::error_code error;
            size_t len = socket.read_some(boost::asio::buffer(reply), error);
            buffer.append(reply, len);

            size_t pos = 0;
            while ((pos = buffer.find('\n')) != std::string::npos) {
                std::string line = buffer.substr(0, pos);
                buffer.erase(0, pos + 1);

                std::cout << "🌐 Nachricht:\n" << line << "\n";

                auto job = parse_notify(line);
                if (job) {
                    std::cout << "🎯 Neuer Job: " << job->job_id << "\n";

                    stop_mining();
                    if (mining_thread.joinable()) {
                        mining_thread.join();
                    }

                    // Miner starten mit intensity
                    mining_thread = std::thread([job, &socket, &wallet, &worker, intensity]() {
                        miner_loop(*job, [&](uint32_t nonce, const std::array<uint8_t, 32>& hash, const MiningJob& job) {
                            std::ostringstream hexstream;
                            for (auto byte : hash)
                                hexstream << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
                            std::string solution = hexstream.str();

                            std::ostringstream nonce_hex;
                            nonce_hex << std::hex << std::setw(8) << std::setfill('0') << nonce;

                            static int extranonce_counter = 1;
                            std::ostringstream extranonce2_stream;
                            extranonce2_stream << std::hex << std::setw(8) << std::setfill('0') << extranonce_counter++;

                            std::string full_user = wallet + "." + worker;

                            std::ostringstream submit;
                            submit << R"({"id":4,"method":"mining.submit","params":[")"
                            << full_user << R"(",")"
                            << job.job_id << R"(",")"
                            << extranonce2_stream.str() << R"(",")"
                            << job.ntime << R"(",")"
                            << nonce_hex.str() << R"(",")"
                            << solution << R"("]})" "\n";

                            boost::asio::write(socket, boost::asio::buffer(submit.str()));
                            std::cout << "📤 Share gesendet:\n" << submit.str();
                        }, intensity);
                    });
                }
            }

            if (error == boost::asio::error::eof) break;
            else if (error) throw boost::system::system_error(error);
        }

        stop_mining();
        if (mining_thread.joinable()) {
            mining_thread.join();
        }

    } catch (const std::exception& e) {
        std::cerr << "❌ Fehler: " << e.what() << "\n";
    }
                     }

                     int main(int argc, char** argv) {
                         int platform_index = 0;
                         int device_index = 0;
                         int intensity = 4;
                         std::string algo = "zhash_144_5";
                         std::string wallet = "default_wallet";
                         std::string worker = "default_worker";
                         std::string password = "x";
                         std::string pool_host = "solo-btg.2miners.com";
                         int pool_port = 4040;

                         for (int i = 1; i < argc; ++i) {
                             std::string arg = argv[i];
                             if (arg == "--platform" && i + 1 < argc) platform_index = std::atoi(argv[++i]);
                             else if (arg == "--device" && i + 1 < argc) device_index = std::atoi(argv[++i]);
                             else if (arg == "--intensity" && i + 1 < argc) intensity = std::atoi(argv[++i]);
                             else if (arg == "--algo" && i + 1 < argc) algo = argv[++i];
                             else if (arg == "--wallet" && i + 1 < argc) wallet = argv[++i];
                             else if (arg == "--worker" && i + 1 < argc) worker = argv[++i];
                             else if (arg == "--password" && i + 1 < argc) password = argv[++i];
                             else if (arg == "--pool" && i + 1 < argc) pool_host = argv[++i];
                             else if (arg == "--port" && i + 1 < argc) pool_port = std::atoi(argv[++i]);
                         }

                         std::cout << "🚀 Starte XBTGPUARC mit Algo: " << algo << "\n";
                         std::cout << "🎛️  Platform: " << platform_index << " | Device: " << device_index << " | Intensity: " << intensity << "\n";
                         std::cout << "👤 Worker: " << wallet << "." << worker << "\n";

                         connect_to_pool(pool_host, pool_port, wallet, worker, password, platform_index, device_index, intensity);
                         return 0;
                     }

