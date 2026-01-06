#include "vconsole_server.hpp"
#include <cstring>
#include <algorithm>
#include <extdll.h>
#include <meta_api.h>

#ifndef _WIN32
#include <netinet/tcp.h>
#endif

extern enginefuncs_t g_engfuncs;

VConsoleServer& VConsoleServer::getInstance() {
    static VConsoleServer instance;
    return instance;
}

VConsoleServer::VConsoleServer()
    : m_listenSocket(INVALID_SOCKET)
    , m_port(0)
    , m_running(false)
    , m_maxConnections(1)
    , m_logging(true)
#ifndef _WIN32
    , m_stdoutPipe{-1, -1}
    , m_stderrPipe{-1, -1}
    , m_origStdout(-1)
    , m_origStderr(-1)
    , m_captureActive(false)
#endif
{
}

VConsoleServer::~VConsoleServer() {
    shutdown();
}

void VConsoleServer::setNonBlocking(SOCKET socket) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(socket, FIONBIO, &mode);
#else
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags != -1) {
        fcntl(socket, F_SETFL, flags | O_NONBLOCK);
    }
#endif
}

bool VConsoleServer::initialize(uint16_t port, const std::string& bindAddr) {
    if (m_running) {
        return true;
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
#endif

    m_port = port;
    m_bindAddr = bindAddr;
    m_running = true;

    startListening();

    if (m_listenSocket == INVALID_SOCKET) {
        m_running = false;
        return false;
    }

#ifndef _WIN32
    setupOutputCapture();
#endif

    return true;
}

void VConsoleServer::stopListening() {
    if (m_listenSocket != INVALID_SOCKET) {
        ::shutdown(m_listenSocket, SHUT_RDWR);
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }
}

void VConsoleServer::startListening() {
    if (m_listenSocket != INVALID_SOCKET || !m_running) {
        return;
    }

    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET) {
        return;
    }

    int opt = 1;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    if (m_bindAddr == "0.0.0.0" || m_bindAddr.empty()) {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, m_bindAddr.c_str(), &serverAddr.sin_addr);
    }
    serverAddr.sin_port = htons(m_port);

    if (bind(m_listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return;
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return;
    }

    setNonBlocking(m_listenSocket);
}

void VConsoleServer::shutdown() {
    if (!m_running) {
        return;
    }

#ifndef _WIN32
    cleanupOutputCapture();
#endif

    m_running = false;

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto& client : m_clients) {
        ::shutdown(client.socket, SHUT_RDWR);
        closesocket(client.socket);
    }
    m_clients.clear();

    stopListening();

#ifdef _WIN32
    WSACleanup();
#endif
}

void VConsoleServer::tick() {
    if (!m_running) {
        return;
    }

#ifndef _WIN32
    readCapturedOutput();
#endif

    acceptClients();
    processClients();
}

void VConsoleServer::acceptClients() {
    if (m_listenSocket == INVALID_SOCKET) {
        return;
    }

    sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    SOCKET clientSocket = accept(m_listenSocket, (sockaddr*)&clientAddr, &clientAddrLen);

    if (clientSocket == INVALID_SOCKET) {
        return;
    }

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
    uint16_t clientPort = ntohs(clientAddr.sin_port);

    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);

        setNonBlocking(clientSocket);

        int opt = 1;
        setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(opt));

        m_clients.emplace_back(clientSocket, clientIP, clientPort);

        if (m_maxConnections > 0 && static_cast<int>(m_clients.size()) >= m_maxConnections) {
            stopListening();
        }
    }

    char logMsg[128];
    snprintf(logMsg, sizeof(logMsg), "[VConsole] Client connected: %s:%u\n", clientIP, clientPort);
    logLocal(logMsg);

    sendAINF(clientSocket);
    sendADON(clientSocket, "HLDS");
    sendCHAN(clientSocket);
}

void VConsoleServer::processClients() {
    std::lock_guard<std::mutex> lock(m_clientsMutex);

    std::vector<SOCKET> toRemove;

    for (auto& client : m_clients) {
        char buffer[4096];
        int bytesReceived = recv(client.socket, buffer, sizeof(buffer), 0);

        if (bytesReceived > 0) {
            handleClientMessage(client, buffer, bytesReceived);
        } else if (bytesReceived == 0) {
            toRemove.push_back(client.socket);
        } else {
#ifdef _WIN32
            if (SOCKET_ERROR_CODE != WSAEWOULDBLOCK) {
                toRemove.push_back(client.socket);
            }
#else
            if (SOCKET_ERROR_CODE != EWOULDBLOCK && SOCKET_ERROR_CODE != EAGAIN) {
                toRemove.push_back(client.socket);
            }
#endif
        }
    }

    for (SOCKET s : toRemove) {
        removeClient(s);
    }

    if (!toRemove.empty() && m_listenSocket == INVALID_SOCKET) {
        if (m_maxConnections == 0 || static_cast<int>(m_clients.size()) < m_maxConnections) {
            startListening();
        }
    }
}

