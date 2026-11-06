#pragma once

#include <functional>
#include <string>

class CGameEntitySystem;
class CEntitySystem;
class CGlobalVars;
class CBaseEntity;
class Vector;
class QAngle;

#define Utils_INTERFACE "UtilsApi"

class IUtilsApi
{
public:
	virtual void PrintToChat(int iSlot, const char* msg, ...) = 0;
	virtual void NextFrame(std::function<void()> fn) = 0;
	virtual CGameEntitySystem* GetCGameEntitySystem() = 0;
	virtual CGlobalVars* GetCGlobalVars() = 0;
	virtual void AcceptEntityInput(CBaseEntity* pEntity, const char* input, CEntityInstance* pActivator = nullptr, CEntityInstance* pCaller = nullptr, variant_t* value = nullptr) = 0;
	virtual void HookEvent(PluginId id, const char* name, void (*callback)(const char*, IGameEvent*, bool)) = 0;
	virtual void ClearAllHooks(PluginId id) = 0;
	virtual void TeleportEntity(CBaseEntity* pEntity, const Vector* pos, const QAngle* ang, const Vector* vel) = 0;
};
