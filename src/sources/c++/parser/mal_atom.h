#pragma once
#include "mal_idatasource.h"
#include "../datasource/mal_local_datasource.h"
#include <vector>
#include <sstream>
#include <iomanip>
#include <functional>
#include "../deps/fast_any/any.h"
#include "../../utils/mal_string.hpp"
extern "C" {
#include "../c_ffi/mdp_type_def.h"
#include "libavcodec/avcodec.h"
}
namespace mal
{
enum class MALMediaType {
    none,
    video,
    audio
};
enum class MALCodecType {
    none,
    h264,
    h265
};


class MALAVCSlice {
public:
    virtual ~MALAVCSlice() {
        
    }
};
class MALNal {
public:
    int nal_unit_type = 0;
    virtual ~MALNal() {
        
    }
};
class MALHEVCNal: public MALNal {
public:
    int forbidden_zero_bit = 0;
    int nuh_layer_id = 0;
    int nuh_temporal_id_plus1 = 0;
    std::vector<std::shared_ptr<MALAVCSlice>> slices;
    virtual ~MALHEVCNal() {
        
    }
};

class MALHEVCSliceSegmentHeader {
public:
    int first_slice_segment_in_pic_flag = 0;
    int no_output_of_prior_pics_flag = 0;
    int slice_pic_parameter_set_id = 0;
//    int dependent_slice_segment_flag = 0;
    int slice_segment_address = 0;
    int slice_type = 0;
    int slice_pic_order_cnt_lsb = 0;
//    int pic_output_flag = 0;
//    int colour_plane_id = 0;
//    int slice_pic_order_cnt_lsb = 0;
//    int short_term_ref_pic_set_sps_flag = 0;
//    int short_term_ref_pic_set_idx = 0;
//    int num_long_term_sps = 0;
//    int num_long_term_pics = 0;
//    int slice_qp_delta = 0;
//    int slice_cb_qp_offset = 0;
//    int slice_cr_qp_offset = 0;
//    int slice_act_y_qp_offset = 0;
//    int slice_act_cb_qp_offset = 0;
//    int slice_act_cr_qp_offset = 0;
    
};
class MALHEVCSliceSegmentLayerRbsp: public MALHEVCNal {
public:
    MALHEVCSliceSegmentHeader header;
};

class MALAVCNal: public MALNal {
public:
    int nal_ref_idc = 0;// 0可以丢弃，1不能丢弃
    std::vector<std::shared_ptr<MALAVCSlice>> slices;
    virtual ~MALAVCNal() {
        
    }
};
class MALAVCSPS: public MALAVCNal { //nal_unit_type 7
public:
    int profile_idc = 0;
    int level_idc = 0;
    int seq_parameter_set_id = 0;
    int chroma_format_idc = 0;
    int bit_depth_luma_minus8 = 0;
    int bit_depth_chroma_minus8 = 0;
    int log2_max_frame_num_minus4 = 0;
    int pic_order_cnt_type = 0;
    int delta_pic_order_always_zero_flag  = 0;
    int num_ref_frames_in_pic_order_cnt_cycle = 0;
    std::vector<int> offset_for_ref_frame;
    int log2_max_pic_order_cnt_lsb_minus4 = 0;
    int max_num_ref_frames = 0;
    int pic_width_in_mbs_minus1 = 0;
    int pic_height_in_map_units_minus1 = 0;
    int frame_mbs_only_flag = 0;
    int mb_adaptive_frame_field_flag = 0;
    int frame_cropping_flag = 0;
    int separate_colour_plane_flag = 0;
    int offset_for_non_ref_pic = 0;
    int offset_for_top_to_bottom_field = 0;
};
class MALAVCPPS: public MALAVCNal { //nal_unit_type 8
public:
    int pic_parameter_set_id = 0; //当前PPS的ID，供slice RBSP使用
    int seq_parameter_set_id = 0; //当前PPS所属的SPS的ID
    int entropy_coding_mode_flag = 0; //为0时表明采用CAVLC编码，为1时采用CABAC编码
    int bottom_field_pic_order_in_frame_present_flag  = 0; //用于POC计算
    int redundant_pic_cnt_present_flag = 0;
    int weighted_pred_flag = 0;
    int weighted_bipred_idc = 0;
    int deblocking_filter_control_present_flag = 0;
    int num_slice_groups_minus1 = 0;
    int slice_group_map_type = 0;
    int slice_group_change_rate_minus1 = 0;
    int pic_size_in_map_units_minus1 = 0;
};
class MALAVCSliceHeader {
public:
    int first_mb_in_slice = 0;
    int slice_type = 0; //slice_type % 5 ->  0:P 1:B 2:I 3:SP 4:SI 
    int colour_plane_id;
    int pic_parameter_set_id = 0;
    int frame_num = 0;
    int field_pic_flag = 0;
    int bottom_field_flag = 0;
    int idr_pic_id = 0;
    int pic_order_cnt_lsb = 0;
    int delta_pic_order_cnt_bottom = 0;
    int delta_pic_order_cnt[2] = {0};
    int slice_qp_delta = 0;
    int num_ref_idx_active_override_flag = 0;
    int num_ref_idx_l0_active_minus1 = 0;
    int num_ref_idx_l1_active_minus1 = 0;
    int memory_management_control_operation = 0;
};
class MALAVCSliceWithOutPartitioning: public MALAVCNal{ //nal_unit_type 1/5
public:
    MALAVCSliceHeader header;
};
class MALAVCSlicePartitionA: public MALAVCNal { //nal_unit_type 2
    MALAVCSliceHeader header;
};
class MALAVCSlicePartitionB: public MALAVCNal { //nal_unit_type 3
    
};
class MALAVCSlicePartitionC: public MALAVCNal { //nal_unit_type 4
    
};

class MALSEIMessage {
public:
    std::string key;
    int payloadType;
    std::string value;
    int payloadSize;
};

class MALAVCSEI: public MALAVCNal { //nal_unit_type 6
public:
    std::vector<MALSEIMessage> messageList;
};



class MALCheck {
public:
    std::vector<std::string> warning;
    std::vector<std::string> errors;
};
class MALShallowCheck: public MALCheck {
   
};
class MALDeepCheck: public MALCheck {
    
};
class MALVideoConfig {
public:
    int64_t lengthSizeMinusOne = 0;
    int64_t width = 0;
    int64_t height = 0;
    virtual ~MALVideoConfig() {
        
    }
};
class MALAVCC: public MALVideoConfig {
public:
    std::vector<std::shared_ptr<MALAVCSPS>> spsList;
    std::vector<std::shared_ptr<MALAVCPPS>> ppsList;
};

enum  MALHEVCNALTYPE {
    TRAIL_N = 0,
    TRAIL_R = 1,
    
