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
    std::cout << "
