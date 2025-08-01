//-------------------------------------------------------------------------
/*
Copyright (C) 2010 EDuke32 developers and contributors

This file is part of EDuke32.

EDuke32 is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------

#ifndef savegame_h_
#define savegame_h_

#include "game.h"

#ifdef __cplusplus
extern "C" {
#endif

# define SV_MAJOR_VER 1
// [AP] Have AP Seed header now
#define SV_MINOR_VER 8

#pragma pack(push,1)
typedef struct _savehead
{
    char headerstr[11];
    uint8_t majorver, minorver, ptrsize;
    uint16_t bytever;
    // 16 bytes

    uint32_t userbytever;
    uint32_t scriptcrc;

    uint8_t comprthres;
    uint8_t recdiffsp, diffcompress, synccompress;
    // 4 bytes

    int32_t reccnt, snapsiz;
    // 8 bytes

    char savename[MAXSAVEGAMENAMESTRUCT];
    uint8_t numplayers, volnum, levnum, skill;
    char boardfn[BMAX_PATH];
    // 286 bytes
#ifdef __ANDROID__
    char skillname[32], volname[32];
#endif
    char ap_seed[128];

    uint8_t getPtrSize() const { return ptrsize & 0x7Fu; }
    bool isAutoSave() const { return !!(ptrsize & (1u<<7u)); }
} savehead_t;
#pragma pack(pop)

struct savebrief_t
{
    savebrief_t()
    {
        reset();
    }
    savebrief_t(char const *n)
    {
        strncpy(name, n, MAXSAVEGAMENAME);
        path[0] = '\0';
    }

    char name[MAXSAVEGAMENAMESTRUCT];
    char path[BMAX_PATH];

    void reset()
    {
        name[0] = '\0';
        path[0] = '\0';
    }
    bool isValid() const
    {
        return path[0] != '\0';
    }
};

struct menusave_t
{
    savebrief_t brief;
    uint8_t isOldVer = 0;
    uint8_t isUnreadable = 0;
    uint8_t isAutoSave = 0;
    void clear()
    {
      brief.reset();
      isOldVer = 0;
      isUnreadable = 0;
      isAutoSave = 0;
    }
};

extern savebrief_t g_lastautosave, g_lastusersave, g_freshload;
extern int32_t g_lastAutoSaveArbitraryID;
extern bool g_saveRequested;
extern savebrief_t * g_quickload;

extern menusave_t * g_menusaves;
extern uint16_t g_nummenusaves;

int32_t sv_updatestate(int32_t frominit);
int32_t sv_readdiff(int32_t fil);
uint32_t sv_writediff(FILE *fil);
int32_t sv_loadheader(int32_t fil, int32_t spot, savehead_t *h);
int32_t sv_loadsnapshot(int32_t fil, int32_t spot, savehead_t *h);
int32_t sv_saveandmakesnapshot(FILE *fil, char const *name, int8_t spot, int8_t recdiffsp, int8_t diffcompress, int8_t synccompress, bool isAutoSave = false);
void sv_freemem();
void G_DeleteSave(savebrief_t const & sv);
void G_DeleteOldSaves(void);
uint16_t G_CountOldSaves(void);
int32_t G_SavePlayer(savebrief_t & sv, bool isAutoSave);
int32_t G_LoadPlayer(savebrief_t & sv);
int32_t G_LoadSaveHeaderNew(char const *fn, savehead_t *saveh);
void ReadSaveGameHeaders(void);
void G_SavePlayerMaybeMulti(savebrief_t & sv, bool isAutoSave = false);
int32_t G_LoadPlayerMaybeMulti(savebrief_t & sv);

#ifdef YAX_ENABLE
extern void sv_postyaxload(void);
#endif

// XXX: The 'bitptr' decl really belongs into gamedef.h, but we don't want to
// pull all of it in savegame.c?
extern char *bitptr;

enum
{
    P2I_BACK_BIT = 1,
    P2I_ONLYNON0_BIT = 2,

    P2I_FWD = 0,
    P2I_BACK = 1,

    P2I_FWD_NON0 = 0+2,
    P2I_BACK_NON0 = 1+2,
};
void G_Util_PtrToIdx(void *ptr, int32_t count, const void *base, int32_t mode);
void G_Util_PtrToIdx2(void *ptr, int32_t count, size_t stride, const void *base, int32_t const mode);

#ifdef __cplusplus
}
#endif

#endif
