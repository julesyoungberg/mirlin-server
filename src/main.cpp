#include <asio/io_service.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "WebsocketServer.hpp"

// The port number the WebSocket server listens on
#define PORT_NUMBER 9002

class Analyzer {
public:
    Analyzer() {}

    void start_session(std::vector<string> features) {
        std::clog << "Session initiated, starting the mirlin analyzer..." << std::endl;
        this->features_ = features;
        this->busy = true;
    }

    void end_session() {
        std::clog << "Session ended" << std::endl;
        this->busy = false;
    }

    bool busy = false;

private:
    std::vector<string> features_;

};

int main(int argc, char* argv[]) {
    std::clog << "Starting the mirlin server..." << std::endl;

    // Create the event loop for the main thread, and the WebSocket server
    asio::io_service main_event_loop;
    WebsocketServer server;
    Analyzer analyzer;

    // Register our network callbacks, ensuring the logic is run on the main thread's event loop
    server.connect([&main_event_loop, &server](ClientConnection conn) {
        main_event_loop.post([conn, &server]() {
            std::clog << "Connection opened." << std::endl;
            std::clog << "There are now " << server.num_connections() << " open connections."
                      << std::endl;
        });
    });

    server.disconnect([&main_event_loop, &server, &analyzer](ClientConnection conn) {
        main_event_loop.post([conn, &server, &analyzer]() {
            std::clog << "Connection closed." << std::endl;
            std::clog << "There are now " << server.num_connections() << " open connections."
                      << std::endl;

            if (server.num_connections() == 0) {
                analyzer.end_session();
            }
        });
    });

    server.message(
        "subscription_request", [&main_event_loop, &server, &analyzer](ClientConnection conn, const Json::Value& args) {
            main_event_loop.post([conn, args, &server, &analyzer]() {
                if (analyzer.busy) {
                    // TODO: respond with error and disconnect
                    return;
                }

                std::clog << "Message payload:" << std::endl;
                std::clog << "type: " << args["type"] << std::endl;
                std::clog << "payload:" << std::endl;
                std::clog << "\tfeatures:" << std::endl;

                auto json_features = args["payload"]["features"];
                std::vector<string> features;

                for (Json::Value::ArrayIndex i = 0; i != json_features.size(); i++) {
                    auto feature = json_features[i].asString();
                    std::clog << "\t\t- " << feature << std::endl;
                    features.push_back(feature);
                }

                analyzer.start_session(features);

                Json::Value payload;
                payload["status"] = "ok";

                Json::Value confirmation;
                confirmation["payload"] = payload;

                server.send_message(conn, "subscription_confirmation", confirmation);
            });
        });

    // Start the networking thread
    std::thread server_thread([&server]() { server.run(PORT_NUMBER); });

    // Start the event loop for the main thread
    asio::io_service::work work(main_event_loop);
    main_event_loop.run();

    return 0;
}