    TSA_N = 2,
    TSA_R = 3,
    
    STSA_N = 4,
    STSA_R = 5,
    
    RADL_N = 6,
    RADL_R = 7,
    
    RASL_N = 8,
    RASL_R = 9,
    
    RSV_VCL_N10 = 10,
    RSV_VCL_N12 = 12,
    RSV_VCL_N14 = 14,
    
    RSV_VCL_R11 = 11,
    RSV_VCL_R13 = 13,
    RSV_VCL_R15 = 15,
    
    BLA_W_LP = 16,
    BLA_W_RADL = 17,
    BLA_N_LP = 18,
    
    IDR_W_RADL = 19,
    IDR_N_LP = 20,
    
    CRA_NUT = 21,
    
    RSV_IRAP_VCL22 = 22,
    RSV_IRAP_VCL23 = 23,
    RSV_VCL24 = 24,
    RSV_VCL25 = 25,
    RSV_VCL26 = 26,
    RSV_VCL27 = 27,
    RSV_VCL28 = 28,
    RSV_VCL29 = 29,
    RSV_VCL30 = 30,
    RSV_VCL31 = 31,
    
    VPS_NUT = 32,
    
    SPS_NUT = 33,
    
    PPS_NUT = 34,
    
    AUD_NUT = 35,
    
    EOS_NUT = 36,
    
    EOB_NUT = 37,
    
    FD_NUT = 38,
    
