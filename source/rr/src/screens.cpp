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
#include "screens.h"
#include "colmatch.h"
#include "input.h"
#include "anim.h"
#include "sbar.h"
#include "menus.h"
#include "osdfuncs.h"
#include "demo.h"
#include "mdsprite.h"
#include "ap_integration.h"

#ifdef __ANDROID__
#include "android.h"
#endif

#define COLOR_RED redcol
#define COLOR_WHITE whitecol

#define quotepulseshade (sintable[((uint32_t)totalclock<<5)&2047]>>11)

palette_t CrosshairColors = { 255, 255, 255, 0 };
palette_t DefaultCrosshairColors = { 0, 0, 0, 0 };
int32_t g_crosshairSum = -1;
// yxaspect and viewingrange just before the 'main' drawrooms call
int32_t dr_yxaspect, dr_viewingrange;
double g_moveActorsTime, g_moveWorldTime;  // in ms, smoothed
int32_t g_noLogoAnim = 0;
int32_t g_noLogo = 0;

////////// OFTEN-USED FEW-LINERS //////////
static void G_HandleEventsWhileNoInput(void)
{
    I_ClearAllInput();

    while (!I_CheckAllInput())
        G_HandleAsync();

    I_ClearAllInput();
}

static int32_t G_PlaySoundWhileNoInput(int32_t soundnum)
{
    S_PlaySound(soundnum);
    I_ClearAllInput();
    while (S_CheckSoundPlaying(-1, soundnum))
    {
        G_HandleAsync();
        if (I_CheckAllInput())
        {
            I_ClearAllInput();
            return 1;
        }
    }

    return 0;
}
//////////

void P_SetGamePalette(DukePlayer_t *player, uint32_t palid, int32_t set)
{
    if (palid >= MAXBASEPALS)
        palid = 0;

    player->palette = palid;

    if (player != g_player[screenpeek].ps)
        return;

    videoSetPalette(ud.brightness>>2, palid, set);
}

void G_GetCrosshairColor(void)
{
    if (DefaultCrosshairColors.f)
        return;

    tileLoad(CROSSHAIR);

    if (!waloff[CROSSHAIR])
        return;

    char const *ptr = (char const *) waloff[CROSSHAIR];

    // find the brightest color in the original 8-bit tile
    int32_t ii = tilesiz[CROSSHAIR].x * tilesiz[CROSSHAIR].y;
    int32_t bri = 0, j = 0, i;

    Bassert(ii > 0);

    do
    {
        if (*ptr != 255)
        {
            i = curpalette[(int32_t) *ptr].r+curpalette[(int32_t) *ptr].g+curpalette[(int32_t) *ptr].b;
            if (i > j) { j = i; bri = *ptr; }
        }
        ptr++;
    } while (--ii);

    Bmemcpy(&CrosshairColors, &curpalette[bri], sizeof(palette_t));
    Bmemcpy(&DefaultCrosshairColors, &curpalette[bri], sizeof(palette_t));
    DefaultCrosshairColors.f = 1; // this flag signifies that the color has been detected
}

void G_SetCrosshairColor(int32_t r, int32_t g, int32_t b)
{
    if (g_crosshairSum == r+(g<<8)+(b<<16))
        return;

    tileLoad(CROSSHAIR);

    if (!waloff[CROSSHAIR])
        return;

    if (!DefaultCrosshairColors.f)
        G_GetCrosshairColor();

    g_crosshairSum = r+(g<<8)+(b<<16);
    CrosshairColors.r = r;
    CrosshairColors.g = g;
    CrosshairColors.b = b;

    char *ptr = (char *) waloff[CROSSHAIR];

    int32_t ii = tilesiz[CROSSHAIR].x * tilesiz[CROSSHAIR].y;

    Bassert(ii > 0);

    int32_t i = (videoGetRenderMode() == REND_CLASSIC)
                ? paletteGetClosestColor(CrosshairColors.r, CrosshairColors.g, CrosshairColors.b)
                : paletteGetClosestColor(255, 255, 255);  // use white in GL so we can tint it to the right color

    do
    {
        if (*ptr != 255)
            *ptr = i;
        ptr++;
    } while (--ii);

    paletteMakeLookupTable(CROSSHAIR_PAL, NULL, CrosshairColors.r, CrosshairColors.g, CrosshairColors.b, 1);

#ifdef USE_OPENGL
    // XXX: this makes us also load all hightile textures tinted with the crosshair color!
    polytint_t & crosshairtint = hictinting[CROSSHAIR_PAL];
    crosshairtint.r = CrosshairColors.r;
    crosshairtint.g = CrosshairColors.g;
    crosshairtint.b = CrosshairColors.b;
    crosshairtint.f = HICTINT_USEONART | HICTINT_GRAYSCALE;
#endif
    tileInvalidate(CROSSHAIR, -1, -1);
}

#define SCORESHEETOFFSET -20
static void G_ShowScores(void)
{
    int32_t t, i;

    if (g_mostConcurrentPlayers > 1 && (g_gametypeFlags[ud.coop]&GAMETYPE_SCORESHEET))
    {
        gametext_center(SCORESHEETOFFSET+58+2, "Multiplayer Totals");
        gametext_center(SCORESHEETOFFSET+58+10, g_mapInfo[G_LastMapInfoIndex()].name);

        t = 0;
        minitext(70, SCORESHEETOFFSET+80, "Name", 8, 2+8+16+ROTATESPRITE_MAX);
        minitext(170, SCORESHEETOFFSET+80, "Frags", 8, 2+8+16+ROTATESPRITE_MAX);
        minitext(200, SCORESHEETOFFSET+80, "Deaths", 8, 2+8+16+ROTATESPRITE_MAX);
        minitext(235, SCORESHEETOFFSET+80, "Ping", 8, 2+8+16+ROTATESPRITE_MAX);

        for (i=g_mostConcurrentPlayers-1; i>=0; i--)
        {
            if (!g_player[i].playerquitflag)
                continue;

            minitext(70, SCORESHEETOFFSET+90+t, g_player[i].user_name, g_player[i].ps->palookup, 2+8+16+ROTATESPRITE_MAX);

            Bsprintf(tempbuf, "%-4d", g_player[i].ps->frag);
            minitext(170, SCORESHEETOFFSET+90+t, tempbuf, 2, 2+8+16+ROTATESPRITE_MAX);

            Bsprintf(tempbuf, "%-4d", g_player[i].frags[i] + g_player[i].ps->fraggedself);
            minitext(200, SCORESHEETOFFSET+90+t, tempbuf, 2, 2+8+16+ROTATESPRITE_MAX);

            //Bsprintf(tempbuf, "%-4d", g_player[i].ping);
            //minitext(235, SCORESHEETOFFSET+90+t, tempbuf, 2, 2+8+16+ROTATESPRITE_MAX);

            t += 7;
        }
    }
}
#undef SCORESHEETOFFSET

////////// TINT ACCUMULATOR //////////

typedef struct {
    int32_t r, g, b;
    // f: 0-63 scale
    int32_t maxf, sumf;
} palaccum_t;

#define PALACCUM_INITIALIZER { 0, 0, 0, 0, 0 }

/* For a picture frame F and n tints C_1, C_2, ... C_n weighted a_1, a_2,
* ... a_n (on a 0-1 scale), the faded frame is calculated as
*
*    F_new := (1-max_i(a_i))*F + d*sum_i(a_i), where
*
*    d := max_i(a_i)/sum_i(a_i).
*
* This means that
*  1) tint application is independent of their order.
*  2) going from n+1 to n tints is continuous when the leaving one has faded.
*
* But note that for more than one tint, the composite tint will in general
* change its hue as the ratio of the weights of the individual ones changes.
*/
static void palaccum_add(palaccum_t *pa, const palette_t *pal, int32_t f)
{
    f = clamp(f, 0, 63);
    if (f == 0)
        return;

    pa->maxf = max(pa->maxf, f);
    pa->sumf += f;

    pa->r += f*clamp(pal->r, 0, 63);
    pa->g += f*clamp(pal->g, 0, 63);
    pa->b += f*clamp(pal->b, 0, 63);
}

static void G_FadePalaccum(const palaccum_t *pa)
{
    videoFadePalette(tabledivide32_noinline(pa->r, pa->sumf)<<2,
        tabledivide32_noinline(pa->g, pa->sumf)<<2,
        tabledivide32_noinline(pa->b, pa->sumf)<<2, pa->maxf<<2);
}


static int32_t gtextsc(int32_t sc)
{
    return scale(sc, ud.textscale, 400);
}

////////// DISPLAYREST //////////

static void G_DrawCameraText(int16_t i)
{
    if (!T1(i))
    {
        rotatesprite_win(24<<16, 33<<16, 65536L, 0, CAMCORNER, 0, 0, 2);
        rotatesprite_win((320-26)<<16, 34<<16, 65536L, 0, CAMCORNER+1, 0, 0, 2);
        rotatesprite_win(22<<16, 163<<16, 65536L, 512, CAMCORNER+1, 0, 0, 2+4);
        rotatesprite_win((310-10)<<16, 163<<16, 65536L, 512, CAMCORNER+1, 0, 0, 2);

        if ((int32_t) totalclock&16)
            rotatesprite_win(46<<16, 32<<16, 65536L, 0, CAMLIGHT, 0, 0, 2);
    }
    else
    {
        int32_t flipbits = ((int32_t) totalclock<<1)&48;

        for (bssize_t x=-64; x<394; x+=64)
            for (bssize_t y=0; y<200; y+=64)
                rotatesprite_win(x<<16, y<<16, 65536L, 0, STATIC, 0, 0, 2+flipbits);
    }
}

static inline void G_MoveClouds(void)
{
    int32_t i;

    if (totalclock <= g_cloudClock && totalclock >= (g_cloudClock-7))
        return;

    g_cloudClock = totalclock+6;

    g_cloudX += sintable[(fix16_to_int(g_player[screenpeek].ps->q16ang)+512)&2047]>>9;
    g_cloudY += sintable[fix16_to_int(g_player[screenpeek].ps->q16ang)&2047]>>9;

    for (i=g_cloudCnt-1; i>=0; i--)
    {
        sector[g_cloudSect[i]].ceilingxpanning = g_cloudX>>6;
        sector[g_cloudSect[i]].ceilingypanning = g_cloudY>>6;
    }
}

