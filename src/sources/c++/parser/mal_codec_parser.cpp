#include "mal_codec_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>
#include <iostream>
#include "../../utils/mal_string.hpp"
extern "C" {
#include "libavutil/mem.h"
#include "../c_ffi/mdp_type_def.h"
}
#include "mal_idatasource.h"
#include "mal_golomb.h"
#include "spdlog/spdlog.h"
using namespace mal;

static mdp_header_item * new_root_item() {
    mdp_header_item * item = (mdp_header_item *)av_mallocz(sizeof(mdp_header_item));
    item->nb_cells = 2;
    item->cells = (mdp_header_cell *)av_malloc_array(item->nb_cells,sizeof(mdp_header_cell));
    item->cells[0].display_type = MDPFieldDisplayType_string;
    item->cells[0].str_val = "root";
    return item;
}

static mdp_header_item * new_child_item(mdp_header_item *item) {
    if (item->nb_childs >= item->capacity) {
        item->childs = (mdp_header_item **)av_realloc(item->childs,(item->nb_childs + MDP_HEADER_DEFAULT_GROW_SIZE) * sizeof(void *));
        item->capacity = item->nb_childs + MDP_HEADER_DEFAULT_GROW_SIZE;
    }
    mdp_header_item * child = (mdp_header_item *)av_mallocz(sizeof(mdp_header_item));
    item->childs[item->nb_childs] = child;
    item->nb_childs ++;
    child->nb_cells = 2;
    child->cells = (mdp_header_cell *)av_malloc_array(child->nb_cells,sizeof(mdp_header_cell));
    return child;
}

static int append_bits_row(mdp_header_item *pitem, const std::string& name,int bits,std::shared_ptr<IDataSource> datasource, bool convert_rbsp = true) {
    mdp_header_item *item =  new_child_item(pitem);
    item->cells[0].display_type = MDPFieldDisplayType_string;
    std::string bits_name = fmt::format("{}({}bits)",name,bits);
    item->cells[0].str_val = av_strdup(bits_name.c_str());
    item->cells[0].enable = 1;
    int64_t val = datasource->readBitsInt64(bits,false,1, convert_rbsp);//mdp_data_buffer_read_bits_ival(dctx,buffer,bits,1,had_read_bits);
    item->cells[1].display_type = MDPFieldDisplayType_int64;
    item->cells[1].i_val = val;
    item->cells[1].enable = 1;
    return (int)val;
}

static int64_t append_value_row(mdp_header_item *pitem, const char *val1,int val) {
    mdp_header_item *item = new_child_item(pitem);
    item->cells[0].display_type = MDPFieldDisplayType_string;
    item->cells[0].str_val = av_strdup(val1);
    item->cells[0].enable = 1;
    item->cells[1].display_type = MDPFieldDisplayType_int64;
    item->cells[1].i_val = val;
    item->cells[1].enable = 1;
    return val;
}

static mdp_header_item* append_enable_row(mdp_header_item *pitem, const std::string& val1,int enable) {
    mdp_header_item *item = new_child_item(pitem);
    item->cells[0].display_type = MDPFieldDisplayType_string;
    item->cells[0].str_val = av_strdup(val1.c_str());
    item->cells[1].display_type = MDPFieldDisplayType_string;
    item->cells[1].str_val = "";
    item->cells[0].enable = enable;
    return item;
}
static std::string append_string_value_row(mdp_header_item *pitem, const char *val1,std::string& val2) {
    mdp_header_item *item = new_child_item(pitem);
    item->cells[0].display_type = MDPFieldDisplayType_string;
    item->cells[0].str_val = av_strdup(val1);
    item->cells[0].enable = 1;
    item->cells[1].display_type = MDPFieldDisplayType_string;
    item->cells[1].str_val = av_strdup(val2.c_str());;
    item->cells[1].enable = 1;
    return val2;
}
static void enable_or_disable_cell(mdp_header_item *item, int enable) {
    item->cells[0].enable = enable;
}
#define APPEND_BITS_ROW_VAR(name,bits) int name = append_bits_row(pitem,#name,bits, datasource_);
#define APPEND_BITS_ROW(name,bits) append_bits_row(pitem,name,bits, datasource_)
#define APPEND_BITS_ROW_NO_RBSP(name,bits) append_bits_row(pitem,name,bits, datasource_, false)
#define APPEND_ENABLE_ROW(name) append_enable_row(pitem,name,0)
#define APPEND_VALUE_ROW(name,val) append_value_row(pitem,name,val)

#define APPEND_GOLOMB_UE_ROW(name) int name = golomb_ue(datasource_); APPEND_VALUE_ROW(#name,name)
#define APPEND_GOLOMB_SE_ROW(name) int name = golomb_se(datasource_); APPEND_VALUE_ROW(#name,name)
#define APPEND_GOLOMB_UE_ROW_NO_VAR(name) APPEND_VALUE_ROW(#name,golomb_ue(datasource_))
#define APPEND_GOLOMB_SE_ROW_NO_VAR(name) APPEND_VALUE_ROW(#name,golomb_se(datasource_))
#define ENABLE_CELL(item) mdp_header_item *pitem = item; enable_or_disable_cell(pitem,1)

#define GOLOMB_UE(name) int name = golomb_ue(datasource_);
#define GOLOMB_SE(name) int name = golomb_se(datasource_);

void MALCodecParser::parse_avc_nal_header(mdp_header_item *item) {
    mdp_header_item * pitem = item;
    int forbidden_zero_bit = APPEND_BITS_ROW("forbidden_zero_bit",1);
    int nal_ref_idc = APPEND_BITS_ROW("nal_ref_idc",2);
    int nal_unit_type = APPEND_BITS_ROW("nal_unit_type",5);//5:idr 6:sei 7:sps 8:pps 9:aud
    if (nal_unit_type == 7) {
        currentAVCNal_ = std::make_shared<MALAVCSPS>();
    } else if (nal_unit_type == 8) {
        currentAVCNal_ = std::make_shared<MALAVCPPS>();
    } else if (nal_unit_type == 6) {
        currentAVCNal_ = std::make_shared<MALAVCSEI>();
    } else if (nal_unit_type == 1 || nal_unit_type == 5){
        currentAVCNal_ = std::make_shared<MALAVCSliceWithOutPartitioning>();
    } else if (nal_unit_type == 2) {
        currentAVCNal_ = std::make_shared<MALAVCSlicePartitionA>();
    } else if (nal_unit_type == 3) {
        currentAVCNal_ = std::make_shared<MALAVCSlicePartitionB>();
    } else if (nal_unit_type == 4) {
        currentAVCNal_ = std::make_shared<MALAVCSlicePartitionC>();
    } else {
        currentAVCNal_ = std::make_shared<MALAVCNal>();
    }
    currentAVCNal_->nal_unit_type = nal_unit_type;
    currentAVCNal_->nal_ref_idc = nal_ref_idc;
    mdp_header_item *citem = APPEND_ENABLE_ROW("if(nal_unit_type == 14 || nal_unit_type == 20 || nal_unit_type == 21 )");
    if(nal_unit_type == 14 || nal_unit_type == 20 || nal_unit_type == 21 ) {
        ENABLE_CELL(citem);
        int svc_extension_flag = 0;
        int avc_3d_extension_flag = 0;
        int row = 0;
        if(nal_unit_type != 21) {
            svc_extension_flag = APPEND_BITS_ROW("svc_extension_flag",1);
        } else {
            avc_3d_extension_flag = APPEND_BITS_ROW("avc_3d_extension_flag",1);
        }
        if(svc_extension_flag) {
            mdp_header_item *citem = APPEND_ENABLE_ROW("nal_unit_header_svc_extension");
            ENABLE_CELL(citem);
            const char* names[] = {
                "idr_flag",
                "priority_id",
                "no_inter_layer_pred_flag",
                "dependency_id",
                "quality_id",
                "temporal_id",
                "use_ref_base_pic_flag",
                "discardable_flag",
                "output_flag",
                "reserved_three_2bits"
            };
            int bits[] = {1,6,1,3,4,3,1,1,1,2};
            for(int i = 0; i < sizeof(names)/sizeof(names[0]); i++) {
                APPEND_BITS_ROW(names[i],bits[i]);
            }
        } else if(avc_3d_extension_flag) {
            mdp_header_item *citem = APPEND_ENABLE_ROW("nal_unit_header_3davc_extension");
            ENABLE_CELL(citem);
            const char* names[] = {
                "view_idx",
                "depth_flag",
                "non_idr_flag",
                "temporal_id",
                "anchor_pic_flag",
                "inter_view_flag"
            };
            int bits[] = {8,1,1,3,1,1};
            for(int i = 0; i < sizeof(names)/sizeof(names[0]); i++) {
                APPEND_BITS_ROW(names[i],bits[i]);
            }
            
        } else {
            mdp_header_item *citem =  APPEND_ENABLE_ROW("nal_unit_header_mvc_extension");
            ENABLE_CELL(citem);
            const char* names[] = {
                "non_idr_flag",
                "priority_id",
                "view_id",
                "temporal_id",
                "anchor_pic_flag",
                "inter_view_flag",
                "reserved_one_bit"
            };
            int bits[] = {1,6,10,3,1,1,1};
            for(int i = 0; i < sizeof(names); i++) {
                APPEND_BITS_ROW(names[i],bits[i]);
            }
        }
    }
}



void MALCodecParser::avc_hrd_parameters(mdp_header_item *item) {
    mdp_header_item *pitem = item;
    APPEND_GOLOMB_UE_ROW(cpb_cnt_minus1);
    int bit_rate_scale = APPEND_BITS_ROW("bit_rate_scale",4);
    int cpb_size_scale = APPEND_BITS_ROW("cpb_size_scale",4);
    int bit_rate_value_minus1[cpb_cnt_minus1+1];
    int cpb_size_value_minus1[cpb_cnt_minus1+1];
    int cbr_flag[cpb_cnt_minus1+1];
    for(int SchedSelIdx = 0; SchedSelIdx <= cpb_cnt_minus1; SchedSelIdx++ ) {
        APPEND_GOLOMB_UE_ROW(bit_rate_value_minus1_val);
        bit_rate_value_minus1[SchedSelIdx] =  bit_rate_value_minus1_val;
        std::string name = fmt::format("bit_rate_value_minus1[{}]", SchedSelIdx);
        APPEND_VALUE_ROW(name.c_str(),bit_rate_value_minus1[SchedSelIdx]);
        APPEND_GOLOMB_UE_ROW(cpb_size_value_minus1_val);
        cpb_size_value_minus1[SchedSelIdx] = cpb_size_value_minus1_val;
        name = fmt::format("cpb_size_value_minus1[{}]", SchedSelIdx);
        APPEND_VALUE_ROW(name.c_str(),cpb_size_value_minus1[ SchedSelIdx]);
        name = fmt::format("cbr_flag[{}]", SchedSelIdx);
        APPEND_BITS_ROW(name.c_str(), 1);
    }
    int initial_cpb_removal_delay_length_minus1 = APPEND_BITS_ROW("initial_cpb_removal_delay_length_minus1",5);
    int cpb_removal_delay_length_minus1 = APPEND_BITS_ROW("cpb_removal_delay_length_minus1",5);
    int dpb_output_delay_length_minus1 = APPEND_BITS_ROW("dpb_output_delay_length_minus1",5);
    int time_offset_length = APPEND_BITS_ROW("time_offset_length",5);
}

