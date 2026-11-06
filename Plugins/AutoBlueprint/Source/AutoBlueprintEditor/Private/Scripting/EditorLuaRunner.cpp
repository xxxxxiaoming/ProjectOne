#include "Scripting/EditorLuaRunner.h"

#include "AutoBlueprintEditorContext.h"
#include "Interfaces/IPluginManager.h"
#include "LuaEnv.h"
#include "Misc/Paths.h"
#include "Scripting/LuaScriptSource.h"
#include "Templates/UnrealTemplate.h"
#include "lua.hpp"

namespace
{
	// DoString owns UnLua's VM/dangling-reference guards.
	// Module discovery is controlled for trusted tools; this is not an untrusted-code sandbox.
	const TCHAR* TaskBootstrap = TEXT(R"lua(
local task = AutoBlueprintTask
AutoBlueprintTask = nil
local readModule = task.readModule
package.path = ""
package.cpath = ""
package.searchers = {}

-- Do not reuse UnLua's already initialized package.loaded table or game loaders.
local loaded = {}
local loading = {}
local moduleCount = 0
local function traceback(message)
    return debug.traceback(tostring(message), 2)
end

require = function(name)
    assert(type(name) == "string", "INVALID_MODULE_NAME: require expects a dotted module name")
    if loaded[name] then
        return loaded[name]
    end
    assert(not loading[name], "LUA_MODULE_CYCLE: " .. name)
    assert(moduleCount < 64, "LUA_MODULE_LIMIT: at most 64 tool modules may load per task")
    local source, chunkName = readModule(name)
    assert(source, chunkName)
    local chunk, loadError = load(source, chunkName, "t")
    assert(chunk, loadError)
    loading[name] = true
    moduleCount = moduleCount + 1
    local ok, value = xpcall(chunk, traceback, name)
    loading[name] = nil
    if not ok then
        error(value, 0)
    end
    if value == nil then
        value = true
    end
    loaded[name] = value
    return value
end

local ok, message = xpcall(function()
    local chunk, loadError = load(task.source, task.chunkName, "t")
    assert(chunk, loadError)
    local module = chunk()
    assert(type(module) == "table" and type(module.run) == "function",
        "Tool script must return a table containing run(arguments)")
    assert(module.run(task.arguments) == true,
        "Tool script must return true after successful completion")
end, traceback)
return ok, message
)lua");

	void PushString(lua_State* State, const FString& Value)
	{
		const FTCHARToUTF8 Utf8(*Value);
		lua_pushlstring(State, Utf8.Get(), Utf8.Length());
	}

	void SetStringField(lua_State* State, const char* Name, const FString& Value)
	{
		PushString(State, Value);
		lua_setfield(State, -2, Name);
	}

	int32 ModuleReadFailure(lua_State* State, const FString& Code, const FString& Message)
	{
		// Raise in Lua after these C++ locals are destroyed, not via luaL_error/longjmp here.
		lua_pushnil(State);
		PushString(State, Code + TEXT(": ") + Message);
		return 2;
	}

	int32 ReadToolModule(lua_State* State)
	{
		if (lua_type(State, 1) != LUA_TSTRING)
		{
			return ModuleReadFailure(State, TEXT("INVALID_MODULE_NAME"), TEXT("A module name must be a string."));
		}
		size_t Length = 0;
		const char* Name = lua_tolstring(State, 1, &Length);
		if (Length == 0 || Length > 128)
		{
			return ModuleReadFailure(State, TEXT("INVALID_MODULE_NAME"), TEXT("Module names must contain 1 to 128 ASCII characters."));
		}
		const FUTF8ToTCHAR ConvertedName(Name, static_cast<int32>(Length));
		const FString ModuleName(ConvertedName.Length(), ConvertedName.Get());
		FString RelativePath;
		if (!AutoBlueprint::MakeLuaModulePath(ModuleName, RelativePath))
		{
			return ModuleReadFailure(State, TEXT("INVALID_MODULE_NAME"), TEXT("Use dotted ASCII identifiers without a .lua extension."));
		}
		const FString Root = UTF8_TO_TCHAR(lua_tostring(State, lua_upvalueindex(1)));
		FString Path;
		FString Code;
		FString Message;
		FString Source;
		if (!AutoBlueprint::ResolveLuaScriptPath(Root, RelativePath, Path, Code, Message)
			|| !AutoBlueprint::LoadLuaScriptSource(Path, Source, Code, Message))
		{
			return ModuleReadFailure(State, Code, Message);
		}
		PushString(State, Source);
		PushString(State, TEXT("@") + Path);
		return 2;
	}
}

