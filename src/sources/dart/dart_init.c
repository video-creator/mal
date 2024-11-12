#include "dart_init.h"
#include "dart_api_dl.h"
#include "../utils/mdp_type_def.h"
#include <libavutil/mem.h>
DART_EXPORT intptr_t InitDartApiDL(void *data)
{
    return Dart_InitializeApiDL(data);
}

Dart_Port send_port_;
DART_EXPORT void registerSendPort(Dart_Port send_port)
{
    send_port_ = send_port;
}

void send_async_msg(int type,void *ptr) {
    MDPMessage *msg = av_mallocz(sizeof(MDPMessage));
    msg->type = type;
    msg->ptr = (intptr_t)ptr;
    Dart_CObject dart_object;
    dart_object.type = Dart_CObject_kInt64;
    const intptr_t msg_addr = (intptr_t)(msg);
    dart_object.value.as_int64 = msg_addr;
    // bool result = Dart_PostCObject_DL(send_port_, &dart_object);
}
