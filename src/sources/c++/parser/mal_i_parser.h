#pragma once
#include "mal_idatasource.h"
#include "mal_atom.h"
#include "mal_codec_parser.h"
#include "../loader/mal_packet_loader.hpp"
#define rbits_i(bits) datasource->readBitsInt64(bits)
#define rbytes_i(bits) datasource->readBytesInt64(bits)
#define rbits_i_little(bits) datasource->readBitsInt64(bits,false,0)
#define rbytes_i_little(bits) datasource->readBytesInt64(bits,false,0)
#define rbytes_s(bits) datasource->readBytesString(bits)
namespace mal {
    class IParser {
        public:
        IParser(const std::shared_ptr<IDataSource>& datasource);
        IParser(const std::string& path, Type type);
        virtual ~IParser();
        virtual int startParse();
        virtual std::vector<std::shared_ptr<MALPacket>> loadPackets(int size);
        virtual bool supportFormat();
        virtual std::string dumpFormats(int full = 0);
        virtual std::string dumpVideoConfig();
        virtual std::string dumpShallowCheck();
        virtual std::string dumpDeepCheck();
        protected:
        std::shared_ptr<IDataSource> _datasource;
        std::shared_ptr<MDPAtom> root_atom_;
        std::shared_ptr<MALPacketLoader> pktLoader_ = nullptr;
        std::shared_ptr<MALFormatContext> malFormatContext_ = nullptr;
    };
}
