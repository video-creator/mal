#include "mal_i_parser.h"
#include "mal_atom.h"
#include <tuple>
#include <functional>
#include <future>
extern "C" {
    #include "../../utils/mdp_string.h"
    #include "../../utils/mdp_error.h"
    #include "../../utils/thpool.h"
    #include "libavutil/mem.h"
    #include "../../utils/cJSON.h"
}
namespace mal {
    class MP4Parser : public IParser {
        public:
        MP4Parser(const std::shared_ptr<IDataSource>& datasource);
        MP4Parser(const std::string& path, Type type);
        int startParse();
        std::string dumpFormats(int full = 0);
        std::string dumpVideoConfig();
        void loadPackets(int size);
        bool supportFormat();
        private:
        mdp_video_header * video_config_;
        std::future<mdp_video_header *> video_config_future_;
        int _parseAtom();
        int _parseChildAtom(std::shared_ptr<MDPAtom> parent, bool once = false);
        void registerParserTableEntry_();
        std::vector<std::tuple<std::string,std::function<void(std::shared_ptr<MDPAtom>)>>> _parseTableEntry;
        uint64_t iref_version_;
        cJSON * dumpPS_(mdp_header_item *item);
        
    };
}
