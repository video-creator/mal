//
//  mal_dump.cpp
//  Demo
//
//  Created by wangyaqiang on 2024/12/24.
//

#include "mal_dump.hpp"

cJSON * dumpPS_(mdp_header_item *item) {
    if (!item) return nullptr;
    cJSON *json = cJSON_CreateObject();
    if (item->nb_childs > 0) {
        cJSON *child = cJSON_CreateArray();
        cJSON_AddItemToObject(json, item->cells[0].str_val, child);
        for (int i = 0; i < item->nb_childs; i++) {
            cJSON_AddItemToArray(child, dumpPS_(item->childs[i])) ;
        }
    } else {
        if (item->cells[1].display_type == MDPFieldDisplayType_string) {
            cJSON_AddStringToObject(json, item->cells[0].str_val, item->cells[1].str_val);
        } else {
            cJSON_AddNumberToObject(json, item->cells[0].str_val, item->cells[1].i_val);
        }
    }
    return json;
}
std::string dumpPacket(std::shared_ptr<MALPacket> pkt) {
    if (!pkt || !pkt->nal_header || !pkt->nal_header->root_item) return "";
    mdp_header_item *item = pkt->nal_header->root_item;
    if (!item) return nullptr;
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "pos", pkt->pos);
    cJSON_AddNumberToObject(json, "index", pkt->number);
    cJSON_AddNumberToObject(json, "poc", pkt->poc);
    cJSON_AddNumberToObject(json, "size", pkt->size());
    cJSON *nals = dumpPS_(item);
    cJSON_AddItemToObject(json, "nals", nals);
    return cJSON_Print(json);
    
}
