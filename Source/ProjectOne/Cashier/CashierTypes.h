#pragma once

#include "CoreMinimal.h"
#include "CashierTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCashier, Log, All);

/** Mirrors Content/Script/Cashier/CashierTypes.lua phase names. */
UENUM(BlueprintType)
enum class ECashierPhase : uint8
{
	WaitCustomer UMETA(DisplayName = "WaitCustomer"),
	Scanning UMETA(DisplayName = "Scanning"),
	PayShown UMETA(DisplayName = "PayShown"),
	MakingChange UMETA(DisplayName = "MakingChange"),
	Confirm UMETA(DisplayName = "Confirm"),
	NextCustomer UMETA(DisplayName = "NextCustomer"),
	ShiftSettle UMETA(DisplayName = "ShiftSettle"),
};

/** One basket row. Index is 1-based, matching the Lua session. */
USTRUCT(BlueprintType)
struct FCashierItemView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	int32 Index = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	int32 Price = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bScanned = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bFocus = false;
};

/**
 * Scalar board the widget can bind.
 * There is no change-due field. PayValue is cleared unless bPayRevealed.
 */
USTRUCT(BlueprintType)
struct FCashierBoardView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	FName Phase;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bHasAmountDue = false;

	/** Scanned sum S. Not the change target. */
	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	int32 AmountDue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bPayRevealed = false;

	/** "？？" while Scanning. Digits only after PayShown. Empty when the board is clear. */
	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	FString PayDisplay;

	/** Meaningful only when bPayRevealed is true. */
	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	int32 PayValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bShowChangeSum = false;

	/** Player bill sum. Not the correct change. */
	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	int32 ChangeSum = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bDrawerEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bConfirmEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bClearEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	int32 BillCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	TArray<int32> BillStrip;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	TArray<FCashierItemView> Items;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	float RemainingSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bHudScoreVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	int32 HudScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bHidePlayfield = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bSettleValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	int32 ShiftScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	int32 CorrectCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	int32 OptimalCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	int32 WrongAbsSum = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	int32 WrongPenalty = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bNewBest = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	int32 BestScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	int32 FocusDenom = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bFocusConfirm = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bFocusClear = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	bool bFocusSettle = false;
};
