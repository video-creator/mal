//
//  mdp_type_def.h
//  Demo
//
//  Created by wangyaqiang on 2024/11/4.
//

#ifndef mdp_type_def_h
#define mdp_type_def_h
#include <stdint.h>
typedef enum MDPFieldDisplayType {
        MDPFieldDisplayType_int64,
        MDPFieldDisplayType_string,
        MDPFieldDisplayType_separator, //分割符，使用在用循环
        MDPFieldDisplayType_hex,
        MDPFieldDisplayType_double,//使用union
        MDPFieldDisplayType_fixed_16X16_float
} MDPFieldDisplayType;
typedef enum MDPMessageType {
        MDPMessageType_Avc_header, //264 SPS,PPS
        MDPMessageType_Frame_Decode
} MDPMessageType;

typedef struct MDPMessage {
        MDPMessageType type;
        intptr_t ptr;
}MDPMessage;

typedef struct mdp_header_cell
{
    MDPFieldDisplayType display_type;
    const char * str_val;
    int enable;
    int i_val;
}mdp_header_cell;

typedef struct mdp_header_item
{
    int nb_childs;
    struct mdp_header_item ** childs;

    int nb_cells;
    mdp_header_cell * cells;
    int capacity;
}mdp_header_item;

typedef struct mdp_video_header_sps {
    int chroma_format_idc;
}mdp_video_header_sps;

typedef struct mdp_video_header
{
    mdp_video_header_sps sps;
    mdp_header_item * root_item;
} mdp_video_header;
#endif /* mdp_type_def_h */
