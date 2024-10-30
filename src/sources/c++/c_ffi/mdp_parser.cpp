#include "mdp_parser.h"
#include "../datasource/IParser.h"
#include "../datasource/MP4Parser.h"
#include <vector>
#ifdef __cplusplus
extern "C" {
#endif
using namespace mdp;
void* mdp_create_parser(char *path) {
    std::vector<mdp::IParser *> parsers = {
        new mdp::MP4Parser(path,mdp::Type::local)
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
    _parser->dumpFormats();
}
#ifdef __cplusplus
}
#endif
