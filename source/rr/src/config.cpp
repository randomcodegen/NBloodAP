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
#include "scriplib.h"
#include "osdcmds.h"
#include "renderlayer.h"
#include "cmdline.h"

#ifdef __ANDROID__
# include "android.h"
#endif

#if defined RENDERTYPESDL && defined SDL_TARGET && SDL_TARGET > 1
# include "sdl_inc.h"
#endif

// we load this in to get default button and key assignments
// as well as setting up function mappings

#define __SETUP__   // JBF 20031211
#include "_functio.h"

hashtable_t h_gamefuncs    = { NUMGAMEFUNCTIONS<<1, NULL };

int32_t CONFIG_FunctionNameToNum(const char *func)
{
    int32_t i;

    if (!func)
        return -1;

    i = hash_find(&h_gamefuncs,func);

    if (i < 0)
    {
        char *str = Bstrtolower(Xstrdup(func));
        i = hash_find(&h_gamefuncs,str);
        Xfree(str);

        return i;
    }

    return i;
}


char *CONFIG_FunctionNumToName(int32_t func)
{
    if ((unsigned)func >= (unsigned)NUMGAMEFUNCTIONS)
        return NULL;
    return gamefunctions[func];
}


int32_t CONFIG_AnalogNameToNum(const char *func)
{
    if (!func)
        return -1;

    if (!Bstrcasecmp(func,"analog_turning"))
    {
        return analog_turning;
    }
    if (!Bstrcasecmp(func,"analog_strafing"))
    {
        return analog_strafing;
    }
    if (!Bstrcasecmp(func,"analog_moving"))
    {
        return analog_moving;
    }
    if (!Bstrcasecmp(func,"analog_lookingupanddown"))
    {
        return analog_lookingupanddown;
    }

    return -1;
}


const char *CONFIG_AnalogNumToName(int32_t func)
{
    switch (func)
    {
    case analog_turning:
        return "analog_turning";
    case analog_strafing:
        return "analog_strafing";
    case analog_moving:
        return "analog_moving";
    case analog_lookingupanddown:
        return "analog_lookingupanddown";
    }

    return NULL;
}


static void CONFIG_SetJoystickButtonFunction(int i, int j, int function)
{
    ud.config.JoystickFunctions[i][j] = function;
    CONTROL_MapButton(function, i, j, controldevice_joystick);
}
static void CONFIG_SetJoystickAnalogAxisScale(int i, int scale)
{
    ud.config.JoystickAnalogueScale[i] = scale;
    CONTROL_SetAnalogAxisScale(i, scale, controldevice_joystick);
}
static void CONFIG_SetJoystickAnalogAxisInvert(int i, int invert)
{
    ud.config.JoystickAnalogueInvert[i] = invert;
    CONTROL_SetAnalogAxisInvert(i, invert);
}
static void CONFIG_SetJoystickAnalogAxisDeadSaturate(int i, int dead, int saturate)
{
    ud.config.JoystickAnalogueDead[i] = dead;
    ud.config.JoystickAnalogueSaturate[i] = saturate;
    JOYSTICK_SetDeadZone(i, dead, saturate);
}
static void CONFIG_SetJoystickDigitalAxisFunction(int i, int j, int function)
{
    ud.config.JoystickDigitalFunctions[i][j] = function;
    CONTROL_MapDigitalAxis(i, function, j);
}
static void CONFIG_SetJoystickAnalogAxisFunction(int i, int function)
{
    ud.config.JoystickAnalogueAxes[i] = function;
    CONTROL_MapAnalogAxis(i, function);
}
void CONFIG_SetDefaultKeys(const char (*keyptr)[MAXGAMEFUNCLEN], bool lazy/*=false*/)
{
    static char const s_gamefunc_[] = "gamefunc_";
    int constexpr strlen_gamefunc_  = ARRAY_SIZE(s_gamefunc_) - 1;

    if (!lazy)
    {
        Bmemset(ud.config.KeyboardKeys, 0xff, sizeof(ud.config.KeyboardKeys));
        CONTROL_ClearAllBinds();
    }

    for (int i=0; i < ARRAY_SSIZE(gamefunctions); ++i)
    {
        if (gamefunctions[i][0] == '\0')
            continue;

        auto &key = ud.config.KeyboardKeys[i];

        int const default0 = KB_StringToScanCode(keyptr[i<<1]);
        int const default1 = KB_StringToScanCode(keyptr[(i<<1)+1]);

        // skip the function if the default key is already used
        // or the function is assigned to another key
        if (lazy && (key[0] != 0xff || (CONTROL_KeyIsBound(default0) && Bstrlen(CONTROL_KeyBinds[default0].cmdstr) > strlen_gamefunc_
                        && CONFIG_FunctionNameToNum(CONTROL_KeyBinds[default0].cmdstr + strlen_gamefunc_) >= 0)))
        {
#if 0 // defined(DEBUGGINGAIDS)
            if (key[0] != 0xff)
                initprintf("Skipping %s bound to %s\n", keyptr[i<<1], CONTROL_KeyBinds[default0].cmdstr);
#endif
            continue;
        }

        key[0] = default0;
        key[1] = default1;

        if (key[0])
            CONTROL_FreeKeyBind(key[0]);

        if (key[1])
            CONTROL_FreeKeyBind(key[1]);

        if (i == gamefunc_Show_Console)
            OSD_CaptureKey(key[0]);
        else
            CONFIG_MapKey(i, key[0], 0, key[1], 0);
    }
}


