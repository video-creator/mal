#pragma once
#include <string>

namespace mal {
    enum class Type
    {
        local,
        concat,
        http,
        memory
    };
    class IDataSource {
        public:
        virtual ~IDataSource() {}
        Type type;
        virtual int open() = 0;
        virtual std::shared_ptr<IDataSource> readBitsStream(int64_t bits_size, bool rewind = false) = 0;
        virtual uint64_t readBitsInt64(int64_t bits_size, bool rewind = false, int big = 1, bool convert_rbsp = false) = 0;
        virtual std::shared_ptr<IDataSource> readBytesStream(int64_t bytes, bool rewind = false) = 0;
        virtual uint64_t readBytesInt64(int64_t bytes, bool rewind = false, int big = 1, bool convert_rbsp = false) = 0;
        virtual std::string readBytesString(int64_t bytes, bool rewind = false) = 0;

        virtual void seekBytes(int64_t position, int whence = SEEK_CUR) = 0;
        virtual void skipBytes(int64_t position) = 0;
        virtual void skipBits(int64_t position) = 0;
        virtual unsigned char *readBytesRaw(int64_t bytes, bool rewind = false) = 0;
        virtual unsigned char *current() = 0; //当前指针，如果在一个字节内，返回这个字节
        virtual unsigned char *ptr() = 0; //原视指针
        virtual int64_t totalSize() = 0;
        virtual int64_t currentBytesPosition() = 0;
        virtual int64_t currentBitsPosition() = 0;
        virtual int64_t lastBytes() = 0;//剩余bytes
        virtual bool isEof() = 0;
        virtual int64_t nextNonNullLength() = 0; //返回从当前位置开始，到下一个非0字符的长度
        virtual std::shared_ptr<IDataSource> shallowCopy() = 0;
        
        private:

    };
}
