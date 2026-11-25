#pragma once

#include "CoreMinimal.h"
#include "AutoBlueprintTypes.h"
#include "AutoBlueprintLuaTypes.generated.h"

/** Synchronous editor tool invocation. Arguments are data, never executable source. */
USTRUCT(BlueprintType)
struct AUTOBLUEPRINTEDITOR_API FAutoBlueprintLuaRunRequest
{
	GENERATED_BODY()

	/** Relative .lua file below the plugin's Scripts/Lua, using forward slashes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoBlueprint")
	FString ScriptPath;

	/** Ordered strings for module.run(arguments). At most 32 entries of 4096 characters each. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoBlueprint")
	TArray<FString> Arguments;
};

/** A completed generation call within one script, in completion order. Contains values, not UObject references. */
USTRUCT(BlueprintType)
struct AUTOBLUEPRINTEDITOR_API FAutoBlueprintScriptOperation
{
	GENERATED_BODY()

	/** Subsystem entry that produced this result. Smoke-test success delegates to BuildWidgetBlueprint. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	FString Operation;

	/** Legacy Build calls populate Widget; task calls also populate Resources and task diagnostics. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	FAutoBlueprintGenerationResult Result;
};

/** Host execution status, separate from each asset's build/save result. */
USTRUCT(BlueprintType)
struct AUTOBLUEPRINTEDITOR_API FAutoBlueprintLuaRunResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	FString Code;

	/** Lua errors include the original message, source location, and traceback. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	FString Message;

	/** Resolved absolute entry path; empty if path resolution did not complete. */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	FString ScriptPath;

	/** Completed subsystem build/generation calls, including failures. Preserved even if Lua later throws.
	 * Not a batch API or rollback log; validation and nested script requests are not generation operations.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "AutoBlueprint")
	TArray<FAutoBlueprintScriptOperation> Operations;
};
