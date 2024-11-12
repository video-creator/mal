//
//  mal_string.hpp
//  Demo
//
//  Created by wangyaqiang on 2024/11/11.
//

#ifndef mal_string_hpp
#define mal_string_hpp
#include <unordered_set>
#include <stdio.h>
namespace mal {
bool contains(const std::unordered_set<std::string> &set,
              const std::string &str);
std::unordered_set<std::string> splitToSet(const std::string &str,
                                           const std::string &delimiter);
};
#endif /* mal_string_hpp */
