//
//  Test.cpp
//  Demo
//
//  Created by wangyaqiang on 2025/1/23.
//

#include "Test.hpp"

extern "C" {
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
};


#define UUID_SIZE 16
//private uuid, different with ffmpeg default uuid, for differentiating from x264 SEI
static uint8_t uuid[UUID_SIZE] = { 0xdc, 0x45, 0xe9, 0xbd, 0xe6, 0xd9, 0x48, 0xb7, 0x96, 0x2c, 0xd8, 0x20, 0xd9, 0x23, 0xee, 0xee };


uint8_t* GetSeiBuffer(uint8_t* data, uint32_t size, int* count) {
  uint8_t* sei = data;
  int sei_type = 0;
  uint32_t sei_size = 0;
  //payload type
  do {
    sei_type += *sei;
  } while (*sei++ == 255);
  //payload size
  do {
    sei_size += *sei;
  } while (*sei++ == 255);

  //check UUID
  if (sei_size >= UUID_SIZE && sei_size <= (data + size - sei) &&
      sei_type == 5 && memcmp(sei, uuid, UUID_SIZE) == 0) {
    sei += UUID_SIZE;
    sei_size -= UUID_SIZE;

    if (count != NULL) {
      *count = sei_size;
    }
    return sei;
  }
  return nullptr;
}


int NalUnitExtractRbsp(const uint8_t* src, uint32_t src_len,
                       uint8_t* dst, int* dst_len, int header_len) {

  uint32_t i, len;
  int emulation_prev_remove_bytes = 0;
  if (!dst)
    return emulation_prev_remove_bytes;

  /* NAL unit header */
  i = len = 0;
  while (i < header_len && i < src_len)
    dst[len++] = src[i++];

  while (i + 2 < src_len)
    if (!src[i] && !src[i + 1] && src[i + 2] == 3) {
      dst[len++] = src[i++];
      dst[len++] = src[i++];
      i++; // remove emulation_prevention_three_byte
      emulation_prev_remove_bytes++;
    } else
      dst[len++] = src[i++];

  while (i < src_len)
    dst[len++] = src[i++];

  memset(dst + len, 0, AV_INPUT_BUFFER_PADDING_SIZE);

  *dst_len = len;
  return emulation_prev_remove_bytes;
}

std::unique_ptr<uint8_t[]> GetSeiContentAVCC(uint8_t* packet, uint32_t packet_size, int* out_content_size) {
  uint8_t* data = packet;
  int nalu_size = 0;

  std::unique_ptr<uint8_t[]>info = nullptr;
  int emulation_prev_remove_bytes = 0;
  int size_of_nal_content = 0;

  // todo: fix read out of range
  while (data < packet + packet_size) {
    //NALU size
    nalu_size = 0;
    for (int i = 0; i < 4; i++)
      nalu_size = ((unsigned)nalu_size << 8) | data[i];
    //NALU header
    if ((*(data + 4) & 0x1F) == 6) {
      //SEI
      uint8_t* sei = data + 4 + 1;

      uint8_t* ret = GetSeiBuffer(sei, std::min(nalu_size, static_cast<int>(packet + packet_size - sei)), out_content_size);
      if (ret) {
        size_of_nal_content = *out_content_size;
        info.reset(new (std::nothrow) uint8_t[*out_content_size + AV_INPUT_BUFFER_PADDING_SIZE]);
        emulation_prev_remove_bytes = NalUnitExtractRbsp(ret, *out_content_size, info.get(), out_content_size, 0);
        if (emulation_prev_remove_bytes > 0) {
          memcpy(info.get() + *out_content_size, ret + size_of_nal_content,
                 emulation_prev_remove_bytes);
          *out_content_size += emulation_prev_remove_bytes;
        }
        return info;
      }
    }
    data += 4 + nalu_size;
  }
  return nullptr;
}


std::unique_ptr<uint8_t[]> GetUserInfoFromSEI(const char* path, int* out_info_size) {
  AVFormatContext* ifmt_ctx = nullptr;
  int ret = -1;
  AVPacket pkt;
  av_init_packet(&pkt);
  pkt.data = nullptr;
  pkt.size = 0;
  std::unique_ptr<uint8_t[]>info = nullptr;
  if (!path) {
//    XLOGE("path is null");
    return nullptr;
  }
  if ((ret = avformat_open_input(&ifmt_ctx, path, 0, 0)) < 0) {
//    XLOGE("Could not open input file '%s', %s", path, AVError2Str(ret).c_str());
    if (ifmt_ctx) {
      avformat_close_input(&ifmt_ctx);
    }
    return info;
  }

  if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
//    XLOGE("Failed to retrieve '%s' input stream information, %s", path, AVError2Str(ret).c_str());
    if (ifmt_ctx) {
      avformat_close_input(&ifmt_ctx);
    }
    return info;
  }

  if (strstr(ifmt_ctx->iformat->name, "mp4") == nullptr) {
//    XLOGE("Input file is not mp4");
    avformat_close_input(&ifmt_ctx);
    return info;
  }

  int video_stream_idx = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
  if (video_stream_idx < 0) {
    avformat_close_input(&ifmt_ctx);
    return info;
  }

  auto codec_ctx = ifmt_ctx->streams[video_stream_idx]->codecpar;
  if (codec_ctx->codec_id != AV_CODEC_ID_H264 && codec_ctx->codec_id != AV_CODEC_ID_HEVC) {
    avformat_close_input(&ifmt_ctx);
    return info;
  }

  while (true) {
    ret = av_read_frame(ifmt_ctx, &pkt);
    if (ret < 0) {
      break;
    }
    if (ret >= 0 && pkt.stream_index == video_stream_idx) {
      break;
    }
    av_packet_unref(&pkt);
  }

  if (ret >= 0 && pkt.size > 5) {
    //first video packet, if could not get SEI, return ""
    if (codec_ctx->codec_id == AV_CODEC_ID_H264) {
      info = GetSeiContentAVCC(pkt.data, pkt.size, out_info_size);
    } else {
      // codec_ctx->codec_id == AV_CODEC_ID_HEVC
    }
  }
  av_packet_unref(&pkt);

  if (ifmt_ctx) {
    avformat_close_input(&ifmt_ctx);
  }
  return info;
}
