#include "AutoBlueprintLibrary.h"

#include "AutoBlueprintEditorContext.h"
#include "AutoBlueprintSubsystem.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"

FAutoBlueprintBuildResult UAutoBlueprintLibrary::BuildSmokeTestWidget(const FAutoBlueprintSmokeTestRequest& Request)
{
	FAutoBlueprintBuildResult Result;
	FAutoBlueprintDiagnostic Diagnostic;
	if (!AutoBlueprint::CheckEditorContext(Diagnostic.Code, Diagnostic.Message))
	{
		Result.Diagnostics.Add(MoveTemp(Diagnostic));
		return Result;
	}

	UAutoBlueprintSubsystem* Subsystem = GEditor->GetEditorSubsystem<UAutoBlueprintSubsystem>();
	if (!Subsystem)
	{
		Diagnostic.Code = TEXT("SUBSYSTEM_UNAVAILABLE");
		Diagnostic.Message = TEXT("The AutoBlueprint editor subsystem is not initialized.");
		Result.Diagnostics.Add(MoveTemp(Diagnostic));
		return Result;
	}
	return Subsystem->BuildSmokeTestWidget(Request);
}
