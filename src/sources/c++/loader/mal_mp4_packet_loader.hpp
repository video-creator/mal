//
//  mal_mp4_packet_loader.hpp
//  Demo
//
//  Created by wangyaqiang on 2024/12/16.
//

#ifndef mal_mp4_packet_loader_hpp
#define mal_mp4_packet_loader_hpp

#include <stdio.h>
#include <iostream>
#include "mal_packet_loader.hpp"
namespace mal {
class MALMP4PacketLoader: public MALPacketLoader {
public:
    MALMP4PacketLoader(std::shared_ptr<MALFormatContext> fmtCtx, int streamIndex);
    std::vector<std::shared_ptr<MALPacket>> loadPackets(int size) override;
    
private:
    int currentIndex_ = 0;
    
    std::tuple<int64_t,int> findSamplePos(int sampleIndex);
    std::shared_ptr<MALMP4Stream> stream_ = nullptr;
    void calculateTS(std::shared_ptr<MALPacket> pkt);
    void checkPTS(std::shared_ptr<MALPacket> pkt);
    std::vector<std::shared_ptr<MALPacket>> sortList_;
};
};
#endif /* mal_mp4_packet_loader_hpp */
