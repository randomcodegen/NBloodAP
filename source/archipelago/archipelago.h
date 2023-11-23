#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum ap_state_ 
{
    AP_DISABLED,
    AP_INITIALIZED,
    AP_CONNECTED,
    AP_CONNECTION_LOST,
} ap_state_t;

extern ap_state_t g_AP_State;

#define AP (g_AP_State > AP_DISABLED)
#define APConnected (g_AP_State == AP_CONNECTED)

extern void AP_Init(void);

#ifdef __cplusplus
}
#endif
