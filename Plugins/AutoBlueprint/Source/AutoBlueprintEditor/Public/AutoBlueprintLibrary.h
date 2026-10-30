#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AutoBlueprintTypes.h"
#include "AutoBlueprintLibrary.generated.h"

/** Editor scripting facade. Generated widgets never reference this class. */
UCLASS()
class AUTOBLUEPRINTEDITOR_API UAutoBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * P0 only: create the fixed Canvas/Text/Button fixture using CreateOnly.
	 * Call on the editor game thread outside PIE/SIE. Saves only when Request.bSave is true.
	 * Failures return diagnostics and any created/unsaved packages; no existing asset is replaced.
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoBlueprint|P0")
	static FAutoBlueprintBuildResult BuildSmokeTestWidget(const FAutoBlueprintSmokeTestRequest& Request);
};