static void G_DrawOverheadMap(int32_t cposx, int32_t cposy, int32_t czoom, int16_t cang)
{
    int32_t i, j, k, l, x1, y1, x2=0, y2=0, x3, y3, x4, y4, ox, oy, xoff, yoff;
    int32_t dax, day, cosang, sinang, xspan, yspan, sprx, spry;
    int32_t xrepeat, yrepeat, z1, z2, startwall, endwall, tilenum, daang;
    int32_t xvect, yvect, xvect2, yvect2;
    int16_t p;
    char col;
    uwalltype *wal, *wal2;
    spritetype *spr;

    int32_t tmpydim = (xdim*5)/8;

    renderSetAspect(65536, divscale16(tmpydim*320, xdim*200));

    xvect = sintable[(-cang)&2047] * czoom;
    yvect = sintable[(1536-cang)&2047] * czoom;
    xvect2 = mulscale16(xvect, yxaspect);
    yvect2 = mulscale16(yvect, yxaspect);

    renderDisableFog();

    //Draw red lines
    for (i=numsectors-1; i>=0; i--)
    {
        if (!(show2dsector[i>>3]&(1<<(i&7)))) continue;

        startwall = sector[i].wallptr;
        endwall = sector[i].wallptr + sector[i].wallnum;

        z1 = sector[i].ceilingz;
        z2 = sector[i].floorz;

        for (j=startwall, wal=(uwalltype *)&wall[startwall]; j<endwall; j++, wal++)
        {
            k = wal->nextwall;
            if (k < 0) continue;

            if (sector[wal->nextsector].ceilingz == z1)
                if (sector[wal->nextsector].floorz == z2)
                    if (((wal->cstat|wall[wal->nextwall].cstat)&(16+32)) == 0) continue;

            col = editorcolors[1]; //red
            if ((wal->cstat|wall[wal->nextwall].cstat)&1) col = editorcolors[5]; //magenta

            if (!(show2dsector[wal->nextsector>>3]&(1<<(wal->nextsector&7))))
                col = editorcolors[7];
            else continue;

            ox = wal->x-cposx;
            oy = wal->y-cposy;
            x1 = dmulscale16(ox, xvect, -oy, yvect)+(xdim<<11);
            y1 = dmulscale16(oy, xvect2, ox, yvect2)+(ydim<<11);

            wal2 = (uwalltype *)&wall[wal->point2];
            ox = wal2->x-cposx;
            oy = wal2->y-cposy;
            x2 = dmulscale16(ox, xvect, -oy, yvect)+(xdim<<11);
            y2 = dmulscale16(oy, xvect2, ox, yvect2)+(ydim<<11);

            renderDrawLine(x1, y1, x2, y2, col);
        }
    }

    renderEnableFog();

    //Draw sprites
    k = g_player[screenpeek].ps->i;
    for (i=numsectors-1; i>=0; i--)
    {
        if (!(show2dsector[i>>3]&(1<<(i&7)))) continue;
        for (j=headspritesect[i]; j>=0; j=nextspritesect[j])
        {
            spr = &sprite[j];

            if (j == k || (spr->cstat&0x8000) || spr->cstat == 257 || spr->xrepeat == 0) continue;

            col = editorcolors[6]; //cyan
            if (spr->cstat&1) col = editorcolors[5]; //magenta

            sprx = spr->x;
            spry = spr->y;

            if ((spr->cstat&257) != 0) switch (spr->cstat&48)
            {
            case 0:
                //                    break;

                ox = sprx-cposx;
                oy = spry-cposy;
                x1 = dmulscale16(ox, xvect, -oy, yvect);
                y1 = dmulscale16(oy, xvect2, ox, yvect2);

                ox = (sintable[(spr->ang+512)&2047]>>7);
                oy = (sintable[(spr->ang)&2047]>>7);
                x2 = dmulscale16(ox, xvect, -oy, yvect);
                y2 = dmulscale16(oy, xvect, ox, yvect);

                x3 = mulscale16(x2, yxaspect);
                y3 = mulscale16(y2, yxaspect);

                renderDrawLine(x1-x2+(xdim<<11), y1-y3+(ydim<<11),
                    x1+x2+(xdim<<11), y1+y3+(ydim<<11), col);
                renderDrawLine(x1-y2+(xdim<<11), y1+x3+(ydim<<11),
                    x1+x2+(xdim<<11), y1+y3+(ydim<<11), col);
                renderDrawLine(x1+y2+(xdim<<11), y1-x3+(ydim<<11),
                    x1+x2+(xdim<<11), y1+y3+(ydim<<11), col);
                break;

            case 16:
                if (spr->picnum == LASERLINE)
                {
                    x1 = sprx;
                    y1 = spry;
                    tilenum = spr->picnum;
                    xoff = picanm[tilenum].xofs + spr->xoffset;
                    if ((spr->cstat&4) > 0) xoff = -xoff;
                    k = spr->ang;
                    l = spr->xrepeat;
                    dax = sintable[k&2047]*l;
                    day = sintable[(k+1536)&2047]*l;
                    l = tilesiz[tilenum].x;
                    k = (l>>1)+xoff;
                    x1 -= mulscale16(dax, k);
                    x2 = x1+mulscale16(dax, l);
                    y1 -= mulscale16(day, k);
                    y2 = y1+mulscale16(day, l);

                    ox = x1-cposx;
                    oy = y1-cposy;
                    x1 = dmulscale16(ox, xvect, -oy, yvect);
                    y1 = dmulscale16(oy, xvect2, ox, yvect2);

                    ox = x2-cposx;
                    oy = y2-cposy;
                    x2 = dmulscale16(ox, xvect, -oy, yvect);
                    y2 = dmulscale16(oy, xvect2, ox, yvect2);

                    renderDrawLine(x1+(xdim<<11), y1+(ydim<<11),
                        x2+(xdim<<11), y2+(ydim<<11), col);
                }

                break;

            case 32:
                tilenum = spr->picnum;
                xoff = picanm[tilenum].xofs + spr->xoffset;
                yoff = picanm[tilenum].yofs + spr->yoffset;
                if ((spr->cstat&4) > 0) xoff = -xoff;
                if ((spr->cstat&8) > 0) yoff = -yoff;

                k = spr->ang;
                cosang = sintable[(k+512)&2047];
                sinang = sintable[k&2047];
                xspan = tilesiz[tilenum].x;
                xrepeat = spr->xrepeat;
                yspan = tilesiz[tilenum].y;
                yrepeat = spr->yrepeat;

                dax = ((xspan>>1)+xoff)*xrepeat;
                day = ((yspan>>1)+yoff)*yrepeat;
                x1 = sprx + dmulscale16(sinang, dax, cosang, day);
                y1 = spry + dmulscale16(sinang, day, -cosang, dax);
                l = xspan*xrepeat;
                x2 = x1 - mulscale16(sinang, l);
                y2 = y1 + mulscale16(cosang, l);
                l = yspan*yrepeat;
                k = -mulscale16(cosang, l);
                x3 = x2+k;
                x4 = x1+k;
                k = -mulscale16(sinang, l);
                y3 = y2+k;
                y4 = y1+k;

                ox = x1-cposx;
                oy = y1-cposy;
                x1 = dmulscale16(ox, xvect, -oy, yvect);
                y1 = dmulscale16(oy, xvect2, ox, yvect2);

                ox = x2-cposx;
                oy = y2-cposy;
                x2 = dmulscale16(ox, xvect, -oy, yvect);
                y2 = dmulscale16(oy, xvect2, ox, yvect2);

                ox = x3-cposx;
                oy = y3-cposy;
                x3 = dmulscale16(ox, xvect, -oy, yvect);
                y3 = dmulscale16(oy, xvect2, ox, yvect2);

                ox = x4-cposx;
                oy = y4-cposy;
                x4 = dmulscale16(ox, xvect, -oy, yvect);
                y4 = dmulscale16(oy, xvect2, ox, yvect2);

                renderDrawLine(x1+(xdim<<11), y1+(ydim<<11),
                    x2+(xdim<<11), y2+(ydim<<11), col);

                renderDrawLine(x2+(xdim<<11), y2+(ydim<<11),
                    x3+(xdim<<11), y3+(ydim<<11), col);

                renderDrawLine(x3+(xdim<<11), y3+(ydim<<11),
                    x4+(xdim<<11), y4+(ydim<<11), col);

                renderDrawLine(x4+(xdim<<11), y4+(ydim<<11),
                    x1+(xdim<<11), y1+(ydim<<11), col);

                break;
            }
        }
    }

    renderDisableFog();

    //Draw white lines
    for (i=numsectors-1; i>=0; i--)
    {
        if (!(show2dsector[i>>3]&(1<<(i&7)))) continue;

        startwall = sector[i].wallptr;
        endwall = sector[i].wallptr + sector[i].wallnum;

        k = -1;
        for (j=startwall, wal=(uwalltype *)&wall[startwall]; j<endwall; j++, wal++)
        {
            if (wal->nextwall >= 0) continue;

            if (tilesiz[wal->picnum].x == 0) continue;
            if (tilesiz[wal->picnum].y == 0) continue;

            if (j == k)
            {
                x1 = x2;
                y1 = y2;
            }
            else
            {
                ox = wal->x-cposx;
                oy = wal->y-cposy;
                x1 = dmulscale16(ox, xvect, -oy, yvect)+(xdim<<11);
                y1 = dmulscale16(oy, xvect2, ox, yvect2)+(ydim<<11);
            }

            k = wal->point2;
            wal2 = (uwalltype *)&wall[k];
            ox = wal2->x-cposx;
            oy = wal2->y-cposy;
            x2 = dmulscale16(ox, xvect, -oy, yvect)+(xdim<<11);
            y2 = dmulscale16(oy, xvect2, ox, yvect2)+(ydim<<11);

            renderDrawLine(x1, y1, x2, y2, editorcolors[7]);
        }
    }

    renderEnableFog();

    videoSetCorrectedAspect();

    for (TRAVERSE_CONNECT(p))
    {
        if (ud.scrollmode && p == screenpeek) continue;

        DukePlayer_t const * const pPlayer = g_player[p].ps;
        uspritetype const * const pSprite = (uspritetype const *)&sprite[pPlayer->i];

        ox = pSprite->x - cposx;
        oy = pSprite->y - cposy;
        daang = (pSprite->ang - cang) & 2047;
        if (p == screenpeek)
        {
            ox = 0;
            oy = 0;
            daang = 0;
        }
        x1 = mulscale16(ox, xvect) - mulscale16(oy, yvect);
        y1 = mulscale16(oy, xvect2) + mulscale16(ox, yvect2);

        if (p == screenpeek || GTFLAGS(GAMETYPE_OTHERPLAYERSINMAP))
        {
            if (REALITY)
            {
                if (pSprite->xvel > 16 && pPlayer->on_ground)
                    i = 1440-((((4-((int32_t) totalclock>>4)))&3)*5);
                else
                    i = APLAYER;
            }
            else
            {
                if (pSprite->xvel > 16 && pPlayer->on_ground)
                    i = APLAYERTOP+(((int32_t) totalclock>>4)&3);
                else
                    i = APLAYERTOP;
            }

            if (i < 0)
                continue;

            j = klabs(pPlayer->truefz - pPlayer->pos.z) >> 8;
            j = mulscale16(czoom * (pSprite->yrepeat + j), yxaspect);

            if (j < 22000) j = 22000;
            else if (j > (65536<<1)) j = (65536<<1);

            if (REALITY)
                j >>= 1;

            rotatesprite_win((x1<<4)+(xdim<<15), (y1<<4)+(ydim<<15), j, daang, i, pSprite->shade,
                P_GetOverheadPal(pPlayer), 0);
        }
    }
}

#define printcoordsline(fmt, ...) do { \
    Bsprintf(tempbuf, fmt, ## __VA_ARGS__); \
    printext256(20, y+=9, COLOR_WHITE, -1, tempbuf, 0); \
} while (0)

#ifdef DEBUGGINGAIDS
sprstat_t g_spriteStat;
#endif

static void G_PrintCoords(int32_t snum)
{
    const int32_t x = 250;
    int32_t y = 16;

    const DukePlayer_t *ps = g_player[snum].ps;
    const int32_t sectnum = ps->cursectnum;

    if ((g_gametypeFlags[ud.coop] & GAMETYPE_FRAGBAR))
    {
        if (ud.multimode > 4)
            y = 32;
        else if (g_netServer || ud.multimode > 1)
            y = 24;
    }
    Bsprintf(tempbuf, "XYZ= (%d, %d, %d)", ps->pos.x, ps->pos.y, ps->pos.z);
    printext256(x, y, COLOR_WHITE, -1, tempbuf, 0);
    char ang[16], horiz[16], horizoff[16];
    fix16_to_str(ps->q16ang, ang, 2);
    fix16_to_str(ps->q16horiz, horiz, 2);
    fix16_to_str(ps->q16horizoff, horizoff, 2);
    Bsprintf(tempbuf, "A/H/HO= %s, %s, %s", ang, horiz, horizoff);
    printext256(x, y+9, COLOR_WHITE, -1, tempbuf, 0);
    Bsprintf(tempbuf, "VEL= (%d, %d, %d) + (%d, %d, 0)",
        ps->vel.x>>14, ps->vel.y>>14, ps->vel.z, ps->fric.x>>5, ps->fric.y>>5);
    printext256(x, y+18, COLOR_WHITE, -1, tempbuf, 0);
    Bsprintf(tempbuf, "OG= %d  SBRIDGE=%d SBS=%d", ps->on_ground, ps->spritebridge, ps->sbs);
    printext256(x, y+27, COLOR_WHITE, -1, tempbuf, 0);
    if (sectnum >= 0)
        Bsprintf(tempbuf, "SECT= %d (LO=%d EX=%d)", sectnum,
            TrackerCast(sector[sectnum].lotag), TrackerCast(sector[sectnum].extra));
    else
        Bsprintf(tempbuf, "SECT= %d", sectnum);
    printext256(x, y+36, COLOR_WHITE, -1, tempbuf, 0);
    //    Bsprintf(tempbuf,"SEED= %d",randomseed);
    //    printext256(x,y+45,COLOR_WHITE,-1,tempbuf,0);
    y -= 9;

    y += 7;
    Bsprintf(tempbuf, "THOLD= %d", ps->transporter_hold);
    printext256(x, y+54, COLOR_WHITE, -1, tempbuf, 0);
    Bsprintf(tempbuf, "GAMETIC= %u, TOTALCLOCK=%d", g_moveThingsCount, totalclock);
    printext256(x, y+63, COLOR_WHITE, -1, tempbuf, 0);
#ifdef DEBUGGINGAIDS
    Bsprintf(tempbuf, "NUMSPRITES= %d", Numsprites);
    printext256(x, y+72, COLOR_WHITE, -1, tempbuf, 0);
    if (g_moveThingsCount > g_spriteStat.lastgtic + REALGAMETICSPERSEC)
    {
        g_spriteStat.lastgtic = g_moveThingsCount;
        g_spriteStat.lastnumins = g_spriteStat.numins;
        g_spriteStat.numins = 0;
    }
    Bsprintf(tempbuf, "INSERTIONS/s= %u", g_spriteStat.lastnumins);
    printext256(x, y+81, COLOR_WHITE, -1, tempbuf, 0);
    Bsprintf(tempbuf, "ONSCREEN= %d", g_spriteStat.numonscreen);
    printext256(x, y+90, COLOR_WHITE, -1, tempbuf, 0);
    y += 3*9;
#endif
    y += 7;
    Bsprintf(tempbuf, "VR=%.03f  YX=%.03f", (double) dr_viewingrange/65536.0, (double) dr_yxaspect/65536.0);
    printext256(x, y+72, COLOR_WHITE, -1, tempbuf, 0);
    Bsprintf(tempbuf, "MOVEACTORS [ms]= %.3e", g_moveActorsTime);
    printext256(x, y+81, COLOR_WHITE, -1, tempbuf, 0);
    Bsprintf(tempbuf, "MOVEWORLD [ms]= %.3e", g_moveWorldTime);
    printext256(x, y+90, COLOR_WHITE, -1, tempbuf, 0);

#ifdef USE_OPENGL
    if (ud.coords == 2)
    {
        y=16;

        printcoordsline("rendmode = %d", videoGetRenderMode());
        printcoordsline("r_ambientlight = %.03f", r_ambientlight);

        if (rendmode >= 3)
        {
            if (rendmode==3)
                printcoordsline("r_usenewshading = %d", r_usenewshading);
# ifdef POLYMER
            else
                printcoordsline("r_pr_artmapping = %d", pr_artmapping);
#endif
        }
    }
#endif
}

static void G_ShowCacheLocks(void)
{
    if (offscreenrendering)
        return;

    int k = 0;

#if !defined DEBUG_ALLOCACHE_AS_MALLOC
    auto indexes = g_cache.getIndex();

    for (int i=g_cache.numBlocks()-1; i>=0; i--)
    {
        if ((*indexes[i].lock) != CACHE1D_LOCKED && (*indexes[i].lock) != 1)
        {
            if (k >= ydim-12)
                break;

            Bsprintf(tempbuf, "Locked- %d: Leng:%d, Lock:%d", i, indexes[i].leng, *indexes[i].lock);
            printext256(0L, k, COLOR_WHITE, -1, tempbuf, 1);
            k += 6;
        }
    }
#endif

    if (k < ydim-12)
        k += 6;

    for (int i=10; i>=0; i--)
    {
        if (rts_lumplockbyte[i] >= CACHE1D_LOCKED)
        {
            if (k >= ydim-12)
                break;

            Bsprintf(tempbuf, "RTS Locked %d:", i);
            printext256(0, k, COLOR_WHITE, -1, tempbuf, 1);
            k += 6;
        }
    }

    if (k >= ydim-12 && k<ydim-6)
        printext256(0, k, COLOR_WHITE, -1, "(MORE . . .)", 1);

    // sounds
    if (xdim < 640)
        return;

    k = 0;

    for (int i=0; i<=g_highestSoundIdx; i++)
    {
        if (g_sounds[i].num > 0)
        {
            for (int j = 0, n = g_sounds[i].num; j < n; j++)
            {
                if (k >= ydim-12)
                    return;

                Bsprintf(tempbuf, "snd %d_%d: voice %d, ow %d", i, j, g_sounds[i].voices[j].id, g_sounds[i].voices[j].owner);
                printext256(160, k, COLOR_WHITE, -1, tempbuf, 1);

                k += 6;
            }
        }
    }
}

#define LOW_FPS 30
#define SLOW_FRAME_TIME 33

#if defined GEKKO
# define FPS_YOFFSET 16
#else
# define FPS_YOFFSET 0
#endif

#define FPS_COLOR(x) ((x) ? COLOR_RED : COLOR_WHITE)

