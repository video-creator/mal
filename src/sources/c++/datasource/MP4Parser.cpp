#include "MP4Parser.h"
#include <vector>
#include <fstream>
#include <unordered_set>
#include "../deps/spdlog/fmt/fmt.h"
extern "C" {
    #include "../../utils/cJSON.h"
} 
using namespace mdp;
MP4Parser::MP4Parser(const std::shared_ptr<IDataSource>& datasource) : IParser(datasource) {

}
MP4Parser::MP4Parser(const std::string &path, Type type) : IParser(path, type)
{
    _parseTableEntry.push_back(std::make_tuple("ftyp", [=](std::shared_ptr<MDPAtom> atom)
    {
        atom->writeField("major_brand",4 * 8,MDPAtomField::DisplayType::vstring);
        atom->writeField("minor_version",4 * 8);
        int64_t compatible_brands_size =  atom->size - atom->dataSource->currentBytesPosition();
        std::string *compatible_brands = atom->writeField("compatible_brands",compatible_brands_size * 8,MDPAtomField::DisplayType::vstring).as<std::string>();
        if (compatible_brands) {
        } 
    }));
    _parseTableEntry.push_back(std::make_tuple("mvhd", [=](std::shared_ptr<MDPAtom> atom)
    {
        int64_t version = *(atom->writeField("version",1 * 8).as<uint64_t>());
        atom->writeField("flags",3 * 8);
        int creation_time_size = 4;
        int modification_time_size = 4;
        int timescale_size = 4;
        int duration = 4;
        if(version == 1) {
            creation_time_size = 8;
            modification_time_size = 8;
            timescale_size = 4;
            duration = 8;
        }
        int filedSize = creation_time_size;
        atom->writeField("creation_time",filedSize * 8);
        filedSize = modification_time_size;
        atom->writeField("modification_time",filedSize * 8);

        filedSize = timescale_size;
        atom->writeField("timescale",filedSize * 8);
        filedSize = duration;
        atom->writeField("duration",filedSize * 8);
        filedSize = 4;
        atom->writeField("rate",filedSize * 8);

        filedSize = 2;
        atom->writeField("volume",filedSize * 8);
        
        filedSize = 2;
        atom->writeField("reserved",filedSize * 8);

        filedSize = 8;
        atom->writeField("reserved",filedSize * 8);

        filedSize = 36;
        atom->writeField("matrix",filedSize * 8,mdp::MDPAtomField::DisplayType::vhex); //TODO

        filedSize = 24;
        atom->writeField("pre_defined",filedSize * 8);

        filedSize = 4;
        atom->writeField("next_track_ID",filedSize * 8);
    }));
    _parseTableEntry.push_back(std::make_tuple("tkhd", [=](std::shared_ptr<MDPAtom> atom) {

        int version = *(atom->writeField("version",1*8).as<uint64_t>());
        atom->writeField("flags",3 * 8);
        int creation_time_size = 4;
        int modification_time_size = 4;
        int track_ID_size = 4;
        int reserved1_size = 4;
        int duration_size = 4;
        if(version == 1) {
            creation_time_size = 8;
            modification_time_size = 8;
            track_ID_size = 4;
            reserved1_size = 4;
            duration_size = 8;
        }
        std::vector<MDPAtomField> fields = {
            MDPAtomField("creation_time",creation_time_size * 8),
            MDPAtomField("modification_time",modification_time_size * 8),
            MDPAtomField("track_ID",track_ID_size * 8),
            MDPAtomField("reserved1",reserved1_size * 8),
            MDPAtomField("duration",duration_size * 8),
            MDPAtomField("reserved2",8 * 8),
            MDPAtomField("layer",2 * 8),
            MDPAtomField("alternate_group",2 * 8),
            MDPAtomField("volume",2 * 8),
            MDPAtomField("reserved3",2 * 8),
            MDPAtomField("matrix",36 * 8,mdp::MDPAtomField::DisplayType::vhex),
            MDPAtomField("width",4 * 8,mdp::MDPAtomField::DisplayType::vfixed_16X16_float),
            MDPAtomField("height",4 * 8,mdp::MDPAtomField::DisplayType::vfixed_16X16_float)
        };
        for (auto& el : fields) {
            atom->writeField(el);
        }
    }));
/**
tref box可以描述两track之间关系。
比如：一个MP4文件中有三条video track，ID分别是2、3、4，以及三条audio track，ID分别是6、7、8。
在播放track 2视频时到底应该采用6、7、8哪条音频与其配套播放？这时候就需要在track 2与6的tref box中指定一下，将2与6两条track绑定起来。
**/
    _parseTableEntry.push_back(std::make_tuple("moov|trak|tref|hint|font|vdep|vplx|subt|trgr|msrc|mdia|minf|udta|edts", [=](std::shared_ptr<MDPAtom> atom) {
        this->_parseChildAtom(atom); 
    }));
    //full box
    _parseTableEntry.push_back(std::make_tuple("meta", [=](std::shared_ptr<MDPAtom> atom) {
        atom->writeField("version",1 * 8);
        atom->writeField("flags", 3* 8);
        this->_parseChildAtom(atom); 
    }));
    _parseTableEntry.push_back(std::make_tuple("mdhd", [=](std::shared_ptr<MDPAtom> atom) {
        int version = *(atom->writeField("version",1 * 8).as<uint64_t>());
        atom->writeField("flags", 3* 8);
        int creation_time_size = 4;
        int modification_time_size = 4;
        int timescale_size = 4;
        int duration_size = 4;
        if(version == 1) {
            creation_time_size = 8;
            modification_time_size = 8;
            timescale_size = 4;
            duration_size = 8;
        }
        std::vector<MDPAtomField> fields = {
            MDPAtomField("creation_time",creation_time_size * 8),
            MDPAtomField("modification_time",modification_time_size * 8),
            MDPAtomField("timescale",timescale_size * 8),
            MDPAtomField("duration",duration_size * 8),
            MDPAtomField("pad_language",2 * 8),
            MDPAtomField("pre_defined",2 * 8),
        };
        for (auto& el : fields) {
            atom->writeField(el);
        }
        
    }));
    _parseTableEntry.push_back(std::make_tuple("hdlr", [=](std::shared_ptr<MDPAtom> atom) {
        std::vector<MDPAtomField> fields = {
            MDPAtomField("version",1 * 8),
            MDPAtomField("flags",3 * 8),
            MDPAtomField("pre_defined",4 * 8),
            MDPAtomField("handler_type",4 * 8, mdp::MDPAtomField::DisplayType::vstring),
            MDPAtomField("reserved",12 * 8, mdp::MDPAtomField::DisplayType::vhex),
        };
        for (auto& el : fields) {
            atom->writeField(el);
        }
        atom->writeField("name", (atom->size - atom->dataSource->currentBytesPosition()) * 8, mdp::MDPAtomField::DisplayType::vstring);
    }));
    _parseTableEntry.push_back(std::make_tuple("elng", [=](std::shared_ptr<MDPAtom> atom) {
        atom->writeField("version",1 * 8);
        atom->writeField("flags",3 * 8);
        atom->writeField("extended_language", (atom->size - atom->dataSource->currentBytesPosition()) * 8, mdp::MDPAtomField::DisplayType::vstring);
    }));
    _parseTableEntry.push_back(std::make_tuple("stbl", [=](std::shared_ptr<MDPAtom> atom) {
        this->_parseChildAtom(atom); 
    }));
    _parseTableEntry.push_back(std::make_tuple("stsd", [=](std::shared_ptr<MDPAtom> atom) {
        atom->writeField("version",1 * 8);
        atom->writeField("flags",3 * 8);
        unsigned char* entry_count_cr = atom->dataSource->readBytesRaw(4);
        int entry_count = (int) (((entry_count_cr[0] & 0xFF) << 24)
                | ((entry_count_cr[1] & 0xFF) << 16)
                | ((entry_count_cr[2] & 0xFF) << 8)
                | (entry_count_cr[3] & 0xFF));
        for(int i = 1; i<= entry_count; i++) {
            this->_parseChildAtom(atom); 
        }
        
    }));
    _parseTableEntry.push_back(std::make_tuple("mp4a", [=](std::shared_ptr<MDPAtom> atom) {
         std::vector<MDPAtomField> fields = {
            MDPAtomField("reserved",6 * 8),
            MDPAtomField("data_reference_index",2 * 8),
            MDPAtomField("version",2 * 8),
            MDPAtomField("reserved",6 * 8),
            MDPAtomField("channelcount",2 * 8),
            MDPAtomField("samplesize",2 * 8),
            MDPAtomField("pre_defined",2 * 8),
            MDPAtomField("reserved",2 * 8),
        };
        for (auto& el : fields) {
            atom->writeField(el);
        }
        atom->writeField("samplerate",4 * 8,MDPAtomField::DisplayType::vint64,">>16",[=](fast_any::any val){
            uint64_t ori = *(val.as<uint64_t>());
            ori = (ori >> 16);
            val.emplace<uint64_t>(ori);
            return val;
        });
        this->_parseChildAtom(atom); 
    }));
    _parseTableEntry.push_back(std::make_tuple("avc1|avc2", [=](std::shared_ptr<MDPAtom> atom) {
        std::vector<MDPAtomField> fields = {
            MDPAtomField("reserved",6 * 8),
            MDPAtomField("data_reference_index",2 * 8),
            MDPAtomField("pre_defined",2 * 8),
            MDPAtomField("reserved",2 * 8),
            MDPAtomField("pre_defined",12 * 8),
            MDPAtomField("width",2 * 8),
            MDPAtomField("height",2 * 8),
            MDPAtomField("horizresolution",4 * 8),
            MDPAtomField("vertresolution",4 * 8),
            MDPAtomField("reserved",4 * 8),
            MDPAtomField("frame_count",2 * 8),
            MDPAtomField("compressorname",32 * 8,MDPAtomField::DisplayType::vstring),
            MDPAtomField("depth",2 * 8),
            MDPAtomField("pre_defined",2 * 8),
            
        };
        for (auto& el : fields) {
            atom->writeField(el);
        }
        this->_parseChildAtom(atom); 
    }));
    _parseTableEntry.push_back(std::make_tuple("avcC", [=](std::shared_ptr<MDPAtom> atom) {
        int numOfSequenceParameterSets = 0;
        std::vector<MDPAtomField> fields = {
            MDPAtomField("configurationVersion",1 * 8),
            MDPAtomField("AVCProfileIndication",1 * 8),
            MDPAtomField("profile_compatibility",1 * 8),
            MDPAtomField("AVCLevelIndication",1 * 8),

            MDPAtomField("reserved",6),
            MDPAtomField("lengthSizeMinusOne",2),

            MDPAtomField("reserved",3),
            MDPAtomField("numOfSequenceParameterSets",5,mdp::MDPAtomField::DisplayType::vint64,[=,&numOfSequenceParameterSets](fast_any::any val){
                uint64_t ori = *(val.as<uint64_t>());
                numOfSequenceParameterSets = ori;
                return val;
            }),
        };
        for (auto& el : fields) {
            atom->writeField(el);
        }
        for (int i = 0; i < numOfSequenceParameterSets; i++) {
            uint64_t sequenceParameterSetLength = *(atom->writeField("sequenceParameterSetLength",2 * 8).as<uint64_t>());
            atom->dataSource->skipBytes(sequenceParameterSetLength);
        }
        uint64_t numOfPictureParameterSets = *(atom->writeField("numOfPictureParameterSets",1 * 8).as<uint64_t>());
        for (int i = 0; i < numOfPictureParameterSets; i++) {
            uint64_t pictureParameterSetLength = *(atom->writeField("pictureParameterSetLength",2 * 8).as<uint64_t>());
            atom->dataSource->skipBytes(pictureParameterSetLength);
        }
        this->_parseChildAtom(atom); 
    }));
    _parseTableEntry.push_back(std::make_tuple("avc1|avc2", [=](std::shared_ptr<MDPAtom> atom) {
        std::vector<MDPAtomField> fields = {
            MDPAtomField("reserved",6 * 8),
            MDPAtomField("data_reference_index",2 * 8),
            MDPAtomField("pre_defined",2 * 8),
            MDPAtomField("reserved",2 * 8),
            MDPAtomField("pre_defined",12 * 8),
            MDPAtomField("width",2 * 8),
            MDPAtomField("height",2 * 8),
            MDPAtomField("horizresolution",4 * 8),
            MDPAtomField("vertresolution",4 * 8),
            MDPAtomField("reserved",4 * 8),
            MDPAtomField("frame_count",2 * 8),
            MDPAtomField("compressorname",32 * 8,MDPAtomField::DisplayType::vstring),
            MDPAtomField("depth",2 * 8),
            MDPAtomField("pre_defined",2 * 8),
            
        };
        for (auto& el : fields) {
            atom->writeField(el);
        }
        this->_parseChildAtom(atom); 
    }));
    _parseTableEntry.push_back(std::make_tuple("elst", [=](std::shared_ptr<MDPAtom> atom) {
        int version = *(atom->writeField("version",1 * 8).as<uint64_t>());
        atom->writeField("flags", 3* 8);
        int entry_count = *(atom->writeField("entry_count",4 * 8).as<uint64_t>());
        int segment_duration_size = 4;
        int media_time_size = 4;
        if(version == 1) {
            segment_duration_size = 8;
            media_time_size = 8;
        }
        for(int i = 1; i<= entry_count; i++) {
            atom->writeField("segment_duration",segment_duration_size * 8);
            atom->writeField("media_time",media_time_size * 8);
            atom->writeField("media_rate_integer",2 * 8);
            atom->writeField("media_rate_fraction",2 * 8);
        }
    }));
    _parseTableEntry.push_back(std::make_tuple("stts", [=](std::shared_ptr<MDPAtom> atom) {
        atom->writeField("version",1 * 8);
        atom->writeField("flags", 3* 8);
        int entry_count = *(atom->writeField("entry_count",4 * 8).as<uint64_t>());
        for(int i = 0; i< entry_count; i++) {
            atom->writeField(fmt::format("sample_count[{}]",i),4 * 8);
            atom->writeField(fmt::format("sample_delta[{}]",i),4 * 8);
        }
    }));
}
bool MP4Parser::supportFormat() {
    if (_datasource->totalSize() < 8) return false;
    int64_t pos = _datasource->currentBytesPosition();
    _datasource->seekBytes(4,SEEK_CUR);
    int type = _datasource->readBytesInt64(4);
    _datasource->seekBytes(pos,SEEK_SET); 
    if (type == mdp_strconvet_to_int("ftype",4,1)) {
        return true;
    }
    
    return false;
}
static cJSON * dumpBox(std::vector<std::shared_ptr<mdp::MDPAtom>> atoms) {
    cJSON *array = cJSON_CreateArray();
    for(auto& atom : atoms)
    {
        cJSON *obj = cJSON_CreateObject();
        cJSON *attr_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(attr_obj, "size", atom->size);
        cJSON_AddNumberToObject(attr_obj, "pos", atom->pos);
        cJSON_AddItemToObject(obj, atom->name.c_str(), attr_obj);
        cJSON_AddItemToArray(array, obj);
        for (auto& filed : atom->fields)
        {
            if (filed->display_type == mdp::MDPAtomField::DisplayType::vint64 && filed->val.has_value()) {
                cJSON_AddNumberToObject(attr_obj,filed->name.c_str(), *(filed->val.as<uint64_t>()));
            } else if (filed->display_type == mdp::MDPAtomField::DisplayType::vstring && filed->val.has_value()) {
                cJSON_AddStringToObject(attr_obj,filed->name.c_str(),filed->val.as<std::string>()->c_str());
            } else if (filed->display_type == mdp::MDPAtomField::DisplayType::vdouble && filed->val.has_value()) {
                cJSON_AddNumberToObject(attr_obj,filed->name.c_str(),(*filed->val.as<double>()));
            }
        }
        
        if (atom->childBoxs.size() > 0) {
            cJSON *child_array = dumpBox(atom->childBoxs);
            if (child_array) {
                cJSON_AddItemToObject(attr_obj,"children",child_array);
            }
        }
    }
    return array;
}
std::string MP4Parser::dumpFormats() {
    if (_rootAtom)
    {
        std::vector<std::shared_ptr<MDPAtom>> atoms = {_rootAtom};
        cJSON *json = dumpBox(atoms);
        char *string = cJSON_Print(json);
        std::cout << string << std::endl;
        std::string filename = "/Users/Shared/output.json";

        // 创建一个输出文件流
        std::ofstream outputFile(filename);

        // 检查文件是否成功打开
        if (!outputFile)
        {
            std::cerr << "无法打开文件: " << filename << std::endl;
            return ""; // 返回错误代码
        }
        // 将 JSON 字符串写入文件
        outputFile << string;

        // 关闭文件
        outputFile.close();
    }
    return "";
}

