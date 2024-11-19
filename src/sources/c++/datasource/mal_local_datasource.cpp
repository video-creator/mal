#include "mal_idatasource.h"
#include "mal_local_datasource.h"
extern "C" {
    #include  <libavutil/mem.h>
    #include "libavutil/file.h"
}
using namespace mal;
LocalDataSource::LocalDataSource(const std::string& path):_path(path) {
    
}
LocalDataSource::LocalDataSource(uint8_t * memoryData, int64_t size, bool copy){
    if (copy) {
        if (memoryData && size > 0) {
            _memoryData = (uint8_t *)av_mallocz(size);
            memccpy(_memoryData,memoryData, 1, size);
        }
    } else {
        _memoryData = memoryData;
    }
    _memorySize = size;
}
int LocalDataSource::open() {
    int ret = 0;
    if (_path.length() > 0) {
        ret = av_file_map(_path.c_str(),&_memoryData,&_memorySize,0,NULL);
    }
    _bitsReader =  std::make_shared<mal::BitStream>((unsigned char *)_memoryData,_memorySize);
    return ret;
}
int64_t LocalDataSource::lastBytes() {
    return totalSize() - currentBytesPosition();
}
std::shared_ptr<IDataSource> LocalDataSource::readBitsStream(int64_t bits_size, bool rewind) {
    if (!_bitsReader->aligned()) {
        return nullptr;
    }
    if (bits_size % 8 != 0) {
        return nullptr;
    }
    std::shared_ptr<LocalDataSource> datasource = std::make_shared<LocalDataSource>(_bitsReader->current(),bits_size/8);
    if (!rewind) {
        _bitsReader->skip(bits_size);
    }
    datasource->open();
    return datasource;
}
unsigned char * LocalDataSource::readBytesRaw(int64_t bytes, bool rewind) {
    unsigned char *data = _bitsReader->current();
    if (!rewind) {
        _bitsReader->skip(bytes * 8);
    }
    return data;
}
int64_t bigEndianToLittleEndian(int64_t bigEndianValue, int64_t bytes_size) {
    int64_t littleEndianValue = 0;
    
    for (int i = 0; i < bytes_size; ++i) {
        littleEndianValue |= ((bigEndianValue >> (8 * i)) & 0xFF) << (8 * (bytes_size - 1 - i));
    }
    
    return littleEndianValue;
}

uint64_t LocalDataSource::readBitsInt64(int64_t bits_size, bool rewind, int big, bool convert_rbsp) {
    if (bits_size == 0) return 0;
    int64_t current = _bitsReader->position();
    int64_t ret = 0;
    if (!big) {
//        int64_t temp = (bits_size / 8 + 1 ) * 8;
//        int64_t more = temp - bits_size;
//        ret = _bitsReader->readBits(temp,convert_rbsp);
//        ret = bigEndianToLittleEndian(ret,temp/8);
//        ret = (ret & (0xffff >> more));
//        _bitsReader->skip(-more);
        ret = _bitsReader->readLittleBits(bits_size,convert_rbsp);
    } else {
        ret = _bitsReader->readBits(bits_size,convert_rbsp);//_bitsReader->read_at<uint64_t>(current,bits_size);
//        if (bits_size > 8 && bits_size % 8 == 0 && !big) {
//            ret = bigEndianToLittleEndian(ret,bits_size/8);
//        }
    }
    
    if (rewind) {
        _bitsReader->seek(current);//_bitsReader->skip(bits_size);
    }
    return ret;
}
void LocalDataSource::seekBytes(int64_t position, int whence) {
    if (whence == SEEK_CUR) {
        _bitsReader->seek((position + currentBytesPosition()) * 8);
    } else if (whence == SEEK_SET) {
        _bitsReader->seek(position * 8);
    } else if (whence == SEEK_END) {
        if (position < 0) {
            _bitsReader->seek((totalSize() + position)* 8);
        }
    }
}
void LocalDataSource::skipBytes(int64_t position) {
    _bitsReader->skip(position * 8);
}
void LocalDataSource::skipBits(int64_t position) {
    _bitsReader->skip(position);
}
std::shared_ptr<IDataSource> LocalDataSource::readBytesStream(int64_t bytes, bool rewind) {
    return readBitsStream(bytes * 8, rewind);
}
uint64_t LocalDataSource::readBytesInt64(int64_t bytes, bool rewind, int big, bool convert_rbsp) {
    return readBitsInt64(bytes * 8, rewind,big, convert_rbsp);
}
std::string LocalDataSource::readBytesString(int64_t bytes, bool rewind) {
    if (bytes == 0) return "";
    int64_t current = currentBitsPosition();
    std::string result = "";
    for (int i = 0; i < bytes; i++) {
        int64_t val = readBytesInt64(1);
        if (val < 128) {
            result += static_cast<char>(val); // 转换为字符并添加到结果中
        } else {
            // 对于非 ASCII 字符，可以选择以十六进制格式添加到结果中
            result += "[" + std::to_string(static_cast<int>(val)) + "]"; // 也可以使用 std::hex 转换
        }
    }
    if (rewind) {
        _bitsReader->seek(current);
    }
    return result;
}
int64_t LocalDataSource::currentBytesPosition() {
    return _bitsReader->position() / 8;
}
unsigned char *LocalDataSource::current() {
    return _bitsReader->current();
}
unsigned char *LocalDataSource::ptr() {
    return _bitsReader->ptr();
}
int64_t LocalDataSource::currentBitsPosition() {
    return _bitsReader->position();
}
bool LocalDataSource::isEof() {
    return _bitsReader->position() == totalSize() * 8;
}
int64_t LocalDataSource::nextNonNullLength() {
    if (isEof()) return 0;
    int64_t current = 0;
    int64_t size = 0;
    current = currentBytesPosition();
    while (!isEof() && readBytesInt64(1)) {
    }
    size = currentBytesPosition() - current;
    if (size > 0) {
        seekBytes(current,SEEK_SET);
    }
    return size;
}

std::shared_ptr<IDataSource> LocalDataSource::shallowCopy() {
    std::shared_ptr<LocalDataSource> datasource = std::make_shared<LocalDataSource>(ptr(), totalSize());
    datasource->open();
    return datasource;
}
