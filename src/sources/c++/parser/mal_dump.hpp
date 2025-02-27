//
//  mal_dump.hpp
//  Demo
//
//  Created by wangyaqiang on 2024/12/24.
//

#ifndef mal_dump_hpp
#define mal_dump_hpp

#include <stdio.h>
#include "mal_atom.h"
extern "C" {
#include "../../utils/cJSON.h"
}
using namespace mal;
std::string dumpPacket(std::shared_ptr<MALPacket> pkt);
cJSON * dumpPS_(mdp_header_item *item);
#endif /* mal_dump_hpp */
