#include "mdp_int.h"
#include <stdint.h>
#include <limits.h>
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