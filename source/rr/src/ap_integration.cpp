#include "ap_integration.h"
#include "names.h"
// For Numsprites and sprite
#include "build.h"
// For G_AddGroup
#include "common.h"
// For g_scriptNamePtr
#include "common_game.h"

static void ap_add_processor_sprite(void);

/*
  Patches sprites loaded from a map file based on active shuffle settings.
*/
void ap_map_patch_sprites(void)
{
    int32_t i;
	// ToDo actually do the right thing here
    for (i = 0; i < Numsprites; i++)
    {
        if (i == 561)
            sprite[i].picnum = AP_ITEM;
            sprite[i].lotag = 123;
        if (i == 562)
            sprite[i].picnum = AP_PROG;
            sprite[i].lotag = 123;
    }

    // Inject an AP_PROCESSOR sprite into the map
    ap_add_processor_sprite();
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

// ToDo read from game grp
#define DUKE3D_AP_GAME_ID (0x1)

void ap_initialize(void)
{
    AP_Init(DUKE3D_AP_GAME_ID);

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