void MALCodecParser::vui_parameters(int hevc, mdp_header_item *item) {
    mdp_header_item *pitem = item;
    int aspect_ratio_info_present_flag = APPEND_BITS_ROW("aspect_ratio_info_present_flag",1);
    mdp_header_item *citem = APPEND_ENABLE_ROW("if(aspect_ratio_info_present_flag)");
    if(aspect_ratio_info_present_flag) {
        ENABLE_CELL(citem);
        int aspect_ratio_idc = APPEND_BITS_ROW("aspect_ratio_info_present_flag",8);
        //Extended_SAR
        mdp_header_item * ccitem = APPEND_ENABLE_ROW("if(aspect_ratio_idc == 255)");
        if(aspect_ratio_idc == 255) {
            enable_or_disable_cell(ccitem,1);
            pitem = ccitem;
            int sar_width = APPEND_BITS_ROW("sar_width",16);
            int sar_height = APPEND_BITS_ROW("sar_height",16);
        }
    }
    pitem = item;
    int overscan_info_present_flag = APPEND_BITS_ROW("overscan_info_present_flag",1);
    citem = APPEND_ENABLE_ROW("if(overscan_info_present_flag)");
    if(overscan_info_present_flag) {
        ENABLE_CELL(citem);
        int overscan_appropriate_flag = APPEND_BITS_ROW("overscan_appropriate_flag",1);
    }
    pitem = item;
    int video_signal_type_present_flag = APPEND_BITS_ROW("video_signal_type_present_flag",1);
    citem = APPEND_ENABLE_ROW("if(video_signal_type_present_flag)");
    if(video_signal_type_present_flag) {
        ENABLE_CELL(citem);
        int video_format = APPEND_BITS_ROW("video_format",3);
        int video_full_range_flag = APPEND_BITS_ROW("video_full_range_flag",1);
        int colour_description_present_flag = APPEND_BITS_ROW("colour_description_present_flag",1);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("if(colour_description_present_flag)");
        if(colour_description_present_flag) {
            enable_or_disable_cell(ccitem,1);
            pitem = ccitem;
            int colour_primaries = APPEND_BITS_ROW("colour_primaries",8);
            int transfer_characteristics = APPEND_BITS_ROW("transfer_characteristics",8);
            int matrix_coefficients = APPEND_BITS_ROW("matrix_coefficients",8);
        }
    }
    int chroma_loc_info_present_flag = APPEND_BITS_ROW("chroma_loc_info_present_flag",1);
    citem = APPEND_ENABLE_ROW("if(chroma_loc_info_present_flag)");
    if(chroma_loc_info_present_flag) {
        ENABLE_CELL(citem);
        APPEND_GOLOMB_UE_ROW(chroma_sample_loc_type_top_field);
        APPEND_GOLOMB_UE_ROW(chroma_sample_loc_type_bottom_field);
    }
    if(!hevc) {
        int timing_info_present_flag = APPEND_BITS_ROW("timing_info_present_flag",1);
        citem = APPEND_ENABLE_ROW("if(timing_info_present_flag)");
        if(timing_info_present_flag) {
            ENABLE_CELL(citem);
            int64_t num_units_in_tick = APPEND_BITS_ROW("num_units_in_tick",32);
            int64_t time_scale = APPEND_BITS_ROW("time_scale",32);
            int fixed_frame_rate_flag = APPEND_BITS_ROW("fixed_frame_rate_flag",1);
        }
        int nal_hrd_parameters_present_flag = APPEND_BITS_ROW("nal_hrd_parameters_present_flag",1);
        citem = APPEND_ENABLE_ROW("if(nal_hrd_parameters_present_flag)");
        if(nal_hrd_parameters_present_flag) {
            ENABLE_CELL(citem);
            mdp_header_item *ccitem = APPEND_ENABLE_ROW("hrd_parameters");
            enable_or_disable_cell(ccitem,1);
            avc_hrd_parameters(ccitem);
        }
        int vcl_hrd_parameters_present_flag = APPEND_BITS_ROW("vcl_hrd_parameters_present_flag",1);
        citem = APPEND_ENABLE_ROW("if(vcl_hrd_parameters_present_flag)");
        if(vcl_hrd_parameters_present_flag) {
            ENABLE_CELL(citem);
            mdp_header_item *ccitem = APPEND_ENABLE_ROW("hrd_parameters");
            enable_or_disable_cell(ccitem,1);
            avc_hrd_parameters(ccitem);
        }
        citem = APPEND_ENABLE_ROW("if( nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)");
        if( nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
            ENABLE_CELL(citem);
            int low_delay_hrd_flag = APPEND_BITS_ROW("low_delay_hrd_flag",1);
        }
        int pic_struct_present_flag = APPEND_BITS_ROW("pic_struct_present_flag",1);
        int bitstream_restriction_flag = APPEND_BITS_ROW("bitstream_restriction_flag",1);
        citem = APPEND_ENABLE_ROW("if(bitstream_restriction_flag)");
        if(bitstream_restriction_flag) {
            ENABLE_CELL(citem);
            int motion_vectors_over_pic_boundaries_flag = APPEND_BITS_ROW("motion_vectors_over_pic_boundaries_flag",1);
            APPEND_GOLOMB_UE_ROW(max_bytes_per_pic_denom);
            APPEND_GOLOMB_UE_ROW(max_bits_per_mb_denom);
            APPEND_GOLOMB_UE_ROW(log2_max_mv_length_horizontal);
            APPEND_GOLOMB_UE_ROW(log2_max_mv_length_vertical);
            APPEND_GOLOMB_UE_ROW(max_num_reorder_frames);
            APPEND_GOLOMB_UE_ROW(max_dec_frame_buffering);
        }
    } else {
        int neutral_chroma_indication_flag = APPEND_BITS_ROW("neutral_chroma_indication_flag",1);
        int field_seq_flag = APPEND_BITS_ROW("neutral_chroma_indication_flag",1);
        int frame_field_info_present_flag = APPEND_BITS_ROW("frame_field_info_present_flag",1);
        int default_display_window_flag = APPEND_BITS_ROW("default_display_window_flag",1);
        citem = APPEND_ENABLE_ROW("if( default_display_window_flag )");
        if( default_display_window_flag ) {
            ENABLE_CELL(citem);
            APPEND_GOLOMB_UE_ROW(def_disp_win_left_offset);
            APPEND_GOLOMB_UE_ROW(def_disp_win_right_offset);
            APPEND_GOLOMB_UE_ROW(def_disp_win_top_offset);
            APPEND_GOLOMB_UE_ROW(def_disp_win_bottom_offset);
        }
        int vui_timing_info_present_flag = APPEND_BITS_ROW("vui_timing_info_present_flag",1);
        citem = APPEND_ENABLE_ROW("if( vui_timing_info_present_flag )");
        if( vui_timing_info_present_flag ) {
            ENABLE_CELL(citem);
            int64_t vui_num_units_in_tick = APPEND_BITS_ROW("vui_num_units_in_tick",32);
            int64_t vui_time_scale = APPEND_BITS_ROW("vui_time_scale",32);
            int vui_poc_proportional_to_timing_flag = APPEND_BITS_ROW("vui_poc_proportional_to_timing_flag",1);
            mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( vui_poc_proportional_to_timing_flag )");
            if( vui_poc_proportional_to_timing_flag ){
                ENABLE_CELL(ccitem);
                APPEND_GOLOMB_UE_ROW(vui_num_ticks_poc_diff_one_minus1);
            }
            int vui_hrd_parameters_present_flag = APPEND_BITS_ROW("vui_hrd_parameters_present_flag",1);
            ccitem = APPEND_ENABLE_ROW("if( vui_hrd_parameters_present_flag )");
            if( vui_hrd_parameters_present_flag ) {
                ENABLE_CELL(ccitem);
                mdp_header_item *cccitem = APPEND_ENABLE_ROW("hrd_parameters");
                enable_or_disable_cell(cccitem,1);
                avc_hrd_parameters(cccitem);
            }
        }
        int bitstream_restriction_flag = APPEND_BITS_ROW("bitstream_restriction_flag",1);
        citem = APPEND_ENABLE_ROW("if( bitstream_restriction_flag )");
        if( bitstream_restriction_flag ) {
            ENABLE_CELL(citem);
            int tiles_fixed_structure_flag = APPEND_BITS_ROW("tiles_fixed_structure_flag",1);
            int motion_vectors_over_pic_boundaries_flag = APPEND_BITS_ROW("motion_vectors_over_pic_boundaries_flag",1);
            int restricted_ref_pic_lists_flag = APPEND_BITS_ROW("restricted_ref_pic_lists_flag",1);
            
            APPEND_GOLOMB_UE_ROW(min_spatial_segmentation_idc);
            APPEND_GOLOMB_UE_ROW(max_bytes_per_pic_denom);
            APPEND_GOLOMB_UE_ROW(max_bits_per_min_cu_denom);
            APPEND_GOLOMB_UE_ROW(log2_max_mv_length_horizontal);
            APPEND_GOLOMB_UE_ROW(log2_max_mv_length_vertical);
        }
        
    }
    
    
}
void MALCodecParser::scaling_list(int *scalingList,int sizeOfScalingList,int* useDefaultScalingMatrixFlag,mdp_header_item *item) {
    int lastScale = 8;
    int nextScale = 8;
    int row = 0;
    mdp_header_item *pitem = item;
    for(int j = 0; j < sizeOfScalingList; j++ ) {
        std::string name = fmt::format("if(nextScale[{}] != 0 )",j);
        mdp_header_item *citem = APPEND_ENABLE_ROW(name.c_str());
        if(nextScale != 0 ) {
            ENABLE_CELL(citem);
            APPEND_GOLOMB_UE_ROW(delta_scale);
            nextScale = ( lastScale + delta_scale + 256 ) % 256;
            *useDefaultScalingMatrixFlag = ( j == 0 && nextScale == 0 );
        }
        scalingList[j] = ( nextScale == 0 ) ? lastScale : nextScale;
        lastScale = scalingList[j];
    }
}
int MALCodecParser::parse_avc_sps(mdp_video_header *header,mdp_header_item *item) {
    int64_t read_bits = 0;
    mdp_header_item *pitem = item;
    int profile_idc = APPEND_BITS_ROW("profile_idc",8);
    auto avcsps = std::dynamic_pointer_cast<MALAVCSPS>(currentAVCNal_);
    if (avcc_ && !curPkt_) {
        avcc_->spsList.push_back(avcsps);
    }
    avcsps->profile_idc = profile_idc;
    // if(mediainfo_->currentTrack) {
    //     if(profile_idc == 66) {
    //         mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("profile","baseline"));
    //     } else if(profile_idc == 77) {
    //         mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("profile","main"));
    //     } else if(profile_idc == 100) {
    //         mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("profile","high"));
    //     } else if(profile_idc == 110) {
    //         mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("profile","high10"));
    //     } else if(profile_idc == 122) {
    //         mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("profile","high422"));
    //     } else if(profile_idc == 144) {
    //         mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("profile","high444"));
    //     }
    // }
    
    int constraint_set0_flag = APPEND_BITS_ROW("constraint_set0_flag",1);
    int constraint_set1_flag = APPEND_BITS_ROW("constraint_set1_flag",1);
    int constraint_set2_flag = APPEND_BITS_ROW("constraint_set2_flag",1);
    int constraint_set3_flag = APPEND_BITS_ROW("constraint_set3_flag",1);
    int constraint_set4_flag = APPEND_BITS_ROW("constraint_set4_flag",1);
    int constraint_set5_flag = APPEND_BITS_ROW("constraint_set5_flag",1);
    int reserved_zero_2bits = APPEND_BITS_ROW("reserved_zero_2bits",2);
    int level_idc = APPEND_BITS_ROW("level_idc",8);
    avcsps->level_idc = level_idc;
    APPEND_GOLOMB_UE_ROW(seq_parameter_set_id);
    avcsps->seq_parameter_set_id = seq_parameter_set_id;
    mdp_header_item *citem = APPEND_ENABLE_ROW("if( profile_idc == 100 || profile_idc == 110 || \
            profile_idc == 122 || profile_idc == 244 || \
            profile_idc == 44 || profile_idc == 83 || \
            profile_idc == 86 || profile_idc == 118 || \
            profile_idc == 128 || profile_idc == 138 || \
            profile_idc == 139 || profile_idc == 134 || profile_idc == 135 )");
    if( profile_idc == 100 || profile_idc == 110 ||
       profile_idc == 122 || profile_idc == 244 ||
       profile_idc == 44 || profile_idc == 83 ||
       profile_idc == 86 || profile_idc == 118 ||
       profile_idc == 128 || profile_idc == 138 ||
       profile_idc == 139 || profile_idc == 134 || profile_idc == 135 ) {
        ENABLE_CELL(citem);
        APPEND_GOLOMB_UE_ROW(chroma_format_idc);
        avcsps->chroma_format_idc = chroma_format_idc;
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("if(chroma_format_idc == 3)");
        if(chroma_format_idc == 3) {
            ENABLE_CELL(ccitem);
            int separate_colour_plane_flag = APPEND_BITS_ROW("separate_colour_plane_flag",1);
            avcsps->separate_colour_plane_flag = separate_colour_plane_flag;
        }
        APPEND_GOLOMB_UE_ROW(bit_depth_luma_minus8);
        avcsps->bit_depth_luma_minus8 = bit_depth_luma_minus8;
        APPEND_GOLOMB_UE_ROW(bit_depth_chroma_minus8);
        avcsps->bit_depth_chroma_minus8 = bit_depth_chroma_minus8;
        
        APPEND_BITS_ROW("qpprime_y_zero_transform_bypass_flag",1);
        int seq_scaling_matrix_present_flag = APPEND_BITS_ROW("seq_scaling_matrix_present_flag",1);
        ccitem = APPEND_ENABLE_ROW("if(seq_scaling_matrix_present_flag == 1)");
        if(seq_scaling_matrix_present_flag == 1) {
            enable_or_disable_cell(ccitem,1);
            pitem = ccitem;
            int size = ((chroma_format_idc != 3) ? 8 : 12);
            int ScalingList4x4[size][16];
            int UseDefaultScalingMatrix4x4Flag[size];
            int ScalingList8x8[size][64];
            int UseDefaultScalingMatrix8x8Flag[size];
            for(int i = 0; i < size; i++) {
                int seq_scaling_list_present_flag = datasource_->readBitsInt64(1);//mdp_data_buffer_read_bits_ival(dctx,buffer,1,1,&read_bits);
                std::string name = fmt::format("seq_scaling_list_present_flag[{}]",i);
                APPEND_VALUE_ROW(name.c_str(),seq_scaling_list_present_flag);
                name = fmt::format("if(seq_scaling_list_present_flag[{}])",i);
                mdp_header_item * cccitem = APPEND_ENABLE_ROW(name.c_str());
                if(seq_scaling_list_present_flag) {
                    ENABLE_CELL(cccitem);
                    if(i<6) {
                        scaling_list(ScalingList4x4[i],16,&UseDefaultScalingMatrix4x4Flag[i],cccitem);
                    } else {
                        scaling_list(ScalingList8x8[i-6],64,&UseDefaultScalingMatrix8x8Flag[i],cccitem);
                    }
                    
                }
                
            }
        }
        
    }
    APPEND_GOLOMB_UE_ROW(log2_max_frame_num_minus4);
    avcsps->log2_max_frame_num_minus4 = log2_max_frame_num_minus4;
    APPEND_GOLOMB_UE_ROW(pic_order_cnt_type);
    avcsps->pic_order_cnt_type = pic_order_cnt_type;
    citem = APPEND_ENABLE_ROW("if(pic_order_cnt_type == 0)");
    if(pic_order_cnt_type == 0) {
        ENABLE_CELL(citem);
        APPEND_GOLOMB_UE_ROW(log2_max_pic_order_cnt_lsb_minus4);
        avcsps->log2_max_pic_order_cnt_lsb_minus4 = log2_max_pic_order_cnt_lsb_minus4;
    } else if(pic_order_cnt_type == 1){
        citem = APPEND_ENABLE_ROW("if(pic_order_cnt_type == 1)");
        ENABLE_CELL(citem);
        APPEND_BITS_ROW_VAR(delta_pic_order_always_zero_flag,1);
        avcsps->delta_pic_order_always_zero_flag = delta_pic_order_always_zero_flag;
        APPEND_GOLOMB_SE_ROW(offset_for_non_ref_pic);
        avcsps->offset_for_non_ref_pic = offset_for_non_ref_pic;
        APPEND_GOLOMB_SE_ROW(offset_for_top_to_bottom_field);
        avcsps->offset_for_top_to_bottom_field = offset_for_top_to_bottom_field;
        APPEND_GOLOMB_UE_ROW(num_ref_frames_in_pic_order_cnt_cycle);
        avcsps->num_ref_frames_in_pic_order_cnt_cycle = num_ref_frames_in_pic_order_cnt_cycle;
        {
            citem = APPEND_ENABLE_ROW("for( i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )");
            for( int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ ) {
                ENABLE_CELL(citem);
                int tmp = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("offset_for_ref_frame[{}]",i));
                avcsps->offset_for_ref_frame.push_back(tmp);
            }
        }
    }
    APPEND_GOLOMB_UE_ROW(max_num_ref_frames);
    avcsps->max_num_ref_frames = max_num_ref_frames;
    APPEND_BITS_ROW("gaps_in_frame_num_value_allowed_flag",1);
    
    APPEND_GOLOMB_UE_ROW(pic_width_in_mbs_minus1);
    avcsps->pic_width_in_mbs_minus1 = pic_width_in_mbs_minus1;
    
    APPEND_GOLOMB_UE_ROW(pic_height_in_map_units_minus1);
    avcsps->pic_height_in_map_units_minus1 = pic_height_in_map_units_minus1;
    
    // if(mediainfo_->currentTrack) {
    //     if(mediainfo_->currentTrack->width <= 0) {
    //         mediainfo_->currentTrack->width = 16 *(pic_width_in_mbs_minus1 + 1);
    //     }
    //     if(mediainfo_->currentTrack->height <= 0) {
    //         mediainfo_->currentTrack->height = 16 *(pic_height_in_map_units_minus1 + 1);
    //     }
    // }
    int frame_mbs_only_flag = APPEND_BITS_ROW("frame_mbs_only_flag",1);
    avcsps->frame_mbs_only_flag = frame_mbs_only_flag;
    
    // mediainfo_->h264_sps.frame_mbs_only_flag = frame_mbs_only_flag;
    // if(mediainfo_->currentTrack) {
    //     if(frame_mbs_only_flag) {
    //         mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("interlace","no(逐行解码)"));
    //     } else {
    //          mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("interlace","yes"));
    //     }
    
    // }
    
    citem = APPEND_ENABLE_ROW("if(frame_mbs_only_flag == 0");
    if(frame_mbs_only_flag == 0) {
        ENABLE_CELL(citem);
        int mb_adaptive_frame_field_flag = APPEND_BITS_ROW("mb_adaptive_frame_field_flag",1);
        avcsps->mb_adaptive_frame_field_flag = mb_adaptive_frame_field_flag;
    }
    int direct_8x8_inference_flag = APPEND_BITS_ROW("direct_8x8_inference_flag",1);
    int frame_cropping_flag = APPEND_BITS_ROW("frame_cropping_flag",1);
    avcsps->frame_cropping_flag = frame_cropping_flag;
    citem = APPEND_ENABLE_ROW("if(frame_cropping_flag != 0)");
    if(frame_cropping_flag != 0) {
        ENABLE_CELL(citem);
        APPEND_GOLOMB_UE_ROW(frame_crop_left_offset);
        APPEND_GOLOMB_UE_ROW(frame_crop_right_offset);
        APPEND_GOLOMB_UE_ROW(frame_crop_top_offset);
        APPEND_GOLOMB_UE_ROW(frame_crop_bottom_offset);
    }
    int vui_parameters_present_flag = APPEND_BITS_ROW("vui_parameters_present_flag",1);
    citem = APPEND_ENABLE_ROW("if(vui_parameters_present_flag != 0)");
    if(vui_parameters_present_flag != 0) {
        ENABLE_CELL(citem);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("vui_parameters");
        enable_or_disable_cell(ccitem,1);
        vui_parameters(0,ccitem);
    }
    return read_bits;
}
int MALCodecParser::more_rbsp_data() {
    int left = datasource_->totalSize() * 8 - datasource_->currentBitsPosition();
    if(left > 8) {
        return 1;
    }
    if(left <= 0) {
        return 0;
    }
    int value = 0;
    if (left == 8) {
        value = datasource_->readBytesInt64(1,true);//mdp_data_buffer_read8(dctx,buffer);
    } else {
//        unsigned char *temp = datasource_->current();
//        value = mdp_strconvet_to_int((char *)temp, 1, 1);//buffer->read_buffer_8[0];
        value = datasource_->readBitsInt64(left,true);
    }
//    value = mdp_char_reverse(value);
//    int i ;
//    for(i = 7; i>= 0; i--) {
//        if(mdp_get_bit_from_byte(value,i) == 1) break;
//    }
//    if(i>0) return 1;
//    return 0;
    if (value == 0x01 || value == (0x01 << 1) || value == (0x01 << 2) || value == (0x01 << 3) || value == (0x01 << 4) || value == (0x01 << 5) || value == (0x01 << 6) || value == (0x01 << 7) || value == (0x01 << 8)) return 0;
    return 1;
}
void MALCodecParser::rbsp_trailing_bits(mdp_header_item *item) {
    mdp_header_item *pitem = item;
    mdp_header_item *citem = APPEND_ENABLE_ROW("rbsp_trailing_bits");
    enable_or_disable_cell(citem,1);
    pitem = citem;
    int rbsp_stop_one_bit = APPEND_BITS_ROW("rbsp_stop_one_bit",1);
    int index = 0;
    citem = APPEND_ENABLE_ROW("while(offset % 8 != 0)");
    while(datasource_->currentBitsPosition() % 8 != 0) {
        ENABLE_CELL(citem);
        int rbsp_alignment_zero_bit = APPEND_BITS_ROW(fmt::format("rbsp_alignment_zero_bit[{}]",index),1);
        index ++;
    }
}
int MALCodecParser::parse_avc_pps(mdp_video_header *header, mdp_header_item *item) {
    int64_t read_bits = 0;
    int64_t * had_read_bits = &read_bits;
    auto avcpps = std::dynamic_pointer_cast<MALAVCPPS>(currentAVCNal_);
    if (avcc_) {
        avcc_->ppsList.push_back(avcpps);
    }
    mdp_header_item *pitem = item;
    APPEND_GOLOMB_UE_ROW(pic_parameter_set_id);
    avcpps->pic_parameter_set_id = pic_parameter_set_id;
    APPEND_GOLOMB_UE_ROW(seq_parameter_set_id);
    avcpps->seq_parameter_set_id = seq_parameter_set_id;
    
    int entropy_coding_mode_flag = APPEND_BITS_ROW("entropy_coding_mode_flag",1);
    avcpps->entropy_coding_mode_flag = entropy_coding_mode_flag;
    // if(mediainfo_->currentTrack) {
    //     mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("coding mode",entropy_coding_mode_flag ? "CABAC" : "CAVLC"));
    // }
    
    int bottom_field_pic_order_in_frame_present_flag = APPEND_BITS_ROW("bottom_field_pic_order_in_frame_present_flag",1);
    avcpps->bottom_field_pic_order_in_frame_present_flag = bottom_field_pic_order_in_frame_present_flag;
    APPEND_GOLOMB_UE_ROW(num_slice_groups_minus1);
    avcpps->num_slice_groups_minus1 = num_slice_groups_minus1;
    mdp_header_item *citem = APPEND_ENABLE_ROW("if( num_slice_groups_minus1 > 0 ) ");
    if(num_slice_groups_minus1 > 0) {
        ENABLE_CELL(citem);
        APPEND_GOLOMB_UE_ROW(slice_group_map_type);
        avcpps->slice_group_map_type = slice_group_map_type;
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( slice_group_map_type = = 0 )");
        if(slice_group_map_type == 0) {
            ENABLE_CELL(ccitem);
            for( int iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ ) {
                std::string name = fmt::format("run_length_minus1[{}]",iGroup);
                int run_length_minus1 = golomb_ue(datasource_);
                APPEND_VALUE_ROW(name.c_str(),run_length_minus1);
            }
        }
        ccitem = APPEND_ENABLE_ROW("else if( slice_group_map_type = = 2 )");
        if(slice_group_map_type == 2) {
            ENABLE_CELL(ccitem);
            for( int iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ ) {
                std::string name = fmt::format("top_left[{}]",iGroup);
                int top_left = golomb_ue(datasource_);
                APPEND_VALUE_ROW(name.c_str(),top_left);
                name = fmt::format("bottom_right[{}]",iGroup);
                int bottom_right = golomb_ue(datasource_);
                APPEND_VALUE_ROW(name.c_str(),top_left);
            }
        }
        ccitem = APPEND_ENABLE_ROW("else if(slice_group_map_type == 3 || slice_group_map_type == 4 || slice_group_map_type == 5)");
        if(slice_group_map_type == 3 || slice_group_map_type == 4 || slice_group_map_type == 5) {
            ENABLE_CELL(ccitem);
            int slice_group_change_direction_flag = APPEND_BITS_ROW("slice_group_change_direction_flag",1);
            APPEND_GOLOMB_UE_ROW(slice_group_change_rate_minus1);
        }
        ccitem = APPEND_ENABLE_ROW("else if( slice_group_map_type = = 6 ) ");
        if(slice_group_map_type == 6) {
            ENABLE_CELL(ccitem);
            APPEND_GOLOMB_UE_ROW(pic_size_in_map_units_minus1);
            avcpps->pic_size_in_map_units_minus1 = pic_size_in_map_units_minus1;
            /*
             * slice_group_id[ i ] identifies a slice group of the i-th slice group map unit in raster scan order.
             * The length of the slice_group_id[ i ] syntax element is Ceil( Log2( num_slice_groups_minus1 + 1 ) ) bits.
             * The value of slice_group_id[ i ] shall be in the range of 0 to num_slice_groups_minus1, inclusive.
             */
            int slice_group_id_bits = ceil(log2(num_slice_groups_minus1 + 1));
            for( int iGroup = 0; iGroup <= pic_size_in_map_units_minus1; iGroup++ ) {
                std::string name = fmt::format("slice_group_id[{}]",iGroup);
                int slice_group_id = APPEND_BITS_ROW(name.c_str(),slice_group_id_bits);
            }
        }
    }
    APPEND_GOLOMB_UE_ROW(num_ref_idx_l0_default_active_minus1);
    APPEND_GOLOMB_UE_ROW(num_ref_idx_l1_default_active_minus1);
    int weighted_pred_flag = APPEND_BITS_ROW("weighted_pred_flag",1);
    int weighted_bipred_idc = APPEND_BITS_ROW("weighted_bipred_idc",2);
    avcpps->weighted_pred_flag = weighted_pred_flag;
    avcpps->weighted_bipred_idc = weighted_bipred_idc;
    APPEND_GOLOMB_SE_ROW(pic_init_qp_minus26);
    APPEND_GOLOMB_SE_ROW(pic_init_qs_minus26);
    APPEND_GOLOMB_SE_ROW(chroma_qp_index_offset);
    
    
    int deblocking_filter_control_present_flag = APPEND_BITS_ROW("deblocking_filter_control_present_flag",1);
    avcpps->deblocking_filter_control_present_flag = deblocking_filter_control_present_flag;
    int constrained_intra_pred_flag = APPEND_BITS_ROW("constrained_intra_pred_flag",1);
    
    int redundant_pic_cnt_present_flag = APPEND_BITS_ROW("redundant_pic_cnt_present_flag",1);
    avcpps->redundant_pic_cnt_present_flag = redundant_pic_cnt_present_flag;
    citem = APPEND_ENABLE_ROW("if( more_rbsp_data( ) )");
    if(more_rbsp_data()) {
        ENABLE_CELL(citem);
        int transform_8x8_mode_flag = APPEND_BITS_ROW("transform_8x8_mode_flag",1);
        int pic_scaling_matrix_present_flag = APPEND_BITS_ROW("pic_scaling_matrix_present_flag",1);
        
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( pic_scaling_matrix_present_flag )");
        if(pic_scaling_matrix_present_flag) {
            ENABLE_CELL(ccitem);
            int size = 6 + ( (header->sps.chroma_format_idc != 3 ) ? 2 : 6 ) * transform_8x8_mode_flag;
            int ScalingList4x4[size][16];
            int ScalingList8x8[size][64];
            int UseDefaultScalingMatrix4x4Flag[16];
            int UseDefaultScalingMatrix8x8Flag[64];
            for(int i = 0; i < size; i++ ) {
                std::string name = fmt::format("pic_scaling_list_present_flag[{}]",i);
                int pic_scaling_list_present_flag = APPEND_BITS_ROW(name.c_str(),1);
                mdp_header_item *cccitem = APPEND_ENABLE_ROW("if( pic_scaling_list_present_flag)");
                if(pic_scaling_list_present_flag) {
                    ENABLE_CELL(cccitem);
                    mdp_header_item *ccccitem = APPEND_ENABLE_ROW("if( i < 6)");
                    if(i < 6) {
                        ENABLE_CELL(ccccitem);
                        scaling_list(ScalingList4x4[i],16,&UseDefaultScalingMatrix4x4Flag[i],pitem);
                    } else {
                        ccccitem = APPEND_ENABLE_ROW("else");
                        ENABLE_CELL(ccccitem);
                        scaling_list(ScalingList8x8[i-6],64,&UseDefaultScalingMatrix8x8Flag[i-6],pitem);
                    }
                }
            }
            
        }
        APPEND_GOLOMB_SE_ROW(second_chroma_qp_index_offset);
    }
    rbsp_trailing_bits(pitem);
    return read_bits;
}
static void reverseOneBytesBitset(std::bitset<8> &set) {
    int a1 = set[0];
    int a2 = set[1];
    int a3 = set[2];
    int a4 = set[3];
    set[0] = set[7];
    set[1] = set[6];
    set[2] = set[5];
    set[3] = set[4];
    set[4] = a4;
    set[5] = a3;
    set[6] = a2;
    set[7] = a1;
}
static int c_more_rbsp_data(uint8_t *data,int64_t &offset,int64_t total_size) {
    int left = total_size - offset;
    if(left > 8) {
        return 1;
    }
    if(left <= 0) {
        return 0;
    }
    uint8_t *last_data = data + total_size/8 - 1;
    int value = last_data[0];
    std::bitset<8> bits(value); //每一位的话，会得到小端数据，刚好相反
    reverseOneBytesBitset(bits);
    int i ;
    for(i = 7; i>= 0; i--) {
        if(bits[i] == 1) break;
    }
    if(i>0) return 1;
    return 0;
}
std::vector<MALSEIMessage> MALCodecParser::parse_avc_sei(mdp_video_header *header, mdp_header_item *pitem, int64_t size) {
    std::vector<MALSEIMessage> list = {};
    unsigned char  *data = datasource_->current();
    unsigned char  *tmp_data = data;
    int payloadType = 0;
    std::vector<std::string> result = {};
    int64_t hadReadBytes = 0;
    do {
        payloadType = 0;
        while ((tmp_data)[0] == 0xff) {
            payloadType += 255;
            tmp_data++;
        }
        int last_payload_type_byte = tmp_data[0];
        tmp_data++;
        payloadType += last_payload_type_byte;
        int payloadSize = 0;
        while (tmp_data[0] == 0xff) {
            payloadSize += 255;
            tmp_data++;
        }
        int last_payload_size_byte = tmp_data[0];
        tmp_data++;
        payloadSize += last_payload_size_byte;
        MALSEIMessage msg = {};
        msg.payloadType = payloadType;
        msg.payloadSize = payloadSize;
        if(payloadType == 5) { //SEI_TYPE_USER_DATA_UNREGISTERED = 5,
            if(payloadSize < 17) {
                //                MLOGME("sei info length must larger 16!\n");
                malFmtCtx_->shallowCheck.warning.push_back("sei info length must larger 16!");
            } else {
               
//                char key[16] = {};
//                memcpy(key, tmp_data, 16);
                msg.key = mal_buffer_to_hexstr((uint8_t *)tmp_data, 16);;//std::string(key);
                msg.value = mal_buffer_to_hexstr((uint8_t *)tmp_data+16, payloadSize-16);
//                std::string str = std::string((char*)tmp_data+16,payloadSize-17); //减去17一般后边有个空格
//                std::string str = mal_buffer_to_hexstr(uint8_t *buffer, int size)
//                append_string_value_row(pitem, "sei", str);
//                result.push_back(str);
                
            }
        } else {
            msg.value = mal_buffer_to_hexstr((uint8_t *)tmp_data, payloadSize);
        }
        list.push_back(msg);
        tmp_data+=payloadSize;
        hadReadBytes = (tmp_data- data)*8;
    }while(c_more_rbsp_data(data,hadReadBytes,size * 8));
    if (currentAVCNal_) {
        auto nal = std::dynamic_pointer_cast<MALAVCSEI>(currentAVCNal_);
        if (nal) {
            nal->messageList = list;
        }
    } else if (currentHEVCNal_) {
        auto nal = std::dynamic_pointer_cast<MALHEVCSEI>(currentHEVCNal_);
        if (nal) {
            nal->messageList = list;
        }
    }
    return list;
}
mdp_video_header * MALCodecParser::parse_avcc() {
    mdp_video_header *header = (mdp_video_header *)av_mallocz(sizeof(mdp_video_header));
    mdp_header_item *pitem = new_root_item();//(mdp_header_item *)av_mallocz(sizeof(mdp_header_item));
    header->root_item = pitem;
    int numOfSequenceParameterSets = 0;
    int profile_idc = 0;
    int lengthSizeMinusOne = 0;
    const char * field_names[] = {"configurationVersion","AVCProfileIndication",
        "profile_compatibility","AVCLevelIndication",
        "reserved","lengthSizeMinusOne",
        "reserved","numOfSequenceParameterSets"
    };
    int field_sizes[] = {8,8,
        8,8,
        6,2,
        3,5
    };
    for (int i = 0; i < sizeof(field_sizes)/sizeof(int); i++) {
        const char *field_name = field_names[i];
        int field_size = field_sizes[i];
        int64_t val = APPEND_BITS_ROW(field_name,field_size);
        if (!strcmp("AVCProfileIndication",field_name)) {
            profile_idc = val;
        }
        if (!strcmp("numOfSequenceParameterSets",field_name)) {
            numOfSequenceParameterSets = val;
        }
        if (!strcmp("lengthSizeMinusOne",field_name)) {
            lengthSizeMinusOne = val;
            if (avcc_) {
                avcc_->lengthSizeMinusOne = lengthSizeMinusOne;
            }
            
        }
    }
    for(int i = 0; i < numOfSequenceParameterSets; i++) {
        std::string name = fmt::format("sps[{}]", i);
        mdp_header_item *item = APPEND_ENABLE_ROW(name.c_str());
        enable_or_disable_cell(item,1);
        uint64_t sequenceParameterSetLength = datasource_->readBytesInt64(2);
        int64_t next = datasource_->currentBytesPosition() + sequenceParameterSetLength;
        parse_avc_nal_header(item);
        parse_avc_sps(header,item);
        datasource_->seekBytes(next,SEEK_SET);
    }
    uint64_t val = datasource_->readBytesInt64(1);;//mdp_data_buffer_read_bits_ival(dctx,buffer,8,0,&had_read_bits);
    uint64_t numOfPictureParameterSets = val;
    for(int i = 0; i < numOfPictureParameterSets; i++) {
        std::string name = fmt::format("pps[{}]", i);
        mdp_header_item *item = APPEND_ENABLE_ROW(name.c_str());
        enable_or_disable_cell(item,1);
        uint64_t pictureParameterSetLength = datasource_->readBytesInt64(2);
        int64_t next = datasource_->currentBytesPosition() + pictureParameterSetLength;
        parse_avc_nal_header(item);
        int64_t read_bits = parse_avc_pps(header,item);
        datasource_->seekBytes(next, SEEK_SET);
    }
    // if ((had_read_bits < atom->size * 8) && (profile_idc  ==  100  ||  profile_idc  ==  110  ||
    //     profile_idc  ==  122  ||  profile_idc  ==  144)) {
    //     mdp_atom_write_field(atom,dctx,atom->buffer,6,"reserved(6bits)",1,MDPFieldDisplayType_int,NULL,0, &had_read_bits);
    //     mdp_atom_write_field(atom,dctx,atom->buffer,2,"chroma_format(2bits)",1,MDPFieldDisplayType_int,NULL,0, &had_read_bits);
    //     mdp_atom_write_field(atom,dctx,atom->buffer,5,"reserved(5bits)",1,MDPFieldDisplayType_int,NULL,0, &had_read_bits);
    //     mdp_atom_write_field(atom,dctx,atom->buffer,3,"bit_depth_luma_minus8(3bits)",1,MDPFieldDisplayType_int,NULL,0, &had_read_bits);
    //     mdp_atom_write_field(atom,dctx,atom->buffer,5,"reserved(5bits)",1,MDPFieldDisplayType_int,NULL,0, &had_read_bits);
    //     mdp_atom_write_field(atom,dctx,atom->buffer,3,"bit_depth_chroma_minus8(3bits)",1,MDPFieldDisplayType_int,NULL,0, &had_read_bits);
    //     int numOfSequenceParameterSetExt = mdp_atom_write_field(atom,dctx,atom->buffer,8,"numOfSequenceParameterSetExt(8bits)",1,MDPFieldDisplayType_int,NULL,0, &had_read_bits)->i_val;
    //     for(int i = 0; i < numOfSequenceParameterSetExt; i++) {
    //         snprintf(name,90,"sequenceParameterSetExtLength[%d](%dbits)",i,16);
    //         MDPAtomField *field = mdp_atom_write_field(atom,dctx,atom->buffer,16,name,1,MDPFieldDisplayType_int,NULL,0, &had_read_bits);
    //         memset(name,0,sizeof(name));
    //         snprintf(name,90,"sequenceParameterSetExtNALUnit[%d](%dbits)",i,field->i_val * 8);
    //         mdp_atom_write_field(atom,dctx,atom->buffer,field->i_val * 8,name,1,MDPFieldDisplayType_hex,NULL,0, &had_read_bits);
    //     }
    // }
    return header;
}
void MALCodecParser::parse_hvc_nal_header(mdp_header_item *item) {
    mdp_header_item * pitem = item;
    int forbidden_zero_bit = APPEND_BITS_ROW("forbidden_zero_bit",1);
    int nal_unit_type = APPEND_BITS_ROW("nal_unit_type",6);
    int nuh_layer_id = APPEND_BITS_ROW("nuh_layer_id",6);
    int nuh_temporal_id_plus1 = APPEND_BITS_ROW("nuh_temporal_id_plus1",3);
    if (nal_unit_type == 32) {
        currentHEVCNal_ = std::make_shared<MALHEVCVPS>();
    } else if (nal_unit_type == 33) {
        currentHEVCNal_ = std::make_shared<MALHEVCSPS>();
    } else if (nal_unit_type == 34) {
        currentHEVCNal_ = std::make_shared<MALHEVCPPS>();
    } else if ((nal_unit_type >= 0 && nal_unit_type <= 9) || (nal_unit_type >= 16 && nal_unit_type <= 21) ) {
        currentHEVCNal_ = std::make_shared<MALHEVCSliceSegmentLayerRbsp>();
    } else if (nal_unit_type == 39 || nal_unit_type == 40) {
        currentHEVCNal_ = std::make_shared<MALHEVCSEI>();
    } else  {
        currentHEVCNal_ = std::make_shared<MALHEVCNal>();
    }
    currentHEVCNal_->nal_unit_type = nal_unit_type;
    currentHEVCNal_->forbidden_zero_bit = forbidden_zero_bit;
    currentHEVCNal_->nuh_layer_id = nuh_layer_id;
    currentHEVCNal_->nuh_temporal_id_plus1 = nuh_temporal_id_plus1;
}

