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

USTRUCT(BlueprintType)
struct AUTOBLUEPRINTEDITOR_API FAutoBlueprintBuildResult
{
	GENERATED_BODY()

	/** True when the build, compile, and requested save all succeed. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	bool bCompiled = false;

	/** Always false when saving was not requested. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	bool bSaved = false;

	/** Intended object path; only CreatedAssets confirms that an object was created. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	FString AssetPath;

	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	TArray<FAutoBlueprintDiagnostic> Diagnostics;

	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	TArray<FString> CreatedAssets;

	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	TArray<FString> SavedPackages;

	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	TArray<FString> UnsavedPackages;
};
