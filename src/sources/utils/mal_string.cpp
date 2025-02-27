//
//  mal_string.cpp
//  Demo
//
//  Created by wangyaqiang on 2024/11/11.
//

#include "mal_string.hpp"
#include <sstream>
#include <iomanip>
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

void mdp_str_case_no_copy( char* s, int64_t size,int upper) {
    for (size_t i = 0; i < size; i++)
    {
        s[i] = upper ? toupper(s[i]) : tolower(s[i]);
    }
}

int mdp_strconvet_to_int(const char *c1, int size,int big) {
    unsigned char *c = (unsigned char *)c1;
    if(size <= 0 || !c) return 0;
    int ret = 0;
    if(big) {

        for(int i = 0; i < size;i++) {
//                qDebug() << (int)c[i];
            ret += ((int)c[i]) << (size -1 - i) * 8;
        }
        return ret;
    } else {
        for(int i = 0; i < size;i++) {
            ret += c[i] << i * 8;
        }
        return ret;
    }
}
std::string mal_buffer_to_hexstr(uint8_t *buffer, int size) {
    std::string ret = "0x";
    for (int i = 0; i < size; i++) {
        ret += mal_int_to_hex_string(buffer[i]);
        if (i != (size - 1)) {
            ret += " ";
        }
    }
    return ret;
}

unsigned int mdp_strconvet_to_uint(char *c1, int size,int big) {
    unsigned char *c = (unsigned char *)c1;
    if(size <= 0 || !c) return 0;
    int ret = 0;
    if(big) {

        for(int i = 0; i < size;i++) {
//                qDebug() << (int)c[i];
            ret += ((unsigned int)c[i]) << ((size -1 - i) * 8);
        }
        return ret;
    } else {
        for(int i = 0; i < size;i++) {
            ret += ((unsigned int)c[i]) << i * 8;
        }
        return ret;
    }
}
int mdp_strconvet_to_int64(char *c1, int size,int big) {
    unsigned char *c = (unsigned char *)c1;
    if(size <= 0 || !c) return 0;
    int64_t ret = 0;
    if(big) {

        for(int i = 0; i < size;i++) {
//                qDebug() << (int)c[i];
            ret += ((int)c[i]) << (size -1 - i) * 8;
        }
        return ret;
    } else {
        for(int i = 0; i < size;i++) {
            ret += c[i] << i * 8;
        }
        return ret;
    }
}
int mdp_strconvet_to_fixed_16x16_point(char *data) {
    int int_part = mdp_strconvet_to_int(data,2,1);
    double float_part = 0;
    unsigned char*float_part_ptr = (unsigned char *)(data + 2);
    for(int i = 0; i < 16;i++) {
        int val = mdp_bits_read((char *)float_part_ptr,i,1);
        float_part += pow(2,-(i+1)) * val;
    }
    return int_part + float_part;
}

char* mdp_intconvert_to_str(int64_t val) {
    static char res[100] = {'\0'};
    snprintf(res,90,"%lld",val);
    return res;
}


int64_t mdp_bits_read(char *data, int64_t offset, int bits) {
   int64_t result = 0;
   int bytesOffset = offset/8;
   data = data + bytesOffset;
   int right_move = 0;
   offset = offset - bytesOffset * 8;
   int result_bytes_size = bits % 8 == 0 ? bits/8 : bits/8 + 1;
   int index = 0;
   while (bits > 0) {
       int value = data[0];
       if(offset > 0) {
           int head = 255 >> offset;
           value = value & head;
       }
       int right = 8 - offset - bits;
       if(right >= 0) {
           right_move = right;
       }
       index++;
       result += value << (result_bytes_size - index);
       bits = bits - (8-offset);
       offset = 0;
       data = data + 1;
   }
   if(right_move > 0) {
       result = result >> right_move;
   }
   return result;
}

unsigned int mdp_char_reverse(unsigned char v)
{
    unsigned char r = v;               // r保存反转后的结果，开始获取v的最低有效位
    int s = sizeof(v) * CHAR_BIT - 1; // 剩余需要移位的比特位

    for (v >>= 1; v; v >>= 1)
    {
        r <<= 1;
        r |= v & 1;
        s--;
    }
    r <<= s; // 当v的最高位为0的时候进行移位
    return r;
}
int mdp_get_bit_from_byte(int v, int bit) {
    int tmp = 1 << bit;
    int ret = (v & tmp) >> bit;
    return ret;
}
std::string mal_int_to_hex_string(int value) {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << value;  // 2 位宽，前面填充零
    return ss.str();
}



}

