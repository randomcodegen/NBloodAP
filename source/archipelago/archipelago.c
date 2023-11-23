#include "archipelago.h"

ap_state_t g_AP_State = AP_DISABLED;

void AP_Init(void)
{
    g_AP_State = AP_INITIALIZED;
}
