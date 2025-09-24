#include <cstring>      // für memcpy
#include <iostream>
#include <string>
#include <thread>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <atomic>

// Dieses Flag sagt dem Miner: "Ein neuer Job ist da, bitte übernehmen!"
std::atomic<bool> job_wurde_übernommen = true;  // Default = true, damit beim ersten Start kein Fehler ausgelöst wird

bool connect_to_pool(const std::string& host, int port, const std::string& wallet, const std::string& worker) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "❌ Socket-Fehler\n";
        return false;
    }

    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        std::cerr << "❌ Host nicht gefunden: " << host << "\n";
        return false;
    }

    sockaddr_in serv_addr {};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    std::cout << "📞 Verbinde zu " << host << ":" << port << "...\n";
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "❌ Verbindung fehlgeschlagen\n";
        return false;
    }

    // Subscribe senden
    std::string subscribe = "{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[]}\n";
    send(sock, subscribe.c_str(), subscribe.size(), 0);

    // Authorize senden
    std::ostringstream auth;
    auth << "{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\""
    << wallet << "." << worker << "\",\"\"]}\n";
    std::string auth_str = auth.str();
    send(sock, auth_str.c_str(), auth_str.size(), 0);

    // Mitlesen
    char buffer[4096];
    while (true) {
        int n = read(sock, buffer, sizeof(buffer) - 1);
        if (n <= 0) break;
        buffer[n] = '\0';
        std::cout << "📨 Pool: " << buffer << std::endl;

        // 🔍 Primitive Erkennung neuer Jobs
        if (std::string(buffer).find("\"method\":\"mining.notify\"") != std::string::npos) {
            std::cout << "📡 Neuer Mining-Job erkannt – setze Job-Flag zurück\n";
            job_wurde_übernommen.store(false);
        }
    }

    close(sock);
    return true;
}