#define rbits_i(datasource,bits) datasource->readBitsInt64(bits)
#define rbytes_i(datasource,bits) datasource->readBytesInt64(bits)
#define rbytes_s(datasource,bits) datasource->readBytesString(bits)
int MP4Parser::_parseAtom() {
    int64_t ret = 0;
    _rootAtom = std::make_shared<MDPAtom>();
    _rootAtom->name = "root";
    _rootAtom->dataSource = _datasource;
    _rootAtom->size = _datasource->totalSize();
    _rootAtom->pos = 0;
    _parseChildAtom(_rootAtom);
    return ret;
}
std::vector<std::string> split(const std::string &str, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0, end = 0;

    while ((end = str.find(delimiter, start)) != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + 1;
    }
    tokens.push_back(str.substr(start)); // Add the last token

    return tokens;
}
std::unordered_set<std::string> splitToSet(const std::string &str, const std::string &delimiter) {
    std::unordered_set<std::string> result;
    size_t start = 0, end = 0;

    while ((end = str.find(delimiter, start)) != std::string::npos) {
        // 提取子字符串
        result.insert(str.substr(start, end - start));
        start = end + delimiter.length(); // 更新起始位置
    }
    // 提取最后一个子字符串
    result.insert(str.substr(start));

    return result;
}
bool contains(const std::unordered_set<std::string>& set, const std::string& str) {
    return set.find(str) != set.end();
}
int MP4Parser::_parseChildAtom(std::shared_ptr<mdp::MDPAtom> parent) {
    auto datasource = parent->dataSource;
    while (!datasource->isEof())
    {
        std::shared_ptr<mdp::MDPAtom> atom = std::make_shared<mdp::MDPAtom>();
        atom->pos = datasource->currentBytesPosition();
        atom->size = rbytes_i(datasource, 4);
        if (atom->size == 0)
        {
            return 0;
        }
        atom->name = rbytes_s(datasource, 4);
        atom->headerSize = 8;
        if (atom->size == 1)
        {
            atom->size = rbytes_i(datasource, 8);
            atom->headerSize += 8;
        }
        int64_t cur = datasource->currentBytesPosition();
        datasource->seekBytes(atom->pos, SEEK_SET);
        atom->dataSource = datasource->readBytesStream(atom->size);
        parent->childBoxs.push_back(atom);
        for (auto& parse : _parseTableEntry) {
            if (contains(splitToSet(std::get<0>(parse),"|"),atom->name)) {
                atom->dataSource->skipBytes(atom->headerSize); //跳过头部
                std::get<1>(parse)(atom);
                break;
            }
        }
    }
    return 0;
}

int MP4Parser::startParse() {
    int ret = 0;
    ret = _parseAtom();
    return 0;
}
void MP4Parser::loadPackets(int size) {

}
