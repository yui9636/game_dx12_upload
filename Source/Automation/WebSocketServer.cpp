#include "Automation/WebSocketServer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <sstream>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

namespace
{
    constexpr const char* kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    constexpr uint64_t kMaxFramePayloadBytes = 16ull * 1024ull * 1024ull;

    uint32_t Rotl32(uint32_t value, int bits)
    {
        return (value << bits) | (value >> (32 - bits));
    }

    std::array<uint8_t, 20> Sha1(const std::string& input)
    {
        uint32_t h0 = 0x67452301u;
        uint32_t h1 = 0xEFCDAB89u;
        uint32_t h2 = 0x98BADCFEu;
        uint32_t h3 = 0x10325476u;
        uint32_t h4 = 0xC3D2E1F0u;

        std::vector<uint8_t> bytes(input.begin(), input.end());
        const uint64_t bitLength = static_cast<uint64_t>(bytes.size()) * 8ull;
        bytes.push_back(0x80u);
        while ((bytes.size() % 64u) != 56u) {
            bytes.push_back(0u);
        }
        for (int i = 7; i >= 0; --i) {
            bytes.push_back(static_cast<uint8_t>((bitLength >> (i * 8)) & 0xffu));
        }

        for (size_t chunk = 0; chunk < bytes.size(); chunk += 64u) {
            uint32_t w[80] = {};
            for (int i = 0; i < 16; ++i) {
                const size_t j = chunk + static_cast<size_t>(i) * 4u;
                w[i] =
                    (static_cast<uint32_t>(bytes[j + 0]) << 24) |
                    (static_cast<uint32_t>(bytes[j + 1]) << 16) |
                    (static_cast<uint32_t>(bytes[j + 2]) << 8) |
                    (static_cast<uint32_t>(bytes[j + 3]));
            }
            for (int i = 16; i < 80; ++i) {
                w[i] = Rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
            }

            uint32_t a = h0;
            uint32_t b = h1;
            uint32_t c = h2;
            uint32_t d = h3;
            uint32_t e = h4;

            for (int i = 0; i < 80; ++i) {
                uint32_t f = 0;
                uint32_t k = 0;
                if (i < 20) {
                    f = (b & c) | ((~b) & d);
                    k = 0x5A827999u;
                }
                else if (i < 40) {
                    f = b ^ c ^ d;
                    k = 0x6ED9EBA1u;
                }
                else if (i < 60) {
                    f = (b & c) | (b & d) | (c & d);
                    k = 0x8F1BBCDCu;
                }
                else {
                    f = b ^ c ^ d;
                    k = 0xCA62C1D6u;
                }

                const uint32_t temp = Rotl32(a, 5) + f + e + k + w[i];
                e = d;
                d = c;
                c = Rotl32(b, 30);
                b = a;
                a = temp;
            }

            h0 += a;
            h1 += b;
            h2 += c;
            h3 += d;
            h4 += e;
        }

        std::array<uint8_t, 20> digest = {};
        const uint32_t words[5] = { h0, h1, h2, h3, h4 };
        for (int i = 0; i < 5; ++i) {
            digest[static_cast<size_t>(i) * 4u + 0u] = static_cast<uint8_t>((words[i] >> 24) & 0xffu);
            digest[static_cast<size_t>(i) * 4u + 1u] = static_cast<uint8_t>((words[i] >> 16) & 0xffu);
            digest[static_cast<size_t>(i) * 4u + 2u] = static_cast<uint8_t>((words[i] >> 8) & 0xffu);
            digest[static_cast<size_t>(i) * 4u + 3u] = static_cast<uint8_t>(words[i] & 0xffu);
        }
        return digest;
    }

