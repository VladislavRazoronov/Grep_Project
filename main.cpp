#include <iostream>
#include <tbb/concurrent_queue.h>
#include <tbb/concurrent_hash_map.h>
#include <archive.h>
#include <archive_entry.h>
#include <boost/filesystem.hpp>
#include <sstream>
#include <queue>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <condition_variable>

inline std::chrono::steady_clock::time_point get_current_time_fenced()
{
    std::atomic_thread_fence(std::memory_order_seq_cst);
    auto res_time = std::chrono::steady_clock::now();
    std::atomic_thread_fence(std::memory_order_seq_cst);
    return res_time;

}

template<class D>
        inline long long to_ms(const D& d)
        {
    return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
        }

void process_file(tbb::concurrent_bounded_queue<std::string>& file_queue, std::string& phrase,
                  tbb::concurrent_hash_map<std::string,std::string>& files, bool& read_finished){
    while(!file_queue.empty() || !read_finished){
        std::string filepath;
        bool contains = false;
        if(file_queue.try_pop(filepath)){
            struct archive *a;
            struct archive_entry *entry;
            int r;
            std::string text;

            a = archive_read_new();
            archive_read_support_filter_all(a);
            archive_read_support_format_all(a);
            r = archive_read_open_filename(a,filepath.c_str(), 10240);
            if(r!= ARCHIVE_OK){
                continue;
            }
            while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {

                std::string filename = archive_entry_pathname(entry);
                if(filename.substr(filename.size()-3, 3) == "txt" && archive_entry_size(entry) < 10000000){
                    text.append(filepath);text.append("/");text.append(filename);text.append("\t");
                    char buff[1024];
                    while (true){
                        auto t = archive_read_data(a,buff,1024);
                        if (t == 0){
                            break;
                        }
                        text.append(buff);
                    }
                    archive_entry_clear(entry);
                }
            }
            archive_read_close(a);
            std::string filename;
            std::istringstream stream(text);
            std::getline(stream, filename,'\t');
            tbb::concurrent_hash_map<std::string,std::string>::accessor acc;
            int line_num = 1;
            for(std::string line; std::getline(stream,line);){
                if(line.find(phrase) != std::string::npos){
                    if(!contains){
                        files.insert(acc,filename);
                    }
                    acc->second += std::to_string(line_num)+". "+line;
                    contains = true;

                }
                line_num++;
            }
        }
    }
}

void read_files(std::string& path, tbb::concurrent_bounded_queue<std::string>& file_queue, bool& read_finished){
    for(boost::filesystem::recursive_directory_iterator iter(path); iter != boost::filesystem::recursive_directory_iterator(); iter++){
        if(!boost::filesystem::is_directory(iter->path())){
            file_queue.push(iter->path().generic_string());
        }
    }
    read_finished = true;
}

bool compare_by_name(const std::pair<std::string, std::string>& operand1,const std::pair<std::string, std::string>& operand2){
    return operand1.first < operand2.first;
}

std::vector<std::pair<std::string,std::string>> sort_by_name(tbb::concurrent_hash_map<std::string,std::string>& map){
    std::vector<std::pair<std::string, std::string>> output;
    for (auto& it:map){
        output.emplace_back(it);
    }
    std::sort(output.begin(), output.end(), compare_by_name);
    return output;
}

void save_output(std::string& path, tbb::concurrent_hash_map<std::string,std::string>& output){
    std::ofstream file(path);
    auto out = sort_by_name(output);
    for(const auto& items: out){
        file << items.first << "\n" << items.second<<"\n";
    }
    file.close();
}

int main(int argc, char** argv){
    long threads;
    if(argc <4){
        return -1;
    }
    std::string output_file = argv[1];
    std::string archive_path = argv[2];
    std::string phrase = argv[3];
    if(argc >4){
        threads = std::strtol(argv[4],nullptr,10);
    }
    std::vector<std::thread> thread_list;
    tbb::concurrent_queue<std::string> file_queue;
    tbb::concurrent_bounded_queue<std::string> File_queue;
    File_queue.set_capacity(100);
    tbb::concurrent_hash_map<std::string,std::string> output_text;
    bool read_finished = false;
    std::thread reader(read_files,std::ref(archive_path),std::ref(File_queue),std::ref(read_finished));
    for(int i=0;i<threads;i++){
        thread_list.emplace_back(process_file,std::ref(File_queue),std::ref(phrase),std::ref(output_text),std::ref(read_finished));
    }
    if(reader.joinable()){
        reader.join();
    }
    for(int i=0;i<threads;i++){
        if(thread_list[i].joinable())
            thread_list[i].join();
    }
    save_output(output_file, output_text);
}
