#include "mal_h2645.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>
#include <iostream>
extern "C" {
#include "libavutil/mem.h"
#include "../../utils/mdp_string.h"
#include "../../utils/mdp_int.h"
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
    std::cout << "current pos:" <<  datasource->currentBitsPosition() << std::endl;
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
static void enable_or_disable_cell(mdp_header_item *item, int enable) {
    item->cells[0].enable = enable;
}

#define APPEND_BITS_ROW(name,bits) append_bits_row(pitem,name,bits, datasource)
#define APPEND_BITS_ROW_NO_RBSP(name,bits) append_bits_row(pitem,name,bits, datasource, false)
#define APPEND_ENABLE_ROW(name) append_enable_row(pitem,name,0)
#define APPEND_VALUE_ROW(name,val) append_value_row(pitem,name,val)

#define APPEND_GOLOMB_UE_ROW(name) int name = golomb_ue(datasource); APPEND_VALUE_ROW(#name,name)
#define APPEND_GOLOMB_SE_ROW(name) int name = golomb_se(datasource); APPEND_VALUE_ROW(#name,name)
#define APPEND_GOLOMB_UE_ROW_NO_VAR(name) APPEND_VALUE_ROW(#name,golomb_ue(datasource))
#define APPEND_GOLOMB_SE_ROW_NO_VAR(name) APPEND_VALUE_ROW(#name,golomb_se(datasource))
#define ENABLE_CELL(item) mdp_header_item *pitem = item; enable_or_disable_cell(pitem,1)

static void parse_avc_nal(mdp_header_item *item, std::shared_ptr<IDataSource> datasource) {
    mdp_header_item * pitem = item;
    int forbidden_zero_bit = APPEND_BITS_ROW("forbidden_zero_bit",1);
    int nal_ref_idc = APPEND_BITS_ROW("nal_ref_idc",2); 
    int nal_unit_type = APPEND_BITS_ROW("nal_unit_type",5);

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



void avc_hrd_parameters(std::shared_ptr<IDataSource> datasource,mdp_header_item *item) {
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

static void vui_parameters(std::shared_ptr<IDataSource> datasource, int hevc, mdp_header_item *item) {
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
        int video_full_range_flag = APPEND_BITS_ROW("video_format",1);
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
            avc_hrd_parameters(datasource,ccitem);
        }
        int vcl_hrd_parameters_present_flag = APPEND_BITS_ROW("vcl_hrd_parameters_present_flag",1);
        citem = APPEND_ENABLE_ROW("if(vcl_hrd_parameters_present_flag)");
        if(vcl_hrd_parameters_present_flag) {
            ENABLE_CELL(citem);
            mdp_header_item *ccitem = APPEND_ENABLE_ROW("hrd_parameters");
            enable_or_disable_cell(ccitem,1);
            avc_hrd_parameters(datasource,ccitem);
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
                avc_hrd_parameters(datasource,cccitem);
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
static void scaling_list(std::shared_ptr<IDataSource> datasource, int *scalingList,int sizeOfScalingList,int* useDefaultScalingMatrixFlag,mdp_header_item *item) {
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
static int parse_avc_sps(mdp_video_header *header, mdp_header_item *item,std::shared_ptr<IDataSource> datasource) {
    int64_t read_bits = 0;
    mdp_header_item *pitem = item;
    parse_avc_nal(item,datasource);
    int profile_idc = APPEND_BITS_ROW("profile_idc",8);
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
   APPEND_GOLOMB_UE_ROW(seq_parameter_set_id);
    
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
        header->sps.chroma_format_idc = chroma_format_idc;
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("if(chroma_format_idc == 3)");
        if(chroma_format_idc == 3) {
            ENABLE_CELL(ccitem);
            APPEND_BITS_ROW("separate_colour_plane_flag",1);
        }
        APPEND_GOLOMB_UE_ROW(bit_depth_luma_minus8);
        APPEND_GOLOMB_UE_ROW(bit_depth_chroma_minus8);
        
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
                int seq_scaling_list_present_flag = datasource->readBitsInt64(1);//mdp_data_buffer_read_bits_ival(dctx,buffer,1,1,&read_bits);
                std::string name = fmt::format("seq_scaling_list_present_flag[{}]",i);
                APPEND_VALUE_ROW(name.c_str(),seq_scaling_list_present_flag);
                name = fmt::format("if(seq_scaling_list_present_flag[{}])",i);
                mdp_header_item * cccitem = APPEND_ENABLE_ROW(name.c_str());
                if(seq_scaling_list_present_flag) {
                    ENABLE_CELL(cccitem);
                     if(i<6) {
                         scaling_list(datasource,ScalingList4x4[i],16,&UseDefaultScalingMatrix4x4Flag[i],cccitem);
                     } else {
                         scaling_list(datasource,ScalingList8x8[i-6],64,&UseDefaultScalingMatrix8x8Flag[i],cccitem);
                     }

                }

            }
        }

    }
    APPEND_GOLOMB_UE_ROW(log2_max_frame_num_minus4);
    APPEND_GOLOMB_UE_ROW(pic_order_cnt_type);
    citem = APPEND_ENABLE_ROW("if(pic_order_cnt_type == 0)");
    if(pic_order_cnt_type == 0) {
        ENABLE_CELL(citem);
        APPEND_GOLOMB_UE_ROW(log2_max_pic_order_cnt_lsb_minus4);
    } else if(pic_order_cnt_type == 1){
        citem = APPEND_ENABLE_ROW("if(pic_order_cnt_type == 1)");
        ENABLE_CELL(citem);
        APPEND_BITS_ROW("delta_pic_order_always_zero_flag",1);
    }
    APPEND_GOLOMB_UE_ROW(max_num_ref_frames);
   
    APPEND_BITS_ROW("gaps_in_frame_num_value_allowed_flag",1);

    APPEND_GOLOMB_UE_ROW(pic_width_in_mbs_minus1);
    
    APPEND_GOLOMB_UE_ROW(pic_height_in_map_units_minus1);
    
    // if(mediainfo_->currentTrack) {
    //     if(mediainfo_->currentTrack->width <= 0) {
    //         mediainfo_->currentTrack->width = 16 *(pic_width_in_mbs_minus1 + 1);
    //     }
    //     if(mediainfo_->currentTrack->height <= 0) {
    //         mediainfo_->currentTrack->height = 16 *(pic_height_in_map_units_minus1 + 1);
    //     }
    // }
    int frame_mbs_only_flag = APPEND_BITS_ROW("frame_mbs_only_flag",1);

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
    }
    int direct_8x8_inference_flag = APPEND_BITS_ROW("direct_8x8_inference_flag",1);
    int frame_cropping_flag = APPEND_BITS_ROW("frame_cropping_flag",1);
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
        vui_parameters(datasource,0,ccitem);
    }
    return read_bits;
}
int more_rbsp_data(std::shared_ptr<IDataSource> datasource) {
    int left = datasource->totalSize() * 8 - datasource->currentBitsPosition();
    if(left > 8) {
        return 1;
    }
    if(left <= 0) {
        return 0;
    }
    int value = 0;
    if (left == 8) {
        value = datasource->readBytesInt64(1,true);//mdp_data_buffer_read8(dctx,buffer);
    } else {
        unsigned char *temp = datasource->current();
        value = mdp_strconvet_to_int((char *)temp, 1, 1);//buffer->read_buffer_8[0];
    }
    value = mdp_char_reverse(value);
    int i ;
    for(i = 7; i>= 0; i--) {
        if(mdp_get_bit_from_byte(value,i) == 1) break;
    }
    if(i>0) return 1;
    return 0;
}
void rbsp_trailing_bits(mdp_header_item *item, std::shared_ptr<IDataSource> datasource) {
    mdp_header_item *pitem = item;
    mdp_header_item *citem = APPEND_ENABLE_ROW("rbsp_trailing_bits");
    enable_or_disable_cell(citem,1);
    pitem = citem;
    int rbsp_stop_one_bit = APPEND_BITS_ROW("rbsp_stop_one_bit",1);
    int index = 0;
    citem = APPEND_ENABLE_ROW("while(offset % 8 != 0)");
    while(datasource->currentBitsPosition() % 8 != 0) {
        ENABLE_CELL(citem);
        int rbsp_alignment_zero_bit = APPEND_BITS_ROW(fmt::format("rbsp_alignment_zero_bit[{}]",index),1);
        index ++;
    }
}
static int parse_avc_pps(mdp_video_header *header, mdp_header_item *item,std::shared_ptr<IDataSource> datasource, int64_t max_bits) {
    int64_t read_bits = 0;
    int64_t * had_read_bits = &read_bits;
    parse_avc_nal(item,datasource);
    mdp_header_item *pitem = item;
    APPEND_GOLOMB_UE_ROW(pic_parameter_set_id);
    APPEND_GOLOMB_UE_ROW(seq_parameter_set_id);
    
    int entropy_coding_mode_flag = APPEND_BITS_ROW("entropy_coding_mode_flag",1);
    // if(mediainfo_->currentTrack) {
    //     mediainfo_->currentTrack->fileds.push_back(std::make_shared<BoxFiled>("coding mode",entropy_coding_mode_flag ? "CABAC" : "CAVLC"));
    // }
    
    int bottom_field_pic_order_in_frame_present_flag = APPEND_BITS_ROW("bottom_field_pic_order_in_frame_present_flag",1);
    APPEND_GOLOMB_UE_ROW(num_slice_groups_minus1);
    
    mdp_header_item *citem = APPEND_ENABLE_ROW("if( num_slice_groups_minus1 > 0 ) ");
    if(num_slice_groups_minus1 > 0) {
        ENABLE_CELL(citem);
        APPEND_GOLOMB_UE_ROW(slice_group_map_type);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("if( slice_group_map_type = = 0 )");
        if(slice_group_map_type == 0) {
            ENABLE_CELL(ccitem);
            for( int iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ ) {
                std::string name = fmt::format("run_length_minus1[{}]",iGroup);
                int run_length_minus1 = golomb_ue(datasource);
                APPEND_VALUE_ROW(name.c_str(),run_length_minus1);
            }
        }
        ccitem = APPEND_ENABLE_ROW("else if( slice_group_map_type = = 2 )");
        if(slice_group_map_type == 2) {
            ENABLE_CELL(ccitem);
            for( int iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ ) {
                std::string name = fmt::format("top_left[{}]",iGroup);
                int top_left = golomb_ue(datasource);
                APPEND_VALUE_ROW(name.c_str(),top_left);
                name = fmt::format("bottom_right[{}]",iGroup);
                int bottom_right = golomb_ue(datasource);
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

    APPEND_GOLOMB_SE_ROW(pic_init_qp_minus26);
    APPEND_GOLOMB_SE_ROW(pic_init_qs_minus26);
    APPEND_GOLOMB_SE_ROW(chroma_qp_index_offset);
    

    int deblocking_filter_control_present_flag = APPEND_BITS_ROW("deblocking_filter_control_present_flag",1);

    int constrained_intra_pred_flag = APPEND_BITS_ROW("constrained_intra_pred_flag",1);

    int redundant_pic_cnt_present_flag = APPEND_BITS_ROW("redundant_pic_cnt_present_flag",1);

    citem = APPEND_ENABLE_ROW("if( more_rbsp_data( ) )");
    if(more_rbsp_data(datasource)) {
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
                        scaling_list(datasource,ScalingList4x4[i],16,&UseDefaultScalingMatrix4x4Flag[i],pitem);
                    } else {
                        ccccitem = APPEND_ENABLE_ROW("else");
                        ENABLE_CELL(ccccitem);
                        scaling_list(datasource,ScalingList8x8[i-6],64,&UseDefaultScalingMatrix8x8Flag[i-6],pitem);
                    }
                }
            }

        }
        APPEND_GOLOMB_SE_ROW(second_chroma_qp_index_offset);
    }
    rbsp_trailing_bits(pitem,datasource);
    return read_bits;
}
mdp_video_header * mdp_parse_avcc(std::shared_ptr<IDataSource> datasource) {
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
        }
    }
    for(int i = 0; i < numOfSequenceParameterSets; i++) {
        std::string name = fmt::format("sps[{}]", i);
        mdp_header_item *item = APPEND_ENABLE_ROW(name.c_str());
        enable_or_disable_cell(item,1);
        uint64_t sequenceParameterSetLength = datasource->readBytesInt64(2);
        int64_t next = datasource->currentBytesPosition() + sequenceParameterSetLength;
        parse_avc_sps(header,item,datasource);
        datasource->seekBytes(next,SEEK_SET);
    }
    uint64_t val = datasource->readBytesInt64(1);;//mdp_data_buffer_read_bits_ival(dctx,buffer,8,0,&had_read_bits);
    uint64_t numOfPictureParameterSets = val;
    for(int i = 0; i < numOfPictureParameterSets; i++) {
        std::string name = fmt::format("pps[{}]", i);
        mdp_header_item *item = APPEND_ENABLE_ROW(name.c_str());
        enable_or_disable_cell(item,1);
        uint64_t pictureParameterSetLength = datasource->readBytesInt64(2);
        int64_t next = datasource->currentBytesPosition() + pictureParameterSetLength;
        int64_t read_bits = parse_avc_pps(header,item,datasource,pictureParameterSetLength * 8);
        datasource->seekBytes(next, SEEK_SET);
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
static void parse_hvc_nal(mdp_header_item *item, std::shared_ptr<IDataSource> datasource) {
    mdp_header_item * pitem = item;
    int forbidden_zero_bit = APPEND_BITS_ROW("forbidden_zero_bit",1);
    int nal_unit_type = APPEND_BITS_ROW("nal_unit_type",6);
    int nuh_layer_id = APPEND_BITS_ROW("nuh_layer_id",6);
    int nuh_temporal_id_plus1 = APPEND_BITS_ROW("nuh_temporal_id_plus1",3);
}

static void hevc_scaling_list_data(mdp_header_item *pitem, std::shared_ptr<IDataSource> datasource){
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
static void hevc_st_ref_pic_set(mdp_header_item *pitem, std::shared_ptr<IDataSource> datasource,int stRpsIdx,int num_short_term_ref_pic_sets,int num_delta_pocs[64]){
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
static void hevc_sps_3d_extension(mdp_header_item *pitem, std::shared_ptr<IDataSource> datasource){
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
static void hevc_profile_tier_level(mdp_header_item *pitem, std::shared_ptr<IDataSource> datasource,int profilePresentFlag, int maxNumSubLayersMinus1) {
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
static void hevc_sub_layer_hrd_parameters(mdp_header_item *pitem, std::shared_ptr<IDataSource> datasource,int i,int cpb_cnt_minus1,int sub_pic_hrd_params_present_flag) {
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
void hevc_hrd_parameters(mdp_header_item *pitem, std::shared_ptr<IDataSource> datasource, int commonInfPresentFlag, int maxNumSubLayersMinus1) {
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
            hevc_sub_layer_hrd_parameters(ccitem,datasource,i,cpb_cnt_minus1,sub_pic_hrd_params_present_flag);
        }

        ccitem = APPEND_ENABLE_ROW("if( vcl_hrd_parameters_present_flag )");
        if( vcl_hrd_parameters_present_flag ) {
            ENABLE_CELL(ccitem);
            hevc_sub_layer_hrd_parameters(ccitem,datasource,i,cpb_cnt_minus1,sub_pic_hrd_params_present_flag);
        }
    }
}
static int parse_hvc_vps(mdp_video_header *header, mdp_header_item *item,std::shared_ptr<IDataSource> datasource) {
    int ret = 0;
    mdp_header_item *pitem = item;
    parse_hvc_nal(item,datasource);
    int vps_video_parameter_set_id = APPEND_BITS_ROW("vps_video_parameter_set_id",4);
    int vps_base_layer_internal_flag = APPEND_BITS_ROW("vps_base_layer_internal_flag",1);
    int vps_base_layer_available_flag = APPEND_BITS_ROW("vps_base_layer_available_flag",1);
    int vps_max_layers_minus1 = APPEND_BITS_ROW("vps_max_layers_minus1",6);
    int vps_max_sub_layers_minus1 = APPEND_BITS_ROW("vps_max_sub_layers_minus1",3);
    int vps_temporal_id_nesting_flag = APPEND_BITS_ROW("vps_temporal_id_nesting_flag",1);
    int vps_reserved_0xffff_16bits = APPEND_BITS_ROW("vps_reserved_0xffff_16bits",16);
    hevc_profile_tier_level(pitem,datasource,1,vps_max_sub_layers_minus1);
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
    for(int i = 1; i <= vps_num_layer_sets_minus1; i++ )  {
        ENABLE_CELL(citem);
        mdp_header_item * ccitem = APPEND_ENABLE_ROW("for( j = 0; j <= vps_max_layer_id; j++ ) ");
        for( int j = 0; j <= vps_max_layer_id; j++ ) {
            ENABLE_CELL(ccitem);
            int layer_id_included_flag = APPEND_BITS_ROW(fmt::format("layer_id_included_flag[{}][{}]",i,j),1);
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
            int cprms_present_flag = 1;
            if( i > 0 ) {
                ENABLE_CELL(cccitem);
                APPEND_BITS_ROW(fmt::format("cprms_present_flag[{}]",i),1);
            }
            cccitem = APPEND_ENABLE_ROW(fmt::format("hrd_parameters[{}]",i));
            enable_or_disable_cell(cccitem, 1);
            hevc_hrd_parameters(cccitem,datasource,cprms_present_flag,vps_max_sub_layers_minus1);
        }
        
    }
    int vps_extension_flag = APPEND_BITS_ROW("vps_extension_flag",1);
    citem = APPEND_ENABLE_ROW("if( vps_extension_flag )");
    if( vps_extension_flag ) {
        ENABLE_CELL(citem);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("while( more_rbsp_data( ) )");
        int index = 0;
        while( more_rbsp_data(datasource)) {
            ENABLE_CELL(ccitem);
            int vps_extension_data_flag = APPEND_BITS_ROW(fmt::format("vps_extension_data_flag[{}]",index),1);
            index ++;
        }
    }
    rbsp_trailing_bits(pitem, datasource);
    return ret;
}

static int parse_hvc_sps(mdp_video_header *header, mdp_header_item *item,std::shared_ptr<IDataSource> datasource) {
    int ret = 0;
    mdp_header_item *pitem = item;
    parse_hvc_nal(item,datasource);
    int64_t sps_video_parameter_set_id = APPEND_BITS_ROW("sps_video_parameter_set_id",4);
    int sps_max_sub_layers_minus1 = APPEND_BITS_ROW("sps_max_sub_layers_minus1",3);
    int64_t sps_temporal_id_nesting_flag = APPEND_BITS_ROW("sps_temporal_id_nesting_flag",1);
    hevc_profile_tier_level(pitem,datasource,1,sps_max_sub_layers_minus1);
    APPEND_GOLOMB_UE_ROW(sps_seq_parameter_set_id);
    APPEND_GOLOMB_UE_ROW(chroma_format_idc);
    mdp_header_item *citem = APPEND_ENABLE_ROW("if( chroma_format_idc = = 3 )");
    if(chroma_format_idc == 3) {
        ENABLE_CELL(citem);
        APPEND_BITS_ROW("separate_colour_plane_flag",1);
    }
    APPEND_GOLOMB_UE_ROW(pic_width_in_luma_samples);
    APPEND_GOLOMB_UE_ROW(pic_height_in_luma_samples);

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
   
    int sps_sub_layer_ordering_info_present_flag  = APPEND_BITS_ROW("sps_sub_layer_ordering_info_present_flag",1);
    
    citem = APPEND_ENABLE_ROW("for( i = ( sps_sub_layer_ordering_info_present_flag ? 0 : sps_max_sub_layers_minus1 ); i <= sps_max_sub_layers_minus1; i++ )");
    for( int i = ( sps_sub_layer_ordering_info_present_flag ? 0 : sps_max_sub_layers_minus1 ); i <= sps_max_sub_layers_minus1; i++ ) {
        ENABLE_CELL(citem);
        APPEND_GOLOMB_UE_ROW(sps_max_dec_pic_buffering_minus1);
        APPEND_GOLOMB_UE_ROW(sps_max_num_reorder_pics);
        APPEND_GOLOMB_UE_ROW(sps_max_latency_increase_plus1);
    }
    APPEND_GOLOMB_UE_ROW(log2_min_luma_coding_block_size_minus3);
    APPEND_GOLOMB_UE_ROW(log2_diff_max_min_luma_coding_block_size);
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
            hevc_scaling_list_data(pitem, datasource);
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
    citem = APPEND_ENABLE_ROW("for( i = 0; i < num_short_term_ref_pic_sets; i++)");
    int num_delta_pocs[64];
    for( int i = 0; i < num_short_term_ref_pic_sets; i++) {
        ENABLE_CELL(citem);
        mdp_header_item * ccitem = APPEND_ENABLE_ROW(fmt::format("st_ref_pic_set[{}]",i));
        enable_or_disable_cell(ccitem, 1);
        hevc_st_ref_pic_set(pitem,datasource,i,num_short_term_ref_pic_sets,num_delta_pocs);
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
        vui_parameters(datasource,1,ccitem);
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
        hevc_sps_3d_extension(citem,datasource);
    }
    citem = APPEND_ENABLE_ROW("if( sps_extension_5bits )");
    if( sps_extension_5bits ) {
        ENABLE_CELL(citem);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("while( more_rbsp_data( ) )");
        enable_or_disable_cell(ccitem, 1);
        int index = 0;
        while(more_rbsp_data(datasource)) {
            int vps_extension_data_flag = APPEND_BITS_ROW(fmt::format("vps_extension_data_flag[{}]",index),1);
            index ++;
        }
    }
    rbsp_trailing_bits(pitem, datasource);
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
static void hevc_pps_range_extension(mdp_header_item *pitem,std::shared_ptr<IDataSource> datasource,int transform_skip_enabled_flag) {
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
static void hevc_colour_mapping_octants(mdp_header_item *pitem,std::shared_ptr<IDataSource> datasource, int inpDepth, int idxY, int idxCb, int idxCr, int inpLength,int cm_octant_depth,int PartNumY,int cm_delta_flc_bits_minus1) {
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
                    hevc_colour_mapping_octants(ccitem,datasource,inpDepth + 1, idxY + PartNumY * k * inpLength / 2, idxCb + m * inpLength / 2, idxCr + n * inpLength / 2, inpLength / 2,cm_octant_depth,PartNumY,cm_delta_flc_bits_minus1);
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
static void hevc_colour_mapping_table(mdp_header_item *pitem,std::shared_ptr<IDataSource> datasource) {
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
    hevc_colour_mapping_octants(citem,datasource,0,0,0,0,1 << cm_octant_depth,cm_octant_depth,PartNumY,cm_delta_flc_bits_minus1);
}
static void hevc_pps_multilayer_extension(mdp_header_item *pitem,std::shared_ptr<IDataSource> datasource) {
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
            hevc_colour_mapping_table(cccitem,datasource);
        }
    }
}
static void hevc_delta_dlt(mdp_header_item *pitem, std::shared_ptr<IDataSource> datasource, int i, int pps_bit_depth_for_depth_layers_minus8){
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
void hevc_pps_3d_extension(mdp_header_item *pitem, std::shared_ptr<IDataSource> datasource) {
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
                    hevc_delta_dlt(cccccitem,datasource,i,pps_bit_depth_for_depth_layers_minus8);
                }
            }
        }
    }
}
static int parse_hvc_pps(mdp_video_header *header, mdp_header_item *item,std::shared_ptr<IDataSource> datasource) {
    int ret = 0;
    mdp_header_item *pitem = item;
    parse_hvc_nal(item,datasource);
    APPEND_GOLOMB_UE_ROW(pps_pic_parameter_set_id);
    APPEND_GOLOMB_UE_ROW(pps_seq_parameter_set_id);
    int dependent_slice_segments_enabled_flag = APPEND_BITS_ROW("dependent_slice_segments_enabled_flag", 1);
    int output_flag_present_flag = APPEND_BITS_ROW("output_flag_present_flag", 1);
    int num_extra_slice_header_bits = APPEND_BITS_ROW("num_extra_slice_header_bits", 3);
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
        hevc_scaling_list_data(citem,datasource);
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
        hevc_pps_range_extension(citem,datasource,transform_skip_enabled_flag);
    }
    citem = APPEND_ENABLE_ROW("if( pps_multilayer_extension_flag ) ");
    if( pps_multilayer_extension_flag ) {
        ENABLE_CELL(citem);
        hevc_pps_multilayer_extension(citem,datasource);
    }
    
    citem = APPEND_ENABLE_ROW("if( pps_3d_extension_flag ) ");
    if( pps_3d_extension_flag ){
        ENABLE_CELL(citem);
        hevc_pps_3d_extension(citem,datasource);
    }
    citem = APPEND_ENABLE_ROW("if( pps_extension_5bits ) ");
    if( pps_extension_5bits ) {
        ENABLE_CELL(citem);
        mdp_header_item *ccitem = APPEND_ENABLE_ROW("while( more_rbsp_data( ) )");
        int index = 0;
        while( more_rbsp_data(datasource)) {
            int pps_extension_data_flag = APPEND_BITS_ROW(fmt::format("pps_extension_data_flag[{}]",index), 1);
            index ++;
        }
    }
    rbsp_trailing_bits(pitem,datasource);
    return ret;
}

mdp_video_header * mdp_parse_hvcc(std::shared_ptr<IDataSource> datasource) {
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
                parse_hvc_vps(header, item, datasource->readBytesStream(nalUnitLength));
            } else if (NAL_unit_type == 33) { //sps
                std::string name = fmt::format("sps[{}]", sps_index++);
                mdp_header_item *item = APPEND_ENABLE_ROW(name.c_str());
                enable_or_disable_cell(item,1);
                parse_hvc_sps(header, item, datasource->readBytesStream(nalUnitLength));
            } else if (NAL_unit_type == 34) { //pps
                std::string name = fmt::format("pps[{}]", pps_index++);
                mdp_header_item *item = APPEND_ENABLE_ROW(name.c_str());
                enable_or_disable_cell(item,1);
                parse_hvc_pps(header, item, datasource->readBytesStream(nalUnitLength));
            }
            datasource->seekBytes(next, SEEK_SET);
        }
    }
    return header;
}

