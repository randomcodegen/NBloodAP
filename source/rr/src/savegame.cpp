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

#include "duke3d.h"
#include "premap.h"
#include "prlights.h"
#include "savegame.h"
#include "ap_integration.h"
#include "Archipelago.h"

static OutputFileCounter savecounter;

// For storing pointers in files.
//  back_p==0: ptr -> "small int"
//  back_p==1: "small int" -> ptr
//
//  mode: see enum in savegame.h
void G_Util_PtrToIdx(void *ptr, int32_t const count, const void *base, int32_t const mode)
{
    intptr_t *iptr = (intptr_t *)ptr;
    intptr_t const ibase = (intptr_t)base;
    int32_t const onlynon0_p = mode&P2I_ONLYNON0_BIT;

    // TODO: convert to proper offsets/indices for (a step towards) cross-
    //       compatibility between 32- and 64-bit systems in the netplay.
    //       REMEMBER to bump BYTEVERSION then.

    // WARNING: C std doesn't say that bit pattern of NULL is necessarily 0!
    if ((mode & P2I_BACK_BIT) == 0)
    {
        for (bssize_t i = 0; i < count; i++)
            if (!onlynon0_p || iptr[i])
                iptr[i] -= ibase;
    }
    else
    {
        for (bssize_t i = 0; i < count; i++)
            if (!onlynon0_p || iptr[i])
                iptr[i] += ibase;
    }
}

void G_Util_PtrToIdx2(void *ptr, int32_t const count, size_t const stride, const void *base, int32_t const mode)
{
    uint8_t *iptr = (uint8_t *)ptr;
    intptr_t const ibase = (intptr_t)base;
    int32_t const onlynon0_p = mode&P2I_ONLYNON0_BIT;

    if ((mode & P2I_BACK_BIT) == 0)
    {
        for (bssize_t i = 0; i < count; ++i)
        {
            if (!onlynon0_p || *(intptr_t *)iptr)
                *(intptr_t *)iptr -= ibase;

            iptr += stride;
        }
    }
    else
    {
        for (bssize_t i = 0; i < count; ++i)
        {
            if (!onlynon0_p || *(intptr_t *)iptr)
                *(intptr_t *)iptr += ibase;

            iptr += stride;
        }
    }
}

// TODO: sync with TROR special interpolations? (e.g. upper floor of subway)
void G_ResetInterpolations(void)
{
    int32_t k, i;

    g_interpolationCnt = 0;

    k = headspritestat[STAT_EFFECTOR];
    while (k >= 0)
    {
        switch (sprite[k].lotag)
        {
        case SE_31_FLOOR_RISE_FALL:
            G_SetInterpolation(&sector[sprite[k].sectnum].floorz);
            break;
        case SE_32_CEILING_RISE_FALL:
            G_SetInterpolation(&sector[sprite[k].sectnum].ceilingz);
            break;
        case SE_17_WARP_ELEVATOR:
        case SE_25_PISTON:
            G_SetInterpolation(&sector[sprite[k].sectnum].floorz);
            G_SetInterpolation(&sector[sprite[k].sectnum].ceilingz);
            break;
        case SE_0_ROTATING_SECTOR:
        case SE_5:
        case SE_6_SUBWAY:
        case SE_11_SWINGING_DOOR:
        case SE_14_SUBWAY_CAR:
        case SE_15_SLIDING_DOOR:
        case SE_16_REACTOR:
        case SE_26:
        case SE_30_TWO_WAY_TRAIN:
            Sect_SetInterpolation(sprite[k].sectnum);
            if (REALITY)
                RT_MS_SetInterpolation(sprite[k].sectnum);
            break;
        }

        k = nextspritestat[k];
    }

    for (i=g_interpolationCnt-1; i>=0; i--) bakipos[i] = *curipos[i];
    for (i = g_animateCnt-1; i>=0; i--)
        G_SetInterpolation(g_animatePtr[i]);
}

savebrief_t g_lastautosave, g_lastusersave, g_freshload;
int32_t g_lastAutoSaveArbitraryID = -1;
bool g_saveRequested;
savebrief_t * g_quickload;

menusave_t * g_menusaves;
uint16_t g_nummenusaves;

static menusave_t * g_internalsaves;
static uint16_t g_numinternalsaves;

static void ReadSaveGameHeaders_CACHE1D(BUILDVFS_FIND_REC *f)
{
    savehead_t h;

    for (; f != nullptr; f = f->next)
    {
        char const * fn = f->name;
        int32_t fil = kopen4loadfrommod(fn, 0);
        if (fil == -1)
            continue;

        menusave_t & msv = g_internalsaves[g_numinternalsaves];

        int32_t k = sv_loadheader(fil, 0, &h);
        if (k)
        {
            if (k < 0)
                msv.isUnreadable = 1;
            msv.isOldVer = 1;
        }
        else
            msv.isOldVer = 0;

        msv.isAutoSave = h.isAutoSave();

        strncpy(msv.brief.path, fn, ARRAY_SIZE(msv.brief.path));
        ++g_numinternalsaves;

        if (k >= 0 && h.savename[0] != '\0')
        {
            memcpy(msv.brief.name, h.savename, ARRAY_SIZE(msv.brief.name));
        }
        else
            msv.isUnreadable = 1;

        kclose(fil);
    }
}

static int countcache1dfind(BUILDVFS_FIND_REC *f)
{
    int x = 0;
    for (; f != nullptr; f = f->next)
        ++x;
    return x;
}

static void ReadSaveGameHeaders_Internal(void)
{
    static char const DefaultPath[] = "/", SavePattern[] = "*.esv";

    BUILDVFS_FIND_REC *findfiles_default = klistpath(DefaultPath, SavePattern, BUILDVFS_FIND_FILE);

    // potentially overallocating but programmatically simple
    int const numfiles = countcache1dfind(findfiles_default);
    size_t const internalsavesize = sizeof(menusave_t) * numfiles;

    g_internalsaves = (menusave_t *)Xrealloc(g_internalsaves, internalsavesize);

    for (int x = 0; x < numfiles; ++x)
        g_internalsaves[x].clear();

    g_numinternalsaves = 0;
    ReadSaveGameHeaders_CACHE1D(findfiles_default);
    klistfree(findfiles_default);

    g_nummenusaves = 0;
    for (int x = g_numinternalsaves-1; x >= 0; --x)
    {
        menusave_t & msv = g_internalsaves[x];
        // [AP] Don't show saves for other seeds
        if (!msv.isUnreadable && !msv.isOldVer)
        {
            ++g_nummenusaves;
        }
    }
    size_t const menusavesize = sizeof(menusave_t) * g_nummenusaves;

    g_menusaves = (menusave_t *)Xrealloc(g_menusaves, menusavesize);
    
    for (int x = 0; x < g_nummenusaves; ++x)
        g_menusaves[x].clear();
    
    for (int x = g_numinternalsaves-1, y = 0; x >= 0; --x)
    {
        menusave_t & msv = g_internalsaves[x];
        if (!msv.isUnreadable && !msv.isOldVer)
        {
            g_menusaves[y++] = msv;
        }
    }
    
    for (int x = g_numinternalsaves-1; x >= 0; --x)
    {
        char const * const path = g_internalsaves[x].brief.path;
        int const pathlen = Bstrlen(path);
        if (pathlen < 12)
            continue;
        char const * const fn = path + (pathlen-12);
        if (fn[0] == 's' && fn[1] == 'a' && fn[2] == 'v' && fn[3] == 'e' &&
            isdigit(fn[4]) && isdigit(fn[5]) && isdigit(fn[6]) && isdigit(fn[7]))
        {
            char number[5];
            memcpy(number, fn+4, 4);
            number[4] = '\0';
            savecounter.count = Batoi(number)+1;
            break;
        }
    }
}

void ReadSaveGameHeaders(void)
{
    ReadSaveGameHeaders_Internal();

    if (!ud.autosavedeletion)
        return;

    bool didDelete = false;
    int numautosaves = 0;
    for (int x = 0; x < g_nummenusaves; ++x)
    {
        menusave_t & msv = g_menusaves[x];
        if (!msv.isAutoSave)
            continue;
        if (numautosaves >= ud.maxautosaves)
        {
            G_DeleteSave(msv.brief);
            didDelete = true;
        }
        ++numautosaves;
    }

    if (didDelete)
        ReadSaveGameHeaders_Internal();
}

int32_t G_LoadSaveHeaderNew(char const *fn, savehead_t *saveh)
{
    int32_t fil = kopen4loadfrommod(fn, 0);
    if (fil == -1)
        return -1;

    int32_t i = sv_loadheader(fil, 0, saveh);
    if (i < 0)
        goto corrupt;

    int32_t screenshotofs;
    if (kread(fil, &screenshotofs, 4) != 4)
        goto corrupt;

    walock[TILE_LOADSHOT] = 255;
    if (waloff[TILE_LOADSHOT] == 0)
        g_cache.allocateBlock(&waloff[TILE_LOADSHOT], 320*200, &walock[TILE_LOADSHOT]);
    tilesiz[TILE_LOADSHOT].x = 200;
    tilesiz[TILE_LOADSHOT].y = 320;
    if (screenshotofs)
    {
        if (kdfread_LZ4((char *)waloff[TILE_LOADSHOT], 320, 200, fil) != 200)
        {
            OSD_Printf("G_LoadSaveHeaderNew(): failed reading screenshot in \"%s\"\n", fn);
            goto corrupt;
        }
    }
    else
    {
        Bmemset((char *)waloff[TILE_LOADSHOT], 0, 320*200);
    }
    tileInvalidate(TILE_LOADSHOT, 0, 255);

    kclose(fil);
    return 0;

corrupt:
    kclose(fil);
    return 1;
}


static void sv_postudload();

// hack
static int different_user_map;

