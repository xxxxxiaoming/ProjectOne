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
