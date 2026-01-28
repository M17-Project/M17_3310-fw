#include "ui_data.h"

extern msg_t rcvd_msg;

const msg_t *UIGetLastMsg(void)
{
    return &rcvd_msg;
}
