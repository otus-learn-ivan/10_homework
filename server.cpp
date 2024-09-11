//#include <boost/thread/thread.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <boost/asio/signal_set.hpp>
#include "server.h"
#include <thread>
#include <signal.h>

using namespace tp_network_client;

bool g_authed = false;

void process_server_response(
        connection_ptr&& soc,
        const boost::system::error_code& err)
{
    if (err && err != boost::asio::error::eof) {
        std::cerr << "Client error on receive: " << err.message() << '\n';
        assert(false);
    }

    if (soc->data.size() != 2) {
        std::cerr << "Wrong bytes count\n";
        assert(false);
    }

    if (soc->data != "OK") {
        std::cerr << "Wrong response: " << soc->data << '\n';
        assert(false);
    }

    std::cout <<"process_server_response: "<< soc->data <<std::endl;

    g_authed = true;
    soc->shutdown();
    tasks_processor::stop();
}

void receive_auth_response(
    connection_ptr&& soc,
    const boost::system::error_code& err)
{
    if (err) {
        std::cerr << "Error on sending data: " << err.message() << '\n';
        assert(false);
    }

    async_read_data(
        std::move(soc),
        &process_server_response,
        2
    );
}

void send_auth() {
    std::cout << "send_auth start\n";
    connection_ptr soc = tasks_processor::create_connection(
        "127.0.0.1", g_port_num
    );
    soc->data = "auth_name";

    async_write_data(
        std::move(soc),
        &receive_auth_response
    );
    std::cout << "send_auth end\n";
}

extern void parser_bulk(std::string cmd);

using namespace tp_network;

class authorizer {
public:
    static void on_connection_accpet(
        connection_ptr&& connection,
        const boost::system::error_code& error)
    {
        if (error) return;
        async_read_data_at_least(std::move(connection), &authorizer::on_datarecieve, 1, 1024);
    }

    static void on_datarecieve(connection_ptr&& connection, const boost::system::error_code& error) {

        //std::cout <<"on_datarecieve: " << connection->data <<std::endl;

        if (error) {
            std::cerr << "authorizer.on_datarecieve: error during recieving response: " << error << '\n';
            assert(false);
        }

        if (connection->data.size() == 0) {
            std::cerr << "authorizer.on_datarecieve: zero bytes recieved\n";
            assert(false);
        }

        //assert(connection->data == "auth_name");

        //std::cout <<"on_datarecieve: \n" << connection->data <<std::endl;

        // We have data and now we can
        // do some authorization.

        parser_bulk( connection->data);

        // ...
        //connection->data = "OK\n";
        // ...

        // Now we have response in `connection->data` and it's time to send it.
        //async_write_data(std::move(connection), &authorizer::on_datasend);
    }

    static void on_datasend(connection_ptr&& connection, const boost::system::error_code& error) {
        if (error) {
            std::cerr << "authorizer.on_datasend: error during sending response: " << error << '\n';
            assert(false);
        }

        connection->shutdown();
    }
};

void client_start(){

}
void server_start(){
    //std::cout << "server_start start\n";
    //tp_network::tasks_processor::run_delayed(boost::posix_time::seconds(1), &send_auth);
    tp_network::tasks_processor::add_listener(g_port_num, &authorizer::on_connection_accpet);
    assert(!g_authed);

    tp_network::tasks_processor::start();
    assert(g_authed);
    //std::cout << "server_start end\n";
}
struct Tserver_start{
    const unsigned short g_port_num;
    Tserver_start( const unsigned short g_port_num_):g_port_num(g_port_num_){}
    void operator ()(){
        tp_network::tasks_processor::add_listener(g_port_num, &authorizer::on_connection_accpet);
        assert(!g_authed);
        tp_network::tasks_processor::start();
        parser_bulk("");
        //assert(g_authed);
    }
};


int main_client_server(const unsigned short g_port_num_) {
    std::thread server_task([g_port_num_](){Tserver_start(g_port_num_).operator()();});
    server_task.join(); // Ожидание завершения потока
    return 0;
}

