#include <asio/io_service.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "Analyzer.hpp"
#include "WebsocketServer.hpp"

// The port number the WebSocket server listens on
#define PORT_NUMBER 9002

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

            analyzer.end_session();
            analyzer = Analyzer();
        });
    });

    server.message("subscription_request", [&main_event_loop, &server, &analyzer](
                                               ClientConnection conn, const Json::Value& args) {
        main_event_loop.post([conn, args, &server, &analyzer]() {
            if (analyzer.busy) {
                // TODO: respond with error and disconnect
                return;
            }

            std::clog << "Message payload:" << std::endl;

            auto sample_rate = args["payload"]["sample_rate"].asUInt();
            std::clog << "\tsample_rate: " << sample_rate << std::endl;

            auto hop_size = args["payload"]["hop_size"].asUInt();
            std::clog << "\thop_size: " << hop_size << std::endl;

            auto memory = args["payload"]["memory"].asUInt();
            std::clog << "\tmemory: " << memory << std::endl;

            std::clog << "\tfeatures:" << std::endl;
            auto json_features = args["payload"]["features"];
            std::vector<std::string> features;

            for (Json::Value::ArrayIndex i = 0; i != json_features.size(); i++) {
                auto feature = json_features[i].asString();
                std::clog << "\t\t- " << feature << std::endl;
                features.push_back(feature);
            }

            analyzer.start_session(sample_rate, hop_size, memory, features);

            Json::Value payload;
            payload["status"] = "ok";

            Json::Value confirmation;
            confirmation["payload"] = payload;

            std::clog << "Sending subscription confirmation" << std::endl;
            server.send_message(conn, "subscription_confirmation", confirmation);
        });
    });

    server.message("audio_frame", [&main_event_loop, &server, &analyzer](ClientConnection conn,
                                                                         const Json::Value& args) {
        main_event_loop.post([conn, args, &server, &analyzer]() {
            std::clog << "Received audio frame" << std::endl;

            auto json_frame = args["payload"];
            std::vector<float> frame;

            for (Json::Value::ArrayIndex i = 0; i != json_frame.size(); i++) {
                float sample = json_frame[i].asFloat();
                frame.push_back(sample);
            }

            float onset_strength = analyzer.process_frame(frame);

            Json::Value features;
            features["onset"] = onset_strength > 0;
            features["pitch"] = "C";

            Json::Value payload;
            payload["features"] = features;

            Json::Value features_msg;
            features_msg["payload"] = payload;

            std::clog << "Sending features to client" << std::endl;
            server.send_message(conn, "audio_features", features_msg);
        });
    });

    // Start the networking thread
    std::thread server_thread([&server]() { server.run(PORT_NUMBER); });

    // Start the event loop for the main thread
    asio::io_service::work work(main_event_loop);
    main_event_loop.run();

    return 0;
}