void CONFIG_SetDefaults(void)
{
    // JBF 20031211
    int32_t i;

    ud.config.scripthandle = -1;
#ifdef __ANDROID__
    droidinput.forward_sens = 5.f;
    droidinput.strafe_sens = 5.f;
    droidinput.pitch_sens = 5.f;
    droidinput.yaw_sens = 5.f;
    droidinput.hideStick = 0;
    droidinput.gameControlsAlpha = 0.5;
    droidinput.toggleCrouch = 1;
    droidinput.quickSelectWeapon = 1;

    ud.setup.xdim = droidinfo.screen_width;
    ud.setup.ydim = droidinfo.screen_height;
#else
# if defined RENDERTYPESDL && SDL_MAJOR_VERSION > 1
    uint32_t inited = SDL_WasInit(SDL_INIT_VIDEO);
    if (inited == 0)
        SDL_Init(SDL_INIT_VIDEO);
    else if (!(inited & SDL_INIT_VIDEO))
        SDL_InitSubSystem(SDL_INIT_VIDEO);

    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0)
    {
        ud.setup.xdim = dm.w;
        ud.setup.ydim = dm.h;
    }
    else
# endif
    {
        ud.setup.xdim = 1024;
        ud.setup.ydim = 768;
    }
#endif

#ifdef USE_OPENGL
    ud.setup.bpp = 32;
#else
    ud.setup.bpp = 8;
#endif
    ud.config.useprecache = 1;
    ud.config.AmbienceToggle = 1;
    ud.config.FXVolume = 255;
#if defined(_WIN32)
    ud.config.MixRate = 44100;
#elif defined __ANDROID__
    ud.config.MixRate = droidinfo.audio_sample_rate;
#else
    ud.config.MixRate = 48000;
#endif
    ud.config.MouseBias = 0;
    ud.config.MouseDeadZone = 0;
    ud.config.MusicToggle = 1;
    ud.config.MusicVolume = 195;
    ud.config.MusicDevice = ASS_AutoDetect;
    g_myAimMode = g_player[0].ps->aim_mode = 1;
    ud.config.AutoAim = 1;
    ud.config.NumBits = 16;
    ud.config.NumChannels = 2;
#if defined GEKKO || defined __OPENDINGUX__
    ud.config.NumVoices = 32;
#else
    ud.config.NumVoices = 64;
#endif
#ifdef ASS_REVERSESTEREO
    ud.config.ReverseStereo = 0;
#endif
    ud.auto_run = 1;
    ud.config.ShowOpponentWeapons = 0;
    ud.config.SoundToggle = 1;
    ud.config.CheckForUpdates = 1;
    ud.config.AutoAim = 1;
    ud.config.JoystickAimWeight = 5;
    ud.config.JoystickViewCentering = 3;
    ud.config.JoystickAimAssist = 1;
    ud.althud = 1;
    ud.automsg = 0;
    ud.autovote = 0;
    ud.brightness = 8;
    ud.camerasprite = -1;

#if defined GEKKO || defined __OPENDINGUX__
    ud.camera_time = 11;
#elif defined(__ANDROID__)
    ud.camera_time = 7;
#else
    ud.camera_time = 4;
