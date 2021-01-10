#define ASIO_STANDALONE

#include <iostream>

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <functional>

typedef websocketpp::server<websocketpp::config::asio> server;

// TODO figure out how to bind this to some sort of analyzer object
void on_open(websocketpp::connection_hdl conn) {
    std::cout << "new connection" << std::endl;
    std::cout << "starting the mirlin analyzer" << std::endl;
}

class MirlinServer {
public:
    MirlinServer() {
        std::cout << "starting the mirlin server" << std::endl;

        // Set logging settings
        endpoint_.set_error_channels(websocketpp::log::elevel::all);
        endpoint_.set_access_channels(websocketpp::log::alevel::all ^
                                      websocketpp::log::alevel::frame_payload);

        endpoint_.init_asio();

        endpoint_.set_open_handler(&on_open);
    }

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
