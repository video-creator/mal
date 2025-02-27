//
//  mal_mp4_packet_loader.cpp
//  Demo
//
//  Created by wangyaqiang on 2024/12/16.
//

#include "mal_mp4_packet_loader.hpp"
#include "../deps/spdlog/fmt/fmt.h"
using namespace mal;
MALMP4PacketLoader::MALMP4PacketLoader(std::shared_ptr<MALFormatContext> fmtCtx, int streamIndex): MALPacketLoader(fmtCtx, streamIndex) {
    if (streamIndex < fmtCtx->streams.size()) {
        stream_ = std::dynamic_pointer_cast<MALMP4Stream>(fmtCtx->streams[streamIndex]);
    }
}
std::tuple<int64_t, int> MALMP4PacketLoader::findSamplePos(int sampleIndex) {
    auto stco = stream_->stco;
    auto stsc = stream_->stsc;
    auto stsz = stream_->stsz;
    uint32_t chunkIndex = 0;
    uint32_t sampleOffsetInChunk = 0;
    uint32_t samplesProcessed = 0;
    int sampleDescriptionIndex = 0;
    // 找到样本所在的chunk
    for (size_t i = 0; i < stsc.size(); ++i) {
        auto el = stsc.at(i);
        auto first_chunk = std::get<0>(el);
        auto samples_per_chunk = std::get<1>(el);
        auto sample_description_index = std::get<2>(el);
        
        int64_t nextFirstChunk = (i + 1 < stsc.size()) ? std::get<0>(stsc[i+1]) : UINT32_MAX;
        int64_t chunksInEntry = nextFirstChunk - first_chunk;
        
        int64_t samplesInEntry = chunksInEntry * samples_per_chunk;
        if (sampleIndex < samplesProcessed + samplesInEntry) {
            sampleDescriptionIndex = sample_description_index;
            chunkIndex = first_chunk + (sampleIndex - samplesProcessed) / samples_per_chunk;
            sampleOffsetInChunk = (sampleIndex - samplesProcessed) % samples_per_chunk;
            break;
        }
        
        samplesProcessed += samplesInEntry;
    }
    
    // 获取chunk的文件偏移
    int64_t chunkOffset = stco[chunkIndex - 1]; // 注意chunkIndex是从1开始的
    
    // 计算样本在chunk中的偏移
    uint32_t sampleOffset = 0;
    for (uint32_t i = sampleIndex - sampleOffsetInChunk; i < sampleIndex; ++i) {
        sampleOffset += stsz [i];
    }
    // 样本的文件位置
    return {chunkOffset + sampleOffset, sampleDescriptionIndex};
}
void MALMP4PacketLoader::calculateTS(std::shared_ptr<MALPacket> pkt) {
    if (!stream_) return;
    auto stts = stream_->stts;
    auto ctts = stream_->ctts;
    auto elst = stream_->elst;
    int64_t num = 0;
    int64_t dts = 0;
    int64_t pts = 0;
    for (int i = 0; i < stts.size(); i++) {
        int64_t sample_count = std::get<0>(stts.at(i));
        int64_t delta = std::get<1>(stts.at(i));
        for (int64_t j = 0; j < sample_count; j++) {
            if (num == pkt->number) {
                goto ctts;
            }
            num++;
            dts += delta;
        }
    }
ctts:
    pts = dts;
    num = 0;
    for (int i = 0; i < ctts.size(); i++) {
        int64_t sample_count = std::get<0>(ctts.at(i));
        int64_t offset = std::get<1>(ctts.at(i));
        for (int64_t j = 0; j < sample_count; j++) {
            if (num == pkt->number) {
                pts += offset;
                goto elst;
            }
            num++;
        }
    }
elst:
    if (elst.size() > 0) {
        auto el = elst.at(0);
        int64_t segment_duration = std::get<0>(el);
        int64_t media_time = std::get<1>(el);
        if (media_time != -1) {
            dts -= media_time;
            pts -= media_time;
        }
    }
    pkt->dts = dts;
    pkt->pts = pts;
}
void MALMP4PacketLoader::checkPTS(std::shared_ptr<MALPacket> pkt) {
    if (pkt->flag == MALPacketFlag::idr) {
        sortList_.clear();
    }
    sortList_.push_back(pkt);
    if (sortList_.size() > 0) {
        std::sort(sortList_.begin(), sortList_.end(), [](const auto &a, const auto &b)
                  {
                      return a->poc < b->poc;
        });
        int64_t prePts = sortList_.at(0)->pts;
        for (auto &el : sortList_) {
            if (el->pts < prePts) {
                std::string check = fmt::format("第{}个packet存在pts倒退，请注意画面抖动",el->number);
                if (std::find(fmtCtx_->shallowCheck.warning.begin(), fmtCtx_->shallowCheck.warning.end(), check) == fmtCtx_->shallowCheck.warning.end()) {
                    fmtCtx_->shallowCheck.warning.push_back(check);
                }
            }
            prePts = el->pts;
        }
        
    }
}
std::vector<std::shared_ptr<MALPacket>> MALMP4PacketLoader::loadPackets(int size) {
    if (!stream_) return {};
    auto stsz = stream_->stsz;
    std::vector<std::shared_ptr<MALPacket>> list = {};
    for (int i = currentIndex_; i < currentIndex_ + size && i < stsz.size(); i ++) {
        int64_t entrySize = stsz[i];
        auto result = findSamplePos(i);
        int64_t pos = std::get<0>(result);
        int sample_description_index = std::get<1>(result);
        fmtCtx_->datasource->seekBytes(pos, SEEK_SET);
        auto packet = std::make_shared<MALPacket>(fmtCtx_->datasource->readBytesRaw(entrySize), entrySize);
        packet->index = stream_->index;
        packet->pos = pos;
        packet->number = currentIndex_;
        if (std::find(stream_->stss.begin(), stream_->stss.end(), packet->number+1) != stream_->stss.end()) {
            packet->flag = MALPacketFlag::idr;
        }
        packet->sample_description_index = sample_description_index;
        if (parser_) {
            parser_->parse_packet(packet);
        }
        calculateTS(packet);
        list.push_back(packet);
        currentIndex_++;
        std::cout << "sample pos :" << pos << " size:" << entrySize << " at index: " << i << " POC : " << packet->poc << " dts:" << packet->dts << " pts:" << packet->pts << " pkt flag:" << mal_convert_pkt_flag_to_str(packet->flag) << " ref_idc: " << packet->nal_ref_idc << std::endl;
        checkPTS(packet);
    }
    return list;
}