static void G_PrintFPS(void)
{
    static int32_t frameCount;
    static double cumulativeFrameDelay;
    static double lastFrameTime;
    static float lastFPS, minFPS = std::numeric_limits<float>::max(), maxFPS;
    static double minGameUpdate = std::numeric_limits<double>::max(), maxGameUpdate;

    double frameTime = timerGetFractionalTicks();
    double frameDelay = frameTime - lastFrameTime;
    cumulativeFrameDelay += frameDelay;

    if (frameDelay >= 0)
    {
        int32_t x = (xdim <= 640);

        if (ud.showfps)
        {
            int32_t chars = Bsprintf(tempbuf, "%.1f ms, %5.1f fps", frameDelay, lastFPS);

            printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+2+FPS_YOFFSET, 0, -1, tempbuf, x);
            printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+1+FPS_YOFFSET,
                FPS_COLOR(lastFPS < LOW_FPS), -1, tempbuf, x);

            if (ud.showfps > 1)
            {
                chars = Bsprintf(tempbuf, "max: %5.1f fps", maxFPS);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+10+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+10+FPS_YOFFSET,
                    FPS_COLOR(maxFPS < LOW_FPS), -1, tempbuf, x);

                chars = Bsprintf(tempbuf, "min: %5.1f fps", minFPS);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+20+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+20+FPS_YOFFSET,
                    FPS_COLOR(minFPS < LOW_FPS), -1, tempbuf, x);
            }
            if (ud.showfps > 2)
            {
                if (g_gameUpdateTime > maxGameUpdate) maxGameUpdate = g_gameUpdateTime;
                if (g_gameUpdateTime < minGameUpdate) minGameUpdate = g_gameUpdateTime;

                chars = Bsprintf(tempbuf, "Game Update: %2.2f ms + draw: %2.2f ms", g_gameUpdateTime, g_gameUpdateAndDrawTime);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+30+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+30+FPS_YOFFSET,
                    FPS_COLOR(g_gameUpdateAndDrawTime >= SLOW_FRAME_TIME), -1, tempbuf, x);

                chars = Bsprintf(tempbuf, "GU min/max/avg: %5.2f/%5.2f/%5.2f ms", minGameUpdate, maxGameUpdate, g_gameUpdateAvgTime);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+40+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+40+FPS_YOFFSET,
                    FPS_COLOR(maxGameUpdate >= SLOW_FRAME_TIME), -1, tempbuf, x);

                chars = Bsprintf(tempbuf, "G_MoveActors(): %.3e ms", g_moveActorsTime);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+50+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+50+FPS_YOFFSET,
                    COLOR_WHITE, -1, tempbuf, x);

                chars = Bsprintf(tempbuf, "G_MoveWorld(): %.3e ms", g_moveWorldTime);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+60+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+60+FPS_YOFFSET,
                    COLOR_WHITE, -1, tempbuf, x);
            }

            // lag meter
            if (g_netClientPeer)
            {
                chars = Bsprintf(tempbuf, "%d +- %d ms", (g_netClientPeer->lastRoundTripTime + g_netClientPeer->roundTripTime)/2,
                    (g_netClientPeer->lastRoundTripTimeVariance + g_netClientPeer->roundTripTimeVariance)/2);

                printext256(windowxy2.x-(chars<<(3-x))+1, windowxy1.y+30+2+FPS_YOFFSET, 0, -1, tempbuf, x);
                printext256(windowxy2.x-(chars<<(3-x)), windowxy1.y+30+1+FPS_YOFFSET, FPS_COLOR(g_netClientPeer->lastRoundTripTime > 200), -1, tempbuf, x);
            }
        }

        if (cumulativeFrameDelay >= 1000.0)
        {
            lastFPS = 1000.f * frameCount / cumulativeFrameDelay;
            g_frameRate = Blrintf(lastFPS);
            frameCount = 0;
            cumulativeFrameDelay = 0.0;

            if (ud.showfps > 1)
            {
                if (lastFPS > maxFPS) maxFPS = lastFPS;
                if (lastFPS < minFPS) minFPS = lastFPS;

                static int secondCounter;

                if (++secondCounter >= ud.frameperiod)
                {
                    maxFPS = (lastFPS + maxFPS) * .5f;
                    minFPS = (lastFPS + minFPS) * .5f;
                    maxGameUpdate = (g_gameUpdateTime + maxGameUpdate) * 0.5;
                    minGameUpdate = (g_gameUpdateTime + minGameUpdate) * 0.5;
                    secondCounter = 0;
                }
            }
        }
        frameCount++;
    }
    lastFrameTime = frameTime;
}

#undef FPS_COLOR

void G_DisplayRest(int32_t smoothratio)
{
    int32_t i, j;
    palaccum_t tint = PALACCUM_INITIALIZER;

    DukePlayer_t *const pp = g_player[screenpeek].ps;
#ifdef SPLITSCREEN_MOD_HACKS
    DukePlayer_t *const pp2 = g_fakeMultiMode==2 ? g_player[1].ps : NULL;
#endif
    int32_t cposx, cposy, cang;

#ifdef USE_OPENGL
    // this takes care of fullscreen tint for OpenGL
    if (videoGetRenderMode() >= REND_POLYMOST)
    {
        polytint_t & fstint = hictinting[MAXPALOOKUPS-1];

        if (pp->palette == WATERPAL)
        {
            fstint.r = 224;
            fstint.g = 192;
            fstint.b = 255;
            fstint.f = 0;
        }
        else if (pp->palette == SLIMEPAL)
        {
            fstint.r = 208;
            fstint.g = 255;
            fstint.b = 192;
            fstint.f = 0;
        }
        else
        {
            fstint.r = 255;
            fstint.g = 255;
            fstint.b = 255;
            fstint.f = 0;
        }
    }
#endif  // USE_OPENGL

    if (!REALITY)
        palaccum_add(&tint, &pp->pals, pp->pals.f);
#ifdef SPLITSCREEN_MOD_HACKS
    if (pp2)
        palaccum_add(&tint, &pp2->pals, pp2->pals.f);
#endif
    if (!RR)
    {
        static const palette_t loogiepal = { 0, 63, 0, 0 };

        palaccum_add(&tint, &loogiepal, pp->loogcnt>>1);
#ifdef SPLITSCREEN_MOD_HACKS
        if (pp2)
            palaccum_add(&tint, &loogiepal, pp2->loogcnt>>1);
#endif
    }

    if (g_restorePalette)
    {
        // reset a normal palette
        static uint32_t omovethingscnt;

        if (g_restorePalette < 2 || omovethingscnt+1 == g_moveThingsCount)
        {
            int32_t pal = pp->palette;
#ifdef SPLITSCREEN_MOD_HACKS
            const int32_t opal = pal;

            if (pp2)  // splitscreen HACK: BASEPAL trumps all, then it's arbitrary.
                pal = min(pal, pp2->palette);
#endif

            // g_restorePalette < 0: reset tinting, too (e.g. when loading new game)
            P_SetGamePalette(pp, pal, 2 + (g_restorePalette>0)*16);

#ifdef SPLITSCREEN_MOD_HACKS
            if (pp2)  // keep first player's pal as its member!
                pp->palette = opal;
#endif

            g_restorePalette = 0;
        }
        else
        {
            // delay setting the palette by one game tic
            omovethingscnt = g_moveThingsCount;
        }
    }

    if (ud.show_help)
    {
        switch (ud.show_help)
        {
        case 1:
            rotatesprite_fs(160<<16, 100<<16, 65536L, 0, TEXTSTORY, 0, 0, 10+64);
            break;
        case 2:
            rotatesprite_fs(160<<16, 100<<16, 65536L, 0, F1HELP, 0, 0, 10+64);
            break;
        case 3:
            if (RRRA)
                rotatesprite_fs(160<<16, 100<<16, 65536L, 0, RRTILE1636, 0, 0, 10+64);
            break;
        }

        if (I_ReturnTrigger())
        {
            I_ReturnTriggerClear();
            ud.show_help = 0;
            if ((!g_netServer && ud.multimode < 2) && ud.recstat != 2)
            {
                ready2send = 1;
                totalclock = ototalclock;
            }
            G_UpdateScreenArea();
        }

        return;
    }

    i = pp->cursectnum;
    if (i > -1)
    {
        const walltype *wal = &wall[sector[i].wallptr];

        show2dsector[i>>3] |= (1<<(i&7));
        for (j=sector[i].wallnum; j>0; j--, wal++)
        {
            i = wal->nextsector;
            if (i < 0) continue;
            if (wal->cstat&0x0071) continue;
            if (wall[wal->nextwall].cstat&0x0071) continue;
            if (sector[i].lotag == 32767) continue;
            if (sector[i].ceilingz >= sector[i].floorz) continue;
            show2dsector[i>>3] |= (1<<(i&7));
        }
    }

    if (ud.camerasprite == -1)
    {
        if (ud.overhead_on != 2)
        {
            if (!RR && pp->newowner >= 0)
                G_DrawCameraText(pp->newowner);
            else
            {
                P_DisplayWeapon();
#ifdef SPLITSCREEN_MOD_HACKS
                if (pp2)  // HACK
                {
                    const int32_t oscreenpeek = screenpeek;
                    screenpeek = 1;
                    P_DisplayWeapon();
                    screenpeek = oscreenpeek;
                }
#endif

                if (pp->over_shoulder_on == 0)
                {
                    if (REALITY)
                        RT_P_DisplayScuba();
                    else
                        P_DisplayScuba();
                }
#ifdef SPLITSCREEN_MOD_HACKS
                if (pp2 && pp2->over_shoulder_on == 0)  // HACK
                {
                    const int32_t oscreenpeek = screenpeek;
                    screenpeek = 1;
                    P_DisplayScuba();
                    screenpeek = oscreenpeek;
                }
#endif
            }
            if (!RR)
                G_MoveClouds();
        }

        if (DEER)
            sub_57B38(pp->opos.x, pp->opos.y, 20, 1536);

        if (ud.overhead_on > 0)
        {
            // smoothratio = min(max(smoothratio,0),65536);
            smoothratio = calc_smoothratio(totalclock, ototalclock);
            G_DoInterpolations(smoothratio);

            if (ud.scrollmode == 0)
            {
                if (pp->newowner == -1 && !ud.pause_on)
                {
                    if (screenpeek == myconnectindex && numplayers > 1)
                    {
                        cposx = omypos.x + mulscale16(mypos.x-omypos.x, smoothratio);
                        cposy = omypos.y + mulscale16(mypos.y-omypos.y, smoothratio);
                        cang = fix16_to_int(omyang) + mulscale16((fix16_to_int(myang+F16(1024)-omyang)&2047)-1024, smoothratio);
                    }
                    else
                    {
                        cposx = pp->opos.x + mulscale16(pp->pos.x-pp->opos.x, smoothratio);
                        cposy = pp->opos.y + mulscale16(pp->pos.y-pp->opos.y, smoothratio);
                        cang = fix16_to_int(pp->oq16ang) + mulscale16((fix16_to_int(pp->q16ang+F16(1024)-pp->oq16ang)&2047)-1024, smoothratio);
                    }
                }
                else
                {
                    cposx = pp->opos.x;
                    cposy = pp->opos.y;
                    cang = fix16_to_int(pp->oq16ang);
                }
            }
            else
            {
#ifdef __ANDROID__
                CONTROL_Android_ScrollMap(&ud.fola, &ud.folx, &ud.foly, &pp->zoom);
#else
                if (!ud.pause_on)
                {
                    ud.fola += ud.folavel>>3;
                    ud.folx += (ud.folfvel*sintable[(512+2048-ud.fola)&2047])>>14;
                    ud.foly += (ud.folfvel*sintable[(512+1024-512-ud.fola)&2047])>>14;
                }
#endif
                cposx = ud.folx;
                cposy = ud.foly;
                cang = ud.fola;
            }

            if (ud.overhead_on == 2)
            {
                videoClearViewableArea(0L);
                renderDrawMapView(cposx, cposy, pp->zoom, cang);
            }
            G_DrawOverheadMap(cposx, cposy, pp->zoom, cang);

            G_RestoreInterpolations();

            if (ud.overhead_on == 2)
            {
                const int32_t a = RR ? 0 : ((ud.screen_size > 0) ? 147 : 179);
                if (RR && g_lastLevel)
                    minitext(5, a+6, "CLOSE ENCOUNTERS", 0, 2+8+16+256);
                else
                {
                    if (!REALITY && !G_HaveUserMap())
                        minitext(5, a+6, g_volumeNames[ud.volume_number], 0, 2+8+16+256);
                    minitext(5, a+6+6, g_mapInfo[ud.volume_number*MAXLEVELS + ud.level_number].name, 0, 2+8+16+256);
                }
            }
        }
    }

    if (!REALITY && pp->invdisptime > 0) G_DrawInventory(pp);

    G_DrawStatusBar(screenpeek);

#ifdef SPLITSCREEN_MOD_HACKS
    // HACK
    if (g_fakeMultiMode==2)
    {
        G_DrawStatusBar(1);
        G_PrintGameQuotes(1);
    }
#endif

    G_PrintGameQuotes(screenpeek);

    if (ud.show_level_text && hud_showmapname && g_levelTextTime > 1)
    {
        int32_t o = 10|16;

        if (g_levelTextTime < 3)
            o |= 1|32;
        else if (g_levelTextTime < 5)
            o |= 1;

        if (g_mapInfo[(ud.volume_number*MAXLEVELS) + ud.level_number].name != NULL)
        {
            char const * const fn = currentboardfilename[0] != 0 &&
                ud.volume_number == 0 && ud.level_number == 7
                    ? currentboardfilename
                    : g_mapInfo[(ud.volume_number*MAXLEVELS) + ud.level_number].name;

            if (REALITY)
            {
                RT_DisablePolymost(0);
                RT_RotateSpriteSetColor(255, 128, 128, clamp(g_levelTextTime * 8, 0, 256));
                menutext_(160<<16, (90+16+8)<<16, 0, fn, 0, TEXT_XCENTER|TEXT_N64NOPAL);
                RT_EnablePolymost();
            }
            else
                menutext_(160<<16, (90+16+8)<<16, -g_levelTextTime+22/*quotepulseshade*/, fn, o, TEXT_XCENTER);
        }
    }

    if (I_EscapeTrigger() && ud.overhead_on == 0
        && ud.show_help == 0
        && g_player[myconnectindex].ps->newowner == -1)
    {
        if ((g_player[myconnectindex].ps->gm&MODE_MENU) == MODE_MENU && g_currentMenu <= MENU_MAIN_INGAME)
        {
            I_EscapeTriggerClear();
            S_PlaySound(REALITY ? 0xaa : EXITMENUSOUND);
            Menu_Change(MENU_CLOSE);
            if (!ud.pause_on)
                S_PauseSounds(false);
        }
        else if ((g_player[myconnectindex].ps->gm&MODE_MENU) != MODE_MENU &&
            g_player[myconnectindex].ps->newowner == -1 &&
            (g_player[myconnectindex].ps->gm&MODE_TYPE) != MODE_TYPE)
        {
            I_EscapeTriggerClear();
            S_PauseSounds(true);

            Menu_Open(myconnectindex);

            if ((!g_netServer && ud.multimode < 2) && ud.recstat != 2) ready2send = 0;

            if (g_player[myconnectindex].ps->gm&MODE_GAME) Menu_Change(MENU_MAIN_INGAME);
            else Menu_Change(MENU_MAIN);
            screenpeek = myconnectindex;

            S_MenuSound();
        }
    }

    if (!DEER && g_player[myconnectindex].ps->newowner == -1 && ud.overhead_on == 0 && ud.crosshair && ud.camerasprite == -1)
    {
        int32_t a = CROSSHAIR;
        if ((unsigned) a < MAXTILES)
        {
            vec2_t crosshairpos = { (160<<16) - (g_player[myconnectindex].ps->look_ang<<15), 100<<16 };
            uint8_t crosshair_pal = CROSSHAIR_PAL;
            uint32_t crosshair_o = 1|2;
            uint32_t crosshair_scale = divscale16(ud.crosshairscale, 100);
            if (RR)
                crosshair_scale >>= 1;

            rotatesprite_win(crosshairpos.x, crosshairpos.y, crosshair_scale, 0, a, 0, crosshair_pal, crosshair_o);
        }
    }

#ifdef GEKKO
    // like the mouse cursor, the pointer doesn't use the crosshair enabled / scale options
    if (g_player[myconnectindex].ps->newowner == -1 && ud.overhead_on == 0 && ud.camerasprite == -1 &&
        (g_player[myconnectindex].ps->gm&MODE_MENU) == 0 && mouseReadAbs((vec2_t *)&ud.returnvar[0], &g_mouseAbs))
    {
        int32_t a = VM_OnEventWithReturn(EVENT_DISPLAYPOINTER, g_player[screenpeek].ps->i, screenpeek, CROSSHAIR);
        if ((unsigned) a < MAXTILES)
        {
            vec2_t pointerpos = { tabledivide32(ud.returnvar[0], upscalefactor), tabledivide32(ud.returnvar[1], upscalefactor) };
            uint8_t pointer_pal = CROSSHAIR_PAL;
            uint32_t pointer_o = 1|2;
            uint32_t pointer_scale = 65536;

            auto const oyxaspect = yxaspect;

            rotatesprite_win(pointerpos.x, pointerpos.y, pointer_scale, 0, a, 0, pointer_pal, pointer_o);
        }
    }
#endif
#if 0
    if (g_gametypeFlags[ud.coop] & GAMETYPE_TDM)
    {
        for (i=0; i<ud.multimode; i++)
        {
            if (g_player[i].ps->team == g_player[myconnectindex].ps->team)
            {
                j = min(max((G_GetAngleDelta(getangle(g_player[i].ps->pos.x-g_player[myconnectindex].ps->pos.x,
                    g_player[i].ps->pos.y-g_player[myconnectindex].ps->pos.y), g_player[myconnectindex].ps->ang))>>1, -160), 160);
                rotatesprite_win((160-j)<<16, 100L<<16, 65536L, 0, DUKEICON, 0, 0, 2+1);
            }
        }
    }
#endif

    if (ud.pause_on==1 && (g_player[myconnectindex].ps->gm&MODE_MENU) == 0)
        menutext_center(100, "Game Paused");

    if (ud.coords)
        G_PrintCoords(screenpeek);

#ifdef YAX_DEBUG
    M32_drawdebug();
#endif

#ifdef USE_OPENGL
    mdpause = (ud.pause_on || (ud.recstat==2 && (g_demo_paused && g_demo_goalCnt==0)) || (g_player[myconnectindex].ps->gm&MODE_MENU && numplayers < 2));
#endif

    G_PrintFPS();

    // JBF 20040124: display level stats in screen corner
    if (ud.overhead_on != 2 && ud.levelstats)
    {
        DukePlayer_t const * const myps = g_player[myconnectindex].ps;
        int const sbarshift = RR ? 15 : 16;
        int const ystep = RR ? (10<<16) : (7<<16);

        i = 198<<16;

        if (ud.screen_size == 4)
        {
            if (ud.althud == 0 || ud.hudontop == 0)
                i -= sbarsc(ud.althud ? ((tilesiz[BIGALPHANUM].y<<sbarshift)+(8<<16)) : (REALITY ? 30 : tilesiz[INVENTORYBOX].y)<<sbarshift);
        }
        else if (RR && ud.screen_size == 12)
        {
            i -= sbarsc((tilesiz[BOTTOMSTATUSBAR].y+tilesiz[WEAPONBAR].y)<<sbarshift);
        }
        else if (ud.screen_size > 2)
            i -= sbarsc(tilesiz[BOTTOMSTATUSBAR].y<<sbarshift);

        int32_t const xbetween = (tilesiz[MF_Bluefont.tilenum + 'A' - '!'].x<<16) + MF_Bluefont.between.x;

        int32_t textflags = TEXT_XOFFSETZERO | TEXT_GAMETEXTNUMHACK;

        if (REALITY)
        {
            RT_DisablePolymost(0);
            RT_RotateSpriteSetColor(255, 80, 0, 256);
            textflags |= TEXT_N64NOPAL;
        }

        Bsprintf(tempbuf, "T:^15%d:%02d.%02d",
            (myps->player_par/(REALGAMETICSPERSEC*60)),
            (myps->player_par/REALGAMETICSPERSEC)%60,
            ((myps->player_par%REALGAMETICSPERSEC)*33)/10
            );
        G_ScreenText(MF_Bluefont.tilenum, 2<<16, i-gtextsc(ystep*((REALITY ? 4 : 3) + (AP ? 1 : 0))), gtextsc(MF_Bluefont.zoom), 0, 0, tempbuf, 0, 10, 2|8|16|256|ROTATESPRITE_FULL16, 0, MF_Bluefont.emptychar.x, MF_Bluefont.emptychar.y, xbetween, MF_Bluefont.between.y, MF_Bluefont.textflags|textflags, 0, 0, xdim-1, ydim-1);

        if ((!RR && ud.player_skill > 3) || ((g_netServer || ud.multimode > 1) && !GTFLAGS(GAMETYPE_PLAYERSFRIENDLY)))
            Bsprintf(tempbuf, "K:^15%d", (ud.multimode>1 &&!GTFLAGS(GAMETYPE_PLAYERSFRIENDLY)) ?
                myps->frag-myps->fraggedself : myps->actors_killed);
        else
        {
            if (myps->actors_killed >= myps->max_actors_killed)
                Bsprintf(tempbuf, "K:%d/%d", myps->actors_killed, myps->actors_killed);
            else
                Bsprintf(tempbuf, "K:^15%d/%d", myps->actors_killed, myps->max_actors_killed);
        }
        G_ScreenText(MF_Bluefont.tilenum, 2<<16, i-gtextsc(ystep*((REALITY ? 3 : 2) + (AP ? 1 : 0))), gtextsc(MF_Bluefont.zoom), 0, 0, tempbuf, 0, 10, 2|8|16|256|ROTATESPRITE_FULL16, 0, MF_Bluefont.emptychar.x, MF_Bluefont.emptychar.y, xbetween, MF_Bluefont.between.y, MF_Bluefont.textflags|textflags, 0, 0, xdim-1, ydim-1);

        if (myps->secret_rooms == myps->max_secret_rooms)
            Bsprintf(tempbuf, "S:%d/%d", myps->secret_rooms, myps->max_secret_rooms);
        else Bsprintf(tempbuf, "S:^15%d/%d", myps->secret_rooms, myps->max_secret_rooms);
        G_ScreenText(MF_Bluefont.tilenum, 2<<16, i-gtextsc(ystep), gtextsc(MF_Bluefont.zoom), 0, 0, tempbuf, 0, 10, 2|8|16|256|ROTATESPRITE_FULL16, 0, MF_Bluefont.emptychar.x, MF_Bluefont.emptychar.y, xbetween, MF_Bluefont.between.y, MF_Bluefont.textflags|textflags, 0, 0, xdim-1, ydim-1);
        
        if (AP)
        {
            uint16_t ap_collected = 0;
            uint16_t ap_total = 0;
            ap_remaining_items(&ap_collected, &ap_total);
            if (ap_collected < ap_total)
                Bsprintf(tempbuf, "AP:^15%d/%d", ap_collected, ap_total);
            else Bsprintf(tempbuf, "AP:%d/%d", ap_collected, ap_total);
            G_ScreenText(MF_Bluefont.tilenum, 2<<16, i-gtextsc(ystep*2), gtextsc(MF_Bluefont.zoom), 0, 0, tempbuf, 0, 10, 2|8|16|256|ROTATESPRITE_FULL16, 0, MF_Bluefont.emptychar.x, MF_Bluefont.emptychar.y, xbetween, MF_Bluefont.between.y, MF_Bluefont.textflags|textflags, 0, 0, xdim-1, ydim-1);
        }

        if (REALITY)
        {
            Bsprintf(tempbuf, "B:%d/%d",
                myps->dn64_36e, myps->dn64_36d
                );
            G_ScreenText(MF_Bluefont.tilenum, 2<<16, i-gtextsc(ystep*(AP ? 3 : 2)), gtextsc(MF_Bluefont.zoom), 0, 0, tempbuf, 0, 10, 2|8|16|256|ROTATESPRITE_FULL16, 0, MF_Bluefont.emptychar.x, MF_Bluefont.emptychar.y, xbetween, MF_Bluefont.between.y, MF_Bluefont.textflags|textflags, 0, 0, xdim-1, ydim-1);

            RT_EnablePolymost();
        }
    }

    if (g_player[myconnectindex].gotvote == 0 && voting != -1 && voting != myconnectindex)
    {
        Bsprintf(tempbuf, "%s^00 has called a vote for map", g_player[voting].user_name);
        gametext_center(40, tempbuf);
        Bsprintf(tempbuf, "%s (E%dL%d)", g_mapInfo[vote_episode*MAXLEVELS + vote_map].name, vote_episode+1, vote_map+1);
        gametext_center(48, tempbuf);
        gametext_center(70, "Press F1 to Accept, F2 to Decline");
    }

    if (BUTTON(gamefunc_Show_DukeMatch_Scores))
        G_ShowScores();

    if (g_Debug)
        G_ShowCacheLocks();

    Net_DisplaySyncMsg();

#ifndef EDUKE32_TOUCH_DEVICES
    if (VOLUMEONE)
    {
        if (ud.show_help == 0 && g_showShareware > 0 && (g_player[myconnectindex].ps->gm&MODE_MENU) == 0)
            rotatesprite_fs((320-50)<<16, 9<<16, 65536L, 0, BETAVERSION, 0, 0, 2+8+16+128);
    }
#endif

    if (!Demo_IsProfiling())
    {
        if (g_player[myconnectindex].ps->gm&MODE_TYPE)
            Net_SendMessage();
        else
            M_DisplayMenus();
    }

    {
        static int32_t applied = 0;

        if (tint.maxf)
        {
            G_FadePalaccum(&tint);
            applied = 1;
        }
        else if (applied)
        {
            // be sure to always un-apply a tint.
            videoFadePalette(0, 0, 0, 0);
            applied = 0;
        }
    }
}

