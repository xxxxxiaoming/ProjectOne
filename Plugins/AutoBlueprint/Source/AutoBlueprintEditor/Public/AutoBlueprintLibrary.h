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
	 * Returns one UI result, including its created package on partial failure; no existing asset is replaced.
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoBlueprint|P0")
	static FAutoBlueprintBuildResult BuildSmokeTestWidget(const FAutoBlueprintSmokeTestRequest& Request);

	/**
	 * TODO API: batch generation is not implemented.
	 * Call on the editor game thread outside PIE/SIE; the facade validates editor availability.
	 * Returns NOT_IMPLEMENTED at batch and item level, including for an empty batch.
	 * Does not validate paths, build, compile, or save. Results retain input order.
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoBlueprint", meta = (DisplayName = "Generate Batch (Not Implemented)"))
	static FAutoBlueprintBatchBuildResult GenerateBatch(const FAutoBlueprintBatchRequest& Request);
};
