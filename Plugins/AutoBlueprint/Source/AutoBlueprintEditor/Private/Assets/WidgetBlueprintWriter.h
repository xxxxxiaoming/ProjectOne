#pragma once

#include "CoreMinimal.h"

class UWidgetBlueprint;
struct FAutoBlueprintBuildResult;

namespace AutoBlueprint
{
	/** Compile a newly created asset, then optionally save its package without replacing a disk asset. */
	void CompileAndSaveWidgetBlueprint(UWidgetBlueprint& Blueprint, bool bSave, FAutoBlueprintBuildResult& Result);
}
