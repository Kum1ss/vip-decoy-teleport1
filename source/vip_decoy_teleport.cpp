// [VIP] Decoy Teleport - module for Pisex VIP (https://github.com/Pisex/cs2-vip)
// Only the long-stable early part of the IVIPApi vtable is used (exists since core 1.2.2).

#include <stdio.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "vip_decoy_teleport.h"

vip_decoy_teleport g_vip_decoy_teleport;

IVIPApi*   g_pVIPCore = nullptr;
IUtilsApi* g_pUtils   = nullptr;

IVEngineServer2*   engine              = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem*     g_pEntitySystem     = nullptr;

PLUGIN_EXPOSE(vip_decoy_teleport, g_vip_decoy_teleport);

namespace
{
constexpr const char* kFeature       = "decoy_teleport";
constexpr const char* kParamCooldown = "decoy_teleport_cooldown";
constexpr const char* kParamLimit    = "decoy_teleport_limit";
constexpr const char* kParamMaxDist  = "decoy_teleport_max_distance";
constexpr const char* kParamRemove   = "decoy_teleport_remove_decoy";
constexpr const char* kParamZOffset  = "decoy_teleport_z_offset";

constexpr float kDefaultCooldown = 0.0f;
constexpr int   kDefaultLimit    = 0;
constexpr float kDefaultMaxDist  = 0.0f;
constexpr bool  kDefaultRemove   = true;
constexpr float kDefaultZOffset  = 1.0f;

constexpr int   kMaxPlayers = 64;
constexpr float kWorldLimit = 32768.0f;
constexpr const char* kDecoyClass = "decoy_projectile";

// State is bound to the owner's SteamID64: a reused slot resets automatically.
struct PlayerState
{
	uint64 steamId     = 0;
	float  nextUse     = 0.0f;
	int    usedRound   = 0;
	int    roundSerial = -1;
};

PlayerState g_State[kMaxPlayers];
int         g_iRoundSerial = 0;

void RefreshEntitySystem()
{
	g_pGameEntitySystem = g_pUtils->GetCGameEntitySystem();
	g_pEntitySystem     = g_pGameEntitySystem;
}

// Core returns -1 / "" when the key is absent from the player's group.
int ReadInt(int iSlot, const char* key, int fallback)
{
	const int v = g_pVIPCore->VIP_GetClientFeatureInt(iSlot, key);
	return v < 0 ? fallback : v;
}

float ReadFloat(int iSlot, const char* key, float fallback)
{
	const char* text = g_pVIPCore->VIP_GetClientFeatureString(iSlot, key);
	if (!text || !text[0])
		return fallback;
	char* end = nullptr;
	const float v = std::strtof(text, &end);
	return (end != text && std::isfinite(v)) ? v : fallback;
}

std::string FormatPhrase(const char* phrase, int number)
{
	std::string text = phrase ? phrase : "";
	const size_t pos = text.find("{1}");
	if (pos != std::string::npos)
		text.replace(pos, 3, std::to_string(number));
	return text;
}

void Say(int iSlot, const char* phraseKey, int number = 0)
{
	const char* phrase = g_pVIPCore->VIP_GetTranslate(phraseKey);
	if (!phrase || !phrase[0])
		return;
	const char* prefix = g_pVIPCore->VIP_GetTranslate("Prefix");
	const std::string text = std::string(prefix ? prefix : "") + " " + FormatPhrase(phrase, number);
	g_pUtils->PrintToChat(iSlot, "%s", text.c_str());
}

// Checked at the moment of use.
bool HasAccess(int iSlot)
{
	return g_pVIPCore->VIP_IsVIPLoaded()
		&& g_pVIPCore->VIP_IsClientVIP(iSlot)
		&& g_pVIPCore->VIP_GetClientFeatureBool(iSlot, kFeature);
}

bool BuildDestination(IGameEvent* pEvent, float zOffset, Vector& out)
{
	const float x = pEvent->GetFloat("x");
	const float y = pEvent->GetFloat("y");
	const float z = pEvent->GetFloat("z");
	if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
		return false;
	if (std::fabs(x) > kWorldLimit || std::fabs(y) > kWorldLimit || std::fabs(z) > kWorldLimit)
		return false;
	out = Vector(x, y, z + zOffset);
	return true;
}

// Deferred by a frame + "Kill" input: deleting an entity inside its own event is unsafe.
void ConsumeDecoy(IGameEvent* pEvent)
{
	const int entIndex = pEvent->GetInt("entityid");
	if (entIndex <= 0 || !g_pEntitySystem)
		return;
	CEntityInstance* pEnt = g_pEntitySystem->GetEntityInstance(CEntityIndex(entIndex));
	if (!pEnt || !pEnt->m_pEntity || strcmp(pEnt->m_pEntity->GetClassname(), kDecoyClass) != 0)
		return;

	CHandle<CBaseEntity> hDecoy(static_cast<CBaseEntity*>(pEnt));
	g_pUtils->NextFrame([hDecoy]()
	{
		CBaseEntity* pDecoy = hDecoy.Get();
		if (pDecoy)
			g_pUtils->AcceptEntityInput(pDecoy, "Kill");
	});
}

void OnDecoyStarted(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
	if (!pEvent || !g_pVIPCore || !g_pUtils)
		return;
	RefreshEntitySystem();

	CCSPlayerController* pController = static_cast<CCSPlayerController*>(pEvent->GetPlayerController("userid"));
	if (!pController)
		return;                                   // thrower disconnected

	const int iSlot = pController->GetPlayerSlot();
	if (iSlot < 0 || iSlot >= kMaxPlayers)
		return;

	CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
	if (!pPawn || !pPawn->IsAlive() || !pController->m_bPawnIsAlive())
		return;                                   // thrower died

	if (CEntityInstance* pEventPawn = pEvent->GetPlayerPawn("userid_pawn"))
		if (pEventPawn != pPawn)
			return;                               // respawned: stale throw

	if (!HasAccess(iSlot))
		return;                                   // stock decoy for everyone else

	const float cooldown = std::fmax(0.0f, ReadFloat(iSlot, kParamCooldown, kDefaultCooldown));
	const int   limit    = ReadInt  (iSlot, kParamLimit,   kDefaultLimit);
	const float maxDist  = std::fmax(0.0f, ReadFloat(iSlot, kParamMaxDist, kDefaultMaxDist));
	const bool  remove   = ReadInt  (iSlot, kParamRemove,  kDefaultRemove ? 1 : 0) != 0;
	const float zOffset  = std::fmin(64.0f, std::fmax(0.0f, ReadFloat(iSlot, kParamZOffset, kDefaultZOffset)));

	PlayerState& st = g_State[iSlot];
	const uint64 steamId = pController->m_steamID();
	if (st.steamId != steamId)
		st = PlayerState{ steamId };
	if (st.roundSerial != g_iRoundSerial)
	{
		st.roundSerial = g_iRoundSerial;
		st.usedRound   = 0;
	}

	const float now = g_pUtils->GetCGlobalVars()->curtime;
	if (cooldown > 0.0f && now < st.nextUse)
	{
		Say(iSlot, "DecoyTeleportCooldown", static_cast<int>(std::ceil(st.nextUse - now)));
		return;
	}
	if (limit > 0 && st.usedRound >= limit)
	{
		Say(iSlot, "DecoyTeleportLimit");
		return;
	}

	Vector dest;
	if (!BuildDestination(pEvent, zOffset, dest))
		return;

	if (maxDist > 0.0f && (dest - pPawn->GetAbsOrigin()).Length() > maxDist)
	{
		Say(iSlot, "DecoyTeleportTooFar");
		return;
	}

	const Vector zeroVelocity(0.0f, 0.0f, 0.0f);
	g_pUtils->TeleportEntity(pPawn, &dest, nullptr, &zeroVelocity);

	st.nextUse = now + cooldown;
	st.usedRound++;

	if (remove)
		ConsumeDecoy(pEvent);

	Say(iSlot, "DecoyTeleportDone");
}

void OnRoundStart(const char* szName, IGameEvent* pEvent, bool bDontBroadcast)
{
	++g_iRoundSerial;
}
} // namespace

