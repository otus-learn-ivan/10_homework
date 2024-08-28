#define BOOST_TEST_MODULE test_bulk

#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <iterator>
#include <iomanip>

#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <string>

#include <sys/types.h>
#include <dirent.h>
#include <thread>
#include <chrono>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(test_bulk)


BOOST_AUTO_TEST_CASE(test_test)
{
  BOOST_CHECK(true == true);
}

std::vector<std::string> created_files_log(){
DIR *dir;
   struct dirent *ent;
   dir = opendir("."); // "." - текущая директория
   std::vector<std::string> log_files;

   if (dir != nullptr) {
       while ((ent = readdir(dir)) != nullptr) {
           if (ent->d_type == DT_REG && std::string(ent->d_name).find(".log") != std::string::npos) {
               log_files.push_back(ent->d_name); // Добавляем имя файла в вектор
           }
       }
       closedir(dir);
   } else {
       // Обработка ошибки
       std::cerr << "Ошибка открытия директории\n";
   }
   std::sort(log_files.begin(),log_files.end());
   return log_files;
}
std::vector<std::string>  set_suit_cmd( std::vector<std::string>& commands ){
    // Запускаем приложение bulk через popen
    FILE* pipe = popen("./bulk 3", "w");
    if (pipe == nullptr) {
        std::cerr << "Ошибка запуска процесса bulk\n";
        return std::vector<std::string>{};
    }
    // Отправляем команды с задержкой
    for (const auto& cmd : commands) {
        fprintf(pipe, "%s\n", cmd.c_str());
        std::cout << cmd << std::endl;
        // Читаем и выводим результат из pipe
         char buffer[1024];
         while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
             std::cout << "Получено: " << buffer << std::endl;
         }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));; // задержка в 1 секунду
    }
    // Закрываем поток для записи
    pclose(pipe);
    return created_files_log();
}

void print_file(std::string name_file){
    std::cout << name_file<<": ";
    std::ifstream file(name_file);
    if (!file.is_open()) {
        std::cerr << "Ошибка открытия файла: " << name_file << std::endl;
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::cout << line << std::endl;
    }
    file.close();
}

BOOST_AUTO_TEST_CASE(test_valid)
{
  // Формируем команды
  std::vector<std::string> suit_commands1 = {"cmd1", "cmd2", "cmd3","cmd4","cmd5"};
  set_suit_cmd(suit_commands1);
  std::vector<std::string> suit_commands2 = {"cmd1", "cmd2","{", "cmd3","cmd4","}","{","cmd5","cmd6","{","cmd7","cmd8","}","cmd9","}","{","cmd10","cmd11"};
  for (auto& files : set_suit_cmd(suit_commands2)){
      print_file(files);
  }
  system("rm *.log");
}

BOOST_AUTO_TEST_SUITE_END()
