#include "mdp_string.h"
#include "mdp_bits.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

static char** _strsplit( const char* s, const char* delim, size_t* nb ) {
	void* data;
	char* _s = ( char* )s;
	const char** ptrs;
	size_t
		ptrsSize,
		nbWords = 1,
		sLen = strlen( s ),
		delimLen = strlen( delim );

	while ( ( _s = strstr( _s, delim ) ) ) {
		_s += delimLen;
		++nbWords;
	}
	ptrsSize = ( nbWords + 1 ) * sizeof( char* );
	ptrs =
	data = malloc( ptrsSize + sLen + 1 );
	if ( data ) {
		*ptrs =
		_s = strcpy( ( ( char* )data ) + ptrsSize, s );
		if ( nbWords > 1 ) {
			while ( ( _s = strstr( _s, delim ) ) ) {
				*_s = '\0';
				_s += delimLen;
				*++ptrs = _s;
			}
		}
		*++ptrs = NULL;
	}
	if ( nb ) {
		*nb = data ? nbWords : 0;
	}
	return data;
}

char** mdp_strsplit( const char* s, const char* delim, size_t* nb ) {
	return _strsplit( s, delim, nb );
}

void mdp_str_case_no_copy( char* s, int64_t size,int upper) {
    for (size_t i = 0; i < size; i++)
    {
        s[i] = upper ? toupper(s[i]) : tolower(s[i]);
    }
}

int mdp_strconvet_to_int(const char *c1, int size,int big) {
	unsigned char *c = (unsigned char *)c1;
    if(size <= 0 || !c) return 0;
    int ret = 0;
    if(big) {

        for(int i = 0; i < size;i++) {
//                qDebug() << (int)c[i];
            ret += ((int)c[i]) << (size -1 - i) * 8;
        }
        return ret;
    } else {
        for(int i = 0; i < size;i++) {
            ret += c[i] << i * 8;
        }
        return ret;
    }
}
unsigned int mdp_strconvet_to_uint(char *c1, int size,int big) {
	unsigned char *c = (unsigned char *)c1;
    if(size <= 0 || !c) return 0;
    int ret = 0;
    if(big) {

        for(int i = 0; i < size;i++) {
//                qDebug() << (int)c[i];
            ret += ((unsigned int)c[i]) << ((size -1 - i) * 8);
        }
        return ret;
    } else {
        for(int i = 0; i < size;i++) {
            ret += ((unsigned int)c[i]) << i * 8;
        }
        return ret;
    }
}
int mdp_strconvet_to_int64(char *c1, int size,int big) {
	unsigned char *c = (unsigned char *)c1;
    if(size <= 0 || !c) return 0;
    int64_t ret = 0;
    if(big) {

        for(int i = 0; i < size;i++) {
//                qDebug() << (int)c[i];
            ret += ((int)c[i]) << (size -1 - i) * 8;
        }
        return ret;
    } else {
        for(int i = 0; i < size;i++) {
            ret += c[i] << i * 8;
        }
        return ret;
    }
}
int mdp_strconvet_to_fixed_16x16_point(char *data) {
	int int_part = mdp_strconvet_to_int(data,2,1);
    double float_part = 0;
    unsigned char*float_part_ptr = (unsigned char *)(data + 2);
    for(int i = 0; i < 16;i++) {
       
        int val = mdp_bits_read(float_part_ptr,i,1);
        float_part += pow(2,-(i+1)) * val;
    }
    return int_part + float_part;
}

char* mdp_intconvert_to_str(int64_t val) {
    static char res[100] = {'\0'};
    snprintf(res,90,"%lld",val);
    return res;
}