void MALCodecParser::hevc_scaling_list_data(mdp_header_item *pitem) {
    mdp_header_item *citem = APPEND_ENABLE_ROW("for( sizeId = 0; sizeId < 4; sizeId++ )");
    enable_or_disable_cell(citem, 1);
    for( int sizeId = 0; sizeId < 4; sizeId++ ) {
        ENABLE_CELL(citem);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("for( matrixId = 0; matrixId < 6; matrixId += ( sizeId = = 3 ) ? 3 : 1 )");
        for(int matrixId = 0; matrixId < 6; matrixId += ( sizeId == 3 ) ? 3 : 1 ) {
            ENABLE_CELL(ccitem);
            int scaling_list_pred_mode_flag = APPEND_BITS_ROW(fmt::format("scaling_list_pred_mode_flag[{}][{}]",sizeId,matrixId),1);
            mdp_header_item *cccitem = APPEND_ENABLE_ROW("if( !scaling_list_pred_mode_flag[ sizeId ][ matrixId ] )");
            if( !scaling_list_pred_mode_flag) {
                ENABLE_CELL(cccitem);
                APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("scaling_list_pred_matrix_id_delta[{}][{}]",sizeId,matrixId));
            } else {
                cccitem = APPEND_ENABLE_ROW("else");
                ENABLE_CELL(cccitem);
                int nextCoef = 8;
                int coefNum=std::min(64,(1 << (4+(sizeId << 1))));
                mdp_header_item *ccccitem = APPEND_ENABLE_ROW("if(sizeId > 1)");
                if(sizeId > 1) {
                    ENABLE_CELL(ccccitem);
                    int64_t scaling_list_dc_coef_minus8 = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("scaling_list_dc_coef_minus8[{}][{}]",sizeId-2,matrixId));
                    nextCoef = scaling_list_dc_coef_minus8 + 8;
                }
                ccccitem = APPEND_ENABLE_ROW("for( i = 0; i < coefNum; i++)");
                for(int i = 0; i < coefNum; i++) {
                    ENABLE_CELL(ccccitem);
                    int64_t scaling_list_delta_coef = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("scaling_list_delta_coef[{}]",i));
                    nextCoef = ( nextCoef + scaling_list_delta_coef + 256 ) % 256;
                }
            }
        }
    }
}
void MALCodecParser::hevc_st_ref_pic_set(mdp_header_item *pitem, int stRpsIdx,int num_short_term_ref_pic_sets,int num_delta_pocs[64], int num_negative_pics_list[64], int num_positive_pics_list[64]){
    num_delta_pocs[stRpsIdx] = num_negative_pics_list[stRpsIdx] + num_positive_pics_list[stRpsIdx];
    mdp_header_item *citem = APPEND_ENABLE_ROW("if( stRpsIdx != 0 )");
    int inter_ref_pic_set_prediction_flag = 0;
    if( stRpsIdx != 0 ) {
        ENABLE_CELL(citem);
        inter_ref_pic_set_prediction_flag = APPEND_BITS_ROW("inter_ref_pic_set_prediction_flag", 1);
    }
    citem = APPEND_ENABLE_ROW("if( inter_ref_pic_set_prediction_flag ) ");
    if(inter_ref_pic_set_prediction_flag) {
        num_delta_pocs[stRpsIdx] = 0;
        ENABLE_CELL(citem);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( stRpsIdx = = num_short_term_ref_pic_sets )");
        int delta_idx_minus1 = 0;
        if( stRpsIdx == num_short_term_ref_pic_sets ) {
            ENABLE_CELL(ccitem);
            delta_idx_minus1 = APPEND_GOLOMB_UE_ROW_NO_VAR("delta_idx_minus1");
        }
        int delta_rps_sign = APPEND_BITS_ROW("delta_rps_sign", 1);
        APPEND_GOLOMB_UE_ROW(abs_delta_rps_minus1);
        ccitem = APPEND_ENABLE_ROW("for( j = 0; j <= NumDeltaPocs[ RefRpsIdx ]; j++ )");
        for( int j = 0; j <= num_delta_pocs[stRpsIdx - 1]; j++ ) {
            ENABLE_CELL(ccitem);
            int used_by_curr_pic_flag = APPEND_BITS_ROW(fmt::format("used_by_curr_pic_flag[{}]",j), 1);
            mdp_header_item *cccitem = APPEND_ENABLE_ROW("if( !used_by_curr_pic_flag[ j ] )");
            if(!used_by_curr_pic_flag) {
                mdp_header_item *pitem = cccitem;
                enable_or_disable_cell(pitem, 1);
                int use_delta_flag = APPEND_BITS_ROW(fmt::format("use_delta_flag[{}]",j), 1);
            }
        }
    } else {
        citem = APPEND_ENABLE_ROW("else");
        ENABLE_CELL(citem);
        APPEND_GOLOMB_UE_ROW(num_negative_pics);
        APPEND_GOLOMB_UE_ROW(num_positive_pics);
        num_negative_pics_list[stRpsIdx] = num_negative_pics;
        num_positive_pics_list[stRpsIdx] = num_positive_pics;
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("for( i = 0; i < num_negative_pics; i++ ) ");
        for(int i = 0; i < num_negative_pics; i++) {
            ENABLE_CELL(ccitem);
            APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("delta_poc_s0_minus1[{}]",i));
            int used_by_curr_pic_s0_flag = APPEND_BITS_ROW(fmt::format("used_by_curr_pic_s0_flag[{}]",i), 1);
        }
        
        ccitem = APPEND_ENABLE_ROW("for( i = 0; i < num_negative_pics; i++ ) ");
        for(int i = 0; i < num_positive_pics; i++) {
            ENABLE_CELL(ccitem);
            APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("delta_poc_s1_minus1[{}]",i));
            int used_by_curr_pic_s1_flag = APPEND_BITS_ROW(fmt::format("used_by_curr_pic_s1_flag[{}]",i), 1);
        }
    }
}
void MALCodecParser::hevc_sps_3d_extension(mdp_header_item *pitem){
    mdp_header_item *citem = APPEND_ENABLE_ROW("for(int i = 0; i <= 1; i++)");
    for(int i = 0; i <= 1; i++) {
        ENABLE_CELL(citem);
        int iv_di_mc_enabled_flag = APPEND_BITS_ROW(fmt::format("iv_di_mc_enabled_flag[{}]",i), 1);
        int iv_mv_scal_enabled_flag = APPEND_BITS_ROW(fmt::format("iv_mv_scal_enabled_flag[{}]",i), 1);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("if(i == 0)");
        if(i == 0) {
            ENABLE_CELL(ccitem);
            APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("log2_ivmc_sub_pb_size_minus3[{}]",i));
            int iv_res_pred_enabled_flag = APPEND_BITS_ROW(fmt::format("iv_res_pred_enabled_flag[{}]",i), 1);
            int depth_ref_enabled_flag = APPEND_BITS_ROW(fmt::format("depth_ref_enabled_flag[{}]",i), 1);
            int iv_mv_scal_enabled_flag = APPEND_BITS_ROW(fmt::format("iv_mv_scal_enabled_flag[{}]",i), 1);
            int vsp_mc_enabled_flag = APPEND_BITS_ROW(fmt::format("vsp_mc_enabled_flag[{}]",i), 1);
            int dbbp_enabled_flag = APPEND_BITS_ROW(fmt::format("dbbp_enabled_flag[{}]",i), 1);
            
        } else {
            ccitem = APPEND_ENABLE_ROW("else");
            ENABLE_CELL(ccitem);
            int tex_mc_enabled_flag = APPEND_BITS_ROW(fmt::format("tex_mc_enabled_flag[{}]",i), 1);
            int log2_texmc_sub_pb_size_minus3 = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("log2_texmc_sub_pb_size_minus3[{}]",i));
            int intra_contour_enabled_flag = APPEND_BITS_ROW(fmt::format("intra_contour_enabled_flag[{}]",i), 1);
            int intra_dc_only_wedge_enabled_flag = APPEND_BITS_ROW(fmt::format("intra_dc_only_wedge_enabled_flag[{}]",i), 1);
            int cqt_cu_part_pred_enabled_flag = APPEND_BITS_ROW(fmt::format("cqt_cu_part_pred_enabled_flag[{}]",i), 1);
            int inter_dc_only_enabled_flag = APPEND_BITS_ROW(fmt::format("inter_dc_only_enabled_flag[{}]",i), 1);
            int skip_intra_enabled_flag = APPEND_BITS_ROW(fmt::format("skip_intra_enabled_flag[{}]",i), 1);
        }
        
    }
}
void MALCodecParser::hevc_profile_tier_level(mdp_header_item *pitem, int profilePresentFlag, int maxNumSubLayersMinus1) {
    mdp_header_item *citem = APPEND_ENABLE_ROW("profile_tier_level()");
    if(profilePresentFlag) {
        ENABLE_CELL(citem);
        int general_profile_space = APPEND_BITS_ROW("general_profile_space", 2);
        int general_tier_flag = APPEND_BITS_ROW("general_tier_flag", 1);
        int general_profile_idc = APPEND_BITS_ROW("general_profile_idc", 5);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("for( j = 0; j < 32; j++ )");
        int general_profile_compatibility_flag[32] = {0};
        for(int j = 0; j < 32; j++) {
            ENABLE_CELL(ccitem);
            general_profile_compatibility_flag[j] = APPEND_BITS_ROW(fmt::format("general_profile_compatibility_flag[{}]",j), 1);
        }
        APPEND_BITS_ROW("general_progressive_source_flag", 1);
        APPEND_BITS_ROW("general_interlaced_source_flag", 1);
        APPEND_BITS_ROW("general_non_packed_constraint_flag", 1);
        APPEND_BITS_ROW("general_frame_only_constraint_flag", 1);
        ccitem = APPEND_ENABLE_ROW("if( general_profile_idc = = 4 | |"
                                   "general_profile_compatibility_flag[ 4 ] | |"
                                   "general_profile_idc = = 5 | | "
                                   "general_profile_compatibility_flag[ 5 ] | |"
                                   "general_profile_idc = = 6 | | "
                                   "general_profile_compatibility_flag[ 6 ] | |"
                                   "general_profile_idc = = 7 | |"
                                   "general_profile_compatibility_flag[ 7 ] "
                                   "general_profile_idc = = 8 | |"
                                   "general_profile_compatibility_flag[ 8 ] "
                                   "general_profile_idc = = 9 | |"
                                   "general_profile_compatibility_flag[ 9 ] "
                                   "general_profile_idc = = 10 | |"
                                   "general_profile_compatibility_flag[ 10 ] "
                                   "general_profile_idc = = 11 | |"
                                   "general_profile_compatibility_flag[ 11 ] "
                                   ")");
        
        if(general_profile_idc == 4 || general_profile_compatibility_flag[4] ||
           general_profile_idc == 5 || general_profile_compatibility_flag[5] ||
           general_profile_idc == 6 || general_profile_compatibility_flag[6] ||
           general_profile_idc == 7 || general_profile_compatibility_flag[7] ||
           general_profile_idc == 8 || general_profile_compatibility_flag[8] ||
           general_profile_idc == 9 || general_profile_compatibility_flag[9] ||
           general_profile_idc == 10 || general_profile_compatibility_flag[10] ||
           general_profile_idc == 11 || general_profile_compatibility_flag[11]) { //最新的hevc标准中有扩展
            ENABLE_CELL(ccitem);
            
            int general_max_12bit_constraint_flag = APPEND_BITS_ROW("general_max_12bit_constraint_flag", 1);
            int general_max_10bit_constraint_flag = APPEND_BITS_ROW("general_max_10bit_constraint_flag", 1);
            int general_max_8bit_constraint_flag = APPEND_BITS_ROW("general_max_8bit_constraint_flag", 1);
            int general_max_422chroma_constraint_flag = APPEND_BITS_ROW("general_max_422chroma_constraint_flag", 1);
            int general_max_420chroma_constraint_flag = APPEND_BITS_ROW("general_max_420chroma_constraint_flag", 1);
            int general_max_monochrome_constraint_flag = APPEND_BITS_ROW("general_max_monochrome_constraint_flag", 1);
            int general_intra_constraint_flag = APPEND_BITS_ROW("general_intra_constraint_flag", 1);
            int general_one_picture_only_constraint_flag = APPEND_BITS_ROW("general_one_picture_only_constraint_flag", 1);
            int general_lower_bit_rate_constraint_flag = APPEND_BITS_ROW("general_lower_bit_rate_constraint_flag", 1);
            int general_max_14bit_constraint_flag = -1;
            
            if( general_profile_idc == 5 || general_profile_compatibility_flag[ 5 ]
               || general_profile_idc == 9 || general_profile_compatibility_flag[ 9 ]
               || general_profile_idc == 10 || general_profile_compatibility_flag[ 10 ]
               || general_profile_idc == 11 || general_profile_compatibility_flag[ 11 ] ) {
                general_max_14bit_constraint_flag = APPEND_BITS_ROW("general_max_14bit_constraint_flag", 1);
                APPEND_BITS_ROW("general_reserved_zero_33bits", 33);
            } else {
                APPEND_BITS_ROW("general_reserved_zero_34bits", 34);
            }
        } else if( general_profile_idc == 2 || general_profile_compatibility_flag[ 2 ] ) {
            ccitem = APPEND_ENABLE_ROW("else if( general_profile_idc == 2 || general_profile_compatibility_flag[ 2 ] )");
            ENABLE_CELL(ccitem);
            APPEND_BITS_ROW("general_reserved_zero_7bits", 7);
            int general_one_picture_only_constraint_flag = APPEND_BITS_ROW("general_one_picture_only_constraint_flag", 1);
            APPEND_BITS_ROW("general_reserved_zero_35bits", 35);
        } else {
            ccitem = APPEND_ENABLE_ROW("else");
            ENABLE_CELL(ccitem);
            APPEND_BITS_ROW("general_reserved_zero_43bits",43);
        }
        ccitem = APPEND_ENABLE_ROW("if( ( general_profile_idc >= 1 && general_profile_idc <= 5 ) | | "
                                   "general_profile_compatibility_flag[ 1 ] || "
                                   "general_profile_compatibility_flag[ 2 ] || "
                                   "general_profile_compatibility_flag[ 3 ] || "
                                   "general_profile_compatibility_flag[ 4 ] || "
                                   "general_profile_compatibility_flag[ 5 ] ||"
                                   "general_profile_idc == 9 ||"
                                   "general_profile_compatibility_flag[ 9 ] ||"
                                   "general_profile_idc == 11 ||"
                                   "general_profile_compatibility_flag[ 11 ] ||"
                                   ")");
        if( ( general_profile_idc >= 1 && general_profile_idc <= 5 ) ||
           general_profile_compatibility_flag[ 1 ] ||
           general_profile_compatibility_flag[ 2 ] ||
           general_profile_compatibility_flag[ 3 ] ||
           general_profile_compatibility_flag[ 4 ] ||
           general_profile_compatibility_flag[ 5 ] ||
           general_profile_idc == 9 || general_profile_compatibility_flag[ 9 ] ||
           general_profile_idc == 11 || general_profile_compatibility_flag[ 11 ]) {
            ENABLE_CELL(ccitem);
            APPEND_BITS_ROW("general_inbld_flag",1);
        } else {
            ccitem = APPEND_ENABLE_ROW("else");
            ENABLE_CELL(ccitem);
            APPEND_BITS_ROW("general_reserved_zero_bit",1);
        }
        
    }
    int general_level_idc = APPEND_BITS_ROW("general_level_idc",8);
    mdp_header_item *ccitem = APPEND_ENABLE_ROW("for(int i = 0; i<maxNumSubLayersMinus1; i++)");
    int* sub_layer_profile_present_flag = nullptr;
    int *sub_layer_level_present_flag = nullptr;
    if(maxNumSubLayersMinus1 > 0) {
        ENABLE_CELL(ccitem);
        sub_layer_profile_present_flag = (int *)malloc(sizeof(int) * maxNumSubLayersMinus1);
        sub_layer_level_present_flag = (int *)malloc(sizeof(int) * maxNumSubLayersMinus1);
        for(int i = 0; i<maxNumSubLayersMinus1; i++) {
            sub_layer_profile_present_flag[i] = APPEND_BITS_ROW(fmt::format("sub_layer_profile_present_flag[{}]",i), 1);
            sub_layer_level_present_flag[i] = APPEND_BITS_ROW(fmt::format("sub_layer_level_present_flag[{}]",i), 1);
        }
    }
    ccitem = APPEND_ENABLE_ROW("if( maxNumSubLayersMinus1 > 0 )");
    if(maxNumSubLayersMinus1 > 0) {
        ENABLE_CELL(ccitem);
        for(int i = maxNumSubLayersMinus1; i<8; i++) {
            APPEND_BITS_ROW(fmt::format("reserved_zero_2bits[{}]",i), 2);
        }
    }
    
    ccitem = APPEND_ENABLE_ROW("for( i = 0; i < maxNumSubLayersMinus1; i++ )");
    if(maxNumSubLayersMinus1 > 0) {
        ENABLE_CELL(ccitem);
        for(int i = 0; i < maxNumSubLayersMinus1; i++) {
            mdp_header_item *cccitem = APPEND_ENABLE_ROW("if( sub_layer_profile_present_flag[ i ] )");
            if(sub_layer_profile_present_flag[i]) {
                ENABLE_CELL(cccitem);
                
                APPEND_BITS_ROW(fmt::format("sub_layer_profile_space[{}]",i), 2);
                APPEND_BITS_ROW(fmt::format("sub_layer_tier_flag[{}]",i), 1);
                int sub_layer_profile_idc = APPEND_BITS_ROW(fmt::format("sub_layer_profile_idc[{}]",i), 5);
                mdp_header_item *ccccitem = APPEND_ENABLE_ROW("for( j = 0; j < 32; j++ )");
                int sub_layer_profile_compatibility_flag[32] = {0};
                for(int j = 0; j < 32; j++) {
                    ENABLE_CELL(ccccitem);
                    APPEND_BITS_ROW(fmt::format("sub_layer_profile_compatibility_flag[{}][{}]",i,j), 1);
                }
                APPEND_BITS_ROW(fmt::format("sub_layer_progressive_source_flag[{}]",i), 1);
                APPEND_BITS_ROW(fmt::format("sub_layer_interlaced_source_flag[{}]",i), 1);
                APPEND_BITS_ROW(fmt::format("sub_layer_non_packed_constraint_flag[{}]",i), 1);
                APPEND_BITS_ROW(fmt::format("sub_layer_frame_only_constraint_flag[{}]",i), 1);
                
                ccccitem = APPEND_ENABLE_ROW("if(sub_layer_profile_idc[ i ] = = 4 | | sub_layer_profile_compatibility_flag[ i ][ 4 ] | | "
                                             "sub_layer_profile_idc[ i ] = = 5 | | sub_layer_profile_compatibility_flag[ i ][ 5 ] | | "
                                             "sub_layer_profile_idc[ i ] = = 6 | | sub_layer_profile_compatibility_flag[ i ][ 6 ] | | "
                                             "sub_layer_profile_idc[ i ] = = 7 | | sub_layer_profile_compatibility_flag[ i ][ 7 ] )");
                if( sub_layer_profile_idc == 4 || sub_layer_profile_compatibility_flag[ 4 ] ||
                   sub_layer_profile_idc == 5 || sub_layer_profile_compatibility_flag[ 5 ] ||
                   sub_layer_profile_idc == 6 || sub_layer_profile_compatibility_flag[ 6 ] ||
                   sub_layer_profile_idc == 7 || sub_layer_profile_compatibility_flag[ 7 ] ) {
                    ENABLE_CELL(ccccitem);
                    
                    APPEND_BITS_ROW(fmt::format("sub_layer_max_12bit_constraint_flag[{}]",i), 1);
                    
                    APPEND_BITS_ROW(fmt::format("sub_layer_max_10bit_constraint_flag[{}]",i), 1);
                    
                    APPEND_BITS_ROW(fmt::format("sub_layer_max_8bit_constraint_flag[{}]",i), 1);
                    
                    APPEND_BITS_ROW(fmt::format("sub_layer_max_422chroma_constraint_flag[{}]",i), 1);
                    
                    APPEND_BITS_ROW(fmt::format("sub_layer_max_420chroma_constraint_flag[{}]",i), 1);
                    
                    APPEND_BITS_ROW(fmt::format("sub_layer_max_monochrome_constraint_flag[{}]",i), 1);
                    
                    APPEND_BITS_ROW(fmt::format("sub_layer_intra_constraint_flag[{}]",i), 1);
                    
                    APPEND_BITS_ROW(fmt::format("sub_layer_one_picture_only_constraint_flag[{}]",i), 1);
                    
                    APPEND_BITS_ROW(fmt::format("sub_layer_lower_bit_rate_constraint_flag[{}]",i), 1);
                    
                    APPEND_BITS_ROW(fmt::format("sub_layer_reserved_zero_34bits[{}]",i), 34);
                } else {
                    ccccitem = APPEND_ENABLE_ROW("else");
                    ENABLE_CELL(ccccitem);
                    APPEND_BITS_ROW(fmt::format("sub_layer_reserved_zero_43bits[{}]",i), 43);
                }
                
                ccccitem = APPEND_ENABLE_ROW("if( ( sub_layer_profile_idc[ i ] >= 1 && sub_layer_profile_idc[ i ] <= 5 ) | | "
                                             "sub_layer_profile_compatibility_flag[ 1 ] | | "
                                             "sub_layer_profile_compatibility_flag[ 2 ] | | "
                                             "sub_layer_profile_compatibility_flag[ 3 ] | | "
                                             "sub_layer_profile_compatibility_flag[ 4 ] | | "
                                             "sub_layer_profile_compatibility_flag[ 5 ] )");
                if( ( sub_layer_profile_idc >= 1 && sub_layer_profile_idc <= 5 ) ||
                   sub_layer_profile_compatibility_flag[1] ||
                   sub_layer_profile_compatibility_flag[2] ||
                   sub_layer_profile_compatibility_flag[3] ||
                   sub_layer_profile_compatibility_flag[4] ||
                   sub_layer_profile_compatibility_flag[ 5 ] ) {
                    ENABLE_CELL(ccccitem);
                    APPEND_BITS_ROW(fmt::format("sub_layer_inbld_flag[{}]",i), 1);
                } else {
                    ccccitem = APPEND_ENABLE_ROW("else");
                    ENABLE_CELL(ccccitem);
                    APPEND_BITS_ROW(fmt::format("sub_layer_reserved_zero_bit[{}]",i), 1);
                }
                
            }
            cccitem = APPEND_ENABLE_ROW("if( sub_layer_level_present_flag[ i ] )");
            if(sub_layer_level_present_flag[i]) {
                ENABLE_CELL(cccitem);
                APPEND_BITS_ROW(fmt::format("sub_layer_level_idc[{}]",i), 8);
            }
            
        }
    }
    if(sub_layer_profile_present_flag) {
        free(sub_layer_profile_present_flag);
    }
    
    if(sub_layer_level_present_flag) {
        free(sub_layer_level_present_flag);
    }
    
}
void MALCodecParser::hevc_sub_layer_hrd_parameters(mdp_header_item *pitem, int i,int cpb_cnt_minus1,int sub_pic_hrd_params_present_flag) {
    char name[50] = {0};
    snprintf(name,49,"sub_layer_hrd_parameters[%d] )",i);
    mdp_header_item * citem = APPEND_ENABLE_ROW(fmt::format("sub_layer_hrd_parameters[{}] )",i));
    enable_or_disable_cell(citem, 1);
    pitem = citem;
    mdp_header_item * ccitem = APPEND_ENABLE_ROW("for( i = 0; i <= CpbCnt; i++ )");
    int CpbCnt = cpb_cnt_minus1;
    for( int i = 0; i <= CpbCnt; i++ ) {
        ENABLE_CELL(ccitem);
        int bit_rate_value_minus1 = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("bit_rate_value_minus1[{}]",i));
        
        int cpb_size_value_minus1 = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("cpb_size_value_minus1[{}]",i));
        
        mdp_header_item * cccitem = APPEND_ENABLE_ROW("if( sub_pic_hrd_params_present_flag )");
        if( sub_pic_hrd_params_present_flag ) {
            ENABLE_CELL(cccitem);
            int cpb_size_du_value_minus1 = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("cpb_size_du_value_minus1[{}]",i));
            int bit_rate_du_value_minus1 = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("bit_rate_du_value_minus1[{}]",i));
        }
        int cbr_flag = APPEND_BITS_ROW(fmt::format("cbr_flag[{}]",i), 1);
    }
    
}
void MALCodecParser::hevc_hrd_parameters(mdp_header_item *pitem, int commonInfPresentFlag, int maxNumSubLayersMinus1) {
    mdp_header_item *citem = APPEND_ENABLE_ROW("if( commonInfPresentFlag ) ");
    int nal_hrd_parameters_present_flag  = 0;
    int sub_pic_hrd_params_present_flag = 0;
    int vcl_hrd_parameters_present_flag = 0;
    if( commonInfPresentFlag )  {
        ENABLE_CELL(citem);
        nal_hrd_parameters_present_flag = APPEND_BITS_ROW("nal_hrd_parameters_present_flag",1);
        vcl_hrd_parameters_present_flag = APPEND_BITS_ROW("vcl_hrd_parameters_present_flag",1);
        
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( nal_hrd_parameters_present_flag | | vcl_hrd_parameters_present_flag )");
        if( nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag ) {
            ENABLE_CELL(ccitem);
            sub_pic_hrd_params_present_flag = APPEND_BITS_ROW("sub_pic_hrd_params_present_flag",1);
            mdp_header_item *cccitem = APPEND_ENABLE_ROW("if( sub_pic_hrd_params_present_flag )");
            if( sub_pic_hrd_params_present_flag ) {
                ENABLE_CELL(cccitem);
                int tick_divisor_minus2 = APPEND_BITS_ROW("tick_divisor_minus2",1);
                int du_cpb_removal_delay_increment_length_minus1 = APPEND_BITS_ROW("du_cpb_removal_delay_increment_length_minus1",1);
                int sub_pic_cpb_params_in_pic_timing_sei_flag = APPEND_BITS_ROW("sub_pic_cpb_params_in_pic_timing_sei_flag",1);
                int dpb_output_delay_du_length_minus1 = APPEND_BITS_ROW("dpb_output_delay_du_length_minus1",1);
            }
            int bit_rate_scale = APPEND_BITS_ROW("bit_rate_scale",4);
            int cpb_size_scale = APPEND_BITS_ROW("cpb_size_scale",4);
            cccitem = APPEND_ENABLE_ROW("if( sub_pic_hrd_params_present_flag )");
            if( sub_pic_hrd_params_present_flag ) {
                ENABLE_CELL(cccitem);
                int cpb_size_du_scale = APPEND_BITS_ROW("cpb_size_du_scale",4);
            }
            int initial_cpb_removal_delay_length_minus1 = APPEND_BITS_ROW("initial_cpb_removal_delay_length_minus1",5);
            int au_cpb_removal_delay_length_minus1 = APPEND_BITS_ROW("au_cpb_removal_delay_length_minus1",5);
            int dpb_output_delay_length_minus1 = APPEND_BITS_ROW("dpb_output_delay_length_minus1",5);
            
        }
        
    }
    citem = APPEND_ENABLE_ROW("for( i = 0; i <= maxNumSubLayersMinus1; i++ )");
    for( int i = 0; i <= maxNumSubLayersMinus1; i++ ) {
        ENABLE_CELL(citem);
        int fixed_pic_rate_general_flag = APPEND_BITS_ROW(fmt::format("fixed_pic_rate_general_flag[{}]",i), 1);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW(fmt::format("if( !fixed_pic_rate_general_flag[{}] )",i));
        int fixed_pic_rate_within_cvs_flag = fixed_pic_rate_general_flag == 1 ? 1:0;
        if( !fixed_pic_rate_general_flag) {
            ENABLE_CELL(ccitem);
            fixed_pic_rate_within_cvs_flag = APPEND_BITS_ROW(fmt::format("fixed_pic_rate_within_cvs_flag[{}]",i), 1);
        }
        ccitem = APPEND_ENABLE_ROW(fmt::format("if( fixed_pic_rate_within_cvs_flag[{}] )",i));
        int low_delay_hrd_flag = 0;
        if( fixed_pic_rate_within_cvs_flag) {
            ENABLE_CELL(ccitem);
            int elemental_duration_in_tc_minus1 = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("elemental_duration_in_tc_minus1[{}]",i));
        } else {
            ccitem = APPEND_ENABLE_ROW("else");
            ENABLE_CELL(ccitem);
            low_delay_hrd_flag = APPEND_BITS_ROW(fmt::format("low_delay_hrd_flag[{}]",i), 1);
        }
        ccitem = APPEND_ENABLE_ROW(fmt::format("if( !low_delay_hrd_flag[{}] )",i));
        int cpb_cnt_minus1 = 0;
        if( !low_delay_hrd_flag) {
            ENABLE_CELL(ccitem);
            cpb_cnt_minus1 = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("cpb_cnt_minus1[{}]",i));
        }
        ccitem = APPEND_ENABLE_ROW("if( nal_hrd_parameters_present_flag )");
        if( nal_hrd_parameters_present_flag ) {
            ENABLE_CELL(ccitem);
            hevc_sub_layer_hrd_parameters(ccitem,i,cpb_cnt_minus1,sub_pic_hrd_params_present_flag);
        }
        
        ccitem = APPEND_ENABLE_ROW("if( vcl_hrd_parameters_present_flag )");
        if( vcl_hrd_parameters_present_flag ) {
            ENABLE_CELL(ccitem);
            hevc_sub_layer_hrd_parameters(ccitem,i,cpb_cnt_minus1,sub_pic_hrd_params_present_flag);
        }
    }
}
void MALCodecParser::hevc_vps_extension(mdp_header_item *pitem) {
    
}
int MALCodecParser::parse_hvc_vps(mdp_video_header *header, mdp_header_item *item) {
    int ret = 0;
    auto vps = std::dynamic_pointer_cast<MALHEVCVPS>(currentHEVCNal_);
    if (hvcc_) {
        hvcc_->vpsList.push_back(vps);
    }
    mdp_header_item *pitem = item;
    int vps_video_parameter_set_id = APPEND_BITS_ROW("vps_video_parameter_set_id",4);
    vps->video_parameter_set_id = vps_video_parameter_set_id;
    int vps_base_layer_internal_flag = APPEND_BITS_ROW("vps_base_layer_internal_flag",1);
    int vps_base_layer_available_flag = APPEND_BITS_ROW("vps_base_layer_available_flag",1);
    int vps_max_layers_minus1 = APPEND_BITS_ROW("vps_max_layers_minus1",6);
    int vps_max_sub_layers_minus1 = APPEND_BITS_ROW("vps_max_sub_layers_minus1",3);
    int vps_temporal_id_nesting_flag = APPEND_BITS_ROW("vps_temporal_id_nesting_flag",1);
    int vps_reserved_0xffff_16bits = APPEND_BITS_ROW("vps_reserved_0xffff_16bits",16);
    hevc_profile_tier_level(pitem,1,vps_max_sub_layers_minus1);
    int vps_sub_layer_ordering_info_present_flag = APPEND_BITS_ROW("vps_sub_layer_ordering_info_present_flag",1);
    
    mdp_header_item *citem = APPEND_ENABLE_ROW("for( i = ( vps_sub_layer_ordering_info_present_flag ? 0 : vps_max_sub_layers_minus1 ); i <= vps_max_sub_layers_minus1; i++ ) ");
    for(int i = ( vps_sub_layer_ordering_info_present_flag ? 0 : vps_max_sub_layers_minus1 ); i <= vps_max_sub_layers_minus1; i++ )  {
        ENABLE_CELL(citem);
        int vps_max_dec_pic_buffering_minus1 = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("vps_max_dec_pic_buffering_minus1[{}]",i));
        
        int vps_max_num_reorder_pics = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("vps_max_num_reorder_pics[{}]",i));
        
        int vps_max_latency_increase_plus1 = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("vps_max_latency_increase_plus1[{}]",i));
    }
    
    int vps_max_layer_id = APPEND_BITS_ROW("vps_max_layer_id",6);
    APPEND_GOLOMB_UE_ROW(vps_num_layer_sets_minus1);
    
    citem = APPEND_ENABLE_ROW("for( i = 1; i <= vps_num_layer_sets_minus1; i++ ) ");
    int layer_id_included_flag[64][64] = {0};
    for(int i = 1; i <= vps_num_layer_sets_minus1; i++ )  {
        ENABLE_CELL(citem);
        mdp_header_item * ccitem = APPEND_ENABLE_ROW("for( j = 0; j <= vps_max_layer_id; j++ ) ");
        for( int j = 0; j <= vps_max_layer_id; j++ ) {
            ENABLE_CELL(ccitem);
            int val = APPEND_BITS_ROW(fmt::format("layer_id_included_flag[{}][{}]",i,j),1);
            layer_id_included_flag[i][j] = val;
        }
    }
    int vps_timing_info_present_flag = APPEND_BITS_ROW("vps_timing_info_present_flag",1);
    
    citem = APPEND_ENABLE_ROW("if( vps_timing_info_present_flag )");
    if( vps_timing_info_present_flag ) {
        ENABLE_CELL(citem);
        int vps_num_units_in_tick = APPEND_BITS_ROW("vps_num_units_in_tick",32);
        
        int vps_time_scale = APPEND_BITS_ROW("vps_time_scale",32);
        
        int vps_poc_proportional_to_timing_flag = APPEND_BITS_ROW("vps_poc_proportional_to_timing_flag",1);
        
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( vps_poc_proportional_to_timing_flag )");
        if( vps_poc_proportional_to_timing_flag ) {
            ENABLE_CELL(ccitem);
            APPEND_GOLOMB_UE_ROW(vps_num_ticks_poc_diff_one_minus1);
        }
        APPEND_GOLOMB_UE_ROW(vps_num_hrd_parameters);
        ccitem = APPEND_ENABLE_ROW("for( i = 0; i < vps_num_hrd_parameters; i++ )");
        for( int i = 0; i < vps_num_hrd_parameters; i++ ) {
            int hrd_layer_set_idx = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("hrd_layer_set_idx[{}]",i));
            mdp_header_item *cccitem = APPEND_ENABLE_ROW("if( i > 0 )");
            int cprms_present_flag = (i == 0 ? 1 : 0);
            if( i > 0 ) {
                ENABLE_CELL(cccitem);
                cprms_present_flag = APPEND_BITS_ROW(fmt::format("cprms_present_flag[{}]",i),1);
            }
            cccitem = APPEND_ENABLE_ROW(fmt::format("hrd_parameters[{}]",i));
            enable_or_disable_cell(cccitem, 1);
            hevc_hrd_parameters(cccitem,cprms_present_flag,vps_max_sub_layers_minus1);
        }
        
    }
    int vps_extension_flag = APPEND_BITS_ROW("vps_extension_flag",1);
    citem = APPEND_ENABLE_ROW("if( vps_extension_flag )");
    if( vps_extension_flag ) {
        ENABLE_CELL(citem);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("while( !byte_aligned( ) )");
        while  (datasource_->currentBitsPosition() % 8 != 0) {
            ENABLE_CELL(ccitem);
            APPEND_BITS_ROW("vps_extension_alignment_bit_equal_to_one", 1);
        }
        ccitem = APPEND_ENABLE_ROW("vps_extension( )");
        {
            ENABLE_CELL(ccitem);
            
            mdp_header_item *citem = APPEND_ENABLE_ROW("if( vps_max_layers_minus1 > 0 && vps_base_layer_internal_flag ) ");
            if( vps_max_layers_minus1 > 0 && vps_base_layer_internal_flag ) {
                ENABLE_CELL(citem);
                hevc_profile_tier_level(pitem,0, vps_max_sub_layers_minus1);
            }
            int splitting_flag = APPEND_BITS_ROW("splitting_flag", 1);
            int i,j,NumScalabilityTypes = 0;
            citem = APPEND_ENABLE_ROW("for( i = 0, NumScalabilityTypes = 0; i < 16; i++ ) ");
            for( i = 0, NumScalabilityTypes = 0; i < 16; i++ ) {
                ENABLE_CELL(citem);
                int scalability_mask_flag = APPEND_BITS_ROW(fmt::format("scalability_mask_flag[{}]",i), 1);
                NumScalabilityTypes += scalability_mask_flag;
            }
            citem = APPEND_ENABLE_ROW("for( j = 0; j < ( NumScalabilityTypes − splitting_flag ); j++ ) ");
            for( j = 0; j < ( NumScalabilityTypes - splitting_flag ); j++ ) {
                ENABLE_CELL(citem);
                APPEND_BITS_ROW(fmt::format("dimension_id_len_minus1[{}]",j), 3);
            }
            int vps_nuh_layer_id_present_flag = APPEND_BITS_ROW("vps_nuh_layer_id_present_flag", 1);
            citem = APPEND_ENABLE_ROW("for( i = 1; i <= MaxLayersMinus1; i++ ) ");
            int MaxLayersMinus1 = std::min(62,vps_max_layers_minus1);
            int layer_id_in_nuh[64] = {0};
            for( i = 1; i <= MaxLayersMinus1; i++ ) {
                ENABLE_CELL(citem);
                auto ccitem = APPEND_ENABLE_ROW("if( vps_nuh_layer_id_present_flag ) ");
                if( vps_nuh_layer_id_present_flag ) {
                    ENABLE_CELL(ccitem);
                    int val = APPEND_BITS_ROW(fmt::format("layer_id_in_nuh[{}]",i), 6);
                    layer_id_in_nuh[i] = val;
                }
                ccitem = APPEND_ENABLE_ROW("if( !splitting_flag ) ");
                if( !splitting_flag ) {
                    ENABLE_CELL(ccitem);
                    for( j = 0; j < NumScalabilityTypes; j++ ) {
                        APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("dimension_id[{}][{}]",i,j));
                    }
                }
            }
            APPEND_BITS_ROW_VAR(view_id_len, 4);
            citem = APPEND_ENABLE_ROW("if( view_id_len > 0 )");
            if( view_id_len > 0 ) {
                ENABLE_CELL(citem);
                int NumViews = 2; //from ffmpeg
                for( i = 0; i < NumViews; i++ ) {
                    APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("view_id_val[{}]",i));
                    APPEND_BITS_ROW(fmt::format("view_id_val[{}]",i), view_id_len);
                }
            }
            int direct_dependency_flag[64][64] = {0};
            citem = APPEND_ENABLE_ROW("for( i = 1; i <= MaxLayersMinus1; i++ )");
            for( i = 1; i <= MaxLayersMinus1; i++ ){
                for( j = 0; j < i; j++ ) {
                    int val = APPEND_BITS_ROW(fmt::format("direct_dependency_flag[{}][ {}]",i,j), 1);
                    direct_dependency_flag[i][j] = val;
                }
            }
            
            int NumIndependentLayers,k,d,r,p,h,iNuhLId,jNuhLid,predLId = 0;
            int layerIdInListFlag[64] = {0};
            int IdDirectRefLayer[64][64] = {0};
            int DependencyFlag[64][64] = {0};
            int IdRefLayer[64][64] = {0};
            int IdPredictedLayer[64][64] = {0};
            int NumDirectRefLayers[64] = {0};
            int NumRefLayers[64] = {0};
            int NumPredictedLayers[64] = {0};
            int TreePartitionLayerIdList[64][64] = {0};
            int NumLayersInTreePartition[64] = {0};
            for( i = 0; i <= MaxLayersMinus1; i++ )
                for( j = 0; j <= MaxLayersMinus1; j++ ) {
                    DependencyFlag[ i ][ j ] = direct_dependency_flag[ i ][ j ];
                    for( k = 0; k < i; k++ )
                        if( direct_dependency_flag[ i ][ k ] && DependencyFlag[ k ][ j ] )
                            DependencyFlag[ i ][ j ] = 1;
                }
            
            for( i = 0; i <= MaxLayersMinus1; i++ ) {
                iNuhLId = layer_id_in_nuh[ i ];
                for( j = 0, d = 0, r = 0, p = 0; j <= MaxLayersMinus1; j++ ) {
                    jNuhLid = layer_id_in_nuh[ j ];
                    if( direct_dependency_flag[ i ][ j ] )
                        IdDirectRefLayer[ iNuhLId ][ d++ ] = jNuhLid;
                    if( DependencyFlag[ i ][ j ] )
                        IdRefLayer[ iNuhLId ][ r++ ] = jNuhLid;
                    if( DependencyFlag[ j ][ i ] )
                        IdPredictedLayer[ iNuhLId ][ p++ ] = jNuhLid;
                }
                NumDirectRefLayers[ iNuhLId ] = d;
                NumRefLayers[ iNuhLId ] = r;
                NumPredictedLayers[ iNuhLId ] = p;
            }
            
            
            for( i = 0; i <= 63; i++ )
                layerIdInListFlag[ i ] = 0;
            for( i = 0, k = 0; i <= MaxLayersMinus1; i++ ) {
                iNuhLId = layer_id_in_nuh[i];
                if( NumDirectRefLayers[ iNuhLId ] == 0 ) {
                    TreePartitionLayerIdList[ k ][ 0 ] = iNuhLId;
                    for( j = 0, h = 1; j < NumPredictedLayers[ iNuhLId ]; j++ ) {
                        predLId = IdPredictedLayer[ iNuhLId ][ j ];
                        if( !layerIdInListFlag[ predLId ] ) {
                            TreePartitionLayerIdList[ k ][ h++ ] = predLId;
                            layerIdInListFlag[ predLId ] = 1;
                        }
                    }
                    NumLayersInTreePartition[ k++ ] = h;
                }
            }
            NumIndependentLayers = k;

            citem = APPEND_ENABLE_ROW("if( NumIndependentLayers > 1 )");
            int num_add_layer_sets = 0;
            if( NumIndependentLayers > 1 ) {
                ENABLE_CELL(citem);
                num_add_layer_sets = APPEND_GOLOMB_UE_ROW_NO_VAR(num_add_layer_sets);
            }
            citem = APPEND_ENABLE_ROW("for( i = 0; i < num_add_layer_sets; i++ )");
            for( i = 0; i < num_add_layer_sets; i++ ) {
                ENABLE_CELL(citem);
                for( j = 1; j < NumIndependentLayers; j++ ) {
                    int len = ceil(log2(NumLayersInTreePartition[j] + 1 ));
                    APPEND_BITS_ROW(fmt::format("highest_layer_idx_plus1[{}][ {}]",i,j), len);
                }
            }
            APPEND_BITS_ROW_VAR(vps_sub_layers_max_minus1_present_flag, 1);
            citem = APPEND_ENABLE_ROW("if (vps_sub_layers_max_minus1_present_flag)");
            if (vps_sub_layers_max_minus1_present_flag) {
                ENABLE_CELL(citem);
                for( i = 0; i <= MaxLayersMinus1; i++ ) {
                    APPEND_BITS_ROW(fmt::format("sub_layers_vps_max_minus1[{}]",i), 3);
                }
            }
            APPEND_BITS_ROW_VAR(max_tid_ref_present_flag, 1);
            citem = APPEND_ENABLE_ROW("if( max_tid_ref_present_flag )");
            if( max_tid_ref_present_flag ) {
                ENABLE_CELL(citem);
                for( i = 0; i < MaxLayersMinus1; i++ ) {
                    for( j = i + 1; j <= MaxLayersMinus1; j++ ) {
                        if( direct_dependency_flag[ j ][ i ] ) {
                            APPEND_BITS_ROW(fmt::format("max_tid_il_ref_pics_plus1[{}][ {}]",i,j), 3);
                        }
                    }
                }
            }
            APPEND_BITS_ROW("default_ref_layers_active_flag", 1);
            APPEND_GOLOMB_UE_ROW(vps_num_profile_tier_level_minus1);
            citem = APPEND_ENABLE_ROW("for( i = vps_base_layer_internal_flag ? 2 : 1; i <= vps_num_profile_tier_level_minus1; i++ ) ");
            for( i = vps_base_layer_internal_flag ? 2 : 1;
                i <= vps_num_profile_tier_level_minus1; i++ ) {
                ENABLE_CELL(citem);
                int vps_profile_present_flag = APPEND_BITS_ROW(fmt::format("vps_profile_present_flag[{}]",i), 1);
                hevc_profile_tier_level(citem, vps_profile_present_flag,  vps_max_sub_layers_minus1);
            }
            int NumLayerSets = vps_num_layer_sets_minus1 + 1 + num_add_layer_sets;
            citem = APPEND_ENABLE_ROW("if( NumLayerSets > 1 )");
            int num_add_olss = 0;
            int default_output_layer_idc = 0;
            if( NumLayerSets > 1 ) {
                ENABLE_CELL(citem);
                num_add_olss = APPEND_GOLOMB_UE_ROW_NO_VAR(num_add_olss);
                default_output_layer_idc = APPEND_BITS_ROW("default_output_layer_idc", 2);
            }
            int NumOutputLayerSets = num_add_olss + NumLayerSets;
            citem = APPEND_ENABLE_ROW("for( i = 1; i < NumOutputLayerSets; i++ )");
            int layer_set_idx_for_ols_minus1[64] = {0};
            int n = 0,m = 0;
            int LayerSetLayerIdList[64][64] = {0};
            int NumLayersInIdList[64] = {0};
            NumLayersInIdList[0] = 1;
            int OlsIdxToLsIdx[64] = {0};
            int NecessaryLayerFlag[64][64] = {0};
            int olsIdx,lsIdx,lsLayerIdx,currLayerId,rLsLayerIdx,refLayerId = 0;
            int OutputLayerFlag[64][64] = {0};
            int NumNecessaryLayers[64] = {0};
            int LayerIdxInVps[64] = {0};
            int NumOutputLayersInOutputLayerSet[64] = {0};
            int defaultOutputLayerIdc = std::min(default_output_layer_idc,2);
            int OlsHighestOutputLayerId[64] = {0};
            
            for (int i = 0; i <= MaxLayersMinus1; i++) {
                LayerIdxInVps[ layer_id_in_nuh[ i ] ] = i;
            }
            memcpy(vps->LayerIdxInVps, LayerIdxInVps, sizeof(LayerIdxInVps));
            for( i = 1; i <= vps_num_layer_sets_minus1; i++ ) {
                for( m = 0; m <= vps_max_layer_id; m++ )
                    if( layer_id_included_flag[ i ][ m ] )
                        LayerSetLayerIdList[ i ][ n++ ] = m;
                NumLayersInIdList[ i ] = n;
                
            }
            for (i = 0; i <= NumOutputLayerSets - 1; i++) {
                OlsIdxToLsIdx[ i ] = ( i < NumLayerSets ) ? i : ( layer_set_idx_for_ols_minus1[ i ] + 1 );
            }
            for( olsIdx = 0; olsIdx < NumOutputLayerSets; olsIdx++ ) {
                lsIdx = OlsIdxToLsIdx[ olsIdx ];
                for( lsLayerIdx = 0; lsLayerIdx < NumLayersInIdList[ lsIdx ]; lsLayerIdx++ )
                    NecessaryLayerFlag[ olsIdx ][ lsLayerIdx ] = 0;
                for( lsLayerIdx = 0; lsLayerIdx < NumLayersInIdList[ lsIdx ]; lsLayerIdx++ )
                    if( OutputLayerFlag[ olsIdx ][ lsLayerIdx ] ) {
                        NecessaryLayerFlag[ olsIdx ][ lsLayerIdx ] = 1;
                        currLayerId = LayerSetLayerIdList[ lsIdx ][ lsLayerIdx ];
                        for( rLsLayerIdx = 0; rLsLayerIdx < lsLayerIdx; rLsLayerIdx++ ) {
                            refLayerId = LayerSetLayerIdList[ lsIdx ][ rLsLayerIdx ];
                            if( DependencyFlag[LayerIdxInVps[currLayerId]][ LayerIdxInVps[ refLayerId ] ] )
                                NecessaryLayerFlag[ olsIdx ][ rLsLayerIdx ] = 1;
                        }
                        NumNecessaryLayers[ olsIdx ] = 0;
                        for( lsLayerIdx = 0; lsLayerIdx < NumLayersInIdList[ lsIdx ]; lsLayerIdx++ )
                            NumNecessaryLayers[ olsIdx ] += NecessaryLayerFlag[ olsIdx ][ lsLayerIdx ];
                    }
            }
            for (i = ((defaultOutputLayerIdc == 2 ) ? 0 : (vps_num_layer_sets_minus1 + 1)); i < (NumOutputLayerSets - 1); i++) {
                for( j = 0; j < NumLayersInIdList[ OlsIdxToLsIdx[ i ] ]; j++ ) {
                    NumOutputLayersInOutputLayerSet[ i ] += OutputLayerFlag[ i ][ j ];
                    if( OutputLayerFlag[ i ][ j ] )
                        OlsHighestOutputLayerId[ i ] = LayerSetLayerIdList[ OlsIdxToLsIdx[ i ] ][ j ];
                }
            }
            for( i = 1; i < NumOutputLayerSets; i++ ) {
                auto ccitem = APPEND_ENABLE_ROW("if( NumLayerSets > 2 && i >= NumLayerSets )");
                if( NumLayerSets > 2 && i >= NumLayerSets ) {
                    ENABLE_CELL(ccitem);
                    int len = ceil(log2(NumLayerSets - 1));
                    int val = APPEND_BITS_ROW(fmt::format("layer_set_idx_for_ols_minus1[{}]",i), len);
                    layer_set_idx_for_ols_minus1[i] = val;
                }
                
                ccitem = APPEND_ENABLE_ROW("if( i > vps_num_layer_sets_minus1 | | defaultOutputLayerIdc = = 2 )");
                if( i > vps_num_layer_sets_minus1 || defaultOutputLayerIdc == 2 ) {
                    ENABLE_CELL(ccitem);
                    for( j = 0; j < NumLayersInIdList[ OlsIdxToLsIdx[ i ] ]; j++ ) {
                        APPEND_BITS_ROW(fmt::format("output_layer_flag[{}][{}]",i,j), 1);
                    }
                }
                ccitem = APPEND_ENABLE_ROW("for( j = 0; j < NumLayersInIdList[ OlsIdxToLsIdx[ i ] ]; j++ )");
                for( j = 0; j < NumLayersInIdList[ OlsIdxToLsIdx[ i ] ]; j++ ) {
                    ENABLE_CELL(ccitem);
                    if( NecessaryLayerFlag[ i ][ j ] && vps_num_profile_tier_level_minus1 > 0 ) {
                        int len = ceil(log2(vps_num_profile_tier_level_minus1 + 1));
                        APPEND_BITS_ROW(fmt::format("profile_tier_level_idx[{}][{}]",i,j), len);
                    }
                }
                ccitem = APPEND_ENABLE_ROW("if( NumOutputLayersInOutputLayerSet[ i ] = = 1 && NumDirectRefLayers[ OlsHighestOutputLayerId[ i ] ] > 0 )");
                if( NumOutputLayersInOutputLayerSet[ i ] == 1 && NumDirectRefLayers[ OlsHighestOutputLayerId[ i ] ] > 0 ) {
                    APPEND_BITS_ROW(fmt::format("alt_output_layer_flag[{}]",i), 1);
                }
            }
            APPEND_GOLOMB_UE_ROW(vps_num_rep_formats_minus1);
            citem = APPEND_ENABLE_ROW("for( i = 0; i <= vps_num_rep_formats_minus1; i++ )");
            for( i = 0; i <= vps_num_rep_formats_minus1; i++ ) {
                ENABLE_CELL(citem);
                auto ccitem = APPEND_ENABLE_ROW(fmt::format("rep_format[{}]",i));
                {
                    ENABLE_CELL(ccitem);
                    APPEND_BITS_ROW("pic_width_vps_in_luma_samples", 16);
                    APPEND_BITS_ROW("pic_height_vps_in_luma_samples", 16);
                    APPEND_BITS_ROW_VAR(chroma_and_bit_depth_vps_present_flag, 1);
                    auto cccitem = APPEND_ENABLE_ROW("if( chroma_and_bit_depth_vps_present_flag ) ");
                    if( chroma_and_bit_depth_vps_present_flag ) {
                        ENABLE_CELL(cccitem);
                        APPEND_BITS_ROW_VAR(chroma_format_vps_idc, 2);
                        auto ccccitem = APPEND_ENABLE_ROW("if( chroma_format_vps_idc = = 3 )");
                        if( chroma_format_vps_idc == 3 ) {
                            ENABLE_CELL(ccccitem);
                            APPEND_BITS_ROW_VAR(separate_colour_plane_vps_flag, 1);
                        }
                        APPEND_BITS_ROW_VAR(bit_depth_vps_luma_minus8, 4);
                        APPEND_BITS_ROW_VAR(bit_depth_vps_chroma_minus8, 4);
                    }
                    APPEND_BITS_ROW_VAR(conformance_window_vps_flag, 1);
                    cccitem = APPEND_ENABLE_ROW("if( conformance_window_vps_flag ) ");
                    if( conformance_window_vps_flag )  {
                        ENABLE_CELL(cccitem);
                        APPEND_GOLOMB_UE_ROW(conf_win_vps_left_offset);
                        APPEND_GOLOMB_UE_ROW(conf_win_vps_right_offset);
                        APPEND_GOLOMB_UE_ROW(conf_win_vps_top_offset);
                        APPEND_GOLOMB_UE_ROW(conf_win_vps_bottom_offset);
                    }
                }
            }
            int rep_format_idx_present_flag = 0;
            citem = APPEND_ENABLE_ROW("if( vps_num_rep_formats_minus1 > 0 )");
            if( vps_num_rep_formats_minus1 > 0 ) {
                ENABLE_CELL(citem);
                rep_format_idx_present_flag = APPEND_BITS_ROW("rep_format_idx_present_flag", 1);
            }
            citem = APPEND_ENABLE_ROW("if( rep_format_idx_present_flag )");
            if( rep_format_idx_present_flag ) {
                ENABLE_CELL(citem);
                for( i = vps_base_layer_internal_flag ? 1 : 0; i <= MaxLayersMinus1; i++ ) {
                    int len = ceil(log2( vps_num_rep_formats_minus1 + 1 ) );
                    APPEND_BITS_ROW(fmt::format("vps_rep_format_idx[{}]",i), len);
                }
            }
            APPEND_BITS_ROW_VAR(max_one_active_ref_layer_flag, 1);
            APPEND_BITS_ROW_VAR(vps_poc_lsb_aligned_flag, 1);
            citem = APPEND_ENABLE_ROW("for( i = 1; i <= MaxLayersMinus1; i++ )");
            for( i = 1; i <= MaxLayersMinus1; i++ ) {
                ENABLE_CELL(citem);
                auto ccitem = APPEND_ENABLE_ROW("if( NumDirectRefLayers[ layer_id_in_nuh[ i ] ] = = 0 )");
                if( NumDirectRefLayers[ layer_id_in_nuh[ i ] ] == 0 ) {
                    int val = APPEND_BITS_ROW(fmt::format("poc_lsb_not_present_flag[[{}]",i), 1);
                    vps->poc_lsb_not_present_flag[i] = val;
                }
            }
            //dpb_size 后边没有再解析
            
            //hevc_vps_extension(ccitem);
        }
        int vps_extension2_flag = APPEND_BITS_ROW("vps_extension2_flag", 1);
        ccitem = APPEND_ENABLE_ROW("if( vps_extension2_flag )");
        if(vps_extension2_flag) {
            ENABLE_CELL(ccitem);
            mdp_header_item *cccitem = APPEND_ENABLE_ROW("while( more_rbsp_data( ) )");
            int index = 0;
            while(more_rbsp_data()) {
                ENABLE_CELL(cccitem);
                int vps_extension_data_flag = APPEND_BITS_ROW(fmt::format("vps_extension_data_flag[{}]",index),1);
                index ++;
            }
        }
    }
    rbsp_trailing_bits(pitem);
    return ret;
}

