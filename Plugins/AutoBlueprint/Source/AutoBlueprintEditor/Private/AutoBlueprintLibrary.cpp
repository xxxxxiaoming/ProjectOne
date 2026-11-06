#include "AutoBlueprintLibrary.h"

#include "AutoBlueprintEditorContext.h"
#include "AutoBlueprintSubsystem.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"

namespace
{
	UAutoBlueprintSubsystem* GetAvailableSubsystem(FAutoBlueprintDiagnostic& Diagnostic)
	{
		if (!AutoBlueprint::CheckEditorContext(Diagnostic.Code, Diagnostic.Message))
		{
			return nullptr;
		}
		UAutoBlueprintSubsystem* Subsystem = GEditor->GetEditorSubsystem<UAutoBlueprintSubsystem>();
		if (!Subsystem)
		{
			Diagnostic.Code = TEXT("SUBSYSTEM_UNAVAILABLE");
			Diagnostic.Message = TEXT("The AutoBlueprint editor subsystem is not initialized.");
		}
		return Subsystem;
	}
}

FAutoBlueprintBuildResult UAutoBlueprintLibrary::BuildSmokeTestWidget(const FAutoBlueprintSmokeTestRequest& Request)
{
	FAutoBlueprintDiagnostic Diagnostic;
	if (UAutoBlueprintSubsystem* Subsystem = GetAvailableSubsystem(Diagnostic))
	{
		return Subsystem->BuildSmokeTestWidget(Request);
	}
	FAutoBlueprintBuildResult Result;
	Result.Diagnostics.Add(MoveTemp(Diagnostic));
	return Result;
}

FAutoBlueprintBatchBuildResult UAutoBlueprintLibrary::GenerateBatch(const FAutoBlueprintBatchRequest& Request)
{
	FAutoBlueprintDiagnostic Diagnostic;
	if (UAutoBlueprintSubsystem* Subsystem = GetAvailableSubsystem(Diagnostic))
	{
		return Subsystem->GenerateBatch(Request);
	}
	FAutoBlueprintBatchBuildResult Result;
	Result.Diagnostics.Add(Diagnostic);
	Result.Results.SetNum(Request.Requests.Num());
	for (FAutoBlueprintBuildResult& Item : Result.Results)
	{
		Item.Diagnostics.Add(Diagnostic);
	}
	return Result;
}

FAutoBlueprintLuaRunResult UAutoBlueprintLibrary::RunEditorScript(const FAutoBlueprintLuaRunRequest& Request)
{
	FAutoBlueprintDiagnostic Diagnostic;
	if (UAutoBlueprintSubsystem* Subsystem = GetAvailableSubsystem(Diagnostic))
	{
		return Subsystem->RunEditorScript(Request);
	}
	FAutoBlueprintLuaRunResult Result;
	Result.Code = MoveTemp(Diagnostic.Code);
	Result.Message = MoveTemp(Diagnostic.Message);
	return Result;
}