// XXX: keyboard input 'blocked' after load fail? (at least ESC?)
int32_t G_LoadPlayer(savebrief_t & sv)
{
    // [AP] Check if saving is allowed
    if (!ap_can_save()) return -1;

    int const fil = kopen4loadfrommod(sv.path, 0);

    if (fil == -1)
        return -1;

    ready2send = 0;

    savehead_t h;
    int status = sv_loadheader(fil, 0, &h);

    if (status < 0 || h.numplayers != ud.multimode)
    {
        if (status == -4 || status == -3 || status == 1)
            P_DoQuote(QUOTE_SAVE_BAD_VERSION, g_player[myconnectindex].ps);
        else if (h.numplayers != ud.multimode)
            P_DoQuote(QUOTE_SAVE_BAD_PLAYERS, g_player[myconnectindex].ps);

        kclose(fil);
        ototalclock = totalclock;
        ready2send = 1;

        return 1;
    }
    // [AP] Committed to loading a save file, store player data now
    ap_store_dynamic_player_data();

    // some setup first
    ud.multimode = h.numplayers;

    if (numplayers > 1)
    {
        pub = NUMPAGES;
        pus = NUMPAGES;
        G_UpdateScreenArea();
        G_DrawBackground();
        menutext_center(100, "Loading...");
        videoNextPage();
    }

    Net_WaitForEverybody();

    FX_StopAllSounds();
    S_ClearSoundLocks();

    // non-"m_" fields will be loaded from svgm_udnetw
    ud.m_volume_number = h.volnum;
    ud.m_level_number = h.levnum;
    ud.m_player_skill = h.skill;

    // NOTE: Bmemcpy needed for SAVEGAME_MUSIC.
    EDUKE32_STATIC_ASSERT(sizeof(boardfilename) == sizeof(h.boardfn));
    different_user_map = Bstrcmp(boardfilename, h.boardfn);
    Bmemcpy(boardfilename, h.boardfn, sizeof(boardfilename));

    int const mapIdx = h.volnum*MAXLEVELS + h.levnum;

    if (boardfilename[0])
        Bstrcpy(currentboardfilename, boardfilename);
    else if (g_mapInfo[mapIdx].filename)
        Bstrcpy(currentboardfilename, g_mapInfo[mapIdx].filename);

    if (currentboardfilename[0])
    {
        artSetupMapArt(currentboardfilename);
        append_ext_UNSAFE(currentboardfilename, ".mhk");
        engineLoadMHK(currentboardfilename);
    }

    Bmemcpy(currentboardfilename, boardfilename, BMAX_PATH);

    if (status == 2)
        G_NewGame_EnterLevel();
    else if ((status = sv_loadsnapshot(fil, 0, &h)))  // read the rest...
    {
        // in theory, we could load into an initial dump first and trivially
        // recover if things go wrong...
        Bsprintf(tempbuf, "Loading save game file \"%s\" failed (code %d), cannot recover.", sv.path, status);
        G_GameExit(tempbuf);
    }

    sv_postudload();  // ud.m_XXX = ud.XXX
    kclose(fil);

    return 0;
}

////////// TIMER SAVING/RESTORING //////////

static struct {
    int32_t totalclock, totalclocklock;  // engine
    int32_t ototalclock, lockclock;  // game
} g_timers;

static void G_SaveTimers(void)
{
    g_timers.totalclock     = (int32_t) totalclock;
    g_timers.totalclocklock = (int32_t) totalclocklock;
    g_timers.ototalclock    = (int32_t) ototalclock;
    g_timers.lockclock      = (int32_t) lockclock;
}

static void G_RestoreTimers(void)
{
    totalclock     = g_timers.totalclock;
    totalclocklock = g_timers.totalclocklock;
    ototalclock    = g_timers.ototalclock;
    lockclock      = g_timers.lockclock;
}

//////////

#ifdef __ANDROID__
static void G_SavePalette(void)
{
    int32_t pfil;

    if ((pfil = kopen4load("palette.dat", 0)) != -1)
    {
        int len = kfilelength(pfil);

        FILE *fil = fopen("palette.dat", "rb");

        if (!fil)
        {
            fil = fopen("palette.dat", "wb");

            if (fil)
            {
                char *buf = (char *) Xaligned_alloc(16, len);

                kread(pfil, buf, len);
                fwrite(buf, len, 1, fil);
                fclose(fil);

                Xfree(buf);
            }
        }
        else fclose(fil);
    }
}
#endif

void G_DeleteSave(savebrief_t const & sv)
{
    if (!sv.isValid())
        return;

    char temp[BMAX_PATH];

    if (G_ModDirSnprintf(temp, sizeof(temp), "%s", sv.path))
    {
        OSD_Printf("G_SavePlayer: file name \"%s\" too long\n", sv.path);
        return;
    }

    unlink(temp);
}

void G_DeleteOldSaves(void)
{
    ReadSaveGameHeaders();

    for (int x = 0; x < g_numinternalsaves; ++x)
    {
        menusave_t const & msv = g_internalsaves[x];
        if (msv.isOldVer || msv.isUnreadable)
            G_DeleteSave(msv.brief);
    }
}

uint16_t G_CountOldSaves(void)
{
    ReadSaveGameHeaders();

    int bad = 0;
    for (int x = 0; x < g_numinternalsaves; ++x)
    {
        menusave_t const & msv = g_internalsaves[x];
        if (msv.isOldVer || msv.isUnreadable)
            ++bad;
    }

    return bad;
}

int32_t G_SavePlayer(savebrief_t & sv, bool isAutoSave)
{
#ifdef __ANDROID__
    G_SavePalette();
#endif

    // [AP] Check if saving is allowed at all
    if (!ap_can_save())
        return -1;

    G_SaveTimers();

    Net_WaitForEverybody();
    ready2send = 0;

    char temp[BMAX_PATH];

    errno = 0;
    FILE *fil;

    if (sv.isValid())
    {
        if (G_ModDirSnprintf(temp, sizeof(temp), "%s", sv.path))
        {
            OSD_Printf("G_SavePlayer: file name \"%s\" too long\n", sv.path);
            goto saveproblem;
        }
        fil = fopen(temp, "wb");
    }
    else
    {
        static char const SaveName[] = "save0000.esv";
        int const len = G_ModDirSnprintfLite(temp, ARRAY_SIZE(temp), SaveName);
        if (len >= ARRAY_SSIZE(temp)-1)
        {
            OSD_Printf("G_SavePlayer: could not form automatic save path\n");
            goto saveproblem;
        }
        char * zeros = temp + (len-8);
        fil = savecounter.opennextfile(temp, zeros);
        savecounter.count++;
        // don't copy the mod dir into sv.path
        Bstrcpy(sv.path, temp + (len-(ARRAY_SIZE(SaveName)-1)));
    }

    if (!fil)
    {
        OSD_Printf("G_SavePlayer: failed opening \"%s\" for writing: %s\n",
                   temp, strerror(errno));
        goto saveproblem;
    }

    // temporary hack
    ud.user_map = G_HaveUserMap();

#ifdef POLYMER
    if (videoGetRenderMode() == REND_POLYMER)
        polymer_resetlights();
#endif

    // SAVE!
    sv_saveandmakesnapshot(fil, sv.name, 0, 0, 0, 0, isAutoSave);

    fclose(fil);

    if (!g_netServer && ud.multimode < 2)
    {
            Bstrcpy(apStrings[QUOTE_RESERVED4], "Game Saved");
        P_DoQuote(QUOTE_RESERVED4, g_player[myconnectindex].ps);
    }

    ready2send = 1;
    Net_WaitForEverybody();

    G_RestoreTimers();
    ototalclock = totalclock;

    return 0;

saveproblem:
    ready2send = 1;
    Net_WaitForEverybody();

    G_RestoreTimers();
    ototalclock = totalclock;

    return -1;
}

int32_t G_LoadPlayerMaybeMulti(savebrief_t & sv)
{
    if (g_netServer || ud.multimode > 1)
    {
        Bstrcpy(apStrings[QUOTE_RESERVED4], "Multiplayer Loading Not Yet Supported");
        P_DoQuote(QUOTE_RESERVED4, g_player[myconnectindex].ps);

//        g_player[myconnectindex].ps->gm = MODE_GAME;
        return 127;
    }
    else
    {
        int32_t c = G_LoadPlayer(sv);
        if (c == 0)
            g_player[myconnectindex].ps->gm = MODE_GAME;
        return c;
    }
}

void G_SavePlayerMaybeMulti(savebrief_t & sv, bool isAutoSave)
{
    CONFIG_WriteSetup(2);

    if (g_netServer || ud.multimode > 1)
    {
        Bstrcpy(apStrings[QUOTE_RESERVED4], "Multiplayer Saving Not Yet Supported");
        P_DoQuote(QUOTE_RESERVED4, g_player[myconnectindex].ps);
    }
    else
    {
        G_SavePlayer(sv, isAutoSave);
    }
}

////////// GENERIC SAVING/LOADING SYSTEM //////////

typedef struct dataspec_
{
    uint32_t flags;
    void * const ptr;
    uint32_t size;
    intptr_t cnt;
} dataspec_t;

typedef struct dataspec_gv_
{
    uint32_t flags;
    void * ptr;
    uint32_t size;
    intptr_t cnt;
} dataspec_gv_t;

#define SV_DEFAULTCOMPRTHRES 8
static uint8_t savegame_diffcompress;  // 0:none, 1:Ken's LZW in cache1d.c
static uint8_t savegame_comprthres;


#define DS_DYNAMIC 1  // dereference .ptr one more time
#define DS_STRING 2
#define DS_CMP 4
// 8
#define DS_CNT(x) ((sizeof(x))<<3)  // .cnt is pointer to...
#define DS_CNT16 16
#define DS_CNT32 32
#define DS_CNTMASK (8|DS_CNT16|DS_CNT32|64)
// 64
#define DS_LOADFN 128  // .ptr is function that is run when loading
#define DS_SAVEFN 256  // .ptr is function that is run when saving
#define DS_NOCHK 1024  // don't check for diffs (and don't write out in dump) since assumed constant throughout demo
#define DS_PROTECTFN 512
#define DS_END (0x70000000)

static int32_t ds_getcnt(const dataspec_t *spec)
{
    int cnt = -1;

    switch (spec->flags & DS_CNTMASK)
    {
        case 0: cnt = spec->cnt; break;
        case DS_CNT16: cnt = *((int16_t *)spec->cnt); break;
        case DS_CNT32: cnt = *((int32_t *)spec->cnt); break;
    }

    return cnt;
}

static inline void ds_get(const dataspec_t *spec, void **ptr, int32_t *cnt)
{
    *cnt = ds_getcnt(spec);
    *ptr = (spec->flags & DS_DYNAMIC) ? *((void **)spec->ptr) : spec->ptr;
}

