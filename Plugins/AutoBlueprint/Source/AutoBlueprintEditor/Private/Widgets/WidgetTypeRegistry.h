#pragma once
#include "CoreMinimal.h"
class UClass;
namespace AutoBlueprint
{
	enum class EChildPolicy : uint8 { Leaf, Single, Panel, DataDriven };
	struct FWidgetTypeDescriptor
	{
		const TCHAR* Name;
		const TCHAR* ChildSlot;
		EChildPolicy Children;
		UClass* (*GetClass)();
	};
	const FWidgetTypeDescriptor* FindWidgetType(const FString& Name);
}