#endif

    ud.color = 0;
    ud.crosshair = 1;
    ud.crosshairscale = 50;
    ud.obituaries = 1;
    ud.democams = 1;
    ud.detail = 0;
    ud.drawweapon = 1;
    ud.idplayers = 1;
    ud.levelstats = 0;
    ud.lockout = 0;
    ud.m_ffire = 1;
    ud.m_marker = 1;
    ud.menu_slidebarz = 65536;
    ud.menu_slidebarmargin = RR ? 6 * 65536 : 65536;
    ud.menu_slidecursorz = RR ? 32768 : 65536;
    ud.mouseaiming = 0;
    ud.mouseflip = 1;
    ud.msgdisptime = 120;
    ud.pwlockout[0] = '\0';
    ud.runkey_mode = 0;
    ud.screen_size = 4;
    ud.screen_tilting = 1;
    ud.shadows = 1;
    ud.statusbarflags = STATUSBAR_NOSHRINK;
    ud.statusbarmode = 1;
    ud.statusbarscale = 100;
    ud.team = 0;
    ud.viewbob = 1;
    ud.weaponsway = 1;
    ud.weaponswitch = 3;	// new+empty
    ud.angleinterpolation = 0;
    ud.setup.usejoystick = 1;
    ud.setup.forcesetup = 1;
    ud.setup.noautoload = 1;
    ud.setup.fullscreen = 1;
    ud.setup.usemouse = 1;

    ud.config.VoiceToggle = 5; // bitfield, 1 = local, 2 = dummy, 4 = other players in DM
    ud.display_bonus_screen = 1;
    ud.show_level_text = 1;
    ud.configversion = 0;
    ud.weaponscale = 100;
    ud.textscale = 200;
    ud.screenfade = 1;
    ud.menubackground = 1;
    ud.hudontop = 0;
    ud.default_skill = 1;
    ud.slidebar_paldisabled = 1;
    ud.shadow_pal = 4;
    ud.menu_scrollbartilenum = -1;
    ud.menu_scrollbarz = 65536;
    ud.menu_scrollcursorz = 65536;
    ud.autosave = 1;
    ud.autosavedeletion = 1;
    ud.maxautosaves = 5;
    ud.fov = 90;

    Bstrcpy(ud.rtsname, G_DefaultRtsFile());

    Bstrcpy(szPlayerName, "Player");

    //if (RR)
    //{
    //    Bstrcpy(ud.ridecule[0], "Yer as ugly as a mud fence!");
    //    Bstrcpy(ud.ridecule[1], "Duck you pecker-head!");
    //    Bstrcpy(ud.ridecule[2], "You like that boy?");
    //    Bstrcpy(ud.ridecule[3], "Yer lower than catfish crap!");
    //    Bstrcpy(ud.ridecule[4], "Eat lead, you shit monkey!");
    //    Bstrcpy(ud.ridecule[5], "You dumb-ass!");
    //    Bstrcpy(ud.ridecule[6], "Yer slower'n a three legged dog!");
    //    Bstrcpy(ud.ridecule[7], "Come on...Squeal like a pig!");
    //    Bstrcpy(ud.ridecule[8], "Haw, haw, haw!");
    //    Bstrcpy(ud.ridecule[9], "Now you gone and done it!");
    //}
    //else
    //{
        Bstrcpy(ud.ridecule[0], "An inspiration for birth control.");
        Bstrcpy(ud.ridecule[1], "You're gonna die for that!");
        Bstrcpy(ud.ridecule[2], "It hurts to be you.");
        Bstrcpy(ud.ridecule[3], "Lucky son of a bitch.");
        Bstrcpy(ud.ridecule[4], "Hmmm... payback time.");
        Bstrcpy(ud.ridecule[5], "You bottom dwelling scum sucker.");
        Bstrcpy(ud.ridecule[6], "Damn, you're ugly.");
        Bstrcpy(ud.ridecule[7], "Ha ha ha... wasted!");
        Bstrcpy(ud.ridecule[8], "You suck!");
        Bstrcpy(ud.ridecule[9], "AARRRGHHHHH!!!");
    //}

    // JBF 20031211

    if (RR)
    {
        Bstrcpy((char*)keydefaults[gamefunc_Holo_Duke<<1], "B");
        Bstrcpy((char*)keydefaults[gamefunc_Jetpack<<1], "C");
        Bstrcpy((char*)keydefaults[gamefunc_NightVision<<1], "Y");
        Bstrcpy((char*)keydefaults[gamefunc_MedKit<<1], "R");
        Bstrcpy((char*)keydefaults[gamefunc_Steroids<<1], "M");
        Bstrcpy((char*)keydefaults[gamefunc_Show_Opponents_Weapon<<1], "V");

        Bstrcpy((char*)oldkeydefaults[gamefunc_Holo_Duke<<1], "B");
        Bstrcpy((char*)oldkeydefaults[gamefunc_Jetpack<<1], "C");
        Bstrcpy((char*)oldkeydefaults[gamefunc_NightVision<<1], "Y");
        Bstrcpy((char*)oldkeydefaults[gamefunc_MedKit<<1], "W");
        Bstrcpy((char*)oldkeydefaults[gamefunc_Steroids<<1], "M");
        Bstrcpy((char*)oldkeydefaults[gamefunc_Show_Opponents_Weapon<<1], "E");
        Bstrcpy((char*)oldkeydefaults[gamefunc_Show_Console<<1], "V");
    }

    CONFIG_SetDefaultKeys(keydefaults);

    memset(ud.config.MouseFunctions, -1, sizeof(ud.config.MouseFunctions));
    memset(ud.config.JoystickFunctions, -1, sizeof(ud.config.JoystickFunctions));
    memset(ud.config.JoystickDigitalFunctions, -1, sizeof(ud.config.JoystickDigitalFunctions));

    CONTROL_MouseSensitivity = DEFAULTMOUSESENSITIVITY;
    for (i=0; i<MAXMOUSEBUTTONS; i++)
    {
        ud.config.MouseFunctions[i][0] = CONFIG_FunctionNameToNum(mousedefaults[i]);
        CONTROL_MapButton(ud.config.MouseFunctions[i][0], i, 0, controldevice_mouse);
        if (i>=4) continue;
        ud.config.MouseFunctions[i][1] = CONFIG_FunctionNameToNum(mouseclickeddefaults[i]);
        CONTROL_MapButton(ud.config.MouseFunctions[i][1], i, 1, controldevice_mouse);
    }

#if !defined GEKKO
    CONFIG_SetGameControllerDefaults();
#else
    for (i=0; i<MAXJOYBUTTONSANDHATS; i++)
    {
        ud.config.JoystickFunctions[i][0] = CONFIG_FunctionNameToNum(joystickdefaults[i]);
        ud.config.JoystickFunctions[i][1] = CONFIG_FunctionNameToNum(joystickclickeddefaults[i]);
        CONTROL_MapButton(ud.config.JoystickFunctions[i][0], i, 0, controldevice_joystick);
        CONTROL_MapButton(ud.config.JoystickFunctions[i][1], i, 1, controldevice_joystick);
    }

    for (i=0; i<MAXJOYAXES; i++)
    {
        ud.config.JoystickAnalogueScale[i] = DEFAULTJOYSTICKANALOGUESCALE;
        ud.config.JoystickAnalogueInvert[i] = 0;
        ud.config.JoystickAnalogueDead[i] = DEFAULTJOYSTICKANALOGUEDEAD;
        ud.config.JoystickAnalogueSaturate[i] = DEFAULTJOYSTICKANALOGUESATURATE;
        CONTROL_SetAnalogAxisScale(i, ud.config.JoystickAnalogueScale[i], controldevice_joystick);
        CONTROL_SetAnalogAxisInvert(i, 0, controldevice_joystick);

        ud.config.JoystickDigitalFunctions[i][0] = CONFIG_FunctionNameToNum(joystickdigitaldefaults[i*2]);
        ud.config.JoystickDigitalFunctions[i][1] = CONFIG_FunctionNameToNum(joystickdigitaldefaults[i*2+1]);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][0], 0, controldevice_joystick);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][1], 1, controldevice_joystick);

        ud.config.JoystickAnalogueAxes[i] = CONFIG_AnalogNameToNum(joystickanalogdefaults[i]);
        CONTROL_MapAnalogAxis(i, ud.config.JoystickAnalogueAxes[i], controldevice_joystick);
    }