int MALCodecParser::parse_hvc_sps(mdp_video_header *header, mdp_header_item *item) {
    int ret = 0;
    auto sps = std::dynamic_pointer_cast<MALHEVCSPS>(currentHEVCNal_);
    if (hvcc_) {
        hvcc_->spsList.push_back(sps);
    }
    mdp_header_item *pitem = item;
    int64_t sps_video_parameter_set_id = APPEND_BITS_ROW("sps_video_parameter_set_id",4);
    sps->sps_video_parameter_set_id = sps_video_parameter_set_id;
    int sps_max_sub_layers_minus1 = APPEND_BITS_ROW("sps_max_sub_layers_minus1",3);
    int64_t sps_temporal_id_nesting_flag = APPEND_BITS_ROW("sps_temporal_id_nesting_flag",1);
    hevc_profile_tier_level(pitem,1,sps_max_sub_layers_minus1);
    APPEND_GOLOMB_UE_ROW(sps_seq_parameter_set_id);
    sps->seq_parameter_set_id = sps_seq_parameter_set_id;
    APPEND_GOLOMB_UE_ROW(chroma_format_idc);
    header->sps.chroma_format_idc = chroma_format_idc;
    mdp_header_item *citem = APPEND_ENABLE_ROW("if( chroma_format_idc = = 3 )");
    if(chroma_format_idc == 3) {
        ENABLE_CELL(citem);
        int separate_colour_plane_flag = APPEND_BITS_ROW("separate_colour_plane_flag",1);
        sps->separate_colour_plane_flag = separate_colour_plane_flag;
    }
    APPEND_GOLOMB_UE_ROW(pic_width_in_luma_samples);
    sps->pic_width_in_luma_samples = pic_width_in_luma_samples;
    APPEND_GOLOMB_UE_ROW(pic_height_in_luma_samples);
    sps->pic_height_in_luma_samples = pic_height_in_luma_samples;
    
    int conformance_window_flag = APPEND_BITS_ROW("conformance_window_flag",1);
    citem = APPEND_ENABLE_ROW("if( conformance_window_flag )");
    if( conformance_window_flag ) {
        ENABLE_CELL(citem);
        APPEND_GOLOMB_UE_ROW(conf_win_left_offset);
        APPEND_GOLOMB_UE_ROW(conf_win_right_offset);
        APPEND_GOLOMB_UE_ROW(conf_win_top_offset);
        APPEND_GOLOMB_UE_ROW(conf_win_bottom_offset);
    }
    APPEND_GOLOMB_UE_ROW(bit_depth_luma_minus8);
    APPEND_GOLOMB_UE_ROW(bit_depth_chroma_minus8);
    APPEND_GOLOMB_UE_ROW(log2_max_pic_order_cnt_lsb_minus4);
    sps->log2_max_pic_order_cnt_lsb_minus4 = log2_max_pic_order_cnt_lsb_minus4;
    int sps_sub_layer_ordering_info_present_flag  = APPEND_BITS_ROW("sps_sub_layer_ordering_info_present_flag",1);
    
    citem = APPEND_ENABLE_ROW("for( i = ( sps_sub_layer_ordering_info_present_flag ? 0 : sps_max_sub_layers_minus1 ); i <= sps_max_sub_layers_minus1; i++ )");
    for( int i = ( sps_sub_layer_ordering_info_present_flag ? 0 : sps_max_sub_layers_minus1 ); i <= sps_max_sub_layers_minus1; i++ ) {
        ENABLE_CELL(citem);
        APPEND_GOLOMB_UE_ROW(sps_max_dec_pic_buffering_minus1);
        APPEND_GOLOMB_UE_ROW(sps_max_num_reorder_pics);
        APPEND_GOLOMB_UE_ROW(sps_max_latency_increase_plus1);
    }
    APPEND_GOLOMB_UE_ROW(log2_min_luma_coding_block_size_minus3);
    sps->log2_min_luma_coding_block_size_minus3 = log2_min_luma_coding_block_size_minus3;
    APPEND_GOLOMB_UE_ROW(log2_diff_max_min_luma_coding_block_size);
    sps->log2_diff_max_min_luma_coding_block_size = log2_diff_max_min_luma_coding_block_size;
    APPEND_GOLOMB_UE_ROW(log2_min_luma_transform_block_size_minus2);
    APPEND_GOLOMB_UE_ROW(log2_diff_max_min_luma_transform_block_size);
    APPEND_GOLOMB_UE_ROW(max_transform_hierarchy_depth_inter);
    APPEND_GOLOMB_UE_ROW(max_transform_hierarchy_depth_intra);
    
    int scaling_list_enabled_flag = APPEND_BITS_ROW("scaling_list_enabled_flag",1);;
    
    citem = APPEND_ENABLE_ROW("if( scaling_list_enabled_flag )");
    if(scaling_list_enabled_flag) {
        ENABLE_CELL(citem);
        int sps_scaling_list_data_present_flag = APPEND_BITS_ROW("sps_scaling_list_data_present_flag",1);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( sps_scaling_list_data_present_flag )");
        if(sps_scaling_list_data_present_flag) {
            pitem = ccitem;
            enable_or_disable_cell(pitem, 1);
            hevc_scaling_list_data(pitem);
        }
    }
    int amp_enabled_flag =  APPEND_BITS_ROW("amp_enabled_flag",1);
    int sample_adaptive_offset_enabled_flag = APPEND_BITS_ROW("sample_adaptive_offset_enabled_flag",1);
    int pcm_enabled_flag = APPEND_BITS_ROW("pcm_enabled_flag",1);
    citem = APPEND_ENABLE_ROW("if( pcm_enabled_flag )");
    if( pcm_enabled_flag ) {
        ENABLE_CELL(citem);
        int pcm_sample_bit_depth_luma_minus1 = APPEND_BITS_ROW("pcm_sample_bit_depth_luma_minus1",4);
        int pcm_sample_bit_depth_chroma_minus1 = APPEND_BITS_ROW("pcm_sample_bit_depth_chroma_minus1",4);
        APPEND_GOLOMB_UE_ROW(log2_min_pcm_luma_coding_block_size_minus3);
        APPEND_GOLOMB_UE_ROW(log2_diff_max_min_pcm_luma_coding_block_size);
        int pcm_loop_filter_disabled_flag = APPEND_BITS_ROW("pcm_loop_filter_disabled_flag",1);
    }
    APPEND_GOLOMB_UE_ROW(num_short_term_ref_pic_sets);
    sps->num_short_term_ref_pic_sets = num_short_term_ref_pic_sets;
    citem = APPEND_ENABLE_ROW("for( i = 0; i < num_short_term_ref_pic_sets; i++)");
    int num_delta_pocs[64] = {0};
    int num_negative_pics[64] = {0};
    int num_positive_pics[64] = {0};
    for( int i = 0; i < num_short_term_ref_pic_sets; i++) {
        ENABLE_CELL(citem);
        mdp_header_item * ccitem = APPEND_ENABLE_ROW(fmt::format("st_ref_pic_set[{}]",i));
        enable_or_disable_cell(ccitem, 1);
        hevc_st_ref_pic_set(ccitem,i,num_short_term_ref_pic_sets,num_delta_pocs,num_negative_pics,num_positive_pics);
    }
    int long_term_ref_pics_present_flag = APPEND_BITS_ROW("long_term_ref_pics_present_flag",1);
    citem = APPEND_ENABLE_ROW("if( long_term_ref_pics_present_flag )");
    if(long_term_ref_pics_present_flag) {
        ENABLE_CELL(citem);
        APPEND_GOLOMB_UE_ROW(num_long_term_ref_pics_sps);
        
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("for( i = 0; i < num_long_term_ref_pics_sps; i++ ) ");
        for(int i = 0; i < num_long_term_ref_pics_sps; i++) {
            ENABLE_CELL(ccitem);
            APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("lt_ref_pic_poc_lsb_sps[{}]",i));
            int used_by_curr_pic_lt_sps_flag = APPEND_BITS_ROW(fmt::format("used_by_curr_pic_lt_sps_flag[{}]",i),1);
        }
    }
    int sps_temporal_mvp_enabled_flag = APPEND_BITS_ROW("sps_temporal_mvp_enabled_flag",1);
    int strong_intra_smoothing_enabled_flag = APPEND_BITS_ROW("strong_intra_smoothing_enabled_flag",1);
    int vui_parameters_present_flag = APPEND_BITS_ROW("vui_parameters_present_flag",1);
    citem = APPEND_ENABLE_ROW("if( vui_parameters_present_flag )");
    if( vui_parameters_present_flag ) {
        ENABLE_CELL(citem);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("vui_parameters");
        enable_or_disable_cell(ccitem, 1);
        vui_parameters(1,ccitem);
    }
    int sps_extension_present_flag = APPEND_BITS_ROW("sps_extension_present_flag",1);
    citem = APPEND_ENABLE_ROW("if( sps_extension_present_flag ) ");
    int sps_range_extension_flag = 0;
    int sps_multilayer_extension_flag = 0;
    int sps_3d_extension_flag = 0;
    int sps_extension_5bits = 0;
    if( sps_extension_present_flag ) {
        ENABLE_CELL(citem);
        sps_range_extension_flag = APPEND_BITS_ROW("sps_range_extension_flag",1);
        sps_multilayer_extension_flag = APPEND_BITS_ROW("sps_multilayer_extension_flag",1);
        sps_3d_extension_flag = APPEND_BITS_ROW("sps_3d_extension_flag",1);
        sps_extension_5bits = APPEND_BITS_ROW("sps_extension_5bits",5);
    }
    citem = APPEND_ENABLE_ROW("if( sps_range_extension_flag ) ");
    if( sps_range_extension_flag ) {
        ENABLE_CELL(citem);
        int transform_skip_rotation_enabled_flag = APPEND_BITS_ROW("transform_skip_rotation_enabled_flag",1);
        int transform_skip_context_enabled_flag = APPEND_BITS_ROW("transform_skip_context_enabled_flag",1);
        int implicit_rdpcm_enabled_flag = APPEND_BITS_ROW("implicit_rdpcm_enabled_flag",1);
        int explicit_rdpcm_enabled_flag = APPEND_BITS_ROW("explicit_rdpcm_enabled_flag",1);
        int extended_precision_processing_flag = APPEND_BITS_ROW("extended_precision_processing_flag",1);
        int intra_smoothing_disabled_flag = APPEND_BITS_ROW("intra_smoothing_disabled_flag",1);
        int high_precision_offsets_enabled_flag = APPEND_BITS_ROW("high_precision_offsets_enabled_flag",1);
        int persistent_rice_adaptation_enabled_flag = APPEND_BITS_ROW("persistent_rice_adaptation_enabled_flag",1);
        int cabac_bypass_alignment_enabled_flag = APPEND_BITS_ROW("cabac_bypass_alignment_enabled_flag",1);
    }
    citem = APPEND_ENABLE_ROW("if( sps_multilayer_extension_flag )");
    if( sps_multilayer_extension_flag ) {
        ENABLE_CELL(citem);
        int inter_view_mv_vert_constraint_flag = APPEND_BITS_ROW("inter_view_mv_vert_constraint_flag",1);
    }
    citem = APPEND_ENABLE_ROW("if( sps_3d_extension_flag )");
    if( sps_3d_extension_flag ) {
        ENABLE_CELL(citem);
        hevc_sps_3d_extension(citem);
    }
    citem = APPEND_ENABLE_ROW("if( sps_extension_5bits )");
    if( sps_extension_5bits ) {
        ENABLE_CELL(citem);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("while( more_rbsp_data( ) )");
        enable_or_disable_cell(ccitem, 1);
        int index = 0;
        while(more_rbsp_data()) {
            int vps_extension_data_flag = APPEND_BITS_ROW(fmt::format("vps_extension_data_flag[{}]",index),1);
            index ++;
        }
    }
    rbsp_trailing_bits(pitem);
    //    if(mediainfo_->currentTrack) {
    //        if(rawProfile.general_profile_idc == 1 || rawProfile.general_profile_compatibility_flag[1]) {
    //            mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("profile","Main"));
    //        } else if(rawProfile.general_profile_idc == 2 || rawProfile.general_profile_compatibility_flag[2]) {
    //            if(rawProfile.general_one_picture_only_constraint_flag) {
    //                mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("profile","Main 10 Still Picture"));
    //            } else {
    //                mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("profile","Main 10"));
    //            }
    //        } else if(rawProfile.general_profile_idc == 3 || rawProfile.general_profile_compatibility_flag[3]) {
    //            mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("profile","Main Still Picture"));
    //        } else if(rawProfile.general_profile_idc == 4 || rawProfile.general_profile_compatibility_flag[4]) {  //format range extensions profiles
    //            std::vector<std::map<std::string,std::vector<int>>> profiles{
    //                {{"Monochrome",{1,1,1,1,1,1,0,0,1}}},
    //                {{"Monochrome10",{1,1,0,1,1,1,0,0,1}}},
    //                {{"Monochrome12",{1,0,0,1,1,1,0,0,1}}},
    //                {{"Monochrome16",{0,0,0,1,1,1,0,0,1}}},
    //                {{"Main 12",{1,0,0,1,1,0,0,0,1}}},
    //                {{"Main 4:2:2 10",{1,1,0,1,0,0,0,0,1}}},
    //                {{"Main 4:2:2 12",{1,0,0,1,0,0,0,0,1}}},
    //                {{"Main 4:4:4",{1,1,1,0,0,0,0,0,1}}},
    //                {{"Main 4:4:4 10",{1,1,0,0,0,0,0,0,1}}},
    //                {{"Main 4:4:4 12",{1,0,0,0,0,0,0,0,1}}},
    //
    //                {{"Main Intra",{1,1,1,1,1,0,1,0,1}}}, //后边这几个最后一个判断条件是0或者1
    //                {{"Main 10 Intra",{1,1,0,1,1,0,1,0,1}}},
    //                {{"Main 12 Intra",{1,0,0,1,1,0,1,0,1}}},
    //                {{"Main 4:2:2 10 Intra",{1,1,0,1,0,0,1,0,1}}},
    //                {{"Main 4:2:2 12 Intra",{1,0,0,1,0,0,1,0,1}}},
    //                {{"Main 4:4:4 Intra",{1,1,1,0,0,0,1,0,1}}},
    //                {{"Main 4:4:4 10 Intra",{1,1,0,0,0,0,1,0,1}}},
    //                {{"Main 4:4:4 12 Intra",{1,0,0,0,0,0,1,0,1}}},
    //                {{"Main 4:4:4 16 Intra",{0,0,0,0,0,0,1,0,1}}},
    //                {{"Main 4:4:4 Still Picture",{1,1,1,0,0,0,1,1,1}}},
    //                {{"Main 4:4:4 16 Still Picture",{0,0,0,0,0,0,1,1,1}}},
    //
    //            };
    //            std::vector<int> currentProfile = {rawProfile.general_max_12bit_constraint_flag,rawProfile.general_max_10bit_constraint_flag,
    //                rawProfile.general_max_8bit_constraint_flag,rawProfile.general_max_422chroma_constraint_flag,
    //                rawProfile.general_max_420chroma_constraint_flag,rawProfile.general_max_monochrome_constraint_flag,
    //                rawProfile.general_intra_constraint_flag,rawProfile.general_one_picture_only_constraint_flag,rawProfile.general_lower_bit_rate_constraint_flag};
    //            std::string profile = "unknown";
    //            int profile_index = 0;
    //            for(auto it = profiles.begin(); it != profiles.end(); it++) {
    //                profile_index = std::distance(profiles.begin(),it);
    //                std::map<std::string,std::vector<int>> map = (*it);
    //                std::string key;
    //                std::vector<int> vec;
    //                for(auto im = map.begin(); im != map.end(); im++) {
    //                    key = im->first;
    //                    vec = im->second;
    //                }
    //                if(profile_index < 10) {
    //                    if(vec == currentProfile) {
    //                        profile = key;
    //                        break;
    //                    }
    //                } else {
    //                    int index;
    //                    for(auto iv = vec.begin(); iv != vec.end(); iv++) {
    //                        index = std::distance(vec.begin(), iv);
    //                        if(*iv != currentProfile[index] && index < 8) {
    //                            break;
    //                        }
    //                        if(index == 8) { //最后一个0或者1 都可以
    //                            if(*iv != 0 && *iv != 1) {
    //                                break;
    //                            }
    //                        }
    //                        profile = key;
    //                    }
    //                }
    //            }
    //            mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("profile",profile));
    //        } else if(rawProfile.general_profile_idc == 5 || rawProfile.general_profile_compatibility_flag[5]) {
    //            std::vector<std::map<std::string,std::vector<int>>> profiles{
    //                {{"High Throughput 4:4:4",{1,1,1,1,0,0,0,0,0,1}}},
    //                {{"High Throughput 4:4:4 10",{1,1,1,0,0,0,0,0,0,1}}},
    //                {{"High Throughput 4:4:4 14",{1,0,0,0,0,0,0,0,0,1}}},
    //                {{"High Throughput 4:4:4 16 Intra",{0,0,0,0,0,0,0,1,0,1}}}
    //            };
    //
    //            std::vector<int> currentProfile = {rawProfile.general_max_14bit_constraint_flag,rawProfile.general_max_12bit_constraint_flag,rawProfile.general_max_10bit_constraint_flag,
    //                rawProfile.general_max_8bit_constraint_flag,rawProfile.general_max_422chroma_constraint_flag,
    //                rawProfile.general_max_420chroma_constraint_flag,rawProfile.general_max_monochrome_constraint_flag,
    //                rawProfile.general_intra_constraint_flag,rawProfile.general_one_picture_only_constraint_flag,rawProfile.general_lower_bit_rate_constraint_flag};
    //            std::string profile = "unknown";
    //            int profile_index = 0;
    //            for(auto it = profiles.begin(); it != profiles.end(); it++) {
    //                profile_index = std::distance(profiles.begin(),it);
    //                std::map<std::string,std::vector<int>> map = (*it);
    //                std::string key;
    //                std::vector<int> vec;
    //                for(auto im = map.begin(); im != map.end(); im++) {
    //                    key = im->first;
    //                    vec = im->second;
    //                }
    //                if(profile_index < 3) {
    //                    if(vec == currentProfile) {
    //                        profile = key;
    //                        break;
    //                    }
    //                } else {
    //                    int index;
    //                    for(auto iv = vec.begin(); iv != vec.end(); iv++) {
    //                        index = std::distance(vec.begin(), iv);
    //                        if(*iv != currentProfile[index] && index < 9) {
    //                            break;
    //                        }
    //                        if(index == 9) { //最后一个0或者1 都可以
    //                            if(*iv != 0 && *iv != 1) {
    //                                break;
    //                            }
    //                        }
    //                        profile = key;
    //                    }
    //                }
    //            }
    //            mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("profile",profile));
    //        } else if(rawProfile.general_profile_idc == 9 || rawProfile.general_profile_compatibility_flag[9]) {
    //            std::vector<std::map<std::string,std::vector<int>>> profiles{
    //                {{"Screen-ExtendedMain",{1,1,1,1,1,1,0,0,0,1}}},
    //                {{"Screen-Extended Main 10",{1,1,1,0,1,1,0,0,0,1}}},
    //                {{"Screen-Extended Main 4:4:4",{1,1,1,1,0,0,0,0,0,1}}},
    //                {{"Screen-Extended Main 4:4:4 10",{1,1,1,0,0,0,0,0,0,1}}}
    //            };
    //
    //            std::vector<int> currentProfile = {rawProfile.general_max_14bit_constraint_flag,rawProfile.general_max_12bit_constraint_flag,rawProfile.general_max_10bit_constraint_flag,
    //                rawProfile.general_max_8bit_constraint_flag,rawProfile.general_max_422chroma_constraint_flag,
    //                rawProfile.general_max_420chroma_constraint_flag,rawProfile.general_max_monochrome_constraint_flag,
    //                rawProfile.general_intra_constraint_flag,rawProfile.general_one_picture_only_constraint_flag,rawProfile.general_lower_bit_rate_constraint_flag};
    //            std::string profile = "unknown";
    //            int profile_index = 0;
    //            for(auto it = profiles.begin(); it != profiles.end(); it++) {
    //                profile_index = std::distance(profiles.begin(),it);
    //                std::map<std::string,std::vector<int>> map = (*it);
    //                std::string key;
    //                std::vector<int> vec;
    //                for(auto im = map.begin(); im != map.end(); im++) {
    //                    key = im->first;
    //                    vec = im->second;
    //                }
    //                if(vec == currentProfile) {
    //                    profile = key;
    //                    break;
    //                }
    //            }
    //            mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("profile",profile));
    //        }
    //    }
    return ret;
}
void MALCodecParser::hevc_pps_range_extension(mdp_header_item *pitem,int transform_skip_enabled_flag) {
    mdp_header_item *citem = APPEND_ENABLE_ROW("pps_range_extension");
    enable_or_disable_cell(citem, 1);
    pitem = citem;
    mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( transform_skip_enabled_flag )");
    if( transform_skip_enabled_flag ) {
        ENABLE_CELL(ccitem);
        APPEND_GOLOMB_UE_ROW(log2_max_transform_skip_block_size_minus2);
    }
    int cross_component_prediction_enabled_flag = APPEND_BITS_ROW("cross_component_prediction_enabled_flag", 1);
    int chroma_qp_offset_list_enabled_flag = APPEND_BITS_ROW("chroma_qp_offset_list_enabled_flag", 1);
    ccitem = APPEND_ENABLE_ROW("if( chroma_qp_offset_list_enabled_flag )");
    if( chroma_qp_offset_list_enabled_flag ) {
        ENABLE_CELL(ccitem);
        APPEND_GOLOMB_UE_ROW(diff_cu_chroma_qp_offset_depth);
        APPEND_GOLOMB_UE_ROW(chroma_qp_offset_list_len_minus1);
        mdp_header_item *cccitem = APPEND_ENABLE_ROW("for( i = 0; i <= chroma_qp_offset_list_len_minus1; i++ ) ");
        for(int i = 0; i <= chroma_qp_offset_list_len_minus1; i++ )  {
            ENABLE_CELL(cccitem);
            int cb_qp_offset_list = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("cb_qp_offset_list[{}]",i));
            int cr_qp_offset_list = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("cr_qp_offset_list[{}]",i));
        }
    }
    APPEND_GOLOMB_UE_ROW(log2_sao_offset_scale_luma);
    APPEND_GOLOMB_UE_ROW(log2_sao_offset_scale_chroma);
    
}
void MALCodecParser::hevc_colour_mapping_octants(mdp_header_item *pitem, int inpDepth, int idxY, int idxCb, int idxCr, int inpLength,int cm_octant_depth,int PartNumY,int cm_delta_flc_bits_minus1) {
    mdp_header_item *citem = APPEND_ENABLE_ROW("colour_mapping_octants");
    enable_or_disable_cell(citem, 1);
    pitem = citem;
    mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( inpDepth < cm_octant_depth )");
    int split_octant_flag = 0;
    if( inpDepth < cm_octant_depth ) {
        ENABLE_CELL(ccitem);
        split_octant_flag = APPEND_BITS_ROW("split_octant_flag", 1);
    }
    ccitem = APPEND_ENABLE_ROW("if( split_octant_flag )");
    if( split_octant_flag ) {
        ENABLE_CELL(ccitem);
        for( int k = 0; k < 2; k++ ) {
            for(int m = 0; m < 2; m++ ) {
                for( int n = 0; n < 2; n++ ) {
                    hevc_colour_mapping_octants(ccitem,inpDepth + 1, idxY + PartNumY * k * inpLength / 2, idxCb + m * inpLength / 2, idxCr + n * inpLength / 2, inpLength / 2,cm_octant_depth,PartNumY,cm_delta_flc_bits_minus1);
                }
            }
        }
    } else {
        ccitem = APPEND_ENABLE_ROW("else");
        ENABLE_CELL(ccitem);
        mdp_header_item *cccitem = APPEND_ENABLE_ROW("for( i = 0; i < PartNumY; i++ )");
        for( int i = 0; i < PartNumY; i++ ) {
            ENABLE_CELL(cccitem);
            int idxShiftY = idxY + ((i << (cm_octant_depth - inpDepth)));
            mdp_header_item *ccccitem = APPEND_ENABLE_ROW("for( j = 0; j < 4; j++ )");
            for(int j = 0; j < 4; j++ ) {
                ENABLE_CELL(ccccitem);
                int coded_res_flag = APPEND_BITS_ROW(fmt::format("coded_res_flag[{}][{}][{}][{}]",idxShiftY,idxCb,idxCr,j), 1);
                mdp_header_item *cccccitem = APPEND_ENABLE_ROW(fmt::format("if( coded_res_flag[%d][%d][%d][%d])",idxShiftY,idxCb,idxCr,j));
                if(coded_res_flag) {
                    ENABLE_CELL(cccccitem);
                    mdp_header_item *ccccccitem = APPEND_ENABLE_ROW("for( c = 0; c < 3; c++ ) ");
                    for( int c = 0; c < 3; c++ )  {
                        ENABLE_CELL(ccccccitem);
                        int res_coeff_q =   APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("res_coeff_q[{}][{}][{}][{}][{}]",idxShiftY,idxCb,idxCr,j,c));
                        int res_coeff_r = APPEND_BITS_ROW(fmt::format("res_coeff_r[{}][{}][{}][{}][{}]",idxShiftY,idxCb,idxCr,j,c), (cm_delta_flc_bits_minus1+1));
                        
                        mdp_header_item *cccccccitem = APPEND_ENABLE_ROW("if( res_coeff_q[ idxShiftY ][ idxCb ][ idxCr ][ j ][ c ] | | res_coeff_r[ idxShiftY ][ idxCb ][ idxCr ][ j ][ c ] ) ");
                        if( res_coeff_q || res_coeff_r) {
                            ENABLE_CELL(cccccccitem);
                            int res_coeff_s = APPEND_BITS_ROW(fmt::format("res_coeff_s[{}][{}][{}][{}][{}]",idxShiftY,idxCb,idxCr,j,c), 1);
                        }
                    }
                }
            }
        }
    }
}
void MALCodecParser::hevc_colour_mapping_table(mdp_header_item *pitem) {
    mdp_header_item *citem = APPEND_ENABLE_ROW("colour_mapping_table");
    enable_or_disable_cell(citem, 1);
    pitem = citem;
    APPEND_GOLOMB_UE_ROW(num_cm_ref_layers_minus1);
    mdp_header_item *ccitem = APPEND_ENABLE_ROW("for( i = 0; i <= num_cm_ref_layers_minus1; i++ )");
    for(int i = 0; i <= num_cm_ref_layers_minus1; i++ ) {
        ENABLE_CELL(ccitem);
        int cm_ref_layer_id = APPEND_BITS_ROW(fmt::format("cm_ref_layer_id[{}]",i), 6);
    }
    int cm_octant_depth = APPEND_BITS_ROW("cm_octant_depth", 2);
    
    int cm_y_part_num_log2 = APPEND_BITS_ROW("cm_y_part_num_log2", 2);
    
    int PartNumY = 1 << cm_y_part_num_log2;
    
    APPEND_GOLOMB_UE_ROW(luma_bit_depth_cm_input_minus8);
    APPEND_GOLOMB_UE_ROW(chroma_bit_depth_cm_input_minus8);
    APPEND_GOLOMB_UE_ROW(luma_bit_depth_cm_output_minus8);
    APPEND_GOLOMB_UE_ROW(chroma_bit_depth_cm_output_minus8);
    
    int cm_res_quant_bits = APPEND_BITS_ROW("cm_res_quant_bits", 2);
    
    int cm_delta_flc_bits_minus1 = APPEND_BITS_ROW("cm_delta_flc_bits_minus1", 2);
    ccitem = APPEND_ENABLE_ROW("if( cm_octant_depth = = 1 ) ");
    if( cm_octant_depth == 1 )  {
        ENABLE_CELL(ccitem);
        APPEND_GOLOMB_SE_ROW(cm_adapt_threshold_u_delta);
        APPEND_GOLOMB_SE_ROW(cm_adapt_threshold_v_delta);
    }
    hevc_colour_mapping_octants(citem,0,0,0,0,1 << cm_octant_depth,cm_octant_depth,PartNumY,cm_delta_flc_bits_minus1);
}
void MALCodecParser::hevc_pps_multilayer_extension(mdp_header_item *pitem) {
    mdp_header_item *citem = APPEND_ENABLE_ROW("pps_multilayer_extension");
    enable_or_disable_cell(citem, 1);
    pitem = citem;
    int poc_reset_info_present_flag = APPEND_BITS_ROW("poc_reset_info_present_flag", 1);
    int pps_infer_scaling_list_flag = APPEND_BITS_ROW("pps_infer_scaling_list_flag", 1);
    mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( pps_infer_scaling_list_flag )");
    if( pps_infer_scaling_list_flag ) {
        ENABLE_CELL(ccitem);
        int pps_scaling_list_ref_layer_id = APPEND_BITS_ROW("pps_scaling_list_ref_layer_id", 6);
    }
    APPEND_GOLOMB_UE_ROW(num_ref_loc_offsets);
    ccitem = APPEND_ENABLE_ROW("for( i = 0; i < num_ref_loc_offsets; i++ )");
    for( int i = 0; i < num_ref_loc_offsets; i++ ) {
        ENABLE_CELL(ccitem);
        int ref_loc_offset_layer_id = APPEND_BITS_ROW(fmt::format("ref_loc_offset_layer_id[{}]",i), 6);
        int scaled_ref_layer_offset_present_flag = APPEND_BITS_ROW(fmt::format("scaled_ref_layer_offset_present_flag[{}]",i), 1);
        mdp_header_item *cccitem = APPEND_ENABLE_ROW(fmt::format("if( scaled_ref_layer_offset_present_flag[{}] )",i));
        if( scaled_ref_layer_offset_present_flag) {
            ENABLE_CELL(cccitem);
            int scaled_ref_layer_left_offset = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("scaled_ref_layer_left_offset[{}]",i));
            int scaled_ref_layer_top_offset = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("scaled_ref_layer_top_offset[{}]",i));
            int scaled_ref_layer_right_offset = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("scaled_ref_layer_right_offset[{}]",i));
            int scaled_ref_layer_bottom_offset = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("scaled_ref_layer_bottom_offset[{}]",i));
            
        }
        int ref_region_offset_present_flag = APPEND_BITS_ROW(fmt::format("ref_region_offset_present_flag[{}]",i), 1);
        cccitem = APPEND_ENABLE_ROW(fmt::format("if( ref_region_offset_present_flag[ {} ] ) ",i));
        if( ref_region_offset_present_flag) {
            ENABLE_CELL(cccitem);
            int ref_region_left_offset = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("ref_region_left_offset[{}]",i));
            int ref_region_top_offset = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("ref_region_top_offset[{}]",i));
            int ref_region_right_offset = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("ref_region_right_offset[{}]",i));
            int ref_region_bottom_offset = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("ref_region_bottom_offset[{}]",i));
        }
        
        int resample_phase_set_present_flag = APPEND_BITS_ROW(fmt::format("resample_phase_set_present_flag[{}]",i), 1);
        cccitem = APPEND_ENABLE_ROW(fmt::format("if( resample_phase_set_present_flag[ %d ] ) ",i));
        if( resample_phase_set_present_flag) {
            ENABLE_CELL(cccitem);
            int phase_hor_luma = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("phase_hor_luma[{}]",i));
            int phase_ver_luma = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("phase_ver_luma[{}]",i));
            int phase_hor_chroma_plus8 = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("phase_hor_chroma_plus8[{}]",i));
            int phase_ver_chroma_plus8 = APPEND_GOLOMB_UE_ROW_NO_VAR(fmt::format("phase_ver_chroma_plus8[{}]",i));
        }
        
        int colour_mapping_enabled_flag = APPEND_BITS_ROW(fmt::format("colour_mapping_enabled_flag[{}]",i), 1);
        cccitem = APPEND_ENABLE_ROW(fmt::format("if( colour_mapping_enabled_flag[ %d ] ) ",i));
        if( colour_mapping_enabled_flag) {
            ENABLE_CELL(cccitem);
            hevc_colour_mapping_table(cccitem);
        }
    }
}
void MALCodecParser::hevc_delta_dlt(mdp_header_item *pitem, int i, int pps_bit_depth_for_depth_layers_minus8){
    mdp_header_item *citem = APPEND_ENABLE_ROW("delta_dlt");
    enable_or_disable_cell(citem, 1);
    pitem = citem;
    int num_val_delta_dlt = APPEND_BITS_ROW("num_val_delta_dlt", pps_bit_depth_for_depth_layers_minus8 + 8);
    mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( num_val_delta_dlt > 0 ) ");
    if( num_val_delta_dlt > 0 ){
        ENABLE_CELL(ccitem);
        mdp_header_item *cccitem = APPEND_ENABLE_ROW("if( num_val_delta_dlt > 1 )");
        int max_diff = 0;
        if( num_val_delta_dlt > 1 ) {
            ENABLE_CELL(cccitem);
            int max_diff = APPEND_BITS_ROW("max_diff", pps_bit_depth_for_depth_layers_minus8 + 8);
        }
        cccitem = APPEND_ENABLE_ROW("if( num_val_delta_dlt > 2 && max_diff > 0 )");
        int min_diff_minus1 = max_diff - 1;
        if( num_val_delta_dlt > 2 && max_diff > 0 ) {
            ENABLE_CELL(cccitem);
            int min_diff_minus1 = APPEND_BITS_ROW("max_diff", std::ceil(std::log(max_diff+1)));
        }
        int delta_dlt_val0 = APPEND_BITS_ROW("delta_dlt_val0", pps_bit_depth_for_depth_layers_minus8 + 8);
        cccitem = APPEND_ENABLE_ROW("if( max_diff > ( min_diff_minus1 + 1 ) )");
        if( max_diff > ( min_diff_minus1 + 1 ) ) {
            ENABLE_CELL(cccitem);
            mdp_header_item *ccccitem = APPEND_ENABLE_ROW("for( k = 1; k < num_val_delta_dlt; k++ )");
            int minDiff = ( min_diff_minus1 + 1 );
            for( int k = 1; k < num_val_delta_dlt; k++ ) {
                ENABLE_CELL(ccccitem);
                APPEND_BITS_ROW(fmt::format("delta_val_diff_minus_min[{}]",k), std::ceil(std::log(max_diff-minDiff+1)));
            }
        }
    }
    
}
void MALCodecParser::hevc_pps_3d_extension(mdp_header_item *pitem) {
    mdp_header_item *citem = APPEND_ENABLE_ROW("pps_3d_extension");
    enable_or_disable_cell(citem, 1);
    pitem = citem;
    int dlts_present_flag = APPEND_BITS_ROW("dlts_present_flag", 1);
    mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( dlts_present_flag ) ");
    if( dlts_present_flag ){
        ENABLE_CELL(ccitem);
        int pps_depth_layers_minus1 = APPEND_BITS_ROW("pps_depth_layers_minus1", 6);
        int pps_bit_depth_for_depth_layers_minus8 = APPEND_BITS_ROW("pps_bit_depth_for_depth_layers_minus8", 4);
        mdp_header_item *cccitem = APPEND_ENABLE_ROW("for( i = 0; i <= pps_depth_layers_minus1; i++ ) ");
        for(int i = 0; i <= pps_depth_layers_minus1; i++ ) {
            ENABLE_CELL(cccitem);
            int dlt_flag = APPEND_BITS_ROW(fmt::format("dlt_flag[{}]",i), 1);
            mdp_header_item *ccccitem = APPEND_ENABLE_ROW(fmt::format("if( dlt_flag[{}] )",i));
            if(dlt_flag) {
                ENABLE_CELL(ccccitem);
                int dlt_pred_flag = APPEND_BITS_ROW(fmt::format("dlt_pred_flag[{}]",i), 1);
                mdp_header_item *cccccitem = APPEND_ENABLE_ROW(fmt::format("if( !dlt_pred_flag[{}] )",i));
                int dlt_val_flags_present_flag = 0;
                if( !dlt_pred_flag) {
                    ENABLE_CELL(cccccitem);
                    dlt_val_flags_present_flag = APPEND_BITS_ROW(fmt::format("dlt_val_flags_present_flag[{}]",i), 1);
                }
                cccccitem = APPEND_ENABLE_ROW(fmt::format("if( dlt_val_flags_present_flag[{}] )",i));
                if( dlt_val_flags_present_flag) {
                    ENABLE_CELL(cccccitem);
                    mdp_header_item *ccccccitem = APPEND_ENABLE_ROW("for( j = 0; j <= depthMaxValue; j++ )");
                    int depthMaxValue = ( 1 << ( pps_bit_depth_for_depth_layers_minus8 + 8 ) ) - 1;
                    for( int j = 0; j <= depthMaxValue; j++ ) {
                        ENABLE_CELL(ccccccitem);
                        int dlt_value_flag = APPEND_BITS_ROW(fmt::format("dlt_value_flag[{}][{}]",i,j), 1);
                    }
                } else {
                    cccccitem = APPEND_ENABLE_ROW("else");
                    ENABLE_CELL(cccccitem);
                    hevc_delta_dlt(cccccitem,i,pps_bit_depth_for_depth_layers_minus8);
                }
            }
        }
    }
}
int MALCodecParser::parse_hvc_pps(mdp_video_header *header, mdp_header_item *item) {
    int ret = 0;
    auto pps = std::dynamic_pointer_cast<MALHEVCPPS>(currentHEVCNal_);
    if (hvcc_) {
        hvcc_->ppsList.push_back(pps);
    }
    mdp_header_item *pitem = item;
    APPEND_GOLOMB_UE_ROW(pps_pic_parameter_set_id);
    pps->pic_parameter_set_id = pps_pic_parameter_set_id;
    APPEND_GOLOMB_UE_ROW(pps_seq_parameter_set_id);
    pps->seq_parameter_set_id = pps_seq_parameter_set_id;
    int dependent_slice_segments_enabled_flag = APPEND_BITS_ROW("dependent_slice_segments_enabled_flag", 1);
    pps->dependent_slice_segments_enabled_flag = dependent_slice_segments_enabled_flag;
    int output_flag_present_flag = APPEND_BITS_ROW("output_flag_present_flag", 1);
    pps->output_flag_present_flag = output_flag_present_flag;
    int num_extra_slice_header_bits = APPEND_BITS_ROW("num_extra_slice_header_bits", 3);
    pps->num_extra_slice_header_bits = num_extra_slice_header_bits;
    int sign_data_hiding_enabled_flag = APPEND_BITS_ROW("sign_data_hiding_enabled_flag", 1);
    int cabac_init_present_flag = APPEND_BITS_ROW("cabac_init_present_flag", 1);
    APPEND_GOLOMB_UE_ROW(num_ref_idx_l0_default_active_minus1);
    APPEND_GOLOMB_UE_ROW(num_ref_idx_l1_default_active_minus1);
    APPEND_GOLOMB_SE_ROW(init_qp_minus26);
    int constrained_intra_pred_flag = APPEND_BITS_ROW("constrained_intra_pred_flag", 1);
    int transform_skip_enabled_flag = APPEND_BITS_ROW("transform_skip_enabled_flag", 1);
    int cu_qp_delta_enabled_flag = APPEND_BITS_ROW("cu_qp_delta_enabled_flag", 1);
    mdp_header_item *citem = APPEND_ENABLE_ROW("if( cu_qp_delta_enabled_flag )");
    if( cu_qp_delta_enabled_flag ) {
        ENABLE_CELL(citem);
        APPEND_GOLOMB_UE_ROW(diff_cu_qp_delta_depth);
    }
    APPEND_GOLOMB_SE_ROW(pps_cb_qp_offset);
    APPEND_GOLOMB_SE_ROW(pps_cr_qp_offset);
    
    
    int pps_slice_chroma_qp_offsets_present_flag = APPEND_BITS_ROW("pps_slice_chroma_qp_offsets_present_flag", 1);
    int weighted_pred_flag = APPEND_BITS_ROW("weighted_pred_flag", 1);
    int weighted_bipred_flag = APPEND_BITS_ROW("weighted_bipred_flag", 1);
    int transquant_bypass_enabled_flag = APPEND_BITS_ROW("transquant_bypass_enabled_flag", 1);
    int tiles_enabled_flag = APPEND_BITS_ROW("tiles_enabled_flag", 1);
    int entropy_coding_sync_enabled_flag = APPEND_BITS_ROW("entropy_coding_sync_enabled_flag", 1);
    citem = APPEND_ENABLE_ROW("if( tiles_enabled_flag ) ");
    if( tiles_enabled_flag )  {
        ENABLE_CELL(citem);
        APPEND_GOLOMB_SE_ROW(num_tile_columns_minus1);
        APPEND_GOLOMB_SE_ROW(num_tile_rows_minus1);
        
        int uniform_spacing_flag = APPEND_BITS_ROW("uniform_spacing_flag", 1);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( !uniform_spacing_flag ) ");
        if( !uniform_spacing_flag ) {
            ENABLE_CELL(ccitem);
            mdp_header_item *cccitem = APPEND_ENABLE_ROW("for( i = 0; i < num_tile_columns_minus1; i++ ) ");
            for( int i = 0; i < num_tile_columns_minus1; i++ ) {
                ENABLE_CELL(cccitem);
                int column_width_minus1 = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("column_width_minus1[{}]",i));
            }
            cccitem = APPEND_ENABLE_ROW("for( i = 0; i < num_tile_rows_minus1; i++ ) ");
            for(int i = 0; i < num_tile_rows_minus1; i++ ) {
                ENABLE_CELL(cccitem);
                int row_height_minus1 = APPEND_GOLOMB_SE_ROW_NO_VAR(fmt::format("row_height_minus1[{}]",i));
            }
        }
        int loop_filter_across_tiles_enabled_flag = APPEND_BITS_ROW("loop_filter_across_tiles_enabled_flag", 1);
    }
    int pps_loop_filter_across_slices_enabled_flag = APPEND_BITS_ROW("pps_loop_filter_across_slices_enabled_flag", 1);
    int deblocking_filter_control_present_flag = APPEND_BITS_ROW("deblocking_filter_control_present_flag", 1);
    citem = APPEND_ENABLE_ROW("if( deblocking_filter_control_present_flag )");
    if( deblocking_filter_control_present_flag ) {
        ENABLE_CELL(citem);
        int deblocking_filter_override_enabled_flag = APPEND_BITS_ROW("deblocking_filter_override_enabled_flag", 1);
        int pps_deblocking_filter_disabled_flag = APPEND_BITS_ROW("pps_deblocking_filter_disabled_flag", 1);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( !pps_deblocking_filter_disabled_flag ) ");
        if( !pps_deblocking_filter_disabled_flag )  {
            ENABLE_CELL(ccitem);
            APPEND_GOLOMB_SE_ROW(pps_beta_offset_div2);
            APPEND_GOLOMB_SE_ROW(pps_tc_offset_div2);
        }
    }
    int pps_scaling_list_data_present_flag = APPEND_BITS_ROW("pps_scaling_list_data_present_flag", 1);
    citem = APPEND_ENABLE_ROW("if( pps_scaling_list_data_present_flag )");
    if( pps_scaling_list_data_present_flag ) {
        ENABLE_CELL(citem);
        hevc_scaling_list_data(citem);
    }
    int lists_modification_present_flag = APPEND_BITS_ROW("lists_modification_present_flag", 1);
    
    APPEND_GOLOMB_SE_ROW(log2_parallel_merge_level_minus2);
    
    int slice_segment_header_extension_present_flag = APPEND_BITS_ROW("slice_segment_header_extension_present_flag", 1);
    
    int pps_extension_present_flag = APPEND_BITS_ROW("pps_extension_present_flag", 1);
    
    citem = APPEND_ENABLE_ROW("if( pps_extension_present_flag ) ");
    int pps_range_extension_flag  = 0;
    int pps_multilayer_extension_flag = 0;
    int pps_3d_extension_flag = 0;
    int pps_extension_5bits = 0;
    if( pps_extension_present_flag )  {
        ENABLE_CELL(citem);
        pps_range_extension_flag = APPEND_BITS_ROW("pps_range_extension_flag", 1);
        pps_multilayer_extension_flag = APPEND_BITS_ROW("pps_multilayer_extension_flag", 1);
        pps_3d_extension_flag = APPEND_BITS_ROW("pps_3d_extension_flag", 1);
        pps_extension_5bits = APPEND_BITS_ROW("pps_extension_5bits", 5);
    }
    citem = APPEND_ENABLE_ROW("if( pps_range_extension_flag ) ");
    if( pps_range_extension_flag ) {
        ENABLE_CELL(citem);
        int crow = 0;
        hevc_pps_range_extension(citem,transform_skip_enabled_flag);
    }
    citem = APPEND_ENABLE_ROW("if( pps_multilayer_extension_flag ) ");
    if( pps_multilayer_extension_flag ) {
        ENABLE_CELL(citem);
        hevc_pps_multilayer_extension(citem);
    }
    
    citem = APPEND_ENABLE_ROW("if( pps_3d_extension_flag ) ");
    if( pps_3d_extension_flag ){
        ENABLE_CELL(citem);
        hevc_pps_3d_extension(citem);
    }
    citem = APPEND_ENABLE_ROW("if( pps_extension_5bits ) ");
    if( pps_extension_5bits ) {
        ENABLE_CELL(citem);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("while( more_rbsp_data( ) )");
        int index = 0;
        while( more_rbsp_data()) {
            int pps_extension_data_flag = APPEND_BITS_ROW(fmt::format("pps_extension_data_flag[{}]",index), 1);
            index ++;
        }
    }
    rbsp_trailing_bits(pitem);
    return ret;
}

