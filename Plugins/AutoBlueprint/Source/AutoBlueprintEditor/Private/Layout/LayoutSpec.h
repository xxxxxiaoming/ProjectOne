#pragma once
#include "CoreMinimal.h"
namespace AutoBlueprint
{
	// Validated property values retain their protocol type, without exposing JSON to builders.
	enum class ELayoutValueKind : uint8 { String, Number, Boolean, Array, Object };
	struct FLayoutValue
	{
		ELayoutValueKind Kind = ELayoutValueKind::Object;
		FString String;
		double Number = 0;
		bool bBoolean = false;
		TArray<FLayoutValue> Elements;
		TMap<FString, TSharedPtr<FLayoutValue>> Fields;
	};
	struct FLayoutNode
	{
		FName Name;
		FString Type;
		bool bIsVariable = false;
		FString JsonPath;
		FString SlotType;
		FLayoutValue Slot;
		FLayoutValue Properties;
		TArray<FLayoutNode> Children;
	};
	struct FLayoutResource
	{
		FString Alias;
		FString ObjectPath;
		FString SourceFile;
		FString PackagePath;
	};
	struct FLayoutSpec
	{
		int32 SchemaVersion = 1;
		FString PackagePath;
		FVector2D DesignSize = FVector2D::ZeroVector;
		TArray<FLayoutResource> Resources;
		FLayoutNode Root;
	};
}