#endif
}


// wrapper for CONTROL_MapKey(), generates key bindings to reflect changes to keyboard setup
void CONFIG_MapKey(int which, kb_scancode key1, kb_scancode oldkey1, kb_scancode key2, kb_scancode oldkey2)
{
    int const keys[] = { key1, key2, oldkey1, oldkey2 };
    char buf[2*MAXGAMEFUNCLEN];

    if (which == gamefunc_Show_Console)
        OSD_CaptureKey(key1);

    for (int k = 0; (unsigned)k < ARRAY_SIZE(keys); k++)
    {
        if (keys[k] == 0xff || !keys[k])
            continue;

        int match = 0;

        for (; sctokeylut[match].key; match++)
        {
            if (keys[k] == sctokeylut[match].sc)
                break;
        }

        tempbuf[0] = 0;

        for (int i=NUMGAMEFUNCTIONS-1; i>=0; i--)
        {
            if (ud.config.KeyboardKeys[i][0] == keys[k] || ud.config.KeyboardKeys[i][1] == keys[k])
            {
                Bsprintf(buf, "gamefunc_%s; ", CONFIG_FunctionNumToName(i));
                Bstrcat(tempbuf,buf);
            }
        }

        int const len = Bstrlen(tempbuf);

        if (len >= 2)
        {
            tempbuf[len-2] = 0;  // cut off the trailing "; "
            CONTROL_BindKey(keys[k], tempbuf, 1, sctokeylut[match].key ? sctokeylut[match].key : "<?>");
        }
        else
        {
            CONTROL_FreeKeyBind(keys[k]);
        }
    }
}


void CONFIG_SetupMouse(void)
{
    int32_t i;
    char str[80];
    char temp[80];
    int32_t scale;

    if (ud.config.scripthandle < 0) return;

    for (i=0; i<MAXMOUSEBUTTONS; i++)
    {
        Bsprintf(str,"MouseButton%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle,"Controls", str,temp))
            ud.config.MouseFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"MouseButtonClicked%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle,"Controls", str,temp))
            ud.config.MouseFunctions[i][1] = CONFIG_FunctionNameToNum(temp);
    }

    {
        tempbuf[0] = 0;
        SCRIPT_GetString(ud.config.scripthandle, "Controls","Mouse_Sensitivity",&tempbuf[0]);
        if (tempbuf[0]) CONTROL_MouseSensitivity = atof(tempbuf);
    }

    for (i=0; i<MAXMOUSEBUTTONS; i++)
    {
        CONTROL_MapButton(ud.config.MouseFunctions[i][0], i, 0, controldevice_mouse);
        CONTROL_MapButton(ud.config.MouseFunctions[i][1], i, 1,  controldevice_mouse);
    }
}


void CONFIG_SetupJoystick(void)
{
    int32_t i;
    char str[80];
    char temp[80];
    int32_t scale;

    if (ud.config.scripthandle < 0) return;

    for (i=0; i<MAXJOYBUTTONSANDHATS; i++)
    {
        Bsprintf(str,"ControllerButton%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle,"Controls", str,temp))
            ud.config.JoystickFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"ControllerButtonClicked%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle,"Controls", str,temp))
            ud.config.JoystickFunctions[i][1] = CONFIG_FunctionNameToNum(temp);
    }

    // map over the axes
    for (i=0; i<MAXJOYAXES; i++)
    {
        Bsprintf(str,"ControllerAnalogAxes%d",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str,temp))
            if (CONFIG_AnalogNameToNum(temp) != -1 || (!temp[0] && CONFIG_FunctionNameToNum(temp) != -1))
                ud.config.JoystickAnalogueAxes[i] = CONFIG_AnalogNameToNum(temp);

        Bsprintf(str,"ControllerDigitalAxes%d_0",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str,temp))
            if (CONFIG_FunctionNameToNum(temp) != -1 || (!temp[0] && CONFIG_FunctionNameToNum(temp) != -1))
                ud.config.JoystickDigitalFunctions[i][0] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"ControllerDigitalAxes%d_1",i);
        temp[0] = 0;
        if (!SCRIPT_GetString(ud.config.scripthandle, "Controls", str,temp))
            if (CONFIG_FunctionNameToNum(temp) != -1 || (!temp[0] && CONFIG_FunctionNameToNum(temp) != -1))
                ud.config.JoystickDigitalFunctions[i][1] = CONFIG_FunctionNameToNum(temp);

        Bsprintf(str,"ControllerAnalogScale%d",i);
        scale = ud.config.JoystickAnalogueScale[i];
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str,&scale);
        ud.config.JoystickAnalogueScale[i] = scale;

        Bsprintf(str,"ControllerAnalogInvert%d",i);
        scale = ud.config.JoystickAnalogueInvert[i];
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str,&scale);
        ud.config.JoystickAnalogueInvert[i] = scale;

        Bsprintf(str,"ControllerAnalogDead%d",i);
        scale = ud.config.JoystickAnalogueDead[i];
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str,&scale);
        ud.config.JoystickAnalogueDead[i] = scale;

        Bsprintf(str,"ControllerAnalogSaturate%d",i);
        scale = ud.config.JoystickAnalogueSaturate[i];
        SCRIPT_GetNumber(ud.config.scripthandle, "Controls", str,&scale);
        ud.config.JoystickAnalogueSaturate[i] = scale;
    }

    for (i=0; i<MAXJOYBUTTONSANDHATS; i++)
    {
        CONTROL_MapButton(ud.config.JoystickFunctions[i][0], i, 0, controldevice_joystick);
        CONTROL_MapButton(ud.config.JoystickFunctions[i][1], i, 1,  controldevice_joystick);
    }
    for (i=0; i<MAXJOYAXES; i++)
    {
        CONTROL_MapAnalogAxis(i, ud.config.JoystickAnalogueAxes[i]);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][0], 0);
        CONTROL_MapDigitalAxis(i, ud.config.JoystickDigitalFunctions[i][1], 1);
        CONTROL_SetAnalogAxisScale(i, ud.config.JoystickAnalogueScale[i], controldevice_joystick);
        CONTROL_SetAnalogAxisInvert(i, ud.config.JoystickAnalogueInvert[i]);
    }
}

