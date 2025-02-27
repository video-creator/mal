#include "mal_mov_parser.h"
#include "../deps/spdlog/fmt/fmt.h"
#include "../../utils/mal_string.hpp"
#include "mal_codec_parser.h"
#include <fstream>
#include <unordered_set>
#include <vector>
#include <iostream>
extern "C" {
#include "../../utils/cJSON.h"
#include "../../dart/dart_init.h"
}
using namespace mal;
MP4Parser::MP4Parser(const std::shared_ptr<IDataSource> &datasource)
: IParser(datasource) {
    registerParserTableEntry_();
}
MP4Parser::MP4Parser(const std::string &path, Type type) : IParser(path, type) {
    registerParserTableEntry_();
}
void MP4Parser::registerParserTableEntry_() {
    _parseTableEntry.push_back(
                               std::make_tuple("ftyp", [=](std::shared_ptr<MDPAtom> atom) {
                                   atom->writeField("major_brand", 4 * 8,
                                                    MDPFieldDisplayType_string);
                                   atom->writeField("minor_version", 4 * 8);
                                   int64_t compatible_brands_size =
                                   atom->size - atom->dataSource->currentBytesPosition();
                                   std::string compatible_brands = atom->writeField<std::string>(
                                                                                                 "compatible_brands", compatible_brands_size * 8,
                                                                                                 MDPFieldDisplayType_string);
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("mvhd", [=](std::shared_ptr<MDPAtom> atom) {
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   int creation_time_size = 4;
                                   int modification_time_size = 4;
                                   int timescale_size = 4;
                                   int duration = 4;
                                   if (version == 1) {
                                       creation_time_size = 8;
                                       modification_time_size = 8;
                                       timescale_size = 4;
                                       duration = 8;
                                   }
                                   int filedSize = creation_time_size;
                                   atom->writeField("creation_time", filedSize * 8);
                                   filedSize = modification_time_size;
                                   atom->writeField("modification_time", filedSize * 8);
                                   
                                   filedSize = timescale_size;
                                   atom->writeField("timescale", filedSize * 8);
                                   filedSize = duration;
                                   atom->writeField("duration", filedSize * 8);
                                   filedSize = 4;
                                   atom->writeField("rate", filedSize * 8);
                                   
                                   filedSize = 2;
                                   atom->writeField("volume", filedSize * 8);
                                   
                                   filedSize = 2;
                                   atom->writeField("reserved", filedSize * 8);
                                   
                                   filedSize = 8;
                                   atom->writeField("reserved", filedSize * 8);
                                   
                                   filedSize = 36;
                                   atom->writeField("matrix", filedSize * 8,
                                                    MDPFieldDisplayType_hex); // TODO
                                   
                                   filedSize = 24;
                                   atom->writeField("pre_defined", filedSize * 8);
                                   
                                   filedSize = 4;
                                   atom->writeField("next_track_ID", filedSize * 8);
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("tkhd", [=](std::shared_ptr<MDPAtom> atom) {
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   int creation_time_size = 4;
                                   int modification_time_size = 4;
                                   int track_ID_size = 4;
                                   int reserved1_size = 4;
                                   int duration_size = 4;
                                   if (version == 1) {
                                       creation_time_size = 8;
                                       modification_time_size = 8;
                                       track_ID_size = 4;
                                       reserved1_size = 4;
                                       duration_size = 8;
                                   }
                                   std::vector<MDPAtomField> fields = {
                                       MDPAtomField("creation_time", creation_time_size * 8),
                                       MDPAtomField("modification_time", modification_time_size * 8),
                                       MDPAtomField("track_ID", track_ID_size * 8),
                                       MDPAtomField("reserved1", reserved1_size * 8),
                                       MDPAtomField("duration", duration_size * 8),
                                       MDPAtomField("reserved2", 8 * 8),
                                       MDPAtomField("layer", 2 * 8),
                                       MDPAtomField("alternate_group", 2 * 8),
                                       MDPAtomField("volume", 2 * 8),
                                       MDPAtomField("reserved3", 2 * 8),
                                       MDPAtomField("matrix", 36 * 8,
                                                    MDPFieldDisplayType_hex),
                                       MDPAtomField("width", 4 * 8,
                                                    MDPFieldDisplayType_fixed_16X16_float),
                                       MDPAtomField("height", 4 * 8,
                                                    MDPFieldDisplayType_fixed_16X16_float)};
                                   for (auto &el : fields) {
                                       atom->writeField(el);
                                   }
                               }));
    /**
     tref box可以描述两track之间关系。
     比如：一个MP4文件中有三条video track，ID分别是2、3、4，以及三条audio
     track，ID分别是6、7、8。 在播放track
     2视频时到底应该采用6、7、8哪条音频与其配套播放？这时候就需要在track 2与6的tref
     box中指定一下，将2与6两条track绑定起来。
     **/
    _parseTableEntry.push_back(std::make_tuple(
                                               "moov|trak|tref|hint|font|vdep|vplx|subt|trgr|msrc|mdia|minf|udta|edts|iprp|ipco|moof|traf|mvex|hoov",
                                               [=](std::shared_ptr<MDPAtom> atom) {
                                                   if (atom->name == "trak") {
                                                       currentStream_ = std::make_shared<MALMP4Stream>();
                                                       malFormatContext_->addStream(currentStream_);
                                                   }
                                                   this->_parseChildAtom(atom);
                                                   
                                               }));
    // full box
    _parseTableEntry.push_back(
                               std::make_tuple("meta", [=](std::shared_ptr<MDPAtom> atom) {
                                   //meta 根据标准里边说的，应该是full box，但是有的是，有的不是，和ffmpeg一样，往后检测，找到hdlr
                                   while (!atom->dataSource->isEof() && atom->dataSource->readBytesString(4) != "hdlr") {}
                                   if (!atom->dataSource->isEof()) {
                                       atom->dataSource->seekBytes(-8);
                                       this->_parseChildAtom(atom);
                                   }
                                   
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("ilst", [=](std::shared_ptr<MDPAtom> atom) {
                                   auto datasource = atom->dataSource;
                                   while (!datasource->isEof()) {
                                       int64_t current = datasource->currentBytesPosition();
                                       //下边也是一个一个的box
                                       int64_t atom_size = rbits_i(4 * 8);
                                       auto atom_type = rbytes_s(4);
                                       std::string key = "";
                                       if (atom_type == "\xa9""nam") {
                                           key = "title";
                                       } else if (atom_type == "\xa9""ART") {
                                           key = "artist";
                                       }
                                       datasource->seekBytes(current + atom_size,SEEK_SET);
                                   }
                                   
                                   
                               }));
    
    _parseTableEntry.push_back(std::make_tuple(
                                               "\xa9nam|\xa9too",
                                               [=](std::shared_ptr<MDPAtom> atom) {
                                                   
                                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("mdhd", [=](std::shared_ptr<MDPAtom> atom) {
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   int creation_time_size = 4;
                                   int modification_time_size = 4;
                                   int timescale_size = 4;
                                   int duration_size = 4;
                                   if (version == 1) {
                                       creation_time_size = 8;
                                       modification_time_size = 8;
                                       timescale_size = 4;
                                       duration_size = 8;
                                   }
                                   std::vector<MDPAtomField> fields = {
                                       MDPAtomField("creation_time", creation_time_size * 8),
                                       MDPAtomField("modification_time", modification_time_size * 8),
                                       MDPAtomField("timescale", timescale_size * 8,MDPFieldDisplayType_int64,1, [=,&version](fast_any::any val) {
                                           currentStream_->timescale =  *(val.as<uint64_t>());
                                           return val;
                                       }),
                                       MDPAtomField("duration", duration_size * 8),
                                       MDPAtomField("pad_language", 2 * 8),
                                       MDPAtomField("pre_defined", 2 * 8),
                                   };
                                   for (auto &el : fields) {
                                       atom->writeField(el);
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("hdlr", [=](std::shared_ptr<MDPAtom> atom) {
                                   std::string handler_type = "";
                                   std::vector<MDPAtomField> fields = {
                                       MDPAtomField("version", 1 * 8),
                                       MDPAtomField("flags", 3 * 8),
                                       MDPAtomField("pre_defined", 4 * 8),
                                       MDPAtomField("handler_type", 4 * 8,
                                                    MDPFieldDisplayType_string,1,[&handler_type](fast_any::any val){
                                           handler_type = *(val.as<std::string>());
                                           return val;
                                       }),
                                       MDPAtomField("reserved", 12 * 8,
                                                    MDPFieldDisplayType_hex),
                                   };
                                   for (auto &el : fields) {
                                       atom->writeField(el);
                                   }
                                   std::string name = atom->writeField<std::string>("name", (atom->size - atom->dataSource->currentBytesPosition()) * 8,MDPFieldDisplayType_string);
                                   if (handler_type== "vide")  {
                                       currentStream_->mediaType = MALMediaType::video;
                                   } else if (handler_type == "soun") {
                                       currentStream_->mediaType = MALMediaType::audio;
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("elng", [=](std::shared_ptr<MDPAtom> atom) {
                                   atom->writeField("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   atom->writeField(
                                                    "extended_language",
                                                    (atom->size - atom->dataSource->currentBytesPosition()) * 8,
                                                    MDPFieldDisplayType_string);
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("stbl", [=](std::shared_ptr<MDPAtom> atom) {
                                   this->_parseChildAtom(atom);
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("stsd", [=](std::shared_ptr<MDPAtom> atom) {
                                   atom->writeField("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   unsigned char *entry_count_cr = atom->dataSource->readBytesRaw(4);
                                   int entry_count = (int)(((entry_count_cr[0] & 0xFF) << 24) |
                                                           ((entry_count_cr[1] & 0xFF) << 16) |
                                                           ((entry_count_cr[2] & 0xFF) << 8) |
                                                           (entry_count_cr[3] & 0xFF));
                                   for (int i = 1; i <= entry_count; i++) {
                                       this->_parseChildAtom(atom);
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("mp4a|ipcm|fpcm", [=](std::shared_ptr<MDPAtom> atom) {
                                   uint64_t version = 0;
                                   std::vector<MDPAtomField> fields = {
                                       MDPAtomField("reserved", 6 * 8),
                                       MDPAtomField("data_reference_index", 2 * 8),
                                       MDPAtomField("version", 2 * 8,MDPFieldDisplayType_int64,1, [=,&version](fast_any::any val) {
                                           version =  *(val.as<uint64_t>());
                                           return val;
                                       }),
                                       MDPAtomField("reserved", 6 * 8),
                                       MDPAtomField("channelcount", 2 * 8),
                                       MDPAtomField("samplesize", 2 * 8),
                                       MDPAtomField("pre_defined", 2 * 8),
                                       MDPAtomField("reserved", 2 * 8),
                                   };
                                   for (auto &el : fields) {
                                       atom->writeField(el);
                                   }
                                   atom->writeField("samplerate", 4 * 8, MDPFieldDisplayType_int64,1,
                                                    ">>16", [=](fast_any::any val) {
                                       uint64_t ori = *(val.as<uint64_t>());
                                       ori = (ori >> 16);
                                       val.emplace<uint64_t>(ori);
                                       return val;
                                   });
                                   if (version == 1) { //这里是否需要判断compatible_brands 为qt,ffmpeg是判断了
                                       atom->writeField("samples_per_frame", 4 * 8);
                                       atom->writeField("bytes_per_packet", 4 * 8);
                                       atom->writeField("bytes_per_frame", 4 * 8);
                                       atom->writeField("bytes_per_sample", 4 * 8);
                                   } else if (version == 2) {
                                       atom->writeField("sizeof_struct_only", 4 * 8);
                                       atom->writeField("sample_rate", 8 * 8, MDPFieldDisplayType_double);
                                       atom->writeField("channels", 4 * 8);
                                       atom->writeField("reserved", 4 * 8,MDPFieldDisplayType_int64,1,"always 0x7F000000");
                                       atom->writeField("bits_per_coded_sample", 4 * 8);
                                       atom->writeField("flags", 4 * 8);
                                       atom->writeField("bytes_per_frame", 4 * 8);
                                       atom->writeField("samples_per_frame", 4 * 8);
                                   }
                                   this->_parseChildAtom(atom);
                                   
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("avc1|avc2", [=](std::shared_ptr<MDPAtom> atom) {
                                   if (currentStream_->videoConfig.size() > 0) {
                                       malFormatContext_->shallowCheck.warning.push_back("stsd下存在多个avc1/avc2 box，视频播放可能存在兼容问题");
                                   }
                                   auto avcc = std::make_shared<MALAVCC>();
                                   currentStream_->videoConfig.push_back(avcc);
                                   currentStream_->codecType = MALCodecType::h264;
                                   std::vector<MDPAtomField> fields = {
                                       MDPAtomField("reserved", 6 * 8),
                                       MDPAtomField("data_reference_index", 2 * 8),
                                       MDPAtomField("pre_defined", 2 * 8),
                                       MDPAtomField("reserved", 2 * 8),
                                       MDPAtomField("pre_defined", 12 * 8),
                                       MDPAtomField("width", 2 * 8,MDPFieldDisplayType_int64,1,[=](fast_any::any any) {
                                           avcc->width = (int)(*(any.as<uint64_t>()));
                                           return any;
                                       }),
                                       MDPAtomField("height", 2 * 8,MDPFieldDisplayType_int64,1,[=](fast_any::any any) {
                                           avcc->height = (int)(*(any.as<uint64_t>()));
                                           return any;
                                       }),
                                       MDPAtomField("horizresolution", 4 * 8),
                                       MDPAtomField("vertresolution", 4 * 8),
                                       MDPAtomField("reserved", 4 * 8),
                                       MDPAtomField("frame_count", 2 * 8),
                                       MDPAtomField("compressorname", 32 * 8,
                                                    MDPFieldDisplayType_string),
                                       MDPAtomField("depth", 2 * 8),
                                       MDPAtomField("pre_defined", 2 * 8),
                                       
                                   };
                                   for (auto &el : fields) {
                                       atom->writeField(el);
                                   }
                                   this->_parseChildAtom(atom);
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("avcC", [=](std::shared_ptr<MDPAtom> atom) {
                                   int64_t lastBytes = atom->dataSource->totalSize() - atom->dataSource->currentBytesPosition();
                                   auto avcc_datasource = atom->dataSource->readBytesStream(lastBytes,true);
                                   if (video_config_future_.valid()) {
                                       mdp_video_header *header = video_config_future_.get();
                                       if (header) {
                                           video_configs_.push_back(header);
                                       }
                                   }
                                   video_config_future_ = std::async(std::launch::async, [=](){
                                       MALCodecParser codecParser(avcc_datasource, malFormatContext_, currentStream_, currentStream_->currentConfig());
                                       mdp_video_header * video_config_ = codecParser.parse_avcc();
                                       return video_config_;
                                   });
                                   uint64_t numOfSequenceParameterSets = 0;
                                   std::vector<MDPAtomField> fields = {
                                       MDPAtomField("configurationVersion", 1 * 8),
                                       MDPAtomField("AVCProfileIndication", 1 * 8),
                                       MDPAtomField("profile_compatibility", 1 * 8),
                                       MDPAtomField("AVCLevelIndication", 1 * 8),
                                       
                                       MDPAtomField("reserved", 6),
                                       MDPAtomField("lengthSizeMinusOne", 2,MDPFieldDisplayType_int64,1,[=](fast_any::any any) {
                                           currentStream_->currentConfig()->lengthSizeMinusOne = (int)(*(any.as<uint64_t>()));
                                           return any;
                                       }),
                                       
                                       MDPAtomField("reserved", 3),
                                       MDPAtomField("numOfSequenceParameterSets", 5,
                                                    MDPFieldDisplayType_int64,1,
                                                    [=, &numOfSequenceParameterSets](fast_any::any val) {
                                           uint64_t ori = *(val.as<uint64_t>());
                                           numOfSequenceParameterSets = ori;
                                           return val;
                                       }),
                                   };
                                   for (auto &el : fields) {
                                       atom->writeField(el);
                                   }
                                   for (int i = 0; i < numOfSequenceParameterSets; i++) {
                                       uint64_t sequenceParameterSetLength =
                                       atom->writeField<uint64_t>("sequenceParameterSetLength", 2 * 8);
                                       //                                       atom->dataSource->skipBytes(sequenceParameterSetLength);
                                       atom->writeField("sps_nal_data",sequenceParameterSetLength * 8,MDPFieldDisplayType_hex);
                                   }
                                   uint64_t numOfPictureParameterSets =
                                   atom->writeField<uint64_t>("numOfPictureParameterSets", 1 * 8);
                                   for (int i = 0; i < numOfPictureParameterSets; i++) {
                                       uint64_t pictureParameterSetLength =
                                       atom->writeField<uint64_t>("pictureParameterSetLength", 2 * 8);
                                       //                                       atom->dataSource->skipBytes(pictureParameterSetLength);
                                       atom->writeField("pps_nal_data",pictureParameterSetLength * 8,MDPFieldDisplayType_hex);
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("elst", [=](std::shared_ptr<MDPAtom> atom) {
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   uint64_t entry_count = atom->writeField<uint64_t>("entry_count", 4 * 8);
                                   int segment_duration_size = 4;
                                   int media_time_size = 4;
                                   if (version == 1) {
                                       segment_duration_size = 8;
                                       media_time_size = 8;
                                   }
                                   for (int i = 1; i <= entry_count; i++) {
                                       uint64_t duration = atom->writeField<uint64_t>("segment_duration", segment_duration_size * 8);
                                       uint64_t media_time = atom->writeField<uint64_t>("media_time", media_time_size * 8);
                                       uint64_t rate_integer = atom->writeField<uint64_t>("media_rate_integer", 2 * 8);
                                       uint64_t rate_fraction = atom->writeField<uint64_t>("media_rate_fraction", 2 * 8);
                                       currentStream_->elst.push_back({duration,media_time,rate_integer,rate_fraction});
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("stts", [=](std::shared_ptr<MDPAtom> atom) {
                                   atom->writeField("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   uint64_t entry_count = atom->writeField<uint64_t>("entry_count", 4 * 8);
                                   for (int i = 0; i < entry_count; i++) {
                                       uint64_t count = atom->writeField<uint64_t>(fmt::format("sample_count[{}]", i), 4 * 8);
                                       uint64_t offset = atom->writeField<uint64_t>(fmt::format("sample_delta[{}]", i), 4 * 8);
                                       currentStream_->stts.push_back({count,offset});
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("stss", [=](std::shared_ptr<MDPAtom> atom) {
                                   atom->writeField("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   uint64_t entry_count = atom->writeField<uint64_t>("entry_count", 4 * 8);
                                   for (int i = 0; i < entry_count; i++) {
                                       auto sample_number = atom->writeField<uint64_t>(fmt::format("sample_number[{}]", i), 4 * 8);
                                       currentStream_->stss.push_back(sample_number);
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("ctts", [=](std::shared_ptr<MDPAtom> atom) {
                                   atom->writeField("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   uint64_t entry_count = atom->writeField<uint64_t>("entry_count", 4 * 8);
                                   for (int i = 0; i < entry_count; i++) {
                                       uint64_t count = atom->writeField<uint64_t>(fmt::format("sample_count[{}]", i), 4 * 8);
                                       uint64_t offset = atom->writeField<uint64_t>(fmt::format("sample_offset[{}]", i), 4 * 8);
                                       currentStream_->ctts.push_back({count,offset});
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("stsh", [=](std::shared_ptr<MDPAtom> atom) {
                                   atom->writeField("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   uint64_t entry_count = atom->writeField<uint64_t>("entry_count", 4 * 8);
                                   for (int i = 0; i < entry_count; i++) {
                                       atom->writeField(fmt::format("shadowed_sample_number[{}]", i), 4 * 8);
                                       atom->writeField(fmt::format("sync_sample_number[{}]", i), 4 * 8);
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("stco", [=](std::shared_ptr<MDPAtom> atom) {
                                   atom->writeField("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   uint64_t entry_count = atom->writeField<uint64_t>("entry_count", 4 * 8);
                                   for (int i = 0; i < entry_count; i++) {
                                       uint64_t chunk_offset = atom->writeField<uint64_t>(fmt::format("chunk_offset[{}]", i), 4 * 8);
                                       currentStream_->stco.push_back(chunk_offset);
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("co64", [=](std::shared_ptr<MDPAtom> atom) {
                                   atom->writeField("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   uint64_t entry_count = atom->writeField<uint64_t>("entry_count", 4 * 8);
                                   for (int i = 0; i < entry_count; i++) {
                                       uint64_t chunk_offset = atom->writeField<uint64_t>(fmt::format("chunk_offset[{}]", i), 8 * 8);
                                       currentStream_->stco.push_back(chunk_offset);
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("stsc", [=](std::shared_ptr<MDPAtom> atom) {
                                   atom->writeField("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   uint64_t entry_count = atom->writeField<uint64_t>("entry_count", 4 * 8);
                                   for (int i = 0; i < entry_count; i++) {
                                       auto first_chunk = atom->writeField<uint64_t>(fmt::format("first_chunk[{}]", i), 4 * 8);
                                       auto samples_per_chunk =atom->writeField<uint64_t>(fmt::format("samples_per_chunk[{}]", i), 4 * 8);
                                       auto sample_description_index = atom->writeField<uint64_t>(fmt::format("sample_description_index[{}]", i),
                                                        4 * 8);
                                       currentStream_->stsc.push_back({first_chunk,samples_per_chunk,sample_description_index});
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("stsz", [=](std::shared_ptr<MDPAtom> atom) {
                                   atom->writeField("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   uint64_t sample_size = atom->writeField<uint64_t>("sample_size", 4 * 8);
                                   uint64_t sample_count = atom->writeField<uint64_t>("sample_count", 4 * 8);
                                   if (sample_size == 0) {
                                       for (int i = 0; i < sample_count; i++) {
                                           auto entry_size = atom->writeField<uint64_t>(fmt::format("entry_size[{}]", i), 4 * 8);
                                           currentStream_->stsz.push_back(entry_size);
                                       }
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("pitm", [=](std::shared_ptr<MDPAtom> atom) {
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   if (!version) {
                                       atom->writeField("item_ID", 2 * 8);
                                   } else {
                                       atom->writeField("item_ID", 4 * 8);
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("iloc", [=](std::shared_ptr<MDPAtom> atom) {
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   uint64_t offset_size = atom->writeField<uint64_t>("offset_size", 4);
                                   uint64_t length_size = atom->writeField<uint64_t>("length_size", 4);
                                   uint64_t base_offset_size =
                                   atom->writeField<uint64_t>("base_offset_size", 4);
                                   uint64_t index_size = 0;
                                   if (version == 1 || version == 2) {
                                       index_size = atom->writeField<uint64_t>("index_size", 4);
                                   } else {
                                       atom->writeField("reserved", 4);
                                   }
                                   int item_count = 0;
                                   int item_count_size = 0;
                                   if (version < 2) {
                                       item_count_size = 2;
                                   } else {
                                       item_count_size = 4;
                                   }
                                   item_count =
                                   atom->writeField<uint64_t>("item_count", item_count_size * 8);
                                   for (int i = 0; i < item_count; i++) {
                                       int item_id_size = 0;
                                       if (version < 2) {
                                           item_id_size = 2;
                                       } else {
                                           item_id_size = 4;
                                       }
                                       atom->writeField("item_id", item_id_size * 8);
                                       if (version == 1 || version == 2) {
                                           atom->writeField("reserved", 12);
                                           atom->writeField("construction_method", 4);
                                       }
                                       atom->writeField("data_reference_index", 2 * 8);
                                       atom->writeField("base_offset", base_offset_size * 8);
                                       uint64_t extent_count = atom->writeField<uint64_t>("extent_count", 2 * 8);
                                       for (int j = 0; j < extent_count; j++) {
                                           if ((version == 1 || version == 2) && (index_size > 0)) {
                                               atom->writeField("extent_index", index_size * 8);
                                           }
                                           atom->writeField("extent_offset", offset_size * 8);
                                           atom->writeField("extent_length", length_size * 8);
                                       }
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("iinf", [=](std::shared_ptr<MDPAtom> atom) {
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   int entry_count_size = version == 0 ? 2 : 4;
                                   atom->writeField("entry_count", entry_count_size * 8);
                                   this->_parseChildAtom(atom);
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("infe", [=](std::shared_ptr<MDPAtom> atom) {
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   if (version == 0 || version == 1) {
                                       atom->writeField("item_id", 2 * 8);
                                       atom->writeField("item_protection_index", 2 * 8);
                                       std::vector<std::string> names = {"item_name","content_type","content_encoding"};
                                       for (auto& name : names) {
                                           int64_t size = atom->dataSource->nextNonNullLength();
                                           if (size > 0) {
                                               atom->writeField(name, size * 8, MDPFieldDisplayType_string);
                                           }
                                       }
                                   }
                                   if (version == 1) {
                                       atom->writeField("extension_type", 4 * 8);
                                       this->_parseChildAtom(atom,true); //ItemInfoExtension
                                   }
                                   if (version >= 2) {
                                       int item_ID_size = version == 2 ? 2:4;
                                       atom->writeField("item_ID", item_ID_size * 8);
                                       atom->writeField("item_protection_index", 2 * 8);
                                       atom->writeField("item_type", 4 * 8,MDPFieldDisplayType_string);
                                   }
                                   
                                   int64_t size = atom->dataSource->nextNonNullLength();
                                   std::string item_name = "";
                                   if (size > 0) {
                                       item_name = atom->writeField<std::string>("item_name", size * 8, MDPFieldDisplayType_string);
                                   }
                                   std::vector<std::string> names = {};
                                   if (item_name == "mime") {
                                       names = {"content_type","content_encoding"};
                                   } else if (item_name == "uri") {
                                       names = {"item_uri_type"};
                                   }
                                   for (auto& name : names) {
                                       int64_t size = atom->dataSource->nextNonNullLength();
                                       if (size > 0) {
                                           atom->writeField(name, size * 8, MDPFieldDisplayType_string);
                                       }
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("iref", [=](std::shared_ptr<MDPAtom> atom) {
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   this->iref_version_ = version;
                                   atom->writeField("flags", 3 * 8);
                                   this->_parseChildAtom(atom);
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("thmb|dimg|cdsc|auxl", [=](std::shared_ptr<MDPAtom> atom) {
                                   int from_item_ID_bytes = 2;
                                   int reference_count_bytes = 2;
                                   int to_item_ID_bytes = 2;
                                   if (this->iref_version_ != 0) {
                                       from_item_ID_bytes = 4;
                                       to_item_ID_bytes = 4;
                                   }
                                   atom->writeField("from_item_ID", from_item_ID_bytes * 8);
                                   uint64_t reference_count = atom->writeField<uint64_t>("reference_count", reference_count_bytes * 8);
                                   for (int j = 0; j < reference_count; j++) {
                                       atom->writeField("to_item_ID", to_item_ID_bytes * 8);
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("ipma", [=](std::shared_ptr<MDPAtom> atom) { //对于heif描述了item_id 和 ipco的关系
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   uint64_t flags = atom->writeField<uint64_t>("flags", 3 * 8);
                                   uint64_t entry_count = atom->writeField<uint64_t>("entry_count", 4 * 8);
                                   for(int i = 0; i < entry_count; i++) {
                                       int item_id_size = version < 1 ? 2 : 4;
                                       atom->writeField("item_ID", item_id_size * 8);
                                       uint64_t association_count = atom->writeField<uint64_t>("association_count", 1 * 8);
                                       for(int i = 0; i<association_count;i++) {
                                           atom->writeField("essential", 1); //必不可少的
                                           if(flags & 1) {
                                               atom->writeField("property_index", 15);
                                           } else {
                                               atom->writeField("property_index", 7);
                                           }
                                       }
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("sidx", [=](std::shared_ptr<MDPAtom> atom) { //对于heif描述了item_id 和 ipco的关系
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   atom->writeField("flags", 3 * 8);
                                   atom->writeField("reference_ID", 4 * 8);
                                   atom->writeField("timescale", 4 * 8);
                                   int earliest_presentation_time_size = 4;
                                   int first_offset_size = 4;
                                   if(version != 0) {
                                       earliest_presentation_time_size = 8;
                                       first_offset_size = 8;
                                   }
                                   atom->writeField("earliest_presentation_time", earliest_presentation_time_size * 8);
                                   atom->writeField("first_offset", first_offset_size * 8);
                                   atom->writeField("reserved", 2 * 8);
                                   uint64_t reference_count = atom->writeField<uint64_t>("reference_count", 2 * 8);
                                   for(int i = 0; i < reference_count; i++) {
                                       atom->writeField("reference_type", 1);
                                       atom->writeField("referenced_size", 31);
                                       atom->writeField("subsegment_duration", 32);
                                       atom->writeField("starts_with_SAP", 1);
                                       atom->writeField("SAP_type", 3);
                                       atom->writeField("SAP_delta_time", 28);
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("tfhd", [=](std::shared_ptr<MDPAtom> atom) { //对于heif描述了item_id 和 ipco的关系
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   uint64_t flags = atom->writeField<uint64_t>("flags", 3 * 8);
                                   uint64_t track_ID = atom->writeField<uint64_t>("track_ID", 4 * 8);
                                   if(flags & 0x01 ) {
                                       atom->writeField("base_data_offset", 8 * 8);
                                   }
                                   if(flags & 0x02 ) {
                                       atom->writeField("sample_description_index", 4 * 8);
                                   }
                                   if(flags & 0x08 ) {
                                       atom->writeField("default_sample_duration", 4 * 8);
                                   }
                                   if(flags & 0x10 ) {
                                       atom->writeField("default_sample_size", 4 * 8);
                                   }
                                   if(flags & 0x20 ) {
                                       atom->writeField("default_sample_flags", 4 * 8);
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("trun", [=](std::shared_ptr<MDPAtom> atom) { //对于heif描述了item_id 和 ipco的关系
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   uint64_t flags = atom->writeField<uint64_t>("flags", 3 * 8);
                                   uint64_t sample_count = atom->writeField<uint64_t>("sample_count", 4 * 8);
                                   if(flags & 0x000001) {
                                       atom->writeField("data_offset", 4 * 8);
                                   }
                                   if(flags & 0x000004) {
                                       atom->writeField("first_sample_flags", 4 * 8);
                                   }
                                   for(int i = 0; i< sample_count; i++) {
                                       if(flags & 0x000100) {
                                           atom->writeField("sample_duration", 4 * 8);
                                       }
                                       if(flags & 0x000200) {
                                           atom->writeField("sample_size", 4 * 8);
                                       }
                                       if(flags & 0x000400) {
                                           atom->writeField("sample_flags", 4 * 8);
                                       }
                                       if(flags & 0x000800) {
                                           atom->writeField("sample_composition_time_offset", 4 * 8);
                                       }
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("trex", [=](std::shared_ptr<MDPAtom> atom) {
                                   std::vector<MDPAtomField> fields = {
                                       MDPAtomField("version", 1 * 8),
                                       MDPAtomField("flags", 3 * 8),
                                       MDPAtomField("track_ID", 4 * 8),
                                       MDPAtomField("default_sample_description_index", 4 * 8),
                                       MDPAtomField("default_sample_duration", 4 * 8),
                                       MDPAtomField("default_sample_size", 4 * 8),
                                       MDPAtomField("default_sample_flags", 4 * 8),
                                   };
                                   for (auto &el : fields) {
                                       atom->writeField(el);
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("tfdt", [=](std::shared_ptr<MDPAtom> atom) { //对于heif描述了item_id 和 ipco的关系
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   uint64_t flags = atom->writeField<uint64_t>("flags", 3 * 8);
                                   int baseMediaDecodeTimeSize = 4;
                                   if(version == 1) {
                                       baseMediaDecodeTimeSize = 8;
                                   }
                                   atom->writeField("baseMediaDecodeTime", baseMediaDecodeTimeSize * 8);
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("mfhd", [=](std::shared_ptr<MDPAtom> atom) { //对于heif描述了item_id 和 ipco的关系
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   uint64_t flags = atom->writeField<uint64_t>("flags", 3 * 8);
                                   atom->writeField("sequence_number", 4 * 8);
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("hvc1|hev1", [=](std::shared_ptr<MDPAtom> atom) {
                                   if (currentStream_->videoConfig.size() > 0) {
                                       malFormatContext_->shallowCheck.warning.push_back("stsd下存在多个hvc1/hev1 box，视频播放可能存在兼容问题");
                                   }
                                   auto hvcc = std::make_shared<MALHVCC>();
                                   currentStream_->videoConfig.push_back(hvcc);
                                   currentStream_->codecType = MALCodecType::h265;
                                   std::vector<MDPAtomField> fields = {
                                       MDPAtomField("reserved", 6 * 8),
                                       MDPAtomField("data_reference_index", 2 * 8),
                                       MDPAtomField("pre_defined", 2 * 8),
                                       MDPAtomField("reserved", 2 * 8),
                                       MDPAtomField("pre_defined", 12 * 8),
                                       MDPAtomField("width", 2 * 8,MDPFieldDisplayType_int64,1,[=](fast_any::any any) {
                                           hvcc->width = (int)(*(any.as<uint64_t>()));
                                           return any;
                                       }),
                                       MDPAtomField("height", 2 * 8,MDPFieldDisplayType_int64,1,[=](fast_any::any any) {
                                           hvcc->height = (int)(*(any.as<uint64_t>()));
                                           return any;
                                       }),
                                       MDPAtomField("horizresolution", 4 * 8, MDPFieldDisplayType_hex), //0x00480000; // 72 dpi
                                       MDPAtomField("vertresolution", 4 * 8, MDPFieldDisplayType_hex),
                                       MDPAtomField("reserved", 4 * 8),
                                       MDPAtomField("frame_count", 2 * 8),
                                       MDPAtomField("compressorname", 32 * 8, MDPFieldDisplayType_string),
                                       MDPAtomField("depth", 2 * 8),
                                       MDPAtomField("pre_defined", 2 * 8),
                                   };
                                   for (auto &el : fields) {
                                       atom->writeField(el);
                                   }
                                   this->_parseChildAtom(atom);
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("pasp", [=](std::shared_ptr<MDPAtom> atom) {
                                   uint64_t hSpacing = atom->writeField<uint64_t>("hSpacing", 4 * 8);
                                   uint64_t vSpacing = atom->writeField<uint64_t>("vSpacing", 4 * 8);
                                   if (hSpacing * 1.0 / vSpacing != 1.0) {
                                       malFormatContext_->shallowCheck.warning.push_back("视频存在pasp，sar值不是1，注意渲染");
                                   }
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("hvcC", [=](std::shared_ptr<MDPAtom> atom) { //对于heif描述了item_id 和 ipco的关系
                                   int64_t lastBytes = atom->dataSource->totalSize() - atom->dataSource->currentBytesPosition();
                                   auto hvcc_datasource = atom->dataSource->readBytesStream(lastBytes,true);
                                   if (video_config_future_.valid()) {
                                       mdp_video_header *header = video_config_future_.get();
                                       if (header) {
                                           video_configs_.push_back(header);
                                       }
                                   }
                                   video_config_future_ = std::async(std::launch::async, [=](){
                                       MALCodecParser codecParser(hvcc_datasource,malFormatContext_, currentStream_, currentStream_->currentConfig());
                                       mdp_video_header *video_config_ =codecParser.parse_hvcc();
                                       return video_config_;
                                   });
                                   std::vector<MDPAtomField> fields = {
                                       MDPAtomField("configurationVersion", 1 * 8),
                                       MDPAtomField("general_profile_space", 2),
                                       MDPAtomField("general_tier_flag", 1),
                                       MDPAtomField("general_profile_idc", 5),
                                       MDPAtomField("general_profile_compatibility_flags", 4 * 8),
                                       MDPAtomField("general_constraint_indicator_flags", 6 * 8),
                                       MDPAtomField("general_level_idc", 1 * 8),
                                       MDPAtomField("reserved", 4),
                                       MDPAtomField("min_spatial_segmentation_idc", 12),
                                       MDPAtomField("reserved", 6),
                                       MDPAtomField("parallelismType", 2),
                                       MDPAtomField("reserved", 6),
                                       MDPAtomField("chroma_format_idc", 2),
                                       MDPAtomField("reserved", 5),
                                       MDPAtomField("bit_depth_luma_minus8", 3),
                                       MDPAtomField("reserved", 5),
                                       MDPAtomField("bit_depth_chroma_minus8", 3),
                                       MDPAtomField("avgFrameRate", 16),
                                       MDPAtomField("constantFrameRate", 2),
                                       MDPAtomField("numTemporalLayers", 3),
                                       MDPAtomField("temporalIdNested", 1),
                                       MDPAtomField("lengthSizeMinusOne", 2,MDPFieldDisplayType_int64,1,[=](fast_any::any any) {
                                           currentStream_->currentConfig()->lengthSizeMinusOne = (int)(*(any.as<uint64_t>()));
                                           return any;
                                       }),
                                   };
                                   for (auto &el : fields) {
                                       atom->writeField(el);
                                   }
                                   uint64_t numOfArrays = atom->writeField<uint64_t>("numOfArrays",8, MDPFieldDisplayType_int64);
                                   for(int j = 0; j < numOfArrays; j++) {
                                       atom->writeField("array_completeness",1);
                                       atom->writeField("reserved",1);
                                       atom->writeField("NAL_unit_type",6);
                                       uint64_t numNalus = atom->writeField<uint64_t>("numNalus",2 * 8);
                                       for(int i = 0; i< numNalus; i++) {
                                           uint64_t nalUnitLength = atom->writeField<uint64_t>("nalUnitLength",2 * 8);
                                           //atom->dataSource->seekBytes(nalUnitLength);
                                           atom->writeField("nal_data",nalUnitLength * 8,MDPFieldDisplayType_hex);
                                       }
                                   }
                                   
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("ispe", [=](std::shared_ptr<MDPAtom> atom) { //对于heif描述了item_id 和 ipco的关系
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   uint64_t flags = atom->writeField<uint64_t>("flags", 3 * 8);
                                   atom->writeField("image_width", 4 * 8);
                                   atom->writeField("image_height", 4 * 8);
                               }));
    _parseTableEntry.push_back(
                               std::make_tuple("mfhd", [=](std::shared_ptr<MDPAtom> atom) { //对于heif描述了item_id 和 ipco的关系
                                   uint64_t version = atom->writeField<uint64_t>("version", 1 * 8);
                                   uint64_t flags = atom->writeField<uint64_t>("flags", 3 * 8);
                                   atom->writeField("format_flags", 1 * 8);
                                   atom->writeField("pcm_sample_size", 1 * 8);
                               }));
}
bool MP4Parser::supportFormat() {
    if (_datasource->totalSize() < 8)
        return false;
    int64_t pos = _datasource->currentBytesPosition();
    _datasource->seekBytes(4, SEEK_CUR);
    int type = _datasource->readBytesInt64(4);
    _datasource->seekBytes(pos, SEEK_SET);
    if (type == mdp_strconvet_to_int("ftype", 4, 1)) {
        return true;
    }
    
    return false;
}

std::string MP4Parser::dumpVideoConfig() {
    if (video_config_future_.valid()) {
        mdp_video_header *header = video_config_future_.get();
        if (header) {
            video_configs_.push_back(header);
        }
    }
    cJSON *array = cJSON_CreateArray();
    for (auto video_config_ : video_configs_) {
        if (video_config_ && video_config_->root_item) {
            cJSON *json =  dumpPS_(video_config_->root_item);
            cJSON_AddItemToArray(array, json);
        }
    }
    char *string = cJSON_Print(array);
    std::cout << string << std::endl;
    std::string filename = "/Users/Shared/output_ps.json";
    // 创建一个输出文件流
    std::ofstream outputFile(filename);
    
    // 检查文件是否成功打开
    if (!outputFile) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return ""; // 返回错误代码
    }
    // 将 JSON 字符串写入文件
    outputFile << string;
    
    // 关闭文件
    outputFile.close();
    //    send_async_msg(MDPMessageType_Avc_header,video_config_);
    return "";
}
cJSON * MP4Parser::dumpPS_(mdp_header_item *item) {
    if (!item) return nullptr;
    cJSON *json = cJSON_CreateObject();
    if (item->nb_childs > 0) {
        cJSON *child = cJSON_CreateArray();
        cJSON_AddItemToObject(json, item->cells[0].str_val, child);
        for (int i = 0; i < item->nb_childs; i++) {
            cJSON_AddItemToArray(child, dumpPS_(item->childs[i])) ;
        }
    } else {
        if (item->cells[1].display_type == MDPFieldDisplayType_string) {
            cJSON_AddStringToObject(json, item->cells[0].str_val, item->cells[1].str_val);
        } else {
            cJSON_AddNumberToObject(json, item->cells[0].str_val, item->cells[1].i_val);
        }
    }
    return json;
}
std::string MP4Parser::dumpFormats(int full) {
    return IParser::dumpFormats(full);
}


int MP4Parser::_parseAtom() {
    int ret = 0;
    root_atom_ = std::make_shared<MDPAtom>();
    root_atom_->name = "root";
    root_atom_->dataSource = _datasource;
    root_atom_->size = _datasource->totalSize();
    root_atom_->pos = 0;
    _parseChildAtom(root_atom_);
    return ret;
}


int MP4Parser::_parseChildAtom(std::shared_ptr<MDPAtom> parent, bool once) {
    auto datasource = parent->dataSource;
    while (!datasource->isEof()) {
        std::shared_ptr<MDPAtom> atom = std::make_shared<MDPAtom>();
        atom->pos = datasource->currentBytesPosition();
        atom->size = rbytes_i(4);
        if (atom->size == 0) {
            return 0;
        }
        atom->name = rbytes_s(4);
        atom->headerSize = 8;
        if (atom->size == 1) {
            atom->size = rbytes_i(8);
            atom->headerSize += 8;
        }
        int64_t cur = datasource->currentBytesPosition();
        datasource->seekBytes(atom->pos, SEEK_SET);
        atom->dataSource = datasource->readBytesStream(atom->size);
        parent->childBoxs.push_back(atom);
        for (auto &parse : _parseTableEntry) {
            if (contains(splitToSet(std::get<0>(parse), "|"), atom->name)) {
                atom->dataSource->skipBytes(atom->headerSize); // 跳过头部
                std::get<1>(parse)(atom);
                break;
            }
        }
        if (once) break;
    }
    return 0;
}

int MP4Parser::startParse() {
    int ret = 0;
    ret = _parseAtom();
    int index = 0;
    for (auto& stream : malFormatContext_->streams) {
        if (stream->mediaType == MALMediaType::video) {
            break;
        }
        index++;
    }
    pktLoader_ = std::make_shared<MALMP4PacketLoader>(malFormatContext_,index);
    return 0;
}


