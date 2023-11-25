#pragma once

#include "ap_lib.h"
#include "duke3d.h"

extern void ap_on_map_load(void);
extern void ap_startup(void);
extern void ap_initialize(void);
extern void ap_process_event_queue(void);
extern void ap_check_secret(int16_t sectornum);
extern void ap_level_end(void);
extern void ap_check_exit(int16_t exitnum);
extern void ap_select_episode(uint8_t i);
extern void ap_select_level(uint8_t i);

extern std::string ap_active_episodes[MAXVOLUMES];
extern uint8_t ap_active_episodes_volumenum[MAXVOLUMES];
extern uint8_t ap_active_episode_count;
extern std::string ap_active_levels[MAXVOLUMES][MAXLEVELS];
extern uint8_t ap_active_levels_count[MAXVOLUMES];
extern uint8_t ap_active_levels_levelnum[MAXVOLUMES][MAXLEVELS];

extern uint8_t ap_return_to_menu;
