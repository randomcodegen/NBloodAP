#include "ap_comp.h"
#include <json/json.h>
#include "ap_lib.h"
#include <fstream>

#ifdef AP_USE_COMP_LAYER

void (*resetItemValues)() = nullptr;
void (*getitemfunc)(int64_t,int,bool) = nullptr;
void (*checklocfunc)(int64_t) = nullptr;
void (*locinfofunc)(std::vector<AP_NetworkItem>) = nullptr;

typedef struct {
	uint8_t flags;
	ap_net_id_t item;
} ap_comp_loc_info_t;

std::map<ap_net_id_t, ap_comp_loc_info_t> ap_comp_loc_infos;
std::map<ap_net_id_t, std::string> ap_comp_loc_names;
std::vector<ap_net_id_t> ap_starting_inv;
std::map<std::string, void (*)(std::string)> slotdata_callback;

void AP_SendItem_Compat(int64_t idx)
{
	if (ap_comp_loc_infos.count(idx) && !(AP_LOCATION_CHECKED(AP_SHORT_LOCATION(idx))))
	{
		if (getitemfunc != nullptr)
		{
			getitemfunc(ap_comp_loc_infos[idx].item, 0, true);
		}
	}
}

void AP_Init_Compat(const char* ip, const char* game, const char* player_name, const char* passwd)
{
  // Not supported, just for linking
}

Json::Value world_info;

void AP_Init_Compat(const char* filename)
{
    Json::Reader reader;
    std::ifstream mwfile(filename);
    reader.parse(mwfile, world_info);
    mwfile.close();
}


void AP_Start_Compat()
{
    // World mappings
    for (std::string loc_str : world_info["location_to_item"].getMemberNames())
    {
        int64_t loc_id = AP_NET_ID(std::stoll(loc_str));
        ap_comp_loc_info_t loc_info = {0, AP_NET_ID(world_info["location_to_item"][loc_str].asInt64())};
        loc_info.flags = 0b001;
        ap_comp_loc_infos[loc_id] = loc_info;
    }
    // Starting inventory
    ap_starting_inv.clear();
    for (unsigned int i = 0; i < world_info["start_inventory"].size(); i++)
        ap_starting_inv.push_back(AP_NET_ID(world_info["start_inventory"][i].asInt64()));

    // Build up location names for all locations
    auto locations = ap_game_config["locations"];
    for (std::string level_name : locations.getMemberNames())
    {
        for (std::string sprite_id : locations[level_name]["sprites"].getMemberNames())
        {
            if (locations[level_name]["sprites"][sprite_id]["id"].isInt64())
            {
                ap_net_id_t location_id = AP_NET_ID(locations[level_name]["sprites"][sprite_id]["id"].asInt64());
                ap_comp_loc_names[location_id] = level_name + ": " + locations[level_name]["sprites"][sprite_id]["name"].asString();
            }
        }
    }

    for (std::string level_name : locations.getMemberNames())
    {
        for (std::string sector_id : locations[level_name]["sectors"].getMemberNames())
        {
            if (locations[level_name]["sectors"][sector_id]["id"].isInt64())
            {
                ap_net_id_t location_id = AP_NET_ID(locations[level_name]["sectors"][sector_id]["id"].asInt64());
                ap_comp_loc_names[location_id] = level_name + " Secret: " + locations[level_name]["sectors"][sector_id]["name"].asString();
            }
        }
    }

    for (std::string level_name : locations.getMemberNames())
    {
        for (std::string exit_tag : locations[level_name]["exits"].getMemberNames())
        {
            if (locations[level_name]["exits"][exit_tag]["id"].isInt64())
            {
                ap_net_id_t location_id = AP_NET_ID(locations[level_name]["exits"][exit_tag]["id"].asInt64());
                ap_comp_loc_names[location_id] = level_name + ": " + locations[level_name]["exits"][exit_tag]["name"].asString();
            }
        }
    }

    // Load slot_data
    for (std::string setting_name : world_info["slot_data"].getMemberNames())
    {
        Json::FastWriter writer;
        if (slotdata_callback.count(setting_name))
        {
            slotdata_callback[setting_name](writer.write(world_info["slot_data"][setting_name]));
        }
    }

    if (getitemfunc == nullptr) return;
    for (ap_net_id_t item_id : ap_starting_inv)
        getitemfunc(item_id, 0, true);
}

void AP_Shutdown_Compat()
{

}

void AP_SetItemClearCallback_Compat(void (*f_itemclr)())
{
	resetItemValues = f_itemclr;
}

void AP_SetItemRecvCallback_Compat(void (*f_itemrecv)(int64_t,int,bool))
{
	getitemfunc = f_itemrecv;
}

void AP_SetLocationCheckedCallback_Compat(void (*f_locrecv)(int64_t))
{
	checklocfunc = f_locrecv;
}

void AP_SendLocationScouts_Compat(std::vector<int64_t> const& locations, int create_as_hint)
{
    if (locinfofunc == nullptr) return;
    std::vector<AP_NetworkItem> items;
    for (int64_t loc_id : locations)
    {
        if (!ap_comp_loc_infos.count(loc_id)) continue;
        AP_NetworkItem temp;

        temp.flags = ap_comp_loc_infos[loc_id].flags;
        temp.item = ap_comp_loc_infos[loc_id].item;
        temp.location = loc_id;
        temp.locationName = AP_GetLocationName_Compat(loc_id);

        items.push_back(temp);
    }
    locinfofunc(items);
}

void AP_SetLocationInfoCallback_Compat(void (*f_locrecv)(std::vector<AP_NetworkItem>))
{
	locinfofunc = f_locrecv;
}

AP_DataPackageSyncStatus AP_GetDataPackageStatus_Compat()
{
	return AP_DataPackageSyncStatus::Synced;
}

std::string AP_GetLocationName_Compat(int64_t location_id)
{
	return ap_comp_loc_names.count(location_id) ? ap_comp_loc_names.at(location_id) : std::string("Unknown location");
}

void AP_SetServerData_Compat(AP_SetServerDataRequest* request)
{
    // ToDo maybe persist this? Don't really care much for compat mode
    request->status = AP_RequestStatus::Done;
}

void AP_GetServerData_Compat(AP_GetServerDataRequest* request)
{
    // ToDo maybe get from persistent storage?
    switch (request->type) {
    case AP_DataType::Int:
        *((int*)request->value) = 0;
        break;
    case AP_DataType::Double:
        *((double*)request->value) = 0;
        break;
    case AP_DataType::Raw:
        *((std::string*)request->value) = "";
        break;
    }
    request->status = AP_RequestStatus::Done;
}

std::string AP_GetPrivateServerDataPrefix_Compat()
{
    // ToDo use seed from def?
    return "abc";
}

void AP_StoryComplete_Compat(void)
{

}

void AP_RegisterSlotDataRawCallback_Compat(std::string key, void (*f_slotdata)(std::string))
{
    slotdata_callback[key] = f_slotdata;
}

#endif
