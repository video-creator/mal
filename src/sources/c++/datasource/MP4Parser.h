#include "IParser.h"
#include "MDPAtom.h"
#include <tuple>
#include <functional>
extern "C" {
    #include "../../utils/mdp_string.h"
    #include "../../utils/mdp_error.h"
    #include "libavutil/mem.h"
}
namespace mdp {
    class MP4Parser : public IParser {
        public:
        MP4Parser(const std::shared_ptr<IDataSource>& datasource);
        MP4Parser(const std::string& path, Type type);
        int startParse();
        std::string dumpFormats();
        void loadPackets(int size);
        bool supportFormat();
        private:
        std::shared_ptr<MDPAtom> _rootAtom;
        int _parseAtom();
        int _parseChildAtom(std::shared_ptr<mdp::MDPAtom> parent, bool once = false);
        std::vector<std::tuple<std::string,std::function<void(std::shared_ptr<mdp::MDPAtom>)>>> _parseTableEntry;
        uint64_t _irefVersion;
    };
}
