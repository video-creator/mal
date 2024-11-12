#pragma once
#include "mal_idatasource.h"
#include <vector>
#include <sstream>
#include <iomanip>
#include <functional>
#include "../deps/fast_any/any.h"

extern "C" {
    #include "../../utils/mdp_string.h"
    #include "../c_ffi/mdp_type_def.h"
}
namespace mal
{   
    class MDPAtomField {
        public:
        MDPAtomField() {}
        MDPAtomField(std::string _name,int64_t _pos, int _bits, std::string _extraVal="") {
            name = _name;
            pos = _pos;
            bits = _bits;
            extraVal = _extraVal;
        }
        MDPAtomField(std::string _name, int _bits, MDPFieldDisplayType type = MDPFieldDisplayType_int64 , int _big =1, std::function<fast_any::any(fast_any::any)> _callback = NULL) {
            name = _name;
            bits = _bits;
            display_type = type;
            callback = _callback;
            big = _big;
        }
        template <typename ValueType>
        void putValue(ValueType newValue, MDPFieldDisplayType type) {
            val.emplace<ValueType>(newValue);
            display_type = type;
        }
        
        fast_any::any val;
        std::string name;
        std::string extraVal;
        int64_t pos;
        int bits;
        MDPFieldDisplayType display_type = MDPFieldDisplayType_int64; //显示int
        std::function<fast_any::any(fast_any::any)> callback;
        int big = 1;
    };

    class MDPAtom {
        public:
        std::string name = "";
        int64_t pos = 0;
        int64_t size = 0;
        int headerSize = 0;
        std::shared_ptr<IDataSource> dataSource;
        std::vector<std::shared_ptr<MDPAtom>> childBoxs;
        std::vector<std::shared_ptr<MDPAtomField>> fields;
        std::string intToHexString(int value) {
            std::stringstream ss;
            ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << value;  // 2 位宽，前面填充零
            return ss.str();
        }
        fast_any::any writeField(MDPAtomField field, int big = 1, std::string _extraVal="") {
            return writeField(field.name, field.bits, field.display_type,big, _extraVal,field.callback);
        }
        fast_any::any writeField(std::string _name, int64_t _bits, MDPFieldDisplayType type = MDPFieldDisplayType_int64,int big = 1, std::string _extraVal="" , std::function<fast_any::any(fast_any::any)> callback = NULL) {
            if (_bits > 0) {
                _name = _name + "(" + std::to_string(_bits) + "bits" + ")";
            }
            if (_bits > 64 && type == MDPFieldDisplayType_int64) {
                type = MDPFieldDisplayType_hex;
            }
            auto filed = std::make_shared<MDPAtomField>(_name,dataSource->currentBitsPosition(),_bits,_extraVal);
            if (type == MDPFieldDisplayType_int64) {
                uint64_t val = dataSource->readBitsInt64(_bits,0, big);
                filed->putValue<uint64_t>(val,MDPFieldDisplayType_int64);
            } else if (type == MDPFieldDisplayType_string) {
                std::string val = dataSource->readBytesString(_bits/8);
                filed->putValue<std::string>(val,MDPFieldDisplayType_string);
            } else if (type == MDPFieldDisplayType_fixed_16X16_float) {
                unsigned char *data = dataSource->readBytesRaw(_bits/8);
                double val = mdp_strconvet_to_fixed_16x16_point((char *)data);
                filed->putValue<double>(val,MDPFieldDisplayType_double);
            } else if (type == MDPFieldDisplayType_hex) {
               int bytes = _bits/8;
               std::string val = "0x";
                for (size_t i = 0; i < bytes; i++) {
                    val += intToHexString(dataSource->readBytesInt64(1)) + " ";
                }
                filed->putValue<std::string>(val,MDPFieldDisplayType_string);
            } else if (type == MDPFieldDisplayType_double) {
                int64_t value = dataSource->readBitsInt64(_bits,0,big);
                union tmp {
                    int64_t a;
                    double b;
                };
                union tmp t;
                t.a = value;
                filed->putValue<double>(t.b,MDPFieldDisplayType_double);
            }  else {
                dataSource->skipBits(_bits);
            }
            if (callback) {
                // fast_any::any any;
                // any.emplace<int64_t>(val);
                // val = *(callback(any).as<int64_t>());
                if (filed->val.has_value()) {
                    filed->val = callback(filed->val);
                }
            }          
            fields.push_back(filed);
            return filed->val;
        }
        template <class T = uint64_t>
        T writeField(std::string _name, int64_t _bits, MDPFieldDisplayType type = MDPFieldDisplayType_int64, int big = 1, std::string _extraVal="" , std::function<fast_any::any(fast_any::any)> callback = NULL) {
            fast_any::any val = writeField(_name,_bits,type,big,_extraVal,callback);
            return *(val.as<T>());
        }

    };
    
} // namespace mdp
    
