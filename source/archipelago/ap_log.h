#pragma once

#define AP_OSDTEXT_NORMAL  "^31^S7  AP:  ^7^S0"
#define AP_OSDTEXT_DEBUG   "^31^S7  AP:  ^15^S0"
#define AP_OSDTEXT_ERROR   "^31^S7  AP:  ^10^S4"

extern int AP_Printf(const char *f, ...);
extern int AP_Debugf(const char *f, ...);
extern int AP_Errorf(const char *f, ...);

extern int AP_Printf(std::string f, ...);
extern int AP_Debugf(std::string f, ...);
extern int AP_Errorf(std::string f, ...);
