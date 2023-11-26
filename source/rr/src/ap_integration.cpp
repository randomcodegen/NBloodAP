#include "ap_integration.h"
// For Numsprites and sprite
#include "build.h"
// For G_AddGroup
#include "common.h"
// For g_scriptNamePtr
#include "common_game.h"
// For ud user defines
#include "global.h"
// For JSON features
#include <json/json.h>
#include <json/reader.h>
// For Menu manipulation
#include "menus.h"

uint8_t ap_return_to_menu = 0;

// Convenience access to player struct
#define ACTIVE_PLAYER g_player[myconnectindex].ps

static std::string item_quote_tmp;

#define AP_RECEIVE_ITEM_QUOTE 5120

static inline ap_location_t safe_location_id(Json::Value& val)
{
    if (val.isInt())
        return AP_SHORT_LOCATION(val.asInt());
    return -1;
}

// Map name in format EYLX[X]
std::string ap_format_map_id(uint8_t level_number, uint8_t volume_number)
{
    if(level_number == 7 && volume_number == 0)
    {
        // user map. Not sure we want to support this in general, but might as well do the right thing here
        return std::string(boardfilename);
    }
    std::stringstream tmp;
    tmp << "E" << volume_number + 1 << "L" << level_number + 1;
    return tmp.str();
}

static std::string current_map(void)
{
    return ap_format_map_id(ud.level_number, ud.volume_number);
}

static void ap_add_processor_sprite(void)
{
    spritetype *player, *processor;
    short       new_idx;
    player  = &sprite[0];
    new_idx = insertsprite(player->sectnum, 0);
    if (new_idx < 0 || new_idx >= MAXSPRITES)
    {
        initprintf("Injecting AP Processor sprite failed\n");
        return;
    }
    processor           = &sprite[new_idx];
    processor->cstat    = 0;
    processor->ang      = 0;
    processor->owner    = 0;
    processor->picnum   = AP_PROCESSOR;
    processor->pal      = 0;
    processor->xoffset  = 0;
    processor->yoffset  = 0;
    processor->xvel     = 0;
    processor->yvel     = 0;
    processor->zvel     = 0;
    processor->shade    = 8;
    processor->xrepeat  = 0;
    processor->yrepeat  = 0;
    processor->clipdist = 0;
    processor->extra    = 0;
}

/*
  Patches sprites loaded from a map file based on active shuffle settings.
*/
static void ap_map_patch_sprites(void)
{
    std::string map = current_map();
    Json::Value sprite_locations = ap_game_config["locations"][map]["sprites"];
    int32_t i;
    Json::Value sprite_info;
    int32_t use_sprite;
    int location_id;
    ap_location_t sprite_location;
    for (i = 0; i < Numsprites; i++)
    {
        sprite_info = sprite_locations[std::to_string(i)];
        if (!sprite_info["id"].isInt()) continue;  // Not a sprite described in the AP Game Data, ignore it
        location_id = sprite_info["id"].asInt();
        if (location_id < 0)
            use_sprite = 0;
        else
        {
            sprite_location = AP_SHORT_LOCATION(location_id);
            use_sprite  = AP_VALID_LOCATION(sprite_location);
        }
        if (use_sprite)
        {
            // Have a sprite that should become an AP Pickup
            sprite[i].lotag  = sprite_location;
            sprite[i].picnum = AP_LOCATION_PROGRESSION(sprite_location) ? AP_PROG__STATIC : AP_ITEM__STATIC;
            bitmap_set(show2dsprite, i);
        }
        else
        {
            // Unused sprite, set it up for deletion
            sprite[i].lotag  = -1;
            sprite[i].picnum = AP_ITEM__STATIC;
        }
    }

    // Inject an AP_PROCESSOR sprite into the map
    ap_add_processor_sprite();
}

#ifdef AP_DEBUG_ON
/* Helps with finding secrets for location definition */
static void print_secret_sectors()
{
    std::stringstream out;
    out << "Secret sectors for " << current_map() << ":";
    int32_t i;
    for (i = 0; i < numsectors; i++)
    {
        if (sector[i].lotag == 32767) {
            out << " " << i << ",";
        }
    }
    AP_Debugf(out.str().c_str());
}
#endif

