#pragma once

#include "CoreMinimal.h"

class UWidgetBlueprint;
struct FAutoBlueprintDiagnostic;

namespace AutoBlueprint
{
	struct FLayoutSpec;

	/** Read-only capability gate after protocol validation, before any package creation. */
	bool ValidateLayoutBuildCapabilities(const FLayoutSpec& Layout, TArray<FAutoBlueprintDiagnostic>& Diagnostics);

	/** Construct asset templates in the Blueprint-owned tree from a fully validated layout. */
	bool BuildLayoutWidgetTree(UWidgetBlueprint& Blueprint, const FLayoutSpec& Layout,
		TArray<FAutoBlueprintDiagnostic>& Diagnostics);
}
