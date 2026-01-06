#ifndef VCONSOLE_SERVER_HPP
#define VCONSOLE_SERVER_HPP

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define SOCKET_ERROR_CODE WSAGetLastError()
#define SHUT_RDWR SD_BOTH
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdio>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#define SOCKET_ERROR_CODE errno
#endif

#pragma pack(push, 1)
struct VConChunk {
    char type[4];
    uint32_t version;
    uint16_t length;
    uint16_t handle;
};
#pragma pack(pop)

struct ClientInfo {
    SOCKET socket;
    std::string ip;
    uint16_t port;

    ClientInfo(SOCKET s, const std::string& i, uint16_t p)
        : socket(s), ip(i), port(p) {}
};

class VConsoleServer {
public:
    static VConsoleServer& getInstance();

    bool initialize(uint16_t port = 29000, const std::string& bindAddr = "0.0.0.0");
    void shutdown();
    void tick();

    void broadcastPrint(const std::string& message, int32_t channelId = 0, uint32_t color = 0xFFFFFFFF);

    uint16_t getPort() const { return m_port; }
    size_t getClientCount() const;
    bool isRunning() const { return m_running; }
    void setMaxConnections(int max) { m_maxConnections = max; }
    void setLogging(bool enabled) { m_logging = enabled; }
    void logLocal(const char* msg);

private:
    VConsoleServer();
    ~VConsoleServer();
    VConsoleServer(const VConsoleServer&) = delete;
    VConsoleServer& operator=(const VConsoleServer&) = delete;

    void acceptClients();
    void processClients();
    void handleClientMessage(ClientInfo& client, const char* data, size_t len);
    void removeClient(SOCKET socket);
    void setNonBlocking(SOCKET socket);

    void sendPacket(SOCKET socket, const char* type, const std::vector<uint8_t>& payload);
    void sendAINF(SOCKET socket);
    void sendADON(SOCKET socket, const std::string& name);
    void sendCHAN(SOCKET socket);

    std::vector<uint8_t> createPRNTPacket(const std::string& message, int32_t channelId, uint32_t color);

    SOCKET m_listenSocket;
    uint16_t m_port;
    std::string m_bindAddr;
    bool m_running;
    int m_maxConnections;
    bool m_logging;

    void stopListening();
    void startListening();

    std::vector<ClientInfo> m_clients;
    std::mutex m_clientsMutex;

#ifndef _WIN32
    int m_stdoutPipe[2];
    int m_stderrPipe[2];
    int m_origStdout;
    int m_origStderr;
    bool m_captureActive;
    std::string m_partialLine;

    void setupOutputCapture();
    void cleanupOutputCapture();
    void readCapturedOutput();
#endif
};

#endif // VCONSOLE_SERVER_HPP