struct GameControllerButtonSetting
{
    GameControllerButton button;
    int function;

    void apply() const
    {
        CONFIG_SetJoystickButtonFunction(button, 0, function);
    }
};
struct GameControllerAnalogAxisSetting
{
    GameControllerAxis axis;
    int function;

    void apply() const
    {
        CONFIG_SetJoystickAnalogAxisFunction(axis, function);
    }
};
struct GameControllerDigitalAxisSetting
{
    GameControllerAxis axis;
    int polarity;
    int function;

    void apply() const
    {
        CONFIG_SetJoystickDigitalAxisFunction(axis, polarity, function);
    }
};

static void CONFIG_SetGameControllerAxesModern()
{
    static GameControllerAnalogAxisSetting const analogAxes[] =
    {
        { CONTROLLER_AXIS_LEFTX, analog_strafing },
        { CONTROLLER_AXIS_LEFTY, analog_moving },
        { CONTROLLER_AXIS_RIGHTX, analog_turning },
        { CONTROLLER_AXIS_RIGHTY, analog_lookingupanddown },
    };

    CONFIG_SetJoystickAnalogAxisScale(CONTROLLER_AXIS_RIGHTX, 65536);
    CONFIG_SetJoystickAnalogAxisScale(CONTROLLER_AXIS_RIGHTY, 65536);

    for (auto const & analogAxis : analogAxes)
        analogAxis.apply();
}

void CONFIG_SetGameControllerDefaults()
{
    CONFIG_SetGameControllerDefaultsClear();
    CONFIG_SetGameControllerAxesModern();

    static GameControllerButtonSetting const buttons[] =
    {
        { CONTROLLER_BUTTON_A, gamefunc_Open },
        { CONTROLLER_BUTTON_B, gamefunc_Toggle_Crouch },
        { CONTROLLER_BUTTON_Y, gamefunc_Quick_Kick },
        { CONTROLLER_BUTTON_BACK, gamefunc_Map },
        { CONTROLLER_BUTTON_LEFTSTICK,  gamefunc_Run },
        { CONTROLLER_BUTTON_RIGHTSTICK, gamefunc_Crouch },
        { CONTROLLER_BUTTON_DPAD_UP,    gamefunc_Previous_Weapon },
        { CONTROLLER_BUTTON_DPAD_DOWN,  gamefunc_Next_Weapon },
        { CONTROLLER_BUTTON_LEFTSHOULDER,  gamefunc_Crouch },
        { CONTROLLER_BUTTON_RIGHTSHOULDER, gamefunc_Alt_Fire },
        { CONTROLLER_BUTTON_MISC, gamefunc_Third_Person_View },
    };

    static GameControllerButtonSetting const buttonsDuke[] =
    {
        { CONTROLLER_BUTTON_X, gamefunc_Inventory },
        { CONTROLLER_BUTTON_DPAD_LEFT,  gamefunc_Inventory_Left  },
        { CONTROLLER_BUTTON_DPAD_RIGHT, gamefunc_Inventory_Right },
    };

    static GameControllerButtonSetting const buttonsFury[] =
    {
        { CONTROLLER_BUTTON_X, gamefunc_Steroids }, // Reload
        { CONTROLLER_BUTTON_DPAD_LEFT,  gamefunc_MedKit },
        { CONTROLLER_BUTTON_DPAD_RIGHT, gamefunc_NightVision }, // Radar
    };

    static GameControllerDigitalAxisSetting const digitalAxes[] =
    {
        { CONTROLLER_AXIS_TRIGGERLEFT,  1, gamefunc_Jump },
        { CONTROLLER_AXIS_TRIGGERRIGHT, 1, gamefunc_Fire },
    };

    for (auto const & button : buttons)
        button.apply();

    for (auto const & button : buttonsDuke)
        button.apply();

    for (auto const & digitalAxis : digitalAxes)
        digitalAxis.apply();
}

void CONFIG_SetGameControllerDefaultsClear()
{
    for (int i=0; i<MAXJOYBUTTONSANDHATS; i++)
    {
        CONFIG_SetJoystickButtonFunction(i, 0, -1);
        CONFIG_SetJoystickButtonFunction(i, 1, -1);
    }

    for (int i=0; i<MAXJOYAXES; i++)
    {
        CONFIG_SetJoystickAnalogAxisScale(i, DEFAULTJOYSTICKANALOGUESCALE);
        CONFIG_SetJoystickAnalogAxisInvert(i, 0);
        CONFIG_SetJoystickAnalogAxisDeadSaturate(i, DEFAULTJOYSTICKANALOGUEDEAD, DEFAULTJOYSTICKANALOGUESATURATE);

        CONFIG_SetJoystickDigitalAxisFunction(i, 0, -1);
        CONFIG_SetJoystickDigitalAxisFunction(i, 1, -1);

        CONFIG_SetJoystickAnalogAxisFunction(i, -1);
    }
}

