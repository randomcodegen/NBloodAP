#pragma once

// Debug behaviour for development only
#ifndef NDEDUG
#ifndef AP_DEBUG_ON
#define AP_DEBUG_ON
#endif
#endif

#include "stdint.h"
#include <json/json.h>
#include "ap_log.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Location ID for AP checks. Can be stored in lotags
typedef int16_t ap_location_t;
// Data type for item/location ids on the Archipelago server side
typedef int64_t ap_net_id_t;

// Namespace prefix for all build engine item and location ids
#define AP_BUILD_ID_PREFIX (0xB17D0000u)
#define AP_LOCATION_MASK (0x7FFu)
#define AP_MAX_LOCATION (AP_LOCATION_MASK)
#define AP_GAME_ID_MASK (0x1Fu)
#define AP_GAME_ID_SHIFT (11u)

extern uint8_t ap_game_id;

#define AP_VALID_LOCATION_ID(x) (x >= 0 && x < AP_MAX_LOCATION)
#define AP_SHORT_LOCATION(x) ((ap_location_t)(x & AP_LOCATION_MASK))
#define AP_NET_ID(x) ((ap_net_id_t)(AP_SHORT_LOCATION(x) | ((ap_game_id & AP_GAME_ID_MASK) << AP_GAME_ID_SHIFT) | AP_BUILD_ID_PREFIX))

typedef enum
{
    AP_LOC_USED        = 0x00000001u,  // Set for all locations that are in use in the current shuffle
    AP_LOC_PROGRESSION = 0x00000010u,  // Set if a progression item is known to be at a location
    AP_LOC_IMPORTANT   = 0x00000020u,  // Set if an item at this location is known to be important
    AP_LOC_TRAP        = 0x00000040u,  // Set if an item at this location is known to be a trap
    AP_LOC_SCOUTED     = 0x00000100u,  // Set if location has been scouted before. This is done during init to get progression state
    AP_LOC_HINTED      = 0x00000200u,  // Set if item at location is logically known to the user
    AP_LOC_CHECKED     = 0x00001000u,  // Set if the location has been checked
    AP_LOC_PICKUP      = 0x00010000u,  // Set if location corresponds to an item pickup
    AP_LOC_EXIT        = 0x00020000u,  // Set if location corresponds to an exit trigger
    AP_LOC_SECRET      = 0x00040000u,  // Set if location corresponds to a secret sector
} ap_location_state_flags_t;

typedef struct {
    uint32_t state;  // State flags of the location
    ap_net_id_t item;  // Item id at the location. Only valid if state & AP_LOC_HINTED != 0
} ap_location_state_t;

extern ap_location_state_t ap_locations[AP_MAX_LOCATION];  // All location states for a shuffle

#define AP_LOCATION_CHECK_MASK(x, y) (AP_VALID_LOCATION_ID(x) && ((ap_locations[x].state & y) == y))
#define AP_VALID_LOCATION(x) (AP_LOCATION_CHECK_MASK(x, AP_LOC_USED))
#define AP_LOCATION_CHECKED(x) (AP_LOCATION_CHECK_MASK(x, (AP_LOC_USED | AP_LOC_CHECKED)))
#define AP_LOCATION_PROGRESSION(x) (AP_LOCATION_CHECK_MASK(x, (AP_LOC_USED | AP_LOC_PROGRESSION)))

typedef enum
{
    AP_UNINIT,
    AP_INITIALIZED,
    AP_CONNECTED,
    AP_CONNECTION_LOST,
} ap_init_state_t;

extern ap_init_state_t ap_global_state;

#define AP (ap_global_state > AP_UNINIT)
#define APConnected (ap_global_state == AP_CONNECTED)

extern Json::Value ap_game_config;  // Only valid if ap_global_state != AP_DISABLED

typedef enum
{
    AP_DISABLED,
    AP_SERVER,
    AP_LOCAL,
} ap_gamemode_t;

typedef struct {
    ap_gamemode_t mode;
    const char* ip;
    const char* game;
    const char* player;
    const char* password;
    const char* sp_world;
} ap_connection_settings_t;

extern void AP_Initialize(Json::Value game_config, ap_connection_settings_t connection);
extern void AP_LibShutdown(void);

/* Player state */

typedef struct {
    std::map<ap_net_id_t, uint16_t> persistent;  // Counts of all items received. Do not modify, this is the progression state!
    std::map<ap_net_id_t, uint16_t> progressive;  // Counts for each progressive item applied. Can be safely cleared when reapplying all items to keep track again
    std::vector<ap_net_id_t> ap_item_queue;  // Queue of items to be provided to the player whenever he's in-game
    Json::Value dynamic_player;  // Game specific dynamic state. This should be conserved, but contains no progression relevant information
} ap_state_t;

extern ap_state_t ap_game_state;

extern std::map<ap_net_id_t, Json::Value> ap_item_info;  // All item descriptions for a game data

extern bool AP_HasItem(ap_net_id_t id);
extern uint16_t AP_ItemCount(ap_net_id_t id);
extern uint16_t AP_ProgressiveItem(ap_net_id_t id);

extern int32_t AP_CheckLocation(ap_location_t loc);

#ifdef __cplusplus
}
#endif
