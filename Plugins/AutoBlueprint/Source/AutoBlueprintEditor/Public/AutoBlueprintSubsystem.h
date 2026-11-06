#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "AutoBlueprintTypes.h"
#include "AutoBlueprintSubsystem.generated.h"

UCLASS()
class AUTOBLUEPRINTEDITOR_API UAutoBlueprintSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	/** Same main-thread, CreateOnly and save contract as UAutoBlueprintLibrary::BuildSmokeTestWidget. */
	FAutoBlueprintBuildResult BuildSmokeTestWidget(const FAutoBlueprintSmokeTestRequest& Request);

	/** TODO API: returns NOT_IMPLEMENTED for every request and the batch; never creates or saves assets. */
	FAutoBlueprintBatchBuildResult GenerateBatch(const FAutoBlueprintBatchRequest& Request);

private:
	bool bBuildInProgress = false;
};
