#include <iostream>
#include <vector>
#include <string>
#include <unordered_map> // 哈希表，用于存储 (word, count)
#include <thread>        // 用于并行的 Map 任务
#include <mutex>         // 用于保护共享的 map_results
#include <sstream>       // 用于字符串流 (分割单词)
#include <algorithm>     // 用于 std::transform (转小写) 和 std::remove_if (去标点)
#include <cctype>        // 用于 ::tolower 和 ::ispunct

using namespace std;
/*
 Map函数：将字符串分割成单词，并统计每个单词的出现次数
 */
unordered_map<string, int> mapFunction(const string& input) {
    unordered_map<string, int> wordCount;
    istringstream iss(input);
    string current_word; //用于积累英文字符

    // 逐字符处理输入字符串
    for(size_t i = 0; i<input.length();){
        // 将char转为unsigned char以避免负值问题，处理utf-8字符
        unsigned char c = static_cast<unsigned char>(input[i]);

        // 如果是英文or标点符号 即ASCII小于128
        if(c < 128){
            if(c<128){
                if(isalpha(c)){
                    // 如果是字母，转成小写再记录到 current_word
                    current_word +=  tolower(c);
                    i++;
                }
                else{
                    // 遇到非字母字符，说明一个单词结束
                    if(!current_word.empty()){
                        wordCount[current_word]++;
                        current_word.clear();
                    }
                    i++;
                }
            }
        }
        // 如果是其他语言，即多字节utf-8，ASCII大于128
        else{
            // 先处理当前积累的英文单词
            if(!current_word.empty()){
                wordCount[current_word]++;
                current_word.clear();
            }
            // 查utf-8编码规则来确定分词字符
            size_t char_len = 0;
            if((c & 0xE0) == 0xC0) char_len = 2;         // 2字节 （俄语、阿拉伯语）
            else if((c & 0xF0) == 0xE0) char_len = 3;    // 3字节 （中文、日文、韩文）
            else if((c & 0xF8) == 0xF0) char_len = 4;    // 4字节 （表情符号、生僻字）
            else {
                // 非法UTF-8起始字节，跳过
                i++;
                continue;
            }
            //确保字符长度不超出字符串边界
            if(i + char_len > input.length()){
                i++;// 非法字符，跳过
                continue;
            }

            //检查全角标点符号范围（中文标点）
            bool is_punctuation = false;
            if(char_len == 3){
                unsigned  char c1 = static_cast<unsigned char>(input[i]);
                unsigned  char c2 = static_cast<unsigned char>(input[i+1]);
                // 查表CJK和全角标点范围：0xE38080 - 0xE3A0BF
                if((c1 == 0xE3 && c2 == 0x80) || (c1 == 0xEF && (c2 == 0xBC || c2 == 0xBD))){
                    is_punctuation = true;
                }
            }
            //跳过标点符号
            if(is_punctuation){
                i += char_len; 
                continue;
            }
            // 提取完整的utf-8字符作为单词
             string utf8_char = input.substr(i, char_len);
            wordCount[utf8_char]++;
            i += char_len;
        }
    }
    // 循环结束检查是否有剩余的英文单词
    if(!current_word.empty()){
        wordCount[current_word]++;
    }
    return wordCount;
}

/*
 Reduce函数：合并多个Map的输出结果
 */
unordered_map<string, int> reduceFunction(const vector<unordered_map<string, int>>& intermediate_maps) {
    unordered_map<string, int> final_result;
    for (const auto& map : intermediate_maps) {
        for (const auto& kv_pair : map) {
            // kv_pair.first 是单词 (key)
            // kv_pair.second 是次数 (value)
            final_result[kv_pair.first] += kv_pair.second;
        }
    }
    return final_result;
}

int main() {
    
    // 1. 准备输入数据 (模拟被分割的数据块/文件)
    // 真实的 MapReduce 框架会从分布式文件系统 (如 HDFS) 读取文件并自动分片。
    // 在这里，我们用一个 vector<string> 来模拟这些"分片"。
    vector<string> input_data = {
        "Hello world, this is MapReduce!",
        "MapReduce 是一个编程模型",
        "This model is used for processing... and generating large data sets.",
        "你好 MapReduce, 你好 C++ world."
        "我是一名研究生，我喜欢学习分布式系统。",
        "分布式系统包括MapReduce和分布式文件系统。"
    };

    cout << "--- MapReduce 平台启动 ---" <<  endl;
    cout << "总共 " << input_data.size() << " 个输入分片。" <<  endl;
    cout << "-----------------------------------" <<  endl;

    // 2. Map 阶段 (并行执行)
    
    // 这个 vector 用来存储所有 Map 线程的中间输出
    vector<unordered_map<string, int>> map_results;
    
    // 这个 vector 用来管理所有的 Map 线程
    vector<thread> map_threads;

    // 互斥锁 (Mutex)，用来保护 'map_results'。
    // 因为多个线程会同时尝试写入它，我们需要一个锁来防止数据竞争。
    mutex mtx; 

    cout << "--- 正在并行执行 Map 阶段... ---" <<  endl;

    for (const auto& chunk : input_data) {
        // emplace_back: 创建一个新线程并立即开始执行
        map_threads.emplace_back(
            // C++ Lambda 表达式，定义了线程要执行的工作
            [&, chunk]() { // 捕获 mtx, map_results 的引用，并拷贝 chunk
                
                // 2.1 每个线程独立执行 MapFunction
                unordered_map<string, int> single_map_result = mapFunction(chunk);

                // 2.2 线程需要安全地将结果写入共享的 map_results
                //  lock_guard 会在创建时自动锁定 mtx，
                // 并在该函数块结束时自动解锁 mtx (即使发生异常)。
                lock_guard< mutex> lock(mtx);
                
                // (--- 锁定的临界区 ---)
                map_results.push_back(single_map_result);
                // (--- 锁自动释放 ---)
            }
        );
    }

    // 2.3 等待所有 Map 线程执行完毕
    // "Join" 操作会阻塞主线程，直到对应的子线程完成工作。
    for (auto& t : map_threads) {
        t.join();
    }

    cout << "--- Map 阶段完成。收集到 " << map_results.size() << " 份中间结果。---" <<endl;
    cout << "-----------------------------------" <<endl;


    // 3. Reduce 阶段 (汇总)
    // MapReduce 框架中的 "Shuffle and Sort" 阶段在这里被简化了。
    // 我们直接将所有 Map 的输出 (一个 vector) 传递给 Reduce 函数。
    cout << "--- 正在执行 Reduce 阶段... ---" <<endl;
    
    unordered_map<string, int> final_result = reduceFunction(map_results);

    cout << "--- Reduce 阶段完成。 ---" <<endl;
    cout << "-----------------------------------" <<endl;


    // 4. 打印最终结果
    cout << "最终单词统计 (WordCount) 结果:" <<endl;
    for (const auto& kv_pair : final_result) {
        cout << "  '" << kv_pair.first << "': " << kv_pair.second <<endl;
    }

    return 0;
}
