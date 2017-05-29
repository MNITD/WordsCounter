#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <thread>
#include <map>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>


template<typename T>
struct QueueWrapper {
    std::atomic<bool> done{false};
//    std::atomic<bool> notified{false};
//    int number{0};
    std::queue<T> queue;
    std::mutex queue_mutex;
    std::condition_variable cond_var;

    QueueWrapper() {
        done = false;
//        notified = false;
//        number = 0;
    }
};

std::vector<std::string> split(std::string &line) {
    // std::operator>> separates by spaces
    std::vector<std::string> words;
    std::string word;
    std::istringstream split(line);

    while (split >> word) {
        words.push_back(word);
    }

    return words;
}

void reduce_symbols(std::string &line, const std::string &symbols) {

    for (std::string::iterator iter = line.begin(); iter != line.end(); iter++) {
        if (symbols.find(*iter) != std::string::npos) {
            *iter = ' ';
        }
    }
}

void read(std::ifstream &file, int amount, std::vector<std::string> *text, bool &end) {

    int i = 0;
    std::string line;

    while (i < amount && std::getline(file, line)) {
        text->push_back(line);
        ++i;
    }
    if (i == 0) end = true;
}

inline std::chrono::high_resolution_clock::time_point get_current_time_fenced() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    auto res_time = std::chrono::high_resolution_clock::now();
    std::atomic_thread_fence(std::memory_order_seq_cst);
    return res_time;
}

template<class D>
inline long long to_us(const D &d) {
    return std::chrono::duration_cast<std::chrono::microseconds>(d).count();
}

/*
 * Reducer
 */

void update_dictionary(QueueWrapper<std::map<std::string, unsigned int>> &dict_qw,
                       std::map<std::string, unsigned int> &general_dict) {

    std::unique_lock<std::mutex> dict_lock(dict_qw.queue_mutex);

    while (!(dict_qw.done && dict_qw.queue.empty())) {
        if (!dict_qw.queue.empty()) {

            std::map<std::string, unsigned int> temp_dict;

            temp_dict = dict_qw.queue.front();
            dict_qw.queue.pop();
//            dict_qw.number++;
            dict_lock.unlock();

            std::for_each(temp_dict.begin(), temp_dict.end(),
                          [&general_dict](std::pair<std::string, unsigned int> pair) {
                              general_dict[pair.first] += pair.second;
                          });

            dict_lock.lock();
        } else {
            dict_qw.cond_var.wait(dict_lock);
        }
    }
}

/*
 * Consumer
 */

void process_block(QueueWrapper<std::vector<std::string>> &block_qw,
                   QueueWrapper<std::map<std::string, unsigned int>> &dict_qw,
                   const std::string &symbols) {

    std::unique_lock<std::mutex> block_lock(block_qw.queue_mutex);

    while (!(block_qw.done && block_qw.queue.empty())) {
        if (!block_qw.queue.empty()) {

            if (block_qw.done) dict_qw.done = true;

            std::map<std::string, unsigned int> temp_dict;
            std::vector<std::string> text;

            text = block_qw.queue.front();
            block_qw.queue.pop();
//            block_qw.number++;

            block_lock.unlock();


            std::for_each(text.begin(), text.end(), [&symbols](std::string &line) { reduce_symbols(line, symbols); });

            std::for_each(text.begin(), text.end(), [&temp_dict](std::string &line) {
                std::vector<std::string> words = split(line);
                std::for_each(words.begin(), words.end(), [&temp_dict](std::string &word) {
                    ++temp_dict[word];
                });
            });

            std::unique_lock<std::mutex> dict_lock(dict_qw.queue_mutex);
            dict_qw.queue.push(temp_dict);
            dict_lock.unlock();

            dict_qw.cond_var.notify_one();

            block_lock.lock();
        } else {
            block_qw.cond_var.wait(block_lock);
        }
    }
}

/*
 * Producer
 */

void read_block(const std::string &path, QueueWrapper<std::vector<std::string>> &block_qw, int block_size,
                std::chrono::high_resolution_clock::time_point &read_finish_time) {
    bool end_flag = false;
    std::ifstream file_data(path);
    while (!end_flag) {
        std::vector<std::string> block;
        read(file_data, block_size, &block, end_flag);
        std::unique_lock<std::mutex> block_lock(block_qw.queue_mutex);
        block_qw.queue.push(block);
        block_lock.unlock();
        block_qw.cond_var.notify_one();
    }
    block_qw.done = true;
    block_qw.cond_var.notify_all();
    file_data.close();
    read_finish_time = get_current_time_fenced();
}


