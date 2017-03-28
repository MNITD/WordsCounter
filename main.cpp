#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <thread>
#include <map>
#include <mutex>

void thread_handler(std::vector<std::string>text, std::map<std::string, unsigned int> & dictionary, std::mutex & dict_mutex, const std::string & symbols);
int get_size(std::string & filename);
std::vector<std::string>  split(std::string & line);
void display(std::string & s);
void reduce_symbols(std::string & line, const std::string & symbols);
void read(std::ifstream  & file, int amount, std::vector<std::string>*text);
void update_dictionary(std::map<std::string, unsigned int> & general_dict, std::map<std::string, unsigned int> & temp_dict, std::mutex & dict_mutex);

void thread_handler(std::vector<std::string>text, std::map<std::string, unsigned int> & dictionary, std::mutex & dict_mutex, const std::string & symbols){
    std::map<std::string, unsigned int> temp_dict;
    std::cout << "from the thread"<< std::endl;

    std::for_each (text.begin(), text.end(), [&symbols](std::string & line){reduce_symbols(line, symbols);});

    std::for_each (text.begin(), text.end(), [&temp_dict](std::string & line){
        std::vector<std::string> words = split(line);
        std::for_each (words.begin(), words.end(), [&temp_dict](std::string & word){
            ++temp_dict[word];
        });
    });
    update_dictionary(dictionary, temp_dict, dict_mutex);
}

int get_size(std::string &filename){
    int size = 0;
    std::string line;
    std::ifstream myfile(filename);
    while (std::getline(myfile, line))
        size++;

    return size;
}

std::vector<std::string> split(std::string & line){
    // std::operator>> separates by spaces
    std::vector<std::string> words;
    std::string word;
    std::istringstream split(line);
    while( split >> word ) {
        words.push_back(word);
    }
    return words;
}

void display(std::string & s){
    std::cout << s << std::endl;
}

void reduce_symbols(std::string & line,const std::string & symbols){
    for(std::string::iterator iter = line.begin(); iter != line.end(); iter++) {
        if ( symbols.find( *iter ) != std::string::npos ) {
            *iter = ' ';
        }
    }
}

void read(std::ifstream  & file, int amount, std::vector<std::string>*text){

    int i = 0;
    std::string line;

    while ( i < amount and std::getline(file, line) ){
        std::cout << "line: " << line << std::endl;
        text->push_back(line);
        i++;
    }
}

void update_dictionary(std::map<std::string, unsigned int> & general_dict, std::map<std::string, unsigned int> & temp_dict, std::mutex & dict_mutex){
    std::lock_guard<std::mutex> lock(dict_mutex);
    std::for_each(temp_dict.begin(), temp_dict.end(), [&general_dict](std::pair<std::string, unsigned int> pair){
        std::cout << pair.first << std::endl;
        general_dict[pair.first] += pair.second;
    });
}

int main() {
    const std::string symbols_g = ",;.:-()\t!¡¿?\"[]{}&<>+-*/=#'";
    const std::string symbols_c = "=\"";
    int lines_num = 0;
    int threads_num = 5;
    int thread_line_num = 0;
    std::string filename("addition/2.txt");
    std::ifstream file(filename);
    std::vector<std::string>text;
    std::vector<std::thread*> threads;
    std::map<std::string, unsigned int> dictionary;
    std::mutex dict_mutex;





    lines_num = get_size(filename);
    std::cout << "Number of lines in text file: " << lines_num << std::endl;

    if (lines_num < threads_num) {
        threads_num = lines_num;
    }

    thread_line_num = lines_num / threads_num;


    for(int i = 0; i < threads_num; i++){
        std::vector<std::string>text_part;
        read(file, thread_line_num, &text_part);
        std::cout << "text size: " << text_part.size() << std::endl;
        threads.push_back(new std::thread (thread_handler, text_part, std::ref(dictionary),  std::ref(dict_mutex), symbols_g));
    }

    std::for_each (threads.begin(), threads.end(), [](std::thread *thread){
        thread->join();
    });



    std::for_each (dictionary.begin(), dictionary.end(), [](std::pair<std::string, unsigned int> pair){
        std::cout << pair.first << " - " << pair.second << std::endl;
    });

    return 0;
}