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

private:
	bool bBuildInProgress = false;
};
