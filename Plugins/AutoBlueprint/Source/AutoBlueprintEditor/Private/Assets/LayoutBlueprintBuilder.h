#pragma once

#include "AutoBlueprintTypes.h"

namespace AutoBlueprint
{
	/** Caller holds the subsystem build guard on the idle editor game thread.
	 * Revalidates input and build capabilities before creating any package.
	 */
	FAutoBlueprintBuildResult BuildLayoutBlueprint(const FString& Json, const FAutoBlueprintBuildOptions& Options);
	FAutoBlueprintGenerationResult GenerateLayoutBlueprint(const FString& Json, const FAutoBlueprintBuildOptions& Options, bool bAllowResources = true);
}
