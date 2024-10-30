#include <stdlib.h>
#ifndef MDP_MEDIATYPE__H
#define MDP_MEDIATYPE__H

typedef enum MDPFieldDisplayType {
        MDPFieldDisplayType_int,
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
#endif