#pragma once

#include "AutoBlueprintTypes.h"

namespace AutoBlueprint
{
	/** Writes only a Saved JSON report. Report failure adds a warning, never changes asset outcomes. */
	void WriteBuildReport(FAutoBlueprintBuildResult& Result, const FAutoBlueprintBuildOptions& Options);
	void WriteGenerationReport(FAutoBlueprintGenerationResult& Result, const FAutoBlueprintBuildOptions& Options);
}
