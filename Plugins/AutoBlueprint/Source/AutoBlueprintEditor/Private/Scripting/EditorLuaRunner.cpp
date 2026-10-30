#include "Scripting/EditorLuaRunner.h"

#include "AutoBlueprintEditorContext.h"
#include "Interfaces/IPluginManager.h"
#include "LuaEnv.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Templates/UnrealTemplate.h"
#include "lua.hpp"

namespace
{
	// Keep the protected call inside DoString so UnLua owns its dangling-reference/VM guards.
	// Inputs are Lua values, never interpolated into executable source.
	const TCHAR* TaskBootstrap = TEXT(R"lua(
local task = AutoBlueprintTask
AutoBlueprintTask = nil

-- P0 scripts are standalone. Prevent accidental require() fallback to game scripts.
package.path = ""
package.cpath = ""
package.searchers = {}

local ok, message = xpcall(function()
    local chunk, loadError = load(task.source, task.chunkName, "t")
    assert(chunk, loadError)
    local module = chunk()
    assert(type(module) == "table" and type(module.run) == "function",
        "P0 script must return a table containing run(packagePath)")
    assert(module.run(task.packagePath) == true,
        "P0 script must return true after successful completion")
end, function(message)
    return debug.traceback(tostring(message), 2)
end)
return ok, message
)lua");

	void SetStringField(lua_State* State, const char* Name, const FString& Value)
	{
		const FTCHARToUTF8 Utf8(*Value);
		lua_pushlstring(State, Utf8.Get(), Utf8.Length());
		lua_setfield(State, -2, Name);
	}
}

FAutoBlueprintLuaRunResult FAutoBlueprintEditorLuaRunner::Run(const FString& ScriptFilename, const FString& PackagePath)
{
	FAutoBlueprintLuaRunResult Result;
	if (!AutoBlueprint::CheckEditorContext(Result.Code, Result.Message))
	{
		return Result;
	}
	if (bRunning)
	{
		Result.Code = TEXT("EDITOR_BUSY");
		Result.Message = TEXT("An AutoBlueprint Lua task is already running.");
		return Result;
	}
	TGuardValue<bool> RunningGuard(bRunning, true);

	// P0 exposes these two fixtures only; general script/module loading belongs to P1.
	if (ScriptFilename != TEXT("p0_generate.lua") && ScriptFilename != TEXT("p0_error.lua"))
	{
		Result.Code = TEXT("INVALID_SCRIPT_PATH");
		Result.Message = TEXT("P0 only accepts the bundled generation and error-probe scripts.");
		return Result;
	}
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AutoBlueprint"));
	if (!Plugin.IsValid())
	{
		Result.Code = TEXT("LUA_SCRIPT_NOT_FOUND");
		Result.Message = TEXT("Cannot locate the AutoBlueprint plugin directory.");
		return Result;
	}
	const FString ScriptPath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(Plugin->GetBaseDir(), TEXT("Scripts/Lua"), ScriptFilename));
	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *ScriptPath))
	{
		Result.Code = TEXT("LUA_SCRIPT_NOT_FOUND");
		Result.Message = TEXT("Cannot read Lua script: ") + ScriptPath;
		return Result;
	}

	// FLuaEnv initializes reflection itself. Do not call the game's SetActive/GetEnv/Start.
	// Stack ownership closes this VM and removes its listeners on every return path.
	UnLua::FLuaEnv Environment;
	Environment.SetName(TEXT("AutoBlueprint_P0"));
	lua_State* State = Environment.GetMainState();
	if (!State)
	{
		Result.Code = TEXT("LUA_ENV_INIT_FAILED");
		Result.Message = TEXT("UnLua did not provide a Lua state.");
		return Result;
	}
	const int32 StackBase = lua_gettop(State);
	lua_createtable(State, 0, 3);
	SetStringField(State, "source", Source);
	SetStringField(State, "chunkName", TEXT("@") + ScriptPath);
	SetStringField(State, "packagePath", PackagePath);
	lua_setglobal(State, "AutoBlueprintTask");

	if (!Environment.DoString(TaskBootstrap, TEXT("@AutoBlueprint/P0Runner")))
	{
		Result.Code = TEXT("LUA_SCRIPT_FAILED");
		Result.Message = TEXT("The P0 Lua bootstrap failed. See LogUnLua for the VM diagnostic.");
		return Result;
	}
	if (lua_gettop(State) != StackBase + 2 || !lua_isboolean(State, StackBase + 1))
	{
		Result.Code = TEXT("LUA_SCRIPT_FAILED");
		Result.Message = TEXT("The P0 Lua bootstrap returned an invalid result.");
		return Result;
	}
	Result.bSuccess = lua_toboolean(State, StackBase + 1) != 0;
	if (!Result.bSuccess)
	{
		Result.Code = TEXT("LUA_SCRIPT_FAILED");
		const char* Error = lua_tostring(State, StackBase + 2);
		Result.Message = Error ? UTF8_TO_TCHAR(Error) : TEXT("Lua failed without a text diagnostic.");
	}
	return Result;
}
