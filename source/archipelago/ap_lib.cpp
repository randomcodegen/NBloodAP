#include "ap_lib.h"
#include "ap_comp.h"
#include "compat.h"

ap_state_t ap_global_state = AP_DISABLED;
uint8_t ap_game_id = 0;
Json::Value ap_game_config;

ap_location_state_t ap_locations[AP_MAX_LOCATION];

static void init_location_table(Json::Value& locations)
{
    Bmemset(ap_locations, 0, AP_MAX_LOCATION * sizeof(ap_location_state_t));

    // ToDo only mark locations as used that appear in the AP seed

    // Iterate through the game config data to set the relevant flags for all known locations
    for (std::string level_name : locations.getMemberNames())
    {
        for (std::string sprite_id : locations[level_name]["sprites"].getMemberNames())
        {
            if (locations[level_name]["sprites"][sprite_id]["id"].isInt())
            {
                int location_id = locations[level_name]["sprites"][sprite_id]["id"].asInt();
                if (location_id >= 0)
                    ap_locations[AP_SHORT_LOCATION(location_id)].state |= (AP_LOC_PICKUP | AP_LOC_USED);
            }
        }
    }

    for (std::string level_name : locations.getMemberNames())
    {
        for (std::string sector_id : locations[level_name]["sectors"].getMemberNames())
        {
            if (locations[level_name]["sectors"][sector_id]["id"].isInt())
            {
                int location_id = locations[level_name]["sectors"][sector_id]["id"].asInt();
                if (location_id >= 0)
                    ap_locations[AP_SHORT_LOCATION(location_id)].state |= (AP_LOC_SECRET | AP_LOC_USED);
            }
        }
    }

    for (std::string level_name : locations.getMemberNames())
    {
        for (std::string exit_tag : locations[level_name]["exits"].getMemberNames())
        {
            if (locations[level_name]["exits"][exit_tag]["id"].isInt())
            {
                int location_id = locations[level_name]["exits"][exit_tag]["id"].asInt();
                if (location_id >= 0)
                    ap_locations[AP_SHORT_LOCATION(location_id)].state |= (AP_LOC_EXIT | AP_LOC_USED);
            }
        }
    }

}

void AP_Init(Json::Value game_config)
{
    if (game_config == NULL) return;
    ap_game_id = game_config["game_id"].asInt() & AP_GAME_ID_MASK;
    if (ap_game_id == 0) return;

    init_location_table(game_config["locations"]);

    ap_game_config = game_config;
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
