#pragma once

#include "CoreMinimal.h"

struct FAutoBlueprintSmokeTestRequest;

namespace AutoBlueprint
{
	/** Compatibility fixture data, serialized through UE JSON to preserve arbitrary input text. */
	FString MakeSmokeTestLayoutJson(const FAutoBlueprintSmokeTestRequest& Request);
}
