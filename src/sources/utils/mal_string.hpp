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

void mdp_str_case_no_copy( char* s, int64_t size,int upper);

int mdp_strconvet_to_int(const char *c1, int size,int big);
unsigned int mdp_strconvet_to_uint(char *c1, int size,int big);
int mdp_strconvet_to_int64(char *c1, int size,int big);
int mdp_strconvet_to_fixed_16x16_point(char *c1);

char* mdp_intconvert_to_str(int64_t val);
std::string mal_buffer_to_hexstr(uint8_t *buffer, int size);
int64_t mdp_bits_read(char *data, int64_t offset, int bits);

unsigned int mdp_char_reverse(unsigned char v);
int mdp_get_bit_from_byte(int v, int bit);
std::string mal_int_to_hex_string(int value);


};
#endif /* mal_string_hpp */
