#define ASIO_STANDALONE

#include <iostream>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <functional>

typedef websocketpp::server<websocketpp::config::asio> server;

class MirlinServer {
public:
    MirlinServer() {
        std::cout << "starting the mirlin server" << std::endl;

        // Set logging settings
        endpoint_.set_error_channels(websocketpp::log::elevel::all);
        endpoint_.set_access_channels(websocketpp::log::alevel::all ^
                                      websocketpp::log::alevel::frame_payload);

        // Initialize Asio
        endpoint_.init_asio();
    }

    // TODO attach a handler to the open event on the endpoint
    // https://docs.websocketpp.org/md_tutorials_utility_server_utility_server.html
    // in the handler attach events for incoming messages and start analyzing incoming audio
    void run() {
        endpoint_.listen(9002);

        // Queues a connection accept operation
        endpoint_.start_accept();

        // Start the Asio io_service run loop
        endpoint_.run();
    }

private:
    server endpoint_;
};

int main(int argc, char const* argv[]) {
    MirlinServer s;
    s.run();
    return 0;
}