int main() {

    const std::string symbols_g = ",;.:-()\t!¡¿?\"[]{}&<>+-*/=#'";
    const std::string symbols_c = "=\"";
    const std::string data_name("infile");
    const std::string output_a_name("out_by_a");
    const std::string output_n_name("out_by_n");
    const std::string threads_name("threads");
    const std::string block_name("block");
    const std::string config_name("../addition/config.txt");
    bool end_flag = false;
    int threads_num = 0;
    int block_size = 0;
    std::ifstream file_data;
    std::ifstream file_config(config_name);
    std::ofstream file_output_a;
    std::ofstream file_output_n;
    std::vector<std::string> text;
    std::vector<std::string> config_text;
    std::vector<std::thread *> threads;
    std::vector<std::pair<unsigned int, std::string>> items;
    std::map<std::string, unsigned int> dictionary;
    std::map<std::string, std::string> config_dict;
    std::chrono::high_resolution_clock::time_point read_start_time;
    std::chrono::high_resolution_clock::time_point read_finish_time;
    std::chrono::high_resolution_clock::time_point count_start_time;
    std::chrono::high_resolution_clock::time_point count_finish_time;
    std::chrono::high_resolution_clock::time_point write_finish_time;
    QueueWrapper<std::vector<std::string>> block_qw;
    QueueWrapper<std::map<std::string, unsigned int>> dict_qw;
    /*
     * Read config file and create config dictionary
     */

    read(file_config, 5, &config_text, end_flag);
    std::for_each(config_text.begin(), config_text.end(), [&symbols_c](std::string &line) {
        reduce_symbols(line, symbols_c);
    });
    std::for_each(config_text.begin(), config_text.end(), [&config_dict](std::string &line) {
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
    block_size = std::stoi(config_dict[block_name]);

    /*
     * Start counting
     */

    count_start_time = get_current_time_fenced();

    threads.push_back(new std::thread(read_block, std::ref(config_dict[data_name]), std::ref(block_qw), block_size,
                                      std::ref(read_finish_time)));
    threads.push_back(new std::thread(update_dictionary, std::ref(dict_qw), std::ref(dictionary)));
    for (int i = 0; i < threads_num - 2; i++) {
        threads.push_back(new std::thread(process_block, std::ref(block_qw), std::ref(dict_qw), symbols_g));
    }


    read_finish_time = get_current_time_fenced();

    /*
     * Finish of reading (above)
     */

    std::for_each(threads.begin(), threads.end(), [](std::thread *thread) {
        thread->join();
    });

//    std::cout << "Block number: " << block_qw.number << std::endl;
//    std::cout << "Dict number: " << dict_qw.number << std::endl;
    count_finish_time = get_current_time_fenced();

    /*
     * Finish of counting (above)
     */

    /*
     * Start of writing
     */
    file_output_a.open(config_dict[output_a_name]);

    std::for_each(dictionary.begin(), dictionary.end(), [&](std::pair<std::string, unsigned int> pair) {
        file_output_a << pair.first << " - " << pair.second << std::endl;
        items.push_back(std::make_pair(pair.second, pair.first));
    });

    file_output_a.close();


    std::sort(items.begin(), items.end(), std::greater<std::pair<unsigned int, std::string>>());

    file_output_n.open(config_dict[output_n_name]);

    std::for_each(items.begin(), items.end(), [&file_output_n](std::pair<unsigned int, std::string> pair) {
        file_output_n << pair.second << " - " << pair.first << std::endl;
    });

    file_output_n.close();

    write_finish_time = get_current_time_fenced();

    /*
     * Finish of writing (above)
     */

    std::cout << "Read: " << to_us(read_finish_time - read_start_time) << std::endl;
    std::cout << "Analyzing: " << to_us(count_finish_time - count_start_time) << std::endl;
    std::cout << "Total: " << to_us(write_finish_time - read_start_time) << std::endl;

    return 0;
}