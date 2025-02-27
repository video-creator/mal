//
//  mal_ff_packet_loader.cpp
//  Demo
//
//  Created by wangyaqiang on 2024/12/11.
//

#include "mal_ff_packet_loader.hpp"
using namespace mal;
static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    MALFFPacketLoader *loader = (MALFFPacketLoader *)opaque;
    auto datasource = loader->datasource();
    int64_t rest = datasource->lastBytes();
    int64_t size = FFMIN((int64_t)buf_size, rest);
    if (!size) {
        return AVERROR_EOF;
    }
    uint8_t *ptr = datasource->readBytesRaw(size);
    memcpy(buf, ptr, size);
    return size;
}
static int64_t io_seek(void *opaque, int64_t offset, int whence)
{
    MALFFPacketLoader *loader = (MALFFPacketLoader *)opaque;
    auto datasource = loader->datasource();
    if (whence == AVSEEK_SIZE) {
        return datasource->totalSize();
    }
    datasource->seekBytes(offset,whence);
    return datasource->currentBytesPosition();
}
MALFFPacketLoader::MALFFPacketLoader(std::shared_ptr<MALFormatContext> fmtCtx, int streamIndex):MALPacketLoader(fmtCtx,streamIndex) {
    av_log_set_level(AV_LOG_DEBUG);
    fmtCtx_ = avformat_alloc_context();
    if (!fmtCtx_) {
        error_ = AVERROR(ENOMEM);
        return;
    }
    int avio_ctx_buffer_size = 4096;
    avioBuffer_ = (uint8_t *)av_malloc(avio_ctx_buffer_size);
    if (!avioBuffer_) {
        error_ = AVERROR(ENOMEM);
        return;
    }
    avioCtx_ = avio_alloc_context(avioBuffer_, avio_ctx_buffer_size,
                                  0, this, &read_packet, NULL, &io_seek);
    if (!avioCtx_) {
        error_ = AVERROR(ENOMEM);
        return;
    }
    fmtCtx_->pb = avioCtx_;
    
    error_ = avformat_open_input(&fmtCtx_, NULL, NULL, NULL);
    if (error_ < 0) {
        fprintf(stderr, "Could not open input\n");
        return;
    }
    
    error_ = avformat_find_stream_info(fmtCtx_, NULL);
    if (error_ < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return;
    }
    for (int i = 0; i < fmtCtx_->nb_streams; i ++) {
        AVStream *st = fmtCtx_->streams[i];
        if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex_ = st->index;
        } else if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex_ = st->index;
        }
    }

}
std::vector<std::shared_ptr<MALPacket>> MALFFPacketLoader::loadPackets(int size) {
    int index = 0;
    std::vector<std::shared_ptr<MALPacket>> list;
    AVPacket *pkt = av_packet_alloc();
    int streamIndex = mediaType_ == MALMediaType::video ? videoStreamIndex_ : audioStreamIndex_;
    while ((error_ = av_read_frame(fmtCtx_, pkt)) >= 0 && index < size) {
        if (pkt->stream_index == streamIndex) {
            auto malPKT = std::make_shared<MALPacket>(pkt);
            list.push_back(malPKT);
            pkt = av_packet_alloc();
            index ++;
        }
    }
    av_packet_free(&pkt);
    return list;
}