// write state to file and/or to dump
static uint8_t *writespecdata(const dataspec_t *spec, FILE *fil, uint8_t *dump)
{
    for (; spec->flags != DS_END; spec++)
    {
        if (spec->flags & (DS_SAVEFN|DS_LOADFN))
        {
            if (spec->flags & DS_SAVEFN)
                (*(void (*)(void))spec->ptr)();
            continue;
        }

        if (!fil && (spec->flags & (DS_NOCHK|DS_CMP|DS_STRING)))
            continue;
        else if (spec->flags & DS_STRING)
        {
            fwrite(spec->ptr, Bstrlen((const char *)spec->ptr), 1, fil);  // not null-terminated!
            continue;
        }

        void *  ptr;
        int32_t cnt;

        ds_get(spec, &ptr, &cnt);

        if (cnt < 0)
        {
            OSD_Printf("wsd: cnt=%d, f=0x%x.\n", cnt, spec->flags);
            continue;
        }

        if (!ptr || !cnt)
            continue;

        if (fil)
        {
            if ((spec->flags & DS_CMP) || ((spec->flags & DS_CNTMASK) == 0 && spec->size * cnt <= savegame_comprthres))
                fwrite(ptr, spec->size, cnt, fil);
            else
                dfwrite_LZ4((void *)ptr, spec->size, cnt, fil);
        }

        if (dump && (spec->flags & (DS_NOCHK|DS_CMP)) == 0)
        {
            Bmemcpy(dump, ptr, spec->size * cnt);
            dump += spec->size * cnt;
        }
    }
    return dump;
}

// let havedump=dumpvar&&*dumpvar
// (fil>=0 && havedump): first restore dump from file, then restore state from dump
// (fil<0 && havedump): only restore state from dump
// (fil>=0 && !havedump): only restore state from file
static int32_t readspecdata(const dataspec_t *spec, int32_t fil, uint8_t **dumpvar)
{
    uint8_t *  dump = dumpvar ? *dumpvar : NULL;
    auto const sptr = spec;

    for (; spec->flags != DS_END; spec++)
    {
        if (fil < 0 && spec->flags & (DS_NOCHK|DS_STRING|DS_CMP))  // we're updating
            continue;

        if (spec->flags & (DS_LOADFN|DS_SAVEFN))
        {
            if (spec->flags & DS_LOADFN)
                (*(void (*)())spec->ptr)();
            continue;
        }

        if (spec->flags & (DS_STRING|DS_CMP))  // DS_STRING and DS_CMP is for static data only
        {
            static char cmpstrbuf[32];
            int const siz  = (spec->flags & DS_STRING) ? Bstrlen((const char *)spec->ptr) : spec->size * spec->cnt;
            int const ksiz = kread(fil, cmpstrbuf, siz);

            if (ksiz != siz || Bmemcmp(spec->ptr, cmpstrbuf, siz))
            {
                OSD_Printf("rsd: spec=%s, idx=%d:\n", (char *)sptr->ptr, (int32_t)(spec-sptr));

                if (ksiz!=siz)
                    OSD_Printf("    kread returned %d, expected %d.\n", ksiz, siz);
                else
                    OSD_Printf("    sp->ptr and cmpstrbuf not identical!\n");

                return -1;
            }
            continue;
        }

        void *  ptr;
        int32_t cnt;

        ds_get(spec, &ptr, &cnt);

        if (cnt < 0)
        {
            OSD_Printf("rsd: cnt<0... wtf?\n");
            return -1;
        }

        if (!ptr || !cnt)
            continue;

        if (fil >= 0)
        {
            auto const mem  = (dump && (spec->flags & DS_NOCHK) == 0) ? dump : (uint8_t *)ptr;
            bool const comp = !((spec->flags & DS_CNTMASK) == 0 && spec->size * cnt <= savegame_comprthres);
            int const  siz  = comp ? cnt : cnt * spec->size;
            int const  ksiz = comp ? kdfread_LZ4(mem, spec->size, siz, fil) : kread(fil, mem, siz);

            if (ksiz != siz)
            {
                OSD_Printf("rsd: spec=%s, idx=%d, mem=%p\n", (char *)sptr->ptr, (int32_t)(spec - sptr), mem);
                OSD_Printf("     (%s): read %d, expected %d!\n",
                           ((spec->flags & DS_CNTMASK) == 0 && spec->size * cnt <= savegame_comprthres) ? "uncompressed" : "compressed", ksiz, siz);

                if (ksiz == -1)
                    OSD_Printf("     read: %s\n", strerror(errno));

                return -1;
            }
        }

        if (dump && (spec->flags & DS_NOCHK) == 0)
        {
            Bmemcpy(ptr, dump, spec->size * cnt);
            dump += spec->size * cnt;
        }
    }

    if (dumpvar)
        *dumpvar = dump;

    return 0;
}

#define UINT(bits) uint##bits##_t
#define BYTES(bits) (bits>>3)
#define VAL(bits,p) (*(UINT(bits) const *)(p))
#define WVAL(bits,p) (*(UINT(bits) *)(p))

static void docmpsd(const void *ptr, void *dump, uint32_t size, uint32_t cnt, uint8_t **diffvar)
{
    uint8_t *retdiff = *diffvar;

    // Hail to the C preprocessor, baby!
#define CPSINGLEVAL(Datbits)                                              \
    if (VAL(Datbits, ptr) != VAL(Datbits, dump))                          \
    {                                                                     \
        WVAL(Datbits, retdiff) = WVAL(Datbits, dump) = VAL(Datbits, ptr); \
        *diffvar = retdiff + BYTES(Datbits);                              \
    }

    if (cnt == 1)
        switch (size)
        {
        case 8: CPSINGLEVAL(64); return;
        case 4: CPSINGLEVAL(32); return;
        case 2: CPSINGLEVAL(16); return;
        case 1: CPSINGLEVAL(8); return;
        }

#define CPELTS(Idxbits, Datbits)             \
    do                                       \
    {                                        \
        for (int i = 0; i < nelts; i++)      \
        {                                    \
            if (*p != *op)                   \
            {                                \
                *op = *p;                    \
                WVAL(Idxbits, retdiff) = i;  \
                retdiff += BYTES(Idxbits);   \
                WVAL(Datbits, retdiff) = *p; \
                retdiff += BYTES(Datbits);   \
            }                                \
            p++;                             \
            op++;                            \
        }                                    \
        WVAL(Idxbits, retdiff) = -1;         \
        retdiff += BYTES(Idxbits);           \
    } while (0)

#define CPDATA(Datbits)                                                  \
    do                                                                   \
    {                                                                    \
        auto p     = (UINT(Datbits) const *)ptr;                         \
        auto op    = (UINT(Datbits) *)dump;                              \
        int  nelts = tabledivide32_noinline(size * cnt, BYTES(Datbits)); \
        if (nelts > 65536)                                               \
            CPELTS(32, Datbits);                                         \
        else if (nelts > 256)                                            \
            CPELTS(16, Datbits);                                         \
        else                                                             \
            CPELTS(8, Datbits);                                          \
    } while (0)

    if (size == 8)
        CPDATA(64);
    else if ((size & 3) == 0)
        CPDATA(32);
    else if ((size & 1) == 0)
        CPDATA(16);
    else
        CPDATA(8);

    *diffvar = retdiff;

#undef CPELTS
#undef CPSINGLEVAL
#undef CPDATA
}

// get the number of elements to be monitored for changes
static int32_t getnumvar(const dataspec_t *spec)
{
    int n = 0;
    for (; spec->flags != DS_END; spec++)
        if (spec->flags & (DS_STRING|DS_CMP|DS_NOCHK|DS_SAVEFN|DS_LOADFN))
            ++n;
    return n;
}

// update dump at *dumpvar with new state and write diff to *diffvar
static void cmpspecdata(const dataspec_t *spec, uint8_t **dumpvar, uint8_t **diffvar)
{
    uint8_t * dump   = *dumpvar;
    uint8_t * diff   = *diffvar;
    int       nbytes = (getnumvar(spec) + 7) >> 3;
    int const slen   = Bstrlen((const char *)spec->ptr);

    Bmemcpy(diff, spec->ptr, slen);
    diff += slen;

    while (nbytes--)
        *(diff++) = 0;  // the bitmap of indices which elements of spec have changed go here

    int eltnum = 0;

    for (spec++; spec->flags!=DS_END; spec++)
    {
        if ((spec->flags&(DS_NOCHK|DS_STRING|DS_CMP)))
            continue;

        if (spec->flags&(DS_LOADFN|DS_SAVEFN))
        {
            if ((spec->flags&(DS_PROTECTFN))==0)
                (*(void (*)())spec->ptr)();
            continue;
        }

        void *  ptr;
        int32_t cnt;

        ds_get(spec, &ptr, &cnt);

        if (cnt < 0)
        {
            OSD_Printf("csd: cnt=%d, f=0x%x\n", cnt, spec->flags);
            continue;
        }

        uint8_t * const tmptr = diff;

        docmpsd(ptr, dump, spec->size, cnt, &diff);

        if (diff != tmptr)
            (*diffvar + slen)[eltnum>>3] |= 1<<(eltnum&7);

        dump += spec->size*cnt;
        eltnum++;
    }

    *diffvar = diff;
    *dumpvar = dump;
}

#define VALOFS(bits,p,ofs) (*(((UINT(bits) *)(p)) + (ofs)))

