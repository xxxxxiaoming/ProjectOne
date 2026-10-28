#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UnLuaInterface.h"
#include "Cashier/CashierTypes.h"
#include "Cashier/CashierShiftClock.h"
#include "CashierDirector.generated.h"

/**
 * Thin actor for cashier mode A. Gameplay lives in Content/Script/Cashier/CashierDirector.lua.
 * Widget blueprints call the Input_* / ScanItem / AddDenom events. Bind BoardView for scalars.
 */
UCLASS()
class PROJECTONE_API ACashierDirector : public AActor, public IUnLuaInterface
{
	GENERATED_BODY()

public:
	ACashierDirector();

	virtual FString GetModuleName_Implementation() const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cashier")
	TObjectPtr<UCashierShiftClock> ShiftClock;

	UPROPERTY(BlueprintReadOnly, Category = "Cashier")
	FCashierBoardView BoardView;

	/** Copies the Lua snapshot and clears Pay unless the phase has revealed it. */
	UFUNCTION(BlueprintCallable, Category = "Cashier")
	void PushBoardView(const FCashierBoardView& InView);

	UFUNCTION(BlueprintNativeEvent, Category = "Cashier")
	void OnCashierTick(float DeltaSeconds);

	UFUNCTION(BlueprintNativeEvent, Category = "Cashier")
	void OnShiftTimeUp();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Cashier|Input")
	bool Input_ScanItem();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Cashier|Input")
	bool Input_AddDenom_1();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Cashier|Input")
	bool Input_AddDenom_5();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Cashier|Input")
	bool Input_AddDenom_10();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Cashier|Input")
	bool Input_RemoveLastDenom();

	/** Widget clear button. Not one of the locked Enhanced Input action names. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Cashier|Input")
	bool Input_ClearDenoms();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Cashier|Input")
	bool Input_Confirm();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Cashier|Input")
	bool Input_FocusNext();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Cashier|Input")
	bool Input_FocusPrev();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Cashier|Input")
	bool Input_OpenSettle();

	/** Direct card click. ItemIndex is 1-based. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Cashier")
	bool ScanItem(int32 ItemIndex);

	/** Direct denom click. Value must be 1, 5, or 10. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Cashier")
	bool AddDenom(int32 Denom);

private:
	UFUNCTION()
	void HandleShiftEnded();
};
