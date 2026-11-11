#pragma once
#include "CoreMinimal.h"
#include "AutoBlueprintTypes.h"
class FJsonObject;
namespace AutoBlueprint
{
	// Applies bundled structural rules and copies defaults into the parsed document.
	bool ValidateLayoutStructure(const TSharedPtr<FJsonObject>& Document, FAutoBlueprintValidationResult& Result);
}
