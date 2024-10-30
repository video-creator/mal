#include <libavformat/avformat.h>
#include "../sources/c++/c_ffi/mdp_parser.h"
int main(int argc, char **argv)
{
    int ret = 0;

    void *temp = mdp_create_parser("/Users/Shared/666.mp4");
    mdp_dump_box(temp);
    return 0;
}