// stolen from: https://github.com/adamrehn/websocket-server-demo/blob/master/server/WebsocketServer.h
#ifndef _WEBSOCKET_SERVER
#define _WEBSOCKET_SERVER

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <json/json.h>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

using std::map;
using std::string;
using std::vector;

typedef websocketpp::server<websocketpp::config::asio> WebsocketEndpoint;
typedef websocketpp::connection_hdl ClientConnection;

class WebsocketServer {
public:
    WebsocketServer();
    void run(int port);

    // Returns the number of currently connected clients
    size_t num_connections();

    // Registers a callback for when a client connects
    template <typename CallbackTy> void connect(CallbackTy handler) {
        // Make sure we only access the handlers list from the networking thread
        this->event_loop_.post([this, handler]() { this->connect_handlers_.push_back(handler); });
    }

    // Registers a callback for when a client disconnects
    template <typename CallbackTy> void disconnect(CallbackTy handler) {
        // Make sure we only access the handlers list from the networking thread
        this->event_loop_.post(
            [this, handler]() { this->disconnect_handlers_.push_back(handler); });
    }

    // Registers a callback for when a particular type of message is received
    template <typename CallbackTy> void message(const string& message_type, CallbackTy handler) {
        // Make sure we only access the handlers list from the networking thread
        this->event_loop_.post([this, message_type, handler]() {
            this->message_handlers_[message_type].push_back(handler);
        });
    }

    // Sends a message to an individual client
    //(Note: the data transmission will take place on the thread that called WebsocketServer::run())
    void send_message(ClientConnection conn, const string& message_type,
                      const Json::Value& arguments);

    // Sends a message to all connected clients
    //(Note: the data transmission will take place on the thread that called WebsocketServer::run())
    void broadcast_message(const string& message_type, const Json::Value& arguments);

protected:
    static Json::Value parse_json(const string& json);
    static string stringify_json(const Json::Value& val);

    void on_open(ClientConnection conn);
    void on_close(ClientConnection conn);
    void on_message(ClientConnection conn, WebsocketEndpoint::message_ptr msg);

    asio::io_service event_loop_;
    WebsocketEndpoint endpoint_;
    vector<ClientConnection> open_connections_;
    std::mutex connection_list_mutex_;

    vector<std::function<void(ClientConnection)>> connect_handlers_;
    vector<std::function<void(ClientConnection)>> disconnect_handlers_;
    map<string, vector<std::function<void(ClientConnection, const Json::Value&)>>>
        message_handlers_;
};

#endif
