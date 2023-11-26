#include "ap_comp.h"
#include <json/json.h>
#include "ap_lib.h"

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

// Not exposed with a header we could include from this layer, need this for the compatibility implementation to
// get the game data somehow
extern Json::Value read_json_from_grp(const char* filename);

void AP_Init_Compat(const char* filename)
{
	Json::Value world_info = read_json_from_grp(filename);

    // World mappings
	for (std::string loc_str : world_info["locations"].getMemberNames())
	{
		int64_t loc_id = AP_NET_ID(std::stoi(loc_str));
		ap_comp_loc_info_t loc_info = {0, AP_NET_ID(world_info["locations"][loc_str]["item"].asInt())};
		if (world_info["locations"][loc_str]["progression"].asBool())
			loc_info.flags = 0b001;
		ap_comp_loc_infos[loc_id] = loc_info;
	}
    // Starting inventory
    ap_starting_inv.clear();
    for (unsigned int i = 0; i < world_info["inventory"].size(); i++)
        ap_starting_inv.push_back(AP_NET_ID(world_info["inventory"][i].asInt()));

	// Build up location names for all locations
    auto locations = ap_game_config["locations"];
    for (std::string level_name : locations.getMemberNames())
    {
        for (std::string sprite_id : locations[level_name]["sprites"].getMemberNames())
        {
            if (locations[level_name]["sprites"][sprite_id]["id"].isInt())
            {
                ap_net_id_t location_id = AP_NET_ID(locations[level_name]["sprites"][sprite_id]["id"].asInt());
                ap_comp_loc_names[location_id] = level_name + ": " + locations[level_name]["sprites"][sprite_id]["name"].asString();
            }
        }
    }

    for (std::string level_name : locations.getMemberNames())
    {
        for (std::string sector_id : locations[level_name]["sectors"].getMemberNames())
        {
            if (locations[level_name]["sectors"][sector_id]["id"].isInt())
            {
                ap_net_id_t location_id = AP_NET_ID(locations[level_name]["sectors"][sector_id]["id"].asInt());
                ap_comp_loc_names[location_id] = level_name + " Secret: " + locations[level_name]["sectors"][sector_id]["name"].asString();
            }
        }
    }

    for (std::string level_name : locations.getMemberNames())
    {
        for (std::string exit_tag : locations[level_name]["exits"].getMemberNames())
        {
            if (locations[level_name]["exits"][exit_tag]["id"].isInt())
            {
                ap_net_id_t location_id = AP_NET_ID(locations[level_name]["exits"][exit_tag]["id"].asInt());
                ap_comp_loc_names[location_id] = level_name + ": " + locations[level_name]["exits"][exit_tag]["name"].asString();
            }
        }
    }
}

void AP_Start_Compat()
{
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

std::vector<int64_t> AP_GetGameLocations_Compat()
{
	std::vector<int64_t> ret;
	for ( const auto &entry : ap_comp_loc_infos)
	{
		ret.push_back(entry.first);
	}
	return ret;
}

std::string AP_GetLocationName_Compat(int64_t location_id)
{
	return ap_comp_loc_names.count(location_id) ? ap_comp_loc_names.at(location_id) : std::string("Unknown location");
}

#endif
