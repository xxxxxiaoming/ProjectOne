#pragma once

#include "CoreMinimal.h"
#include "AutoBlueprintTypes.h"

class UWidgetBlueprint;

namespace AutoBlueprint
{
	/** Private per-call write outcome; has no ownership of the enclosing build or batch result. */
	struct FAutoBlueprintWidgetWriteResult
	{
		bool bCompiled = false;
		bool bSaved = false;
		TArray<FAutoBlueprintDiagnostic> Diagnostics;
	};

	/**
	 * Compile one newly created asset, then optionally save its package using CreateOnly.
	 * Must run on the editor game thread outside PIE/SIE after successful tree construction.
	 * Each call returns a fresh result; failures never overwrite the outcome of an earlier call.
	 */
	FAutoBlueprintWidgetWriteResult CompileAndSaveWidgetBlueprint(UWidgetBlueprint& Blueprint, bool bSave);
}
