#pragma once

#include "CoreMinimal.h"

class UClass;
class UWidget;
class UPanelSlot;
struct FAutoBlueprintDiagnostic;

namespace AutoBlueprint
{
	struct FLayoutNode;
	struct FLayoutValue;

	enum class EChildPolicy : uint8
	{
		Leaf,
		Single,
		Panel,
		DataDriven
	};

	struct FWidgetTypeDescriptor
	{
		const TCHAR* Name;
		const TCHAR* ChildSlot;
		EChildPolicy Children;
		UClass* (*GetClass)();
		bool bCanBuild = false;
		bool (*ValidateBuild)(const FLayoutNode&, TArray<FAutoBlueprintDiagnostic>&) = nullptr;
		void (*ApplyProperties)(UWidget&, const FLayoutNode&) = nullptr;
		bool (*ApplyChildSlot)(UPanelSlot&, const FLayoutValue&) = nullptr;
	};

	const FWidgetTypeDescriptor* FindWidgetType(const FString& Name);
}
