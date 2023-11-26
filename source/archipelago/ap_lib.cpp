#include "ap_lib.h"
#include "ap_comp.h"
#include "compat.h"
#include <chrono>
#include <thread>

ap_init_state_t ap_global_state = AP_UNINIT;
uint8_t ap_game_id = 0;
Json::Value ap_game_config;

ap_location_state_t ap_locations[AP_MAX_LOCATION];
ap_state_t ap_game_state = { std::map<ap_net_id_t, uint16_t>() };
std::map<ap_net_id_t, Json::Value> ap_item_info;
std::vector<ap_net_id_t> ap_item_queue;

static void init_location_table(Json::Value& locations)
{
    Bmemset(ap_locations, 0, AP_MAX_LOCATION * sizeof(ap_location_state_t));

    // Iterate through the game config data to set the relevant flags for all known locations
    for (std::string level_name : locations.getMemberNames())
    {
        for (std::string sprite_id : locations[level_name]["sprites"].getMemberNames())
        {
            if (locations[level_name]["sprites"][sprite_id]["id"].isInt())
            {
                int location_id = locations[level_name]["sprites"][sprite_id]["id"].asInt();
                if (location_id >= 0)
                    ap_locations[AP_SHORT_LOCATION(location_id)].state |= (AP_LOC_PICKUP);
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
                    ap_locations[AP_SHORT_LOCATION(location_id)].state |= (AP_LOC_SECRET);
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
                    ap_locations[AP_SHORT_LOCATION(location_id)].state |= (AP_LOC_EXIT);
            }
        }
    }

}

static void init_item_table(Json::Value& items)
{
    ap_item_info.clear();
    for (std::string item_str : items.getMemberNames())
    {
        ap_net_id_t item_id = AP_NET_ID(std::stoi(item_str));
        ap_item_info[item_id] = items[item_str];
    }
}

static void mark_used_locations(void)
{
    // Get locations that are known to exist for our current game seed
    auto server_locations = AP_GetGameLocations_Compat();

    // Send out a location scout package for them so we can see which ones are progressive
    AP_SendLocationScouts_Compat(server_locations, FALSE);

    // And set the location flags
    for (int64_t location_id : server_locations)
    {
        ap_location_t short_id = AP_SHORT_LOCATION(location_id);
        if (AP_VALID_LOCATION_ID(short_id))
            ap_locations[short_id].state |= (AP_LOC_USED);
    }
}

int32_t AP_CheckLocation(ap_location_t loc)
{
    // Check if the location is even a valid id for current game
    if (!AP_VALID_LOCATION(loc))
        return 0;
    // Check if we already have this location confirmed as checked.
    if (AP_LOCATION_CHECKED(loc))
        return 0;
    // Forward check to AP server
    ap_net_id_t net_loc = AP_NET_ID(loc);
    AP_SendItem_Compat(AP_NET_ID(net_loc));
    ap_locations[loc].state |= AP_LOC_CHECKED;
    // And note the location check in the console
    AP_Printf(("New Check: " + AP_GetLocationName_Compat(net_loc)).c_str());
    return 1;
}

/* Library callbacks */
void AP_ClearAllItems()
{
  // Not sure why we would want to do this? Skip for now
}

void AP_ItemReceived(int64_t item_id, int slot, bool notify)
{
    if (!ap_item_info.count(item_id)) return;  // Don't know anything about this type of item, ignore it
    Json::Value item_info = ap_item_info[item_id];

    // Store counts for stateful items
    if (item_info["state"].asBool())
    {
        uint16_t count = 0;
        if (AP_HasItem(item_id))
        {
            count = ap_game_state.persistent[item_id];
        }
        // Increment
        count++;
        // Set to 1 if it's a unique item
        if (item_info["unique"].asBool())
            count = 1;
        ap_game_state.persistent[item_id] = count;
    }

    // If we should tell the player about the item, put it into the item queue
    if (notify)
        ap_item_queue.push_back(item_id);
}

void AP_ExtLocationCheck(int64_t location_id)
{
    ap_location_t loc = AP_SHORT_LOCATION(location_id);
    // Check if the location is even a valid id for current game
    if (!AP_VALID_LOCATION_ID(loc))
        return;
    // Check if we already have this location confirmed as checked.
    if (AP_LOCATION_CHECKED(loc))
        return;
    ap_locations[loc].state |= (AP_LOC_CHECKED);
}

void AP_LocationInfo(std::vector<AP_NetworkItem> scouted_items)
{
    // ToDo can't distinguish this from a received hint?
    // How do we decide when we can legally store the item content
    // as something the player should know about?
    for (auto item : scouted_items)
    {
        ap_location_t loc = AP_SHORT_LOCATION(item.location);
        if (!AP_VALID_LOCATION_ID(loc))
            continue;
        ap_locations[loc].item = item.item;
        // Set state based on active flags
        if (item.flags & 0b001)
            ap_locations[loc].state |= AP_LOC_PROGRESSION;
        if (item.flags & 0b010)
            ap_locations[loc].state |= AP_LOC_IMPORTANT;
        if (item.flags & 0b100)
            ap_locations[loc].state |= AP_LOC_TRAP;
    }
}

bool sync_wait_for_data(uint32_t timeout)
{
    // Wait for server connection and data package exchange to occur
    auto start_time = std::chrono::steady_clock::now();
    while (AP_GetDataPackageStatus_Compat() != AP_DataPackageSyncStatus::Synced)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (std::chrono::steady_clock::now() - start_time > std::chrono::milliseconds(timeout))
        {
            printf("AP: Failed to connect\n");
            return TRUE;
        }
    }
    return FALSE;
}

uint16_t AP_ItemCount(ap_net_id_t id)
{
    return ap_game_state.persistent.count(id) ? ap_game_state.persistent[id] : 0;
}

bool AP_HasItem(ap_net_id_t id)
{
    return AP_ItemCount(id) > 0;
}

void AP_Initialize(Json::Value game_config, ap_connection_settings_t connection)
{
    if (game_config == NULL || connection.mode != AP_LOCAL) return; // ToDo should be == AP_DISABLED, but only have local games supported for now with the comp layer
    ap_game_id = game_config["game_id"].asInt() & AP_GAME_ID_MASK;
    if (ap_game_id == 0) return;

    init_location_table(game_config["locations"]);
    init_item_table(game_config["items"]);
    ap_game_config = game_config;

    AP_Init_Compat(connection.sp_world);
    AP_SetItemClearCallback_Compat(&AP_ClearAllItems);
    AP_SetItemRecvCallback_Compat(&AP_ItemReceived);
    AP_SetLocationCheckedCallback_Compat(&AP_ExtLocationCheck);
    AP_SetLocationInfoCallback_Compat(&AP_LocationInfo);
    AP_Start_Compat();

    if(sync_wait_for_data(10000))
    {
        AP_Shutdown_Compat();
        // ToDo Just abort entirely?
        AP_Errorf("Could not establish connection to Server in time.");
        return;
    }

    mark_used_locations();

    ap_global_state = AP_INITIALIZED;
}

void AP_LibShutdown(void)
{
    AP_Shutdown_Compat();
}
