#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <sstream>
#include <algorithm>

// Map函数：将字符串分割成单词，并统计每个单词的出现次数
std::unordered_map<std::string, int> mapFunction(const std::string& input) {
    std::unordered_map<std::string, int> wordCount;
    std::istringstream iss(input);
    std::string word;
    while (iss >> word) {
        std::transform(word.begin(), word.end(), word.begin(), ::tolower); // 转换为小写
        wordCount[word]++;
    }
    return wordCount;
}

// Reduce函数：合并多个Map的输出结果
std::unordered_map<std::string, int> reduceFunction(const std::vector<std::unordered_map<std::string, int>>& maps) {
    std::unordered_map<std::string, int> result;
    for (const auto& map : maps) {
        for (const auto& kv : map) {
            result[kv.first] += kv.second;
        }
    }
    return result;
}
