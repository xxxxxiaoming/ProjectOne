#pragma once

#include "CoreMinimal.h"

class UWidgetBlueprint;
struct FAutoBlueprintSmokeTestRequest;

namespace AutoBlueprint
{
	/** Fixed P0 fixture; replaced by registered layout handlers when P2/P3 are implemented. */
	bool BuildSmokeTestWidgetTree(UWidgetBlueprint& Blueprint, const FAutoBlueprintSmokeTestRequest& Request);
}