// apply diff to dump, not to state! state is restored from dump afterwards.
static int32_t applydiff(const dataspec_t *spec, uint8_t **dumpvar, uint8_t **diffvar)
{
    uint8_t * dump   = *dumpvar;
    uint8_t * diff   = *diffvar;
    int const nbytes = (getnumvar(spec)+7)>>3;
    int const slen   = Bstrlen((const char *)spec->ptr);

    if (Bmemcmp(diff, spec->ptr, slen))  // check STRING magic (sync check)
        return 1;

    diff += slen+nbytes;

    int eltnum = -1;
    for (spec++; spec->flags != DS_END; spec++)
    {
        if ((spec->flags & (DS_NOCHK|DS_STRING|DS_CMP|DS_LOADFN|DS_SAVEFN)))
            continue;

        int const cnt = ds_getcnt(spec);
        if (cnt < 0) return 1;

        eltnum++;
        if (((*diffvar+slen)[eltnum>>3] & (1<<(eltnum&7))) == 0)
        {
            dump += spec->size * cnt;
            continue;
        }

// ----------
#define CPSINGLEVAL(Datbits)                  \
    WVAL(Datbits, dump) = VAL(Datbits, diff); \
    diff += BYTES(Datbits);                   \
    dump += BYTES(Datbits)

        if (cnt == 1)
        {
            switch (spec->size)
            {
                case 8: CPSINGLEVAL(64); continue;
                case 4: CPSINGLEVAL(32); continue;
                case 2: CPSINGLEVAL(16); continue;
                case 1: CPSINGLEVAL(8); continue;
            }
        }

#define CPELTS(Idxbits, Datbits)                             \
    do                                                       \
    {                                                        \
        UINT(Idxbits) idx;                                   \
        goto readidx_##Idxbits##_##Datbits;                  \
        do                                                   \
        {                                                    \
            VALOFS(Datbits, dump, idx) = VAL(Datbits, diff); \
            diff += BYTES(Datbits);                          \
readidx_##Idxbits##_##Datbits:                               \
            idx = VAL(Idxbits, diff);                        \
            diff += BYTES(Idxbits);                          \
        } while ((int##Idxbits##_t)idx != -1);               \
    } while (0)

#define CPDATA(Datbits)                                                            \
    do                                                                             \
    {                                                                              \
        int const elts = tabledivide32_noinline(spec->size * cnt, BYTES(Datbits)); \
        if (elts > 65536)                                                          \
            CPELTS(32, Datbits);                                                   \
        else if (elts > 256)                                                       \
            CPELTS(16, Datbits);                                                   \
        else                                                                       \
            CPELTS(8, Datbits);                                                    \
    } while (0)

        if (spec->size == 8)
            CPDATA(64);
        else if ((spec->size & 3) == 0)
            CPDATA(32);
        else if ((spec->size & 1) == 0)
            CPDATA(16);
        else
            CPDATA(8);
        dump += spec->size * cnt;
// ----------

#undef CPELTS
#undef CPSINGLEVAL
#undef CPDATA
    }

    *diffvar = diff;
    *dumpvar = dump;
    return 0;
}

#undef VAL
#undef VALOFS
#undef BYTES
#undef UINT

// calculate size needed for dump
static uint32_t calcsz(const dataspec_t *spec)
{
    uint32_t dasiz = 0;

    for (; spec->flags != DS_END; spec++)
    {
        // DS_STRINGs are used as sync checks in the diffs but not in the dump
        if ((spec->flags & (DS_CMP|DS_NOCHK|DS_SAVEFN|DS_LOADFN|DS_STRING)))
            continue;

        int const cnt = ds_getcnt(spec);

        if (cnt <= 0)
            continue;

        dasiz += cnt * spec->size;
    }

    return dasiz;
}

#ifdef USE_OPENGL
static void sv_prespriteextsave();
static void sv_postspriteext();
#endif
static void sv_postactordata();
static void sv_preanimateptrsave();
static void sv_postanimateptr();
static void sv_prequote();
static void sv_quotesave();
static void sv_quoteload();
static void sv_restsave();
static void sv_restload();
static void sv_rrrafog();
static void sv_preloaddn64();

#define SVARDATALEN \
    ((sizeof(g_player[0].user_name)+sizeof(g_player[0].pcolor)+sizeof(g_player[0].pteam) \
      +sizeof(g_player[0].frags)+sizeof(DukePlayer_t))*MAXPLAYERS)

static uint8_t savegame_quotedef[MAXQUOTES>>3];
static char (*savegame_quotes)[MAXQUOTELEN];
static uint8_t savegame_restdata[SVARDATALEN];

static char svgm_udnetw_string [] = "blK:udnt";
static const dataspec_t svgm_udnetw[] =
{
    { DS_STRING, (void *)svgm_udnetw_string, 0, 1 },
    { 0, &ud.multimode, sizeof(ud.multimode), 1 },
    { 0, &g_playerSpawnCnt, sizeof(g_playerSpawnCnt), 1 },
    { 0, &g_playerSpawnPoints, sizeof(g_playerSpawnPoints), 1 },

    { DS_NOCHK, &ud.volume_number, sizeof(ud.volume_number), 1 },
    { DS_NOCHK, &ud.level_number, sizeof(ud.level_number), 1 },
    { DS_NOCHK, &ud.user_map, sizeof(ud.user_map), 1 },
    { DS_NOCHK, &ud.player_skill, sizeof(ud.player_skill), 1 },
    { DS_NOCHK, &ud.music_episode, sizeof(ud.music_episode), 1 },
    { DS_NOCHK, &ud.music_level, sizeof(ud.music_level), 1 },

    { DS_NOCHK, &ud.from_bonus, sizeof(ud.from_bonus), 1 },
    { DS_NOCHK, &ud.secretlevel, sizeof(ud.secretlevel), 1 },
    { DS_NOCHK, &ud.respawn_monsters, sizeof(ud.respawn_monsters), 1 },
    { DS_NOCHK, &ud.respawn_items, sizeof(ud.respawn_items), 1 },
    { DS_NOCHK, &ud.respawn_inventory, sizeof(ud.respawn_inventory), 1 },
    { 0, &ud.god, sizeof(ud.god), 1 },
    { 0, &ud.auto_run, sizeof(ud.auto_run), 1 },
//    { DS_NOCHK, &ud.crosshair, sizeof(ud.crosshair), 1 },
    { DS_NOCHK, &ud.monsters_off, sizeof(ud.monsters_off), 1 },
    { DS_NOCHK, &ud.last_level, sizeof(ud.last_level), 1 },
    { 0, &ud.eog, sizeof(ud.eog), 1 },
    { DS_NOCHK, &ud.coop, sizeof(ud.coop), 1 },
    { DS_NOCHK, &ud.marker, sizeof(ud.marker), 1 },
    { DS_NOCHK, &ud.ffire, sizeof(ud.ffire), 1 },
    { DS_NOCHK, &ud.noexits, sizeof(ud.noexits), 1 },
    { DS_NOCHK, &ud.playerai, sizeof(ud.playerai), 1 },
    { 0, &ud.pause_on, sizeof(ud.pause_on), 1 },
    { DS_NOCHK, &currentboardfilename[0], BMAX_PATH, 1 },
//    { DS_LOADFN, (void *)&sv_postudload, 0, 1 },
    { 0, connectpoint2, sizeof(connectpoint2), 1 },
    { 0, &randomseed, sizeof(randomseed), 1 },
    { 0, &g_globalRandom, sizeof(g_globalRandom), 1 },
//    { 0, &lockclock_dummy, sizeof(lockclock), 1 },
    { DS_NOCHK, &rt_boardnum, sizeof(rt_boardnum), 1 },
    { DS_NOCHK, &rt_levelnum, sizeof(rt_levelnum), 1 },
    { DS_END, 0, 0, 0 }
};

#if !defined DEBUG_MAIN_ARRAYS
# define DS_MAINAR DS_DYNAMIC
#else
# define DS_MAINAR 0
#endif

static char svgm_secwsp_string [] = "blK:swsp";
static const dataspec_t svgm_secwsp[] =
{
    { DS_STRING, (void *)svgm_secwsp_string, 0, 1 },
    { DS_NOCHK, &numwalls, sizeof(numwalls), 1 },
    { DS_MAINAR|DS_CNT(numwalls), &wall, sizeof(walltype), (intptr_t)&numwalls },
    { DS_NOCHK, &numsectors, sizeof(numsectors), 1 },
    { DS_MAINAR|DS_CNT(numsectors), &sector, sizeof(sectortype), (intptr_t)&numsectors },
    { DS_MAINAR, &sprite, sizeof(spritetype), MAXSPRITES },
#ifdef YAX_ENABLE
    { DS_NOCHK, &numyaxbunches, sizeof(numyaxbunches), 1 },
# if !defined NEW_MAP_FORMAT
    { DS_CNT(numsectors), yax_bunchnum, sizeof(yax_bunchnum[0]), (intptr_t)&numsectors },
    { DS_CNT(numwalls), yax_nextwall, sizeof(yax_nextwall[0]), (intptr_t)&numwalls },
# endif
    { DS_LOADFN|DS_PROTECTFN, (void *)&sv_postyaxload, 0, 1 },
#endif
    { 0, &Numsprites, sizeof(Numsprites), 1 },
    { 0, &tailspritefree, sizeof(tailspritefree), 1 },
    { 0, &headspritesect[0], sizeof(headspritesect[0]), MAXSECTORS+1 },
    { 0, &prevspritesect[0], sizeof(prevspritesect[0]), MAXSPRITES },
    { 0, &nextspritesect[0], sizeof(nextspritesect[0]), MAXSPRITES },
    { 0, &headspritestat[0], sizeof(headspritestat[0]), MAXSTATUS+1 },
    { 0, &prevspritestat[0], sizeof(prevspritestat[0]), MAXSPRITES },
    { 0, &nextspritestat[0], sizeof(nextspritestat[0]), MAXSPRITES },
#ifdef USE_OPENGL
    { DS_SAVEFN, (void *)&sv_prespriteextsave, 0, 1 },
#endif
    { DS_MAINAR, &spriteext, sizeof(spriteext_t), MAXSPRITES },
#ifndef NEW_MAP_FORMAT
    { DS_MAINAR, &wallext, sizeof(wallext_t), MAXWALLS },
#endif
#ifdef USE_OPENGL
    { DS_SAVEFN|DS_LOADFN, (void *)&sv_postspriteext, 0, 1 },
#endif
    { DS_NOCHK, &g_cyclerCnt, sizeof(g_cyclerCnt), 1 },
    { DS_CNT(g_cyclerCnt), &g_cyclers[0][0], sizeof(g_cyclers[0]), (intptr_t)&g_cyclerCnt },
    { DS_NOCHK, &g_animWallCnt, sizeof(g_animWallCnt), 1 },
    { DS_CNT(g_animWallCnt), &animwall, sizeof(animwall[0]), (intptr_t)&g_animWallCnt },
    { DS_NOCHK, &g_mirrorCount, sizeof(g_mirrorCount), 1 },
    { DS_NOCHK, &g_mirrorWall[0], sizeof(g_mirrorWall[0]), ARRAY_SIZE(g_mirrorWall) },
    { DS_NOCHK, &g_mirrorSector[0], sizeof(g_mirrorSector[0]), ARRAY_SIZE(g_mirrorSector) },
    { 0, &everyothertime, sizeof(everyothertime), 1 },

    { DS_END, 0, 0, 0 }
};

static char svgm_dn64_string [] = "blK:dn64";
static const dataspec_t svgm_dn64[] =
{
    { DS_STRING, (void *)svgm_dn64_string, 0, 1 },
    { DS_NOCHK, &rt_vtxnum, sizeof(rt_vtxnum), 1 },
    { DS_LOADFN, (void *)sv_preloaddn64, 0, 1 },
    { DS_DYNAMIC|DS_CNT(rt_vtxnum), &rt_sectvtx, sizeof(rt_vertex_t), (intptr_t)&rt_vtxnum },
    { DS_DYNAMIC|DS_CNT(numwalls), &rt_wall, sizeof(rt_walltype), (intptr_t)&numwalls },
    { DS_DYNAMIC|DS_CNT(numsectors), &rt_sector, sizeof(rt_sectortype), (intptr_t)&numsectors },
    { DS_NOCHK, &rt_sky_color[0], sizeof(rt_sky_color[0]), ARRAY_SIZE(rt_sky_color) },
    { DS_NOCHK, &ms_list_cnt, sizeof(ms_list_cnt), 1 },
    { DS_NOCHK, &ms_vtx_cnt, sizeof(ms_vtx_cnt), 1 },
    { DS_NOCHK, &ms_list[0], sizeof(ms_list[0]), MOVESECTNUM },
    { DS_NOCHK, &ms_listvtxptr[0], sizeof(ms_listvtxptr[0]), MOVESECTNUM },
    { DS_NOCHK, &ms_dx[0], sizeof(ms_dx[0]), MOVESECTVTXNUM },
    { DS_NOCHK, &ms_dy[0], sizeof(ms_dy[0]), MOVESECTVTXNUM },
    { 0, &ms_vx[0], sizeof(ms_vx[0]), MOVESECTVTXNUM },
    { 0, &ms_vy[0], sizeof(ms_vy[0]), MOVESECTVTXNUM },
    { 0, &explosions[0], sizeof(explosions[0]), MAXEXPLOSIONS },
    { 0, &smoke[0], sizeof(smoke[0]), MAXEXPLOSIONS },
    { 0, &boss2seq, sizeof(boss2seq), 1 },
    { 0, &boss2seqframe, sizeof(boss2seqframe), 1 },
    { 0, &boss2mdlstate, sizeof(boss2mdlstate), 1 },
    { 0, &boss2mdlstate2, sizeof(boss2mdlstate2), 1 },
    { 0, &boss2timer_step, sizeof(boss2timer_step), 1 },
    { 0, &boss2_frame, sizeof(boss2_frame), 1 },
    { 0, &boss2_frame2, sizeof(boss2_frame2), 1 },
    { 0, &boss2timer, sizeof(boss2timer), 1 },
    { 0, &boss2_interp, sizeof(boss2_interp), 1 },

    { DS_END, 0, 0, 0 }
};

static char svgm_script_string [] = "blK:scri";
static const dataspec_t svgm_script[] =
{
    { DS_STRING, (void *)svgm_script_string, 0, 1 },
    { 0, &actor[0], sizeof(actor_t), MAXSPRITES },
    { DS_SAVEFN|DS_LOADFN, (void *)&sv_postactordata, 0, 1 },
    { DS_END, 0, 0, 0 }
};

static char svgm_anmisc_string [] = "blK:anms";
static char svgm_end_string [] = "savegame_end";

static const dataspec_t svgm_anmisc[] =
{
    { DS_STRING, (void *)svgm_anmisc_string, 0, 1 },
    { 0, &g_animateCnt, sizeof(g_animateCnt), 1 },
    { 0, &g_animateSect[0], sizeof(g_animateSect[0]), MAXANIMATES },
    { 0, &g_animateGoal[0], sizeof(g_animateGoal[0]), MAXANIMATES },
    { 0, &g_animateVel[0], sizeof(g_animateVel[0]), MAXANIMATES },
    { DS_SAVEFN, (void *)&sv_preanimateptrsave, 0, 1 },
    { 0, &g_animatePtr[0], sizeof(g_animatePtr[0]), MAXANIMATES },
    { DS_SAVEFN|DS_LOADFN , (void *)&sv_postanimateptr, 0, 1 },
    { 0, &g_curViewscreen, sizeof(g_curViewscreen), 1 },
    { 0, &g_origins[0], sizeof(g_origins[0]), ARRAY_SIZE(g_origins) },
    { 0, &g_spriteDeleteQueuePos, sizeof(g_spriteDeleteQueuePos), 1 },
    { DS_NOCHK, &g_deleteQueueSize, sizeof(g_deleteQueueSize), 1 },
    { DS_CNT(g_deleteQueueSize), &SpriteDeletionQueue[0], sizeof(int16_t), (intptr_t)&g_deleteQueueSize },
    { 0, &show2dsector[0], sizeof(uint8_t), MAXSECTORS>>3 },
    { DS_NOCHK, &g_cloudCnt, sizeof(g_cloudCnt), 1 },
    { 0, &g_cloudSect[0], sizeof(g_cloudSect), 1 },
    { 0, &g_cloudX, sizeof(g_cloudX), 1 },
    { 0, &g_cloudY, sizeof(g_cloudY), 1 },
    { 0, &g_pskyidx, sizeof(g_pskyidx), 1 },  // DS_NOCHK?
    { 0, &g_earthquakeTime, sizeof(g_earthquakeTime), 1 },

    // RR stuff
    { 0, &g_spriteExtra[0], sizeof(g_spriteExtra[0]), MAXSPRITES },
    { 0, &g_sectorExtra[0], sizeof(g_sectorExtra[0]), MAXSECTORS },

    { 0, &g_jailDoorSecHitag[0], sizeof(g_jailDoorSecHitag[0]), ARRAY_SIZE(g_jailDoorSecHitag) },
    { 0, &g_jailDoorSect[0], sizeof(g_jailDoorSect[0]), ARRAY_SIZE(g_jailDoorSect) },
    { 0, &g_jailDoorOpen[0], sizeof(g_jailDoorOpen[0]), ARRAY_SIZE(g_jailDoorOpen) },
    { 0, &g_jailDoorDir[0], sizeof(g_jailDoorDir[0]), ARRAY_SIZE(g_jailDoorDir) },
    { 0, &g_jailDoorDrag[0], sizeof(g_jailDoorDrag[0]), ARRAY_SIZE(g_jailDoorDrag) },
    { 0, &g_jailDoorDist[0], sizeof(g_jailDoorDist[0]), ARRAY_SIZE(g_jailDoorDist) },
    { 0, &g_jailDoorSpeed[0], sizeof(g_jailDoorSpeed[0]), ARRAY_SIZE(g_jailDoorSpeed) },
    { 0, &g_jailDoorSound[0], sizeof(g_jailDoorSound[0]), ARRAY_SIZE(g_jailDoorSound) },
    { 0, &g_jailDoorCnt, sizeof(g_jailDoorCnt), 1 },

    { 0, &g_shadedSector[0], sizeof(g_shadedSector[0]), MAXSECTORS },

    { 0, &g_mineCartSect[0], sizeof(g_mineCartSect[0]), ARRAY_SIZE(g_mineCartSect) },
    { 0, &g_mineCartChildSect[0], sizeof(g_mineCartChildSect[0]), ARRAY_SIZE(g_mineCartChildSect) },
    { 0, &g_mineCartOpen[0], sizeof(g_mineCartOpen[0]), ARRAY_SIZE(g_mineCartOpen) },
    { 0, &g_mineCartDir[0], sizeof(g_mineCartDir[0]), ARRAY_SIZE(g_mineCartDir) },
    { 0, &g_mineCartDrag[0], sizeof(g_mineCartDrag[0]), ARRAY_SIZE(g_mineCartDrag) },
    { 0, &g_mineCartDist[0], sizeof(g_mineCartDist[0]), ARRAY_SIZE(g_mineCartDist) },
    { 0, &g_mineCartSpeed[0], sizeof(g_mineCartSpeed[0]), ARRAY_SIZE(g_mineCartSpeed) },
    { 0, &g_mineCartSound[0], sizeof(g_mineCartSound[0]), ARRAY_SIZE(g_mineCartSound) },
    { 0, &g_mineCartCnt, sizeof(g_mineCartCnt), 1 },

    { 0, &g_ambientCnt, sizeof(g_ambientCnt), 1 },
    { 0, &g_ambientHitag[0], sizeof(g_ambientHitag[0]), ARRAY_SIZE(g_ambientHitag) },
    { 0, &g_ambientLotag[0], sizeof(g_ambientLotag[0]), ARRAY_SIZE(g_ambientLotag) },
    
    { 0, &g_ufoSpawn, sizeof(g_ufoSpawn), 1 },
    { 0, &g_ufoCnt, sizeof(g_ufoCnt), 1 },
    { 0, &g_hulkSpawn, sizeof(g_hulkSpawn), 1 },
    { 0, &g_lastLevel, sizeof(g_lastLevel), 1 },

    { 0, &g_torchSector[0], sizeof(g_torchSector[0]), ARRAY_SIZE(g_torchSector) },
    { 0, &g_torchSectorShade[0], sizeof(g_torchSectorShade[0]), ARRAY_SIZE(g_torchSectorShade) },
    { 0, &g_torchType[0], sizeof(g_torchType[0]), ARRAY_SIZE(g_torchType) },
    { 0, &g_torchCnt, sizeof(g_torchCnt), 1 },

    { 0, &g_lightninSector[0], sizeof(g_lightninSector[0]), ARRAY_SIZE(g_lightninSector) },
    { 0, &g_lightninSectorShade[0], sizeof(g_lightninSectorShade[0]), ARRAY_SIZE(g_lightninSectorShade) },
    { 0, &g_lightninCnt, sizeof(g_lightninCnt), 1 },

    { 0, &g_geoSector[0], sizeof(g_geoSector[0]), ARRAY_SIZE(g_geoSector) },
    { 0, &g_geoSectorWarp[0], sizeof(g_geoSectorWarp[0]), ARRAY_SIZE(g_geoSectorWarp) },
    { 0, &g_geoSectorX[0], sizeof(g_geoSectorX[0]), ARRAY_SIZE(g_geoSectorX) },
    { 0, &g_geoSectorY[0], sizeof(g_geoSectorY[0]), ARRAY_SIZE(g_geoSectorY) },
    { 0, &g_geoSectorWarp2[0], sizeof(g_geoSectorWarp2[0]), ARRAY_SIZE(g_geoSectorWarp2) },
    { 0, &g_geoSectorX2[0], sizeof(g_geoSectorX2[0]), ARRAY_SIZE(g_geoSectorX2) },
    { 0, &g_geoSectorY2[0], sizeof(g_geoSectorY2[0]), ARRAY_SIZE(g_geoSectorY2) },
    { 0, &g_geoSectorCnt, sizeof(g_geoSectorCnt), 1 },

    { 0, &g_windTime, sizeof(g_windTime), 1 },
    { 0, &g_windDir, sizeof(g_windDir), 1 },
    { 0, &g_fakeBubbaCnt, sizeof(g_fakeBubbaCnt), 1 },
    { 0, &g_mamaSpawnCnt, sizeof(g_mamaSpawnCnt), 1 },
    { 0, &g_banjoSong, sizeof(g_banjoSong), 1 },
    { 0, &g_bellTime, sizeof(g_bellTime), 1 },
    { 0, &g_bellSprite, sizeof(g_bellSprite), 1 },

    { 0, &g_changeEnemySize, sizeof(g_changeEnemySize), 1 },
    { 0, &g_slotWin, sizeof(g_slotWin), 1 },
    { 0, &g_ufoSpawnMinion, sizeof(g_ufoSpawnMinion), 1 },
    { 0, &g_pistonSound, sizeof(g_pistonSound), 1 },
    { 0, &g_chickenWeaponTimer, sizeof(g_chickenWeaponTimer), 1 },
    { 0, &g_RAendLevel, sizeof(g_RAendLevel), 1 },
    { 0, &g_RAendEpisode, sizeof(g_RAendEpisode), 1 },
    { 0, &g_fogType, sizeof(g_fogType), 1 },
    { DS_LOADFN, (void *)sv_rrrafog, 0, 1 },

    { DS_SAVEFN|DS_LOADFN|DS_NOCHK, (void *)sv_prequote, 0, 1 },
    { DS_SAVEFN, (void *)&sv_quotesave, 0, 1 },
    { DS_NOCHK, &savegame_quotedef, sizeof(savegame_quotedef), 1 },  // quotes can change during runtime, but new quote numbers cannot be allocated
    { DS_DYNAMIC, &savegame_quotes, MAXQUOTELEN, MAXQUOTES },
    { DS_LOADFN, (void *)&sv_quoteload, 0, 1 },

    { DS_SAVEFN, (void *)&sv_restsave, 0, 1 },
    { 0, savegame_restdata, 1, sizeof(savegame_restdata) },  // sz/cnt swapped for kdfread
    { DS_LOADFN, (void *)&sv_restload, 0, 1 },

    { DS_STRING, (void *)svgm_end_string, 0, 1 },
    { DS_END, 0, 0, 0 }
};

static dataspec_gv_t *svgm_vars=NULL;
static uint8_t *dosaveplayer2(FILE *fil, uint8_t *mem);
static int32_t doloadplayer2(int32_t fil, uint8_t **memptr);
static void postloadplayer(int32_t savegamep);

// SVGM snapshot system
static uint32_t svsnapsiz;
static uint8_t *svsnapshot;
static uint8_t *svinitsnap;
static uint32_t svdiffsiz;
static uint8_t *svdiff;

#include "gamedef.h"

#define SV_SKIPMASK (/*GAMEVAR_SYSTEM|*/ GAMEVAR_READONLY | GAMEVAR_PTR_MASK | /*GAMEVAR_NORESET |*/ GAMEVAR_SPECIAL)

static char svgm_vars_string [] = "blK:vars";
// setup gamevar data spec for snapshotting and diffing... gamevars must be loaded when called
static void sv_makevarspec()
{
    int vcnt = 0;

    for (int i = 0; i < g_gameVarCount; i++)
        vcnt += (aGameVars[i].flags & SV_SKIPMASK) ? 0 : 1;

    svgm_vars = (dataspec_gv_t *)Xrealloc(svgm_vars, (vcnt + 2) * sizeof(dataspec_gv_t));

    svgm_vars[0].flags = DS_STRING;
    svgm_vars[0].ptr   = svgm_vars_string;
    svgm_vars[0].cnt   = 1;

    vcnt = 1;

    for (int i = 0; i < g_gameVarCount; i++)
    {
        if (aGameVars[i].flags & SV_SKIPMASK)
            continue;

        unsigned const per = aGameVars[i].flags & GAMEVAR_USER_MASK;

        svgm_vars[vcnt].flags = 0;
        svgm_vars[vcnt].ptr   = (per == 0) ? &aGameVars[i].global : aGameVars[i].pValues;
        svgm_vars[vcnt].size  = sizeof(intptr_t);
        svgm_vars[vcnt].cnt   = (per == 0) ? 1 : (per == GAMEVAR_PERPLAYER ? MAXPLAYERS : MAXSPRITES);

        ++vcnt;
    }

    svgm_vars[vcnt].flags = DS_END;
    svgm_vars[vcnt].ptr   = NULL;
    svgm_vars[vcnt].size  = 0;
    svgm_vars[vcnt].cnt   = 0;
}

void sv_freemem()
{
    DO_FREE_AND_NULL(svsnapshot);
    DO_FREE_AND_NULL(svinitsnap);
    DO_FREE_AND_NULL(svdiff);
}

static void SV_AllocSnap(int32_t allocinit)
{
    sv_freemem();

    svsnapshot = (uint8_t *)Xmalloc(svsnapsiz);
    if (allocinit)
        svinitsnap = (uint8_t *)Xmalloc(svsnapsiz);
    svdiffsiz = svsnapsiz;  // theoretically it's less than could be needed in the worst case, but practically it's overkill
    svdiff = (uint8_t *)Xmalloc(svdiffsiz);
}

// make snapshot only if spot < 0 (demo)
int32_t sv_saveandmakesnapshot(FILE *fil, char const *name, int8_t spot, int8_t recdiffsp, int8_t diffcompress, int8_t synccompress, bool isAutoSave)
{
    savehead_t h;

    // set a few savegame system globals
    savegame_comprthres = SV_DEFAULTCOMPRTHRES;
    savegame_diffcompress = diffcompress;

    // calculate total snapshot size
    sv_makevarspec();
    svsnapsiz = calcsz((const dataspec_t *)svgm_vars);
    svsnapsiz += calcsz(svgm_udnetw) + calcsz(svgm_secwsp) + calcsz(svgm_script) + calcsz(svgm_anmisc);
    if (REALITY)
        svsnapsiz += calcsz(svgm_dn64);


    // create header
    Bmemcpy(h.headerstr, "E32SAVEGAME", 11);
    h.majorver = SV_MAJOR_VER;
    h.minorver = SV_MINOR_VER;
    h.ptrsize  = sizeof(intptr_t);

    if (isAutoSave)
        h.ptrsize |= 1u << 7u;

    h.bytever      = BYTEVERSION;
    h.userbytever  = ud.userbytever;
    h.scriptcrc    = g_scriptcrc;
    h.comprthres   = savegame_comprthres;
    h.recdiffsp    = recdiffsp;
    h.diffcompress = savegame_diffcompress;
    h.synccompress = synccompress;

    h.reccnt  = 0;
    h.snapsiz = svsnapsiz;

    // the following is kinda redundant, but we save it here to be able to quickly fetch
    // it in a savegame header read

    h.numplayers = ud.multimode;
    h.volnum     = ud.volume_number;
    h.levnum     = ud.level_number;
    h.skill      = ud.player_skill;

    // [AP] Store seed name in header for identification
    if (AP)
        Bstrncpy(h.ap_seed, AP_GetPrivateServerDataPrefix().c_str(), 128);
    else
        Bmemset(h.ap_seed, 0, 128);

    const uint32_t BSZ = sizeof(h.boardfn);
    EDUKE32_STATIC_ASSERT(BSZ == sizeof(currentboardfilename));
    Bstrncpy(h.boardfn, currentboardfilename, BSZ);

    if (spot >= 0)
    {
        // savegame
        Bstrncpyz(h.savename, name, sizeof(h.savename));
#ifdef __ANDROID__
        Bstrncpyz(h.volname, g_volumeNames[ud.volume_number], sizeof(h.volname));
        Bstrncpyz(h.skillname, g_skillNames[ud.player_skill], sizeof(h.skillname));
#endif
    }
    else
    {
        // demo

        const time_t t = time(NULL);
        struct tm *  st;

        Bstrncpyz(h.savename, "EDuke32 demo", sizeof(h.savename));
        if (t>=0 && (st = localtime(&t)))
            Bsnprintf(h.savename, sizeof(h.savename), "Demo %04d%02d%02d %s",
                      st->tm_year+1900, st->tm_mon+1, st->tm_mday, s_buildRev);
    }


    // write header
    fwrite(&h, sizeof(savehead_t), 1, fil);

    // for savegames, the file offset after the screenshot goes here;
    // for demos, we keep it 0 to signify that we didn't save one
    fwrite("\0\0\0\0", 4, 1, fil);
    if (spot >= 0 && waloff[TILE_SAVESHOT])
    {
        int32_t ofs;

        // write the screenshot compressed
        dfwrite_LZ4((char *)waloff[TILE_SAVESHOT], 320, 200, fil);

        // write the current file offset right after the header
        ofs = ftell(fil);
        fseek(fil, sizeof(savehead_t), SEEK_SET);
        fwrite(&ofs, 4, 1, fil);
        fseek(fil, ofs, SEEK_SET);
    }

#ifdef DEBUGGINGAIDS
    OSD_Printf("sv_saveandmakesnapshot: snapshot size: %d bytes.\n", svsnapsiz);
#endif

    if (spot >= 0)
    {
        // savegame
        dosaveplayer2(fil, NULL);
    }
    else
    {
        // demo
        SV_AllocSnap(0);

        uint8_t * const p = dosaveplayer2(fil, svsnapshot);

        if (p != svsnapshot+svsnapsiz)
        {
            OSD_Printf("sv_saveandmakesnapshot: ptr-(snapshot end)=%d!\n", (int32_t)(p - (svsnapshot + svsnapsiz)));
            return 1;
        }
    }

    return 0;
}

// if file is not an EDuke32 savegame/demo, h->headerstr will be all zeros
int32_t sv_loadheader(int32_t fil, int32_t spot, savehead_t *h)
{
    int32_t havedemo = (spot < 0);

    if (kread(fil, h, sizeof(savehead_t)) != sizeof(savehead_t))
    {
        OSD_Printf("%s %d header corrupt.\n", havedemo ? "Demo":"Savegame", havedemo ? -spot : spot);
        Bmemset(h->headerstr, 0, sizeof(h->headerstr));
        return -1;
    }

    if (Bmemcmp(h->headerstr, "E32SAVEGAME", 11)
#if 1
        && Bmemcmp(h->headerstr, "EDuke32SAVE", 11)
#endif
       )
    {
        char headerCstr[sizeof(h->headerstr) + 1];
        Bmemcpy(headerCstr, h->headerstr, sizeof(h->headerstr));
        headerCstr[sizeof(h->headerstr)] = '\0';
        OSD_Printf("%s %d header reads \"%s\", expected \"E32SAVEGAME\".\n",
                   havedemo ? "Demo":"Savegame", havedemo ? -spot : spot, headerCstr);
        Bmemset(h->headerstr, 0, sizeof(h->headerstr));
        return -2;
    }

    if (h->majorver != SV_MAJOR_VER || h->minorver != SV_MINOR_VER || h->bytever != BYTEVERSION || h->userbytever != ud.userbytever || (apScript != NULL && h->scriptcrc != g_scriptcrc))
    {
#ifndef DEBUGGINGAIDS
        if (havedemo)
#endif
            OSD_Printf("Incompatible file version. Expected %d.%d.%d.%d.%0x, found %d.%d.%d.%d.%0x\n", SV_MAJOR_VER, SV_MINOR_VER, BYTEVERSION,
                       ud.userbytever, g_scriptcrc, h->majorver, h->minorver, h->bytever, h->userbytever, h->scriptcrc);

        if (h->majorver == SV_MAJOR_VER && h->minorver == SV_MINOR_VER)
        {
            return 1;
        }
        else
        {
            Bmemset(h->headerstr, 0, sizeof(h->headerstr));
            return -3;
        }
    }

    // [AP] Check if save matches loaded AP Seed
    if (Bstrncmp(h->ap_seed, AP ? AP_GetPrivateServerDataPrefix().c_str() : "", 128) != 0)
    {
        OSD_Printf("Savegame is for a different AP Seed.");
        return -2;
    }

    if (h->getPtrSize() != sizeof(intptr_t))
    {
#ifndef DEBUGGINGAIDS
        if (havedemo)
#endif
            OSD_Printf("File incompatible. Expected pointer size %d, found %d\n",
                       (int32_t)sizeof(intptr_t), h->getPtrSize());

        Bmemset(h->headerstr, 0, sizeof(h->headerstr));
        return -4;
    }

    return 0;
}

int32_t sv_loadsnapshot(int32_t fil, int32_t spot, savehead_t *h)
{
    uint8_t *p;
    int32_t i;

    if (spot < 0)
    {
        // demo
        i = sv_loadheader(fil, spot, h);
        if (i)
            return i;

        // Used to be in doloadplayer2(), now redundant for savegames since
        // we checked before. Multiplayer demos need still to be taken care of.
        if (h->numplayers != numplayers)
            return 9;
    }
    // else (if savegame), we just read the header and are now at offset sizeof(savehead_t)

#ifdef DEBUGGINGAIDS
    OSD_Printf("sv_loadsnapshot: snapshot size: %d bytes.\n", h->snapsiz);
#endif

    if (kread(fil, &i, 4) != 4)
    {
        OSD_Printf("sv_snapshot: couldn't read 4 bytes after header.\n");
        return 7;
    }
    if (i > 0)
    {
        if (klseek(fil, i, SEEK_SET) != i)
        {
            OSD_Printf("sv_snapshot: failed skipping over the screenshot.\n");
            return 8;
        }
    }

    savegame_comprthres = h->comprthres;

    if (spot >= 0)
    {
        // savegame
        i = doloadplayer2(fil, NULL);
        if (i)
        {
            OSD_Printf("sv_loadsnapshot: doloadplayer2() returned %d.\n", i);
            return 5;
        }
    }
    else
    {
        // demo
        savegame_diffcompress = h->diffcompress;

        svsnapsiz = h->snapsiz;

        SV_AllocSnap(1);

        p = svsnapshot;
        i = doloadplayer2(fil, &p);
        if (i)
        {
            OSD_Printf("sv_loadsnapshot: doloadplayer2() returned %d.\n", i);
            sv_freemem();
            return 5;
        }

        if (p != svsnapshot+svsnapsiz)
        {
            OSD_Printf("sv_loadsnapshot: internal error: p-(snapshot end)=%d!\n",
                       (int32_t)(p-(svsnapshot+svsnapsiz)));
            sv_freemem();
            return 6;
        }

        Bmemcpy(svinitsnap, svsnapshot, svsnapsiz);
    }

    postloadplayer((spot >= 0));

    return 0;
}


uint32_t sv_writediff(FILE *fil)
{
    uint8_t *p = svsnapshot;
    uint8_t *d = svdiff;

    cmpspecdata(svgm_udnetw, &p, &d);
    cmpspecdata(svgm_secwsp, &p, &d);
    if (REALITY)
        cmpspecdata(svgm_dn64, &p, &d);
    cmpspecdata(svgm_script, &p, &d);
    cmpspecdata(svgm_anmisc, &p, &d);
    cmpspecdata((const dataspec_t *)svgm_vars, &p, &d);

    if (p != svsnapshot+svsnapsiz)
        OSD_Printf("sv_writediff: dump+siz=%p, p=%p!\n", svsnapshot+svsnapsiz, p);
    
    uint32_t const diffsiz = d - svdiff;

    fwrite("dIfF",4,1,fil);
    fwrite(&diffsiz, sizeof(diffsiz), 1, fil);

    if (savegame_diffcompress)
        dfwrite_LZ4(svdiff, 1, diffsiz, fil);  // cnt and sz swapped
    else
        fwrite(svdiff, 1, diffsiz, fil);

    return diffsiz;
}

int32_t sv_readdiff(int32_t fil)
{
    int32_t diffsiz;

    if (kread(fil, &diffsiz, sizeof(uint32_t)) != sizeof(uint32_t))
        return -1;

    if (savegame_diffcompress)
    {
        if (kdfread_LZ4(svdiff, 1, diffsiz, fil) != diffsiz)  // cnt and sz swapped
            return -2;
    }
    else
    {
        if (kread(fil, svdiff, diffsiz) != diffsiz)
            return -2;
    }

    uint8_t *p = svsnapshot;
    uint8_t *d = svdiff;

    if (applydiff(svgm_udnetw, &p, &d)) return -3;
    if (applydiff(svgm_secwsp, &p, &d)) return -4;
    if (REALITY && applydiff(svgm_dn64, &p, &d)) return -5;
    if (applydiff(svgm_script, &p, &d)) return -6;
    if (applydiff(svgm_anmisc, &p, &d)) return -7;
    if (applydiff((const dataspec_t *)svgm_vars, &p, &d)) return -8;

    int i = 0;

    if (p!=svsnapshot+svsnapsiz)
        i|=1;
    if (d!=svdiff+diffsiz)
        i|=2;
    if (i)
        OSD_Printf("sv_readdiff: p=%p, svsnapshot+svsnapsiz=%p; d=%p, svdiff+diffsiz=%p",
                   p, svsnapshot+svsnapsiz, d, svdiff+diffsiz);
    return i;
}

// SVGM data description
static void sv_postudload()
{
//    Bmemcpy(&boardfilename[0], &currentboardfilename[0], BMAX_PATH);  // DON'T do this in demos!
#if 1
    ud.m_level_number      = ud.level_number;
    ud.m_volume_number     = ud.volume_number;
    ud.m_player_skill      = ud.player_skill;
    ud.m_respawn_monsters  = ud.respawn_monsters;
    ud.m_respawn_items     = ud.respawn_items;
    ud.m_respawn_inventory = ud.respawn_inventory;
    ud.m_monsters_off      = ud.monsters_off;
    ud.m_coop              = ud.coop;
    ud.m_marker            = ud.marker;
    ud.m_ffire             = ud.ffire;
    ud.m_noexits           = ud.noexits;
#endif
}
//static int32_t lockclock_dummy;

#ifdef USE_OPENGL
static void sv_prespriteextsave()
{
    for (int i=0; i<MAXSPRITES; i++)
        if (spriteext[i].mdanimtims)
        {
            spriteext[i].mdanimtims -= mdtims;
            if (spriteext[i].mdanimtims==0)
                spriteext[i].mdanimtims++;
        }
}
static void sv_postspriteext()
{
    for (int i=0; i<MAXSPRITES; i++)
        if (spriteext[i].mdanimtims)
            spriteext[i].mdanimtims += mdtims;
}
#endif

#ifdef YAX_ENABLE
void sv_postyaxload(void)
{
    yax_update(numyaxbunches>0 ? 2 : 1);
}
#endif

static void sv_postactordata()
{
#ifdef POLYMER
    for (auto & i : actor)
    {
        i.lightptr = NULL;
        i.lightId = -1;
    }
#endif
}

static void sv_preanimateptrsave()
{
    G_Util_PtrToIdx(g_animatePtr, g_animateCnt, sector, P2I_FWD);
}
static void sv_postanimateptr()
{
    G_Util_PtrToIdx(g_animatePtr, g_animateCnt, sector, P2I_BACK);
}
static void sv_prequote()
{
    if (!savegame_quotes)
    {
        void *ptr = Xcalloc(MAXQUOTES, MAXQUOTELEN);
        savegame_quotes = (char(*)[MAXQUOTELEN])ptr;
    }
}
static void sv_quotesave()
{
    Bmemset(savegame_quotedef, 0, sizeof(savegame_quotedef));
    for (int i = 0; i < MAXQUOTES; i++)
        if (apStrings[i])
        {
            savegame_quotedef[i>>3] |= 1<<(i&7);
            Bmemcpy(savegame_quotes[i], apStrings[i], MAXQUOTELEN);
        }
}
static void sv_quoteload()
{
    for (int i = 0; i < MAXQUOTES; i++)
        if (savegame_quotedef[i>>3] & (1<<(i&7)))
        {
            C_AllocQuote(i);
            Bmemcpy(apStrings[i], savegame_quotes[i], MAXQUOTELEN);
        }
}
static void sv_restsave()
{
    uint8_t *    mem = savegame_restdata;
    DukePlayer_t dummy_ps;

    Bmemset(&dummy_ps, 0, sizeof(DukePlayer_t));

#define CPDAT(ptr,sz) do { Bmemcpy(mem, ptr, sz), mem+=sz ; } while (0)
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        CPDAT(g_player[i].user_name, 32);
        CPDAT(&g_player[i].pcolor, sizeof(g_player[0].pcolor));
        CPDAT(&g_player[i].pteam, sizeof(g_player[0].pteam));
        CPDAT(&g_player[i].frags[0], sizeof(g_player[0].frags));
        CPDAT(g_player[i].ps ? g_player[i].ps : &dummy_ps, sizeof(DukePlayer_t));
    }
    
    Bassert((savegame_restdata + SVARDATALEN) - mem == 0);
#undef CPDAT
}
static void sv_restload()
{
    uint8_t *    mem = savegame_restdata;
    DukePlayer_t dummy_ps;

#define CPDAT(ptr,sz) Bmemcpy(ptr, mem, sz), mem+=sz
    for (int i = 0; i < MAXPLAYERS; i++)
    {
        CPDAT(g_player[i].user_name, 32);
        CPDAT(&g_player[i].pcolor, sizeof(g_player[0].pcolor));
        CPDAT(&g_player[i].pteam, sizeof(g_player[0].pteam));
        CPDAT(&g_player[i].frags[0], sizeof(g_player[0].frags));
        CPDAT(g_player[i].ps ? g_player[i].ps : &dummy_ps, sizeof(DukePlayer_t));
    }
#undef CPDAT

    if (g_player[myconnectindex].ps)
        g_player[myconnectindex].ps->auto_aim = ud.config.AutoAim;
}

#ifdef DEBUGGINGAIDS
# define PRINTSIZE(name) do { if (mem) OSD_Printf(name ": %d\n", (int32_t)(mem-tmem)); \
        OSD_Printf(name ": %d ms\n", timerGetTicks()-t); t=timerGetTicks(); tmem=mem; } while (0)