mdp_video_header * MALCodecParser::parse_hvcc() {
    std::shared_ptr<IDataSource> datasource = datasource_;
    mdp_video_header *header = (mdp_video_header *)av_mallocz(sizeof(mdp_video_header));
    mdp_header_item *pitem = new_root_item();//(mdp_header_item *)av_mallocz(sizeof(mdp_header_item));
    header->root_item = pitem;
    std::vector<std::string> fileds = {"configurationVersion",
        "general_profile_space",
        "general_tier_flag",
        "general_profile_idc",
        "general_profile_compatibility_flags",
        "general_constraint_indicator_flags",
        "general_level_idc",
        "reserved",
        "min_spatial_segmentation_idc",
        "reserved",
        "parallelismType",
        "reserved",
        "chroma_format_idc",
        "reserved",
        "bit_depth_luma_minus8",
        "reserved",
        "bit_depth_chroma_minus8",
        "avgFrameRate",
        "constantFrameRate",
        "numTemporalLayers",
        "temporalIdNested",
        "lengthSizeMinusOne",
        "numOfArrays",
    };
    std::vector<int> bits = {8,2,1,5,32,48,8,4,12,6,2,6,2,5,3,5,3,16,2,3,1,2,8};
    int64_t numOfArrays = 0;
    int64_t lengthSizeMinusOne = 0;
    for (int i = 0; i < fileds.size(); i++) {
        std::string& name = fileds.at(i);
        int bit = bits.at(i);
        int64_t val = APPEND_BITS_ROW_NO_RBSP(name,bit);
        if (name == "lengthSizeMinusOne") {
            lengthSizeMinusOne = val;
        } else if (name == "numOfArrays") {
            numOfArrays = val;
        }
    }
    int vps_index = 0;
    int sps_index = 0;
    int pps_index = 0;
    for (int j = 0; j < numOfArrays; j++) {
        int64_t numNalus = 0;
        APPEND_BITS_ROW_NO_RBSP("array_completeness",1);
        APPEND_BITS_ROW_NO_RBSP("reserved",1);
        int64_t NAL_unit_type = APPEND_BITS_ROW_NO_RBSP("NAL_unit_type",6);
        numNalus = APPEND_BITS_ROW_NO_RBSP("numNalus",16);
        for(int i = 0; i< numNalus; i++) {
            int64_t nalUnitLength = APPEND_BITS_ROW_NO_RBSP("nalUnitLength",16);
            int64_t next = datasource->currentBytesPosition() + nalUnitLength;
            if (NAL_unit_type == 32) { //vps
                std::string name = fmt::format("vps[{}]", vps_index++);
                mdp_header_item *item = APPEND_ENABLE_ROW(name.c_str());
                enable_or_disable_cell(item,1);
                int64_t next = datasource_->currentBytesPosition() + nalUnitLength;
                parse_hvc_nal_header(item);
                parse_hvc_vps(header, item);
                datasource->seekBytes(next,SEEK_SET);
                
            } else if (NAL_unit_type == 33) { //sps
                std::string name = fmt::format("sps[{}]", sps_index++);
                mdp_header_item *item = APPEND_ENABLE_ROW(name.c_str());
                enable_or_disable_cell(item,1);
                parse_hvc_nal_header(item);
                parse_hvc_sps(header, item);
            } else if (NAL_unit_type == 34) { //pps
                std::string name = fmt::format("pps[{}]", pps_index++);
                mdp_header_item *item = APPEND_ENABLE_ROW(name.c_str());
                enable_or_disable_cell(item,1);
                parse_hvc_nal_header(item);
                parse_hvc_pps(header, item);
            }
            datasource->seekBytes(next, SEEK_SET);
        }
    }
    return header;
}
/*
 对于H.264的码流，需要计算POC包括三种：coded frame（编码帧）, coded field（编码场）和complementary field pair（互补参考场对），每种类型的POC都由 TopFieldOrderCnt 和BottomFieldOrderCnt 这两个值的一个或两个组成：
 
 每一个编码帧的poc包含两个值TopFieldOrderCnt和BottomFieldOrderCnt；
 每一个编码场的poc只包含一个值，如果该field为顶场则为TopFieldOrderCnt，如果是底场为BottomFieldOrderCnt；
 对于每一个互补参考场对，POC包含两个值，顶场为TopFieldOrderCnt，底场为BottomFieldOrderCnt；
 ————————————————
 
 
 //判断是底场还是顶场
  解析NAL单元： 首先，需要解析NAL单元，以获取帧头信息。这通常涉及读取H.264比特流中的RBSP（Raw Byte Sequence Payload）。

  访问Slice Header： 在NAL单元中，访问Slice Header以获取帧类型信息。Slice Header中包含关于当前Slice的信息，包括编码类型和场信息。

  检查field_pic_flag： 在Slice Header中，有一个field_pic_flag，如果它为1，表示这是一个场编码的图像（而不是帧编码）。

  判断bottom_field_flag： 如果field_pic_flag为1，则需要进一步检查bottom_field_flag：

  如果bottom_field_flag为0，表示这是一个顶场（Top Field）。
  如果bottom_field_flag为1，表示这是一个底场（Bottom Field）。

 
 原文链接：https://blog.csdn.net/zhoutaopower/article/details/127342722
 */
