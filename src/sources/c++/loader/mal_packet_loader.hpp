//
//  mal_packet_loader.hpp
//  Demo
//
//  Created by wangyaqiang on 2024/12/11.
//

#ifndef mal_packet_loader_hpp
#define mal_packet_loader_hpp

#include <stdio.h>
#include "../datasource/mal_idatasource.h"
#include "../parser/mal_atom.h"
#include "../parser/mal_codec_parser.h"
namespace mal {
class MALPacketLoader {
public:
    explicit MALPacketLoader(std::shared_ptr<MALFormatContext> fmtCtx, int streamIndex):fmtCtx_(fmtCtx), streamIndex_(streamIndex){
        parser_ = std::make_shared<MALCodecParser>(fmtCtx);
    }
    virtual ~MALPacketLoader() {
        
    }
    virtual std::vector<std::shared_ptr<MALPacket>> loadPackets(int size) = 0;
    std::shared_ptr<IDataSource> datasource() {
        return datasource_;
    }
protected:
    int streamIndex_ = 0;
    std::shared_ptr<MALFormatContext> fmtCtx_ = nullptr;
    std::shared_ptr<IDataSource> datasource_ = nullptr;
    MALMediaType mediaType_ = MALMediaType::video;
    int error_ = 0;
    std::shared_ptr<MALCodecParser> parser_ = nullptr;
};
};
#endif /* mal_packet_loader_hpp */