#else
# define PRINTSIZE(name) do { } while (0)
#endif

static uint8_t *dosaveplayer2(FILE *fil, uint8_t *mem)
{
#ifdef DEBUGGINGAIDS
    uint8_t *tmem = mem;
    int32_t t=timerGetTicks();
#endif
    mem=writespecdata(svgm_udnetw, fil, mem);  // user settings, players & net
    PRINTSIZE("ud");
    mem=writespecdata(svgm_secwsp, fil, mem);  // sector, wall, sprite
    PRINTSIZE("sws");
    if (REALITY)
    {
        mem=writespecdata(svgm_dn64, fil, mem);  // dn64 vars
        PRINTSIZE("dn64");
    }
    mem=writespecdata(svgm_script, fil, mem);  // script
    PRINTSIZE("script");
    mem=writespecdata(svgm_anmisc, fil, mem);  // animates, quotes & misc.
    PRINTSIZE("animisc");
    Gv_WriteSave(fil);  // gamevars
    mem=writespecdata((const dataspec_t *)svgm_vars, 0, mem);
    PRINTSIZE("vars");

    return mem;
}

static int32_t doloadplayer2(int32_t fil, uint8_t **memptr)
{
    uint8_t *mem = memptr ? *memptr : NULL;
#ifdef DEBUGGINGAIDS
    uint8_t *tmem=mem;
    int32_t t=timerGetTicks();
#endif
    if (readspecdata(svgm_udnetw, fil, &mem)) return -2;
    PRINTSIZE("ud");
    if (readspecdata(svgm_secwsp, fil, &mem)) return -4;
    PRINTSIZE("sws");
    if (REALITY)
    {
        if (readspecdata(svgm_dn64, fil, &mem)) return -5;
        PRINTSIZE("dn64");
    }
    if (readspecdata(svgm_script, fil, &mem)) return -6;
    PRINTSIZE("script");
    if (readspecdata(svgm_anmisc, fil, &mem)) return -7;
    PRINTSIZE("animisc");

    int i;

    if ((i = Gv_ReadSave(fil))) return i;

    if (mem)
    {
        int32_t i;

        sv_makevarspec();
        for (i=1; svgm_vars[i].flags!=DS_END; i++)
        {
            Bmemcpy(mem, svgm_vars[i].ptr, svgm_vars[i].size*svgm_vars[i].cnt);  // careful! works because there are no DS_DYNAMIC's!
            mem += svgm_vars[i].size*svgm_vars[i].cnt;
        }
    }
    PRINTSIZE("vars");

    if (memptr)
        *memptr = mem;
    return 0;
}