/*
  Marks all secret sectors corresponding to already checked
  locations as already collected.
*/
static void ap_mark_known_secret_sectors(void)
{
    int32_t i;
    Json::Value cur_secret_locations = ap_game_config["locations"][current_map()]["sectors"];
    Json::Value sector_info;
    for (i = 0; i < numsectors; i++)
    {
        if (sector[i].lotag == 32767)
        {
            // Secret sector, check if it is a valid location for the AP seed and has been collected already
            sector_info = cur_secret_locations[std::to_string(i)];
            ap_location_t location_id = safe_location_id(sector_info["id"]);
            if (location_id < 0)
                continue;
            if AP_LOCATION_CHECKED(location_id)
            {
                // Already have this secret, disable the sector and mark it as collected
                sector[i].lotag = 0;
                ACTIVE_PLAYER->secret_rooms++;
                // Also increase the max secret count for this one as the sector will no longer be tallied when
                // the map is processed further
                ACTIVE_PLAYER->max_secret_rooms++;
            }
        }
    }
}

/*
  Load correct game data based on configured AP settings
*/
void ap_startup(void)
{
    // [AP] Always load our patch groups and set the main con file
    // ToDo find a better way to do this with dependency chaining?
    // Eventually this might become entirely unnecessary if we get the grp
    // handling right
    const char apgrp[19] = "ap/ARCHIPELAGO.zip";
    G_AddGroup(apgrp);
    const char customgrp[16] = "ap/DUKE3DAP.zip";
    G_AddGroup(customgrp);
    const char customcon[13] = "DUKE3DAP.CON";
    g_scriptNamePtr          = Xstrdup(customcon);

    // This quote is managed dynamically by our code, so free the pre-allocated memory
    if (apStrings[AP_RECEIVE_ITEM_QUOTE] != NULL)
    {
        Xfree(apStrings[AP_RECEIVE_ITEM_QUOTE]);
        apStrings[AP_RECEIVE_ITEM_QUOTE] = NULL;
    }
}

Json::Value read_json_from_grp(const char* filename)
{
    int kFile = kopen4loadfrommod(filename, 0);

    if (kFile == -1)  // JBF: was 0
    {
        return NULL;
    }

    int const kFileLen = kfilelength(kFile);
    char     *mptr     = (char *)Xmalloc(kFileLen + 1);
    mptr[kFileLen]     = 0;
    kread(kFile, mptr, kFileLen);
    kclose(kFile);

    Json::Value ret;
    Json::Reader reader;

    reader.parse(std::string(mptr, kFileLen), ret);
    Xfree(mptr);

    return ret;
}

std::string ap_episode_names[MAXVOLUMES] = { "" };
std::vector<uint8_t> ap_active_episodes;
std::vector<std::vector<uint8_t>> ap_active_levels;
std::map<std::string, Json::Value> ap_level_data;

void ap_parse_levels()
{
    ap_level_data.clear();
    ap_active_episodes.clear();
    ap_active_levels.clear();

    // Iterate over all episodes defined in the game config
    // JSON iteration can be out of order, use flags to keep things sequential
    Json::Value tmp_levels[MAXVOLUMES][MAXLEVELS] = { 0 };

    for (std::string ep_id : ap_game_config["episodes"].getMemberNames())
    {
        uint8_t volume = ap_game_config["episodes"][ep_id]["volumenum"].asInt();
        ap_episode_names[volume] = ap_game_config["episodes"][ep_id]["name"].asString();
        for (std::string lev_id : ap_game_config["episodes"][ep_id]["levels"].getMemberNames())
        {
            uint8_t levelnum = ap_game_config["episodes"][ep_id]["levels"][lev_id]["levelnum"].asInt();
            if (AP_VALID_LOCATION(safe_location_id(ap_game_config["episodes"][ep_id]["levels"][lev_id]["exit"])))
            {
                ap_level_data[ap_format_map_id(levelnum, volume)] = ap_game_config["episodes"][ep_id]["levels"][lev_id];
            }
        }
    }

    // Now agregate and insert in correct order
    for(int i=0; i < MAXVOLUMES; i++) {
        std::vector<uint8_t> episode_active_levels;
        for (int j = 0; j < MAXLEVELS; j++) {
            if (ap_level_data.count(ap_format_map_id(j, i))) {
                episode_active_levels.push_back(j);
            }
        }

        if (episode_active_levels.size())
        {
            ap_active_episodes.push_back(i);
        }
        ap_active_levels.push_back(episode_active_levels);
    }

}