int32_t CONFIG_ReadSetup(void)
{
    int32_t dummy;
    char commmacro[] = "CommbatMacro# ";
    char tempbuf[1024];

    CONTROL_ClearAssignments();
    CONFIG_SetDefaults();

    ud.config.setupread = 1;
    pathsearchmode = 1;

    if (ud.config.scripthandle < 0)
    {
        if (buildvfs_exists(g_setupFileName))  // JBF 20031211
            ud.config.scripthandle = SCRIPT_Load(g_setupFileName);
#if !defined(EDUKE32_TOUCH_DEVICES)
        else if (buildvfs_exists(SETUPFILENAME))
        {
            int32_t i;
            i=wm_ynbox("Import Configuration Settings", "The configuration file \"%s\" was not found. "
                "Import configuration data from \"%s\"?", g_setupFileName, SETUPFILENAME);
            if (i) ud.config.scripthandle = SCRIPT_Load(SETUPFILENAME);
        }
        else if (buildvfs_exists("duke3d.cfg"))
        {
            int32_t i;
            i=wm_ynbox("Import Configuration Settings", "The configuration file \"%s\" was not found. "
                "Import configuration data from \"duke3d.cfg\"?", g_setupFileName);
            if (i) ud.config.scripthandle = SCRIPT_Load("duke3d.cfg");
        }
#endif
    }

    pathsearchmode = 0;

    if (ud.config.scripthandle < 0)
        return -1;

    for (dummy = 0; dummy < MAXRIDECULE; dummy++)
    {
        commmacro[13] = dummy+'0';
        SCRIPT_GetString(ud.config.scripthandle, "Comm Setup",commmacro,&ud.ridecule[dummy][0]);
    }

    Bmemset(tempbuf, 0, sizeof(tempbuf));
    SCRIPT_GetString(ud.config.scripthandle, "Comm Setup","PlayerName",&tempbuf[0]);

    char nameBuf[64];

    while (Bstrlen(OSD_StripColors(nameBuf, tempbuf)) > 10)
        tempbuf[Bstrlen(tempbuf) - 1] = '\0';

    Bstrncpyz(szPlayerName, tempbuf, sizeof(szPlayerName));

    SCRIPT_GetString(ud.config.scripthandle, "Comm Setup","RTSName",&ud.rtsname[0]);

    SCRIPT_GetNumber(ud.config.scripthandle, "Setup", "ConfigVersion", &ud.configversion);
    SCRIPT_GetNumber(ud.config.scripthandle, "Setup", "ForceSetup", &ud.setup.forcesetup);
    SCRIPT_GetNumber(ud.config.scripthandle, "Setup", "NoAutoLoad", &ud.setup.noautoload);
    SCRIPT_GetNumber(ud.config.scripthandle, "Setup", "CacheSize", &dummy);

    if (dummy > MAXCACHE1DSIZE)
        MAXCACHE1DSIZE = dummy;

    if (g_noSetup == 0 && g_modDir[0] == '/')
    {
        struct Bstat st;
        SCRIPT_GetString(ud.config.scripthandle, "Setup","ModDir",&g_modDir[0]);

        if (Bstat(g_modDir, &st))
        {
            if ((st.st_mode & S_IFDIR) != S_IFDIR)
            {
                initprintf("Invalid mod dir in cfg!\n");
                Bsprintf(g_modDir,"/");
            }
        }
    }

    if (g_grpNamePtr == NULL && g_addonNum == 0)
    {
        SCRIPT_GetStringPtr(ud.config.scripthandle, "Setup","SelectedGRP",&g_grpNamePtr);
        if (g_grpNamePtr && !Bstrlen(g_grpNamePtr))
            g_grpNamePtr = dup_filename(G_DefaultGrpFile());
    }

    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "Out",&ud.lockout);
    SCRIPT_GetString(ud.config.scripthandle, "Screen Setup","Password",&ud.pwlockout[0]);
    
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenHeight", &ud.setup.ydim);
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenMode", &ud.setup.fullscreen);
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenWidth", &ud.setup.xdim);

    vec2_t windowPos;
    if (!SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "WindowPosX", &windowPos.x)
        && !SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "WindowPosY", &windowPos.y))
    {
        g_windowPos = windowPos;
        g_windowPosValid = true;
    }

    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "MaxRefreshFreq", (int32_t *)&maxrefreshfreq);
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "ScreenBPP", &ud.setup.bpp);
    
    if (ud.setup.bpp < 8) ud.setup.bpp = 32;

#ifdef POLYMER
    int32_t rendmode = 0;
    SCRIPT_GetNumber(ud.config.scripthandle, "Screen Setup", "Polymer", &rendmode);
    glrendmode = (rendmode > 0) ? REND_POLYMER : REND_POLYMOST;
#endif

    SCRIPT_GetNumber(ud.config.scripthandle, "Misc", "Executions",&ud.executions);

#ifdef _WIN32
    SCRIPT_GetNumber(ud.config.scripthandle, "Updates", "CheckForUpdates", &ud.config.CheckForUpdates);
    SCRIPT_GetNumber(ud.config.scripthandle, "Updates", "LastUpdateCheck", &ud.config.LastUpdateCheck);
#endif

    // [AP] Read connection settings
    SCRIPT_GetString(ud.config.scripthandle, "AP", "Server", ud.setup.ap_server);
    SCRIPT_GetString(ud.config.scripthandle, "AP", "User", ud.setup.ap_user);
    SCRIPT_GetString(ud.config.scripthandle, "AP", "Password", ud.setup.ap_pass);

    ud.config.setupread = 1;
    return 0;
}