int32_t sv_updatestate(int32_t frominit)
{
    uint8_t *p = svsnapshot, *pbeg=p;

    if (frominit)
        Bmemcpy(svsnapshot, svinitsnap, svsnapsiz);

    if (readspecdata(svgm_udnetw, -1, &p)) return -2;
    if (readspecdata(svgm_secwsp, -1, &p)) return -4;
    if (REALITY && readspecdata(svgm_dn64, -1, &p)) return -5;
    if (readspecdata(svgm_script, -1, &p)) return -6;
    if (readspecdata(svgm_anmisc, -1, &p)) return -7;
    if (readspecdata((const dataspec_t *)svgm_vars, -1, &p)) return -8;

    if (p != pbeg+svsnapsiz)
    {
        OSD_Printf("sv_updatestate: ptr-(snapshot end)=%d\n", (int32_t)(p-(pbeg+svsnapsiz)));
        return -9;
    }

    if (frominit)
        postloadplayer(0);
#ifdef POLYMER
    if (videoGetRenderMode() == REND_POLYMER)
        polymer_resetlights();  // must do it after polymer_loadboard() !!!
#endif

    return 0;
}

static void sv_rrrafog()
{
    G_SetFog(g_fogType ? 2 : 0);
}

static void sv_preloaddn64()
{
    if (rt_sectvtx)
        Xfree(rt_sectvtx);
    if (rt_wall)
        Xfree(rt_wall);
    if (rt_sector)
        Xfree(rt_sector);
    rt_sectvtx = (rt_vertex_t*)Xmalloc(sizeof(rt_vertex_t) * rt_vtxnum);
    rt_wall = (rt_walltype*)Xmalloc(sizeof(rt_walltype) * numwalls);
    rt_sector = (rt_sectortype*)Xmalloc(sizeof(rt_sectortype) * numsectors);
    if (rt_boardnum == 27)
        RT_LoadBOSS2MDL();
}

