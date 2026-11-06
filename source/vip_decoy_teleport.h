#ifndef _INCLUDE_VIP_DECOY_TELEPORT_H_
#define _INCLUDE_VIP_DECOY_TELEPORT_H_

#include <ISmmPlugin.h>
#include <igameevents.h>
#include <iplayerinfo.h>
#include "utlvector.h"
#include "ehandle.h"
#include <sh_vector.h>
#include <entity2/entitysystem.h>
#include "utils.hpp"
#include "module.h"
#include "CCSPlayerController.h"
#include "CCSPlayerPawn.h"
#include "iserver.h"
#include "vector.h"
#include "include/vip.h"
#include "include/utils.h"
#include <string>
#include <map>

class vip_decoy_teleport : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	void AllPluginsLoaded();
public:
	const char *GetAuthor();
	const char *GetName();
	const char *GetDescription();
	const char *GetURL();
	const char *GetLicense();
	const char *GetVersion();
	const char *GetDate();
	const char *GetLogTag();
};

extern vip_decoy_teleport g_vip_decoy_teleport;

PLUGIN_GLOBALVARS();

#endif
