#pragma once
#include "dart_api.h"
DART_EXPORT intptr_t InitDartApiDL(void *data);
DART_EXPORT void registerSendPort(Dart_Port send_port);

void send_async_msg(int type,void *ptr);
