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

#if WITH_DEV_AUTOMATION_TESTS
	/** Instance-local fault injection: exercise missing-state cleanup without provoking an allocation failure. */
	bool bSimulateUnavailableStateForTest = false;
#endif

private:
	bool bRunning = false;
};
