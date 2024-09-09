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
        connection->data = "OK\n";
        // ...

        // Now we have response in `connection->data` and it's time to send it.
        async_write_data(std::move(connection), &authorizer::on_datasend);
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
        assert(g_authed);
    }
};


void signal_handler(int signal) {

  std::cout << "Получен сигнал "<< signal <<" Завершение работы..." <<std::endl;

  if (signal == SIGINT) {
    std::cout << "Получен сигнал SIGINT. Завершение работы..." <<std::endl;
    exit(0);
  }
}


int main_client_server(const unsigned short g_port_num_) {



    std::cout << "main_client_server start\n";
//    std::thread server_task(server_start);
    std::thread server_task([g_port_num_](){Tserver_start(g_port_num_).operator()();});
   // std::thread client_task(send_auth);
    server_task.join(); // Ожидание завершения потока
   // client_task.join();
    std::cout << "main_client_server end\n";
    return 0;
}

#if 0
using tasks_processor = detail::tasks_processor;

void  print_time_now(std::string s) {
  time_t current_time = time(nullptr);
  std::tm* time_info = localtime(&current_time);
  char buffer[80];
  strftime(buffer, sizeof(buffer), "%A, %B %d, %Y %H:%M:%S", time_info);
  std::cout << s << " Текущее время: " << buffer << std::endl;
}
struct test_functor {
    int& i_;
    explicit test_functor(int& i):i_(i){};
    void operator()() const {
        i_ = 1;
        print_time_now("test_functor operator");
        tasks_processor::stop();
    }
};
void test_func1() {
    print_time_now("test_func1");
    throw std::logic_error("It works!");
}

void main_timer(){
    const int seconds_to_wait = 5;
    int i = 0;
    //test_functor t(i);
    tasks_processor::run_delayed(
                boost::posix_time::seconds(seconds_to_wait),
                test_functor(i)
                );
    tasks_processor::run_delayed(
                 boost::posix_time::from_time_t(time(NULL) + 2),
                 &test_func1
                 );
    assert(i == 0);
    // Блокирует, пока одна из задач не вызовет tasks_processor::stop().
    print_time_now("START");
    tasks_processor::start();
}
#endif

#if 0
int func_test() {
    static int counter = 0;
    ++ counter;
    //std::cout << "func_test " << counter  <<" id "<< boost::this_thread::get_id() << std::endl;
    boost::this_thread::interruption_point();
    switch (counter) {
    case 3:
        throw std::logic_error("Just checking");
    case 10:
        // Эмуляция прерывания потока.
        // Перехват внутри task_wrapped. Выполнение следующих задач продолжится.
        throw boost::thread_interrupted();
    case 90:
        // Остановка task_processor.
        tasks_processor::stop();
    }
    return counter;
}

void main_timer() {

    // for (std::size_t i = 0; i < 100; ++i) {
    //     tasks_processor::push_task(&func_test);
    // }
    std::cout << "test 1 "  << std::endl;
    // Обработка не была начата.
    assert(func_test() == 1);
    // Мы также можем использовать лямбда-выражение в качестве задачи.
    // Асинхронно считаем 2 + 2.
    std::cout << "test 2 "  << std::endl;
    int sum = 0;
    tasks_processor::push_task(
                [&sum]() {
        std::cout << "push_task 1 " << sum << std::endl;
        sum = 2 + 2;
        std::cout << "push_task 2 " << sum << std::endl;
    }
    );
    // Обработка не была начата.
    std::cout << "test 3 "  << std::endl;
    assert(sum == 0);
    // Не выбрасывает исключение, но блокирует текущий поток выполнения до тех пор,
    // пока одна из задач не вызовет tasks_processor::stop().
    std::cout << "sum 0 " << sum << std::endl;
    //asio::signal_set signals{ioContext, SIGINT, SIGTERM};
    tasks_processor::start();
    std::cout << "sum 1 " << sum << std::endl;
    // assert(func_test() == 91);
    std::cout << "sum 2 " << sum << std::endl;
    tasks_processor::stop();
    std::cout << "sum 3 " << sum << std::endl;

}
#endif
