#include "UdpServer.h"

#include <csignal>
#include <iostream>

// Global server instance for signal handling
UdpServer* g_server = nullptr;

// Signal handler for graceful shutdown
void signalHandler(int signum) {
    std::cout << "\nInterrupt signal received (" << signum << "). Shutting down..." << std::endl;

    if (g_server) {
        g_server->stop();
    }

    exit(signum);
}

int main(int argc, char* argv[]) {
    int port = 8080;  // Default port

    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    std::cout << "Starting UDP server on port " << port << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;

    // Register signal handler for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Create and start UDP server
    UdpServer server;
    g_server = &server;

    int result = server.start(port);

    g_server = nullptr;
    return result;
}