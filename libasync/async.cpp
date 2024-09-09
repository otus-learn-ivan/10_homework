
#include <fstream>
#include<chrono>

#include <iostream>
#include <map>
#include <memory>
//#include <format>
#include <boost/format.hpp>
#include <cstring>

#include <thread>
#include <boost/circular_buffer.hpp>
#include <utility>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <sstream>

namespace libasync_protected {

using namespace std;


  class Tmessage{
    std::unique_ptr<char[]> msg;   
  public:
    const size_t sz_msg;
    const size_t t_start;
    Tmessage(const char* buff, size_t size_buf,size_t t_s):sz_msg(size_buf),t_start(t_s){
      msg = std::make_unique<char[]>(size_buf);
      memcpy(msg.get(), buff, size_buf);
    }
    const std::unique_ptr<char[]>& get_msg(){
      return msg;
    }
  };

  class Twrite_to_file{
  public:
      static const string title_file_name;
      void operator()(size_t id, std::shared_ptr<Tmessage> msg) const{
          ostringstream id_th;
          id_th << std::this_thread::get_id();
          std::string file_name = (boost::format("%1%_%2%_%3%_%4%.log")%title_file_name%id%id_th.str()%msg->t_start).str();
          // cout << "create file " << file_name << endl;
          ofstream file_(file_name);
          if(file_.is_open()){
              file_.write(msg->get_msg().get(),msg->sz_msg);
              file_.close();
          }
      }
  };

  const string Twrite_to_file::title_file_name = "bulk";



  template <class T>
  struct Tthreadsafe_queue {
    std::mutex mutex;
    std::condition_variable_any cv;
    std::queue<T> data_queue;
  };

  template <class T>
  class Tconsumer{
    std::shared_ptr<Tthreadsafe_queue <T>> qu;
  public:
    Tconsumer(std::shared_ptr<Tthreadsafe_queue <T>> q):qu(q){}
    static Tconsumer Creator(std::shared_ptr<Tthreadsafe_queue <T>> q){
      return Tconsumer(q);
    }
    void push(const T& value) {
      std::lock_guard<std::mutex> lock(qu->mutex);
      qu->data_queue.push(value);
      qu->cv.notify_all(); // Сигнализируем о доступном элементе
    }
  };



  class Tproducer{
    const size_t  id;
    std::shared_ptr<Tthreadsafe_queue <std::shared_ptr<Tmessage>>> qu;
    std::function<void(size_t,std::shared_ptr<Tmessage>) > action;
  public:
    Tproducer(const size_t  ID,std::shared_ptr<Tthreadsafe_queue <std::shared_ptr<Tmessage>>>& q, std::function<void(size_t, std::shared_ptr<Tmessage>)> act):
    id(ID), qu(q), action(act){}
    void operator()(std::stop_token stopToken){
        while (1) {
            {
                std::shared_ptr<Tmessage> value =nullptr;
                {
                    std::unique_lock<std::mutex> lock(qu->mutex);
                    qu->cv.wait(lock,stopToken, [this] { return !qu->data_queue.empty(); }); // Ждем, пока очередь не будет пуста
                    if(stopToken.stop_requested()&&qu->data_queue.empty()){
                       return;
                    }
                    value = qu->data_queue.front();
                    qu->data_queue.pop();
                }
                action(id, value);
            }
        }
    }
    ~Tproducer(){
        //std::cout << "~Tproducer() destroy" << std::endl;
    }
  };
  class Treceive {
    std::unique_ptr<size_t> ID_this = nullptr;
    std::shared_ptr<Tthreadsafe_queue <std::shared_ptr<Tmessage>>> log_qu = nullptr;
    std::shared_ptr<Tthreadsafe_queue <std::shared_ptr<Tmessage>>> file_qu = nullptr;
    using Tconsumer_ = Tconsumer<std::shared_ptr<Tmessage>>;
    std::unique_ptr<Tproducer> loger = nullptr;
    std::unique_ptr<Tproducer> file_saver = nullptr;
    std::jthread log;
    std::jthread saver1;
    std::jthread saver2;
  public:
    Treceive(){
//        std::cout << "!!! Treceive() !!!" << std::endl;
    }
    Treceive(size_t ID){
      ID_this = std::make_unique<size_t>(ID);
      log_qu = std::make_shared<Tthreadsafe_queue <std::shared_ptr<Tmessage>>>();
      loger = std::make_unique<Tproducer>(ID, log_qu,[](size_t id, std::shared_ptr<Tmessage> msg ){
              std::string s(msg->get_msg().get(),msg->sz_msg);
              std::cout << "log id: " << id << " " << std::string(msg->get_msg().get(),msg->sz_msg).c_str()<< std::endl;
            });
      log = std::jthread(*loger);

      file_qu = std::make_shared<Tthreadsafe_queue <std::shared_ptr<Tmessage>>>();
      Twrite_to_file action_file_saver;
      file_saver = std::make_unique<Tproducer>(ID, file_qu,[action_file_saver](size_t id,std::shared_ptr<Tmessage> msg){
              action_file_saver.operator ()(id,msg);
      });
      saver1 = std::jthread(*file_saver);
      saver2 = std::jthread(*file_saver);
    }
    ~Treceive(){
        log.request_stop();
        saver1.request_stop();
        saver2.request_stop();
    }
    void receive(const char* buff, size_t size_buf,size_t t_start);
  };

  void Treceive::receive(const char* buff, size_t size_buf,size_t t_start){
    if(ID_this==nullptr){ throw std::out_of_range(boost::format("receiver  not created").str()); }
    {
      std::shared_ptr<Tmessage> msg = std::make_shared<Tmessage>(buff, size_buf,t_start);
      Tconsumer_::Creator(file_qu).push(msg);
      Tconsumer_::Creator(log_qu).push(msg);
    }    
  }

  class Tlibasync {
    std::unordered_map<size_t, std::unique_ptr<Treceive>> map_receives;
  public:
    size_t connect(size_t size_blok);
    void receive(const char* buff, size_t size_buf, size_t ID,size_t t_start);
    void disconnect(size_t ID);
  };

  size_t Tlibasync::connect(size_t ID){
    //std::cout << "!!! connect :" << ID << std::endl;
    if(map_receives.find(ID)== map_receives.end()){
      map_receives[ID] = std::make_unique<Treceive>(ID);
    }
    return ID;
  }

  void Tlibasync::receive(const char* buff, size_t size_buf, size_t ID,size_t t_start){
    if (map_receives.find(ID) == map_receives.end()) {throw std::out_of_range((boost::format("id_connector %1% not created")%ID).str());}
    map_receives[ID]->receive(buff, size_buf,t_start);
  }

  void Tlibasync::disconnect(size_t ID){
    //std::cout << "!!! disconnect :" << ID << std::endl;
    map_receives.erase(ID);
  }

  static std::unique_ptr<Tlibasync> libasync = nullptr;
  std::once_flag flag_create_libasync;
  void create_libasync(){
      std::call_once(flag_create_libasync,[](){
           libasync = std::make_unique<Tlibasync>();
      });
  }
}

namespace libasync {
  size_t connect(size_t size_blok) {
    libasync_protected::create_libasync();
    libasync_protected::libasync->connect(size_blok);
    return size_blok;
  }
  void receive(const char* buff, size_t size_buf, size_t ID,size_t tiks_start){
    libasync_protected::libasync->receive(buff, size_buf, ID,tiks_start);
  }
  void disconnect(size_t ID) {
    libasync_protected::libasync->disconnect(ID);
  }
}
