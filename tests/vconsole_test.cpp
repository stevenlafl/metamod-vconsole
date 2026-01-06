#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#pragma pack(push, 1)
struct VConChunk {
    char type[4];
    uint32_t version;
    uint16_t length;
    uint16_t handle;
};
#pragma pack(pop)

struct Channel {
    int32_t id;
    int32_t unknown1;
    int32_t unknown2;
    int32_t verbosity_default;
    int32_t verbosity_current;
    uint32_t text_RGBA_override;
    char name[34];
};

class VConsoleTest {
public:
    VConsoleTest() : m_socket(-1) {}
    ~VConsoleTest() { disconnect(); }

    bool connect(const std::string& ip, int port) {
        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket < 0) {
            std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
            return false;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
            std::cerr << "Invalid address: " << ip << std::endl;
            close(m_socket);
            m_socket = -1;
            return false;
        }

        if (::connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "Connection failed: " << strerror(errno) << std::endl;
            close(m_socket);
            m_socket = -1;
            return false;
        }

        std::cout << "Connected to " << ip << ":" << port << std::endl;
        return true;
    }

    void disconnect() {
        if (m_socket >= 0) {
            close(m_socket);
            m_socket = -1;
        }
    }

    bool sendCommand(const std::string& cmd) {
        std::vector<uint8_t> payload;

        VConChunk header;
        memcpy(header.type, "CMND", 4);
        header.version = htonl(0x000000D4);
        header.length = htons(static_cast<uint16_t>(sizeof(VConChunk) + cmd.length() + 1));
        header.handle = htons(0);

        payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&header),
                       reinterpret_cast<uint8_t*>(&header) + sizeof(VConChunk));
        payload.insert(payload.end(), cmd.begin(), cmd.end());
        payload.push_back(0);

        ssize_t sent = send(m_socket, payload.data(), payload.size(), 0);
        if (sent < 0) {
            std::cerr << "Failed to send command: " << strerror(errno) << std::endl;
            return false;
        }

        std::cout << "Sent command: " << cmd << std::endl;
        return true;
    }

    bool readPacket(std::string& msgType, std::vector<char>& payload, int timeoutMs = 5000) {
        struct pollfd pfd;
        pfd.fd = m_socket;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, timeoutMs);
        if (ret <= 0) {
            if (ret == 0) {
                std::cerr << "Timeout waiting for data" << std::endl;
            } else {
                std::cerr << "Poll error: " << strerror(errno) << std::endl;
            }
            return false;
        }

        VConChunk header;
        ssize_t n = recv(m_socket, &header, sizeof(header), MSG_WAITALL);
        if (n < static_cast<ssize_t>(sizeof(VConChunk))) {
            if (n == 0) {
                std::cerr << "Connection closed by server" << std::endl;
            } else {
                std::cerr << "Failed to read header: " << strerror(errno) << std::endl;
            }
            return false;
        }

        msgType = std::string(header.type, 4);
        uint16_t length = ntohs(header.length);
        uint16_t payloadLen = length - sizeof(VConChunk);

        payload.resize(payloadLen);
        if (payloadLen > 0) {
            n = recv(m_socket, payload.data(), payloadLen, MSG_WAITALL);
            if (n < payloadLen) {
                std::cerr << "Failed to read payload: " << strerror(errno) << std::endl;
                return false;
            }
        }

        return true;
    }

    void parseAINF(const std::vector<char>& payload) {
        std::cout << "  AINF payload size: " << payload.size() << " bytes" << std::endl;
    }

    void parseADON(const std::vector<char>& payload) {
        if (payload.size() < 4) return;
        uint16_t nameLen = ntohs(*reinterpret_cast<const uint16_t*>(payload.data() + 2));
        std::string name(payload.data() + 4, std::min((size_t)nameLen, payload.size() - 4));
        std::cout << "  Server name: " << name << std::endl;
    }

    void parseCHAN(const std::vector<char>& payload) {
        if (payload.size() < 2) return;
        uint16_t numChannels = ntohs(*reinterpret_cast<const uint16_t*>(payload.data()));
        std::cout << "  Number of channels: " << numChannels << std::endl;

        const char* data = payload.data() + 2;
        for (int i = 0; i < numChannels && (data - payload.data()) + sizeof(Channel) <= payload.size(); i++) {
            Channel ch;
            memcpy(&ch, data, sizeof(Channel));
            ch.id = ntohl(ch.id);
            ch.verbosity_default = ntohl(ch.verbosity_default);
            ch.verbosity_current = ntohl(ch.verbosity_current);
            ch.text_RGBA_override = ntohl(ch.text_RGBA_override);
            std::cout << "    Channel " << ch.id << ": " << ch.name << std::endl;
            data += sizeof(Channel);
        }
    }

    void parsePRNT(const std::vector<char>& payload) {
        if (payload.size() < 29) return;
        int32_t channelId = ntohl(*reinterpret_cast<const int32_t*>(payload.data()));
        uint32_t color = ntohl(*reinterpret_cast<const uint32_t*>(payload.data() + 12));
        std::string message(payload.data() + 28);

        std::cout << "  [CH" << channelId << "] ";

        uint8_t r = (color >> 24) & 0xFF;
        uint8_t g = (color >> 16) & 0xFF;
        uint8_t b = (color >> 8) & 0xFF;
        if (r == 0xFF && g == 0 && b == 0) {
            std::cout << "\033[31m" << message << "\033[0m";
        } else {
            std::cout << message;
        }

        if (message.empty() || message.back() != '\n') {
            std::cout << std::endl;
        }
    }

    int getSocket() const { return m_socket; }

