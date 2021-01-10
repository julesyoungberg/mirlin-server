#include <algorithm>
#include <functional>
#include <iostream>

#include "WebsocketServer.hpp"

// The name of the special JSON field that holds the message type for messages
#define MESSAGE_FIELD "__MESSAGE__"

Json::Value WebsocketServer::parse_json(const string& json) {
    Json::Value root;
    Json::Reader reader;
    reader.parse(json, root);
    return root;
}

string WebsocketServer::stringify_json(const Json::Value& val) {
    // When we transmit JSON data, we omit all whitespace
    Json::StreamWriterBuilder wbuilder;
    wbuilder["commentStyle"] = "None";
    wbuilder["indentation"] = "";

    return Json::writeString(wbuilder, val);
}

WebsocketServer::WebsocketServer() {
    this->endpoint_.clear_access_channels(websocketpp::log::alevel::control);
    this->endpoint_.clear_access_channels(websocketpp::log::alevel::frame_header);
    this->endpoint_.clear_access_channels(websocketpp::log::alevel::frame_payload);

    // Wire up our event handlers
    this->endpoint_.set_open_handler(
        std::bind(&WebsocketServer::on_open, this, std::placeholders::_1));
    this->endpoint_.set_close_handler(
        std::bind(&WebsocketServer::on_close, this, std::placeholders::_1));
    this->endpoint_.set_message_handler(
        std::bind(&WebsocketServer::on_message, this, std::placeholders::_1, std::placeholders::_2));

    // Initialise the Asio library, using our own event loop object
    this->endpoint_.init_asio(&(this->event_loop_));
}

void WebsocketServer::run(int port) {
    // Listen on the specified port number and start accepting connections
    this->endpoint_.listen(port);
    this->endpoint_.start_accept();

    // Start the Asio event loop
    this->endpoint_.run();
}

size_t WebsocketServer::num_connections() {
    // Prevent concurrent access to the list of open connections from multiple threads
    std::lock_guard<std::mutex> lock(this->connection_list_mutex_);

    return this->open_connections_.size();
}

void WebsocketServer::send_message(ClientConnection conn, const string& message_type,
                                  const Json::Value& arguments) {
    // Copy the argument values, and bundle the message type into the object
    Json::Value message_data = arguments;
    message_data[MESSAGE_FIELD] = message_type;

    // Send the JSON data to the client (will happen on the networking thread's event loop)
    this->endpoint_.send(conn, WebsocketServer::stringify_json(message_data),
                        websocketpp::frame::opcode::text);
}

void WebsocketServer::broadcast_message(const string& message_type, const Json::Value& arguments) {
    // Prevent concurrent access to the list of open connections from multiple threads
    std::lock_guard<std::mutex> lock(this->connection_list_mutex_);

    for (auto conn : this->open_connections_) {
        this->send_message(conn, message_type, arguments);
    }
}

void WebsocketServer::on_open(ClientConnection conn) {
    {
        // Prevent concurrent access to the list of open connections from multiple threads
        std::lock_guard<std::mutex> lock(this->connection_list_mutex_);

        // Add the connection handle to our list of open connections
        this->open_connections_.push_back(conn);
    }

    // Invoke any registered handlers
    for (auto handler : this->connect_handlers_) {
        handler(conn);
    }
}

void WebsocketServer::on_close(ClientConnection conn) {
    {
        // Prevent concurrent access to the list of open connections from multiple threads
        std::lock_guard<std::mutex> lock(this->connection_list_mutex_);

        // Remove the connection handle from our list of open connections
        auto conn_val = conn.lock();
        auto new_end = std::remove_if(this->open_connections_.begin(), this->open_connections_.end(),
                                     [&conn_val](ClientConnection elem) {
                                         // If the pointer has expired, remove it from the vector
                                         if (elem.expired() == true) {
                                             return true;
                                         }

                                         // If the pointer is still valid, compare it to the handle
                                         // for the closed connection
                                         auto elem_val = elem.lock();
                                         if (elem_val.get() == conn_val.get()) {
                                             return true;
                                         }

                                         return false;
                                     });

        // Truncate the connections vector to erase the removed elements
        this->open_connections_.resize(std::distance(this->open_connections_.begin(), new_end));
    }

    // Invoke any registered handlers
    for (auto handler : this->disconnect_handlers_) {
        handler(conn);
    }
}

void WebsocketServer::on_message(ClientConnection conn, WebsocketEndpoint::message_ptr msg) {
    // Validate that the incoming message contains valid JSON
    Json::Value message_object = WebsocketServer::parse_json(msg->get_payload());
    if (message_object.isNull() == false) {
        // Validate that the JSON object contains the message type field
        if (message_object.isMember(MESSAGE_FIELD)) {
            // Extract the message type and remove it from the payload
            std::string message_type = message_object[MESSAGE_FIELD].asString();
            message_object.removeMember(MESSAGE_FIELD);

            // If any handlers are registered for the message type, invoke them
            auto& handlers = this->message_handlers_[message_type];
            for (auto handler : handlers) {
                handler(conn, message_object);
            }
        }
    }
}