void VConsoleServer::handleClientMessage(ClientInfo& client, const char* data, size_t len) {
    if (len < sizeof(VConChunk)) {
        return;
    }

    const VConChunk* header = reinterpret_cast<const VConChunk*>(data);
    std::string msgType(header->type, 4);

    if (msgType == "CMND") {
        uint16_t packetLen = ntohs(header->length);
        if (packetLen > len) {
            return;
        }

        const char* cmdData = data + sizeof(VConChunk);
        size_t cmdLen = packetLen - sizeof(VConChunk);

        if (cmdLen > 0) {
            std::string command(cmdData, strnlen(cmdData, cmdLen));
            if (!command.empty()) {
                char logMsg[512];
                snprintf(logMsg, sizeof(logMsg), "[VConsole] Command from %s:%u: %s\n",
                         client.ip.c_str(), client.port, command.c_str());
                logLocal(logMsg);

                extern void executeServerCommand(const std::string& cmd);
                executeServerCommand(command);
            }
        }
    } else {
        char logMsg[512];
        snprintf(logMsg, sizeof(logMsg), "[VConsole] Unknown packet type '%s', hex dump: ", msgType.c_str());
        logLocal(logMsg);

        std::string hexDump;
        for (size_t i = 0; i < len && i < 64; i++) {
            char hex[4];
            snprintf(hex, sizeof(hex), "%02X ", (unsigned char)data[i]);
            hexDump += hex;
        }
        if (len > 64) hexDump += "...";
        hexDump += "\n";
        logLocal(hexDump.c_str());
    }
}

void VConsoleServer::removeClient(SOCKET socket) {
    auto it = std::find_if(m_clients.begin(), m_clients.end(),
        [socket](const ClientInfo& c) { return c.socket == socket; });
    if (it != m_clients.end()) {
        char logMsg[128];
        snprintf(logMsg, sizeof(logMsg), "[VConsole] Client disconnected: %s:%u\n", it->ip.c_str(), it->port);
        logLocal(logMsg);

        ::shutdown(it->socket, SHUT_RDWR);
        closesocket(it->socket);
        m_clients.erase(it);
    }
}

size_t VConsoleServer::getClientCount() const {
    return m_clients.size();
}

void VConsoleServer::logLocal(const char* msg) {
    if (!m_logging) {
        return;
    }
#ifndef _WIN32
    if (m_origStdout != -1) {
        write(m_origStdout, msg, strlen(msg));
        return;
    }
#endif
    SERVER_PRINT(msg);
}

void VConsoleServer::sendPacket(SOCKET socket, const char* type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> packet;
    packet.reserve(sizeof(VConChunk) + payload.size());

    VConChunk header;
    memcpy(header.type, type, 4);
    header.version = htonl(0x000000D4);
    header.length = htons(static_cast<uint16_t>(sizeof(VConChunk) + payload.size()));
    header.handle = htons(0);

    packet.insert(packet.end(), reinterpret_cast<uint8_t*>(&header),
                  reinterpret_cast<uint8_t*>(&header) + sizeof(VConChunk));
    packet.insert(packet.end(), payload.begin(), payload.end());

    send(socket, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0);
}

void VConsoleServer::sendAINF(SOCKET socket) {
    std::vector<uint8_t> payload(77, 0);
    sendPacket(socket, "AINF", payload);
}

void VConsoleServer::sendADON(SOCKET socket, const std::string& name) {
    std::vector<uint8_t> payload;
    uint16_t unknown = htons(0);
    uint16_t nameLen = htons(static_cast<uint16_t>(name.length()));

    payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&unknown),
                   reinterpret_cast<uint8_t*>(&unknown) + 2);
    payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&nameLen),
                   reinterpret_cast<uint8_t*>(&nameLen) + 2);
    payload.insert(payload.end(), name.begin(), name.end());

    sendPacket(socket, "ADON", payload);
}

void VConsoleServer::sendCHAN(SOCKET socket) {
    std::vector<uint8_t> payload;

    uint16_t numChannels = htons(1);
    payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&numChannels),
                   reinterpret_cast<uint8_t*>(&numChannels) + 2);

    int32_t id = htonl(0);
    int32_t unknown1 = htonl(0);
    int32_t unknown2 = htonl(0);
    int32_t verbosity_default = htonl(1);
    int32_t verbosity_current = htonl(1);
    uint32_t color = htonl(0xFFFFFFFF);
    char name[34] = "Console";

    payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&id),
                   reinterpret_cast<uint8_t*>(&id) + 4);
    payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&unknown1),
                   reinterpret_cast<uint8_t*>(&unknown1) + 4);
    payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&unknown2),
                   reinterpret_cast<uint8_t*>(&unknown2) + 4);
    payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&verbosity_default),
                   reinterpret_cast<uint8_t*>(&verbosity_default) + 4);
    payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&verbosity_current),
                   reinterpret_cast<uint8_t*>(&verbosity_current) + 4);
    payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&color),
                   reinterpret_cast<uint8_t*>(&color) + 4);
    payload.insert(payload.end(), name, name + 34);

    sendPacket(socket, "CHAN", payload);
}

