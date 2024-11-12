//
//  bits_stream.hpp
//  Demo
//
//  Created by wangyaqiang on 2024/11/8.
//

#ifndef bits_stream_hpp
#define bits_stream_hpp

#include <stdio.h>
#include <vector>
namespace mal {
class BitStream {
public:
    BitStream(unsigned char *data, int64_t size) : data_(data), size_(size), bitPosition_(0) {}

    // 读取n位的数据
    uint64_t readBits(size_t n, bool convert_rbsp = false) {
        uint64_t value = 0;
        for (size_t i = 0; i < n; ++i) {
            value <<= 1;
            if (convert_rbsp && aligned() && bitPosition_ > 16) {
                uint8_t *temp = (uint8_t *)data_;
                int64_t currentBytes = bitPosition_ / 8;
                if (temp[currentBytes] == 3 && temp[currentBytes-1] == 0 && temp[currentBytes-2] == 0) {
                    skip(8);
                }
            }
            if (readBit()) {
                value |= 1;
            }
        }
        return value;
    } 
    uint64_t readLittleBits(size_t n, bool convert_rbsp = false) {
        uint64_t value = 0;
        for (size_t i = 0; i < n; ++i) {
            if (convert_rbsp && aligned() && bitPosition_ > 16) {
                uint8_t *temp = (uint8_t *)data_;
                int64_t currentBytes = bitPosition_ / 8;
                if (temp[currentBytes] == 3 && temp[currentBytes-1] == 0 && temp[currentBytes-2] == 0) {
                    skip(8);
                }
            }
            int x = readLittleBit();
//            if (readBit()) {
//                value |= 1;
//            }
            value |= x << i;
        }
        return value;
    }
    int64_t position() {
        return bitPosition_;
    }
    unsigned char *current() {
        return data_ + bitPosition_ / 8;
    }
    unsigned char *ptr() {
        return data_;
    }
    bool aligned() {
        return bitPosition_ % 8 == 0;
    }
    void seek(int64_t position) { bitPosition_ = position; }
    void skip(int64_t bits) { bitPosition_ += bits;}
    // 读取1位数据
    bool readBit() {
        if (bitPosition_ >= size_ * 8) {
            throw std::out_of_range("Bit position out of range");
        }

        size_t byteIndex = bitPosition_ / 8;
        size_t bitIndex = 7 - (bitPosition_ % 8);
        ++bitPosition_;

        return (data_[byteIndex] & (1 << bitIndex)) != 0;
    }
    bool readLittleBit() {
        if (bitPosition_ >= size_ * 8) {
            throw std::out_of_range("Bit position out of range");
        }

        size_t byteIndex = bitPosition_ / 8;
        size_t bitIndex = (bitPosition_ % 8);
        ++bitPosition_;

        return (data_[byteIndex] & (1 << bitIndex)) != 0;
    }

    // 重置读取位置
    void reset() {
        bitPosition_ = 0;
    }

private:
    unsigned char * data_; // 数据流
    int64_t bitPosition_;               // 当前位位置
    int64_t size_;
};
};
#endif /* bits_stream_hpp */
