//
//  mal_ff_packet_loader.hpp
//  Demo
//
//  Created by wangyaqiang on 2024/12/11.
//

#ifndef mal_ff_packet_loader_hpp
#define mal_ff_packet_loader_hpp

#include <stdio.h>
#include "mal_packet_loader.hpp"
extern "C" {
#include "libavformat/avformat.h"
}
namespace mal {
class MALFFPacketLoader: public MALPacketLoader {
public:
    MALFFPacketLoader(std::shared_ptr<MALFormatContext> fmtCtx, int streamIndex);
    std::vector<std::shared_ptr<MALPacket>> loadPackets(int size) override;
private:
    AVFormatContext *fmtCtx_ = nullptr;
    AVIOContext *avioCtx_ = nullptr;
    uint8_t * avioBuffer_ = nullptr;
    int videoStreamIndex_  = -1;
    int audioStreamIndex_ = -1;
};
};
#endif /* mal_ff_packet_loader_hpp */
