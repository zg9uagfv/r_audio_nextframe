#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <string>

class UdpServer {
  public:
    UdpServer();
    ~UdpServer();

    int start(int port);
    void stop();

  private:
    bool running_;
    int socket_fd_;

    std::string generateOutputFileName() const;
};

#endif  // UDP_SERVER_H