void CONFIG_WriteSettings(void) // save binds and aliases to <cfgname>_settings.cfg
{
    int32_t i;
    BFILE *fp;
    char *ptr = Xstrdup(g_setupFileName);
    char tempbuf[128];

    if (!Bstrcmp(g_setupFileName, SETUPFILENAME))
        Bsprintf(tempbuf, "settings.cfg");
    else Bsprintf(tempbuf, "%s_settings.cfg", strtok(ptr, "."));

    fp = Bfopen(tempbuf, "wt");

    if (fp)
    {
        Bfprintf(fp,"// this file is automatically generated by %s\n", AppProperName);
        Bfprintf(fp,"unbindall\n");

        for (i=0; i<MAXBOUNDKEYS+MAXMOUSEBUTTONS; i++)
            if (CONTROL_KeyIsBound(i))
                Bfprintf(fp,"bind \"%s\"%s \"%s\"\n",CONTROL_KeyBinds[i].key,
                CONTROL_KeyBinds[i].repeat?"":" norepeat",CONTROL_KeyBinds[i].cmdstr);

        for (int i=0; i<NUMGAMEFUNCTIONS; ++i)
        {
            char const * name = CONFIG_FunctionNumToName(i);
            if (name && name[0] != '\0' && (ud.config.KeyboardKeys[i][0] == 0xff || !ud.config.KeyboardKeys[i][0]))
            {
                Bfprintf(fp,"unbound %s\n", name);
            }
        }

        OSD_WriteAliases(fp);

        if (g_crosshairSum != -1 && g_crosshairSum != DefaultCrosshairColors.r+(DefaultCrosshairColors.g<<8)+(DefaultCrosshairColors.b<<16))
            Bfprintf(fp, "crosshaircolor %d %d %d\n", CrosshairColors.r, CrosshairColors.g, CrosshairColors.b);

        OSD_WriteCvars(fp);

        Bfclose(fp);

        if (!Bstrcmp(g_setupFileName, SETUPFILENAME))
            OSD_Printf("Wrote settings.cfg\n");
        else OSD_Printf("Wrote %s_settings.cfg\n",ptr);

        Xfree(ptr);
        return;
    }

    if (!Bstrcmp(g_setupFileName, SETUPFILENAME))
        OSD_Printf("Error writing settings.cfg: %s\n", strerror(errno));
    else OSD_Printf("Error writing %s_settings.cfg: %s\n",ptr,strerror(errno));

    Xfree(ptr);
}

void CONFIG_WriteSetup(uint32_t flags)
{
    int32_t dummy;

    if (!ud.config.setupread) return;

    if (ud.config.scripthandle < 0)
        ud.config.scripthandle = SCRIPT_Init(g_setupFileName);

    SCRIPT_PutNumber(ud.config.scripthandle, "Misc", "Executions",ud.executions,FALSE,FALSE);

    SCRIPT_PutNumber(ud.config.scripthandle, "Setup","ConfigVersion",BYTEVERSION_EDUKE32,FALSE,FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Setup", "ForceSetup", ud.setup.forcesetup, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Setup", "NoAutoLoad", ud.setup.noautoload, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Setup", "CacheSize", MAXCACHE1DSIZE, FALSE, FALSE);

#ifdef POLYMER
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "Polymer",glrendmode == REND_POLYMER,FALSE,FALSE);
#endif

    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenBPP", ud.setup.bpp, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenHeight", ud.setup.ydim, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenMode", ud.setup.fullscreen, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "ScreenWidth", ud.setup.xdim, FALSE, FALSE);

    if (g_grpNamePtr && !g_addonNum)
        SCRIPT_PutString(ud.config.scripthandle, "Setup","SelectedGRP",g_grpNamePtr);

#ifdef STARTUP_SETUP_WINDOW
    if (g_noSetup == 0)
        SCRIPT_PutString(ud.config.scripthandle, "Setup","ModDir",&g_modDir[0]);
#endif

    // [AP] Connection settings
    SCRIPT_PutString(ud.config.scripthandle, "AP", "Server", ud.setup.ap_server);
    SCRIPT_PutString(ud.config.scripthandle, "AP", "User", ud.setup.ap_user);
    SCRIPT_PutString(ud.config.scripthandle, "AP", "Password", ud.setup.ap_pass);

    // exit early after only updating the values that can be changed from the startup window
    if (flags & 1)
    {
        SCRIPT_Save(ud.config.scripthandle, g_setupFileName);
        SCRIPT_Free(ud.config.scripthandle);
        return;
    }

    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "MaxRefreshFreq", maxrefreshfreq, FALSE, FALSE);
    if (g_windowPosValid)
    {
        SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "WindowPosX", g_windowPos.x, FALSE, FALSE);
        SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "WindowPosY", g_windowPos.y, FALSE, FALSE);
    }

    SCRIPT_PutNumber(ud.config.scripthandle, "Screen Setup", "Out",ud.lockout,FALSE,FALSE);
    SCRIPT_PutString(ud.config.scripthandle, "Screen Setup", "Password",ud.pwlockout);

#ifdef _WIN32
    SCRIPT_PutNumber(ud.config.scripthandle, "Updates", "CheckForUpdates", ud.config.CheckForUpdates, FALSE, FALSE);
    SCRIPT_PutNumber(ud.config.scripthandle, "Updates", "LastUpdateCheck", ud.config.LastUpdateCheck, FALSE, FALSE);