std::vector<uint8_t> VConsoleServer::createPRNTPacket(const std::string& message, int32_t channelId, uint32_t color) {
    std::vector<uint8_t> payload;

    // offset 0: channelId (4 bytes)
    int32_t chanId = htonl(channelId);
    payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&chanId),
                   reinterpret_cast<uint8_t*>(&chanId) + 4);

    // offset 4: unknown1 (8 bytes)
    uint8_t unknown1[8] = {0};
    payload.insert(payload.end(), unknown1, unknown1 + 8);

    // offset 12: color (4 bytes) - client reads color from here
    uint32_t col = htonl(color);
    payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&col),
                   reinterpret_cast<uint8_t*>(&col) + 4);

    // offset 16: unknown2 (12 bytes) - padding to reach offset 28
    uint8_t unknown2[12] = {0};
    payload.insert(payload.end(), unknown2, unknown2 + 12);

    // offset 28: message (null-terminated)
    payload.insert(payload.end(), message.begin(), message.end());
    payload.push_back(0);

    return payload;
}

void VConsoleServer::broadcastPrint(const std::string& message, int32_t channelId, uint32_t color) {
    if (!m_running || message.empty()) {
        return;
    }

    std::vector<uint8_t> payload = createPRNTPacket(message, channelId, color);

    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto& client : m_clients) {
        sendPacket(client.socket, "PRNT", payload);
    }
}

#ifndef _WIN32
void VConsoleServer::setupOutputCapture() {
    if (m_captureActive) {
        return;
    }

    if (pipe(m_stdoutPipe) == -1 || pipe(m_stderrPipe) == -1) {
        return;
    }

    m_origStdout = dup(STDOUT_FILENO);
    m_origStderr = dup(STDERR_FILENO);

    if (m_origStdout == -1 || m_origStderr == -1) {
        close(m_stdoutPipe[0]);
        close(m_stdoutPipe[1]);
        close(m_stderrPipe[0]);
        close(m_stderrPipe[1]);
        return;
    }

    dup2(m_stdoutPipe[1], STDOUT_FILENO);
    dup2(m_stderrPipe[1], STDERR_FILENO);

    int flags = fcntl(m_stdoutPipe[0], F_GETFL, 0);
    fcntl(m_stdoutPipe[0], F_SETFL, flags | O_NONBLOCK);

    flags = fcntl(m_stderrPipe[0], F_GETFL, 0);
    fcntl(m_stderrPipe[0], F_SETFL, flags | O_NONBLOCK);

    m_captureActive = true;
}

void VConsoleServer::cleanupOutputCapture() {
    if (!m_captureActive) {
        return;
    }

    m_captureActive = false;

    if (m_origStdout != -1) {
        dup2(m_origStdout, STDOUT_FILENO);
        close(m_origStdout);
        m_origStdout = -1;
    }

    if (m_origStderr != -1) {
        dup2(m_origStderr, STDERR_FILENO);
        close(m_origStderr);
        m_origStderr = -1;
    }

    if (m_stdoutPipe[0] != -1) {
        close(m_stdoutPipe[0]);
        close(m_stdoutPipe[1]);
        m_stdoutPipe[0] = m_stdoutPipe[1] = -1;
    }

    if (m_stderrPipe[0] != -1) {
        close(m_stderrPipe[0]);
        close(m_stderrPipe[1]);
        m_stderrPipe[0] = m_stderrPipe[1] = -1;
    }
}

void VConsoleServer::readCapturedOutput() {
    if (!m_captureActive) {
        return;
    }

    char buffer[4096];

    ssize_t bytesRead = read(m_stdoutPipe[0], buffer, sizeof(buffer) - 1);
    while (bytesRead > 0) {
        buffer[bytesRead] = '\0';

        if (m_origStdout != -1) {
            write(m_origStdout, buffer, bytesRead);
        }

        m_partialLine += buffer;

        size_t pos;
        while ((pos = m_partialLine.find('\n')) != std::string::npos) {
            std::string line = m_partialLine.substr(0, pos + 1);
            m_partialLine.erase(0, pos + 1);
            broadcastPrint(line);
        }

        bytesRead = read(m_stdoutPipe[0], buffer, sizeof(buffer) - 1);
    }

    bytesRead = read(m_stderrPipe[0], buffer, sizeof(buffer) - 1);
    while (bytesRead > 0) {
        buffer[bytesRead] = '\0';

        if (m_origStderr != -1) {
            write(m_origStderr, buffer, bytesRead);
        }

        std::string errMsg(buffer, bytesRead);
        size_t pos;
        while ((pos = errMsg.find('\n')) != std::string::npos) {
            std::string line = errMsg.substr(0, pos + 1);
            errMsg.erase(0, pos + 1);
            broadcastPrint(line, 0, 0xFFFF0000);
        }
        if (!errMsg.empty()) {
            broadcastPrint(errMsg, 0, 0xFFFF0000);
        }

        bytesRead = read(m_stderrPipe[0], buffer, sizeof(buffer) - 1);
    }
}
#endif
