#include <asio/io_service.hpp>
#include <iostream>
#include <thread>

#include "WebsocketServer.hpp"

// The port number the WebSocket server listens on
#define PORT_NUMBER 9002

int main(int argc, char* argv[]) {
    std::clog << "Starting the mirlin server..." << std::endl;

    // Create the event loop for the main thread, and the WebSocket server
    asio::io_service main_event_loop;
    WebsocketServer server;

    // Register our network callbacks, ensuring the logic is run on the main thread's event loop
    server.connect([&main_event_loop, &server](ClientConnection conn) {
        main_event_loop.post([conn, &server]() {
            std::clog << "Connection opened." << std::endl;
            std::clog << "There are now " << server.num_connections() << " open connections."
                      << std::endl;

            // Send a hello message to the client
            server.send_message(conn, "hello", Json::Value());
        });
    });

    server.disconnect([&main_event_loop, &server](ClientConnection conn) {
        main_event_loop.post([conn, &server]() {
            std::clog << "Connection closed." << std::endl;
            std::clog << "There are now " << server.num_connections() << " open connections."
                      << std::endl;
        });
    });

    server.message(
        "message", [&main_event_loop, &server](ClientConnection conn, const Json::Value& args) {
            main_event_loop.post([conn, args, &server]() {
                std::clog << "message handler on the main thread" << std::endl;
                std::clog << "Message payload:" << std::endl;
                for (auto key : args.getMemberNames()) {
                    std::clog << "\t" << key << ": " << args[key].asString() << std::endl;
                }

                // Echo the message pack to the client
                server.send_message(conn, "message", args);
            });
        });

    // Start the networking thread
    std::thread server_thread([&server]() { server.run(PORT_NUMBER); });

    // Start the event loop for the main thread
    asio::io_service::work work(main_event_loop);
    main_event_loop.run();

    return 0;
}
