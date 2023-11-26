#pragma once

#include "stdint.h"
#include "Archipelago.h"

/*
  Compatibility layer for APCpp library

  This implements the same interfaces but with local testing logic.

  This is useful until we have apworld generation up and running to use
  locally generated worlds for testing properly.
*/

#define AP_USE_COMP_LAYER

#ifdef AP_USE_COMP_LAYER

void AP_SendItem_Compat(int64_t);
void AP_Init_Compat(const char* ip, const char* game, const char* player_name, const char* passwd);
void AP_Init_Compat(const char* filename);
void AP_Start_Compat();
void AP_Shutdown_Compat();
void AP_SetItemClearCallback_Compat(void (*f_itemclr)());
void AP_SetItemRecvCallback_Compat(void (*f_itemrecv)(int64_t,int,bool));
void AP_SetLocationCheckedCallback_Compat(void (*f_locrecv)(int64_t));
void AP_SendLocationScouts_Compat(std::vector<int64_t> const& locations, int create_as_hint);
void AP_SetLocationInfoCallback_Compat(void (*f_locrecv)(std::vector<AP_NetworkItem>));
AP_DataPackageSyncStatus AP_GetDataPackageStatus_Compat();
std::vector<int64_t> AP_GetGameLocations_Compat();
std::string AP_GetLocationName_Compat(int64_t);

#else

#define AP_SendItem_Compat AP_SendItem
#define AP_Init_Compat AP_Init
#define AP_Start_Compat AP_Start
#define AP_Shutdown_Compat AP_Compat
#define AP_SetItemClearCallback_Compat AP_SetItemClearCallback
#define AP_SetItemRecvCallback_Compat AP_SetItemRecvCallback
#define AP_SetLocationCheckedCallback_Compat AP_SetLocationCheckedCallback
#define AP_SendLocationScouts_Compat AP_SendLocationScouts
#define AP_SetLocationInfoCallback_Compat AP_SetLocationInfoCallback
#define AP_GetDataPackageStatus_Compat AP_GetDataPackageStatus
#define AP_GetGameLocations_Compat AP_GetGameLocations
#define AP_GetLocationName_Compat AP_GetLocationName

#endif
