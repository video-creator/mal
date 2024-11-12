#pragma once
extern "C" {
#include "../c_ffi/mdp_type_def.h"
}

#include <stdlib.h>
#include "mal_idatasource.h"
using namespace mal;
#define MDP_HEADER_DEFAULT_GROW_SIZE 5
mdp_video_header * mdp_parse_avcc(std::shared_ptr<IDataSource> datasource);

mdp_video_header * mdp_parse_hvcc(std::shared_ptr<IDataSource> datasource);

char ** mdp_parse_h264_sei(std::shared_ptr<IDataSource> datasource);