bool vip_decoy_teleport::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);
	g_SMAPI->AddListener(this, this);
	return true;
}

bool vip_decoy_teleport::Unload(char *error, size_t maxlen)
{
	// The feature stays registered in the core until it reloads (no callbacks, so inert);
	// VIP_UnregisterFeature is a 2026 addition and is avoided for binary compatibility.
	if (g_pUtils)
		g_pUtils->ClearAllHooks(g_PLID);
	g_pVIPCore = nullptr;
	g_pUtils   = nullptr;
	return true;
}

void vip_decoy_teleport::AllPluginsLoaded()
{
	int ret = 0;

	g_pVIPCore = static_cast<IVIPApi*>(g_SMAPI->MetaFactory(VIP_INTERFACE, &ret, nullptr));
	if (ret == META_IFACE_FAILED || !g_pVIPCore)
	{
		ConColorMsg(Color(255, 0, 0, 255), "%s Failed to find the VIP core. Aborting.\n", GetLogTag());
		g_pVIPCore = nullptr;
		engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
		return;
	}

	g_pUtils = static_cast<IUtilsApi*>(g_SMAPI->MetaFactory(UTILS_INTERFACE, &ret, nullptr));
	if (ret == META_IFACE_FAILED || !g_pUtils)
	{
		ConColorMsg(Color(255, 0, 0, 255), "%s Failed to find the Utils API. Aborting.\n", GetLogTag());
		g_pUtils = nullptr;
		engine->ServerCommand(("meta unload " + std::to_string(g_PLID)).c_str());
		return;
	}

	g_pVIPCore->VIP_RegisterFeature(kFeature, VIP_BOOL, TOGGLABLE);
	g_pUtils->HookEvent(g_PLID, "decoy_started", OnDecoyStarted);
	g_pUtils->HookEvent(g_PLID, "round_start",   OnRoundStart);
}

const char *vip_decoy_teleport::GetLicense()     { return "Public"; }
const char *vip_decoy_teleport::GetVersion()     { return "1.0.0"; }
const char *vip_decoy_teleport::GetDate()        { return __DATE__; }
const char *vip_decoy_teleport::GetLogTag()      { return "[VIP-DECOY-TP]"; }
const char *vip_decoy_teleport::GetAuthor()      { return "Claude"; }
const char *vip_decoy_teleport::GetDescription() { return "Teleport VIP players to their Decoy grenade"; }
const char *vip_decoy_teleport::GetName()        { return "[VIP] Decoy Teleport"; }
const char *vip_decoy_teleport::GetURL()         { return ""; }
