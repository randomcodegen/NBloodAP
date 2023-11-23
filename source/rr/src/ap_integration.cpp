#include "ap_integration.h"
#include "names.h"
// For Numsprites and sprite
#include "build.h"
// For G_AddGroup
#include "common.h"
// For g_scriptNamePtr
#include "common_game.h"

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
        if (i == 562)
            sprite[i].picnum = AP_PROG;
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
}

void ap_initialize(void)
{
    AP_Init();
}
