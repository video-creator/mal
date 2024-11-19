//
//  mal_webp_parser.cpp
//  Demo
//
//  Created by wangyaqiang on 2024/11/11.
//

#include "mal_flv_parser.hpp"
#include "../../utils/mal_string.hpp"
#include "../deps/spdlog/spdlog.h"
using namespace mal;
FLVPParser::FLVPParser(const std::shared_ptr<IDataSource>& datasource):IParser(datasource) {
    
}
FLVPParser::FLVPParser(const std::string& path, Type type):IParser(path, type) {
}
int FLVPParser::startParse() {
    int ret = 0;
    ret = _parseAtom();
    return 0;
}

void FLVPParser::loadPackets(int size) {
    
}
bool FLVPParser::supportFormat() {
    if (_datasource->totalSize() < 9) return false;
    if (_datasource->readBytesString(3,true) == "FLV") {
        return true;
    }
    return false;
}
int FLVPParser::processFlvHeader(std::shared_ptr<MDPAtom> atom) {
    std::vector<MDPAtomField> fields = {
        MDPAtomField("Signature", 1 * 8, MDPFieldDisplayType_string),
        MDPAtomField("Signature", 1 * 8, MDPFieldDisplayType_string),
        MDPAtomField("Signature", 1 * 8, MDPFieldDisplayType_string),
        MDPAtomField("Version", 1 * 8),
        MDPAtomField("TypeFlagsReserved", 5),
        MDPAtomField("TypeFlagsAudio", 1),
        MDPAtomField("TypeFlagsReserved", 1),
        MDPAtomField("TypeFlagsVideo", 1),
        MDPAtomField("DataOffset", 4 * 8)
    };
    for(auto el : fields) {
        atom->writeField(el);
    }
    return 0;
}
int FLVPParser::_parseAtom() {
    int64_t ret = 0;
    root_atom_ = std::make_shared<MDPAtom>();
    root_atom_->name = "root";
    root_atom_->dataSource = _datasource;
    root_atom_->size = _datasource->totalSize();
    root_atom_->pos = 0;
    auto header = std::make_shared<MDPAtom>();
    root_atom_->childBoxs.push_back(header);
    header->pos = _datasource->currentBytesPosition();
    header->name = "FLV Header";
    header->size = 9;
    header->dataSource = _datasource->readBytesStream(header->size);
    processFlvHeader(header);
    
    
    auto bodyBox = std::make_shared<MDPAtom>();
    bodyBox->name = "FLV Body";
    bodyBox->pos = _datasource->currentBytesPosition();
    bodyBox->size = _datasource->lastBytes();
    bodyBox->dataSource = _datasource->readBytesStream(bodyBox->size);
    root_atom_->childBoxs.push_back(bodyBox);
    _parseChildAtom(bodyBox);
    return ret;
}
int FLVPParser::processVideoTag_(std::shared_ptr<MDPAtom> atom) {
    auto datasource = atom->dataSource;
    int64_t frameType = rbits_i(4);
    uint8_t IsExHeader = 0;
    if (frameType >> 3) { // enhanced flv 4位第一位表示是否有IsExHeader
        IsExHeader = 1;
    }
    if (IsExHeader) {
        atom->writeValueField<uint64_t>("IsExHeader(1bits)", IsExHeader);
    }
    std::string extraValue = "";
    frameType = frameType & 0x7;
    if (frameType == 0) {
        extraValue = "(reserved)";
    } else if(frameType == 1) {
        extraValue = "(key frame (a seekable frame))";
    } else if(frameType == 2){
        extraValue = "inter frame (a non-seekable frame)";
    } else if(frameType == 3){
        extraValue = "(disposable inter frame (H.263 only))";
    } else if(frameType == 4){
        extraValue = "( generated key frame (reserved for server use only))";
    } else if(frameType == 5){
        extraValue = "(video info/command frame)";
    } else if(frameType == 6){
        extraValue = "(reserved)";
    } else if(frameType == 7){
        extraValue = "(reserved)";
    }
    if (IsExHeader) {
        atom->writeValueField<uint64_t>("FrameType(3bits)", frameType, extraValue);
    } else {
        atom->writeValueField<uint64_t>("FrameType(4bits)", frameType, extraValue);
    }
    
    int64_t codec = -1;
    if (IsExHeader == 0) {
        auto codecID = atom->writeField<uint64_t>("CodecID", 4);
        codec = codecID;
        extraValue = "";
        if(codecID == 0) {
            extraValue = "(Reserved)";
        } else if(codecID == 1) {
            extraValue = "(Reserved)";
        } else if(codecID == 2){
            extraValue = "(Sorenson H.263)";
        } else if(codecID == 3){
            extraValue = "(Screen video)";
        } else if(codecID == 4){
            extraValue = "(On2 VP6)";
        } else if(codecID == 5){
            extraValue = "(On2 VP6 with alpha channel)";
        } else if(codecID == 6){
            extraValue = "(Screen video version 2)";
        } else if(codecID == 7){
            extraValue = "(AVC)";
        } else {
            extraValue = "(Reserved)";
        }
        if (codecID == 7) { //AVC
            auto AVCPacketType = atom->writeField<uint64_t>("AVCPacketType", 8);//MediaParseUtils::readBits(fileData,tmp,8,tmp);
            extraValue = "";
            if(AVCPacketType == 0) {
                extraValue = "(AVC sequence header)";
            } else if(AVCPacketType == 1) {
                extraValue = "(AVC NALU)";
            } else if(AVCPacketType == 2) {
                extraValue = "(AVC end of sequence (lower level NALU sequence ender is not REQUIRED or supported))";
            }
            auto CompositionTime = atom->writeField<uint64_t>("CompositionTime", 3 * 8);
        }
    } else if (IsExHeader == 1) {
        auto PacketType = atom->writeField<uint64_t>("PacketType", 4);
        extraValue = "";
        if(PacketType == 0) {
            extraValue = "(PacketTypeSequenceStart)";
        } else if(PacketType == 1) {
            extraValue = "(PacketTypeCodedFrames)";
        } else if(PacketType == 2){
            extraValue = "(PacketTypeSequenceEnd)";
        } else if(PacketType == 3){
            extraValue = "(PacketTypeCodedFramesX)";
        } else if(PacketType == 4){
            extraValue = "(PacketTypeMetadata)";
        } else if(PacketType == 5){
            extraValue = "(PacketTypeMPEG2TSSequenceStart)";
        } else {
            extraValue = "(Reserved)";
        }
        
        auto FourCC = atom->writeField<std::string>("FourCC", 4 * 8, MDPFieldDisplayType_string);;//MediaParseUtils::readBits(fileData,tmp,32,tmp);
        if (FourCC == "hvc1") {
            if (PacketType == 1) { //PacketTypeCodedFrames
                auto CompositionTime = atom->writeField<uint64_t>("CompositionTime", 3 * 8);
            }
        }
    }
    return 0;
}
int FLVPParser::processAudioTag_(std::shared_ptr<MDPAtom> atom) {
    auto datasource = atom->dataSource;
    uint64_t soundFormat = rbits_i(4);//atom->writeField<uint64_t>("SoundFormat", 4);
    std::string extraValue = "";
    switch (soundFormat) {
        case 0:
            extraValue = "(Linear PCM, platform endian)";
            break;
        case 1:
            extraValue = "(ADPCM)";
            break;
        case 2:
            extraValue = "(MP3)";
            break;
        case 3:
            extraValue = "(Linear PCM, little endian)";
            break;
        case 4:
            extraValue = "(Nellymoser 16-kHz mono)";
            break;
        case 5:
            extraValue = "(Nellymoser 8-kHz mono)";
            break;
        case 6:
            extraValue = "(Nellymoser)";
            break;
        case 7:
            extraValue = "(G.711 A-law logarithmic PCM)";
            break;
        case 8:
            extraValue = "(G.711 mu-law logarithmic PCM)";
            break;
        case 9:
            extraValue = "(reserved)";
            break;
        case 10:
            extraValue = "(AAC)";
            break;
        case 11:
            extraValue = "(Speex)";
            break;
        case 14:
            extraValue = "(MP3 8-Khz)";
            break;
        case 15:
            extraValue = "(Device-specific sound)";
            break;
        default:
            extraValue = "(UNKNOWN)";
    }
    atom->writeValueField<uint64_t>("SoundFormat(4bits)", soundFormat,extraValue);
    extraValue.clear();
    uint64_t soundRate = rbits_i(2);
    extraValue += "(For AAC: always 3)";
    switch (soundRate) {
        case 0:
            extraValue += "(5.5-kHz)";
            break;
        case 1:
            extraValue += "(11-kHz)";
            break;
        case 2:
            extraValue += "(22-kHz)";
            break;
        case 3:
            extraValue += "(44-kHz)";
            break;
        default:
            extraValue += "(UNKNOWN)";
    }
    atom->writeValueField<uint64_t>("SoundRate(2bits)", soundRate,extraValue);
    extraValue.clear();
    uint64_t soundSize = rbits_i(1);
    switch (soundSize) {
        case 0:
            extraValue = "(snd8Bit)";
            break;
        case 1:
            extraValue = "(snd16Bit)";
            break;
        default:
            extraValue = "(UNKNOWN)";
    }
    atom->writeValueField<uint64_t>("SoundSize(1bits)", soundSize,extraValue);
    uint64_t soundType = rbits_i(1);
    extraValue.clear();
    extraValue = "(For AAC: always 1)";
    switch (soundType) {
        case 0:
            extraValue += "(sndMono)";
            break;
        case 1:
            extraValue += "(sndStereo)";
            break;
        default:
            extraValue += "(UNKNOWN)";
    }
    atom->writeValueField<uint64_t>("SoundType(1bits)", soundType,extraValue);
    if (soundFormat == 10) { //aac
        uint64_t AACPacketType = rbits_i(8);
        std::string extraValue =  AACPacketType == 0 ? "AAC sequence header" : "AAC raw";
        atom->writeValueField<uint64_t>("AACPacketType(8bits)", AACPacketType,extraValue);
    }
    return 0;
}
std::string processScriptString(std::shared_ptr<IDataSource> datasource) {
    auto length = datasource->readBitsInt64(16);//MediaParseUtils::readBits(data,offset,16,offset);
    auto value = datasource->readBytesString(length);//MediaParseUtils::readBits(data,offset,8 * length_v,offset);
    return value;
}
std::string processScriptLongString(std::shared_ptr<IDataSource> datasource) {
    auto length = datasource->readBitsInt64(32);//MediaParseUtils::readBits(data,offset,16,offset);
    auto value = datasource->readBytesString(length);//MediaParseUtils::readBits(data,offset,8 * length_v,offset);
    return value;
}
void FLVPParser::processScriptObject_(std::shared_ptr<MDPAtom> atom) {
    auto datasource = atom->dataSource;
    auto type = rbits_i(8);
    switch (type) {
        case 0: //double
            atom->writeField(current_key_, 64, MDPFieldDisplayType_double);
            current_key_ = "";
            break;
        case 1: //boolean
            atom->writeField(current_key_, 8);
            current_key_ = "";
            break;
        case 2: //string
        {
            auto length = datasource->readBitsInt64(16);
            if (current_key_.length() > 0) {
                atom->writeField(current_key_, 8 * length, MDPFieldDisplayType_string);
            } else {
                current_key_ = datasource->readBytesString(length);
            }
        }
          
            break;
        case 3: //object
            if (current_key_.length() > 0) {
                atom->writeField(current_key_, 0,MDPFieldDisplayType_separator);
            }
            current_key_ = processScriptString(datasource);
            processScriptTag_(atom);
            current_key_ = "";
            break;
        case 4: //MovieClip type
            break;
        case 5: //Null type
            break;
        case 6: //Undefined type
            break;
        case 7: //Reference type
            atom->writeField(current_key_, 16);
            break;
        case 8: //ECMA array type //可变数组长度
        {
            if (current_key_.length() > 0) {
                atom->writeField(current_key_, 0,MDPFieldDisplayType_separator);
            }
            auto ECMAArrayLength = datasource->readBitsInt64(32);//这个是可能的长度，需要遍历，没啥用
            while (!datasource->isEof()) {
                int64_t current = datasource->currentBytesPosition();
                int64_t checkEnd = datasource->readBitsInt64(24);
                if (checkEnd == 0x09) break;// SCRIPTDATAVARIABLEEND
                datasource->seekBytes(current,SEEK_SET);
                current_key_ = processScriptString(datasource);
                processScriptObject_(atom);
                current_key_ = "";
            }
        }
            break;
        case 10: //Strict array type //定长
        {
            auto length = datasource->readBitsInt64(32);//定长
            for (int i = 0; i < length; i++){
                processScriptTag_(atom);
                current_key_ = "";
            }
        }
        case 11: //Date type
        {
            auto datetime = atom->writeField(fmt::format("{} datetime", current_key_), 64);
            auto LocalDateTimeOffset = atom->writeField(fmt::format("{} LocalDateTimeOffset", current_key_), 16);
            current_key_ = "";
        }
        case 12: //Long string type
        {
            auto length = datasource->readBitsInt64(32);
            if (current_key_.length() > 0) {
                atom->writeField(current_key_, 8 * length, MDPFieldDisplayType_string);
            } else {
                current_key_ = datasource->readBytesString(length);
            }
        }
            break;
        default:
            break;
    }
}
int FLVPParser::processScriptTag_(std::shared_ptr<MDPAtom> atom) {
    auto datasource = atom->dataSource;
    int64_t length = 0;
    while (!datasource->isEof()) {
        processScriptObject_(atom);
    }
    return 0;
}
int FLVPParser::processTag_(std::shared_ptr<MDPAtom> atom) {
    std::string name = "";
    auto datasource = atom->dataSource;
    int64_t type = atom->writeField<uint64_t>("Type", 1 * 8);
    if (type == 8) {
        name = fmt::format("AudioTag[{}]",audio_tag_index_++);
    } else if (type == 9) {
        name = fmt::format("VideoTag[{}]",video_tag_index_++);
    } else if (type == 18) {
        name = fmt::format("ScriptDataTag[{}]",script_tag_index_++);
    } else {
        name = "UnknownTag";
    }
    atom->name = name;
    std::vector<MDPAtomField> fields = {
        MDPAtomField("DataSize", 3 * 8, MDPFieldDisplayType_string),
        MDPAtomField("Timestamp", 3 * 8, MDPFieldDisplayType_string),
        MDPAtomField("TimestampExtended", 1 * 8, MDPFieldDisplayType_string),
        MDPAtomField("StreamID", 3 * 8),
    };
    int64_t data_size = atom->writeField<uint64_t>("DataSize", 3 * 8); //这里不包括 这个header
    int64_t timestamp = atom->writeField<uint64_t>("Timestamp", 3 * 8,MDPFieldDisplayType_int64,1,"(milliseconds)");
    int64_t timestamp_extended = atom->writeField<uint64_t>("TimestampExtended", 1 * 8,MDPFieldDisplayType_int64,1,"(milliseconds)");
    int64_t stream_id = atom->writeField<uint64_t>("StreamID", 3 * 8,MDPFieldDisplayType_int64,1,"(not stream index,always 0)");
    if (type == 8) {
        processAudioTag_(atom);
    } else if (type == 9) {
        processVideoTag_(atom);
    } else if (type == 18) {
        processScriptTag_(atom);
    } else {
        
    }
    return 0;
}
int FLVPParser::_parseChildAtom(std::shared_ptr<MDPAtom> parent, bool once) {
    auto datasource = parent->dataSource;
    while (!datasource->isEof()) {
        auto presize_atom = std::make_shared<MDPAtom>();
        presize_atom->pos = datasource->currentBytesPosition();
        presize_atom->name = fmt::format("PreviousTagSize[{}]",pre_tag_index_++);
        presize_atom->size = 4;
        datasource->seekBytes(presize_atom->pos, SEEK_SET);
        presize_atom->dataSource = datasource->readBytesStream(presize_atom->size);
        parent->childBoxs.push_back(presize_atom);
        
        if (datasource->isEof()) break;
        auto tag_atom = std::make_shared<MDPAtom>();
        parent->childBoxs.push_back(tag_atom);
        tag_atom->pos = datasource->currentBytesPosition();
        int type = rbits_i(8);
        int64_t data_size = rbits_i(3 * 8);
        tag_atom->size = data_size + (8 + 3 * 8 + 3 * 8 + 1 * 8 + 3 * 8) / 8;
        datasource->seekBytes(tag_atom->pos, SEEK_SET);
        tag_atom->dataSource = datasource->readBytesStream(tag_atom->size);
        processTag_(tag_atom);
        if (once) break;
    }
    return 0;
}

