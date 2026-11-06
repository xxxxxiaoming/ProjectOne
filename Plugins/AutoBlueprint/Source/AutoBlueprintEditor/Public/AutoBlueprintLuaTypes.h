#pragma once

#include "CoreMinimal.h"
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
};
