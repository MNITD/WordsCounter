#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <thread>
#include <map>
#include <mutex>
#include <atomic>

void thread_handler(std::vector<std::string>text, std::map<std::string, unsigned int> & dictionary, std::mutex & dict_mutex, const std::string & symbols);
int get_size(std::string & filename);
std::vector<std::string>  split(std::string & line);
void reduce_symbols(std::string & line, const std::string & symbols);
void read(std::ifstream  & file, int amount, std::vector<std::string>*text);
void update_dictionary(std::map<std::string, unsigned int> & general_dict, std::map<std::string, unsigned int> & temp_dict, std::mutex & dict_mutex);

void thread_handler(std::vector<std::string>text, std::map<std::string, unsigned int> & dictionary, std::mutex & dict_mutex, const std::string & symbols){

    std::map<std::string, unsigned int> temp_dict;

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
    std::ifstream file(filename);

    while (std::getline(file, line))
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
        text->push_back(line);
        i++;
    }
}

void update_dictionary(std::map<std::string, unsigned int> & general_dict, std::map<std::string, unsigned int> & temp_dict, std::mutex & dict_mutex){

    std::lock_guard<std::mutex> lock(dict_mutex);
    std::for_each(temp_dict.begin(), temp_dict.end(), [&general_dict](std::pair<std::string, unsigned int> pair){
        general_dict[pair.first] += pair.second;
    });
}

inline std::chrono::high_resolution_clock::time_point get_current_time_fenced() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    auto res_time = std::chrono::high_resolution_clock::now();
    std::atomic_thread_fence(std::memory_order_seq_cst);
    return res_time;
}

template<class D>
inline long long to_us(const D& d) {
    return std::chrono::duration_cast<std::chrono::microseconds>(d).count();
}

int main() {

    const std::string symbols_g = ",;.:-()\t!¡¿?\"[]{}&<>+-*/=#'";
    const std::string symbols_c = "=\"";
    const std::string data_name("infile");
    const std::string output_a_name("out_by_a");
    const std::string output_n_name("out_by_n");
    const std::string threads_name("threads");
    const std::string config_name("../addition/config.txt");
    int lines_num = 0;
    int threads_num = 0;
    int thread_line_num = 0;
    std::ifstream file_data;
    std::ifstream file_config(config_name);
    std::ofstream file_output_a;
    std::ofstream file_output_n;
    std::vector<std::string>text;
    std::vector<std::string>config_text;
    std::vector<std::thread*> threads;
    std::vector<std::pair<unsigned  int, std::string>> items;
    std::map<std::string, unsigned int> dictionary;
    std::map<std::string, std::string> config_dict;
    std::mutex dict_mutex;
    std::chrono::high_resolution_clock::time_point read_start_time;
    std::chrono::high_resolution_clock::time_point read_finish_time;
    std::chrono::high_resolution_clock::time_point count_start_time;
    std::chrono::high_resolution_clock::time_point count_finish_time;
    std::chrono::high_resolution_clock::time_point write_finish_time;

    /*
     * Read config file and create config dictionary
     */

    read(file_config, 5, &config_text);
    std::for_each (config_text.begin(), config_text.end(), [&symbols_c](std::string & line){
        reduce_symbols(line, symbols_c);
    });
    std::for_each (config_text.begin(), config_text.end(), [&config_dict](std::string & line){
        std::vector<std::string> words = split(line);
        config_dict[words.at(0)] = words.at(1);
    });

    /*
     * Start of reading
     * first phase: count number of lines in text
     * second phase: read part of text and start thread
     */

    read_start_time = get_current_time_fenced();

    threads_num = std::stoi(config_dict[threads_name]);
    lines_num = get_size(config_dict[data_name]);

    if (lines_num < threads_num) {
        threads_num = lines_num;
    }

    thread_line_num = lines_num / threads_num;

    file_data.open(config_dict[data_name]);

    for(int i = 0; i < threads_num; i++){
        std::vector<std::string>text_part;
        read(file_data, thread_line_num, &text_part);

        /*
         * Start counting with a first thread
         */

        if(i == 0) count_start_time = get_current_time_fenced();

        threads.push_back(new std::thread (thread_handler, text_part, std::ref(dictionary),  std::ref(dict_mutex), symbols_g));
    }

    file_data.close();

    read_finish_time = get_current_time_fenced();

    /*
     * Finish of reading (above)
     */

    std::for_each (threads.begin(), threads.end(), [](std::thread *thread){
        thread->join();
    });

    count_finish_time = get_current_time_fenced();

    /*
     * Finish of counting (above)
     */

    /*
     * Start of writing
     */
    file_output_a.open(config_dict[output_a_name]);

    std::for_each (dictionary.begin(), dictionary.end(), [&](std::pair<std::string, unsigned int> pair){
        file_output_a << pair.first << " - " << pair.second << std::endl;
        items.push_back(std::make_pair(pair.second, pair.first));
    });

    file_output_a.close();


    std::sort(items.begin(), items.end(), std::greater<std::pair<unsigned int, std::string>>());

    file_output_n.open(config_dict[output_n_name]);

    std::for_each (items.begin(), items.end(), [&file_output_n](std::pair<unsigned int, std::string> pair){
        file_output_n << pair.second << " - " << pair.first << std::endl;
    });

    file_output_n.close();

    write_finish_time = get_current_time_fenced();

    /*
     * Finish of writing (above)
     */

    std::cout << "Read: "<< to_us(read_finish_time - read_start_time) << std::endl;
    std::cout << "Analyzing: "<< to_us(count_finish_time - count_start_time) << std::endl;
    std::cout << "Total: "<< to_us(write_finish_time - read_start_time) << std::endl;

    return 0;
}