void G_FadePalette(int32_t r, int32_t g, int32_t b, int32_t e)
{
    if (ud.screenfade == 0)
      return;
    videoFadePalette(r, g, b, e);
    videoNextPage();

    int32_t tc = (int32_t) totalclock;
    while (totalclock < tc + 4)
        G_HandleAsync();
}

// START and END limits are always inclusive!
// STEP must evenly divide END-START, i.e. abs(end-start)%step == 0
void fadepal(int32_t r, int32_t g, int32_t b, int32_t start, int32_t end, int32_t step)
{
    if (ud.screenfade == 0)
      return;
    if (videoGetRenderMode() >= REND_POLYMOST)
    {
        G_FadePalette(r, g, b, end);
        return;
    }

    // (end-start)/step + 1 iterations
    do
    {
        if (KB_KeyPressed(sc_Space))
        {
            KB_ClearKeyDown(sc_Space);
            videoFadePalette(r, g, b, end);  // have to set to end fade value if we break!
            return;
        }

        G_FadePalette(r, g, b, start);

        start += step;
    } while (start != end+step);
}

// START and END limits are always inclusive!
static void fadepaltile(int32_t r, int32_t g, int32_t b, int32_t start, int32_t end, int32_t step, int32_t tile)
{
    if (ud.screenfade == 0)
      return;
    // STEP must evenly divide END-START
    Bassert(klabs(end-start)%step == 0);

    videoClearScreen(0);

    // (end-start)/step + 1 iterations
    do
    {
#ifdef __ANDROID__ //Needed for N7 2013 to stop corruption while fading video
        videoClearViewableArea(0);
#endif
        if (KB_KeyPressed(sc_Space))
        {
            KB_ClearKeyDown(sc_Space);
            videoFadePalette(r, g, b, end);  // have to set to end fade value if we break!
            return;
        }
        rotatesprite_fs(160<<16, 100<<16, 65536L, 0, tile, 0, 0, 2+8+64+BGSTRETCH);
        G_FadePalette(r, g, b, start);

        start += step;
    } while (start != end+step);
}

#ifdef __ANDROID__
int inExtraScreens = 0;
#endif

void G_DisplayExtraScreens(void)
{
    S_StopMusic();
    FX_StopAllSounds();
    if (RR)
        return;

    if (!VOLUMEALL)
    {
#ifdef __ANDROID__
        inExtraScreens = 1;
#endif
        videoSetViewableArea(0, 0, xdim-1, ydim-1);
        renderFlushPerms();
        //g_player[myconnectindex].ps->palette = palette;
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 1);    // JBF 20040308
        fadepal(0, 0, 0, 0, 252, 28);
        I_ClearAllInput();
        rotatesprite_fs(160<<16, 100<<16, 65536L, 0, 3291, 0, 0, 2+8+64+BGSTRETCH);
        fadepaltile(0, 0, 0, 252, 0, -28, 3291);
        while (!I_CheckAllInput())
            G_HandleAsync();

        fadepaltile(0, 0, 0, 0, 252, 28, 3291);
        I_ClearAllInput();
        rotatesprite_fs(160<<16, 100<<16, 65536L, 0, 3290, 0, 0, 2+8+64+BGSTRETCH);
        fadepaltile(0, 0, 0, 252, 0, -28, 3290);
        while (!I_CheckAllInput())
            G_HandleAsync();

#ifdef __ANDROID__
        inExtraScreens = 0;
#endif
    }

    if (0)
    {
#ifdef __ANDROID__
        inExtraScreens = 1;
#endif
        videoSetViewableArea(0, 0, xdim-1, ydim-1);
        renderFlushPerms();
        //g_player[myconnectindex].ps->palette = palette;
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 1);    // JBF 20040308
        fadepal(0, 0, 0, 0, 252, 28);
        I_ClearAllInput();
        totalclock = 0;
        rotatesprite_fs(160<<16, 100<<16, 65536L, 0, TENSCREEN, 0, 0, 2+8+64+BGSTRETCH);
        fadepaltile(0, 0, 0, 252, 0, -28, TENSCREEN);
        while (!I_CheckAllInput() && totalclock < 2400)
            G_HandleAsync();

        fadepaltile(0, 0, 0, 0, 252, 28, TENSCREEN);
        I_ClearAllInput();
#ifdef __ANDROID__
        inExtraScreens = 0;
