#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Minimal RFC 6455 WebSocket server.
//
// Threading model
// ---------------
//   AcceptThread  – blocks on accept(), spawns a ClientThread per connection
//   ClientThread  – one per connection, blocks on recv(), queues messages
//   Main thread   – calls PollMessage() + SendToClient() / BroadcastEvent()
//
// Engine / game state is only touched from the main thread.
// WebSocket threads only touch the thread-safe queues and client list.

class WebSocketServer
{
public:
    struct Message
    {
        std::string text;      // raw JSON payload
        std::string clientId;  // opaque ID; pass back to SendToClient
    };

    explicit WebSocketServer(uint16_t port = 9876);
    ~WebSocketServer();

    // Start listening (non-blocking – spawns background thread).
    bool Start();
    void Stop();
    bool IsRunning() const { return m_running.load(); }
    int  GetConnectedClientCount() const;

    // ---- Main-thread API ----------------------------------------

    // Returns true and fills |out| if a message is waiting.
    bool PollMessage(Message& out);

    // Send a JSON response to the client that sent |clientId|.
    void SendToClient(const std::string& clientId, const std::string& json);

    // Push an unsolicited event JSON to every connected client.
    void BroadcastEvent(const std::string& json);

private:
    struct ClientEntry
    {
        SOCKET      socket  = INVALID_SOCKET;
        std::string id;
    };

    // Background threads
    void AcceptLoop();
    void ClientLoop(SOCKET sock, std::string clientId);

    // WebSocket protocol helpers
    bool        DoHandshake(SOCKET sock);
    bool        RecvExact(SOCKET sock, uint8_t* buf, size_t len);
    bool        RecvFrame(SOCKET sock, std::string& outText, bool& outClose);
    void        SendTextFrame(SOCKET sock, const std::string& text);
    void        SendPongFrame(SOCKET sock, const std::vector<uint8_t>& payload);
    void        SendCloseFrame(SOCKET sock);

    // Crypto helpers (implemented in .cpp anonymous namespace)
    static std::string MakeAcceptKey(const std::string& clientKey);

    // State
    uint16_t              m_port;
    SOCKET                m_listenSocket = INVALID_SOCKET;
    std::atomic<bool>     m_running{ false };
    std::thread           m_acceptThread;

    mutable std::mutex        m_clientsMtx;
    std::vector<ClientEntry>  m_clients;         // guarded by m_clientsMtx
    std::vector<std::thread>  m_clientThreads;   // guarded by m_clientsMtx

    std::mutex            m_inQueueMtx;
    std::queue<Message>   m_inQueue;             // WS threads → main thread

    std::atomic<uint32_t> m_nextId{ 1 };
};