int MALCodecParser::avc_packet_poc(MALAVCSliceHeader &sliceHeader, std::shared_ptr<MALPacket> pkt, std::shared_ptr<MALAVCSPS> sps ,std::shared_ptr<MALAVCPPS> pps) {
    pkt->avc_PicOrderCntLsb = sliceHeader.pic_order_cnt_lsb;
    if (sps->pic_order_cnt_type == 0) {
        int prevPicOrderCntMsb = 0;
        int prevPicOrderCntLsb = 0;
        if (pkt->avc_IdrPicFlag) {
            prevPicOrderCntMsb = prevPicOrderCntLsb = 0;
        } else {
            int prePktBottomField = 0;
            if (prePkt_->sliceHeader->field_pic_flag == 1 && prePkt_->sliceHeader->bottom_field_flag == 1) {
                prePktBottomField == 1;
            }
            if (prePkt_->sliceHeader->memory_management_control_operation == 5 && !prePktBottomField) {
                prevPicOrderCntMsb=0;
                prevPicOrderCntLsb = prePkt_->avc_TopFieldOrderCnt;
            } else if (prePkt_->sliceHeader->memory_management_control_operation == 5 && prePktBottomField) {
                prevPicOrderCntMsb = prevPicOrderCntLsb = 0;
            } else if (prePkt_->sliceHeader->memory_management_control_operation != 5) {
                prevPicOrderCntMsb = prePkt_->avc_PicOrderCntMsb;
                prevPicOrderCntLsb = prePkt_->avc_PicOrderCntLsb;
            }
        }
        int pic_order_cnt_lsb = sliceHeader.pic_order_cnt_lsb;
        int MaxPicOrderCntLsb = (1 << (sps->log2_max_pic_order_cnt_lsb_minus4+4));
        int PicOrderCntMsb = 0;
        if( (pic_order_cnt_lsb < prevPicOrderCntLsb) && ((prevPicOrderCntLsb - pic_order_cnt_lsb) >= (MaxPicOrderCntLsb / 2)))
            PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
        else if((pic_order_cnt_lsb > prevPicOrderCntLsb) &&
                ((pic_order_cnt_lsb - prevPicOrderCntLsb) > (MaxPicOrderCntLsb / 2)))
            PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
        else
            PicOrderCntMsb = prevPicOrderCntMsb;
        
        int pktBottomField = 0;
        if (pkt->sliceHeader->field_pic_flag == 1 && pkt->sliceHeader->bottom_field_flag == 1) {
            pktBottomField = 1;
        }

        int TopFieldOrderCnt = 0;
        
        if (!pktBottomField) { //非底场： 包括顶场或者帧
            pkt->avc_TopFieldOrderCnt = TopFieldOrderCnt = PicOrderCntMsb + pic_order_cnt_lsb;
        }
        
        //非顶场
        if (!pkt->sliceHeader->field_pic_flag) { //帧
            pkt->avc_BottomFieldOrderCnt = TopFieldOrderCnt + sliceHeader.delta_pic_order_cnt_bottom;
        } else if (pktBottomField){ //底场
            pkt->avc_BottomFieldOrderCnt = PicOrderCntMsb + pic_order_cnt_lsb; //前场和顶场
        }
        pkt->avc_PicOrderCntMsb = PicOrderCntMsb;
        pkt->avc_PicOrderCntLsb = pic_order_cnt_lsb;
        
    } else if (sps->pic_order_cnt_type == 1) {
        int FrameNumOffset = 0;
        if (curPkt_->avc_IdrPicFlag) {
            FrameNumOffset = 0;
        } else {
            if (prePkt_->sliceHeader->frame_num > curPkt_->sliceHeader->frame_num) {
                FrameNumOffset = prePkt_->avc_FrameNumOffset + (1<<(sps->log2_max_frame_num_minus4+4));
            } else {
                FrameNumOffset = prePkt_->avc_FrameNumOffset;
            }
        }
        curPkt_->avc_FrameNumOffset = FrameNumOffset;
        int absFrameNum = 0;
        if (sps->num_ref_frames_in_pic_order_cnt_cycle != 0) {
            absFrameNum = FrameNumOffset + curPkt_->sliceHeader->frame_num;
        } else
            absFrameNum = 0;
        if (currentAVCNal_->nal_ref_idc == 0 && absFrameNum > 0) {
            absFrameNum --;
        }
        int picOrderCntCycleCnt = 0;
        int frameNumInPicOrderCntCycle = 0;
        if (absFrameNum > 0) {
            picOrderCntCycleCnt = ( absFrameNum - 1 ) / sps->num_ref_frames_in_pic_order_cnt_cycle;
            frameNumInPicOrderCntCycle = ( absFrameNum - 1 ) % sps->num_ref_frames_in_pic_order_cnt_cycle;
        }
        int expectedPicOrderCnt = 0;
        if( absFrameNum > 0 ){
            int ExpectedDeltaPerPicOrderCntCycle = 0;
            for( int i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++ )
                ExpectedDeltaPerPicOrderCntCycle += sps->offset_for_ref_frame[i];
            expectedPicOrderCnt = picOrderCntCycleCnt * ExpectedDeltaPerPicOrderCntCycle;
            for( int i = 0; i <= frameNumInPicOrderCntCycle; i++ )
                expectedPicOrderCnt = expectedPicOrderCnt + sps->offset_for_ref_frame[i];
        } else
            expectedPicOrderCnt = 0;
        
        if(currentAVCNal_->nal_ref_idc == 0 )
            expectedPicOrderCnt = expectedPicOrderCnt + sps->offset_for_non_ref_pic;
        int TopFieldOrderCnt = 0;
        int BottomFieldOrderCnt = 0;
        if (!sliceHeader.field_pic_flag) {
            TopFieldOrderCnt = expectedPicOrderCnt + sliceHeader.delta_pic_order_cnt[ 0 ];
            BottomFieldOrderCnt = TopFieldOrderCnt +
            sps->offset_for_top_to_bottom_field + sliceHeader.delta_pic_order_cnt[ 1 ];
        } else if (!sliceHeader.bottom_field_flag) {
            TopFieldOrderCnt = expectedPicOrderCnt + sliceHeader.delta_pic_order_cnt[0];
        } else {
            BottomFieldOrderCnt = expectedPicOrderCnt + sps->offset_for_top_to_bottom_field + sliceHeader.delta_pic_order_cnt[0];
        }
        pkt->avc_TopFieldOrderCnt = TopFieldOrderCnt;
        pkt->avc_BottomFieldOrderCnt = BottomFieldOrderCnt;
        
    } else if (sps->pic_order_cnt_type == 2) { //不能出现B帧，解码和显示一样
        int FrameNumOffset = 0;
        if (curPkt_->avc_IdrPicFlag) {
            FrameNumOffset = 0;
        } else {
            if (prePkt_->sliceHeader->frame_num > curPkt_->sliceHeader->frame_num) {
                FrameNumOffset = prePkt_->avc_FrameNumOffset + (1<<(sps->log2_max_frame_num_minus4+4));
            } else {
                FrameNumOffset = prePkt_->avc_FrameNumOffset;
            }
        }
        curPkt_->avc_FrameNumOffset = FrameNumOffset;
        
        int tempPicOrderCnt = 0;
        if (curPkt_->avc_IdrPicFlag) {
            tempPicOrderCnt = 0;
        } else if (currentAVCNal_->nal_ref_idc == 0) {
            tempPicOrderCnt = 2 * (FrameNumOffset + sliceHeader.frame_num ) - 1;
        } else {
            tempPicOrderCnt = 2 * (FrameNumOffset + sliceHeader.frame_num);
        }
        int TopFieldOrderCnt = 0;
        int BottomFieldOrderCnt = 0;
//        if (!sliceHeader.field_pic_flag) {
//            TopFieldOrderCnt = tempPicOrderCnt;
//            BottomFieldOrderCnt = tempPicOrderCnt;
//        } else if ()
        TopFieldOrderCnt = BottomFieldOrderCnt = tempPicOrderCnt;
        pkt->avc_TopFieldOrderCnt = pkt->avc_BottomFieldOrderCnt = tempPicOrderCnt;
    }
    int picture_structure = 0;
    if (!sliceHeader.field_pic_flag){ //帧
        picture_structure = 3; //frame
    } else {
        if (sliceHeader.bottom_field_flag) {
            picture_structure = 2; //bottom field;
        } else {
            picture_structure = 1; // top field;
        }
    }
    int tmp0 = 0;
    int tmp1 = 0;
    if (picture_structure != 2) {
        tmp0 = pkt->avc_TopFieldOrderCnt;
    }
    if (picture_structure != 1) {
        tmp1 = pkt->avc_BottomFieldOrderCnt;
    }
    pkt->poc = std::min(tmp0, tmp1);
    return 0;
}
std::vector<MALSEIMessage> MALCodecParser::parse_hevc_sei(mdp_video_header *header, mdp_header_item *item, int64_t size) {
    return parse_avc_sei(header, item, size);
}
static int has_start_code( uint8_t *p,  int64_t len) {
    int ret = 0;
    uint8_t *op_p = p;
    uint8_t *end = p + len;
    while ((end - op_p) > 3) {
        if (op_p[0] != 0) {op_p++; continue;}
        if (op_p[1] != 0) {op_p++; continue;}
        if (op_p[2] != 0 && op_p[2] != 1) {op_p++; continue;}
        if (op_p[2] == 1) {ret = 1;break;} //0x000001
        if (op_p[3] != 1 && op_p[3] != 0) {op_p++; continue;}
        if (op_p[3] == 1) {ret = 1;break;} //0x00000001
    }
    return ret;
}
int MALCodecParser::parse_packet(std::shared_ptr<MALPacket> pkt, bool annextb) {
    if (!pkt) return 0;
    curPkt_ = pkt;
    datasource_ = pkt->datasource->shallowCopy();
    auto stream = malFmtCtx_->streams[pkt->index];
    if (stream->codecType == MALCodecType::h264) {
        if (!annextb) {
            if (stream->videoConfig.size() < (pkt->sample_description_index-1)) {
                malFmtCtx_->shallowCheck.errors.push_back(fmt::format("没有找到第{}个pkt对应avcc",pkt->number));
                return -1;
            }
            std::shared_ptr<MALAVCC> avcc = std::dynamic_pointer_cast<MALAVCC>(stream->videoConfig[pkt->sample_description_index-1]);
            avcc_ = avcc;
            mdp_video_header *header = (mdp_video_header *)av_mallocz(sizeof(mdp_video_header));
            mdp_header_item *pitem = new_root_item();//(mdp_header_item *)av_mallocz(sizeof(mdp_header_item));
            header->root_item = pitem;
            while (!datasource_->isEof()) {
                uint8_t * start = (uint8_t *)datasource_->ptr();
                int is_multi_slice = has_start_code(start+3, datasource_->totalSize()); //第一个startcode不检测
                if (is_multi_slice) {
                    malFmtCtx_->shallowCheck.warning.push_back(fmt::format("【multi slice】: 第{}个packet中一个nal包含多个slice，在mac上可能不兼容",pkt->number));
                }
                mdp_header_item *citem = APPEND_ENABLE_ROW("nal");
                ENABLE_CELL(citem);
                int64_t nalLen = datasource_->readBytesInt64(avcc->lengthSizeMinusOne+1);
                int64_t currentPos = datasource_->currentBytesPosition();
                int64_t next = currentPos + nalLen;
                parse_avc_nal_header(pitem);
                if (currentAVCNal_) {
                    pkt->nals.push_back(currentAVCNal_);
                }
                pkt->avc_IdrPicFlag = currentAVCNal_-> nal_unit_type == 5 ? 1 : 0;
                if (currentAVCNal_->nal_unit_type == 7) {
                    mdp_header_item *item = APPEND_ENABLE_ROW("sps");
                    parse_avc_sps(header, item);
                } else if (currentAVCNal_->nal_unit_type == 8) {
                    mdp_header_item *item = APPEND_ENABLE_ROW("pps");
                    parse_avc_pps(header, item);
                } else if (currentAVCNal_->nal_unit_type == 6) {
                    mdp_header_item *item = APPEND_ENABLE_ROW("sei");
                    std::vector<MALSEIMessage> sei = parse_avc_sei(header, item,nalLen-1);//这里-1是因为nal的header 1个字节(parse_avc_nal_header)
                    int x = 0;
                } else {
                    mdp_header_item *item = APPEND_ENABLE_ROW("vcl");
                    parse_avc_slice(header,item, pkt);
                }
                pkt->nal_header  = header;
                datasource_->seekBytes(next,SEEK_SET);
            }
            
        }
    } else if (stream->codecType == MALCodecType::h265) {
        if (!annextb) {
            if (stream->videoConfig.size() < (pkt->sample_description_index-1)) {
                malFmtCtx_->shallowCheck.errors.push_back(fmt::format("没有找到第{}个pkt对应avcc",pkt->number));
                return -1;
            }
            std::shared_ptr<MALHVCC> hvcc = std::dynamic_pointer_cast<MALHVCC>( stream->videoConfig[pkt->sample_description_index-1]);
            hvcc_ = hvcc;
            mdp_video_header *header = (mdp_video_header *)av_mallocz(sizeof(mdp_video_header));
            mdp_header_item *pitem = new_root_item();//(mdp_header_item *)av_mallocz(sizeof(mdp_header_item));
            header->root_item = pitem;
            while (!datasource_->isEof()) {
                mdp_header_item *citem = APPEND_ENABLE_ROW("nal");
                ENABLE_CELL(citem);
                int64_t nalLen = datasource_->readBytesInt64(hvcc->lengthSizeMinusOne+1);
                int64_t currentPos = datasource_->currentBytesPosition();
                int64_t next = currentPos + nalLen;
                parse_hvc_nal_header(pitem);
                if (currentHEVCNal_) {
                    pkt->nals.push_back(currentHEVCNal_);
                }
                if (currentHEVCNal_->nal_unit_type == 32) { //vps
                    mdp_header_item *item = APPEND_ENABLE_ROW("vps");
                    parse_hvc_vps(header, item);
                } else if (currentHEVCNal_->nal_unit_type == 33) {
                    mdp_header_item *item = APPEND_ENABLE_ROW("sps");
                    parse_hvc_sps(header, item);
                } else if (currentHEVCNal_->nal_unit_type == 34) {
                    mdp_header_item *item = APPEND_ENABLE_ROW("pps");
                    parse_hvc_pps(header, item);
                } else if (currentHEVCNal_->nal_unit_type == 39 || currentHEVCNal_->nal_unit_type == 40) {
                    mdp_header_item *item = APPEND_ENABLE_ROW("sei");
                    parse_hevc_sei(header, item,nalLen - 2);
                } else if ((currentHEVCNal_->nal_unit_type >= 0 && currentHEVCNal_->nal_unit_type <= 9) || (currentHEVCNal_->nal_unit_type >= 16 && currentHEVCNal_->nal_unit_type <= 21) ) {
                    mdp_header_item *item = APPEND_ENABLE_ROW("vcl");
                    parse_hevc_slice(header, item, pkt);
                } else  {
                    mdp_header_item *item = APPEND_ENABLE_ROW("vlc");
                    
//                    parse_avc_slice(header,item, pkt);
                }
                pkt->nal_header  = header;
                datasource_->seekBytes(next,SEEK_SET);
            }
            
        }
    }
    prePkt_ = pkt;
    return 0;
}
int MALCodecParser::avc_ref_pic_list_modification(MALAVCSliceHeader &sliceHeader, mdp_video_header *header, mdp_header_item *pitem) {
    {
        pitem = APPEND_ENABLE_ROW("if( slice_type % 5 != 2 && slice_type % 5 != 4 ) ");
        if (sliceHeader.slice_type % 5 != 2 && sliceHeader.slice_type % 5 != 4) {
            enable_or_disable_cell(pitem, 1);
            APPEND_BITS_ROW_VAR(ref_pic_list_modification_flag_l0,1);
            {
                pitem = APPEND_ENABLE_ROW("if( ref_pic_list_modification_flag_l0 )");
                if (ref_pic_list_modification_flag_l0) {
                    enable_or_disable_cell(pitem, 1);
                    int modification_of_pic_nums_idc = 0;
                    do {
                        modification_of_pic_nums_idc = APPEND_GOLOMB_UE_ROW_NO_VAR(modification_of_pic_nums_idc);
                        if( modification_of_pic_nums_idc == 0 ||
                           modification_of_pic_nums_idc == 1 ) {
                            APPEND_GOLOMB_UE_ROW(abs_diff_pic_num_minus1);
                        } else if( modification_of_pic_nums_idc == 2 ) {
                            APPEND_GOLOMB_UE_ROW(long_term_pic_num);
                        }
                    } while(modification_of_pic_nums_idc != 3);
                }
            }
        }
    }
    {
        pitem = APPEND_ENABLE_ROW("if( slice_type % 5 = = 1 )");
        if (sliceHeader.slice_type % 5 == 1) {
            enable_or_disable_cell(pitem, 1);
            APPEND_BITS_ROW_VAR(ref_pic_list_modification_flag_l1, 1);
            {
                pitem = APPEND_ENABLE_ROW("if( ref_pic_list_modification_flag_l1 )");
                if (ref_pic_list_modification_flag_l1) {
                    enable_or_disable_cell(pitem, 1);
                    int modification_of_pic_nums_idc = 0;
                    do {
                        modification_of_pic_nums_idc = APPEND_GOLOMB_UE_ROW_NO_VAR(modification_of_pic_nums_idc);
                        if( modification_of_pic_nums_idc == 0 || modification_of_pic_nums_idc == 1) {
                            APPEND_GOLOMB_UE_ROW(abs_diff_pic_num_minus1);
                        } else if( modification_of_pic_nums_idc == 2 ) {
                            APPEND_GOLOMB_UE_ROW(long_term_pic_num);
                        }
                    } while(modification_of_pic_nums_idc != 3 );
                }
            }
        }
        
    }
    
    return 0;
}
int MALCodecParser::avc_dec_ref_pic_marking(MALAVCSliceHeader &sliceHeader, mdp_video_header *header, mdp_header_item *pitem,std::shared_ptr<MALAVCSPS> sps ,std::shared_ptr<MALAVCPPS> pps) {
    {
        pitem = APPEND_ENABLE_ROW("if( IdrPicFlag )");
        int IdrPicFlag = currentAVCNal_-> nal_unit_type == 5 ? 1 : 0;
        if (IdrPicFlag) {
            enable_or_disable_cell(pitem, 1);
            APPEND_BITS_ROW_VAR(no_output_of_prior_pics_flag, 1);
            APPEND_BITS_ROW_VAR(long_term_reference_flag, 1);
        } else {
            pitem = APPEND_ENABLE_ROW("else");
            enable_or_disable_cell(pitem, 1);
            APPEND_BITS_ROW_VAR(adaptive_ref_pic_marking_mode_flag, 1);
            {
                pitem = APPEND_ENABLE_ROW("if( adaptive_ref_pic_marking_mode_flag )");
                if( adaptive_ref_pic_marking_mode_flag ) {
                    enable_or_disable_cell(pitem, 1);
                    int memory_management_control_operation = 0;
                    do {
                        APPEND_GOLOMB_UE_ROW(memory_management_control_operation);
                        if( memory_management_control_operation == 1 ||
                           memory_management_control_operation == 3 ) {
                            APPEND_GOLOMB_UE_ROW(difference_of_pic_nums_minus1);
                        }
                        if(memory_management_control_operation == 2 ) {
                            APPEND_GOLOMB_UE_ROW(long_term_pic_num);
                        }
                        if( memory_management_control_operation == 3 ||
                           memory_management_control_operation == 6 ) {
                            APPEND_GOLOMB_UE_ROW(long_term_frame_idx);
                        }
                        if( memory_management_control_operation == 4 ) {
                            APPEND_GOLOMB_UE_ROW(max_long_term_frame_idx_plus1);
                        }
                    }while(memory_management_control_operation != 0);
                    sliceHeader.memory_management_control_operation = memory_management_control_operation;
                }
            }
        }
    }
    return 0;
}
int MALCodecParser::avc_pred_weight_table(MALAVCSliceHeader &sliceHeader, mdp_video_header *header, mdp_header_item *pitem,std::shared_ptr<MALAVCSPS> sps ,std::shared_ptr<MALAVCPPS> pps) {
    APPEND_GOLOMB_UE_ROW(luma_log2_weight_denom);
    int ChromaArrayType = sps->separate_colour_plane_flag == 0 ? sps->chroma_format_idc : 0;
    {
        pitem = APPEND_ENABLE_ROW("if( ChromaArrayType != 0 )");
        if( ChromaArrayType != 0 ) {
            enable_or_disable_cell(pitem, 1);
            APPEND_GOLOMB_UE_ROW(chroma_log2_weight_denom);
        }
    }
    {
        pitem = APPEND_ENABLE_ROW("for( i = 0; i <= num_ref_idx_l0_active_minus1; i++ )");
        for (int i = 0; i <= sliceHeader.num_ref_idx_l0_active_minus1;i++){
            enable_or_disable_cell(pitem, 1);
            APPEND_BITS_ROW_VAR(luma_weight_l0_flag, 1);
            if( luma_weight_l0_flag) {
                APPEND_GOLOMB_SE_ROW_NO_VAR( fmt::format("luma_weight_l0[{}]",i));
                APPEND_GOLOMB_SE_ROW_NO_VAR( fmt::format("luma_offset_l0[{}]",i));
            }
            if( ChromaArrayType != 0 ) {
                APPEND_BITS_ROW_VAR(chroma_weight_l0_flag, 1);
                if (chroma_weight_l0_flag) {
                    for (int j = 0; j < 2; j++) {
                        APPEND_GOLOMB_SE_ROW_NO_VAR( fmt::format("chroma_weight_l0[{}][{}]",i,j));
                        APPEND_GOLOMB_SE_ROW_NO_VAR( fmt::format("chroma_offset_l0[{}][{}]",i,j));
                    }
                }
            }
        }
    }
    {
        pitem = APPEND_ENABLE_ROW("if( slice_type % 5 = = 1 )");
        if (sliceHeader.slice_type % 5 == 1) {
            enable_or_disable_cell(pitem, 1);
            pitem = APPEND_ENABLE_ROW("for( i = 0; i <= num_ref_idx_l1_active_minus1; i++ )");
            for (int i = 0; i <= sliceHeader.num_ref_idx_l1_active_minus1;i++){
                enable_or_disable_cell(pitem, 1);
                APPEND_BITS_ROW_VAR(luma_weight_l1_flag, 1);
                if( luma_weight_l1_flag) {
                    APPEND_GOLOMB_SE_ROW_NO_VAR( fmt::format("luma_weight_l1[{}]",i));
                    APPEND_GOLOMB_SE_ROW_NO_VAR( fmt::format("luma_offset_l1[{}]",i));
                }
                if( ChromaArrayType != 0 ) {
                    APPEND_BITS_ROW_VAR(chroma_weight_l1_flag, 1);
                    if (chroma_weight_l1_flag) {
                        for (int j = 0; j < 2; j++) {
                            APPEND_GOLOMB_SE_ROW_NO_VAR( fmt::format("chroma_weight_l1[{}][{}]",i,j));
                            APPEND_GOLOMB_SE_ROW_NO_VAR( fmt::format("chroma_offset_l1[{}][{}]",i,j));
                        }
                    }
                }
            }
        }
        
    }
    
    return 0;
}