#endif

    if (ud.setup.usemouse)
    {
        for (dummy=0; dummy<MAXMOUSEBUTTONS; dummy++)
        {
            if (CONFIG_FunctionNumToName(ud.config.MouseFunctions[dummy][0]))
            {
                Bsprintf(buf, "MouseButton%d", dummy);
                SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.MouseFunctions[dummy][0]));
            }

            if (dummy >= (MAXMOUSEBUTTONS-2)) continue;

            if (CONFIG_FunctionNumToName(ud.config.MouseFunctions[dummy][1]))
            {
                Bsprintf(buf, "MouseButtonClicked%d", dummy);
                SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.MouseFunctions[dummy][1]));
            }
        }
    }

    if (ud.setup.usejoystick)
    {
        for (dummy=0; dummy<MAXJOYBUTTONSANDHATS; dummy++)
        {
            if (CONFIG_FunctionNumToName(ud.config.JoystickFunctions[dummy][0]))
            {
                Bsprintf(buf, "ControllerButton%d", dummy);
                SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickFunctions[dummy][0]));
            }

            if (CONFIG_FunctionNumToName(ud.config.JoystickFunctions[dummy][1]))
            {
                Bsprintf(buf, "ControllerButtonClicked%d", dummy);
                SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickFunctions[dummy][1]));
            }
        }
        for (dummy=0; dummy<MAXJOYAXES; dummy++)
        {
            if (CONFIG_AnalogNumToName(ud.config.JoystickAnalogueAxes[dummy]))
            {
                Bsprintf(buf, "ControllerAnalogAxes%d", dummy);
                SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_AnalogNumToName(ud.config.JoystickAnalogueAxes[dummy]));
            }

            if (CONFIG_FunctionNumToName(ud.config.JoystickDigitalFunctions[dummy][0]))
            {
                Bsprintf(buf, "ControllerDigitalAxes%d_0", dummy);
                SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickDigitalFunctions[dummy][0]));
            }

            if (CONFIG_FunctionNumToName(ud.config.JoystickDigitalFunctions[dummy][1]))
            {
                Bsprintf(buf, "ControllerDigitalAxes%d_1", dummy);
                SCRIPT_PutString(ud.config.scripthandle, "Controls", buf, CONFIG_FunctionNumToName(ud.config.JoystickDigitalFunctions[dummy][1]));
            }

            if (ud.config.JoystickAnalogueScale[dummy] != DEFAULTJOYSTICKANALOGUESCALE)
            {
                Bsprintf(buf, "ControllerAnalogScale%d", dummy);
                SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueScale[dummy], FALSE, FALSE);
            }

            Bsprintf(buf, "ControllerAnalogInvert%d", dummy);
            SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueInvert[dummy], FALSE, FALSE);

            if (ud.config.JoystickAnalogueDead[dummy] != DEFAULTJOYSTICKANALOGUEDEAD)
            {
                Bsprintf(buf, "ControllerAnalogDead%d", dummy);
                SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueDead[dummy], FALSE, FALSE);
            }

            if (ud.config.JoystickAnalogueSaturate[dummy] != DEFAULTJOYSTICKANALOGUESATURATE)
            {
                Bsprintf(buf, "ControllerAnalogSaturate%d", dummy);
                SCRIPT_PutNumber(ud.config.scripthandle, "Controls", buf, ud.config.JoystickAnalogueSaturate[dummy], FALSE, FALSE);
            }
        }
    }

    SCRIPT_PutString(ud.config.scripthandle, "Comm Setup","PlayerName",&szPlayerName[0]);

    SCRIPT_PutString(ud.config.scripthandle, "Comm Setup","RTSName",&ud.rtsname[0]);

    char commmacro[] = "CommbatMacro# ";

    for (dummy = 0; dummy < MAXRIDECULE; dummy++)
    {
        commmacro[13] = dummy+'0';
        SCRIPT_PutString(ud.config.scripthandle, "Comm Setup",commmacro,&ud.ridecule[dummy][0]);
    }

    SCRIPT_Save(ud.config.scripthandle, g_setupFileName);

    if ((flags & 2) == 0)
        SCRIPT_Free(ud.config.scripthandle);

    OSD_Printf("Wrote %s\n",g_setupFileName);
    CONFIG_WriteSettings();
    Bfflush(NULL);
}

static const char *CONFIG_GetMapEntryName(char m[], char const * const mapname)
{
    strcpy(m, mapname);

    char *p = strrchr(m, '/');
    if (!p) p = strrchr(m, '\\');
    if (p) Bmemmove(m, p, Bstrlen(p)+1);
    for (p=m; *p; p++) *p = tolower(*p);

    // cheap hack because SCRIPT_GetNumber doesn't like the slashes
    p = m;
    while (*p == '/') p++;

    return p;
}

static void CONFIG_GetMD4EntryName(char m[], uint8_t const * const md4)
{
    sprintf(m, "MD4_%08x%08x%08x%08x",
            B_BIG32(B_UNBUF32(&md4[0])), B_BIG32(B_UNBUF32(&md4[4])),
            B_BIG32(B_UNBUF32(&md4[8])), B_BIG32(B_UNBUF32(&md4[12])));
}

int32_t CONFIG_GetMapBestTime(char const * const mapname, uint8_t const * const mapmd4)
{
    if (!ud.config.setupread || ud.config.scripthandle < 0)
        return -1;

    char m[37];
    CONFIG_GetMD4EntryName(m, mapmd4);

    int32_t t = -1;
    if (SCRIPT_GetNumber(ud.config.scripthandle, "MapTimes", m, &t))
    {
        // fall back to map filenames
        char m2[BMAX_PATH];
        char const * const p = CONFIG_GetMapEntryName(m2, mapname);
        SCRIPT_GetNumber(ud.config.scripthandle, "MapTimes", p, &t);
    }
    return t;
}

int32_t CONFIG_SetMapBestTime(uint8_t const * const mapmd4, int32_t const tm)
{
    if (ud.config.scripthandle < 0) ud.config.scripthandle = SCRIPT_Init(g_setupFileName);
    if (ud.config.scripthandle < 0) return -1;

    char m[37];
    CONFIG_GetMD4EntryName(m, mapmd4);

    SCRIPT_PutNumber(ud.config.scripthandle, "MapTimes", m, tm, FALSE, FALSE);
    return 0;
}

