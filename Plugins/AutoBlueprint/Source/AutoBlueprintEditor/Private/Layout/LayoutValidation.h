#pragma once
#include "CoreMinimal.h"
#include "AutoBlueprintTypes.h"
#include "Layout/LayoutSpec.h"
namespace AutoBlueprint
{
	// Main thread, read-only preflight. OutLayout is assigned only on complete success.
	FAutoBlueprintValidationResult ValidateLayoutJson(const FString& Json, FLayoutSpec& OutLayout);
	bool ReadLayoutFile(const FString& RelativePath, FString& OutJson, FAutoBlueprintValidationResult& Result);
}
