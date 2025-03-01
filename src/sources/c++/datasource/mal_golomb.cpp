#include "mal_golomb.h"
#include <limits.h>
#include <stdio.h>
#include <bitset>
static void reverseOneBytesBitset(std::bitset<8> &set) {
      int a1 = set[0];
      int a2 = set[1];
      int a3 = set[2];
      int a4 = set[3];
      set[0] = set[7];
      set[1] = set[6];
      set[2] = set[5];
      set[3] = set[4];
      set[4] = a4;
      set[5] = a3;
      set[6] = a2;
      set[7] = a1;
  }

// 0阶无符号哥伦布解码
unsigned int golomb_ue(std::shared_ptr<IDataSource> datasource, int64_t *had_read_bits)
{
    int64_t offset = datasource->currentBitsPosition();
    unsigned char *data = datasource->ptr();
    data = data+offset/8;
    offset = offset % 8;
    int index = 0;
    int m = 0;
    int k = 0; //0阶
    bool findM = false;
    int totalReadBits = 0;
retry:
    int temp = data[index];
    std::bitset<8> bits(temp);
    //        qDebug() << bits[0] << "," << bits[1] << "," << bits[2] << "," << bits[3] << "," << bits[4] << "," << bits[5] << "," << bits[6] << ","<< bits[7] ;
    reverseOneBytesBitset(bits);
    
    
    int start = offset;
    offset = 0;
    while(start < 8) {
        if(bits[start] == 0) {
            m++;
            totalReadBits++;
        } else {
            findM = true;
            break;
        }
        start ++;
    }
    if(findM == false) {
        index ++;
        goto retry;
    }
    start += 1;
    totalReadBits++;
    if(start >= 8) {
        index++;
        start -= 8;
    }
    int totolBits  = m+k;
    unsigned int resut = 0;
value:
    int value_temp = data[index];
    std::bitset<8> value_bits(value_temp);
    reverseOneBytesBitset(value_bits);
    for(int i= start; i< 8 && totolBits > 0;i++) {
        int x = value_bits[i];
        resut += (x << (totolBits - 1));
        totolBits --;
        totalReadBits++;
        if(totolBits <= 0) {
            break;
        }
    }
    start = 0;
    if(totolBits > 0) {
        index++;
        goto value;
    }
    int x1 = (1 << (m+k));
    int x2 = (1 << k);
    
    if (had_read_bits) {
        *had_read_bits = totalReadBits;
    }
    datasource->skipBits(totalReadBits);
    return (1 << (m+k)) - (1 << k) + resut;
}
int golomb_se(std::shared_ptr<IDataSource> datasource, int64_t *had_read_bits)
{
    unsigned int ue = golomb_ue(datasource, had_read_bits);
    if (ue & 1)
    {
        return (ue + 1) / 2;
    }
    return ue / 2 * -1;
}
