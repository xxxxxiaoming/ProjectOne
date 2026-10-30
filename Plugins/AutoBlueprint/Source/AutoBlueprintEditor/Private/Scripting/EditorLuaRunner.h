#pragma once

#include "CoreMinimal.h"

/** Script execution status, separate from the build result returned to Lua. */
struct FAutoBlueprintLuaRunResult
{
	bool bSuccess = false;
	FString Code;
	FString Message;
};

class FAutoBlueprintEditorLuaRunner
{
public:
	/** Runs one standalone P0 script from Scripts/Lua in a task-scoped UnLua environment. */
	FAutoBlueprintLuaRunResult Run(const FString& ScriptFilename, const FString& PackagePath);

private:
	bool bRunning = false;
};
