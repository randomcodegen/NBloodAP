#pragma once

#include "stdint.h"

/*
  Compatibility layer for APCpp library

  This implements the same interfaces but with local testing logic.

  This is useful until we have apworld generation up and running to use
  locally generated worlds for testing properly.
*/

#define AP_USE_COMP_LAYER

#ifdef AP_USE_COMP_LAYER
#ifdef __cplusplus
extern "C"
{
#endif

extern void AP_SendItem_Comp(int64_t);

#ifdef __cplusplus
}
#endif
#else
#include "Archipelago.h"
#define AP_SendItem_Comp AP_SendItem
#endif