int MALCodecParser::parse_avc_slice(mdp_video_header *header, mdp_header_item *pitem,std::shared_ptr<MALPacket> pkt) {
    int ret = 0;
    pitem = APPEND_ENABLE_ROW("slice_header");
    enable_or_disable_cell(pitem, 1);
    if (currentAVCNal_-> nal_unit_type == 1 || currentAVCNal_-> nal_unit_type == 5) {
        auto nal = std::dynamic_pointer_cast<MALAVCSliceWithOutPartitioning>(currentAVCNal_);
        auto& sliceHeader = nal->header;
        APPEND_GOLOMB_UE_ROW(first_mb_in_slice);
        nal->header.first_mb_in_slice = first_mb_in_slice;
        if (first_mb_in_slice) {
            int x = 0;
        }
        APPEND_GOLOMB_UE_ROW(slice_type);
        nal->header.slice_type = slice_type;
        APPEND_GOLOMB_UE_ROW(pic_parameter_set_id);
        nal->header.pic_parameter_set_id = pic_parameter_set_id;
        std::shared_ptr<MALAVCPPS> pps= nullptr;
        std::shared_ptr<MALAVCSPS> sps= nullptr;
        if (avcc_) {
            for (auto el : avcc_->ppsList) {
                if (el->pic_parameter_set_id == pic_parameter_set_id) {
                    pps = el;
                    break;
                }
            }
            if (pps) {
                for (auto el : avcc_->spsList) {
                    if (el->seq_parameter_set_id == pps->seq_parameter_set_id) {
                        sps = el;
                        break;
                    }
                }
            } else {
                malFmtCtx_->shallowCheck.errors.push_back("slice not find pps!");
                goto end;
            }
            if (!sps) {
                malFmtCtx_->shallowCheck.errors.push_back("slice not find sps!");
                goto end;
            }
        } else {
            malFmtCtx_->shallowCheck.errors.push_back("slice not find avcc!");
            goto end;
        }
        mdp_header_item *citem = APPEND_ENABLE_ROW("if( separate_colour_plane_flag = = 1 )");
        if (sps->separate_colour_plane_flag) {
            enable_or_disable_cell(citem, 1);
            pitem = citem;
            APPEND_BITS_ROW_VAR(colour_plane_id, 2);
            sliceHeader.colour_plane_id = colour_plane_id;
        }
        APPEND_BITS_ROW_VAR(frame_num,sps->log2_max_frame_num_minus4+4);
        sliceHeader.frame_num = frame_num;
        citem = APPEND_ENABLE_ROW("if( !frame_mbs_only_flag )");
        if (!sps->frame_mbs_only_flag) {
            enable_or_disable_cell(citem, 1);
            pitem = citem;
            APPEND_BITS_ROW_VAR(field_pic_flag,1);
            sliceHeader.field_pic_flag = field_pic_flag;
            mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( field_pic_flag )");
            if (field_pic_flag) {
                enable_or_disable_cell(ccitem, 1);
                pitem = ccitem;
                APPEND_BITS_ROW_VAR(bottom_field_flag,1);
                sliceHeader.bottom_field_flag = bottom_field_flag;
            }
        }
        int IdrPicFlag = currentAVCNal_-> nal_unit_type == 5 ? 1 : 0;
        citem = APPEND_ENABLE_ROW("if( IdrPicFlag )");
        if (IdrPicFlag) {
            enable_or_disable_cell(citem, 1);
            pitem = citem;
            APPEND_GOLOMB_UE_ROW(idr_pic_id);
            sliceHeader.idr_pic_id = idr_pic_id;
            if (pkt->flag != MALPacketFlag::idr) { //说明和stss box中的不一致
                malFmtCtx_->shallowCheck.warning.push_back(fmt::format("第{}个packet，slice中是idr，但是stss中不是idr，seek需要花更多时间",pkt->number));
            }
        } else {
            if (pkt->flag == MALPacketFlag::idr) { //说明slice中不是idr，但是在
                malFmtCtx_->shallowCheck.warning.push_back(fmt::format("第{}个packet，slice中标识不是idr，但是stss中是idr，seek时会花屏",pkt->number));
            }
        }
        citem = APPEND_ENABLE_ROW("if( pic_order_cnt_type = = 0 ) ");
        if (sps->pic_order_cnt_type == 0) {
            enable_or_disable_cell(citem, 1);
            pitem = citem;
            int pic_order_cnt_lsb_bits = sps->log2_max_pic_order_cnt_lsb_minus4 + 4;
            APPEND_BITS_ROW_VAR(pic_order_cnt_lsb, pic_order_cnt_lsb_bits);
            sliceHeader.pic_order_cnt_lsb = pic_order_cnt_lsb;
            mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( bottom_field_pic_order_in_frame_present_flag && !field_pic_flag )");
            if (pps->bottom_field_pic_order_in_frame_present_flag && !sliceHeader.field_pic_flag) {
                enable_or_disable_cell(ccitem, 1);
                pitem = ccitem;
                APPEND_GOLOMB_SE_ROW(delta_pic_order_cnt_bottom);
            }
        }
        citem = APPEND_ENABLE_ROW("if( pic_order_cnt_type = = 1 && !delta_pic_order_always_zero_flag )");
        if (sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag) {
            enable_or_disable_cell(citem, 1);
            pitem = citem;
            int delta_pic_order_cnt = APPEND_GOLOMB_SE_ROW_NO_VAR("delta_pic_order_cnt[ 0 ]");
            sliceHeader.delta_pic_order_cnt[0] = delta_pic_order_cnt;
            mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( bottom_field_pic_order_in_frame_present_flag && !field_pic_flag )");
            if (pps->bottom_field_pic_order_in_frame_present_flag && !sliceHeader.field_pic_flag) {
                enable_or_disable_cell(ccitem, 1);
                pitem = ccitem;
                delta_pic_order_cnt = APPEND_GOLOMB_SE_ROW_NO_VAR("delta_pic_order_cnt[ 1 ]");
                sliceHeader.delta_pic_order_cnt[1] = delta_pic_order_cnt;
            }
        }
        citem = APPEND_ENABLE_ROW("if( redundant_pic_cnt_present_flag )");
        if (pps->redundant_pic_cnt_present_flag) {
            enable_or_disable_cell(citem, 1);
            pitem = citem;
            APPEND_GOLOMB_UE_ROW(redundant_pic_cnt);
        }
        citem = APPEND_ENABLE_ROW("if( slice_type = = B )");
        if (slice_type % 5 == 1) {
            enable_or_disable_cell(citem, 1);
            pitem = citem;
            APPEND_BITS_ROW_VAR(direct_spatial_mv_pred_flag,1);
        }
        citem = APPEND_ENABLE_ROW("if( slice_type = = P | | slice_type = = SP | | slice_type = = B )");
        // 0:P 1:B 2:I 3:SP 4:SI
        if (slice_type % 5 == 0 || slice_type % 5 == 3 || slice_type % 5 == 1) {
            enable_or_disable_cell(citem, 1);
            pitem = citem;
            APPEND_BITS_ROW_VAR(num_ref_idx_active_override_flag,1);
            sliceHeader.num_ref_idx_active_override_flag = num_ref_idx_active_override_flag;
            mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( num_ref_idx_active_override_flag )");
            if (num_ref_idx_active_override_flag) {
                enable_or_disable_cell(ccitem, 1);
                pitem = ccitem;
                APPEND_GOLOMB_UE_ROW(num_ref_idx_l0_active_minus1);
                sliceHeader.num_ref_idx_l0_active_minus1 = num_ref_idx_l0_active_minus1;
                mdp_header_item *cccitem = APPEND_ENABLE_ROW("if( slice_type = = B )");
                if (slice_type % 5 == 1) {
                    enable_or_disable_cell(cccitem, 1);
                    pitem = cccitem;
                    APPEND_GOLOMB_UE_ROW(num_ref_idx_l1_active_minus1);
                    sliceHeader.num_ref_idx_l1_active_minus1 = num_ref_idx_l1_active_minus1;
                }
            }
        }
        citem = APPEND_ENABLE_ROW("if( nal_unit_type = = 20 | | nal_unit_type = = 21 )");
        if (currentAVCNal_->nal_unit_type == 20 || currentAVCNal_->nal_unit_type == 21) {
            enable_or_disable_cell(citem, 1);
            pitem = citem;
        } else {
            citem = APPEND_ENABLE_ROW("else");
            enable_or_disable_cell(citem, 1);
            pitem = citem;
            {
                citem = APPEND_ENABLE_ROW("ref_pic_list_modification( )");
                enable_or_disable_cell(citem, 1);
                avc_ref_pic_list_modification(sliceHeader, header, citem);
            }
        }
        citem = APPEND_ENABLE_ROW("if( ( weighted_pred_flag && ( slice_type = = P | | slice_type = = SP ) ) | | ( weighted_bipred_idc = = 1 && slice_type = = B ) )");
        if ((pps->weighted_pred_flag && (slice_type % 5 == 0 || slice_type % 5 == 3)) || (pps->weighted_bipred_idc == 1 && slice_type % 5 == 1)) {
            citem = APPEND_ENABLE_ROW("pred_weight_table( )");
            enable_or_disable_cell(citem, 1);
            avc_pred_weight_table(sliceHeader, header, citem,sps,pps);
        }
        citem = APPEND_ENABLE_ROW("if( nal_ref_idc != 0 )");
        pkt->nal_ref_idc = currentAVCNal_->nal_ref_idc;
        if (currentAVCNal_->nal_ref_idc != 0) {
            enable_or_disable_cell(citem, 1);
            citem = APPEND_ENABLE_ROW("dec_ref_pic_markin( )");
            enable_or_disable_cell(citem, 1);
            avc_dec_ref_pic_marking(sliceHeader, header, citem,sps,pps);
        }
        citem = APPEND_ENABLE_ROW("if( entropy_coding_mode_flag && slice_type != I && slice_type != SI )");
        if (pps->entropy_coding_mode_flag && slice_type % 5 != 2 && slice_type % 5 != 4 ) {
            enable_or_disable_cell(citem, 1);
            APPEND_GOLOMB_UE_ROW(cabac_init_idc);
        }
        APPEND_GOLOMB_SE_ROW(slice_qp_delta);
        citem = APPEND_ENABLE_ROW("if( slice_type = = SP | | slice_type = = SI ) ");
        if (slice_type % 5 == 3 || slice_type % 5 == 4) {
            enable_or_disable_cell(citem, 1);
            if (slice_type % 5 == 3) {
                APPEND_BITS_ROW_VAR(sp_for_switch_flag, 1);
            }
            APPEND_GOLOMB_SE_ROW(slice_qs_delta);
        }
        citem = APPEND_ENABLE_ROW("if( deblocking_filter_control_present_flag ) ");
        if (pps->deblocking_filter_control_present_flag) {
            enable_or_disable_cell(citem, 1);
            APPEND_GOLOMB_UE_ROW(disable_deblocking_filter_idc);
            if( disable_deblocking_filter_idc != 1 ) {
                APPEND_GOLOMB_SE_ROW(slice_alpha_c0_offset_div2);
                APPEND_GOLOMB_SE_ROW(slice_beta_offset_div2);
            }
        }
        citem = APPEND_ENABLE_ROW("if( num_slice_groups_minus1 > 0 && slice_group_map_type >= 3 && slice_group_map_type <= 5) ");
        if( pps->num_slice_groups_minus1 > 0 &&
           pps->slice_group_map_type >= 3 && pps->slice_group_map_type <= 5) {
            enable_or_disable_cell(citem, 1);
            //Ceil( Log2( PicSizeInMapUnits ÷ SliceGroupChangeRate + 1 ) )
            int SliceGroupChangeRate = pps->slice_group_change_rate_minus1 + 1;
            int PicSizeInMapUnits = pps->pic_size_in_map_units_minus1 + 1;
            int slice_group_change_cycle_bits = ceil(log2(PicSizeInMapUnits/SliceGroupChangeRate + 1));
            APPEND_BITS_ROW_VAR(slice_group_change_cycle, slice_group_change_cycle_bits);
        }
        pkt->sliceHeader = &sliceHeader;
        avc_packet_poc(sliceHeader, pkt, sps, pps);
        
        if (sliceHeader.slice_type % 5 == 0) { //P
            if (pkt->flag != MALPacketFlag::none) {
                malFmtCtx_->shallowCheck.warning.push_back(fmt::format("第{}个packet中slice_type校验异常",pkt->number));
            }
            pkt->flag = MALPacketFlag::p;
        } else  if (sliceHeader.slice_type % 5 == 1) { //B
            if (pkt->flag != MALPacketFlag::none) {
                malFmtCtx_->shallowCheck.warning.push_back(fmt::format("第{}个packet中slice_type校验异常",pkt->number));
            }
            pkt->flag = MALPacketFlag::b;
        } else  if (sliceHeader.slice_type % 5 == 2) { //I
            if (pkt->flag != MALPacketFlag::idr) {
                pkt->flag == MALPacketFlag::i;
            }
            
        }
    }
    
    
end:
    return ret;
}


