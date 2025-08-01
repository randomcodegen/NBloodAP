//
// Definitions of common game-only data structures/functions
// (and declarations of data appearing in both)
// for EDuke32 and Mapster32
//

#ifndef RR_COMMON_GAME_H_
#define RR_COMMON_GAME_H_
#pragma once

#include "collections.h"
#include "grpscan.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int g_useCwd;

// [AP] Give our application a distinct name to highlight the archipelago integration
#ifndef APPNAME
#define APPNAME             "RednukemAP"
#endif

#ifndef APPBASENAME
#define APPBASENAME         "rednukemAP"
#endif

#define GAMEFLAG_DUKE       0x00000001
#define GAMEFLAG_NAM        0x00000002
#define GAMEFLAG_NAPALM     0x00000004
#define GAMEFLAG_WW2GI      0x00000008
#define GAMEFLAG_ADDON      0x00000010
#define GAMEFLAG_SHAREWARE  0x00000020
#define GAMEFLAG_RR         0x00000040
#define GAMEFLAG_RRRA       0x00000080
#define GAMEFLAG_DEER       0x00000100
#define GAMEFLAG_REALITY    0x00000200
//#define GAMEFLAG_DUKEBETA   0x00000060 // includes 0x20 since it's a shareware beta
//#define GAMEFLAG_FURY       0x00000080
//#define GAMEFLAG_STANDALONE 0x00000100
#define GAMEFLAG_AP         0x00100000 // [AP] Marks
#define GAMEFLAGMASK        0x001003FF // flags allowed from grpinfo

extern struct grpfile_t const *g_selectedGrp;

extern int32_t g_gameType;
extern int     g_addonNum;

#define DUKE                (g_gameType & GAMEFLAG_DUKE)
#define RR                  (g_gameType & GAMEFLAG_RR)
#define RRRA                (g_gameType & GAMEFLAG_RRRA)
#define NAM                 (g_gameType & GAMEFLAG_NAM)
#define NAPALM              (g_gameType & GAMEFLAG_NAPALM)
#define WW2GI               (g_gameType & GAMEFLAG_WW2GI)
#define NAM_WW2GI           (g_gameType & (GAMEFLAG_NAM|GAMEFLAG_WW2GI))
#define SHAREWARE           (g_gameType & GAMEFLAG_SHAREWARE)
#define DEER                (g_gameType & GAMEFLAG_DEER)
#define REALITY             (g_gameType & GAMEFLAG_REALITY)
//#define DUKEBETA            ((g_gameType & GAMEFLAG_DUKEBETA) == GAMEFLAG_DUKEBETA)
//#define FURY                (g_gameType & GAMEFLAG_FURY)

enum Games_t {
    GAME_DUKE = 0,
    GAME_RR,
    GAME_RRRA,
    GAME_NAM,
    GAME_NAPALM,
    GAME_WW2GI,
    GAMECOUNT
};

enum searchpathtypes_t {
    SEARCHPATH_REMOVE = 1<<0,
    SEARCHPATH_NAM    = 1<<1,
    SEARCHPATH_WW2GI  = 1<<2,
    SEARCHPATH_RR     = 1<<3,
    SEARCHPATH_RRRA   = 1<<4,
    SEARCHPATH_DEER   = 1<<5,
};

typedef enum basepal_ {
    BASEPAL = 0,
    WATERPAL,
    SLIMEPAL,
    DREALMSPAL,
    TITLEPAL,
    ENDINGPAL,  // 5
    ANIMPAL,
    DRUGPAL,
    BASEPALCOUNT
} basepal_t;

#define OSDTEXT_DEFAULT   "^00"
#define OSDTEXT_DARKRED   "^10"
#define OSDTEXT_GREEN     "^11"
#define OSDTEXT_RED       "^21"
#define OSDTEXT_YELLOW    "^23"

#define OSDTEXT_BRIGHT    "^S0"

#define OSD_ERROR OSDTEXT_DARKRED OSDTEXT_BRIGHT

extern const char *g_gameNamePtr;

extern char *g_grpNamePtr;
extern char *g_scriptNamePtr;
extern char *g_rtsNamePtr;

extern const char *G_DefaultGrpFile(void);
extern const char *G_GrpFile(void);

extern const char *G_DefaultConFile(void);
extern const char *G_ConFile(void);

extern GrowArray<char *> g_scriptModules;

extern void G_AddCon(const char *buffer);
extern void G_AddConModule(const char *buffer);

extern void clearGrpNamePtr(void);
extern void clearScriptNamePtr(void);

extern int loaddefinitions_game(const char *fn, int32_t preload);
extern int32_t g_groupFileHandle;

//////////

extern void G_InitMultiPsky(int CLOUDYOCEAN__DYN, int MOONSKY1__DYN, int BIGORBIT1__DYN, int LA__DYN);
extern void G_SetupGlobalPsky(void);

//////////

extern void G_AddSearchPaths(void);
extern void G_CleanupSearchPaths(void);

extern void G_ExtPreInit(int32_t argc,char const * const * argv);
extern void G_ExtInit(void);
extern void G_ScanGroups(void);
extern void G_LoadGroups(int32_t autoload);

extern const char * G_GetInstallPath(int32_t insttype);

//////////

void G_LoadGroupsInDir(const char *dirname);
void G_DoAutoload(const char *dirname);

//////////

extern void G_LoadLookups(void);

//////////

void Duke_CommonCleanup(void);

#if defined HAVE_FLAC || defined HAVE_VORBIS
# define FORMAT_UPGRADE_ELIGIBLE
extern int g_maybeUpgradeSoundFormats;
extern int32_t S_OpenAudio(const char *fn, char searchfirst, uint8_t ismusic);
#else
# define S_OpenAudio(fn, searchfirst, ismusic) kopen4loadfrommod(fn, searchfirst)
#endif

#ifdef __cplusplus
}
#endif

#endif
