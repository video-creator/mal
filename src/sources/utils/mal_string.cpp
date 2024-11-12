//
//  mal_string.cpp
//  Demo
//
//  Created by wangyaqiang on 2024/11/11.
//

#include "mal_string.hpp"

namespace mal {
std::unordered_set<std::string> splitToSet(const std::string &str,
                                           const std::string &delimiter) {
  std::unordered_set<std::string> result;
  size_t start = 0, end = 0;

  while ((end = str.find(delimiter, start)) != std::string::npos) {
    // 提取子字符串
    result.insert(str.substr(start, end - start));
    start = end + delimiter.length(); // 更新起始位置
  }
  // 提取最后一个子字符串
  result.insert(str.substr(start));

  return result;
}
bool contains(const std::unordered_set<std::string> &set,
              const std::string &str) {
  return set.find(str) != set.end();
}
}

