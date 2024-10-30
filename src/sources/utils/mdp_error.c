#include "mdp_error.h"

#define MDP_ERROR_MSG(ID, NAME, ERR_MSG, ERR_DESC) case ID: return ERR_MSG;
const char* mdp_error_msg(int error) {
    switch (error) {
        MDP_ERROR_CODES(MDP_ERROR_MSG);
        default:
            return "Unknown TMFErrorCode";
    }
}