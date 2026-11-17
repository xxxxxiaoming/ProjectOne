#pragma once

#include "CoreMinimal.h"

class UWidget;
class UPanelSlot;
struct FAutoBlueprintDiagnostic;

namespace AutoBlueprint
{
	struct FLayoutNode;
	struct FLayoutValue;

	// Registered callbacks consume schema-normalized values only.
	bool ValidateBasicTextBuild(const FLayoutNode& Node, TArray<FAutoBlueprintDiagnostic>& Diagnostics);
	bool ValidateBasicButtonBuild(const FLayoutNode& Node, TArray<FAutoBlueprintDiagnostic>& Diagnostics);
	void ApplyCommonWidgetProperties(UWidget& Widget, const FLayoutNode& Node);
	void ApplyBasicTextProperties(UWidget& Widget, const FLayoutNode& Node);
	void ApplyBasicButtonProperties(UWidget& Widget, const FLayoutNode& Node);
	bool ApplyCanvasChildSlot(UPanelSlot& Slot, const FLayoutValue& Values);
	bool ApplyButtonChildSlot(UPanelSlot& Slot, const FLayoutValue& Values);
}
