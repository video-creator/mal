#include "mdp_parser.h"
#include "../parser/mal_i_parser.h"
#include "../parser/mal_mov_parser.h"
#include "../parser/mal_webp_parser.hpp"
#include "../parser/mal_flv_parser.hpp"
#include <vector>
#ifdef __cplusplus
extern "C" {
#endif
using namespace mal;
void* mdp_create_parser(char *path) {
    std::vector<IParser *> parsers = {
        new MP4Parser(path,Type::local),
        new WEBPParser(path,Type::local),
        new FLVPParser(path,Type::local)
    };
    for (auto& el : parsers) {
        if (el->supportFormat()) {
            return el;
        }
    }
    return nullptr;
}
void *mdp_dump_box(void *parser) {
    IParser *_parser = (IParser *)parser;
    _parser->startParse();
    _parser->dumpFormats(1);
    _parser->dumpVideoConfig();
    _parser->loadPackets(500);
    _parser->dumpShallowCheck();
    return NULL;
}

#ifdef __cplusplus
}
#endif
