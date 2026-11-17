#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "AutoBlueprintTypes.h"
#include "AutoBlueprintLuaTypes.h"
#include "AutoBlueprintSubsystem.generated.h"

UCLASS()
class AUTOBLUEPRINTEDITOR_API UAutoBlueprintSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	/** Validate again, then create one native Widget Blueprint; never replaces an existing package.
	 * Editor game thread outside PIE/SIE only. Failed compilation never saves the Widget.
	 * bSave=false still creates an in-memory asset. See result for partial creation and report path.
	 */
	FAutoBlueprintBuildResult BuildWidgetBlueprint(const FString& LayoutJson, const FAutoBlueprintBuildOptions& Options);

	/** Same build contract; reads a project-relative JSON under the validated layout directories. */
	FAutoBlueprintBuildResult BuildFromLayoutFile(const FString& ProjectRelativePath, const FAutoBlueprintBuildOptions& Options);

	FAutoBlueprintValidationResult ValidateLayout(const FString& LayoutJson);
	FAutoBlueprintValidationResult ValidateLayoutFile(const FString& ProjectRelativePath);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Synchronous execution on the idle editor game thread; rejects nested script requests. */
	FAutoBlueprintLuaRunResult RunEditorScript(const FAutoBlueprintLuaRunRequest& Request);

	/** Same main-thread, CreateOnly and save contract as UAutoBlueprintLibrary::BuildSmokeTestWidget. */
	FAutoBlueprintBuildResult BuildSmokeTestWidget(const FAutoBlueprintSmokeTestRequest& Request);

	/** TODO API: returns NOT_IMPLEMENTED for every request and the batch; never creates or saves assets. */
	FAutoBlueprintBatchBuildResult GenerateBatch(const FAutoBlueprintBatchRequest& Request);

private:
	bool bAcceptScriptRequests = false;
	bool bScriptInProgress = false;
	bool bBuildInProgress = false;
};