/*
 
 0 TRAIL_N
 1 TRAIL_R
 
 2 TSA_N
 3 TSA_R
 
 4 STSA_N
 5 STSA_R
 
 6 RADL_N
 7 RADL_R
 
 8 RASL_N
 9 RASL_R
 
 10 RSV_VCL_N10
 12 RSV_VCL_N12
 14 RSV_VCL_N14
 
 11 RSV_VCL_R11
 13 RSV_VCL_R13
 15 RSV_VCL_R15
 
 16 BLA_W_LP
 17 BLA_W_RADL
 18 BLA_N_LP
 
 19 IDR_W_RADL
 20 IDR_N_LP
 
 21 CRA_NUT
 
 22 RSV_IRAP_VCL22
 23 RSV_IRAP_VCL23
 
 24..31 RSV_VCL24..RSV_VCL31
 
 32 VPS_NUT
 
 33 SPS_NUT
 
 34 PPS_NUT
 
 35 AUD_NUT
 
 36 EOS_NUT
 
 37 EOB_NUT
 
 38 FD_NUT
 
 39 PREFIX_SEI_NUT
 40 SUFFIX_SEI_NUT
 

 */
int MALCodecParser::hevc_poc(std::shared_ptr<MALHEVCSPS> sps, int pocTid0, int poc_lsb, int nal_unit_type) {
    int max_poc_lsb  = 1 << (sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
    int prev_poc_lsb = pocTid0 % max_poc_lsb;
    int prev_poc_msb = pocTid0 - prev_poc_lsb;
    int poc_msb;
    
    if (poc_lsb < prev_poc_lsb && prev_poc_lsb - poc_lsb >= max_poc_lsb / 2)
        poc_msb = prev_poc_msb + max_poc_lsb;
    else if (poc_lsb > prev_poc_lsb && poc_lsb - prev_poc_lsb > max_poc_lsb / 2)
        poc_msb = prev_poc_msb - max_poc_lsb;
    else
        poc_msb = prev_poc_msb;
    
    // For BLA picture types, POCmsb is set to 0.
    if (nal_unit_type == 16   ||
        nal_unit_type == 17 ||
        nal_unit_type == 18)
        poc_msb = 0;
    
    return poc_msb + poc_lsb;
}
int MALCodecParser::parse_hevc_slice(mdp_video_header *header, mdp_header_item *pitem,std::shared_ptr<MALPacket> pkt) {
    {
        mdp_header_item *citem = APPEND_ENABLE_ROW("slice_segment_header");
        ENABLE_CELL(citem);
        auto nal = std::dynamic_pointer_cast<MALHEVCSliceSegmentLayerRbsp>(currentHEVCNal_);
        int first_slice_segment_in_pic_flag = APPEND_BITS_ROW("first_slice_segment_in_pic_flag", 1);
        nal->header.first_slice_segment_in_pic_flag = first_slice_segment_in_pic_flag;
        citem = APPEND_ENABLE_ROW("if( nal_unit_type >= BLA_W_LP && nal_unit_type <= RSV_IRAP_VCL23 )");
        if (nal->nal_unit_type >= 16 && nal->nal_unit_type <= 23) {
            ENABLE_CELL(citem);
            nal->header.no_output_of_prior_pics_flag = APPEND_BITS_ROW("no_output_of_prior_pics_flag", 1);
        }
        APPEND_GOLOMB_UE_ROW(slice_pic_parameter_set_id);
        nal->header.slice_pic_parameter_set_id = slice_pic_parameter_set_id;
        
        std::shared_ptr<MALHEVCVPS> vps= nullptr;
        std::shared_ptr<MALHEVCSPS> sps= nullptr;
        std::shared_ptr<MALHEVCPPS> pps= nullptr;
        if (hvcc_) {
            for (auto el : hvcc_->ppsList) {
                if (el->pic_parameter_set_id == slice_pic_parameter_set_id) {
                    pps = el;
                    break;
                }
            }
            if (pps) {
                for (auto el : hvcc_->spsList) {
                    if (el->seq_parameter_set_id == pps->seq_parameter_set_id) {
                        sps = el;
                        break;
                    }
                }
            } else {
                malFmtCtx_->shallowCheck.errors.push_back("slice not find pps!");
                goto end;
            }
            if (!sps) {
                malFmtCtx_->shallowCheck.errors.push_back("slice not find sps!");
                goto end;
            }
            for (auto el : hvcc_->vpsList) {
                if (el->video_parameter_set_id == sps->sps_video_parameter_set_id) {
                    vps = el;
                    break;
                }
            }
            if (!vps) {
                malFmtCtx_->shallowCheck.errors.push_back("slice not find vps!");
                goto end;
            }
        } else {
            malFmtCtx_->shallowCheck.errors.push_back("slice not find avcc!");
            goto end;
        }
        citem = APPEND_ENABLE_ROW("if( !first_slice_segment_in_pic_flag ) ");
        int dependent_slice_segment_flag = 0;
        if (!first_slice_segment_in_pic_flag) {
            ENABLE_CELL(citem);
            mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( dependent_slice_segments_enabled_flag )");
            if (pps->dependent_slice_segments_enabled_flag) {
                ENABLE_CELL(ccitem);
                dependent_slice_segment_flag = APPEND_BITS_ROW("dependent_slice_segment_flag", 1);
            }
            int MinCbLog2SizeY = sps->log2_min_luma_coding_block_size_minus3 + 3;
            int CtbLog2SizeY = MinCbLog2SizeY + sps->log2_diff_max_min_luma_coding_block_size;
            int CtbSizeY = (1 << CtbLog2SizeY);
            int PicWidthInCtbsY = ceil(sps->pic_width_in_luma_samples/CtbSizeY);
            int PicHeightInCtbsY = ceil( sps->pic_height_in_luma_samples/CtbSizeY );
            int PicSizeInCtbsY = PicWidthInCtbsY * PicHeightInCtbsY;
            int len = ceil(log2(PicSizeInCtbsY));
            int slice_segment_address = APPEND_BITS_ROW("slice_segment_address", len);
            nal->header.slice_segment_address = slice_segment_address;
        }
        citem = APPEND_ENABLE_ROW("if( !int dependent_slice_segment_flag ) ");
        if (!dependent_slice_segment_flag) {
            ENABLE_CELL(citem);
            int i = 0;
            mdp_header_item* ccitem = APPEND_ENABLE_ROW("if (pps->num_extra_slice_header_bits > i)");
            if (pps->num_extra_slice_header_bits > i) {
                ENABLE_CELL(ccitem);
                i++;
                APPEND_BITS_ROW(fmt::format("discardable_flag[{}]",i), 1);
            }
            ccitem = APPEND_ENABLE_ROW("if (pps->num_extra_slice_header_bits > i)");
            if (pps->num_extra_slice_header_bits > i) {
                ENABLE_CELL(ccitem);
                i++;
                APPEND_BITS_ROW(fmt::format("cross_layer_bla_flag[{}]",i), 1);
            }
            ccitem = APPEND_ENABLE_ROW("for (; i < pps->num_extra_slice_header_bits; i++)");
            for (; i < pps->num_extra_slice_header_bits; i++) {
                ENABLE_CELL(ccitem);
                APPEND_BITS_ROW(fmt::format("slice_reserved_flag[{}]",i), 1);
            }
            APPEND_GOLOMB_UE_ROW(slice_type);
            nal->header.slice_type = slice_type;
            ccitem = APPEND_ENABLE_ROW("if( output_flag_present_flag )");
            if(pps->output_flag_present_flag ) {
                ENABLE_CELL(ccitem);
                APPEND_BITS_ROW("pic_output_flag", 1);
            }
            ccitem = APPEND_ENABLE_ROW("if( separate_colour_plane_flag = = 1 )");
            if( sps->separate_colour_plane_flag == 1 ) {
                ENABLE_CELL(ccitem);
                APPEND_BITS_ROW("colour_plane_id", 2);
            }
            
            ccitem = APPEND_ENABLE_ROW("if( ( nuh_layer_id > 0 && !poc_lsb_not_present_flag[ LayerIdxInVps[ nuh_layer_id ] ] ) | | \
                                       ( nal_unit_type != IDR_W_RADL && nal_unit_type != IDR_N_LP ) )");
            if( (nal->nuh_layer_id > 0 &&
                 !vps->poc_lsb_not_present_flag[ vps->LayerIdxInVps[ nal->nuh_layer_id ] ] ) ||
               ( nal->nal_unit_type !=  MALHEVCNALTYPE::IDR_W_RADL && nal->nal_unit_type != MALHEVCNALTYPE::IDR_N_LP ) ) {
                ENABLE_CELL(ccitem);
                int len = sps->log2_max_pic_order_cnt_lsb_minus4 + 4;
                APPEND_BITS_ROW_VAR(slice_pic_order_cnt_lsb, len);
                nal->header.slice_pic_order_cnt_lsb = slice_pic_order_cnt_lsb;
            }
            ccitem = APPEND_ENABLE_ROW("if( nal_unit_type != IDR_W_RADL && nal_unit_type != IDR_N_LP )");
            if( nal->nal_unit_type != MALHEVCNALTYPE::IDR_W_RADL && nal->nal_unit_type != MALHEVCNALTYPE::IDR_N_LP ) {
                ENABLE_CELL(ccitem);
                APPEND_BITS_ROW_VAR(short_term_ref_pic_set_sps_flag, 1);
                auto cccitem = APPEND_ENABLE_ROW("if( !short_term_ref_pic_set_sps_flag )");
                if( !short_term_ref_pic_set_sps_flag ) {
                    ENABLE_CELL(cccitem);
                    auto ccccitem = APPEND_ENABLE_ROW("st_ref_pic_set");
                    {
                        ENABLE_CELL(ccccitem);
                        int num_delta_pocs[64] = {0};
                        int num_negative_pics[64] = {0};
                        int num_positive_pics[64] = {0};
                        hevc_st_ref_pic_set(ccccitem, sps->num_short_term_ref_pic_sets,sps->num_short_term_ref_pic_sets,num_delta_pocs,num_negative_pics,num_positive_pics);
                    }
                    
                } else if( sps->num_short_term_ref_pic_sets > 1 ) {
                    auto cccitem = APPEND_ENABLE_ROW("else if( sps->num_short_term_ref_pic_sets > 1 )");
                    ENABLE_CELL(cccitem);
                    int len =  ceil( log2( sps->num_short_term_ref_pic_sets ) ) ;
                    APPEND_BITS_ROW_VAR(short_term_ref_pic_set_idx, len);
                }
                //if( long_term_ref_pics_present_flag )  后边的没解析，有需要的字段再解析
            }
        }
#define IS_IDR(s) ((s)->nal_unit_type == int(MALHEVCNALTYPE::IDR_W_RADL) || (s)->nal_unit_type == int(MALHEVCNALTYPE::IDR_N_LP))
        if (pkt->number == 1224) {
            int x = 0;
        }
        int poc = 0;
        if (!IS_IDR(nal)) {
            poc = hevc_poc(sps, hevc_pocTid0, nal->header.slice_pic_order_cnt_lsb, nal->nal_unit_type);
        } else {
            pkt->flag = MALPacketFlag::idr;
        }
        if (nal->header.slice_type == 0) { //B
            if (pkt->flag != MALPacketFlag::none) {
                malFmtCtx_->shallowCheck.warning.push_back(fmt::format("第{}个packet中slice_type校验异常",pkt->number));
            }
            pkt->flag = MALPacketFlag::b;
        } else if (nal->header.slice_type == 1) { // P
            if (pkt->flag != MALPacketFlag::none) {
                malFmtCtx_->shallowCheck.warning.push_back(fmt::format("第{}个packet中slice_type校验异常",pkt->number));
            }
            pkt->flag = MALPacketFlag::p;
        } else if (nal->header.slice_type == 2) { //I
            if (pkt->flag != MALPacketFlag::idr) {
                pkt->flag = MALPacketFlag::i;
            }
        } else {
            malFmtCtx_->shallowCheck.errors.push_back(fmt::format("第{}个packet中slice_type不在0-2之间，存在异常",pkt->number));
        }
        pkt->poc = poc;
        if (nal->nuh_temporal_id_plus1 - 1 == 0 &&
                nal->nal_unit_type != MALHEVCNALTYPE::TRAIL_N &&
                nal->nal_unit_type != MALHEVCNALTYPE::TSA_N &&
                nal->nal_unit_type != MALHEVCNALTYPE::STSA_N &&
                nal->nal_unit_type != MALHEVCNALTYPE::RADL_N &&
                nal->nal_unit_type != MALHEVCNALTYPE::RASL_N &&
                nal->nal_unit_type != MALHEVCNALTYPE::RADL_R &&
                nal->nal_unit_type != MALHEVCNALTYPE::RASL_R)
                hevc_pocTid0 = poc;
    }
    end:
    return 0;
}