#endif
    }
}

void G_DisplayLogo(void)
{
    int32_t soundanm = 0;
    //int32_t logoflags = G_GetLogoFlags();

    ready2send = 0;

    I_ClearAllInput();

    videoSetViewableArea(0, 0, xdim-1, ydim-1);
    videoClearScreen(0L);
    G_FadePalette(0, 0, 0, 252);

    renderFlushPerms();
    videoNextPage();

    G_UpdateAppTitle();

    S_StopMusic();
    FX_StopAllSounds(); // JBF 20031228
    S_ClearSoundLocks();  // JBF 20031228

    // [AP] Skip logos and opening scenes when returning to menu from in-game
    g_noLogo = ap_return_to_menu;
    if (DEER)
    {
        if (!g_noLogo /* && (!g_netServer && ud.multimode < 2) */)
        {
            if (!I_CheckAllInput() && g_noLogoAnim == 0)
            {
                videoClearScreen(0);

                P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 8 + 2 + 1);    // JBF 20040308
                fadepal(0, 0, 0, 0, 252, 28);
                renderFlushPerms();
                rotatesprite_fs(160 << 16, 100 << 16, 65536L, 0, 7106, 0, 0, 2 + 8 + 64 + BGSTRETCH);
                videoNextPage();
                fadepaltile(0, 0, 0, 252, 0, -4, 7106);
                totalclock = 0;

                while (totalclock < (120 * 3) && !I_CheckAllInput())
                {
                    if (engineFPSLimit())
                    {
                        videoClearScreen(0);
                        rotatesprite_fs(160 << 16, 100 << 16, 65536L, 0, 7106, 0, 0, 2 + 8 + 64 + BGSTRETCH);
                        G_HandleAsync();

                        if (g_restorePalette)
                        {
                            P_SetGamePalette(g_player[myconnectindex].ps, g_player[myconnectindex].ps->palette, 0);
                            g_restorePalette = 0;
                        }
                        videoNextPage();
                    }
                }

                fadepaltile(0, 0, 0, 0, 252, 4, 7106);
            }

            videoClearScreen(0L);
            videoNextPage();

            I_ClearAllInput();

            videoClearScreen(0L);
            videoNextPage();

            videoClearScreen(0);

            //g_player[myconnectindex].ps->palette = drealms;
            //G_FadePalette(0,0,0,252);

            if (!I_CheckAllInput() && g_noLogoAnim == 0)
            {
                videoClearScreen(0);

                P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 8 + 2 + 1);    // JBF 20040308
                fadepal(0, 0, 0, 0, 252, 28);
                renderFlushPerms();
                rotatesprite_fs(160 << 16, 100 << 16, 65536L, 0, 7107, 0, 0, 2 + 8 + 64 + BGSTRETCH);
                videoNextPage();
                fadepaltile(0, 0, 0, 252, 0, -4, 7107);
                totalclock = 0;

                while (totalclock < (120 * 3) && !I_CheckAllInput())
                {
                    if (engineFPSLimit())
                    {
                        videoClearScreen(0);
                        rotatesprite_fs(160 << 16, 100 << 16, 65536L, 0, 7107, 0, 0, 2 + 8 + 64 + BGSTRETCH);
                        G_HandleAsync();

                        if (g_restorePalette)
                        {
                            P_SetGamePalette(g_player[myconnectindex].ps, g_player[myconnectindex].ps->palette, 0);
                            g_restorePalette = 0;
                        }
                        videoNextPage();
                    }
                }

                fadepaltile(0, 0, 0, 0, 252, 4, 7107);
            }

            I_ClearAllInput();
        }

        renderFlushPerms();
        videoClearScreen(0L);
        videoNextPage();

        //g_player[myconnectindex].ps->palette = palette;
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 0);    // JBF 20040308

        //G_FadePalette(0,0,0,0);
        videoClearScreen(0L);

        g_noLogo = 1; // Play intro only once
        return;
    }
    if (RRRA)
        return;
    if (RR)
    {
        if (!I_CheckAllInput() && g_noLogoAnim == 0)
        {
            Net_GetPackets();
            Anim_Play("rr_intro.anm");
            G_FadePalette(0, 0, 0, 252);
            I_ClearAllInput();
        }

        videoClearScreen(0L);
        videoNextPage();
        FX_StopAllSounds();
        S_ClearSoundLocks();

        if (!I_CheckAllInput() && g_noLogoAnim == 0)
        {
            Net_GetPackets();
            Anim_Play("redneck.anm");
            G_FadePalette(0, 0, 0, 252);
            I_ClearAllInput();
        }

        videoClearScreen(0L);
        videoNextPage();
        FX_StopAllSounds();
        S_ClearSoundLocks();

        if (!I_CheckAllInput() && g_noLogoAnim == 0)
        {
            Net_GetPackets();
            Anim_Play("xatlogo.anm");
            G_FadePalette(0, 0, 0, 252);
            I_ClearAllInput();
        }

        videoClearScreen(0L);
        videoNextPage();
        FX_StopAllSounds();
        S_ClearSoundLocks();

        //g_player[myconnectindex].ps->palette = palette;
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 0);    // JBF 20040308
        S_PlaySound(NITEVISION_ONOFF);

        //G_FadePalette(0,0,0,0);
        videoClearScreen(0L);
        return;
    }
    if (REALITY)
    {
        G_FadePalette(0, 0, 0, 0);
        totalclock = 0;

        while ((int)totalclock < (4 * (16 + 120 + 16)) && !I_CheckAllInput())
        {
            if (engineFPSLimit())
            {
                int alpha = 0;
                if (totalclock < (4 * 16))
                    alpha = clamp((int)totalclock * 4, 0, 256);
                else if (totalclock < (4 * (16 + 120)))
                    alpha = 256;
                else
                    alpha = clamp(256 - ((int)totalclock - (4 * (16 + 120))) * 4, 0, 256);


                videoClearScreen(0);

                RT_DisablePolymost(0);
                RT_RotateSpriteSetColor(255, 255, 255, alpha);

                RT_RotateSprite(160, 60, 100, 100, 0xe09, 0);
                RT_RotateSprite(100, 160, 100, 100, 0xe5d, 0);
                RT_RotateSprite(220, 160, 100, 100, 0xe5e, 0);
                RT_EnablePolymost();

                G_HandleAsync();
                videoNextPage();
            }
        }

        I_ClearAllInput();

        S_PlaySpecialMusicOrNothing(MUS_INTRO);

        renderFlushPerms();
        videoClearScreen(0L);
        videoNextPage();

        //g_player[myconnectindex].ps->palette = palette;
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 0);    // JBF 20040308

        //G_FadePalette(0,0,0,0);
        videoClearScreen(0L);
        return;
    }
    if (!g_noLogo /* && (!g_netServer && ud.multimode < 2) */)
    {
#ifndef EDUKE32_TOUCH_DEVICES
        if (VOLUMEALL)
#endif
        {
            if (!I_CheckAllInput() && g_noLogoAnim == 0)
            {
                Net_GetPackets();
                Anim_Play("logo.anm");
                G_FadePalette(0, 0, 0, 252);
                I_ClearAllInput();
            }

            videoClearScreen(0L);
            videoNextPage();
            FX_StopAllSounds();
            S_ClearSoundLocks();
        }

        S_PlaySpecialMusicOrNothing(MUS_INTRO);

        //g_player[myconnectindex].ps->palette = drealms;
        //G_FadePalette(0,0,0,252);

        if (!I_CheckAllInput() && g_noLogoAnim == 0)
        {
            int32_t i;
            Net_GetPackets();

            i = kopen4loadfrommod("3dr.ivf", 0);

            if (i == -1)
                i = kopen4loadfrommod("3dr.anm", 0);

            if (i != -1)
            {
                kclose(i);
                Anim_Play("3dr.anm");
                G_FadePalette(0, 0, 0, 252);
                I_ClearAllInput();
            }
            else
            {
                videoClearScreen(0);

                P_SetGamePalette(g_player[myconnectindex].ps, DREALMSPAL, 8 + 2 + 1);    // JBF 20040308
                fadepal(0, 0, 0, 0, 252, 28);
                renderFlushPerms();
                rotatesprite_fs(160 << 16, 100 << 16, 65536L, 0, DREALMS, 0, 0, 2 + 8 + 64 + BGSTRETCH);
                videoNextPage();
                fadepaltile(0, 0, 0, 252, 0, -28, DREALMS);
                totalclock = 0;

                while (totalclock < (120 * 7) && !I_CheckAllInput())
                {
                    if (engineFPSLimit())
                    {
                        videoClearScreen(0);
                        rotatesprite_fs(160 << 16, 100 << 16, 65536L, 0, DREALMS, 0, 0, 2 + 8 + 64 + BGSTRETCH);
                        G_HandleAsync();

                        if (g_restorePalette)
                        {
                            P_SetGamePalette(g_player[myconnectindex].ps, g_player[myconnectindex].ps->palette, 0);
                            g_restorePalette = 0;
                        }
                        videoNextPage();
                    }
                }

                fadepaltile(0, 0, 0, 0, 252, 28, DREALMS);
            }
        }

        videoClearScreen(0L);
        videoNextPage();

        I_ClearAllInput();

        videoClearScreen(0L);
        videoNextPage();

        videoClearScreen(0);

        //g_player[myconnectindex].ps->palette = titlepal;
        P_SetGamePalette(g_player[myconnectindex].ps, TITLEPAL, 8+2+1);   // JBF 20040308
        renderFlushPerms();
        rotatesprite_fs(160<<16, 100<<16, 65536L, 0, BETASCREEN, 0, 0, 2+8+64+BGSTRETCH);
        KB_FlushKeyboardQueue();
        fadepaltile(0, 0, 0, 252, 0, -28, BETASCREEN);
        totalclock = 0;

        while (
#ifndef EDUKE32_SIMPLE_MENU
            totalclock < (860+120) &&
#endif
            !I_CheckAllInput())
        {
            if (engineFPSLimit())
            {
                videoClearScreen(0);
                rotatesprite_fs(160<<16, 100<<16, 65536L, 0, BETASCREEN, 0, 0, 2+8+64+BGSTRETCH);

                if (totalclock > 120 && totalclock < (120+60))
                {
                    if (soundanm == 0)
                    {
                        soundanm++;
                        S_PlaySound(PIPEBOMB_EXPLODE);
                    }
                    rotatesprite_fs(160<<16, 104<<16, ((int32_t) totalclock-120)<<10, 0, DUKENUKEM, 0, 0, 2+8);
                }
                else if (totalclock >= (120+60))
                    rotatesprite_fs(160<<16, (104)<<16, 60<<10, 0, DUKENUKEM, 0, 0, 2+8);

                if (totalclock > 220 && totalclock < (220+30))
                {
                    if (soundanm == 1)
                    {
                        soundanm++;
                        S_PlaySound(PIPEBOMB_EXPLODE);
                    }

                    rotatesprite_fs(160<<16, (104)<<16, 60<<10, 0, DUKENUKEM, 0, 0, 2+8);
                    rotatesprite_fs(160<<16, (129)<<16, ((int32_t) totalclock - 220)<<11, 0, THREEDEE, 0, 0, 2+8);
                }
                else if (totalclock >= (220+30))
                    rotatesprite_fs(160<<16, (129)<<16, 30<<11, 0, THREEDEE, 0, 0, 2+8);

                if (PLUTOPAK)
                {
                    // JBF 20030804
                    if (totalclock >= 280 && totalclock < 395)
                    {
                        rotatesprite_fs(160<<16, (151)<<16, (410-(int32_t) totalclock)<<12, 0, PLUTOPAKSPRITE+1, (sintable[((int32_t) totalclock<<4)&2047]>>11), 0, 2+8);
                        if (soundanm == 2)
                        {
                            soundanm++;
                            S_PlaySound(FLY_BY);
                        }
                    }
                    else if (totalclock >= 395)
                    {
                        if (soundanm == 3)
                        {
                            soundanm++;
                            S_PlaySound(PIPEBOMB_EXPLODE);
                        }
                        rotatesprite_fs(160<<16, (151)<<16, 30<<11, 0, PLUTOPAKSPRITE+1, (sintable[((int32_t) totalclock<<4)&2047]>>11), 0, 2+8);
                    }
                }

                if (g_restorePalette)
                {
                    P_SetGamePalette(g_player[myconnectindex].ps, g_player[myconnectindex].ps->palette, 0);
                    g_restorePalette = 0;
                }

                videoNextPage();
            }

            G_HandleAsync();
        }

        I_ClearAllInput();
    }

    renderFlushPerms();
    videoClearScreen(0L);
    videoNextPage();

    //g_player[myconnectindex].ps->palette = palette;
    P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 0);    // JBF 20040308
    S_PlaySound(NITEVISION_ONOFF);

    //G_FadePalette(0,0,0,0);
    videoClearScreen(0L);

    ap_return_to_menu = 0;
}

void G_DoOrderScreen(void)
{
    int32_t i;

    videoSetViewableArea(0, 0, xdim-1, ydim-1);

    P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 1);    // JBF 20040308

    for (i=0; i<4; i++)
    {
        fadepal(0, 0, 0, 0, 252, 28);
        I_ClearAllInput();
        rotatesprite_fs(160<<16, 100<<16, 65536L, 0, ORDERING+i, 0, 0, 2+8+64+BGSTRETCH);
        fadepal(0, 0, 0, 252, 0, -28);
        while (!I_CheckAllInput())
            G_HandleAsync();
    }

    I_ClearAllInput();
}


