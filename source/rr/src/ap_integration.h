#pragma once

#include "ap_lib.h"
#include "duke3d.h"

extern void ap_on_map_load(void);
extern void ap_on_save_load(void);
extern void ap_sync_inventory();
extern void ap_store_dynamic_player_data(void);
extern void ap_startup(void);
extern void ap_shutdown(void);
extern void ap_initialize(void);
extern bool ap_process_periodic(void);
extern void ap_process_game_tic(void);
extern void ap_check_secret(int16_t sectornum);
extern void ap_level_end(void);
extern void ap_check_exit(int16_t exitnum);
extern void ap_select_episode(uint8_t i);
extern void ap_select_level(uint8_t i);
extern std::string ap_format_map_id(uint8_t level_number, uint8_t volume_number);

// Conditional abilities
extern bool ap_can_dive();
extern bool ap_can_jump();
extern bool ap_can_run();
extern bool ap_can_crouch();

extern std::string ap_episode_names[MAXVOLUMES];
extern std::vector<uint8_t> ap_active_episodes;
extern std::vector<std::vector<uint8_t>> ap_active_levels;
extern std::map<std::string, Json::Value> ap_level_data;

extern uint8_t ap_return_to_menu;
