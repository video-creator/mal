#include <libavformat/avformat.h>
#include "../sources/c++/c_ffi/mdp_parser.h"
int main(int argc, char **argv)
{
    int ret = 0;
    char * url = "/Users/Shared/test.heif";
    // url = "/Users/Shared/666.mp4";
    void *temp = mdp_create_parser(url);
    
    mdp_dump_box(temp);
    return 0;
}