FAutoBlueprintLuaRunResult FAutoBlueprintEditorLuaRunner::Run(const FString& ScriptFilename, const FString& PackagePath)
{
	FAutoBlueprintLuaRunRequest Request;
	Request.ScriptPath = ScriptFilename;
	if (!PackagePath.IsEmpty())
	{
		Request.Arguments.Add(PackagePath);
	}
	return Run(Request);
}

FAutoBlueprintLuaRunResult FAutoBlueprintEditorLuaRunner::Run(const FAutoBlueprintLuaRunRequest& Request)
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
	const bool bHasOversizedArgument = Request.Arguments.ContainsByPredicate([](const FString& Argument)
	{
		return Argument.Len() > 4096;
	});
	if (Request.Arguments.Num() > 32 || bHasOversizedArgument)
	{
		Result.Code = TEXT("INVALID_ARGUMENTS");
		Result.Message = TEXT("Tool scripts accept at most 32 arguments of at most 4096 characters each.");
		return Result;
	}
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AutoBlueprint"));
	if (!Plugin.IsValid())
	{
		Result.Code = TEXT("PLUGIN_NOT_FOUND");
		Result.Message = TEXT("Cannot locate the AutoBlueprint plugin directory.");
		return Result;
	}
	const FString ScriptRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Scripts/Lua")));
	FString Source;
	if (!AutoBlueprint::ResolveLuaScriptPath(ScriptRoot, Request.ScriptPath, Result.ScriptPath, Result.Code, Result.Message)
		|| !AutoBlueprint::LoadLuaScriptSource(Result.ScriptPath, Source, Result.Code, Result.Message))
	{
		return Result;
	}

	// No Start/SetActive/GetEnv: construction initializes reflection, destruction releases this VM.
	UnLua::FLuaEnv Environment;
	Environment.SetName(TEXT("AutoBlueprint_Tool"));
	lua_State* State = Environment.GetMainState();
	if (!State)
	{
		Result.Code = TEXT("LUA_ENV_INIT_FAILED");
		Result.Message = TEXT("UnLua did not provide a Lua state.");
		return Result;
	}
	const int32 StackBase = lua_gettop(State);
	lua_createtable(State, 0, 4);
	SetStringField(State, "source", Source);
	SetStringField(State, "chunkName", TEXT("@") + Result.ScriptPath);
	lua_createtable(State, Request.Arguments.Num(), 0);
	for (int32 Index = 0; Index < Request.Arguments.Num(); ++Index)
	{
		PushString(State, Request.Arguments[Index]);
		lua_rawseti(State, -2, Index + 1);
	}
	lua_setfield(State, -2, "arguments");
	// The closure owns a Lua string, not a pointer to a C++ local or subsystem.
	PushString(State, ScriptRoot);
	lua_pushcclosure(State, &ReadToolModule, 1);
	lua_setfield(State, -2, "readModule");
	lua_setglobal(State, "AutoBlueprintTask");

	if (!Environment.DoString(TaskBootstrap, TEXT("@AutoBlueprint/ToolRunner")))
	{
		Result.Code = TEXT("LUA_SCRIPT_FAILED");
		Result.Message = TEXT("The tool bootstrap failed. See LogUnLua for the VM diagnostic.");
		return Result;
	}
	if (lua_gettop(State) != StackBase + 2 || !lua_isboolean(State, StackBase + 1))
	{
		Result.Code = TEXT("LUA_SCRIPT_FAILED");
		Result.Message = TEXT("The tool bootstrap returned an invalid result.");
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
