#pragma once

#include "CoreMinimal.h"
#include "AutoBlueprintTypes.generated.h"

UENUM(BlueprintType)
enum class EAutoBlueprintDiagnosticSeverity : uint8
{
	Info,
	Warning,
	Error
};

USTRUCT(BlueprintType)
struct AUTOBLUEPRINTEDITOR_API FAutoBlueprintDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	EAutoBlueprintDiagnosticSeverity Severity = EAutoBlueprintDiagnosticSeverity::Error;

	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	FString Code;

	/** Empty for P0, which takes a reflected request rather than layout JSON. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	FString JsonPath;

	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	FString WidgetName;

	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	FString Message;
};

/** Fixed P0 fixture inputs. General layout parsing is introduced in P2/P3. */
USTRUCT(BlueprintType)
struct AUTOBLUEPRINTEDITOR_API FAutoBlueprintSmokeTestRequest
{
	GENERATED_BODY()

	/** New long package name below /Game/UI/AutoBlueprintTests/, without an extension. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoBlueprint")
	FString PackagePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoBlueprint")
	FString TitleText = TEXT("AutoBlueprint P0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoBlueprint")
	FString ButtonText = TEXT("Confirm");

	/** False still creates an in-memory asset; this is not a read-only validation flag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoBlueprint")
	bool bSave = true;
};

/** One Widget Blueprint in one newly created package. Never reuse this as a batch accumulator. */
USTRUCT(BlueprintType)
struct AUTOBLUEPRINTEDITOR_API FAutoBlueprintBuildResult
{
	GENERATED_BODY()

	/** True when this request's build, compile, and requested save all succeed. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	bool bSuccess = false;

	/** True only after this operation creates the target Widget Blueprint. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	bool bAssetCreated = false;

	/** Compile outcome for this Widget Blueprint only. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	bool bCompiled = false;

	/** True only if this call saved CreatedPackageName; false for unrequested, skipped, or failed saves. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	bool bSaved = false;

	/** Intended object path after path validation; does not prove that an asset was created. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	FString TargetAssetPath;

	/** Package actually created by this operation, even if asset creation later fails. Empty otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	FString CreatedPackageName;

	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	TArray<FAutoBlueprintDiagnostic> Diagnostics;
};

/** Reserved batch input. General layout requests belong to P2/P3; P0 uses fixed fixture requests. */
USTRUCT(BlueprintType)
struct AUTOBLUEPRINTEDITOR_API FAutoBlueprintBatchRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoBlueprint")
	TArray<FAutoBlueprintSmokeTestRequest> Requests;
};

/** Ordered per-request results; deliberately has no aggregate bSaved or duplicate package lists. */
USTRUCT(BlueprintType)
struct AUTOBLUEPRINTEDITOR_API FAutoBlueprintBatchBuildResult
{
	GENERATED_BODY()

	/** One entry per input request, in the same order, including requests that did not run. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	TArray<FAutoBlueprintBuildResult> Results;

	/** Batch-level failures, including NOT_IMPLEMENTED for an empty batch. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	TArray<FAutoBlueprintDiagnostic> Diagnostics;
};
