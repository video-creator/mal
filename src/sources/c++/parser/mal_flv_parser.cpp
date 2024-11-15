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
    return 0;
}
int FLVPParser::processAudioTag_(std::shared_ptr<MDPAtom> atom) {
    return 0;
}
int FLVPParser::processScriptTag_(std::shared_ptr<MDPAtom> atom) {
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