static void G_BonusCutscenes(void)
{
    if (RRRA)
        return;
    if (!(numplayers < 2 && ud.eog && ud.from_bonus == 0))
        return;

    if (RR)
    {
        switch (ud.volume_number)
        {
        case 0:
            videoClearScreen(0L);
            videoNextPage();
            if (ud.lockout == 0)
            {
                Anim_Play("turdmov.anm");
                I_ClearAllInput();
                videoClearScreen(0L);
                videoNextPage();
            }
            ud.level_number = 0;
            ud.volume_number = 1;
            ud.eog = 0;
            fadepal(0, 0, 0, 0, 252, 4);
            I_ClearAllInput();
            P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 8+2+1);
            rotatesprite_fs(0, 0, 65536L, 0, TENSCREEN, 0, 0, 2+8+16+64+128+BGSTRETCH);
            videoNextPage();
            fadepal(0, 0, 0, 252, 0, -4);
            I_ClearAllInput();
            G_HandleEventsWhileNoInput();
            fadepal(0, 0, 0, 0, 252, 4);
            FX_StopAllSounds();
            S_ClearSoundLocks();
            break;
        case 1:
            videoClearScreen(0L);
            videoNextPage();
            if (ud.lockout == 0)
            {
                Anim_Play("rr_outro.anm");
                I_ClearAllInput();
                videoClearScreen(0L);
                videoNextPage();
            }
            g_lastLevel = 0;
            g_vixenLevel = 1;
            ud.level_number = 0;
            ud.volume_number = 0;
            fadepal(0, 0, 0, 0, 252, 4);
            videoSetViewableArea(0, 0, xdim-1, ydim-1);
            I_ClearAllInput();
            P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 8 + 2 + 1);
            rotatesprite_fs(0, 0, 65536L, 0, TENSCREEN, 0, 0, 2 + 8 + 16 + 64 + 128 + BGSTRETCH);
            videoNextPage();
            fadepal(0, 0, 0, 252, 0, -4);
            I_ClearAllInput();
            G_HandleEventsWhileNoInput();
            fadepal(0, 0, 0, 0, 252, 4);
            FX_StopAllSounds();
            S_ClearSoundLocks();
            break;
        }
        return;
    }

    switch (ud.volume_number)
    {
    case 0:
        if (ud.lockout == 0)
        {
            int32_t bonuscnt=0;
            int32_t const bossmove [] =
            {
                0, 120, VICTORY1+3, 86, 59,
                220, 260, VICTORY1+4, 86, 59,
                260, 290, VICTORY1+5, 86, 59,
                290, 320, VICTORY1+6, 86, 59,
                320, 350, VICTORY1+7, 86, 59,
                350, 380, VICTORY1+8, 86, 59,
                350, 380, VICTORY1+8, 86, 59 // duplicate row to alleviate overflow in the for loop below "boss"
            };

            P_SetGamePalette(g_player[myconnectindex].ps, ENDINGPAL, 8+2+1); // JBF 20040308
            videoClearScreen(0L);
            rotatesprite_fs(0, 50<<16, 65536L, 0, VICTORY1, 0, 0, 2+8+16+64+128+BGSTRETCH);
            videoNextPage();
            fadepal(0, 0, 0, 252, 0, -4);

            I_ClearAllInput();
            totalclock = 0;

            while (1)
            {
                if (engineFPSLimit())
                {
                    videoClearScreen(0L);
                    rotatesprite_fs(0, 50<<16, 65536L, 0, VICTORY1, 0, 0, 2+8+16+64+128+BGSTRETCH);

                    // boss
                    if (totalclock > 390 && totalclock < 780)
                        for (bssize_t t=0; t<35; t+=5) if (bossmove[t+2] && (totalclock%390) > bossmove[t] && (totalclock%390) <= bossmove[t+1])
                        {
                            if (t==10 && bonuscnt == 1)
                            {
                                S_PlaySound(SHOTGUN_FIRE);
                                S_PlaySound(SQUISHED);
                                bonuscnt++;
                            }
                            rotatesprite_fs(bossmove[t+3]<<16, bossmove[t+4]<<16, 65536L, 0, bossmove[t+2], 0, 0, 2+8+16+64+128+BGSTRETCH);
                        }

                    // Breathe
                    if (totalclock < 450 || totalclock >= 750)
                    {
                        int32_t const breathe [] =
                        {
                            0,  30, VICTORY1+1, 176, 59,
                            30,  60, VICTORY1+2, 176, 59,
                            60,  90, VICTORY1+1, 176, 59,
                            90, 120,          0, 176, 59
                        };

                        if (totalclock >= 750)
                        {
                            rotatesprite_fs(86<<16, 59<<16, 65536L, 0, VICTORY1+8, 0, 0, 2+8+16+64+128+BGSTRETCH);
                            if (totalclock >= 750 && bonuscnt == 2)
                            {
                                S_PlaySound(DUKETALKTOBOSS);
                                bonuscnt++;
                            }

                        }
                        for (bssize_t t=0; t<20; t+=5)
                            if (breathe[t+2] && (totalclock%120) > breathe[t] && (totalclock%120) <= breathe[t+1])
                            {
                                if (t==5 && bonuscnt == 0)
                                {
                                    S_PlaySound(BOSSTALKTODUKE);
                                    bonuscnt++;
                                }
                                rotatesprite_fs(breathe[t+3]<<16, breathe[t+4]<<16, 65536L, 0, breathe[t+2], 0, 0, 2+8+16+64+128+BGSTRETCH);
                            }
                    }

                    videoNextPage();
                }

                G_HandleAsync();

                if (I_CheckAllInput()) break;
            }

            fadepal(0, 0, 0, 0, 252, 4);
        }

        I_ClearAllInput();
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 8+2+1);   // JBF 20040308

        rotatesprite_fs(160<<16, 100<<16, 65536L, 0, 3292, 0, 0, 2+8+64+BGSTRETCH);
        fadepal(0, 0, 0, 252, 0, -4);
        G_HandleEventsWhileNoInput();
        fadepal(0, 0, 0, 0, 252, 4);

        S_StopMusic();
        FX_StopAllSounds();
        S_ClearSoundLocks();
        break;

    case 1:
        videoSetViewableArea(0, 0, xdim-1, ydim-1);

        S_StopMusic();
        videoClearScreen(0L);
        videoNextPage();

        if (ud.lockout == 0)
        {
            fadepal(0, 0, 0, 252, 0, -4);
            Anim_Play("cineov2.anm");
            I_ClearAllInput();
            videoClearScreen(0L);
            videoNextPage();

            S_PlaySound(PIPEBOMB_EXPLODE);
            fadepal(0, 0, 0, 0, 252, 4);
        }

        I_ClearAllInput();
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 8+2+1);   // JBF 20040308
        rotatesprite_fs(160<<16, 100<<16, 65536L, 0, 3293, 0, 0, 2+8+64+BGSTRETCH);
        fadepal(0, 0, 0, 252, 0, -4);
        G_HandleEventsWhileNoInput();
        fadepal(0, 0, 0, 0, 252, 4);

        break;

    case 3:
        videoSetViewableArea(0, 0, xdim-1, ydim-1);

        S_StopMusic();
        videoClearScreen(0L);
        videoNextPage();

        if (ud.lockout == 0)
        {
            fadepal(0, 0, 0, 252, 0, -4);

            I_ClearAllInput();
            int t = Anim_Play("vol4e1.anm");
            videoClearScreen(0L);
            videoNextPage();
            if (t)
                goto end_vol4e;

            t = Anim_Play("vol4e2.anm");
            videoClearScreen(0L);
            videoNextPage();
            if (t)
                goto end_vol4e;

            Anim_Play("vol4e3.anm");
            videoClearScreen(0L);
            videoNextPage();
        }

    end_vol4e:
        FX_StopAllSounds();
        S_ClearSoundLocks();
        S_PlaySound(ENDSEQVOL3SND4);
        I_ClearAllInput();

        G_FadePalette(0, 0, 0, 0);
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 8+2+1);   // JBF 20040308
                                                                         //        G_FadePalette(0,0,0,252);
        videoClearScreen(0L);
        menutext_center(60, "Thanks to all our");
        menutext_center(60+16, "fans for giving");
        menutext_center(60+16+16, "us big heads.");
        menutext_center(70+16+16+16, "Look for a Duke Nukem 3D");
        menutext_center(70+16+16+16+16, "sequel soon.");
        videoNextPage();

        fadepal(0, 0, 0, 252, 0, -12);
        videoNextPage();
        I_ClearAllInput();
        G_HandleEventsWhileNoInput();
        fadepal(0, 0, 0, 0, 252, 12);

        videoClearScreen(0L);
        videoNextPage();

        Anim_Play("DUKETEAM.ANM");

        I_ClearAllInput();
        G_HandleEventsWhileNoInput();

        videoClearScreen(0L);
        videoNextPage();
        G_FadePalette(0, 0, 0, 252);

        FX_StopAllSounds();
        S_ClearSoundLocks();
        I_ClearAllInput();

        break;

    case 2:
        S_StopMusic();
        videoClearScreen(0L);
        videoNextPage();
        if (!AP && ud.lockout == 0)
        {
            fadepal(0, 0, 0, 252, 0, -4);
            Anim_Play("cineov3.anm");
            I_ClearAllInput();
            ototalclock = totalclock+200;
            while (totalclock < ototalclock)
                G_HandleAsync();
            videoClearScreen(0L);
            videoNextPage();

            FX_StopAllSounds();
            S_ClearSoundLocks();
        }

        Anim_Play("RADLOGO.ANM");

        if (ud.lockout == 0 && !I_CheckAllInput())
        {
            if (G_PlaySoundWhileNoInput(ENDSEQVOL3SND5)) goto ENDANM;
            if (G_PlaySoundWhileNoInput(ENDSEQVOL3SND6)) goto ENDANM;
            if (G_PlaySoundWhileNoInput(ENDSEQVOL3SND7)) goto ENDANM;
            if (G_PlaySoundWhileNoInput(ENDSEQVOL3SND8)) goto ENDANM;
            if (G_PlaySoundWhileNoInput(ENDSEQVOL3SND9)) goto ENDANM;
        }

        I_ClearAllInput();

        totalclock = 0;
        if (PLUTOPAK)
        {
            while (totalclock < 120 && !I_CheckAllInput())
                G_HandleAsync();

            I_ClearAllInput();
        }
        else
        {
            G_HandleEventsWhileNoInput();
        }

    ENDANM:
        if (!PLUTOPAK)
        {
            FX_StopAllSounds();
            S_ClearSoundLocks();
            S_PlaySound(ENDSEQVOL3SND4);

            videoClearScreen(0L);
            videoNextPage();

            Anim_Play("DUKETEAM.ANM");

            I_ClearAllInput();
            G_HandleEventsWhileNoInput();

            videoClearScreen(0L);
            videoNextPage();
            G_FadePalette(0, 0, 0, 252);
        }

        I_ClearAllInput();
        FX_StopAllSounds();
        S_ClearSoundLocks();

        videoClearScreen(0L);

        break;
    }
}

static void G_DisplayMPResultsScreen(void)
{
    int32_t i, y, t = 0;

    rotatesprite_fs(160<<16, 100<<16, 65536L, 0, MENUSCREEN, 16, 0, 2+8+64+BGSTRETCH);
    rotatesprite_fs(160<<16, 34<<16, RR ? 23592L : 65536L, 0, INGAMEDUKETHREEDEE, 0, 0, 10);
    if (!RR && PLUTOPAK)   // JBF 20030804
        rotatesprite_fs((260)<<16, 36<<16, 65536L, 0, PLUTOPAKSPRITE+2, 0, 0, 2+8);
    gametext_center(58+(RR ? 0 : 2), "Multiplayer Totals");
    gametext_center(58+10, g_mapInfo[G_LastMapInfoIndex()].name);

    gametext_center_shade(RR ? 175 : 165, "Press any key or button to continue", quotepulseshade);

    minitext(38, 80, "Name", 8, 2+8+16+128);
    minitext(269, 80, "Kills", 8, 2+8+16+128);
    for (i=0; i<g_mostConcurrentPlayers; i++)
    {
        Bsprintf(tempbuf, "%-4d", i+1);
        minitext(92+(i*23), 80, tempbuf, RR ? 0 : 3, 2+8+16+128);
    }

    for (i=0; i<g_mostConcurrentPlayers; i++)
    {
        int32_t xfragtotal = 0;
        Bsprintf(tempbuf, "%d", i+1);

        minitext(30, 90+t, tempbuf, 0, 2+8+16+128);
        minitext(38, 90+t, g_player[i].user_name, g_player[i].ps->palookup, 2+8+16+128);

        for (y=0; y<g_mostConcurrentPlayers; y++)
        {
            if (i == y)
            {
                Bsprintf(tempbuf, "%-4d", g_player[y].ps->fraggedself);
                minitext(92+(y*23), 90+t, tempbuf, RR ? 0 : 2, 2+8+16+128);
                xfragtotal -= g_player[y].ps->fraggedself;
            }
            else
            {
                Bsprintf(tempbuf, "%-4d", g_player[i].frags[y]);
                minitext(92+(y*23), 90+t, tempbuf, 0, 2+8+16+128);
                xfragtotal += g_player[i].frags[y];
            }
        }

        Bsprintf(tempbuf, "%-4d", xfragtotal);
        minitext(101+(8*23), 90+t, tempbuf, RR ? 0 : 2, 2+8+16+128);

        t += 7;
    }

    for (y=0; y<g_mostConcurrentPlayers; y++)
    {
        int32_t yfragtotal = 0;
        for (i=0; i<g_mostConcurrentPlayers; i++)
        {
            if (i == y)
                yfragtotal += g_player[i].ps->fraggedself;
            else
                yfragtotal += g_player[i].frags[y];
        }
        Bsprintf(tempbuf, "%-4d", yfragtotal);
        minitext(92+(y*23), 96+(8*7), tempbuf, RR ? 0 : 2, 2+8+16+128);
    }

    minitext(45, 96+(8*7), "Deaths", RR ? 0 : 8, 2+8+16+128);
}

static int32_t G_PrintTime_ClockPad(void)
{
    int32_t clockpad = 2;
    int32_t ii, ij;

    for (ii=g_player[myconnectindex].ps->player_par/(REALGAMETICSPERSEC*60), ij=1; ii>9; ii/=10, ij++) { }
    clockpad = max(clockpad, ij);
    if (!(ud.volume_number == 0 && ud.last_level-1 == 7 && boardfilename[0]))
    {
        for (ii=g_mapInfo[G_LastMapInfoIndex()].partime/(REALGAMETICSPERSEC*60), ij=1; ii>9; ii/=10, ij++) { }
        clockpad = max(clockpad, ij);
        if (g_mapInfo[G_LastMapInfoIndex()].designertime)
        {
            for (ii=g_mapInfo[G_LastMapInfoIndex()].designertime/(REALGAMETICSPERSEC*60), ij=1; ii>9; ii/=10, ij++) { }
            clockpad = max(clockpad, ij);
        }
    }
    if (ud.playerbest > 0) for (ii=ud.playerbest/(REALGAMETICSPERSEC*60), ij=1; ii>9; ii/=10, ij++) { }
    clockpad = max(clockpad, ij);

    return clockpad;
}

static const char* G_PrintTime2(int32_t time)
{
    Bsprintf(tempbuf, RR ? "%0*d : %02d" : "%0*d:%02d", G_PrintTime_ClockPad(), time/(REALGAMETICSPERSEC*60), (time/REALGAMETICSPERSEC)%60);
    return tempbuf;
}
static const char* G_PrintTime3(int32_t time)
{
    Bsprintf(tempbuf, RR ? "%0*d : %02d . %02d" : "%0*d:%02d.%02d", G_PrintTime_ClockPad(), time/(REALGAMETICSPERSEC*60), (time/REALGAMETICSPERSEC)%60, ((time%REALGAMETICSPERSEC)*33)/10);
    return tempbuf;
}