static void postloadplayer(int32_t savegamep)
{
    int32_t i;

    //1
    if (g_player[myconnectindex].ps->over_shoulder_on != 0)
    {
        CAMERADIST = 0;
        CAMERACLOCK = 0;
        g_player[myconnectindex].ps->over_shoulder_on = 1;
    }

    //2
    screenpeek = myconnectindex;

    //2.5
    if (savegamep)
    {
        int32_t musicIdx = (ud.music_episode*MAXLEVELS) + ud.music_level;

        Bmemset(gotpic, 0, sizeof(gotpic));
        S_ClearSoundLocks();
        G_CacheMapData();

        if (boardfilename[0] != 0 && ud.level_number == 7 && ud.volume_number == 0 && ud.music_level == 7 && ud.music_episode == 0)
        {
            char levname[BMAX_PATH];
            G_SetupFilenameBasedMusic(levname, boardfilename, ud.level_number);
        }

        if (g_mapInfo[musicIdx].musicfn != NULL && (musicIdx != g_musicIndex || different_user_map))
        {
            ud.music_episode = g_musicIndex / MAXLEVELS;
            ud.music_level   = g_musicIndex % MAXLEVELS;
            S_PlayLevelMusicOrNothing(musicIdx);
        }

        if (ud.config.MusicToggle)
            S_PauseMusic(false);

        g_player[myconnectindex].ps->gm = MODE_GAME;
        ud.recstat = 0;

        if (g_player[myconnectindex].ps->jetpack_on)
            A_PlaySound(REALITY ? 39 : DUKE_JETPACK_IDLE, g_player[myconnectindex].ps->i);
    }

    //3
    P_UpdateScreenPal(g_player[myconnectindex].ps);
    g_restorePalette = -1;

    //3.5
    if (savegamep)
    {
        for (SPRITES_OF(STAT_FX, i))
            if (sprite[i].picnum == MUSICANDSFX)
            {
                T2(i) = ud.config.SoundToggle;
                T1(i) = 0;
            }

        G_UpdateScreenArea();
        FX_SetReverb(0);
    }

    //4
    if (savegamep)
    {
        if (ud.lockout)
        {
            for (i=0; i<g_animWallCnt; i++)
                switch (DYNAMICTILEMAP(wall[animwall[i].wallnum].picnum))
                {
                case FEMPIC1__STATIC:
                    wall[animwall[i].wallnum].picnum = BLANKSCREEN;
                    break;
                case FEMPIC2__STATIC:
                case FEMPIC3__STATIC:
                    wall[animwall[i].wallnum].picnum = SCREENBREAK6;
                    break;
                }
        }
#if 0
        else
        {
            for (i=0; i<g_numAnimWalls; i++)
                if (wall[animwall[i].wallnum].extra >= 0)
                    wall[animwall[i].wallnum].picnum = wall[animwall[i].wallnum].extra;
        }
#endif
    }

    //5
    G_ResetInterpolations();

    //6
    g_showShareware = 0;
    if (savegamep)
        everyothertime = 0;

    //7
    for (i=0; i<MAXPLAYERS; i++)
        g_player[i].playerquitflag = 1;

    // ----------

    //7.5
    if (savegamep)
    {
        ready2send = 1;
        G_ClearFIFO();
        Net_WaitForEverybody();
    }

    //8
    // if (savegamep)  ?
    G_ResetTimers(0);

#ifdef USE_STRUCT_TRACKERS
    Bmemset(sectorchanged, 0, sizeof(sectorchanged));
    Bmemset(spritechanged, 0, sizeof(spritechanged));
    Bmemset(wallchanged, 0, sizeof(wallchanged));
#endif

#ifdef POLYMER
    //9
    if (videoGetRenderMode() == REND_POLYMER)
        polymer_loadboard();

    // this light pointer nulling needs to be outside the videoGetRenderMode check
    // because we might be loading the savegame using another renderer but
    // change to Polymer later
    for (i=0; i<MAXSPRITES; i++)
    {
        actor[i].lightptr = NULL;
        actor[i].lightId = -1;
    }
#endif
    for (i=0; i<MAXPLAYERS; i++)
        g_player[i].ps->drug_timer = 0;

    calc_sector_reachability();

    G_InitRRRASkies();

    if (REALITY)
        RT_ResetBarScroll();

    // [AP] Loaded player, restore AP inventory status
    ap_on_save_load();
}

////////// END GENERIC SAVING/LOADING SYSTEM //////////

