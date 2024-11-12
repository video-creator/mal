#ifndef MDP_MEDIASTRING__H
#define MDP_MEDIASTRING__H
#include <stddef.h>
#include <stdint.h>
char** mdp_strsplit( const char* s, const char* delim, size_t* nb );
void mdp_str_case_no_copy( char* s, int64_t size,int upper);

int mdp_strconvet_to_int(const char *c1, int size,int big);
unsigned int mdp_strconvet_to_uint(char *c1, int size,int big);
int mdp_strconvet_to_int64(char *c1, int size,int big);
int mdp_strconvet_to_fixed_16x16_point(char *c1);

char* mdp_intconvert_to_str(int64_t val);
#endif
