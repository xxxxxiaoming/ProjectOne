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
	/** P4 task API: revalidate, resolve/import textures, build one Widget and optionally save each new package.
	 * Idle editor game thread only. CreateOnly; no overwrite, reimport, deletion or rollback.
	 * bSave=false creates unsaved assets. Inspect independent results on partial failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoBlueprint|Generation")
	static FAutoBlueprintGenerationResult GenerateFromLayout(const FString& LayoutJson, const FAutoBlueprintBuildOptions& Options);

	/** Same task contract; reads a JSON from the existing allowed project-relative layout directories. */
	UFUNCTION(BlueprintCallable, Category = "AutoBlueprint|Generation")
	static FAutoBlueprintGenerationResult GenerateFromLayoutFile(const FString& ProjectRelativePath, const FAutoBlueprintBuildOptions& Options);

	/** Validate again, then create one native Widget Blueprint; never replaces an existing package.
	 * Declared textures require GenerateFromLayout/GenerateFromLayoutFile and their separate resource results.
	 * Editor game thread outside PIE/SIE only. Failed compilation never saves the Widget.
	 * bSave=false still creates an in-memory asset. See result for partial creation and report path.
	 */
	UFUNCTION(BlueprintCallable, Category = "AutoBlueprint|Build")
	static FAutoBlueprintBuildResult BuildWidgetBlueprint(const FString& LayoutJson, const FAutoBlueprintBuildOptions& Options);

	/** Same build contract; reads a project-relative JSON under the validated layout directories. */
	UFUNCTION(BlueprintCallable, Category = "AutoBlueprint|Build")
	static FAutoBlueprintBuildResult BuildFromLayoutFile(const FString& ProjectRelativePath, const FAutoBlueprintBuildOptions& Options);

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
	 * Scripts may call build APIs. Operations preserves completed subsystem generation results even after Lua errors.
	 * bSuccess describes script completion only; inspect each operation. No rollback or batch scheduling.
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
