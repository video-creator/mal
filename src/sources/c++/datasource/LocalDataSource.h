#pragma once
#include "IDataSource.h"
namespace mdp {
    class LocalDataSource : public IDataSource {
        public:
        LocalDataSource(const std::string& path);
        LocalDataSource(uint8_t * memoryData, int64_t size, bool copy = false);
        int open();
        std::shared_ptr<IDataSource> readBitsStream(int64_t bits_size, bool rewind = false);
        uint64_t readBitsInt64(int64_t bits_size, bool rewind = false);
        std::shared_ptr<IDataSource> readBytesStream(int64_t bytes, bool rewind = false);
        uint64_t readBytesInt64(int64_t bytes, bool rewind = false);
        std::string readBytesString(int64_t bytes, bool rewind = false);

        int64_t totalSize() { //bytes
            return _memorySize;
        }
        void seekBytes(int64_t position, int whence = SEEK_CUR); // position: bytes
        void skipBytes(int64_t position);
        void skipBits(int64_t position);
        int64_t currentBytesPosition(); // bytes
        int64_t currentBitsPosition();
        unsigned char *readBytesRaw(int64_t bytes, bool rewind = false);
        bool isEof();
        private:
        std::string _path;
        uint8_t *_memoryData;
        size_t _memorySize;
        std::shared_ptr<bits::bitstream> _bitsReader;
    };
}