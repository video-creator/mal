//
//  mal_webp_parser.hpp
//  Demo
//
//  Created by wangyaqiang on 2024/11/11.
//

#ifndef mal_flv_parser_hpp
#define mal_flv_parser_hpp

#include <stdio.h>
#include "mal_i_parser.h"
#include "mal_atom.h"
#include <tuple>
#include <functional>
#include <future>
extern "C" {
    #include "../../utils/mdp_error.h"
    #include "../../utils/thpool.h"
    #include "libavutil/mem.h"
    #include "../../utils/cJSON.h"
}
namespace mal {
    class FLVPParser : public IParser {
        public:
        explicit FLVPParser(const std::shared_ptr<IDataSource>& datasource) ;
        FLVPParser(const std::string& path, Type type);
        int startParse();
        std::vector<std::shared_ptr<MALPacket>> loadPackets(int size);
        bool supportFormat();
        private:
        int _parseAtom();
        int _parseChildAtom(std::shared_ptr<MDPAtom> parent, bool once = false);
        int pre_tag_index_ = 0;
        int audio_tag_index_ = 0;
        int video_tag_index_ = 0;
        int script_tag_index_ = 0;
        std::string current_key_ = ""; //script tag
        int processFlvHeader(std::shared_ptr<MDPAtom> atom);
        int processVideoTag_(std::shared_ptr<MDPAtom> atom);
        int processAudioTag_(std::shared_ptr<MDPAtom> atom);
        int processScriptTag_(std::shared_ptr<MDPAtom> atom);
        int processTag_(std::shared_ptr<MDPAtom> atom);
        void processScriptObject_(std::shared_ptr<MDPAtom> atom);
    };
}

#endif /* mal_flv_parser_hpp */
