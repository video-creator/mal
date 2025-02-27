#pragma once
extern "C" {
#include "../c_ffi/mdp_type_def.h"
}

#include <stdlib.h>
#include "mal_idatasource.h"
#include "mal_atom.h"
using namespace mal;
#define MDP_HEADER_DEFAULT_GROW_SIZE 5
class MALCodecParser {
public:
    explicit MALCodecParser(std::shared_ptr<IDataSource> datasource, std::shared_ptr<MALFormatContext> malFmtCtx, std::shared_ptr<MALStream> stream, std::shared_ptr<MALVideoConfig> videoConfig ): datasource_(datasource), malFmtCtx_(malFmtCtx), stream_(stream) {
        if (stream->codecType == MALCodecType::h264) {
            avcc_ = std::dynamic_pointer_cast<MALAVCC>(videoConfig);
        } else if (stream->codecType == MALCodecType::h265) {
            hvcc_ = std::dynamic_pointer_cast<MALHVCC>(videoConfig);
        }
    }
    explicit MALCodecParser(std::shared_ptr<MALFormatContext> malFmtCtx):malFmtCtx_(malFmtCtx) {
        
    }
    mdp_video_header * parse_avcc();
    mdp_video_header * parse_hvcc();
    int parse_packet(std::shared_ptr<MALPacket> pkt, bool annextb = false);
private:
    std::shared_ptr<IDataSource> datasource_ = nullptr;
    std::shared_ptr<MALFormatContext> malFmtCtx_ = nullptr;
    std::shared_ptr<MALAVCC> avcc_ = nullptr;
    std::shared_ptr<MALHVCC> hvcc_ = nullptr;
    std::shared_ptr<MALAVCNal> currentAVCNal_ = nullptr;
    std::shared_ptr<MALHEVCNal> currentHEVCNal_ = nullptr;
    std::shared_ptr<MALPacket> prePkt_ = nullptr;
    std::shared_ptr<MALPacket> curPkt_ = nullptr;
    std::shared_ptr<MALStream> stream_ = nullptr;
    int hevc_pocTid0 = 0; //poc使用
    
   
    void parse_avc_nal_header(mdp_header_item *item);
    void avc_hrd_parameters(mdp_header_item *item);
    void vui_parameters(int hevc, mdp_header_item *item);
    void scaling_list(int *scalingList,int sizeOfScalingList,int* useDefaultScalingMatrixFlag,mdp_header_item *item);
    int parse_avc_sps(mdp_video_header *header, mdp_header_item *item);
    int more_rbsp_data();
    void rbsp_trailing_bits(mdp_header_item *item);
    int parse_avc_pps(mdp_video_header *header, mdp_header_item *item);
    std::vector<MALSEIMessage> parse_avc_sei(mdp_video_header *header, mdp_header_item *item, int64_t size);
    int parse_avc_slice(mdp_video_header *header, mdp_header_item *item,std::shared_ptr<MALPacket> pkt);
    int avc_ref_pic_list_modification(MALAVCSliceHeader &sliceHeader, mdp_video_header *header, mdp_header_item *item);
    int avc_pred_weight_table(MALAVCSliceHeader &sliceHeader, mdp_video_header *header, mdp_header_item *pitem,std::shared_ptr<MALAVCSPS> sps ,std::shared_ptr<MALAVCPPS> pps);
    int avc_dec_ref_pic_marking(MALAVCSliceHeader &sliceHeader, mdp_video_header *header, mdp_header_item *pitem,std::shared_ptr<MALAVCSPS> sps ,std::shared_ptr<MALAVCPPS> pps) ;
    int avc_packet_poc(MALAVCSliceHeader &sliceHeader, std::shared_ptr<MALPacket> pkt, std::shared_ptr<MALAVCSPS> sps ,std::shared_ptr<MALAVCPPS> pps) ;
    
    
    
    void parse_hvc_nal_header(mdp_header_item *item);
    void hevc_scaling_list_data(mdp_header_item *pitem);
    void hevc_st_ref_pic_set(mdp_header_item *pitem,int stRpsIdx,int num_short_term_ref_pic_sets,int num_delta_pocs[64], int num_negative_pics[64], int num_positive_pics[64]);
    void hevc_sps_3d_extension(mdp_header_item *pitem);
    void hevc_profile_tier_level(mdp_header_item *pitem, int profilePresentFlag, int maxNumSubLayersMinus1);
    void hevc_sub_layer_hrd_parameters(mdp_header_item *pitem, int i,int cpb_cnt_minus1,int sub_pic_hrd_params_present_flag);
    void hevc_hrd_parameters(mdp_header_item *pitem, int commonInfPresentFlag, int maxNumSubLayersMinus1);
    int parse_hvc_vps(mdp_video_header *header, mdp_header_item *item);
    int parse_hvc_sps(mdp_video_header *header, mdp_header_item *item);
    void hevc_colour_mapping_octants(mdp_header_item *pitem, int inpDepth, int idxY, int idxCb, int idxCr, int inpLength,int cm_octant_depth,int PartNumY,int cm_delta_flc_bits_minus1);
    void hevc_colour_mapping_table(mdp_header_item *pitem);
    void hevc_delta_dlt(mdp_header_item *pitem, int i, int pps_bit_depth_for_depth_layers_minus8);
    int parse_hvc_pps(mdp_video_header *header, mdp_header_item *item);
    void hevc_pps_3d_extension(mdp_header_item *pitem);
    void hevc_pps_multilayer_extension(mdp_header_item *pitem);
    void hevc_pps_range_extension(mdp_header_item *pitem,int transform_skip_enabled_flag);
    int parse_hevc_slice(mdp_video_header *header, mdp_header_item *pitem,std::shared_ptr<MALPacket> pkt);
    
    void hevc_vps_extension(mdp_header_item *pitem);
    
    int hevc_poc(std::shared_ptr<MALHEVCSPS> sps, int pocTid0, int poc_lsb, int nal_unit_type);
    std::vector<MALSEIMessage> parse_hevc_sei(mdp_video_header *header, mdp_header_item *item, int64_t size);
};
//mdp_video_header * mdp_parse_avcc(std::shared_ptr<IDataSource> datasource);
//
//mdp_video_header * mdp_parse_hvcc(std::shared_ptr<IDataSource> datasource);
//
//char ** mdp_parse_h264_sei(std::shared_ptr<IDataSource> datasource);


