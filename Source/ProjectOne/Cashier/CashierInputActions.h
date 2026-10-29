#pragma once

#include "CoreMinimal.h"

/**
 * Logical Enhanced Input action names for cashier mode A.
 * Keep these identical to Content/Script/Cashier/CashierTypes.lua.
 * IMC / IA data assets are created in the editor. See Content/UI/Cashier/README.md.
 * There is no UndoScan action.
 */
struct PROJECTONE_API FCashierInputActions
{
	static const FName ScanItem;
	static const FName AddDenom1;
	static const FName AddDenom5;
	static const FName AddDenom10;
	static const FName RemoveLastDenom;
	static const FName Confirm;
	static const FName FocusNext;
	static const FName FocusPrev;
	static const FName OpenSettle;

	static constexpr int32 Denom1 = 1;
	static constexpr int32 Denom5 = 5;
	static constexpr int32 Denom10 = 10;

	static bool IsLegalDenom(int32 Value)
	{
		return Value == Denom1 || Value == Denom5 || Value == Denom10;
	}
};