const char* G_PrintYourTime(void)
{
    return G_PrintTime3(g_player[myconnectindex].ps->player_par);
}
const char* G_PrintParTime(void)
{
    if (ud.last_level < 1)
        return "<invalid>";
    return G_PrintTime2(g_mapInfo[G_LastMapInfoIndex()].partime);
}
const char* G_PrintDesignerTime(void)
{
    if (ud.last_level < 1)
        return "<invalid>";
    return G_PrintTime2(g_mapInfo[G_LastMapInfoIndex()].designertime);
}
const char* G_PrintBestTime(void)
{
    return G_PrintTime3(ud.playerbest);
}

void G_BonusScreen(int32_t bonusonly)
{
    int32_t gfx_offset;
    int32_t bonuscnt;
    int32_t clockpad = 2;
    const char *lastmapname;

    if (REALITY)
    {
        RT_Bonus();
        return;
    }

    //if (g_networkMode == NET_DEDICATED_SERVER)
    //    return;

    G_UpdateAppTitle();

    if (ud.volume_number == 0 && ud.last_level == 8 && boardfilename[0])
    {
        lastmapname = Bstrrchr(boardfilename, '\\');
        if (!lastmapname) lastmapname = Bstrrchr(boardfilename, '/');
        if (!lastmapname) lastmapname = boardfilename;
    }
    else
    {
        lastmapname = g_mapInfo[G_LastMapInfoIndex()].name;
        if (!lastmapname) // this isn't right but it's better than no name at all
            lastmapname = g_mapInfo[G_LastMapInfoIndex()].name;
    }

    if (RR)
    {
        if ((g_lastLevel && ud.volume_number == 2) || g_vixenLevel)
            lastmapname = "CLOSE ENCOUNTERS";
        else if (g_turdLevel)
            lastmapname = "SMELTING PLANT";
    }


    fadepal(0, 0, 0, 0, 252, RR ? 4 : 28);
    videoSetViewableArea(0, 0, xdim-1, ydim-1);
    videoClearScreen(0L);
    videoNextPage();
    renderFlushPerms();

    FX_StopAllSounds();
    S_ClearSoundLocks();
    FX_SetReverb(0L);
    CONTROL_BindsEnabled = 1; // so you can use your screenshot bind on the score screens

    if (!bonusonly)
        G_BonusCutscenes();

    P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 8+2+1);   // JBF 20040308
    G_FadePalette(0, 0, 0, 252);   // JBF 20031228
    KB_FlushKeyboardQueue();
    totalclock = 0;
    bonuscnt = 0;

    S_StopMusic();
    FX_StopAllSounds();
    S_ClearSoundLocks();

    if (g_mostConcurrentPlayers > 1 && (g_gametypeFlags[ud.coop]&GAMETYPE_SCORESHEET))
    {
        videoClearScreen(0);
        G_DisplayMPResultsScreen();

        if (ud.config.MusicToggle)
            S_PlaySound(BONUSMUSIC);

        videoNextPage();
        I_ClearAllInput();
        fadepal(0, 0, 0, 252, 0, RR ? -4 : -28);
        totalclock = 0;

        while (totalclock < TICRATE*10)
        {
            G_HandleAsync();
            MUSIC_Update();

            if (engineFPSLimit())
            {
                videoClearScreen(0);
                G_DisplayMPResultsScreen();
                videoNextPage();
            }

            if (I_CheckAllInput())
            {
                I_ClearAllInput();
                break;
            }
        }

        fadepal(0, 0, 0, 0, 252, RR ? 4 : 28);
    }

    if (AP || bonusonly || (g_netServer || ud.multimode > 1)) return;

    if (!RR)
    {
        gfx_offset = (ud.volume_number==1) ? 5 : 0;
        gfx_offset += BONUSSCREEN;
        rotatesprite_fs(160<<16, 100<<16, 65536L, 0, gfx_offset, 0, 0, 2+8+64+128+BGSTRETCH);

        if (lastmapname)
            menutext_center(20-6, lastmapname);
        menutext_center(36-6, "Completed");

        gametext_center_shade(192, "Press any key or button to continue", quotepulseshade);

        if (ud.config.MusicToggle)
            S_PlaySound(BONUSMUSIC);
    }
    else
    {
        gfx_offset = (ud.volume_number==0) ? RRTILE403 : RRTILE409;
        gfx_offset += ud.level_number-1;

        if (g_lastLevel || g_vixenLevel)
            gfx_offset = RRTILE409+7;

        if (boardfilename[0])
            gfx_offset = RRTILE403;

        rotatesprite_fs(160<<16, 100<<16, 65536L, 0, gfx_offset, 0, 0, 2+8+64+128+BGSTRETCH);
        if (lastmapname)
            menutext(80,16, lastmapname);

        menutext(15, 192, "Press any key to continue");
    }

    videoNextPage();
    I_ClearAllInput();
    fadepal(0, 0, 0, 252, 0, -4);
    bonuscnt = 0;
    totalclock = 0;

    do
    {
        int32_t yy = 0, zz;

        G_HandleAsync();
        MUSIC_Update();

        if (engineFPSLimit())
        {
            if (g_player[myconnectindex].ps->gm&MODE_EOL)
            {
                videoClearScreen(0);
                rotatesprite_fs(160<<16, 100<<16, 65536L, 0, gfx_offset, 0, 0, 2+8+64+128+BGSTRETCH);

                if (totalclock >= 1000000000 && totalclock < 1000000320)
                {
                    switch (((int32_t) totalclock>>4)%15)
                    {
                    case 0:
                        if (bonuscnt == 6)
                        {
                            bonuscnt++;
                            S_PlaySound(RR ? 425 : SHOTGUN_COCK);
                            switch (wrand()&3)
                            {
                            case 0:
                                S_PlaySound(BONUS_SPEECH1);
                                break;
                            case 1:
                                S_PlaySound(BONUS_SPEECH2);
                                break;
                            case 2:
                                S_PlaySound(BONUS_SPEECH3);
                                break;
                            case 3:
                                S_PlaySound(BONUS_SPEECH4);
                                break;
                            }
                        }
                        fallthrough__;
                    case 1:
                    case 4:
                    case 5:
                        if (!RR)
                            rotatesprite_fs(199<<16, 31<<16, 65536L, 0, 3+gfx_offset, 0, 0, 2+8+16+64+128+BGSTRETCH);
                        break;
                    case 2:
                    case 3:
                        if (!RR)
                            rotatesprite_fs(199<<16, 31<<16, 65536L, 0, 4+gfx_offset, 0, 0, 2+8+16+64+128+BGSTRETCH);
                        break;
                    }
                }
                else if (totalclock > (10240+120L)) break;
                else
                {
                    switch (((int32_t) totalclock>>5)&3)
                    {
                    case 1:
                    case 3:
                        if (!RR)
                            rotatesprite_fs(199<<16, 31<<16, 65536L, 0, 1+gfx_offset, 0, 0, 2+8+16+64+128+BGSTRETCH);
                        break;
                    case 2:
                        if (!RR)
                            rotatesprite_fs(199<<16, 31<<16, 65536L, 0, 2+gfx_offset, 0, 0, 2+8+16+64+128+BGSTRETCH);
                        break;
                    }
                }

                if (!RR)
                {
                    if (lastmapname)
                        menutext_center(20-6, lastmapname);
                    menutext_center(36-6, "Completed");

                    gametext_center_shade(192, "Press any key or button to continue", quotepulseshade);
                }
                else
                {
                    if (lastmapname)
                        menutext(80, 16, lastmapname);

                    menutext(15, 192, "Press any key to continue");
                }

                const int yystep = RR ? 16 : 10;
                if (totalclock > (60*3))
                {
                    yy = zz = RR ? 48 : 59;

                    if (!RR)
                        gametext(10, yy+9, "Your Time:");
                    else
                        menutext(30, yy, "Yer Time:");

                    yy+= yystep;
                    if (!(ud.volume_number == 0 && ud.last_level-1 == 7 && boardfilename[0]))
                    {
                        if (g_mapInfo[G_LastMapInfoIndex()].partime)
                        {
                            if (!RR)
                                gametext(10, yy+9, "Par Time:");
                            else
                                menutext(30, yy, "Par Time:");
                            yy+=yystep;
                        }
                        if (g_mapInfo[G_LastMapInfoIndex()].designertime)
                        {
                            // EDuke 2.0 / NAM source suggests "Green Beret's Time:"
                            if (DUKE)
                                gametext(10, yy+9, "3D Realms' Time:");
                            else if (RR)
                                menutext(30, yy, "Xatrix Time:");
                            yy+=yystep;
                        }

                    }
                    if (ud.playerbest > 0)
                    {
                        if (!RR)
                            gametext(10, yy+9, (g_player[myconnectindex].ps->player_par > 0 && g_player[myconnectindex].ps->player_par < ud.playerbest) ? "Prev Best Time:" : "Your Best Time:");
                        else
                            menutext(30, yy, (g_player[myconnectindex].ps->player_par > 0 && g_player[myconnectindex].ps->player_par < ud.playerbest) ? "Prev Best:" : "Yer Best:");
                        yy += yystep;
                    }

                    if (bonuscnt == 0)
                        bonuscnt++;

                    yy = zz;
                    if (totalclock >(60*4))
                    {
                        if (bonuscnt == 1)
                        {
                            bonuscnt++;
                            S_PlaySound(RR ? 404 : PIPEBOMB_EXPLODE);
                        }

                        if (g_player[myconnectindex].ps->player_par > 0)
                        {
                            G_PrintYourTime();
                            if (!RR)
                            {
                                gametext_number((320>>2)+71, yy+9, tempbuf);
                                if (g_player[myconnectindex].ps->player_par < ud.playerbest)
                                    gametext((320>>2)+89+(clockpad*24), yy+9, "New record!");
                            }
                            else
                            {
                                menutext(191, yy, tempbuf);
                                //if (g_player[myconnectindex].ps->player_par < ud.playerbest)
                                //    menutext(191 + 30 + (clockpad*24), yy, "New record!");
                            }
                        }
                        else if (!RR)
                            gametext_pal((320>>2)+71, yy+9, "Cheated!", 2);
                        else
                            menutext(191, yy, "Cheated!");
                        yy+=yystep;

                        if (!(ud.volume_number == 0 && ud.last_level-1 == 7 && boardfilename[0]))
                        {
                            if (g_mapInfo[G_LastMapInfoIndex()].partime)
                            {
                                G_PrintParTime();
                                if (!RR)
                                    gametext_number((320>>2)+71, yy+9, tempbuf);
                                else
                                    menutext(191, yy, tempbuf);
                                yy+=yystep;
                            }
                            if (g_mapInfo[G_LastMapInfoIndex()].designertime)
                            {
                                G_PrintDesignerTime();
                                if (DUKE)
                                    gametext_number((320>>2)+71, yy+9, tempbuf);
                                else if (RR)
                                    menutext(191, yy, tempbuf);
                                yy+=yystep;
                            }
                        }

                        if (ud.playerbest > 0)
                        {
                            G_PrintBestTime();
                            if (!RR)
                                gametext_number((320>>2)+71, yy+9, tempbuf);
                            else
                                menutext(191, yy, tempbuf);
                            yy+=yystep;
                        }
                    }
                }

                zz = yy += RR ? 16 : 5;
                if (totalclock > (60*6))
                {
                    if (!RR)
                        gametext(10, yy+9, "Enemies Killed:");
                    else
                        menutext(30, yy, "Varmints Killed:");
                    yy += yystep;
                    if (!RR)
                        gametext(10, yy+9, "Enemies Left:");
                    else
                        menutext(30, yy, "Varmints Left:");
                    yy += yystep;

                    if (bonuscnt == 2)
                    {
                        bonuscnt++;
                        if (!RR)
                            S_PlaySound(FLY_BY);
                    }

                    yy = zz;

                    if (totalclock > (60*7))
                    {
                        if (bonuscnt == 3)
                        {
                            bonuscnt++;
                            S_PlaySound(RR ? 422 : PIPEBOMB_EXPLODE);
                        }
                        Bsprintf(tempbuf, "%-3d", g_player[myconnectindex].ps->actors_killed);
                        if (!RR)
                            gametext_number((320>>2)+70, yy+9, tempbuf);
                        else
                            menutext(231,yy,tempbuf);
                        yy += yystep;
                        if (ud.player_skill > 3 && !RR)
                        {
                            if (!RR)
                                gametext((320>>2)+70, yy+9, "N/A");
                            else
                                menutext(231,yy, "N/A");
                            yy += yystep;
                        }
                        else
                        {
                            if ((g_player[myconnectindex].ps->max_actors_killed-g_player[myconnectindex].ps->actors_killed) < 0)
                                Bsprintf(tempbuf, "%-3d", 0);
                            else Bsprintf(tempbuf, "%-3d", g_player[myconnectindex].ps->max_actors_killed-g_player[myconnectindex].ps->actors_killed);
                            if (!RR)
                                gametext_number((320>>2)+70, yy+9, tempbuf);
                            else
                                menutext(231, yy, tempbuf);
                            yy += yystep;
                        }
                    }
                }

                zz = yy += RR ? 0 : 5;
                if (totalclock > (60*9))
                {
                    if (!RR)
                        gametext(10, yy+9, "Secrets Found:");
                    else
                        menutext(30, yy, "Secrets Found:");
                    yy += yystep;
                    if (!RR)
                        gametext(10, yy+9, "Secrets Missed:");
                    else
                        menutext(30, yy, "Secrets Missed:");
                    yy += yystep;
                    if (bonuscnt == 4) bonuscnt++;

                    yy = zz;
                    if (totalclock > (60*10))
                    {
                        if (bonuscnt == 5)
                        {
                            bonuscnt++;
                            S_PlaySound(RR ? 404 : PIPEBOMB_EXPLODE);
                        }
                        Bsprintf(tempbuf, "%-3d", g_player[myconnectindex].ps->secret_rooms);
                        if (!RR)
                            gametext_number((320>>2)+70, yy+9, tempbuf);
                        else
                            menutext(231, yy, tempbuf);
                        yy += yystep;
#if 0
                        // Always overwritten.
                        if (g_player[myconnectindex].ps->secret_rooms > 0)
                            Bsprintf(tempbuf, "%-3d%%", (100*g_player[myconnectindex].ps->secret_rooms/g_player[myconnectindex].ps->max_secret_rooms));
#endif
                        Bsprintf(tempbuf, "%-3d", g_player[myconnectindex].ps->max_secret_rooms-g_player[myconnectindex].ps->secret_rooms);
                        if (!RR)
                            gametext_number((320>>2)+70, yy+9, tempbuf);
                        else
                            menutext(231, yy, tempbuf);
                        yy += yystep;
                        }
                    }

                if (totalclock > 10240 && totalclock < 10240+10240)
                    totalclock = 1024;

                if (I_CheckAllInput() && totalclock >(60*2)) // JBF 20030809
                {
                    I_ClearAllInput();
                    if (totalclock < (60*13))
                    {
                        KB_FlushKeyboardQueue();
                        totalclock = (60*13);
                    }
                    else if (totalclock < 1000000000)
                        totalclock = 1000000000;
                }
            }
            else
                break;

            videoNextPage();
        }
    } while (1);
    if (g_turdLevel)
        g_turdLevel = 0;
    if (g_vixenLevel)
        g_vixenLevel = 0;
}

