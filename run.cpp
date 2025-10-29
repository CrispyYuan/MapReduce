#include <iostream>
#include <vector>
#include <string>
#include <unordered_map> // 哈希表，用于存储 (word, count)
#include <thread>        // 用于并行的 Map 任务
#include <mutex>         // 用于保护共享的 map_results
#include <sstream>       // 用于字符串流 (分割单词)
#include <algorithm>     // 用于 std::transform (转小写) 和 std::remove_if (去标点)
#include <cctype>        // 用于 ::tolower 和 ::ispunct

/**
 * @brief Map函数：将字符串分割成单词，并统计每个单词的出现次数
 * (来自你的 mapreduce.h 文件，并稍作增强以处理标点符号)
 */
std::unordered_map<std::string, int> mapFunction(const std::string& input) {
    std::unordered_map<std::string, int> wordCount;
    std::istringstream iss(input);
    std::string word;
    
    while (iss >> word) {
        // 1. 移除所有标点符号
        word.erase(std::remove_if(word.begin(), word.end(), 
            [](unsigned char c) { return std::ispunct(c); }), word.end());

        // 2. 转换为小写
        std::transform(word.begin(), word.end(), word.begin(),
            [](unsigned char c) { return std::tolower(c); });

        // 3. 统计 (忽略空字符串)
        if (!word.empty()) {
            wordCount[word]++;
        }
    }
    return wordCount;
}

/**
 * @brief Reduce函数：合并多个Map的输出结果
 * (来自你的 mapreduce.h 文件)
 */
std::unordered_map<std::string, int> reduceFunction(
    const std::vector<std::unordered_map<std::string, int>>& intermediate_maps
) {
    std::unordered_map<std::string, int> final_result;
    for (const auto& map : intermediate_maps) {
        for (const auto& kv_pair : map) {
            // kv_pair.first 是单词 (key)
            // kv_pair.second 是次数 (value)
            final_result[kv_pair.first] += kv_pair.second;
        }
    }
    return final_result;
}

// -----------------------------------------------------------------
//                   MapReduce 驱动程序 (主平台)
// -----------------------------------------------------------------
int main() {
    
    // 1. 准备输入数据 (模拟被分割的数据块/文件)
    // 真实的 MapReduce 框架会从分布式文件系统 (如 HDFS) 读取文件并自动分片。
    // 在这里，我们用一个 vector<string> 来模拟这些"分片"。
    std::vector<std::string> input_data = {
        "Hello world, this is MapReduce!",
        "MapReduce is a simple programming model.",
        "This model is used for processing... and generating large data sets.",
        "Hello MapReduce, hello C++ world."
    };

    std::cout << "--- MapReduce 平台启动 ---" << std::endl;
    std::cout << "总共 " << input_data.size() << " 个输入分片。" << std::endl;
    std::cout << "-----------------------------------" << std::endl;

    // 2. Map 阶段 (并行执行)
    
    // 这个 vector 用来存储所有 Map 线程的中间输出
    std::vector<std::unordered_map<std::string, int>> map_results;
    
    // 这个 vector 用来管理所有的 Map 线程
    std::vector<std::thread> map_threads;

    // 互斥锁 (Mutex)，用来保护 'map_results'。
    // 因为多个线程会同时尝试写入它，我们需要一个锁来防止数据竞争。
    std::mutex mtx; 

    std::cout << "--- 正在并行执行 Map 阶段... ---" << std::endl;

    for (const auto& chunk : input_data) {
        // emplace_back: 创建一个新线程并立即开始执行
        map_threads.emplace_back(
            // C++ Lambda 表达式，定义了线程要执行的工作
            [&, chunk]() { // 捕获 mtx, map_results 的引用，并拷贝 chunk
                
                // 2.1 每个线程独立执行 MapFunction
                std::unordered_map<std::string, int> single_map_result = mapFunction(chunk);

                // 2.2 线程需要安全地将结果写入共享的 map_results
                // std::lock_guard 会在创建时自动锁定 mtx，
                // 并在该函数块结束时自动解锁 mtx (即使发生异常)。
                std::lock_guard<std::mutex> lock(mtx);
                
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

    std::cout << "--- Map 阶段完成。收集到 " << map_results.size() << " 份中间结果。---" << std::endl;
    std::cout << "-----------------------------------" << std::endl;


    // 3. Reduce 阶段 (汇总)
    // MapReduce 框架中的 "Shuffle and Sort" 阶段在这里被简化了。
    // 我们直接将所有 Map 的输出 (一个 vector) 传递给 Reduce 函数。
    std::cout << "--- 正在执行 Reduce 阶段... ---" << std::endl;
    
    std::unordered_map<std::string, int> final_result = reduceFunction(map_results);

    std::cout << "--- Reduce 阶段完成。 ---" << std::endl;
    std::cout << "-----------------------------------" << std::endl;


    // 4. 打印最终结果
    std::cout << "最终单词统计 (WordCount) 结果:" << std::endl;
    for (const auto& kv_pair : final_result) {
        std::cout << "  '" << kv_pair.first << "': " << kv_pair.second << std::endl;
    }

    return 0;
}