char ** mdp_parse_h264_sei(std::shared_ptr<IDataSource> datasource) {
    // int payloadType = 0;
    // int64_t hadReadBytes = 0;
    char* seis[100] = {0};
    // int current_size = 0;
    // MDPDataSourceContext *dctx = NULL;
    // do {
    //     payloadType = 0;
    //     while (mdp_db_r8(buffer) == 0xff) {
    //         payloadType += 255;
    //     }
    //     int last_payload_type_byte = mdp_db_r8(buffer);
    //     payloadType += last_payload_type_byte;
    //     int payloadSize = 0;
    //     while (mdp_db_r8(buffer) == 0xff) {
    //         payloadSize += 255;
    //     }
    //     int last_payload_size_byte = mdp_db_r8(buffer);
    //     payloadSize += last_payload_size_byte;
    //     if(payloadType == 5) { //SEI_TYPE_USER_DATA_UNREGISTERED = 5,
    //         if(payloadSize < 17) {
    //             // MLOGME("sei info length must larger 16!\n");
    //         } else {
    //             mdp_data_buffer_skip(dctx,buffer,16);
    //             char *sei = av_strdup(mdp_data_buffer_read_bytes(dctx,buffer,payloadSize-17));
    //             seis[current_size++] = sei;
    //         }


    //     }
    //     tmp_data+=payloadSize;
    //     hadReadBytes = (tmp_data- data)*8;
    // }while(more_rbsp_data(data,hadReadBytes,size * 8));
    return seis;
}
