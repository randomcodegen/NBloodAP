#include "ap_lib.h"
#include "ap_comp.h"
#include "compat.h"

ap_state_t ap_global_state = AP_DISABLED;
uint8_t ap_game_id = 0;

ap_location_state_t ap_locations[AP_MAX_LOCATION];

void AP_Init(uint8_t game_id)
{
    ap_game_id = game_id & AP_GAME_ID_MASK;
    Bmemset(ap_locations, 0, AP_MAX_LOCATION * sizeof(ap_location_state_t));
    ap_global_state = AP_INITIALIZED;
}

int32_t AP_CheckLocation(ap_location_t loc)
{
    // Check if the location is even a valid id for current game
    if (!AP_VALID_LOCATION_ID(loc))
        return 0;
    // Check if we already have this location confirmed as checked.
    if (AP_LOCATION_CHECKED(loc))
        return 0;
    // Forward check to AP server
    AP_SendItem_Comp(AP_NET_LOCATION(loc));
    ap_locations[loc].state |= AP_LOC_CHECKED;
    return 1;
}
