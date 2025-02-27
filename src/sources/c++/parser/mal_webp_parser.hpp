//
//  mal_webp_parser.hpp
//  Demo
//
//  Created by wangyaqiang on 2024/11/11.
//

#ifndef mal_webp_parser_hpp
#define mal_webp_parser_hpp

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
    class WEBPParser : public IParser {
        public:
        WEBPParser(const std::shared_ptr<IDataSource>& datasource);
        WEBPParser(const std::string& path, Type type);
        int startParse();
        std::vector<std::shared_ptr<MALPacket>> loadPackets(int size);
        bool supportFormat();
        private:
        mdp_video_header * video_config_;
        std::future<mdp_video_header *> video_config_future_;
        int _parseAtom();
        int _parseChildAtom(std::shared_ptr<MDPAtom> parent, bool once = false);
        void registerParserTableEntry_();
        std::vector<std::tuple<std::string,std::function<void(std::shared_ptr<MDPAtom>)>>> _parseTableEntry;
        uint64_t iref_version_;
        
    };
}

#endif /* mal_webp_parser_hpp */
