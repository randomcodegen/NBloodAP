//-------------------------------------------------------------------------
/*
Copyright (C) 2016 EDuke32 developers and contributors

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
#include "compat.h"
#include "sbar.h"
#include "ap_integration.h"

int32_t althud_flashing = 1;
int32_t althud_numbertile = 2930;
int32_t althud_numberpal = 0;

#ifdef EDUKE32_TOUCH_DEVICES
int32_t althud_shadows = 0;
#else
int32_t althud_shadows = 1;
#endif

static int32_t sbarx(int32_t x)
{
    if (ud.screen_size == 4) return sbarsc(x<<16);
    return (((320<<16) - sbarsc(320<<16)) >> 1) + sbarsc(x<<16);
}

static int32_t sbarxr(int32_t x)
{
    if (ud.screen_size == 4) return (320<<16) - sbarsc(x<<16);
    return (((320<<16) - sbarsc(320<<16)) >> 1) + sbarsc(x<<16);
}

static int32_t sbary(int32_t y)
{
    if (ud.hudontop == 1 && ud.screen_size == 4 && ud.althud == 1) return sbarsc(y << 16);
    else return (200<<16) - sbarsc(200<<16) + sbarsc(y<<16);
}

int32_t sbarx16(int32_t x)
{
    if (ud.screen_size == 4) return sbarsc(x);
    return (((320<<16) - sbarsc(320<<16)) >> 1) + sbarsc(x);
}

int32_t sbarxr16(int32_t x)
{
    if (ud.screen_size == 4) return (320<<16) - sbarsc(x);
    return (((320<<16) - sbarsc(320<<16)) >> 1) + sbarsc(x);
}

#if 0 // enable if ever needed
static int32_t sbarxr16(int32_t x)
{
    if (ud.screen_size == 4) return (320<<16) - sbarsc(x);
    return (((320<<16) - sbarsc(320<<16)) >> 1) + sbarsc(x);
}
#endif

int32_t sbary16(int32_t y)
{
    return (200<<16) - sbarsc(200<<16) + sbarsc(y);
}

static void G_PatchStatusBar(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t aspectCorrect = 1)
{
    int32_t const scl = sbarsc(RR ? 32768 : 65536);
    int32_t const tx = sbarx16((160<<16) - (tilesiz[BOTTOMSTATUSBAR].x<<(RR ? 14 : 15))); // centered
    int32_t const ty = sbary(200-(tilesiz[BOTTOMSTATUSBAR].y>>(RR ? 1 : 0)));
    // Assumed to be 4:3 aspect ratio
    int32_t const xdimcorrect = aspectCorrect ? (4 * ydim) / 3 : xdim;

    int32_t const clx1 = sbarsc(scale(x1, xdimcorrect, 320)), cly1 = sbarsc(scale(y1, ydim, 200));
    int32_t const clx2 = sbarsc(scale(x2, xdimcorrect, 320)), cly2 = sbarsc(scale(y2, ydim, 200));
    int32_t const clofx = (xdim - sbarsc(xdimcorrect)) >> 1, clofy = (ydim - sbarsc(ydim));

    rotatesprite(tx, ty, scl, 0, BOTTOMSTATUSBAR, 4, 0, 10+16+64, clx1+clofx, cly1+clofy, clx2+clofx-1, cly2+clofy-1);
}

static void G_PatchStatusBar2(int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    if (!RR)
        return;
    int32_t const scl = sbarsc(32768);
    int32_t const tx = sbarx16((160<<16) - (tilesiz[WEAPONBAR].x<<14)); // centered
    int32_t const ty = sbary(200-tilesiz[BOTTOMSTATUSBAR].y/2-tilesiz[WEAPONBAR].y/2);
    // Assumed to be 4:3 aspect ratio
    int32_t const xdimcorrect = (4 * ydim) / 3;

    int32_t const clx1 = sbarsc(scale(x1, xdimcorrect, 320)), cly1 = sbarsc(scale(y1, ydim, 200));
    int32_t const clx2 = sbarsc(scale(x2, xdimcorrect, 320)), cly2 = sbarsc(scale(y2, ydim, 200));
    int32_t const clofx = (xdim - sbarsc(xdimcorrect)) >> 1, clofy = (ydim - sbarsc(ydim));

    rotatesprite(tx, ty, scl, 0, WEAPONBAR, 4, 0, 10+16+64, clx1+clofx, cly1+clofy, clx2+clofx-1, cly2+clofy-1);
}

#define POLYMOSTTRANS (1)
#define POLYMOSTTRANS2 (1|32)

// Draws inventory numbers in the HUD for both the full and mini status bars.
// yofs: in hud_scale-independent, (<<16)-scaled, 0-200-normalized y coords.
static void G_DrawInvNum(int32_t x, int32_t yofs, int32_t y, int32_t num1, char ha, int32_t sbits)
{
    char dabuf[16];
    int32_t shd = (x < 0);

    const int32_t sbscale = sbarsc(RR ? 32768 : 65536);
    const int32_t sby = yofs+sbary(y), sbyp1 = yofs+sbary(y+1), sbym1 = yofs+sbary(y-1);

    const int32_t font = REALITY ? 3689 + '0' : THREEBYFIVE;

    if (shd) x = -x;

    Bsprintf(dabuf, "%d", num1);

    if (sbits & RS_ALIGN_R)
    {
        if (num1 > 999)
        {
            if (shd && ud.screen_size == 4 && videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
            {
                rotatesprite_fs(sbarxr(x+8+1), RR ? sby : sbyp1, sbscale, 0, font+dabuf[0]-'0', 127, 4, POLYMOSTTRANS|sbits);
                rotatesprite_fs(sbarxr(x+4+1), sbyp1, sbscale, 0, font+dabuf[1]-'0', 127, 4, POLYMOSTTRANS|sbits);
                rotatesprite_fs(sbarxr(x-1), sbyp1, sbscale, 0, font+dabuf[2]-'0', 127, 4, POLYMOSTTRANS|sbits);
                rotatesprite_fs(sbarxr(x-4+1), sbyp1, sbscale, 0, font+dabuf[3]-'0', 127, 4, POLYMOSTTRANS|sbits);
            }

            rotatesprite_fs(sbarxr(x+8), RR ? sbym1 : sby, sbscale, 0, font+dabuf[0]-'0', ha, 0, sbits);
            rotatesprite_fs(sbarxr(x+4), sby, sbscale, 0, font+dabuf[1]-'0', ha, 0, sbits);
            rotatesprite_fs(sbarxr(x), sby, sbscale, 0, font+dabuf[2]-'0', ha, 0, sbits);
            rotatesprite_fs(sbarxr(x-4), sby, sbscale, 0, font+dabuf[3]-'0', ha, 0, sbits);
            return;
        }

        if (num1 > 99)
        {
            if (shd && ud.screen_size == 4 && videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
            {
                rotatesprite_fs(sbarxr(x+4+1), RR ? sby : sbyp1, sbscale, 0, font+dabuf[0]-'0', 127, 4, POLYMOSTTRANS|sbits);
                rotatesprite_fs(sbarxr(x-1), sbyp1, sbscale, 0, font+dabuf[1]-'0', 127, 4, POLYMOSTTRANS|sbits);
                rotatesprite_fs(sbarxr(x-4+1), sbyp1, sbscale, 0, font+dabuf[2]-'0', 127, 4, POLYMOSTTRANS|sbits);
            }
        
            rotatesprite_fs(sbarxr(x+4), RR ? sbym1 : sby, sbscale, 0, font+dabuf[0]-'0', ha, 0, sbits);
            rotatesprite_fs(sbarxr(x), sby, sbscale, 0, font+dabuf[1]-'0', ha, 0, sbits);
            rotatesprite_fs(sbarxr(x-4), sby, sbscale, 0, font+dabuf[2]-'0', ha, 0, sbits);
            return;
        }

        if (num1 > 9)
        {
            if (shd && ud.screen_size == 4 && videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
            {
                rotatesprite_fs(sbarxr(x+1), sbyp1, sbscale, 0, font+dabuf[0]-'0', 127, 4, POLYMOSTTRANS|sbits);
                rotatesprite_fs(sbarxr(x-4-1), sbyp1, sbscale, 0, font+dabuf[1]-'0', 127, 4, POLYMOSTTRANS|sbits);
            }

            rotatesprite_fs(sbarxr(x), sby, sbscale, 0, font+dabuf[0]-'0', ha, 0, sbits);
            rotatesprite_fs(sbarxr(x-4), sby, sbscale, 0, font+dabuf[1]-'0', ha, 0, sbits);
            return;
        }

        rotatesprite_fs(sbarxr(x-4+1), sbyp1, sbscale, 0, font+dabuf[0]-'0', ha, 4, sbits);
        rotatesprite_fs(sbarxr(x-4), sby, sbscale, 0, font+dabuf[0]-'0', ha, 0, sbits);
        return;
    }

    if (num1 > 999)
    {
        if (shd && ud.screen_size == 4 && videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
        {
            rotatesprite_fs(sbarx(x-8+1), RR ? sby : sbyp1, sbscale, 0, font+dabuf[0]-'0', 127, 4, POLYMOSTTRANS|sbits);
            rotatesprite_fs(sbarx(x-4+1), sbyp1, sbscale, 0, font+dabuf[1]-'0', 127, 4, POLYMOSTTRANS|sbits);
            rotatesprite_fs(sbarx(x+1), sbyp1, sbscale, 0, font+dabuf[2]-'0', 127, 4, POLYMOSTTRANS|sbits);
            rotatesprite_fs(sbarx(x+4+1), sbyp1, sbscale, 0, font+dabuf[3]-'0', 127, 4, POLYMOSTTRANS|sbits);
        }

        rotatesprite_fs(sbarx(x-8), RR ? sbym1 : sby, sbscale, 0, font+dabuf[0]-'0', ha, 0, sbits);
        rotatesprite_fs(sbarx(x-4), sby, sbscale, 0, font+dabuf[1]-'0', ha, 0, sbits);
        rotatesprite_fs(sbarx(x), sby, sbscale, 0, font+dabuf[2]-'0', ha, 0, sbits);
        rotatesprite_fs(sbarx(x+4), sby, sbscale, 0, font+dabuf[3]-'0', ha, 0, sbits);
        return;
    }

    if (num1 > 99)
    {
        if (shd && ud.screen_size == 4 && videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
        {
            rotatesprite_fs(sbarx(x-4+1), RR ? sby : sbyp1, sbscale, 0, font+dabuf[0]-'0', 127, 4, POLYMOSTTRANS|sbits);
            rotatesprite_fs(sbarx(x+1), sbyp1, sbscale, 0, font+dabuf[1]-'0', 127, 4, POLYMOSTTRANS|sbits);
            rotatesprite_fs(sbarx(x+4+1), sbyp1, sbscale, 0, font+dabuf[2]-'0', 127, 4, POLYMOSTTRANS|sbits);
        }
        
        rotatesprite_fs(sbarx(x-4), RR ? sbym1 : sby, sbscale, 0, font+dabuf[0]-'0', ha, 0, sbits);
        rotatesprite_fs(sbarx(x), sby, sbscale, 0, font+dabuf[1]-'0', ha, 0, sbits);
        rotatesprite_fs(sbarx(x+4), sby, sbscale, 0, font+dabuf[2]-'0', ha, 0, sbits);
        return;
    }

    if (num1 > 9)
    {
        if (shd && ud.screen_size == 4 && videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
        {
            rotatesprite_fs(sbarx(x+1), sbyp1, sbscale, 0, font+dabuf[0]-'0', 127, 4, POLYMOSTTRANS|sbits);
            rotatesprite_fs(sbarx(x+4+1), sbyp1, sbscale, 0, font+dabuf[1]-'0', 127, 4, POLYMOSTTRANS|sbits);
        }

        rotatesprite_fs(sbarx(x), sby, sbscale, 0, font+dabuf[0]-'0', ha, 0, sbits);
        rotatesprite_fs(sbarx(x+4), sby, sbscale, 0, font+dabuf[1]-'0', ha, 0, sbits);
        return;
    }

    rotatesprite_fs(sbarx(x+4+1), sbyp1, sbscale, 0, font+dabuf[0]-'0', ha, 4, sbits);
    rotatesprite_fs(sbarx(x+4), sby, sbscale, 0, font+dabuf[0]-'0', ha, 0, sbits);
}

static void G_DrawWeapNum(int16_t ind, int32_t x, int32_t y, int32_t num1, int32_t num2, int32_t ha)
{
    char dabuf[16];

    const int32_t sbscale = sbarsc(65536);
    const int32_t sby = sbary(y);

    rotatesprite_fs(sbarx(x-7), sby, sbscale, 0, THREEBYFIVE+ind+1, ha-10, 7, 10);
    rotatesprite_fs(sbarx(x-3), sby, sbscale, 0, THREEBYFIVE+10, ha, 0, 10);

    if (VOLUMEONE && (ind > HANDBOMB_WEAPON || ind < 0))
    {
        minitextshade(x+1, y-4, "ORDER", 20, 11, 2+8+16+ROTATESPRITE_MAX);
        return;
    }

    rotatesprite_fs(sbarx(x+9), sby, sbscale, 0, THREEBYFIVE+11, ha, 0, 10);

    if (num1 > 99) num1 = 99;
    if (num2 > 99) num2 = 99;

    Bsprintf(dabuf, "%d", num1);
    if (num1 > 9)
    {
        rotatesprite_fs(sbarx(x), sby, sbscale, 0, THREEBYFIVE+dabuf[0]-'0', ha, 0, 10);
        rotatesprite_fs(sbarx(x+4), sby, sbscale, 0, THREEBYFIVE+dabuf[1]-'0', ha, 0, 10);
    }
    else rotatesprite_fs(sbarx(x+4), sby, sbscale, 0, THREEBYFIVE+dabuf[0]-'0', ha, 0, 10);

    Bsprintf(dabuf, "%d", num2);
    if (num2 > 9)
    {
        rotatesprite_fs(sbarx(x+13), sby, sbscale, 0, THREEBYFIVE+dabuf[0]-'0', ha, 0, 10);
        rotatesprite_fs(sbarx(x+17), sby, sbscale, 0, THREEBYFIVE+dabuf[1]-'0', ha, 0, 10);
        return;
    }
    rotatesprite_fs(sbarx(x+13), sby, sbscale, 0, THREEBYFIVE+dabuf[0]-'0', ha, 0, 10);
}

static void G_DrawWeapNum2(char ind, int32_t x, int32_t y, int32_t num1, int32_t num2, char ha)
{
    char dabuf[16];

    const int32_t sbscale = sbarsc(65536);
    const int32_t sby = sbary(y);

    rotatesprite_fs(sbarx(x-7), sby, sbscale, 0, THREEBYFIVE+ind+1, ha-10, 7, 10);
    rotatesprite_fs(sbarx(x-4), sby, sbscale, 0, THREEBYFIVE+10, ha, 0, 10);
    rotatesprite_fs(sbarx(x+13), sby, sbscale, 0, THREEBYFIVE+11, ha, 0, 10);

    Bsprintf(dabuf, "%d", num1);
    if (num1 > 99)
    {
        rotatesprite_fs(sbarx(x), sby, sbscale, 0, THREEBYFIVE+dabuf[0]-'0', ha, 0, 10);
        rotatesprite_fs(sbarx(x+4), sby, sbscale, 0, THREEBYFIVE+dabuf[1]-'0', ha, 0, 10);
        rotatesprite_fs(sbarx(x+8), sby, sbscale, 0, THREEBYFIVE+dabuf[2]-'0', ha, 0, 10);
    }
    else if (num1 > 9)
    {
        rotatesprite_fs(sbarx(x+4), sby, sbscale, 0, THREEBYFIVE+dabuf[0]-'0', ha, 0, 10);
        rotatesprite_fs(sbarx(x+8), sby, sbscale, 0, THREEBYFIVE+dabuf[1]-'0', ha, 0, 10);
    }
    else rotatesprite_fs(sbarx(x+8), sby, sbscale, 0, THREEBYFIVE+dabuf[0]-'0', ha, 0, 10);

    Bsprintf(dabuf, "%d", num2);
    if (num2 > 99)
    {
        rotatesprite_fs(sbarx(x+17), sby, sbscale, 0, THREEBYFIVE+dabuf[0]-'0', ha, 0, 10);
        rotatesprite_fs(sbarx(x+21), sby, sbscale, 0, THREEBYFIVE+dabuf[1]-'0', ha, 0, 10);
        rotatesprite_fs(sbarx(x+25), sby, sbscale, 0, THREEBYFIVE+dabuf[2]-'0', ha, 0, 10);
    }
    else if (num2 > 9)
    {
        rotatesprite_fs(sbarx(x+17), sby, sbscale, 0, THREEBYFIVE+dabuf[0]-'0', ha, 0, 10);
        rotatesprite_fs(sbarx(x+21), sby, sbscale, 0, THREEBYFIVE+dabuf[1]-'0', ha, 0, 10);
        return;
    }
    else
        rotatesprite_fs(sbarx(x+25), sby, sbscale, 0, THREEBYFIVE+dabuf[0]-'0', ha, 0, 10);
}

static void G_DrawWeapAmounts(const DukePlayer_t *p, int32_t x, int32_t y, int32_t u)
{
    int32_t cw = p->curr_weapon;

    if (u&4)
    {
        if (u != -1) G_PatchStatusBar(88, 178, 88+37, 178+6); //original code: (96,178,96+12,178+6);
        G_DrawWeapNum2(PISTOL_WEAPON, x, y,
            p->ammo_amount[PISTOL_WEAPON], p->max_ammo_amount[PISTOL_WEAPON],
            12-20*(cw == PISTOL_WEAPON));
    }
    if (u&8)
    {
        if (u != -1) G_PatchStatusBar(88, 184, 88+37, 184+6); //original code: (96,184,96+12,184+6);
        G_DrawWeapNum2(SHOTGUN_WEAPON, x, y+6,
            p->ammo_amount[SHOTGUN_WEAPON], p->max_ammo_amount[SHOTGUN_WEAPON],
            (((p->gotweapon & (1<<SHOTGUN_WEAPON)) == 0)*9)+12-18*
            (cw == SHOTGUN_WEAPON));
    }
    if (u&16)
    {
        if (u != -1) G_PatchStatusBar(88, 190, 88+37, 190+6); //original code: (96,190,96+12,190+6);
        G_DrawWeapNum2(CHAINGUN_WEAPON, x, y+12,
            p->ammo_amount[CHAINGUN_WEAPON], p->max_ammo_amount[CHAINGUN_WEAPON],
            (((p->gotweapon & (1<<CHAINGUN_WEAPON)) == 0)*9)+12-18*
            (cw == CHAINGUN_WEAPON));
    }
    if (u&32)
    {
        if (u != -1) G_PatchStatusBar(127, 178, 127+29, 178+6); //original code: (135,178,135+8,178+6);
        G_DrawWeapNum(RPG_WEAPON, x+39, y,
            p->ammo_amount[RPG_WEAPON], p->max_ammo_amount[RPG_WEAPON],
            (((p->gotweapon & (1<<RPG_WEAPON)) == 0)*9)+12-19*
            (cw == RPG_WEAPON));
    }
    if (u&64)
    {
        if (u != -1) G_PatchStatusBar(127, 184, 127+29, 184+6); //original code: (135,184,135+8,184+6);
        G_DrawWeapNum(HANDBOMB_WEAPON, x+39, y+6,
            p->ammo_amount[HANDBOMB_WEAPON], p->max_ammo_amount[HANDBOMB_WEAPON],
            (((!p->ammo_amount[HANDBOMB_WEAPON])|((p->gotweapon & (1<<HANDBOMB_WEAPON)) == 0))*9)+12-19*
            ((cw == HANDBOMB_WEAPON) || (cw == HANDREMOTE_WEAPON)));
    }
    if (u&128)
    {
        if (u != -1) G_PatchStatusBar(127, 190, 127+29, 190+6); //original code: (135,190,135+8,190+6);

        if (p->subweapon&(1<<GROW_WEAPON))
            G_DrawWeapNum(SHRINKER_WEAPON, x+39, y+12,
                p->ammo_amount[GROW_WEAPON], p->max_ammo_amount[GROW_WEAPON],
                (((p->gotweapon & (1<<GROW_WEAPON)) == 0)*9)+12-18*
                (cw == GROW_WEAPON));
        else
            G_DrawWeapNum(SHRINKER_WEAPON, x+39, y+12,
                p->ammo_amount[SHRINKER_WEAPON], p->max_ammo_amount[SHRINKER_WEAPON],
                (((p->gotweapon & (1<<SHRINKER_WEAPON)) == 0)*9)+12-18*
                (cw == SHRINKER_WEAPON));
    }
    if (u&256)
    {
        if (u != -1) G_PatchStatusBar(158, 178, 162+29, 178+6); //original code: (166,178,166+8,178+6);

        G_DrawWeapNum(DEVISTATOR_WEAPON, x+70, y,
            p->ammo_amount[DEVISTATOR_WEAPON], p->max_ammo_amount[DEVISTATOR_WEAPON],
            (((p->gotweapon & (1<<DEVISTATOR_WEAPON)) == 0)*9)+12-18*
            (cw == DEVISTATOR_WEAPON));
    }
    if (u&512)
    {
        if (u != -1) G_PatchStatusBar(158, 184, 162+29, 184+6); //original code: (166,184,166+8,184+6);

        G_DrawWeapNum(TRIPBOMB_WEAPON, x+70, y+6,
            p->ammo_amount[TRIPBOMB_WEAPON], p->max_ammo_amount[TRIPBOMB_WEAPON],
            (((p->gotweapon & (1<<TRIPBOMB_WEAPON)) == 0)*9)+12-18*
            (cw == TRIPBOMB_WEAPON));
    }

    if (u&65536L)
    {
        if (u != -1) G_PatchStatusBar(158, 190, 162+29, 190+6); //original code: (166,190,166+8,190+6);

        G_DrawWeapNum(-1, x+70, y+12,
            p->ammo_amount[FREEZE_WEAPON], p->max_ammo_amount[FREEZE_WEAPON],
            (((p->gotweapon & (1<<FREEZE_WEAPON)) == 0)*9)+12-18*
            (cw == FREEZE_WEAPON));
    }
}

// yofs: in hud_scale-independent, (<<16)-scaled, 0-200-normalized y coords.
static inline void G_DrawDigiNum_(int32_t x, int32_t yofs, int32_t y, int32_t n, char s, int32_t cs)
{
    if (!(cs & ROTATESPRITE_FULL16))
    {
        x <<= 16;
        y <<= 16;
    }

    G_DrawTXDigiNumZ(DIGITALNUM, sbarx16(x), yofs + sbary16(y), n, s, 0, cs|ROTATESPRITE_FULL16, 0, 0, xdim-1, ydim-1, sbarsc(RR ? 32768L : 65536L));
}

static inline void G_DrawDigiNum(int32_t x, int32_t y, int32_t n, char s, int32_t cs)
{
    G_DrawDigiNum_(x, 0, y, n, s, cs);
}

void G_DrawTXDigiNumZ(int32_t starttile, int32_t x, int32_t y, int32_t n, int32_t s, int32_t pal,
    int32_t cs, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t z)
{
    char b[12];
    Bsprintf(b, "%d", n);

    if (!(cs & ROTATESPRITE_FULL16))
    {
        x <<= 16;
        y <<= 16;
    }

    G_ScreenText(starttile, x, y, z, 0, 0, b, s, pal, cs|2|ROTATESPRITE_FULL16, 0, (4<<16), (8<<16), (1<<16), 0, TEXT_XCENTER|TEXT_DIGITALNUMBER, x1, y1, x2, y2);
}

static void G_DrawAltDigiNum(int32_t x, int32_t y, int32_t n, char s, int32_t cs)
{
    int32_t i, j = 0, k, p, c;
    char b[12];
    int32_t rev = (x < 0);
    int32_t shd = (y < 0);

    const int32_t sbscale = sbarsc(RR ? 32768 : 65536);

    if (rev) x = -x;
    if (shd) y = -y;

    i = Bsprintf(b, "%d", n);

    for (k=i-1; k>=0; k--)
    {
        p = althud_numbertile + b[k]-'0';
        j += (tilesiz[p].x>>(RR?1:0))+1;
    }
    c = x-(j>>1);

    if (rev)
    {
        for (k=0; k<i; k++)
        {
            p = althud_numbertile + b[k]-'0';
            if (shd && videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
                rotatesprite_fs(sbarxr(c+j-1), sbary(y+1), sbscale, 0, p, 127, 4, cs|POLYMOSTTRANS2);
            rotatesprite_fs(sbarxr(c+j), sbary(y), sbscale, 0, p, s, althud_numberpal, cs);
            j -= (tilesiz[p].x>>(RR?1:0))+1;
        }
        return;
    }

    j = 0;
    for (k=0; k<i; k++)
    {
        p = althud_numbertile + b[k]-'0';
        if (shd && videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
            rotatesprite_fs(sbarx(c+j+1), sbary(y+1), sbscale, 0, p, 127, 4, cs|POLYMOSTTRANS2);
        rotatesprite_fs(sbarx(c+j), sbary(y), sbscale, 0, p, s, althud_numberpal, cs);
        j += (tilesiz[p].x>>(RR?1:0))+1;
    }
}

static int32_t invensc(int32_t maximum) // used to reposition the inventory icon selector as the HUD scales
{
    return scale(maximum << 16, ud.statusbarscale - 36, 100 - 36);
}

void G_DrawInventory(const DukePlayer_t *p)
{
    int32_t n, j = 0, x = 0, y;

    // [AP] Inventory is drawn fixed for AP seeds, so no need to display selection on screen center
    if (AP)
        return;

    n = (p->inv_amount[GET_JETPACK] > 0)<<3;
    if (n&8) j++;
    n |= (p->inv_amount[GET_SCUBA] > 0)<<5;
    if (n&32) j++;
    n |= (p->inv_amount[GET_STEROIDS] > 0)<<1;
    if (n&2) j++;
    n |= (p->inv_amount[GET_HOLODUKE] > 0)<<2;
    if (n&4) j++;
    n |= (p->inv_amount[GET_FIRSTAID] > 0);
    if (n&1) j++;
    n |= (p->inv_amount[GET_HEATS] > 0)<<4;
    if (n&16) j++;
    n |= (p->inv_amount[GET_BOOTS] > 0)<<6;
    if (n&64) j++;

    x = (160-(j*11))<<16; // nearly center

    j = 0;

    if (ud.screen_size < 8) // mini-HUDs or no HUD
    {
        y = RR ? 180<<16 : 172<<16;

        if (ud.screen_size == 4 && ud.althud == 1) // modern mini-HUD
            y -= invensc(tilesiz[BIGALPHANUM].y+10); // slide on the y-axis
    }
    else // full HUD
    {
        y = (200<<16) - (sbarsc(tilesiz[BOTTOMSTATUSBAR].y<<(RR ? 15 : 16)) + (RR ? (6<<16) : (12<<16)) + (tilesiz[BOTTOMSTATUSBAR].y<<((RR ? 15 : 16)-1)));

        if (!ud.statusbarmode) // original non-overlay mode
            y += sbarsc(tilesiz[BOTTOMSTATUSBAR].y<<(RR ? 15 : 16))>>1; // account for the viewport y-size as the HUD scales
    }

    if (ud.screen_size == 4 && !ud.althud) // classic mini-HUD
        x += invensc(ud.multimode > 1 ? 56 : 65); // slide on the x-axis

    while (j <= 9)
    {
        const int iconscale = RR ? 32768 : 65536;
        if (n&(1<<j))
        {
            switch (n&(1<<j))
            {
            case 1:
                rotatesprite_win(x, y, iconscale, 0, FIRSTAID_ICON, 0, 0, 2+16);
                break;
            case 2:
                rotatesprite_win(x+(1<<16), y, iconscale, 0, STEROIDS_ICON, 0, 0, 2+16);
                break;
            case 4:
                rotatesprite_win(x+(2<<16), y, iconscale, 0, HOLODUKE_ICON, 0, 0, 2+16);
                break;
            case 8:
                rotatesprite_win(x, y, iconscale, 0, JETPACK_ICON, 0, 0, 2+16);
                break;
            case 16:
                rotatesprite_win(x, y, iconscale, 0, HEAT_ICON, 0, 0, 2+16);
                break;
            case 32:
                rotatesprite_win(x, y, iconscale, 0, AIRTANK_ICON, 0, 0, 2+16);
                break;
            case 64:
                rotatesprite_win(x, y-(1<<16), iconscale, 0, BOOT_ICON, 0, 0, 2+16);
                break;
            }

            x += 22<<16;

            if (p->inven_icon == j+1)
                rotatesprite_win(x-(2<<16), y+(19<<16), iconscale, 1024, ARROW, -32, 0, 2+16);
        }

        j++;
    }
}

void G_DrawFrags(void)
{
    if (ud.statusbarflags & STATUSBAR_NOFRAGBAR)
        return;

    int32_t i, j = 0;
    const int32_t orient = 2+8+16+64;

    for (TRAVERSE_CONNECT(i))
        if (i > j)
            j = i;

    for (i=0; i<=(j>>2); i++)
        rotatesprite_fs(0, (8*i)<<16, RR ? 32800 : 65600, 0, FRAGBAR, 0, 0, orient);

    for (TRAVERSE_CONNECT(i))
    {
        const DukePlayer_t *ps = g_player[i].ps;
        minitext(21+(73*(i&3)), 2+((i&28)<<1), g_player[i].user_name, ps->palookup, 2+8+16);
        Bsprintf(tempbuf, "%d", ps->frag-ps->fraggedself);
        minitext(17+50+(73*(i&3)), 2+((i&28)<<1), tempbuf, ps->palookup, 2+8+16);
    }
}

static int32_t G_GetInvAmountId(const DukePlayer_t *p, uint8_t inv_id)
{
    switch (inv_id)
    {
    case ICON_FIRSTAID:
        return p->inv_amount[GET_FIRSTAID];
    case ICON_STEROIDS:
        return AP ? (p->inv_amount[GET_STEROIDS] / ap_steroids_duration()) : (p->inv_amount[GET_STEROIDS]+3)>>2;
    case ICON_HOLODUKE:
        if (RR) return AP ? p->inv_amount[GET_HOLODUKE] : p->inv_amount[GET_HOLODUKE]/400;
        return AP ? p->inv_amount[GET_HOLODUKE] : (p->inv_amount[GET_HOLODUKE]+15)/24;
    case ICON_JETPACK:
        if (RR) return AP ? p->inv_amount[GET_JETPACK] : p->inv_amount[GET_JETPACK]/100;
        return AP ? p->inv_amount[GET_JETPACK] : (p->inv_amount[GET_JETPACK]+15)>>4;
    case ICON_HEATS:
        return AP ? p->inv_amount[GET_HEATS] : p->inv_amount[GET_HEATS]/12;
    case ICON_SCUBA:
        return AP ? p->inv_amount[GET_SCUBA] : (p->inv_amount[GET_SCUBA]+63)>>6;
    case ICON_BOOTS:
        if (RR) return AP ? p->inv_amount[GET_BOOTS] : (p->inv_amount[GET_BOOTS]/10)>>1;
        return AP ? p->inv_amount[GET_BOOTS] : p->inv_amount[GET_BOOTS]>>1;
    }

    return -1;
}

// [AP] Just use raw numbers in AP mode
static int32_t G_GetInvAmount(const DukePlayer_t *p)
{
    return G_GetInvAmountId(p, p->inven_icon);
}

static void G_DrawWeaponBar(const DukePlayer_t *p)
{
    const int32_t sbscale = sbarsc(32800);
    const int32_t SBY = (200-tilesiz[BOTTOMSTATUSBAR].y/2);
    rotatesprite_fs(sbarx(0), sbary(158), sbscale, 0, WEAPONBAR, 0, 0, 10+16+64);
    for (int i = 0; i < 9; i++) {
        if (RRRA && i == 4 && p->curr_weapon == CHICKEN_WEAPON)
        {
            rotatesprite_fs(sbarx(18+i*32), sbary(SBY-6), sbscale, 0, AMMO_ICON + 10, 0, 0, 10+16+64);
            G_DrawInvNum(38+i*32, 0, 162, (char)p->ammo_amount[CHICKEN_WEAPON], 0, 10);
        }
        else
        {
            if (p->gotweapon&(1 << (i + 1))) {
                rotatesprite_fs(sbarx(18+i*32), sbary(SBY-6), sbscale, 0, AMMO_ICON + i, 0, 0, 10+16+64);
            }
            G_DrawInvNum(38+i*32, 0, SBY-4, (char)p->ammo_amount[i+1], 0, 10);
        }
    }
}

static int32_t G_GetInvOn(const DukePlayer_t *p)
{
    switch (p->inven_icon)
    {
    case ICON_HOLODUKE:
        return p->holoduke_on;
    case ICON_JETPACK:
        return p->jetpack_on;
    case ICON_HEATS:
        return p->heat_on;
    case ICON_STEROIDS:
        if (AP)
            return p->steroids_on;
    }

    return 0x80000000;
}

static int32_t G_GetMorale(int32_t p_i, int32_t snum)
{
    return Gv_GetVarByLabel("PLR_MORALE", -1, p_i, snum);
}

static inline void rotatesprite_althud(int32_t sx, int32_t sy, int32_t z, int16_t a, int16_t picnum, int8_t dashade, char dapalnum, int32_t dastat)
{
    if (videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
        rotatesprite_(sbarx(sx+1), sbary(sy+1), z, a, picnum, 127, 4, dastat + POLYMOSTTRANS2, 0, 0, 0, 0, xdim - 1, ydim - 1);
    rotatesprite_(sbarx(sx), sbary(sy), z, a, picnum, dashade, dapalnum, dastat, 0, 0, 0, 0, xdim - 1, ydim - 1);
}

static inline void rotatesprite_althudr(int32_t sx, int32_t sy, int32_t z, int16_t a, int16_t picnum, int8_t dashade, char dapalnum, int32_t dastat)
{
    if (videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
        rotatesprite_(sbarxr(sx + 1), sbary(sy + 1), z, a, picnum, 127, 4, dastat + POLYMOSTTRANS2, 0, 0, 0, 0, xdim - 1, ydim - 1);
    rotatesprite_(sbarxr(sx), sbary(sy), z, a, picnum, dashade, dapalnum, dastat, 0, 0, 0, 0, xdim - 1, ydim - 1);
}

const int32_t ap_ammo_sprites[MAX_WEAPONS] = { -1, AMMO__STATIC, SHOTGUNAMMO__STATIC, BATTERYAMMO__STATIC,
    RPGAMMO__STATIC, HBOMBAMMO__STATIC, CRYSTALAMMO__STATIC, DEVISTATORAMMO__STATIC,
    TRIPBOMBSPRITE__STATIC, FREEZEAMMO__STATIC+1, HBOMBAMMO__STATIC, GROWAMMO__STATIC
};

const int32_t ap_weapon_sprites[MAX_WEAPONS] = { -1, FIRSTGUNSPRITE__STATIC, SHOTGUNSPRITE__STATIC, CHAINGUNSPRITE__STATIC,
RPGSPRITE__STATIC, HEAVYHBOMB__STATIC, SHRINKERSPRITE__STATIC, DEVISTATORSPRITE__STATIC,
TRIPBOMBSPRITE__STATIC, FREEZESPRITE__STATIC+1, -1, GROWSPRITEICON__STATIC
};

static void G_DrawAPString(unsigned int x, unsigned int y, char* text, unsigned int shade, unsigned int pal, unsigned int zoom, unsigned int text_flags, unsigned int rs_flags)
{
    G_ScreenText(MF_Bluefont.tilenum, x, y, zoom, 0, 0, text, shade, pal, rs_flags|RS_AUTO|RS_NOCLIP|ROTATESPRITE_FULL16, 0, tilesiz[MF_Bluefont.tilenum + 'A' - '!'].x<<16, MF_Bluefont.emptychar.y, MF_Bluefont.between.x, MF_Bluefont.between.y, text_flags, 0, 0, xdim-1, ydim-1);
}

void G_DrawStatusBar(int32_t snum)
{
    if (REALITY && RT_DrawStatusBar(snum))
        return;
    DukePlayer_t *const p = g_player[snum].ps;
    int32_t i, j, o, u;
    int32_t permbit = 0;

#ifdef SPLITSCREEN_MOD_HACKS
    const int32_t ss = g_fakeMultiMode ? 4 : ud.screen_size;
    const int32_t althud = g_fakeMultiMode ? 0 : ud.althud;
#else
    const int32_t ss = ud.screen_size;
    const int32_t althud = ud.althud;
#endif

    const int32_t SBY = (200-(tilesiz[BOTTOMSTATUSBAR].y >> (RR ? 1 : 0)));

    const int32_t sb15 = sbarsc(32768), sb15h = sbarsc(49152);
    const int32_t sb16 = sbarsc(65536);

    static int32_t item_icons[8];

    if (ss < 4)
        return;

    if (item_icons[0] == 0)
    {
        int32_t iicons[8] = { -1, FIRSTAID_ICON, STEROIDS_ICON, HOLODUKE_ICON,
            JETPACK_ICON, HEAT_ICON, AIRTANK_ICON, BOOT_ICON };
        int32_t iicons64[9] = { -1, FIRSTAID, STEROIDS, HOLODUKE,
            JETPACK, HEATSENSOR, AIRTANK, BOOTS, SHIELD };
        if (REALITY)
            Bmemcpy(item_icons, iicons64, sizeof(item_icons));
        else
            Bmemcpy(item_icons, iicons, sizeof(item_icons));
    }

    if (videoGetRenderMode() >= REND_POLYMOST) pus = NUMPAGES;   // JBF 20040101: always redraw in GL

    if ((g_netServer || ud.multimode > 1) && ((g_gametypeFlags[ud.coop] & GAMETYPE_FRAGBAR)))
    {
        if (pus)
            G_DrawFrags();
        else
        {
            for (TRAVERSE_CONNECT(i))
                if (g_player[i].ps->frag != sbar.frag[i])
                {
                    G_DrawFrags();
                    break;
                }

        }
        for (TRAVERSE_CONNECT(i))
            if (i != myconnectindex)
                sbar.frag[i] = g_player[i].ps->frag;
    }

    // [AP] Custom HUD based on alternative mini
    if (AP)
    {
        int32_t hudoffset = 200;

        // Health and Armor
        rotatesprite_althud(2, hudoffset-21, sb15h, 0, COLA, 0, 0, 10+16+256);
        if (sprite[p->i].pal == 1 && p->last_extra < 2)
            G_DrawAltDigiNum(40, -(hudoffset-22), 1, -16, 10+16+256);
        else if (!althud_flashing || p->last_extra >(p->max_player_health>>2) || (int32_t) totalclock&32)
        {
            int32_t s = -8;
            if (althud_flashing && p->last_extra > p->max_player_health)
                s += (sintable[((int32_t) totalclock<<5)&2047]>>10);
            G_DrawAltDigiNum(40, -(hudoffset-22), p->last_extra, s, 10+16+256);
        }

        rotatesprite_althud(62, hudoffset-25, sb15h, 0, SHIELD, 0, 0, 10+16+256);
        {
            G_DrawAltDigiNum(105, -(hudoffset-22), p->inv_amount[GET_SHIELD], -16, 10+16+256);
        }

        // Inventory Items
        o = 0;
        for (i = 0; i < ICON_SHIELD; i++)
        {
            j = icon_to_inv[i];
            if (j < 0 || p->inv_amount[j] <= 0 || item_icons[i] < 0)
                continue;

            switch(i)
            {
            case ICON_STEROIDS:
                u = 1;
                break;
            case ICON_HOLODUKE:
                u = 3;
                break;
            default:
                u = 0;
                break;


            }

            rotatesprite_althud(134 + o + u, hudoffset-21-2, sb16, 0, item_icons[i], 0, 0, RS_AUTO|RS_NOCLIP|RS_ALIGN_L|RS_TOPLEFT);
            if (i == p->inven_icon)
                // Draw selection border
                rotatesprite_althud(130 + o, hudoffset-25, sb16, 0, ARROW, 0, 0, RS_AUTO|RS_NOCLIP|RS_ALIGN_L|RS_TOPLEFT);
            char inv_buf[5] = {0};
            Bsnprintf(inv_buf, 8, "%4u", G_GetInvAmountId(p, i));
            G_DrawAPString(sbarx(152+o), sbary(hudoffset-4), inv_buf, 0, i == p->inven_icon ? 11 : 12, sb15, TEXT_XRIGHT|TEXT_YBOTTOM, RS_ALIGN_L|RS_TOPLEFT);
            
            o += 22;
        }

        // Show ability lock items
        o=132;

        if (!(ap_can_use()))
        {
            rotatesprite_althudr(o, hudoffset-22, sb15, 0, AP_NO_USE, 0, 0, 10+16+512);
            o += 18;
        }
        if (!(ap_can_open()))
        {
            rotatesprite_althudr(o, hudoffset-22, sb15, 0, AP_NO_OPEN, 0, 0, 10+16+512);
            o += 18;
        }
        if (!(ap_can_dive()))
        {
            rotatesprite_althudr(o, hudoffset-22, sb15, 0, AP_NO_DIVE, 0, 0, 10+16+512);
            o += 18;
        }
        if (!(ap_can_run()))
        {
            rotatesprite_althudr(o, hudoffset-22, sb15, 0, AP_NO_SPRINT, 0, 0, 10+16+512);
            o += 18;
        }
        if (!(ap_can_crouch()))
        {
            rotatesprite_althudr(o, hudoffset-22, sb15, 0, AP_NO_CROUCH, 0, 0, 10+16+512);
            o += 18;
        }
        if (!(ap_can_jump()))
        {
            rotatesprite_althudr(o, hudoffset-22, sb15, 0, AP_NO_JUMP, 0, 0, 10+16+512);
            o += 18;
        }

        // Show access cards
        if (p->got_access&1) rotatesprite_althudr(113, hudoffset-22, sb15, 0, ACCESSCARD, 0, 0, 10+16+512);
        if (p->got_access&4) rotatesprite_althudr(108, hudoffset-20, sb15, 0, ACCESSCARD, 0, 23, 10+16+512);
        if (p->got_access&2) rotatesprite_althudr(103, hudoffset-18, sb15, 0, ACCESSCARD, 0, 21, 10+16+512);

        // Big display for equipped weapon
        if (ap_ammo_sprites[p->curr_weapon] >= 0)
        {
            i = (tilesiz[ap_ammo_sprites[p->curr_weapon]].y >= 50) ? 16384 : 32768;
            rotatesprite_althudr(57, hudoffset-15, sbarsc(i), 0, ap_ammo_sprites[p->curr_weapon], 0, 0, 10+512);
        }
        if (p->curr_weapon== HANDREMOTE_WEAPON) i = HANDBOMB_WEAPON;
        else i = p->curr_weapon;
        if (p->curr_weapon != KNEE_WEAPON &&
            (!althud_flashing || (int32_t) totalclock&32 || p->ammo_amount[i] > (p->max_ammo_amount[i]/10)))
            G_DrawAltDigiNum(-20, -(hudoffset-22), p->ammo_amount[i], -16, 10+16+512);

        // Then draw ammo amounts for all unlocked weapons
        o = 0;
        for(i = MAX_WEAPONS; i > 0; i--)
        {
            if (p->gotweapon & (1 << i) && p->max_ammo_amount[i] > 0)
            {
                char ammo_buf[10] = {0};
                Bsnprintf(ammo_buf, 8, "%4u/%-4u", p->ammo_amount[i], p->max_ammo_amount[i]);
                G_DrawAPString(318<<16, (180-(4*o))<<16, ammo_buf, 0, (i == p->curr_weapon || (p->curr_weapon == HANDREMOTE_WEAPON__STATIC && i == HANDBOMB_WEAPON__STATIC)) ? 11 : 12, 3<<13, TEXT_XRIGHT|TEXT_YBOTTOM, RS_ALIGN_R);
                if (ap_weapon_sprites[i] > 0)
                    rotatesprite_(290<<16, (180-(4*(o+1)))<<16, 3<<11, 0, ap_weapon_sprites[i], 0, 0, RS_AUTO|RS_NOCLIP|RS_ALIGN_R, 0, 0, 0, 0, xdim - 1, ydim - 1);
                o++;
            }
        }
        return;
    }

    if (ss == 4)   //DRAW MINI STATUS BAR:
    {
        if (althud)
        {
            // ALTERNATIVE STATUS BAR

            if (RR)
            {
                int32_t hudoffset = ud.hudontop == 1 ? 32 : 200;
                static int32_t ammo_sprites[MAX_WEAPONS];

                if (EDUKE32_PREDICT_FALSE(ammo_sprites[0] == 0))
                {
                    /* this looks stupid but it lets us initialize static memory to dynamic values
                    these values can be changed from the CONs with dynamic tile remapping
                    but we don't want to have to recreate the values in memory every time
                    the HUD is drawn */

                    int32_t asprites[MAX_WEAPONS] = { -1, AMMO, SHOTGUNAMMO, BATTERYAMMO,
                        HBOMBAMMO, HBOMBAMMO, RRTILE43, DEVISTATORAMMO,
                        TRIPBOMBSPRITE, GROWSPRITEICON, HBOMBAMMO, -1, BOWLINGBALLSPRITE,
                        MOTOAMMO, BOATAMMO, -1, RPG2SPRITE
                    };
                    Bmemcpy(ammo_sprites, asprites, sizeof(ammo_sprites));
                    // Hardcoded for now
                    althud_numbertile = BIGALPHANUM-10;
                }

                //            rotatesprite_fs(sbarx(5+1),sbary(200-25+1),sb15h,0,SIXPAK,0,4,10+16+1+32);
                //            rotatesprite_fs(sbarx(5),sbary(200-25),sb15h,0,SIXPAK,0,0,10+16);
                rotatesprite_althud(2, hudoffset-26, sbarsc(10000), 0, SPINNINGNUKEICON+1, 0, 0, 10+16+256);

                if (sprite[p->i].pal == 1 && p->last_extra < 2)
                    G_DrawAltDigiNum(44, -(hudoffset-22), 1, -16, 10+16+256);
                else if (!althud_flashing || p->last_extra >(p->max_player_health>>2) || (int32_t) totalclock&32)
                {
                    int32_t s = -8;
                    if (althud_flashing && p->last_extra > p->max_player_health)
                        s += (sintable[((int32_t) totalclock<<5)&2047]>>10);
                    G_DrawAltDigiNum(44, -(hudoffset-22), p->last_extra, s, 10+16+256);
                }

                rotatesprite_althud(70, hudoffset-22, sbarsc(10000), 0, COLA, 0, 0, 10+16+256);
                G_DrawAltDigiNum(98, -(hudoffset-22), p->drink_amt, -8, 10+16+256);

                rotatesprite_althud(122, hudoffset-22, sbarsc(19000), 0, JETPACK, 0, 0, 10+16+256);
                G_DrawAltDigiNum(175, -(hudoffset-22), p->eat_amt, -8, 10+16+256);

                if (ammo_sprites[p->curr_weapon] >= 0)
                {
                    i = (tilesiz[ammo_sprites[p->curr_weapon]].y >= 50) ? 16384 : 32768;
                    if (p->curr_weapon == TRIPBOMB_WEAPON)
                        rotatesprite_althudr(57, hudoffset-30, sbarsc(i), 0, ammo_sprites[p->curr_weapon], 0, 0, 10+512);
                    else
                        rotatesprite_althudr(57, hudoffset-15, sbarsc(i), 0, ammo_sprites[p->curr_weapon], 0, 0, 10+512);
                }

                if (p->curr_weapon== HANDREMOTE_WEAPON) i = HANDBOMB_WEAPON;
                else i = p->curr_weapon;

                if (p->curr_weapon != KNEE_WEAPON && p->curr_weapon != SLINGBLADE_WEAPON &&
                    (!althud_flashing || (int32_t) totalclock&32 || p->ammo_amount[i] > (p->max_ammo_amount[i]/10)))
                    G_DrawAltDigiNum(-20, -(hudoffset-22), p->ammo_amount[i], -16, 10+16+512);

                o = 102;
                permbit = 0;

                if (p->inven_icon)
                {
                    const int32_t orient = 10+16+permbit+512;

                    i = ((unsigned) p->inven_icon < ICON_MAX) ? item_icons[p->inven_icon] : -1;

                    if (i >= 0)
                        rotatesprite_althudr(320-(231-o+85), hudoffset-21-2, sb15, 0, i, 0, 0, orient);

                    if (!AP && (p->inven_icon == 1 || p->inven_icon == 2))
                    {
                        if (videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
                            minitextshade(320-(292-30-o+1+85), hudoffset-10-3+1, "%", 127, 4, POLYMOSTTRANS+orient+ROTATESPRITE_MAX);
                        minitext(320-(292-30-o+85), hudoffset-10-3, "%", 0, orient+ROTATESPRITE_MAX);
                    }

                    i = G_GetInvAmount(p);

                    G_DrawInvNum(320-(284-30-o+85), 0, hudoffset-6-3, i, 0, 10+permbit+512);

                    if (p->inven_icon >= ICON_SCUBA)
                    {
                        if (videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
                            minitextshade(320-(284-35-o+1+85), hudoffset-20-3+1, "Auto", 127, 4, POLYMOSTTRANS+orient+ROTATESPRITE_MAX);
                        minitext(320-(284-35-o+85), hudoffset-20-3, "Auto", 2, orient+ROTATESPRITE_MAX);
                    }
                }

                if (ud.hudontop == 1)
                    hudoffset += 40;

                if (p->keys[1]) rotatesprite_althudr(39, hudoffset-43, sb15, 0, ACCESSCARD, 0, 0, 10+16+512);
                if (p->keys[3]) rotatesprite_althudr(34, hudoffset-41, sb15, 0, ACCESSCARD, 0, 23, 10+16+512);
                if (p->keys[2]) rotatesprite_althudr(29, hudoffset-39, sb15, 0, ACCESSCARD, 0, 21, 10+16+512);
            }
            else
            {
                int32_t hudoffset = ud.hudontop == 1 ? 32 : 200;
                static int32_t ammo_sprites[MAX_WEAPONS];

                if (EDUKE32_PREDICT_FALSE(ammo_sprites[0] == 0))
                {
                    /* this looks stupid but it lets us initialize static memory to dynamic values
                    these values can be changed from the CONs with dynamic tile remapping
                    but we don't want to have to recreate the values in memory every time
                    the HUD is drawn */

                    int32_t asprites[MAX_WEAPONS] = { -1, AMMO, SHOTGUNAMMO, BATTERYAMMO,
                        RPGAMMO, HBOMBAMMO, CRYSTALAMMO, DEVISTATORAMMO,
                        TRIPBOMBSPRITE, FREEZEAMMO+1, HBOMBAMMO, GROWAMMO
                    };

                    int32_t asprites64[MAX_WEAPONS] = { -1, AMMO, SHOTGUNAMMO, BATTERYAMMO,
                        RPGAMMO, HBOMBAMMO, CRYSTALAMMO, GROWAMMO, DEVISTATORAMMO,
                        TRIPBOMBSPRITE, FREEZEAMMO, HBOMBAMMO, 34, 50, 43
                    };
                    if (REALITY)
                        Bmemcpy(ammo_sprites, asprites64, sizeof(ammo_sprites));
                    else
                        Bmemcpy(ammo_sprites, asprites, sizeof(ammo_sprites));
                    // Hardcoded for now
                    althud_numbertile = BIGALPHANUM-10;
                }

                //            rotatesprite_fs(sbarx(5+1),sbary(200-25+1),sb15h,0,SIXPAK,0,4,10+16+1+32);
                //            rotatesprite_fs(sbarx(5),sbary(200-25),sb15h,0,SIXPAK,0,0,10+16);
                rotatesprite_althud(2, hudoffset-21, sb15h, 0, COLA, 0, 0, 10+16+256);

                if (sprite[p->i].pal == 1 && p->last_extra < 2)
                    G_DrawAltDigiNum(40, -(hudoffset-22), 1, -16, 10+16+256);
                else if (!althud_flashing || p->last_extra >(p->max_player_health>>2) || (int32_t) totalclock&32)
                {
                    int32_t s = -8;
                    if (althud_flashing && p->last_extra > p->max_player_health)
                        s += (sintable[((int32_t) totalclock<<5)&2047]>>10);
                    G_DrawAltDigiNum(40, -(hudoffset-22), p->last_extra, s, 10+16+256);
                }

                rotatesprite_althud(62, hudoffset-25, sb15h, 0, SHIELD, 0, 0, 10+16+256);

                {
                    int32_t lAmount = G_GetMorale(p->i, snum);
                    if (lAmount == -1)
                        lAmount = p->inv_amount[GET_SHIELD];
                    G_DrawAltDigiNum(105, -(hudoffset-22), lAmount, -16, 10+16+256);
                }

                if (ammo_sprites[p->curr_weapon] >= 0)
                {
                    i = (tilesiz[ammo_sprites[p->curr_weapon]].y >= 50) ? 16384 : 32768;
                    rotatesprite_althudr(57, hudoffset-15, sbarsc(i), 0, ammo_sprites[p->curr_weapon], 0, 0, 10+512);
                }

                if (p->curr_weapon== HANDREMOTE_WEAPON) i = HANDBOMB_WEAPON;
                else i = p->curr_weapon;

                if (p->curr_weapon != KNEE_WEAPON &&
                    (!althud_flashing || (int32_t) totalclock&32 || p->ammo_amount[i] > (p->max_ammo_amount[i]/10)))
                    G_DrawAltDigiNum(-20, -(hudoffset-22), p->ammo_amount[i], -16, 10+16+512);

                o = 102;
                permbit = 0;

                if (p->inven_icon && (!REALITY || p->inven_icon != ICON_SHIELD))
                {
                    const int32_t orient = 10+16+permbit+256;

                    i = ((unsigned) p->inven_icon < ICON_MAX) ? item_icons[p->inven_icon] : -1;

                    if (i >= 0)
                    {
                        int sc = sb16;
                        if (REALITY && tilesiz[i].x != 0 && tilesiz[i].y != 0)
                            sc = min(scale(sc, 16, tilesiz[i].x), scale(sc, 16, tilesiz[i].y));
                        rotatesprite_althud(231-o, hudoffset-21-2, sc, 0, i, 0, 0, orient);
                    }

                    if (REALITY)
                        rotatesprite_althud(292-30-o, hudoffset-10-3, sb16, 0, 3736, 0, 0, orient);
                    else if (!AP)
                    {
                        if (videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
                            minitextshade(292-30-o+1, hudoffset-10-3+1, "%", 127, 4, POLYMOSTTRANS+orient+ROTATESPRITE_MAX);
                        minitext(292-30-o, hudoffset-10-3, "%", 6, orient+ROTATESPRITE_MAX);
                    }

                    i = G_GetInvAmount(p);
                    j = G_GetInvOn(p);

                    if (REALITY)
                        G_DrawInvNum(-(284-30+7-o), 0, hudoffset-6-3-2, i, 0, 10+permbit+256);
                    else
                        G_DrawInvNum(-(284-30-o), 0, hudoffset-6-3, i, 0, 10+permbit+256);

                    if (REALITY)
                    {
                        if (j > 0)
                            rotatesprite_althud(288-30-o, hudoffset-20-3, sb16, 0, 3749, 0, 0, orient);
                        else if ((uint32_t) j != 0x80000000)
                            rotatesprite_althud(284-30-o, hudoffset-20-3, sb16, 0, 3748, 0, 0, orient);

                        if (p->inven_icon >= ICON_SCUBA)
                            rotatesprite_althud(284-35-o, hudoffset-20-3, sb16, 0, 3747, 0, 0, orient);
                    }
                    else
                    {
                        if (!WW2GI)
                        {
                            if (j > 0)
                            {
                                if (videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
                                    minitextshade(288-30-o+1, hudoffset-20-3+1, "On", 127, 4, POLYMOSTTRANS+orient+ROTATESPRITE_MAX);
                                minitext(288-30-o, hudoffset-20-3, "On", 0, orient+ROTATESPRITE_MAX);
                            }
                            else if ((uint32_t) j != 0x80000000)
                            {
                                if (videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
                                    minitextshade(284-30-o+1, hudoffset-20-3+1, "Off", 127, 4, POLYMOSTTRANS+orient+ROTATESPRITE_MAX);
                                minitext(284-30-o, hudoffset-20-3, "Off", 2, orient+ROTATESPRITE_MAX);
                            }
                        }

                        if (p->inven_icon >= ICON_SCUBA)
                        {
                            if (videoGetRenderMode() >= REND_POLYMOST && althud_shadows)
                                minitextshade(284-35-o+1, hudoffset-20-3+1, "Auto", 127, 4, POLYMOSTTRANS+orient+ROTATESPRITE_MAX);
                            minitext(284-35-o, hudoffset-20-3, "Auto", 2, orient+ROTATESPRITE_MAX);
                        }
                    }
                }

                if (ud.hudontop == 1)
                    hudoffset += 40;

                if (REALITY)
                {
                    if (p->got_access&1) rotatesprite_althudr(39, hudoffset-43, sb15, 0, ACCESSCARD, 0, 0, 10+16+512);
                    if (p->got_access&4) rotatesprite_althudr(34, hudoffset-41, sb15, 0, DN64TILE66, 0, 0, 10+16+512);
                    if (p->got_access&2) rotatesprite_althudr(29, hudoffset-39, sb15, 0, DN64TILE65, 0, 0, 10+16+512);
                }
                else
                {
                    if (p->got_access&1) rotatesprite_althudr(39, hudoffset-43, sb15, 0, ACCESSCARD, 0, 0, 10+16+512);
                    if (p->got_access&4) rotatesprite_althudr(34, hudoffset-41, sb15, 0, ACCESSCARD, 0, 23, 10+16+512);
                    if (p->got_access&2) rotatesprite_althudr(29, hudoffset-39, sb15, 0, ACCESSCARD, 0, 21, 10+16+512);
                }
            }
        }
        else
        {
            // ORIGINAL MINI STATUS BAR
            int32_t orient = 2+8+16+256, yofssh=0;

#ifdef SPLITSCREEN_MOD_HACKS
            int32_t yofs=0;

            if (g_fakeMultiMode)
            {
                const int32_t sidebyside = (ud.screen_size!=0);

                if (sidebyside && snum==1)
                {
                    orient |= RS_CENTERORIGIN;
                }
                else if (!sidebyside && snum==0)
                {
                    yofs = -100;
                    yofssh = yofs<<16;
                }
            }
#endif

            if (RR)
            {
                rotatesprite_fs(sbarx(2), yofssh+sbary(200-28), sb15, 0, HEALTHBOX, 0, 21, orient);
                if (p->inven_icon)
                    rotatesprite_fs(sbarx(77), yofssh+sbary(200-30), sb15, 0, INVENTORYBOX, 0, 21, orient);

                // health
                {
                    int32_t health = (sprite[p->i].pal == 1 && p->last_extra < 2) ? 1 : p->last_extra;
                    G_DrawDigiNum_(20, yofssh, 200-17, health, -16, orient);
                }

                rotatesprite_fs(sbarx(41), yofssh+sbary(200-28), sb15, 0, AMMOBOX, 0, 21, orient);

                if (p->curr_weapon == HANDREMOTE_WEAPON)
                    i = HANDBOMB_WEAPON;
                else
                    i = p->curr_weapon;
                G_DrawDigiNum_(59, yofssh, 200-17, p->ammo_amount[i], -16, orient);

                o = 158;
                permbit = 0;
                if (p->inven_icon)
                {
                    //                orient |= permbit;

                    i = ((unsigned) p->inven_icon < ICON_MAX) ? item_icons[p->inven_icon] : -1;
                    if (i >= 0)
                        rotatesprite_fs(sbarx(231-o+10), yofssh+sbary(200-21), sb15, 0, i, 0, 0, orient);

                    // scale by status bar size
                    orient |= ROTATESPRITE_MAX;

                    minitext_yofs = yofssh;
                    if (!AP && (p->inven_icon == 1 || p->inven_icon == 2))
                        minitext(292-30-o+10, 190, "%", 0, orient);
                    i = G_GetInvAmount(p);

                    G_DrawInvNum(284-30-o+8, yofssh, 200-6, i, 0, orient&~16);

                    minitext_yofs = 0;
                }
            }
            else
            {
                rotatesprite_fs(sbarx(5), yofssh+sbary(200-28), sb16, 0, HEALTHBOX, 0, 21, orient);
                if (p->inven_icon)
                    rotatesprite_fs(sbarx(69), yofssh+sbary(200-30), sb16, 0, INVENTORYBOX, 0, 21, orient);

                // health
                {
                    int32_t health = (sprite[p->i].pal == 1 && p->last_extra < 2) ? 1 : p->last_extra;
                    G_DrawDigiNum_(20, yofssh, 200-17, health, -16, orient);
                }

                rotatesprite_fs(sbarx(37), yofssh+sbary(200-28), sb16, 0, AMMOBOX, 0, 21, orient);

                if (p->curr_weapon == HANDREMOTE_WEAPON)
                    i = HANDBOMB_WEAPON;
                else
                    i = p->curr_weapon;
                G_DrawDigiNum_(53, yofssh, 200-17, p->ammo_amount[i], -16, orient);

                o = 158;
                permbit = 0;
                if (p->inven_icon)
                {
                    //                orient |= permbit;

                    i = ((unsigned) p->inven_icon < ICON_MAX) ? item_icons[p->inven_icon] : -1;
                    if (i >= 0)
                        rotatesprite_fs(sbarx(231-o), yofssh+sbary(200-21), sb16, 0, i, 0, 0, orient);

                    // scale by status bar size
                    orient |= ROTATESPRITE_MAX;

                    minitext_yofs = yofssh;
                    if (!AP)
                        minitext(292-30-o, 190, "%", 6, orient);

                    i = G_GetInvAmount(p);
                    j = G_GetInvOn(p);

                    G_DrawInvNum(284-30-o, yofssh, 200-6, i, 0, orient&~16);

                    if (!WW2GI)
                    {
                        if (j > 0)
                            minitext(288-30-o, 180, "On", 0, orient);
                        else if ((uint32_t) j != 0x80000000)
                            minitext(284-30-o, 180, "Off", 2, orient);
                    }

                    if (p->inven_icon >= ICON_SCUBA)
                        minitext(284-35-o, 180, "Auto", 2, orient);

                    minitext_yofs = 0;
                }
            }
        }

        return;
    }

    //DRAW/UPDATE FULL STATUS BAR:

    if (pus)
    {
        pus = 0;
        u = -1;
    }
    else u = 0;

    if (sbar.frag[myconnectindex] != p->frag)
    {
        sbar.frag[myconnectindex] = p->frag;
        u |= 32768;
    }
    if (sbar.got_access != p->got_access)
    {
        sbar.got_access = p->got_access;
        u |= 16384;
    }

    if (sbar.last_extra != p->last_extra)
    {
        sbar.last_extra = p->last_extra;
        u |= 1;
    }

    {
        int32_t lAmount = G_GetMorale(p->i, snum);
        if (lAmount == -1)
            lAmount = p->inv_amount[GET_SHIELD];
        if (sbar.inv_amount[GET_SHIELD] != lAmount)
        {
            sbar.inv_amount[GET_SHIELD] = lAmount;
            u |= 2;
        }
    }

    if (sbar.curr_weapon != p->curr_weapon)
    {
        sbar.curr_weapon = p->curr_weapon;
        u |= (4+8+16+32+64+128+256+512+1024+65536L);
    }

    for (i=1; i<MAX_WEAPONS; i++)
    {
        if (sbar.ammo_amount[i] != p->ammo_amount[i])
        {
            sbar.ammo_amount[i] = p->ammo_amount[i];
            if (i < 9)
                u |= ((2<<i)+1024);
            else if (WW2GI && i == 11)
                u |= 1024 + 128;
            else
                u |= 65536L+1024;
        }

        if ((sbar.gotweapon & (1<<i)) != (p->gotweapon & (1<<i)))
        {
            if (p->gotweapon & (1<<i))
                sbar.gotweapon |= 1<<i;
            else sbar.gotweapon &= ~(1<<i);

            if (i < 9)
                u |= ((2<<i)+1024);
            else
                u |= 65536L+1024;
        }
    }

    if (sbar.inven_icon != p->inven_icon)
    {
        sbar.inven_icon = p->inven_icon;
        u |= (2048+4096+8192);
    }
    if (sbar.holoduke_on != p->holoduke_on)
    {
        sbar.holoduke_on = p->holoduke_on;
        u |= (4096+8192);
    }
    if (sbar.jetpack_on != p->jetpack_on)
    {
        sbar.jetpack_on = p->jetpack_on;
        u |= (4096+8192);
    }
    if (sbar.heat_on != p->heat_on)
    {
        sbar.heat_on = p->heat_on;
        u |= (4096+8192);
    }

    {
        static const int32_t check_items [] = {
            GET_FIRSTAID, GET_STEROIDS, GET_HOLODUKE, GET_JETPACK,
            GET_HEATS, GET_SCUBA, GET_BOOTS
        };

        for (i=0; i<(int32_t)sizeof(check_items)/(int32_t)sizeof(check_items[0]); i++)
        {
            int32_t item = check_items[i];

            if (sbar.inv_amount[item] != p->inv_amount[item])
            {
                sbar.inv_amount[item] = p->inv_amount[item];
                u |= 8192;
            }
        }
    }
#if 0
    if (u == 0)
        return;
#else
    // FIXME: full status bar draws rectangles in the wrong places when it's
    // updated partially.
    u = -1;
#endif

    //0 - update health
    //1 - update armor
    //2 - update PISTOL_WEAPON ammo
    //3 - update SHOTGUN_WEAPON ammo
    //4 - update CHAINGUN_WEAPON ammo
    //5 - update RPG_WEAPON ammo
    //6 - update HANDBOMB_WEAPON ammo
    //7 - update SHRINKER_WEAPON ammo
    //8 - update DEVISTATOR_WEAPON ammo
    //9 - update TRIPBOMB_WEAPON ammo
    //10 - update ammo display
    //11 - update inventory icon
    //12 - update inventory on/off
    //13 - update inventory %
    //14 - update keys
    //15 - update kills
    //16 - update FREEZE_WEAPON ammo

    if (u == -1)
    {
        G_PatchStatusBar(0, 0, 320, 200, 0);
        if ((g_netServer || ud.multimode > 1) && (g_gametypeFlags[ud.coop] & GAMETYPE_FRAGBAR))
            rotatesprite_fs(sbarx(277+1), sbary(SBY+7-1), sb16, 0, KILLSICON, 0, 0, 10+16);
        if (ud.screen_size > 8)
            G_DrawWeaponBar(p);
    }

    if ((g_netServer || ud.multimode > 1) && (g_gametypeFlags[ud.coop] & GAMETYPE_FRAGBAR))
    {
        if (u&32768)
        {
            if (u != -1) G_PatchStatusBar(276, SBY+17, 299, SBY+17+10);
            G_DrawDigiNum(287, SBY+17, max(p->frag-p->fraggedself, 0), -16, 10+16);
        }
    }
    else
    {
        if (u&16384)
        {
            if (RR)
            {
                if (u != -1) G_PatchStatusBar(136, SBY+16, 164, SBY+16+12);
                if (p->keys[3]) rotatesprite_fs(sbarx(140), sbary(SBY+16), sb15, 0, ACCESS_ICON, 0, 23, 10+16);
                if (p->keys[2]) rotatesprite_fs(sbarx(153), sbary(SBY+16), sb15, 0, ACCESS_ICON, 0, 21, 10+16);
                if (p->keys[1]) rotatesprite_fs(sbarx(146), sbary(SBY+23), sb15, 0, ACCESS_ICON, 0, 0, 10+16);
            }
            else
            {
                if (u != -1) G_PatchStatusBar(275, SBY+18, 299, SBY+18+12);
                if (p->got_access&4) rotatesprite_fs(sbarx(275), sbary(SBY+16), sb16, 0, ACCESS_ICON, 0, 23, 10+16);
                if (p->got_access&2) rotatesprite_fs(sbarx(288), sbary(SBY+16), sb16, 0, ACCESS_ICON, 0, 21, 10+16);
                if (p->got_access&1) rotatesprite_fs(sbarx(281), sbary(SBY+23), sb16, 0, ACCESS_ICON, 0, 0, 10+16);
            }
        }
    }

    if (!RR && (u&(4+8+16+32+64+128+256+512+65536L)))
        G_DrawWeapAmounts(p, 96, SBY+16, u);

    if (u&1)
    {
        if (u != -1) G_PatchStatusBar(RR ? 52 : 20, SBY+17, RR ? 77 : 43, SBY+17+11);
        if (sprite[p->i].pal == 1 && p->last_extra < 2)
            G_DrawDigiNum(RR ? 64 : 32, SBY+17, 1, -16, 10+16);
        else G_DrawDigiNum(RR ? 64 : 32, SBY+17, p->last_extra, -16, 10+16);
    }
    if (!RR && (u&2))
    {
        if (u != -1)
            G_PatchStatusBar(52, SBY+17, 75, SBY+17+11);

        G_DrawDigiNum(64, SBY+17, sbar.inv_amount[GET_SHIELD], -16, 10+16);
    }

    if (u&1024)
    {
        if (u != -1) G_PatchStatusBar(RR ? 95 : 196, SBY+17, RR ? 120 : 219, SBY+17+11);
        if (p->curr_weapon != KNEE_WEAPON && (!RRRA || p->curr_weapon != SLINGBLADE_WEAPON))
        {
            if (p->curr_weapon == HANDREMOTE_WEAPON) i = HANDBOMB_WEAPON;
            else i = p->curr_weapon;
            G_DrawDigiNum(RR ? 107 : (230-22), SBY+17, p->ammo_amount[i], -16, 10+16);
            if (RR && ud.screen_size > 8)
            {
                if (p->curr_weapon == RPG_WEAPON || p->curr_weapon == HANDBOMB_WEAPON)
                {
                    G_PatchStatusBar2(126, SBY-8, 142, SBY-8+16);
                    G_DrawInvNum(134, 0, SBY-4, p->ammo_amount[HANDBOMB_WEAPON], 0, 10);
                    G_PatchStatusBar2(158, SBY-8, 174, SBY-8+16);
                    G_DrawInvNum(166, 0, SBY-4, p->ammo_amount[RPG_WEAPON], 0, 10);
                }
                else if (RRRA && p->curr_weapon == CHICKEN_WEAPON)
                {
                    G_PatchStatusBar2(158, SBY-8, 174, SBY-8+16);
                    G_DrawInvNum(166, 0, SBY-4, p->ammo_amount[CHICKEN_WEAPON], 0, 10);
                }
                else if (p->curr_weapon <= DEVISTATOR_WEAPON)
                {
                    G_PatchStatusBar2(p->curr_weapon * 32 - 2, SBY-8, p->curr_weapon * 32 + 14, SBY-8+16);
                    G_DrawInvNum(p->curr_weapon * 32 + 6, 0, SBY-4, p->ammo_amount[p->curr_weapon], 0, 10);
                }
                /*for (i = 1; i < 10; i++)
                {
                    if (RRRA && i == 4 && p->curr_weapon == CHICKEN_WEAPON)
                    {
                        if (!p->ammo_amount[CHICKEN_WEAPON] && (p->gotweapon&(1<< CHICKEN_WEAPON)))
                        {
                            G_PatchStatusBar2(i * 32 - 2, SBY-8, i * 32 + 14, SBY-8+16);
                            G_DrawInvNum(i * 32 + 6, 0, SBY-4, p->ammo_amount[CHICKEN_WEAPON], 0, 10);
                        }
                    }
                    else if (!p->ammo_amount[i] && (p->gotweapon&(1<<i)))
                    {
                        G_PatchStatusBar2(i * 32 - 2, SBY-8, i * 32 + 14, SBY-8+16);
                        G_DrawInvNum(i * 32 + 6, 0, SBY-4, p->ammo_amount[i], 0, 10);
                    }
                }*/
            }
        }
    }

    if (u&(2048+4096+8192))
    {
        if (u != -1)
        {
            if (RR)
            {
                if (u&(2048+4096))
                    G_PatchStatusBar(177, SBY+10, 222, SBY+10+21);
                else
                    G_PatchStatusBar(201, SBY+24, 216, SBY+24+10);
            }
            else
            {
                if (u&(2048+4096))
                    G_PatchStatusBar(231, SBY+13, 265, SBY+13+18);
                else
                    G_PatchStatusBar(250, SBY+24, 261, SBY+24+6);
            }
        }

        if (p->inven_icon)
        {
            o = 0;
            //            permbit = 128;

            if (u&(2048+4096))
            {
                i = ((unsigned) p->inven_icon < ICON_MAX) ? item_icons[p->inven_icon] : -1;
                // XXX: i < 0?
                if (RR)
                {
                    if (!AP && (i == FIRSTAID_ICON || i == STEROIDS_ICON))
                        minitext(214-o+2, SBY+24, "%", 0, 10+16+permbit  + ROTATESPRITE_MAX);
                    if (i == AIRTANK_ICON)
                        rotatesprite_fs(sbarx(183-o), sbary(SBY+10), sb15, 0, i, 0, 0, 10+16+permbit);
                    else if (i == FIRSTAID_ICON || i == BOOT_ICON || i == STEROIDS_ICON)
                        rotatesprite_fs(sbarx(183-o), sbary(SBY+12), sb15, 0, i, 0, 0, 10+16+permbit);
                    else
                        rotatesprite_fs(sbarx(183-o), sbary(SBY+13), sb15, 0, i, 0, 0, 10+16+permbit);
                    if (p->inven_icon == ICON_SCUBA || p->inven_icon == ICON_BOOTS)
                        minitext(201-o, SBY+14, "Auto", 2, 10+16+permbit + ROTATESPRITE_MAX);
                }
                else
                {
                    rotatesprite_fs(sbarx(231-o), sbary(SBY+13), sb16, 0, i, 0, 0, 10+16+permbit);
                    if(!AP)
                        minitext(292-30-o, SBY+24, "%", 6, 10+16+permbit + ROTATESPRITE_MAX);
                    if (p->inven_icon >= ICON_SCUBA) minitext(284-35-o, SBY+14, "Auto", 2, 10+16+permbit + ROTATESPRITE_MAX);
                }
            }

            if (u&(2048+4096) && !WW2GI)
            {
                j = G_GetInvOn(p);

                if (!RR)
                {
                    if (j > 0) minitext(288-30-o, SBY+14, "On", 0, 10+16+permbit  + ROTATESPRITE_MAX);
                    else if ((uint32_t) j != 0x80000000) minitext(284-30-o, SBY+14, "Off", 2, 10+16+permbit + ROTATESPRITE_MAX);
                }
            }

            if (u&8192)
            {
                i = G_GetInvAmount(p);
                G_DrawInvNum((RR ? 206 : (284-30))-o, 0, SBY+28, i, 0, 10+permbit);
            }
        }
    }

    // Gut meter
    if (RR)
    {
        G_PatchStatusBar(240,SBY+2,310,SBY+33);
        p->drink_ang = ((p->drink_amt*8)+1647)&2047;
        if (p->drink_amt >= 100)
        {
            p->drink_amt = 100;
            p->drink_ang = 400;
        }
        rotatesprite_fs(sbarx(257),sbary(SBY+15),sb15,p->drink_ang,GUTMETER,0,0,10);
        rotatesprite_fs(sbarx(293),sbary(SBY+15),sb15,p->eat_ang,GUTMETER,0,0,10);
        o = 9;
        if (p->drink_amt >= 0 && p->drink_amt <= 30)
        {
            rotatesprite_fs(sbarx(239),sbary((SBY+15+o)),sb15,0,GUTMETER_LIGHT1,0,0,10+16);
        }
        else if (p->drink_amt >= 31 && p->drink_amt <= 65)
        {
            rotatesprite_fs(sbarx(248),sbary((SBY+15+o)),sb15,0,GUTMETER_LIGHT2,0,0,10+16);
        }
        else if (p->drink_amt >= 66 && p->drink_amt <= 87)
        {
            rotatesprite_fs(sbarx(256),sbary((SBY+15+o)),sb15,0,GUTMETER_LIGHT3,0,0,10+16);
        }
        else
        {
            rotatesprite_fs(sbarx(265),sbary((SBY+15+o)),sb15,0,GUTMETER_LIGHT4,0,0,10+16);
        }

        if (p->eat_amt >= 0 && p->eat_amt <= 30)
        {
            rotatesprite_fs(sbarx(276),sbary((SBY+15+o)),sb15,0,GUTMETER_LIGHT1,0,0,10+16);
        }
        else if (p->eat_amt >= 31 && p->eat_amt <= 65)
        {
            rotatesprite_fs(sbarx(285),sbary((SBY+15+o)),sb15,0,GUTMETER_LIGHT2,0,0,10+16);
        }
        else if (p->eat_amt >= 66 && p->eat_amt <= 87)
        {
            rotatesprite_fs(sbarx(294),sbary((SBY+15+o)),sb15,0,GUTMETER_LIGHT3,0,0,10+16);
        }
        else
        {
            rotatesprite_fs(sbarx(302),sbary((SBY+15+o)),sb15,0,GUTMETER_LIGHT4,0,0,10+16);
        }
    }
}

void G_DrawBackground(void)
{
    int32_t x, y, x1, x2;

    renderFlushPerms();

    int32_t y1=0, y2=ydim;

    if ((g_player[myconnectindex].ps->gm&MODE_GAME) == 0 && ud.recstat != 2)
    {
        const int32_t MENUTILE = DEER ? 7040 : MENUSCREEN;//(videoGetRenderMode() == REND_CLASSIC ? MENUSCREEN : LOADSCREEN);
        const int32_t fstilep = tilesiz[MENUTILE].x>=320 && tilesiz[MENUTILE].y==200;
        int32_t bgtile = (fstilep ? MENUTILE : (RRRA ? RRTILE7629 : BIGHOLE));

        videoClearScreen(0);

        if (REALITY)
        {
            RT_DisablePolymost(0);
            RT_RotateSpriteSetColor(255, 255, 255, 255);
            RT_RotateSprite(160, 120, 100, 100, 3670, RTRS_SCALED);
            RT_EnablePolymost();
            return;
        }

        // when not rendering a game, fullscreen wipe
        //        Gv_SetVar(g_iReturnVarID,tilesizx[MENUTILE]==320&&tilesizy[MENUTILE]==200?MENUTILE:BIGHOLE, -1, -1);
        // MENU_TILE: is the menu tile tileable?
        if (!fstilep)
        {
            const int32_t tileScale = RR ? 32768L : 65536L;
            const int32_t tileShade = RR ? 18 : 8;
            if ((unsigned) bgtile < MAXTILES)
                for (y=y1; y<y2; y+=tilesiz[bgtile].y)
                    for (x=0; x<xdim; x+=tilesiz[bgtile].x)
                        rotatesprite_fs(x<<16, y<<16, tileScale, 0, bgtile, tileShade, 0, 8+16+64);
        }
        else rotatesprite_fs(160<<16, 100<<16, 65536L, 0, bgtile, DEER ? 0 : 16, 0, 2+8+64+BGSTRETCH);

        return;
    }

    if (REALITY)
        return;

    int32_t const dapicnum = (RRRA ? RRTILE7629 : BIGHOLE);

    // XXX: if dapicnum is not available, this might leave the menu background
    // not drawn, leading to "HOM".
    if ((dapicnum >= 0 && tilesiz[dapicnum].x == 0) || (dapicnum >= 0 && tilesiz[dapicnum].y == 0) ||
        (windowxy1.x-1 <= 0 && windowxy2.x >= xdim-1 && windowxy1.y-1 <= 0 && windowxy2.y >= ydim-1) ||
        dapicnum < 0)
    {
        pus = pub = NUMPAGES;
        return;
    }

    y1 = fragbarheight();
    if (y1 != 0)
    {
        y1 = scale(ydim, y1, 200);
        y1 -= ((tilesiz[dapicnum].y / y1) +1) * tilesiz[dapicnum].y;
    }

    if (windowxy1.y > 0)
    {
        for (y=y1; y<windowxy1.y; y+=tilesiz[dapicnum].y)
            for (x=0; x<xdim; x+=tilesiz[dapicnum].x)
                rotatesprite(x<<16, y<<16, 65536L, 0, dapicnum, 8, 0, 8+16+64, 0, 0, xdim-1, windowxy1.y-2); // top
    }

    if (windowxy1.x > 0 || windowxy2.x < xdim)
    {
        const int32_t rx = windowxy2.x-windowxy2.x%tilesiz[dapicnum].x;
        for (y=y1+windowxy1.y-windowxy1.y%tilesiz[dapicnum].y; y<windowxy2.y; y+=tilesiz[dapicnum].y)
            for (x=0; x<windowxy1.x || x+rx<xdim; x+=tilesiz[dapicnum].x)
            {
                if (windowxy1.x > 0)
                    rotatesprite(x<<16, y<<16, 65536L, 0, dapicnum, 8, 0, 8+16+64, 0, windowxy1.y-1, windowxy1.x-2, windowxy2.y); // left
                if (windowxy2.x < xdim)
                    rotatesprite((x+rx)<<16, y<<16, 65536L, 0, dapicnum, 8, 0, 8+16+64, windowxy2.x+1, windowxy1.y-1, xdim-1, windowxy2.y); // right
            }
    }

    if (windowxy2.y < ydim)
    {
        for (y=y1+windowxy2.y-(windowxy2.y%tilesiz[dapicnum].y); y<y2; y+=tilesiz[dapicnum].y)
            for (x=0; x<xdim; x+=tilesiz[dapicnum].x)
                rotatesprite(x<<16, y<<16, 65536L, 0, dapicnum, 8, 0, 8+16+64, 0, windowxy2.y+1, xdim-1, ydim-1); //  bottom
    }

    x1 = windowxy1.x-4;
    y1 = windowxy1.y-4;
    x2 = windowxy2.x+4;
    y2 = windowxy2.y+4;

    if (windowxy1.x > 0 || windowxy2.x < xdim)
    for (y=y1+4; y<y2-4; y+=64)
    {
        if (windowxy1.x > 0)
            rotatesprite(x1<<16, y<<16, 65536L, 0, VIEWBORDER, 0, 0, 8+16+64, x1, y1, x2, y2); // left
        if (windowxy2.x < xdim)
            rotatesprite((x2+1)<<16, (y+64)<<16, 65536L, 1024, VIEWBORDER, 0, 0, 8+16+64, x1, y1, x2, y2); // right
    }

    if (windowxy1.y > 0 || windowxy2.y < ydim)
    for (x=x1+4; x<x2-4; x+=64)
    {
        if (windowxy1.y > 0)
            rotatesprite((x+64)<<16, y1<<16, 65536L, 512, VIEWBORDER, 0, 0, 8+16+64, x1, y1, x2, y2); // top
        if (windowxy2.y < ydim)
            rotatesprite(x<<16, (y2+1)<<16, 65536L, 1536, VIEWBORDER, 0, 0, 8+16+64, x1, y1, x2, y2); // bottom
    }

    if (windowxy1.x > 0 && windowxy1.y > 0)
        rotatesprite(x1<<16, y1<<16, 65536L, 0, VIEWBORDER+1, 0, 0, 8+16+64, x1, y1, x2, y2); // top left
    if (windowxy2.x < xdim && windowxy1.y > 0)
        rotatesprite((x2+1)<<16, y1<<16, 65536L, 512, VIEWBORDER+1, 0, 0, 8+16+64, x1, y1, x2, y2); // top right
    if (windowxy2.x < xdim && windowxy2.y < ydim)
        rotatesprite((x2+1)<<16, (y2+1)<<16, 65536L, 1024, VIEWBORDER+1, 0, 0, 8+16+64, x1, y1, x2, y2); // bottom right
    if (windowxy1.x > 0 && windowxy2.y < ydim)
        rotatesprite(x1<<16, (y2+1)<<16, 65536L, 1536, VIEWBORDER+1, 0, 0, 8+16+64, x1, y1, x2, y2); // bottom left

    pus = pub = NUMPAGES;
}
