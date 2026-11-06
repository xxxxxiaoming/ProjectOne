#pragma once

#include "CoreMinimal.h"
#include "AutoBlueprintLuaTypes.h"

/** Internal synchronous host. The subsystem serializes all public script requests. */
class FAutoBlueprintEditorLuaRunner
{
public:
	/** Fresh VM per call; require discovers only plugin tool modules. */
	FAutoBlueprintLuaRunResult Run(const FAutoBlueprintLuaRunRequest& Request);

	/** Compatibility adapter for P0 C++ tests; uses the same P1 host and argument contract. */
	FAutoBlueprintLuaRunResult Run(const FString& ScriptFilename, const FString& PackagePath);

private:
	bool bRunning = false;
};
