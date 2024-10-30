#include "mdp_bits.h"
#include "libavutil/mem.h"
 
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