private:
    int m_socket;
};

void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --host <ip>     Server IP (default: 127.0.0.1)" << std::endl;
    std::cout << "  -p, --port <port>   Server port (default: 29000)" << std::endl;
    std::cout << "  -c, --cmd <command> Command to send (can be repeated)" << std::endl;
    std::cout << "  -t, --timeout <ms>  Read timeout in ms (default: 5000)" << std::endl;
    std::cout << "  -l, --listen        Keep listening for messages" << std::endl;
    std::cout << "  --help              Show this help" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 29000;
    std::vector<std::string> commands;
    int timeout = 5000;
    bool keepListening = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "-h" || arg == "--host") && i + 1 < argc) {
            host = argv[++i];
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if ((arg == "-c" || arg == "--cmd") && i + 1 < argc) {
            commands.push_back(argv[++i]);
        } else if ((arg == "-t" || arg == "--timeout") && i + 1 < argc) {
            timeout = std::stoi(argv[++i]);
        } else if (arg == "-l" || arg == "--listen") {
            keepListening = true;
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
    }

    VConsoleTest client;

    std::cout << "=== VConsole Test Client ===" << std::endl;
    std::cout << "Connecting to " << host << ":" << port << "..." << std::endl;

    if (!client.connect(host, port)) {
        return 1;
    }

    std::cout << std::endl << "=== Receiving Handshake ===" << std::endl;

    std::string msgType;
    std::vector<char> payload;
    int handshakePackets = 0;

    while (handshakePackets < 3) {
        if (!client.readPacket(msgType, payload, timeout)) {
            std::cerr << "Failed to receive handshake packet " << (handshakePackets + 1) << std::endl;
            return 1;
        }

        std::cout << "Received: " << msgType << " (" << payload.size() << " bytes)" << std::endl;

        if (msgType == "AINF") {
            client.parseAINF(payload);
        } else if (msgType == "ADON") {
            client.parseADON(payload);
        } else if (msgType == "CHAN") {
            client.parseCHAN(payload);
        }

        handshakePackets++;
    }

    std::cout << std::endl << "=== Handshake Complete ===" << std::endl;

    if (commands.empty() && !keepListening) {
        commands.push_back("status");
    }

    for (const auto& cmd : commands) {
        std::cout << std::endl << "=== Sending Command: " << cmd << " ===" << std::endl;
        if (!client.sendCommand(cmd)) {
            return 1;
        }

        std::cout << std::endl << "=== Waiting for Response ===" << std::endl;

        while (client.readPacket(msgType, payload, timeout)) {
            std::cout << "Received: " << msgType << std::endl;
            if (msgType == "PRNT") {
                client.parsePRNT(payload);
            }

            if (!keepListening) {
                break;
            }
        }
    }

    if (keepListening) {
        std::cout << std::endl << "=== Listening for Messages (Ctrl+C to exit) ===" << std::endl;
        while (client.readPacket(msgType, payload, -1)) {
            if (msgType == "PRNT") {
                client.parsePRNT(payload);
            } else {
                std::cout << "Received: " << msgType << " (" << payload.size() << " bytes)" << std::endl;
            }
        }
    }

    std::cout << std::endl << "=== Test Complete ===" << std::endl;
    return 0;
}
