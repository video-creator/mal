#include "IParser.h"
#include "LocalDataSource.h"
extern "C" {
    #include <libavutil/avassert.h>
}
using namespace mdp;
IParser::IParser(const std::shared_ptr<IDataSource>& datasource):_datasource(datasource) {

}
IParser::IParser(const std::string& path, Type type) {
    if (type == Type::local) {
        _datasource = std::make_shared<LocalDataSource>(path);
    } else {
        av_assert0(true);
    }
    _datasource->open();
    
}
IParser::~IParser() {

}