#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AutoBlueprintTypes.h"
#include "AutoBlueprintLuaTypes.h"
#include "AutoBlueprintLibrary.generated.h"

/** Editor scripting facade. Generated widgets never reference this class. */
UCLASS()
class AUTOBLUEPRINTEDITOR_API UAutoBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Read-only P2 preflight on the idle editor game thread. Never creates/imports/saves assets. */
	UFUNCTION(BlueprintCallable, Category = "AutoBlueprint|Validation")
	static FAutoBlueprintValidationResult ValidateLayout(const FString& LayoutJson);
	/** Read a project-relative .json below UIBlueprintLayouts or this plugin's Examples directory. */
	UFUNCTION(BlueprintCallable, Category = "AutoBlueprint|Validation")
	static FAutoBlueprintValidationResult ValidateLayoutFile(const FString& ProjectRelativePath);
	/**
	 * Run a trusted plugin tool synchronously on the editor game thread outside PIE/SIE.
	 * ScriptPath is relative to Scripts/Lua; module.run receives Arguments as a Lua table.
	 * Releases the fresh task VM before returning; nested script requests are rejected.
	 * Scripts may call build APIs. A later script failure does not roll back saved assets.
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoBlueprint|Tools")
	static FAutoBlueprintLuaRunResult RunEditorScript(const FAutoBlueprintLuaRunRequest& Request);

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
