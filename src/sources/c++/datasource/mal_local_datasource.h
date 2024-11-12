#pragma once
#include "mal_idatasource.h"
//#include "../bits/bits-stream.h"
#include "../bits/bits_stream.hpp"
namespace mal {
    class LocalDataSource : public IDataSource {
        public:
        LocalDataSource(const std::string& path);
        LocalDataSource(uint8_t * memoryData, int64_t size, bool copy = false);
        int open();
        std::shared_ptr<IDataSource> readBitsStream(int64_t bits_size, bool rewind = false);
        uint64_t readBitsInt64(int64_t bits_size, bool rewind = false, int big = 1, bool convert_rbsp = false);
        std::shared_ptr<IDataSource> readBytesStream(int64_t bytes, bool rewind = false);
        uint64_t readBytesInt64(int64_t bytes, bool rewind = false, int big = 1, bool convert_rbsp = false);
        std::string readBytesString(int64_t bytes, bool rewind = false);

        int64_t totalSize() { //bytes
            return _memorySize;
        }
        void seekBytes(int64_t position, int whence = SEEK_CUR); // position: bytes
        void skipBytes(int64_t position);
        void skipBits(int64_t position);
        int64_t currentBytesPosition(); // bytes
        int64_t currentBitsPosition();
        int64_t lastBytes();
        unsigned char *readBytesRaw(int64_t bytes, bool rewind = false);
        unsigned char *current(); //当前指针，如果在一个字节内，返回这个字节
        unsigned char *ptr(); //原视指针
        bool isEof();
        std::shared_ptr<IDataSource> shallowCopy();
        
        private:
        std::string _path;
        uint8_t *_memoryData;
        size_t _memorySize;
        std::shared_ptr<mal::BitStream> _bitsReader;
        int64_t nextNonNullLength();
    };
}