void G_PlayMapAnim(void)
{
    const char *animFile;
    if (ud.volume_number == 0)
    {
        switch (ud.level_number)
        {
            case 1:
                animFile = "lvl1.anm";
                break;
            case 2:
                animFile = "lvl2.anm";
                break;
            case 3:
                animFile = "lvl3.anm";
                break;
            case 4:
                animFile = "lvl4.anm";
                break;
            case 5:
                animFile = "lvl5.anm";
                break;
            case 6:
                animFile = "lvl6.anm";
                break;
            default:
                animFile = "lvl7.anm";
                break;
        }
    }
    else
    {
        switch (ud.level_number)
        {
            case 1:
                animFile = "lvl8.anm";
                break;
            case 2:
                animFile = "lvl9.anm";
                break;
            case 3:
                animFile = "lvl10.anm";
                break;
            case 4:
                animFile = "lvl11.anm";
                break;
            case 5:
                animFile = "lvl12.anm";
                break;
            case 6:
                animFile = "lvl13.anm";
                break;
            default:
                animFile = NULL;
                break;
        }
    }

    if (animFile == NULL)
        return;

    Anim_Play(animFile);
}

void G_ShowMapFrame(void)
{
    int frame = -1;

    if (ud.volume_number == 0)
    {
        switch (ud.level_number)
        {
        case 1:
            frame = 0;
            break;
        case 2:
            frame = 1;
            break;
        case 3:
            frame = 2;
            break;
        case 4:
            frame = 3;
            break;
        case 5:
            frame = 4;
            break;
        case 6:
            frame = 5;
            break;
        default:
            frame = 6;
            break;
        }
    }
    else
    {
        switch (ud.level_number)
        {
        case 1:
            frame = 7;
            break;
        case 2:
            frame = 8;
            break;
        case 3:
            frame = 9;
            break;
        case 4:
            frame = 10;
            break;
        case 5:
            frame = 11;
            break;
        case 6:
            frame = 12;
            break;
        default:
            frame = -1;
            break;
        }
    }
    rotatesprite_fs(160<<16,100<<16,65536L,0,RRTILE8624+frame,0,0,10+64+128);
}

void G_BonusScreenRRRA(int32_t bonusonly)
{
    int32_t gfx_offset;
    int32_t bonuscnt;
    int32_t showMap = 0;
    const char *lastmapname;

    //if (g_networkMode == NET_DEDICATED_SERVER)
    //    return;

    G_UpdateAppTitle();

    if (ud.volume_number == 0 && ud.last_level == 8 && boardfilename[0])
    {
        lastmapname = Bstrrchr(boardfilename, '\\');
        if (!lastmapname) lastmapname = Bstrrchr(boardfilename, '/');
        if (!lastmapname) lastmapname = boardfilename;
    }
    else
    {
        lastmapname = g_mapInfo[G_LastMapInfoIndex()].name;
        if (!lastmapname) // this isn't right but it's better than no name at all
            lastmapname = g_mapInfo[G_LastMapInfoIndex()].name;
    }

    if ((g_lastLevel && ud.volume_number == 2) || g_vixenLevel)
        lastmapname = "CLOSE ENCOUNTERS";
    else if (g_turdLevel)
        lastmapname = "SMELTING PLANT";


    fadepal(0, 0, 0, 0, 252, 4);
    videoSetViewableArea(0, 0, xdim-1, ydim-1);
    videoClearScreen(0L);
    videoNextPage();
    renderFlushPerms();

    FX_StopAllSounds();
    S_ClearSoundLocks();
    FX_SetReverb(0L);
    CONTROL_BindsEnabled = 1; // so you can use your screenshot bind on the score screens

    if (boardfilename[0] == 0 && numplayers < 2)
    {
        if ((ud.eog == 0 || ud.volume_number != 1) && ud.volume_number <= 1)
        {
            showMap = 1;
            MUSIC_StopSong();
            KB_FlushKeyboardQueue();

            P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 8+2+1);
            G_ShowMapFrame();
            fadepal(0, 0, 0, 252, 0, -4);
            P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 8+2+1);
        }
    }

    if (!bonusonly)
        G_BonusCutscenes();

    P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 8+2+1);   // JBF 20040308
    //G_FadePalette(0, 0, 0, 252);   // JBF 20031228
    KB_FlushKeyboardQueue();
    totalclock = 0;
    bonuscnt = 0;

    S_StopMusic();
    FX_StopAllSounds();
    S_ClearSoundLocks();

    if (g_mostConcurrentPlayers > 1 && (g_gametypeFlags[ud.coop]&GAMETYPE_SCORESHEET))
    {
        videoClearScreen(0);
        G_DisplayMPResultsScreen();

        if (ud.config.MusicToggle)
            S_PlaySound(BONUSMUSIC);

        videoNextPage();
        I_ClearAllInput();
        fadepal(0, 0, 0, 252, 0, -4);
        totalclock = 0;

        while (totalclock < TICRATE*10)
        {
            G_HandleAsync();
            MUSIC_Update();

            if (engineFPSLimit())
            {
                videoClearScreen(0);
                G_DisplayMPResultsScreen();
                videoNextPage();
            }

            if (I_CheckAllInput())
            {
                I_ClearAllInput();
                break;
            }
        }

        fadepal(0, 0, 0, 0, 252, 4);
    }

    if (bonusonly || (g_netServer || ud.multimode > 1)) return;

    gfx_offset = (ud.volume_number==0) ? RRTILE403 : RRTILE409;
    gfx_offset += ud.level_number-1;

    if (g_lastLevel || g_vixenLevel)
        gfx_offset = RRTILE409+7;

    if (boardfilename[0])
        gfx_offset = RRTILE403;

    if (!showMap)
        rotatesprite_fs(160<<16, 100<<16, 65536L, 0, gfx_offset, 0, 0, 2+8+64+128+BGSTRETCH);
    if (lastmapname)
        menutext(80,16, lastmapname);

    menutext(15, 192, "Press any key to continue");

    I_ClearAllInput();
    if (!showMap)
    {
        videoNextPage();
        fadepal(0, 0, 0, 252, 0, -4);
    }
    bonuscnt = 0;
    totalclock = 0;

    do
    {
        int32_t yy = 0, zz;

        G_HandleAsync();
        MUSIC_Update();

        if (engineFPSLimit())
        {
            if (g_player[myconnectindex].ps->gm&MODE_EOL)
            {
                videoClearScreen(0);
                if (showMap)
                    G_ShowMapFrame();
                else
                    rotatesprite_fs(160<<16, 100<<16, 65536L, 0, gfx_offset, 0, 0, 2+8+64+128+BGSTRETCH);

                if (showMap)
                {
                    if (bonuscnt == 7)
                    {
                        bonuscnt++;
                        MUSIC_StopSong();
                        G_PlayMapAnim();
                        break;
                    }
                }

                if (totalclock >= 1000000000 && totalclock < 1000000320)
                {
                    switch (((int32_t) totalclock>>4)%15)
                    {
                    case 0:
                        if (bonuscnt == 6)
                        {
                            bonuscnt++;
                            S_PlaySound(425);
                            switch (wrand()&3)
                            {
                            case 0:
                                S_PlaySound(BONUS_SPEECH1);
                                break;
                            case 1:
                                S_PlaySound(BONUS_SPEECH2);
                                break;
                            case 2:
                                S_PlaySound(BONUS_SPEECH3);
                                break;
                            case 3:
                                S_PlaySound(BONUS_SPEECH4);
                                break;
                            }
                        }
                        fallthrough__;
                    case 1:
                    case 4:
                    case 5:
                        break;
                    case 2:
                    case 3:
                        break;
                    }
                }
                else if (totalclock > (10240+120L)) break;
                else
                {
                    switch (((int32_t) totalclock>>5)&3)
                    {
                    case 1:
                    case 3:
                        break;
                    case 2:
                        break;
                    }
                }

                if (lastmapname)
                    menutext(80, 16, lastmapname);

                menutext(15, 192, "Press any key to continue");

                const int yystep = 16;
                if (totalclock > (60*3))
                {
                    yy = zz = 48;

                    menutext(30, yy, "Yer Time:");

                    yy+= yystep;
                    if (!(ud.volume_number == 0 && ud.last_level-1 == 7 && boardfilename[0]))
                    {
                        if (g_mapInfo[G_LastMapInfoIndex()].partime)
                        {
                            menutext(30, yy, "Par Time:");
                            yy+=yystep;
                        }
                        if (g_mapInfo[G_LastMapInfoIndex()].designertime)
                        {
                            menutext(30, yy, "Xatrix Time:");
                            yy+=yystep;
                        }

                    }
                    if (ud.playerbest > 0)
                    {
                        menutext(30, yy, (g_player[myconnectindex].ps->player_par > 0 && g_player[myconnectindex].ps->player_par < ud.playerbest) ? "Prev Best:" : "Yer Best:");
                        yy += yystep;
                    }

                    if (bonuscnt == 0)
                        bonuscnt++;

                    yy = zz;
                    if (totalclock >(60*4))
                    {
                        if (bonuscnt == 1)
                        {
                            bonuscnt++;
                            S_PlaySound(404);
                        }

                        if (g_player[myconnectindex].ps->player_par > 0)
                        {
                            G_PrintYourTime();
                            menutext(191, yy, tempbuf);
                            //if (g_player[myconnectindex].ps->player_par < ud.playerbest)
                            //    menutext(191 + 30 + (clockpad*24), yy, "New record!");
                        }
                        else
                            menutext(191, yy, "Cheated!");
                        yy+=yystep;

                        if (!(ud.volume_number == 0 && ud.last_level-1 == 7 && boardfilename[0]))
                        {
                            if (g_mapInfo[G_LastMapInfoIndex()].partime)
                            {
                                G_PrintParTime();
                                menutext(191, yy, tempbuf);
                                yy+=yystep;
                            }
                            if (g_mapInfo[G_LastMapInfoIndex()].designertime)
                            {
                                G_PrintDesignerTime();
                                menutext(191, yy, tempbuf);
                                yy+=yystep;
                            }
                        }

                        if (ud.playerbest > 0)
                        {
                            G_PrintBestTime();
                            menutext(191, yy, tempbuf);
                            yy+=yystep;
                        }
                    }
                }

                zz = yy += 16;
                if (totalclock > (60*6))
                {
                    menutext(30, yy, "Varmints Killed:");
                    yy += yystep;
                    menutext(30, yy, "Varmints Left:");
                    yy += yystep;

                    if (bonuscnt == 2)
                        bonuscnt++;

                    yy = zz;

                    if (totalclock > (60*7))
                    {
                        if (bonuscnt == 3)
                        {
                            bonuscnt++;
                            S_PlaySound(422);
                        }
                        Bsprintf(tempbuf, "%-3d", g_player[myconnectindex].ps->actors_killed);
                        menutext(231,yy,tempbuf);
                        yy += yystep;
                        //if (ud.player_skill > 3)
                        //{
                        //    menutext(231,yy, "N/A");
                        //    yy += yystep;
                        //}
                        //else
                        {
                            if ((g_player[myconnectindex].ps->max_actors_killed-g_player[myconnectindex].ps->actors_killed) < 0)
                                Bsprintf(tempbuf, "%-3d", 0);
                            else Bsprintf(tempbuf, "%-3d", g_player[myconnectindex].ps->max_actors_killed-g_player[myconnectindex].ps->actors_killed);
                            menutext(231, yy, tempbuf);
                            yy += yystep;
                        }
                    }
                }

                zz = yy += 0;
                if (totalclock > (60*9))
                {
                    menutext(30, yy, "Secrets Found:");
                    yy += yystep;
                    menutext(30, yy, "Secrets Missed:");
                    yy += yystep;
                    if (bonuscnt == 4) bonuscnt++;

                    yy = zz;
                    if (totalclock > (60*10))
                    {
                        if (bonuscnt == 5)
                        {
                            bonuscnt++;
                            S_PlaySound(404);
                        }
                        Bsprintf(tempbuf, "%-3d", g_player[myconnectindex].ps->secret_rooms);
                        menutext(231, yy, tempbuf);
                        yy += yystep;
#if 0
                        // Always overwritten.
                        if (g_player[myconnectindex].ps->secret_rooms > 0)
                            Bsprintf(tempbuf, "%-3d%%", (100*g_player[myconnectindex].ps->secret_rooms/g_player[myconnectindex].ps->max_secret_rooms));
#endif
                        Bsprintf(tempbuf, "%-3d", g_player[myconnectindex].ps->max_secret_rooms-g_player[myconnectindex].ps->secret_rooms);
                        menutext(231, yy, tempbuf);
                        yy += yystep;
                        }
                    }

                if (totalclock > 10240 && totalclock < 10240+10240)
                    totalclock = 1024;

                if (I_CheckAllInput() && totalclock >(60*2)) // JBF 20030809
                {
                    I_ClearAllInput();

                    if (totalclock < (60*13))
                    {
                        KB_FlushKeyboardQueue();
                        totalclock = (60*13);
                    }
                    else if (totalclock < 1000000000)
                        totalclock = 1000000000;
                }
            }
            else
                break;
            videoNextPage();
        }
    } while (1);
    if (ud.eog)
    {
        fadepal(0, 0, 0, 0, 252, 4);
        videoClearScreen(0L);
        videoNextPage();
        S_PlaySound(35);
        G_FadePalette(0, 0, 0, 0);
        P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 8+2+1);
        while (1)
        {
            switch (((int32_t) totalclock >> 4) & 1)
            {
            case 0:
                rotatesprite(0,0,65536,0,RRTILE8677,0,0,2+8+16+64+128,0,0,xdim-1,ydim-1);
                videoNextPage();
                G_FadePalette(0, 0, 0, 0);
                P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 8+2+1);
                Net_GetPackets();
                break;
            case 1:
                rotatesprite(0,0,65536,0,RRTILE8677+1,0,0,2+8+16+64+128,0,0,xdim-1,ydim-1);
                videoNextPage();
                G_FadePalette(0, 0, 0, 0);
                P_SetGamePalette(g_player[myconnectindex].ps, BASEPAL, 8+2+1);
                Net_GetPackets();
                break;
            }
            S_Cleanup();
            if (!S_CheckSoundPlaying(-1,35)) break;
            if (I_CheckAllInput())
            {
                I_ClearAllInput();
                S_StopSound(35);
                break;
            }
        }
    }
    if (g_RAendEpisode)
    {
        g_RAendEpisode = 0;
        ud.m_volume_number = ud.volume_number = 1;
        ud.m_level_number = ud.level_number = 0;
        ud.eog = 0;
    }
    if (g_turdLevel)
        g_turdLevel = 0;
    if (g_vixenLevel)
        g_vixenLevel = 0;
}

