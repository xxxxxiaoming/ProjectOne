#pragma once

#include "CoreMinimal.h"

namespace AutoBlueprint
{
	/** Lexical containment under a trusted root; rejects absolute paths and traversal. */
	bool ResolveLuaScriptPath(const FString& Root, const FString& RelativePath,
		FString& OutPath, FString& OutCode, FString& OutMessage);

	/** Convert a dotted module identifier to a relative .lua path without filesystem access. */
	bool MakeLuaModulePath(const FString& ModuleName, FString& OutRelativePath);

	/** Bounded text read, up to 1 MiB; never executes the source. */
	bool LoadLuaScriptSource(const FString& Path, FString& OutSource, FString& OutCode, FString& OutMessage);
}
