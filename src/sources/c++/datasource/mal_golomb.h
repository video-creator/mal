//
// Created by wangyaqiang on 2023/4/3.
//
#pragma once
#ifndef MEDIA_EYE_GOLOMB_H
#define MEDIA_EYE_GOLOMB_H
#include <string.h>
#include "mal_idatasource.h"
using namespace mal;
unsigned int golomb_ue(std::shared_ptr<mal::IDataSource> datasource,int64_t *had_read_bits = nullptr);
int golomb_se(std::shared_ptr<mal::IDataSource> datasource, int64_t *had_read_bits = nullptr);

#endif //MEDIA_EYE_GOLOMB_H
