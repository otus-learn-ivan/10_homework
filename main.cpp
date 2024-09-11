#include <fstream>
#include <iostream>
#include <memory>
#include <chrono>
#include <vector>
#include "async.h"
#include <boost/asio.hpp>
#include <sstream>

using namespace std;
//--------------------------------------------------------------------------
class Tlogger{
public:
    virtual void print(size_t tiks_start,string b)=0;
};
//--------------------------------------------------------------------------
class Tlogger_to_file: public Tlogger{
    static const string title_file_name;
    void print(size_t tiks_start,string b){
        string file_name = title_file_name+ to_string(tiks_start)+".log";
        ofstream file_(file_name);
        if(file_.is_open()){
            file_ << b;
            file_.close();
        }
    }
};
//--------------------------------------------------------------------------
const string Tlogger_to_file::title_file_name = "bulk";
//--------------------------------------------------------------------------
class Tlogger_to_queue: public Tlogger{
    size_t ID;
public:
    Tlogger_to_queue(size_t id):ID(id){
       libasync::connect(id);
    }
    ~Tlogger_to_queue(){
         libasync::disconnect(ID);
    }
    void print(size_t tiks_start,string b) override{
        libasync::receive(b.c_str(), b.size(),ID,tiks_start);
    }
};
//--------------------------------------------------------------------------
template<class Tlog = Tlogger>
class Tpoket_{
    static const string title;
    vector<string> bulk;
    size_t tiks_start;
    bool brace;
    public:
    Tpoket_():bulk(){}
    bool empty_poket()const{
        return bulk.empty();
    }
    void add_cmd(string cmd){
        if(empty_poket()){
            auto now = chrono::system_clock::now();
            tiks_start = chrono::duration_cast<chrono::system_clock::duration>(now.time_since_epoch()).count();
        }
        if(cmd.empty())return;
        bulk.push_back(cmd);
    }
    string bulk_to_string()const {
        string out = title;
        if(bulk.empty()) return out;
        size_t i = 0;
        for(;i < bulk.size()-1;i++){out+=bulk[i]+",";}
        out +=bulk[i];
        return out;
    }
    const Tpoket_& print_poket() const{
        if(empty_poket())return *this;
        cout << bulk_to_string() << "\n";
        return *this;
    }
    const Tpoket_& log_poket(Tlog& logger) const {
        if(empty_poket())return *this;
        logger.print(tiks_start,bulk_to_string());
        return *this;
    }
};
template<class Tlog>
const string Tpoket_<Tlog>::title = "bulk: ";
using Tpoket = Tpoket_<Tlogger>;
//--------------------------------------------------------------------------
class Tparser_cmd{
    enum{OPEN_BRACE=1,CLOSE_BRACE=0};
    static const string brace_l;
    static const string brace_r;
    static const string delimetr;
    int N;
    unique_ptr<Tpoket> current_poket;
    public:
    Tparser_cmd(int N):N(N){
        current_poket = make_unique<Tpoket>();
    }
    unique_ptr<Tpoket> operator()(string&& cmd);
    unique_ptr<Tpoket> return_out_poket(int& N_,string& cmd);
};
const string Tparser_cmd::brace_l = "{";
const string Tparser_cmd::brace_r = "}";
const string Tparser_cmd::delimetr = ",";//",";
unique_ptr<Tpoket> Tparser_cmd::return_out_poket(int& N_,string& cmd){
    N_ = N;
    current_poket->add_cmd(cmd);
    unique_ptr<Tpoket> out = move(current_poket);
    current_poket = make_unique<Tpoket>();
    return out ;
}
unique_ptr<Tpoket> Tparser_cmd::operator()(string&& cmd){
        static int N_ = N;
        static int counter_brace = 0;

        if(cmd==""){return return_out_poket(N_,cmd);}


        if(cmd==brace_l){
            counter_brace++;
            if(counter_brace==OPEN_BRACE){return return_out_poket(N_,cmd="");}
            return make_unique<Tpoket>();
        }
        if(cmd==brace_r){
            counter_brace--;
            if(counter_brace==CLOSE_BRACE){return return_out_poket(N_,cmd="");}
            return make_unique<Tpoket>();
        }
        if(counter_brace==0){ N_--;}
        if(!N_){return return_out_poket(N_,cmd);}
        current_poket->add_cmd(cmd);
        return make_unique<Tpoket>();
}
//--------------------------------------------------------------------------

unique_ptr <Tparser_cmd> parser_cmd;
unique_ptr <Tlogger_to_queue> logger_to_queue;

void parser_bulk(std::string cmd){
    if(cmd.size()==0)(*parser_cmd)("")->log_poket(*logger_to_queue);
    istringstream iss(cmd);
    while(iss){
        string s;
        iss >> s;
        if(iss){
            (*parser_cmd)(std::move(s))->log_poket(*logger_to_queue);
        }
    }
}

void signal_handler(int signal) {
  if (signal == SIGINT) {
//    (*parser_cmd)("")->log_poket(*logger_to_queue);
    std::cout << "Получен сигнал SIGINT. Завершение работы..." <<std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    exit(0);
  }
}
int main_client_server(const unsigned short g_port_num_);
int main(int argc, char* argv[])
{
#if 1
    std::signal(SIGINT, signal_handler);
    if(argc == 1){return 0;}
    parser_cmd =  make_unique<Tparser_cmd>( atoi(argv[2]));
    logger_to_queue = make_unique<Tlogger_to_queue>( atoi(argv[2]));
    main_client_server(atoi(argv[1]));
#endif
    return 0;
}
