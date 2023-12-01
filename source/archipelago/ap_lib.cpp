#include "ap_lib.h"
#include "Archipelago.h"
#include "compat.h"
#include <chrono>
#include <thread>

ap_init_state_t ap_global_state = AP_UNINIT;
uint8_t ap_game_id = 0;
Json::Value ap_game_config;

ap_location_state_t ap_locations[AP_MAX_LOCATION];
ap_state_t ap_game_state = { 
    std::map<ap_net_id_t, uint16_t>(), 
    std::map<ap_net_id_t, uint16_t>(),
    std::vector<ap_net_id_t>(),
    Json::Value(),
    false
};
std::map<ap_net_id_t, Json::Value> ap_item_info;

std::map<std::string, Json::Value> ap_game_settings;
std::map<ap_net_id_t, uint16_t> ap_goals;
std::map<ap_net_id_t, uint8_t> ap_used_level_unlocks;

Json::Reader ap_reader;
Json::FastWriter ap_writer;

std::vector<std::string> ap_message_queue;


static void init_location_table(Json::Value& locations)
{
    Bmemset(ap_locations, 0, AP_MAX_LOCATION * sizeof(ap_location_state_t));

    // Iterate through the game config data to set the relevant flags for all known locations
    for (std::string level_name : locations.getMemberNames())
    {
        for (std::string sprite_id : locations[level_name]["sprites"].getMemberNames())
        {
            if (locations[level_name]["sprites"][sprite_id]["id"].isInt64())
            {
                int location_id = locations[level_name]["sprites"][sprite_id]["id"].asInt64();
                if (location_id >= 0)
                    ap_locations[AP_SHORT_LOCATION(location_id)].state |= (AP_LOC_PICKUP);
            }
        }
    }

    for (std::string level_name : locations.getMemberNames())
    {
        for (std::string sector_id : locations[level_name]["sectors"].getMemberNames())
        {
            if (locations[level_name]["sectors"][sector_id]["id"].isInt64())
            {
                int location_id = locations[level_name]["sectors"][sector_id]["id"].asInt64();
                if (location_id >= 0)
                    ap_locations[AP_SHORT_LOCATION(location_id)].state |= (AP_LOC_SECRET);
            }
        }
    }

    for (std::string level_name : locations.getMemberNames())
    {
        for (std::string exit_tag : locations[level_name]["exits"].getMemberNames())
        {
            if (locations[level_name]["exits"][exit_tag]["id"].isInt64())
            {
                int location_id = locations[level_name]["exits"][exit_tag]["id"].asInt64();
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
        ap_net_id_t item_id = AP_NET_ID(std::stoll(item_str));
        ap_item_info[item_id] = items[item_str];
    }
}

static void set_goals(std::string json)
{
    Json::Value goals;
    ap_reader.parse(json, goals);

    ap_goals.clear();
    for (std::string goal_str : goals.getMemberNames())
    {
        ap_net_id_t goal_id = AP_NET_ID(std::stoll(goal_str));
        ap_goals[goal_id] = goals[goal_str].asInt();
    }
}

static void set_available_locations(std::string json)
{
    Json::Value locations;
    ap_reader.parse(json, locations);

    std::vector<int64_t> scout_reqs;

    for (unsigned int i=0; i < locations.size(); i++)
    {
        ap_net_id_t location_id = locations[i].asInt64();
        ap_location_t short_id = AP_SHORT_LOCATION(location_id);
        if (AP_VALID_LOCATION_ID(short_id))
        {
            ap_locations[short_id].state |= (AP_LOC_USED);
            scout_reqs.push_back(location_id);
        }
    }
    // Send out a location scout package for them so we can see which ones are progressive
    AP_SendLocationScouts(scout_reqs, FALSE);
}

static void set_used_levels(std::string json)
{
    Json::Value levels;
    ap_reader.parse(json, levels);

    ap_used_level_unlocks.clear();
    for (unsigned int i=0; i < levels.size(); i++)
    {
        ap_used_level_unlocks[levels[i].asUInt64()] = 1;
    }
}

static std::string remote_id_checksum;

static void set_id_checksums(std::string json)
{
    Json::Value checksum;
    ap_reader.parse(json, checksum);
    remote_id_checksum = checksum.asString();
}

static void set_settings(std::string json)
{
    Json::Value settings;
    ap_reader.parse(json, settings);

    ap_game_settings.clear();
    for (std::string setting_name : settings.getMemberNames())
    {
        ap_game_settings[setting_name] = settings[setting_name];
    }
}

static void initialize_save_data(Json::Value& init_data)
{
    ap_game_state.dynamic_player.copy(init_data["player"]);
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
    AP_SendItem(AP_NET_ID(net_loc));
    ap_locations[loc].state |= AP_LOC_CHECKED;
    // And note the location check in the console
    AP_Printf(("New Check: " + AP_GetLocationName(net_loc)).c_str());
    return 1;
}

/* Library callbacks */
void AP_ClearAllItems()
{
   ap_game_state.ap_item_queue.clear();
   ap_game_state.persistent.clear();
}

void AP_ItemReceived(int64_t item_id, int slot, bool notify)
{
    if (!ap_item_info.count(item_id)) return;  // Don't know anything about this type of item, ignore it
    Json::Value item_info = ap_item_info[item_id];

    // Store counts for stateful items
    if (item_info["persistent"].asBool())
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
        ap_game_state.ap_item_queue.push_back(item_id);
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
    Json::Value save_data;
    std::string serialized_save_data;

    AP_GetServerDataRequest save_req = {
        AP_RequestStatus::Pending,
        AP_GetPrivateServerDataPrefix() + "_save_data",
        (void *)&serialized_save_data,
        AP_DataType::Raw
    };

    // Request save data from server
    AP_GetServerData(&save_req);

    // Wait for server connection and data package exchange to occur
    auto start_time = std::chrono::steady_clock::now();
    while ((AP_GetDataPackageStatus() != AP_DataPackageSyncStatus::Synced) && (save_req.status != AP_RequestStatus::Done))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (std::chrono::steady_clock::now() - start_time > std::chrono::milliseconds(timeout))
        {
            printf("AP: Failed to connect\n");
            return TRUE;
        }
    }

    // Should have the id checksum from slot data by now, verify it matches our loaded ap_config.json
    if (strcmp(ap_game_config["checksum"].asCString(), remote_id_checksum.c_str())) {
        printf("AP: Remote server item/location IDs don't match locally loaded configuration.\n");
        return TRUE;
    }

    ap_reader.parse(serialized_save_data, save_data);

    initialize_save_data(save_data);

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

/* Increments the progressive count for an item in the state and returns the current total count */
uint16_t AP_ProgressiveItem(ap_net_id_t id)
{
    uint16_t count = ap_game_state.progressive.count(id) ? ap_game_state.progressive[id] : 0;
    count++;
    ap_game_state.progressive[id] = count;
    return count;
}

void AP_Initialize(Json::Value game_config, ap_connection_settings_t connection)
{
    if (game_config == NULL || connection.mode == AP_DISABLED) return;
    ap_game_id = game_config["game_id"].asUInt() & AP_GAME_ID_MASK;
    if (ap_game_id == 0) return;

    init_location_table(game_config["locations"]);
    init_item_table(game_config["items"]);
    ap_game_config = game_config;

    if (connection.mode == AP_SERVER)
        AP_Init(connection.ip, connection.game, connection.player, connection.password);
    else if (connection.mode == AP_LOCAL)
        AP_Init(connection.sp_world);
    else
        return;
    AP_SetItemClearCallback(&AP_ClearAllItems);
    AP_SetItemRecvCallback(&AP_ItemReceived);
    AP_SetLocationCheckedCallback(&AP_ExtLocationCheck);
    AP_SetLocationInfoCallback(&AP_LocationInfo);
    AP_RegisterSlotDataRawCallback("goal", &set_goals);
    AP_RegisterSlotDataRawCallback("locations", &set_available_locations);
    AP_RegisterSlotDataRawCallback("settings", &set_settings);
    AP_RegisterSlotDataRawCallback("levels", &set_used_levels);
    AP_RegisterSlotDataRawCallback("checksum", &set_id_checksums);
    AP_Start();

    if(sync_wait_for_data(10000))
    {
        AP_Shutdown();
        // ToDo Just abort entirely?
        AP_Errorf("Could not establish connection to Server in time.");
        return;
    }

    ap_global_state = AP_INITIALIZED;
}

void AP_LibShutdown(void)
{
    AP_SyncProgress();
    AP_Shutdown();
}

void AP_SyncProgress(void)
{
    if (!ap_game_state.need_sync) return;
    // Wait until initialization is done before writing back, only then things are consistent
    if (!ap_global_state == AP_INITIALIZED) return;

    Json::Value save_data;

    save_data["player"] = ap_game_state.dynamic_player;

    std::string serialized = ap_writer.write(save_data);

    AP_SetServerDataRequest req;
    req.key = AP_GetPrivateServerDataPrefix() + "_save_data";
    AP_DataStorageOperation op = { "replace", &serialized };
    req.operations.push_back(op);
    req.type = AP_DataType::Raw;
    std::string default_value = "";
    req.default_value = &default_value;

    AP_SetServerData(&req);

    ap_game_state.need_sync = false;
}

static bool reached_goal = false;

bool AP_CheckVictory(void)
{
    if (reached_goal) return false; // Already reached victory state once
    bool all_reached = true;
    for (auto pair: ap_goals)
    {
        // Check goal count
        if (AP_ItemCount(pair.first) < pair.second)
        {
            all_reached = false;
            break;
        }
    }

    if (all_reached)
    {
        // Send victory state to AP Server
        reached_goal = true;
        AP_StoryComplete();
    }

    return reached_goal;
}

extern bool AP_IsLevelUsed(ap_net_id_t unlock_key)
{
    return ap_used_level_unlocks.count(AP_NET_ID(unlock_key));
}

void AP_QueueMessage(std::string msg)
{
    AP_Printf(msg);
    ap_message_queue.push_back(msg);
}

void AP_ProcessMessages()
{
    // Fetch remaining messages from the AP Server
    while (AP_IsMessagePending())
    {
        AP_Message* message = AP_GetLatestMessage();

        std::stringstream msgbuf;

        if (message->messageParts.empty())
        {
            msgbuf << message->text;
        }
        else
        {
            for (auto &part : message->messageParts)
            {
                switch(part.type)
                {
                case AP_LocationText:
                    msgbuf << "^8^S2";
                    break;
                case AP_ItemText:
                    msgbuf << "^7^S0";
                    break;
                case AP_PlayerText:
                    msgbuf << "^2^S4";
                    break;
                case AP_NormalText:
                default:
                    msgbuf << "^12^S0";
                    break;
                }

                msgbuf << part.text;
            }
        }

        ap_message_queue.push_back(msgbuf.str());

        AP_ClearLatestMessage();
    }
}