void ap_initialize(void)
{
    Json::Value game_ap_config = read_json_from_grp("ap_config.json");

    // ToDo get from settings window/cli
    ap_connection_settings_t connection = {AP_LOCAL, "", "", "", "", "local_world.json"};

    AP_Initialize(game_ap_config, connection);

    if (AP)
    {
        // Additional initializations after the archipelago setup is done
        ap_parse_levels();
    }
}

// Safe check if an item is scoped to a level
static inline bool item_for_level(Json::Value& info, uint8_t level, uint8_t volume)
{
    return (info["levelnum"].isInt() && (info["levelnum"].asInt() == level)) && (info["volumenum"].isInt() && (info["volumenum"].asInt() == level));
}

static inline bool item_for_current_level(Json::Value& info)
{
    return item_for_level(info, ud.level_number, ud.volume_number);
}

/* Apply whatever item we just got to our current game state */
static void ap_get_item(Json::Value item_info, bool silent)
{
    silent |= item_info["silent"].asBool();
    if (!silent)
        AP_Printf(("Got Item: " + item_info["name"].asString()).c_str());
        // Ensure message gets displayed, even though it reuses the same quote id
        item_quote_tmp = "Received " + item_info["name"].asString();
        apStrings[AP_RECEIVE_ITEM_QUOTE] = (char *)item_quote_tmp.c_str();
        ACTIVE_PLAYER->ftq = 0;
        P_DoQuote(AP_RECEIVE_ITEM_QUOTE, ACTIVE_PLAYER);

    std::string item_type =item_info["type"].asString();
    // Poor man's switch
    if (item_type == "key" && item_for_current_level(item_info))
    {
        // Key is for current level, apply
        // Lower 3 bits match the flags we have on access cards
        uint8_t key_flag = item_info["flags"].asInt();
        ACTIVE_PLAYER->got_access |= (key_flag & 0x7);
        // Remaining flags are for RR keys
        for (uint8_t i = 0; i < 5; i++)
        {
            if (key_flag & (1 << (i + 2)))
                ACTIVE_PLAYER->keys[i] = 1;
        }
    }
    else if (item_type == "automap" && item_for_current_level(item_info))
    {
        // Enable all sectors
        ud.showallmap = 1;
        Bmemset(show2dsector, 0xFF, 512);
    }
}

void ap_process_event_queue(void)
{
    // Check for items in our queue to process
    while (!ap_item_queue.empty())
    {
        ap_net_id_t item_id = ap_item_queue.front();
        ap_item_queue.erase(ap_item_queue.begin());
        ap_get_item(ap_item_info[item_id], false);
    }
}

void ap_sync_inventory(void)
{
    // Apply state for all persistent items we have unlocked
    for (auto item_pair : ap_game_state.persistent)
    {
        for(unsigned int i = 0; i < item_pair.second; i++)
            ap_get_item(ap_item_info[item_pair.first], true);
    }
}

void ap_on_map_load(void)
{
    ap_map_patch_sprites();

    ap_mark_known_secret_sectors();
#ifdef AP_DEBUG_ON
    print_secret_sectors();
#endif
}

void ap_on_save_load(void)
{
    ap_mark_known_secret_sectors();
}

void ap_check_secret(int16_t sectornum)
{
    AP_CheckLocation(safe_location_id(ap_game_config["locations"][current_map()]["sectors"][std::to_string(sectornum)]["id"]));
}

void ap_level_end(void)
{
    // Return to menu after beating a level
    ACTIVE_PLAYER->gm = 0;
    Menu_Open(myconnectindex);
    Menu_Change(MENU_AP_LEVEL);
    ap_return_to_menu = 1;
}

void ap_check_exit(int16_t exitnum)
{
    ap_location_t exit_location = safe_location_id(ap_game_config["locations"][current_map()]["exits"][std::to_string(exitnum)]["id"]);
    // Might not have a secret exit defined, so in this case treat as regular exit
    if (exit_location < 0)
        exit_location = safe_location_id(ap_game_config["locations"][current_map()]["exits"][std::to_string(0)]["id"]);
    AP_CheckLocation(exit_location);
}

void ap_select_episode(uint8_t i)
{
    ud.m_volume_number = ap_active_episodes[i];
}

void ap_select_level(uint8_t i)
{
    ud.m_level_number = ap_active_levels[ud.m_volume_number][i];
}

void ap_shutdown(void)
{
    AP_LibShutdown();
    // Fix our dynamically managed quote. The underlying c_str is managed by the string variable
    apStrings[AP_RECEIVE_ITEM_QUOTE] = NULL;
}