    PREFIX_SEI_NUT = 39,
    SUFFIX_SEI_NUT = 40
};

class MALHEVCVPS: public MALHEVCNal {
public:
    int video_parameter_set_id = 0;
    int LayerIdxInVps[64] = {0};
    int poc_lsb_not_present_flag[64] = {0};
//    int profile_idc = 0;
//    int level_idc = 0;
//    int seq_parameter_set_id = 0;
//    int chroma_format_idc = 0;
//    int bit_depth_luma_minus8 = 0;
//    int bit_depth_chroma_minus8 = 0;
//    int log2_max_frame_num_minus4 = 0;
//    int pic_order_cnt_type = 0;
//    int delta_pic_order_always_zero_flag  = 0;
//    int num_ref_frames_in_pic_order_cnt_cycle = 0;
//    std::vector<int> offset_for_ref_frame;
//    int log2_max_pic_order_cnt_lsb_minus4 = 0;
//    int max_num_ref_frames = 0;
//    int pic_width_in_mbs_minus1 = 0;
//    int pic_height_in_map_units_minus1 = 0;
//    int frame_mbs_only_flag = 0;
//    int mb_adaptive_frame_field_flag = 0;
//    int frame_cropping_flag = 0;
//    int separate_colour_plane_flag = 0;
//    int offset_for_non_ref_pic = 0;
//    int offset_for_top_to_bottom_field = 0;
};
class MALHEVCSPS: public MALHEVCNal {
public:
    int sps_video_parameter_set_id = 0;
    int seq_parameter_set_id = 0;
    int pic_width_in_luma_samples = 0;
    int pic_height_in_luma_samples = 0;
    int log2_max_pic_order_cnt_lsb_minus4 = 0;
    int log2_min_luma_coding_block_size_minus3 = 0;
    int log2_diff_max_min_luma_coding_block_size = 0;
    int separate_colour_plane_flag = 0;
    int num_short_term_ref_pic_sets = 0;
};
class MALHEVCPPS: public MALHEVCNal { //nal_unit_type 8
public:
    int pic_parameter_set_id = 0; //当前PPS的ID，供slice RBSP使用
    int seq_parameter_set_id = 0; //当前PPS所属的SPS的ID
//    int entropy_coding_mode_flag = 0; //为0时表明采用CAVLC编码，为1时采用CABAC编码
//    int bottom_field_pic_order_in_frame_present_flag  = 0; //用于POC计算
//    int redundant_pic_cnt_present_flag = 0;
//    int weighted_pred_flag = 0;
//    int weighted_bipred_idc = 0;
//    int deblocking_filter_control_present_flag = 0;
//    int num_slice_groups_minus1 = 0;
//    int slice_group_map_type = 0;
//    int slice_group_change_rate_minus1 = 0;
//    int pic_size_in_map_units_minus1 = 0;
    int dependent_slice_segments_enabled_flag  = 0;
    int num_extra_slice_header_bits = 0;
    int output_flag_present_flag = 0;
};
class MALHEVCSEI: public MALHEVCNal { //nal_unit_type 6
public:
    std::vector<MALSEIMessage> messageList;
};
class MALHVCC: public MALVideoConfig {
public:
    std::vector<std::shared_ptr<MALHEVCVPS>> vpsList;
    std::vector<std::shared_ptr<MALHEVCSPS>> spsList;
    std::vector<std::shared_ptr<MALHEVCPPS>> ppsList;
};
class MALStream {
public:
    MALMediaType mediaType = MALMediaType::none;
    MALCodecType codecType = MALCodecType::none;
    int index = -1;
    int64_t timescale = 1;
    std::vector<std::shared_ptr<MALVideoConfig>> videoConfig;
    std::shared_ptr<MALVideoConfig> currentConfig() {
        if (videoConfig.empty()) return nullptr;
        return videoConfig[videoConfig.size() - 1];
    }
    virtual ~MALStream() {
        
    }
};
class MALFormatContext {
public:
    std::vector<std::shared_ptr<MALStream>> streams = {};
    int newStream() {
        auto stream = std::make_shared<MALStream>();
        return addStream(stream);
    }
    int addStream(std::shared_ptr<MALStream> stream) {
        streams.push_back(stream);
        streams[index_]->index = index_;
        index_++;
        return 0;
    }
    std::shared_ptr<MALStream> currentStream() {
        if (index_ == 0) return nullptr;
        return streams[index_-1];
    }
    std::shared_ptr<MALStream> currentStream(int index) {
        if (index >= index_) return nullptr;
        return streams[index];
    }
    MALShallowCheck shallowCheck;
    MALDeepCheck deepCheck;
    std::shared_ptr<IDataSource> datasource = nullptr;
    void addShallowWarning(std::string msg) {
        if (std::find(shallowCheck.warning.begin(), shallowCheck.warning.end(), msg) == shallowCheck.warning.end()) {
            shallowCheck.warning.push_back(msg);
        }
    }
private:
    int index_ = 0;
};
class MALMP4Stream: public MALStream {
public:
    std::vector<int64_t> stss; //关键帧
    std::vector<std::tuple<int64_t,int64_t,int64_t>> stsc; //first_chunk,samples_per_chunk,sample_description_index
    std::vector<int64_t> stsz; //每帧大小size
    std::vector<int64_t> stco; //每个chunk offset 存放每个chunck相对于文件的位置
    std::vector<std::tuple<uint64_t,uint64_t>> stts;//dts
    std::vector<std::tuple<uint64_t,uint64_t>> ctts;//dts
    std::vector<std::tuple<uint64_t,uint64_t,uint64_t,uint64_t>> elst;//elst
};

enum MALPacketFlag {
    none = -1,
    idr,
    i,
    p,
    b
};
inline std::string mal_convert_pkt_flag_to_str(MALPacketFlag flag) {
    if (flag == MALPacketFlag::none) {
        return "unknown";
    } else if (flag == MALPacketFlag::idr) {
        return "IDR";
    } else if (flag == MALPacketFlag::i) {
        return "I";
    } else if (flag == MALPacketFlag::p) {
        return "P";
    } else if (flag == MALPacketFlag::b) {
        return "B";
    }
    return "unknown";
}

class MALPacket {
public:
    explicit MALPacket(AVPacket *pkt): pkt_(pkt) {
        
    }
    explicit MALPacket(uint8_t *data, uint64_t size): data_(data),size_(size) {
        datasource = std::make_shared<LocalDataSource>(data,size);
        datasource->open();
    }
    std::vector<std::shared_ptr<MALNal>> nals;
    uint8_t *data() {
        return pkt_ == NULL ? data_ : pkt_->data;
    }
    int64_t size() {
        return pkt_ == NULL ? size_ : pkt_->size;
    }
    int64_t pos = 0;
    int64_t pts= 0;
    double pts_time = 0.0;
    int64_t dts = 0;
    int64_t number = 0; //序号
    int sample_description_index = 0;
    int index = -1;
    mdp_video_header *nal_header = nullptr;
    int avc_PicOrderCntMsb = 0;
    int avc_PicOrderCntLsb = 0;
    int avc_TopFieldOrderCnt = 0;
    int avc_BottomFieldOrderCnt = 0;
    int avc_FrameNumOffset = 0;
    int avc_IdrPicFlag = 0;
    int nal_ref_idc = 0;
    int poc = 0;
    MALPacketFlag flag = MALPacketFlag::none;
    std::shared_ptr<IDataSource> datasource = nullptr;
//    int POC = 0;
    MALAVCSliceHeader *sliceHeader = nullptr;
    ~MALPacket() {
        av_packet_free(&pkt_);
    }
private:
    AVPacket *pkt_ = nullptr;
    uint8_t *data_ = nullptr;
    uint64_t size_ = 0;
    
};
class MDPAtomField {
public:
    MDPAtomField() {}
    MDPAtomField(std::string _name,int64_t _pos, int _bits, std::string _extraVal="") {
        name = _name;
        pos = _pos;
        bits = _bits;
        extraVal = _extraVal;
    }
    MDPAtomField(std::string _name, int _bits, MDPFieldDisplayType type = MDPFieldDisplayType_int64 , int _big =1, std::function<fast_any::any(fast_any::any)> _callback = NULL) {
        name = _name;
        bits = _bits;
        display_type = type;
        callback = _callback;
        big = _big;
    }
    template <typename ValueType>
    void putValue(ValueType newValue, MDPFieldDisplayType type) {
        val.emplace<ValueType>(newValue);
        display_type = type;
    }
    
