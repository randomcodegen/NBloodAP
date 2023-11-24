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

static void ap_add_processor_sprite(void);


// Current map name in format eXlY
static std::string current_map(void)
{
    if(ud.m_level_number == 7 && ud.m_volume_number == 0)
    { 
        // user map. Not sure we want to support this in general, but might as well do the right thing here
        return std::string(boardfilename);
    }
    std::stringstream tmp;
    tmp << "e" << ud.volume_number + 1 << "l" << ud.level_number + 1;
    return tmp.str();
}

/*
  Patches sprites loaded from a map file based on active shuffle settings.
*/
static void ap_map_patch_sprites(void)
{
    std::string map = current_map();
    Json::Value sprite_locations = ap_game_config["locations"][map]["sprites"];
    int32_t i;
    for (i = 0; i < Numsprites; i++)
    {
        Json::Value sprite_info = sprite_locations[std::to_string(i)];
        if (!sprite_info["id"].isInt()) continue;
        ap_location_t sprite_location = AP_SHORT_LOCATION(sprite_info["id"].asInt());
        if (!AP_VALID_LOCATION(sprite_location)) continue;
        // Have a sprite that should become an AP Item
        sprite[i].lotag  = sprite_location;
        sprite[i].picnum = AP_LOCATION_PROGRESSION(sprite_location) ? AP_PROG__STATIC : AP_ITEM__STATIC;
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
    // ToDo find good way to print this
    OSD_Printf(out.str().c_str());
}
#endif

void ap_on_map_load(void)
{
    ap_map_patch_sprites();
#ifdef AP_DEBUG_ON
    print_secret_sectors();
#endif
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
}

static Json::Value read_json_from_grp(const char* filename)
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

void ap_initialize(void)
{
    Json::Value game_ap_config = read_json_from_grp("ap_config.json");

    AP_Init(game_ap_config);

    // Fixed data for used locations for now
    ap_locations[123].state = AP_LOC_PROGRESSION | AP_LOC_USED | AP_LOC_SCOUTED;
}

void ap_process_event_queue(void)
{

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
    processor = &sprite[new_idx];
    processor->cstat = 0;
    processor->ang   = 0;
    processor->owner = 0;
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