    std::string Base64Encode(const uint8_t* data, size_t len)
    {
        static constexpr char kTable[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string out;
        out.reserve(((len + 2u) / 3u) * 4u);
        for (size_t i = 0; i < len; i += 3u) {
            const uint32_t b0 = data[i];
            const uint32_t b1 = (i + 1u < len) ? data[i + 1u] : 0u;
            const uint32_t b2 = (i + 2u < len) ? data[i + 2u] : 0u;
            const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;

            out.push_back(kTable[(triple >> 18) & 0x3fu]);
            out.push_back(kTable[(triple >> 12) & 0x3fu]);
            out.push_back((i + 1u < len) ? kTable[(triple >> 6) & 0x3fu] : '=');
            out.push_back((i + 2u < len) ? kTable[triple & 0x3fu] : '=');
        }
        return out;
    }

    std::string TrimCopy(const std::string& value)
    {
        const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
        const auto first = std::find_if_not(value.begin(), value.end(), isSpace);
        const auto last = std::find_if_not(value.rbegin(), value.rend(), isSpace).base();
        if (first >= last) {
            return {};
        }
        return std::string(first, last);
    }

    std::string ToLowerCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    std::string GetHeaderValue(const std::string& request, const std::string& headerName)
    {
        const std::string desired = ToLowerCopy(headerName);
        std::istringstream stream(request);
        std::string line;
        while (std::getline(stream, line)) {
            const size_t colon = line.find(':');
            if (colon == std::string::npos) {
                continue;
            }
            const std::string name = ToLowerCopy(TrimCopy(line.substr(0, colon)));
            if (name == desired) {
                return TrimCopy(line.substr(colon + 1u));
            }
        }
        return {};
    }

    bool SendAll(SOCKET sock, const uint8_t* data, size_t len)
    {
        size_t sent = 0;
        while (sent < len) {
            const int chunk = static_cast<int>(std::min<size_t>(len - sent, 64u * 1024u));
            const int result = send(sock, reinterpret_cast<const char*>(data + sent), chunk, 0);
            if (result <= 0) {
                return false;
            }
            sent += static_cast<size_t>(result);
        }
        return true;
    }

    bool SendAll(SOCKET sock, const std::string& text)
    {
        return SendAll(sock, reinterpret_cast<const uint8_t*>(text.data()), text.size());
    }

    void AppendPayloadLength(std::vector<uint8_t>& frame, uint64_t len)
    {
        if (len <= 125ull) {
            frame.push_back(static_cast<uint8_t>(len));
            return;
        }

        if (len <= 0xffffull) {
            frame.push_back(126u);
            frame.push_back(static_cast<uint8_t>((len >> 8) & 0xffu));
            frame.push_back(static_cast<uint8_t>(len & 0xffu));
            return;
        }

        frame.push_back(127u);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xffu));
        }
    }

    void CloseSocketQuietly(SOCKET& sock)
    {
        if (sock == INVALID_SOCKET) {
            return;
        }
        shutdown(sock, SD_BOTH);
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
}

WebSocketServer::WebSocketServer(uint16_t port)
    : m_port(port)
{
}

WebSocketServer::~WebSocketServer()
{
    Stop();
}

bool WebSocketServer::Start()
{
    if (m_running.load()) {
        return true;
    }

    WSADATA wsaData = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    int reuse = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(m_port);

    if (bind(listenSock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(listenSock);
        WSACleanup();
        return false;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSock);
        WSACleanup();
        return false;
    }

    m_listenSocket = listenSock;
    m_nextId.store(1);
    m_running.store(true);

    try {
        m_acceptThread = std::thread(&WebSocketServer::AcceptLoop, this);
    }
    catch (...) {
        m_running.store(false);
        CloseSocketQuietly(m_listenSocket);
        WSACleanup();
        return false;
    }

    return true;
}

void WebSocketServer::Stop()
{
    if (!m_running.exchange(false)) {
        return;
    }

    CloseSocketQuietly(m_listenSocket);

    {
        std::lock_guard<std::mutex> lock(m_clientsMtx);
        for (ClientEntry& client : m_clients) {
            CloseSocketQuietly(client.socket);
        }
    }

    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }

    {
        using namespace std::chrono_literals;
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (m_activeClientCount.load() > 0 &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(10ms);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_clientsMtx);
        m_clients.clear();
    }

    {
        std::lock_guard<std::mutex> lock(m_inQueueMtx);
        std::queue<Message> empty;
        m_inQueue.swap(empty);
    }

    WSACleanup();
}

int WebSocketServer::GetConnectedClientCount() const
{
    std::lock_guard<std::mutex> lock(m_clientsMtx);
    return static_cast<int>(m_clients.size());
}