    fast_any::any val;
    std::string name;
    std::string extraVal;
    int64_t pos;
    int bits;
    MDPFieldDisplayType display_type = MDPFieldDisplayType_int64; //显示int
    std::function<fast_any::any(fast_any::any)> callback;
    int big = 1;
};

class MDPAtom {
public:
    std::string name = "";
    int64_t pos = 0;
    int64_t size = 0;
    int headerSize = 0;
    std::shared_ptr<IDataSource> dataSource;
    std::vector<std::shared_ptr<MDPAtom>> childBoxs;
    std::vector<std::shared_ptr<MDPAtomField>> fields;
   
    fast_any::any writeField(MDPAtomField field, int big = 1, std::string _extraVal="") {
        return writeField(field.name, field.bits, field.display_type,big, _extraVal,field.callback);
    }
    fast_any::any writeField(std::string _name, int64_t _bits, MDPFieldDisplayType type = MDPFieldDisplayType_int64,int big = 1, std::string _extraVal="" , std::function<fast_any::any(fast_any::any)> callback = NULL, fast_any::any *direct_val = nullptr) {
        if (_bits > 0) {
            _name = _name + "(" + std::to_string(_bits) + "bits" + ")";
        }
        if (_bits > 64 && type == MDPFieldDisplayType_int64) {
            type = MDPFieldDisplayType_hex;
        }
        auto filed = std::make_shared<MDPAtomField>(_name,dataSource->currentBitsPosition(),_bits,_extraVal);
        if (type == MDPFieldDisplayType_int64) {
            int64_t val = dataSource->readBitsInt64(_bits,0, big);
            if (_bits == 0 && direct_val != nullptr && direct_val->has_value()) {
                if (direct_val->as<uint64_t>()) {
                    val = *(direct_val->as<uint64_t>());
                } else if (direct_val->as<int64_t>()) {
                    val = *(direct_val->as<int64_t>());
                } else if (direct_val->as<int>()) {
                    val = *(direct_val->as<int>());
                } else if (direct_val->as<int32_t>()) {
                    val = *(direct_val->as<int32_t>());
                }  else if (direct_val->as<uint32_t>()) {
                    val = *(direct_val->as<uint32_t>());
                }
            }
            filed->putValue<uint64_t>(val,MDPFieldDisplayType_int64);
        } else if (type == MDPFieldDisplayType_string) {
            std::string val = dataSource->readBytesString(_bits/8);
            if (_bits == 0 && direct_val != nullptr && direct_val->has_value()) {
                val = *(direct_val->as<std::string>());
            }
            filed->putValue<std::string>(val,MDPFieldDisplayType_string);
        } else if (type == MDPFieldDisplayType_fixed_16X16_float) {
            unsigned char *data = dataSource->readBytesRaw(_bits/8);
            double val = mdp_strconvet_to_fixed_16x16_point((char *)data);
            if (_bits == 0 && direct_val != nullptr && direct_val->has_value()) {
                val = *(direct_val->as<double>());
            }
            filed->putValue<double>(val,MDPFieldDisplayType_double);
        } else if (type == MDPFieldDisplayType_hex) {
            int bytes = _bits/8;
            std::string val = "0x";
            for (size_t i = 0; i < bytes; i++) {
                val += mal_int_to_hex_string(dataSource->readBytesInt64(1)) + " ";
            }
            if (_bits == 0 && direct_val != nullptr && direct_val->has_value()) {
                val = *(direct_val->as<std::string>());
            }
            filed->putValue<std::string>(val,MDPFieldDisplayType_string);
        } else if (type == MDPFieldDisplayType_double) {
            double val = 0;
            if (_bits > 0) {
                int64_t value = dataSource->readBitsInt64(_bits,0,big);
                union tmp {
                    int64_t a;
                    double b;
                };
                union tmp t;
                t.a = value;
                val = t.b;
            } else {
                val = *(direct_val->as<double>());
            }
            
            filed->putValue<double>(val,MDPFieldDisplayType_double);
        } else if (type == MDPFieldDisplayType_separator){
            filed->putValue<std::string>("↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓",MDPFieldDisplayType_string);
        }  else {
            dataSource->skipBits(_bits);
        }
        if (callback) {
            // fast_any::any any;
            // any.emplace<int64_t>(val);
            // val = *(callback(any).as<int64_t>());
            if (filed->val.has_value()) {
                filed->val = callback(filed->val);
            }
        }
        fields.push_back(filed);
        return filed->val;
    }
    template <class T = uint64_t>
    T writeField(std::string _name, int64_t _bits, MDPFieldDisplayType type = MDPFieldDisplayType_int64, int big = 1, std::string _extraVal="" , std::function<fast_any::any(fast_any::any)> callback = NULL) {
        fast_any::any val = writeField(_name,_bits,type,big,_extraVal,callback);
        return *(val.as<T>());
    }
    template <typename T = uint64_t>
    void writeValueField(std::string _name, T val, std::string _extraVal="") {
        fast_any::any obj;
        MDPFieldDisplayType type = MDPFieldDisplayType_int64;
        if(std::is_same<T,int>::value || std::is_same<T,uint64_t>::value) {
            type = MDPFieldDisplayType_int64;
        } else if(std::is_same<T,std::string>::value) {
            type = MDPFieldDisplayType_string;
        } else if(std::is_same<T,double>::value) {
            type = MDPFieldDisplayType_double;
        }
        obj.emplace<T>(val);
        writeField(_name, 0,type,1,_extraVal,nullptr, &obj);
    }
    
};

} // namespace mdp