bool WebSocketServer::PollMessage(Message& out)
{
    std::lock_guard<std::mutex> lock(m_inQueueMtx);
    if (m_inQueue.empty()) {
        return false;
    }

    out = std::move(m_inQueue.front());
    m_inQueue.pop();
    return true;
}

void WebSocketServer::SendToClient(const std::string& clientId, const std::string& json)
{
    SOCKET target = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(m_clientsMtx);
        for (const ClientEntry& client : m_clients) {
            if (client.id == clientId && client.socket != INVALID_SOCKET) {
                target = client.socket;
                break;
            }
        }
    }
    if (target != INVALID_SOCKET) {
        SendTextFrame(target, json);
    }
}

void WebSocketServer::BroadcastEvent(const std::string& json)
{
    std::vector<SOCKET> sockets;
    {
        std::lock_guard<std::mutex> lock(m_clientsMtx);
        for (const ClientEntry& client : m_clients) {
            if (client.socket != INVALID_SOCKET) {
                sockets.push_back(client.socket);
            }
        }
    }
    for (SOCKET sock : sockets) {
        SendTextFrame(sock, json);
    }
}

void WebSocketServer::AcceptLoop()
{
    while (m_running.load()) {
        SOCKET clientSock = accept(m_listenSocket, nullptr, nullptr);
        if (clientSock == INVALID_SOCKET) {
            if (m_running.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            continue;
        }

        if (!m_running.load()) {
            closesocket(clientSock);
            break;
        }

        int timeoutMs = 5000;
        setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

        if (!DoHandshake(clientSock)) {
            closesocket(clientSock);
            continue;
        }

        timeoutMs = 0;
        setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

        const std::string clientId = "client-" + std::to_string(m_nextId.fetch_add(1));
        {
            std::lock_guard<std::mutex> lock(m_clientsMtx);
            m_clients.push_back(ClientEntry{ clientSock, clientId });
        }
        m_activeClientCount.fetch_add(1);
        std::thread(&WebSocketServer::ClientLoop, this, clientSock, clientId).detach();
    }
}

void WebSocketServer::ClientLoop(SOCKET sock, std::string clientId)
{
    while (m_running.load()) {
        std::string text;
        bool close = false;
        if (!RecvFrame(sock, text, close)) {
            break;
        }

        if (close) {
            SendCloseFrame(sock);
            break;
        }

        {
            std::lock_guard<std::mutex> lock(m_inQueueMtx);
            m_inQueue.push(Message{ std::move(text), clientId });
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_clientsMtx);
        auto it = std::remove_if(m_clients.begin(), m_clients.end(), [&](ClientEntry& client) {
            if (client.id == clientId) {
                CloseSocketQuietly(client.socket);
                return true;
            }
            return false;
        });
        m_clients.erase(it, m_clients.end());
    }
    m_activeClientCount.fetch_sub(1);
}

bool WebSocketServer::DoHandshake(SOCKET sock)
{
    std::string request;
    char buffer[1024] = {};

    while (request.find("\r\n\r\n") == std::string::npos) {
        const int received = recv(sock, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            return false;
        }
        request.append(buffer, static_cast<size_t>(received));
        if (request.size() > 16u * 1024u) {
            return false;
        }
    }

    const std::string clientKey = GetHeaderValue(request, "Sec-WebSocket-Key");
    if (clientKey.empty()) {
        return false;
    }

    const std::string acceptKey = MakeAcceptKey(clientKey);
    const std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + acceptKey + "\r\n"
        "\r\n";

    return SendAll(sock, response);
}

bool WebSocketServer::RecvExact(SOCKET sock, uint8_t* buf, size_t len)
{
    size_t read = 0;
    while (read < len && m_running.load()) {
        const int chunk = static_cast<int>(std::min<size_t>(len - read, 64u * 1024u));
        const int received = recv(sock, reinterpret_cast<char*>(buf + read), chunk, 0);
        if (received <= 0) {
            return false;
        }
        read += static_cast<size_t>(received);
    }
    return read == len;
}

bool WebSocketServer::RecvFrame(SOCKET sock, std::string& outText, bool& outClose)
{
    outText.clear();
    outClose = false;

    std::string fragmented;
    bool inFragmented = false;

    for (;;) {
        uint8_t header[2] = {};
        if (!RecvExact(sock, header, sizeof(header))) {
            return false;
        }

        const bool fin    = (header[0] & 0x80u) != 0;
        const uint8_t opcode = header[0] & 0x0fu;
        const bool masked = (header[1] & 0x80u) != 0;
        uint64_t payloadLen = header[1] & 0x7fu;

        // Control frames must not be fragmented (RFC 6455 §5.5)
        const bool isControl = (opcode >= 0x8u);
        if (isControl && !fin) {
            return false;
        }

        if (payloadLen == 126u) {
            uint8_t ext[2] = {};
            if (!RecvExact(sock, ext, sizeof(ext))) {
                return false;
            }
            payloadLen = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
        }
        else if (payloadLen == 127u) {
            uint8_t ext[8] = {};
            if (!RecvExact(sock, ext, sizeof(ext))) {
                return false;
            }
            payloadLen = 0;
            for (uint8_t byte : ext) {
                payloadLen = (payloadLen << 8) | byte;
            }
        }

        if (!masked) {
            return false;
        }

        // Guard total accumulated size against the limit
        if (fragmented.size() + payloadLen > kMaxFramePayloadBytes) {
            return false;
        }

        uint8_t mask[4] = {};
        if (!RecvExact(sock, mask, sizeof(mask))) {
            return false;
        }

        std::vector<uint8_t> payload(static_cast<size_t>(payloadLen));
        if (payloadLen > 0 && !RecvExact(sock, payload.data(), payload.size())) {
            return false;
        }

        for (size_t i = 0; i < payload.size(); ++i) {
            payload[i] ^= mask[i % 4u];
        }

        if (opcode == 0x1u) {
            // Text frame: start of a (possibly fragmented) message
            if (inFragmented) {
                return false; // protocol error: new data frame during fragmentation
            }
            if (fin) {
                outText.assign(reinterpret_cast<const char*>(payload.data()), payload.size());
                return true;
            }
            // FIN=0: begin fragmented message
            fragmented.assign(reinterpret_cast<const char*>(payload.data()), payload.size());
            inFragmented = true;
            continue;
        }

        if (opcode == 0x0u) {
            // Continuation frame
            if (!inFragmented) {
                return false; // protocol error: continuation without initial frame
            }
            fragmented.append(reinterpret_cast<const char*>(payload.data()), payload.size());
            if (fin) {
                outText = std::move(fragmented);
                return true;
            }
            continue;
        }

        if (opcode == 0x8u) {
            outClose = true;
            return true;
        }
        if (opcode == 0x9u) {
            SendPongFrame(sock, payload);
            continue;
        }
        if (opcode == 0xAu) {
            continue;
        }

        return false;
    }
}

void WebSocketServer::SendTextFrame(SOCKET sock, const std::string& text)
{
    std::vector<uint8_t> frame;
    frame.reserve(text.size() + 10u);
    frame.push_back(0x81u);
    AppendPayloadLength(frame, static_cast<uint64_t>(text.size()));
    frame.insert(frame.end(), text.begin(), text.end());
    SendAll(sock, frame.data(), frame.size());
}

void WebSocketServer::SendPongFrame(SOCKET sock, const std::vector<uint8_t>& payload)
{
    if (payload.size() > 125u) {
        return;
    }

    std::vector<uint8_t> frame;
    frame.reserve(payload.size() + 2u);
    frame.push_back(0x8Au);
    AppendPayloadLength(frame, static_cast<uint64_t>(payload.size()));
    frame.insert(frame.end(), payload.begin(), payload.end());
    SendAll(sock, frame.data(), frame.size());
}

void WebSocketServer::SendCloseFrame(SOCKET sock)
{
    const uint8_t frame[2] = { 0x88u, 0x00u };
    SendAll(sock, frame, sizeof(frame));
}

std::string WebSocketServer::MakeAcceptKey(const std::string& clientKey)
{
    const auto digest = Sha1(clientKey + kWebSocketGuid);
    return Base64Encode(digest.data(), digest.size());
}
