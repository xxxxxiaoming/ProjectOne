#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CashierShiftClock.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCashierShiftEnded);

/** One-shot shift timer. Gameplay phases stay in Lua. */
UCLASS(ClassGroup = (Cashier), meta = (BlueprintSpawnableComponent))
class PROJECTONE_API UCashierShiftClock : public UActorComponent
{
	GENERATED_BODY()

public:
	UCashierShiftClock();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cashier")
	float ShiftDurationSeconds = 120.f;

	UFUNCTION(BlueprintCallable, Category = "Cashier")
	void SetShiftDuration(float Seconds);

	UFUNCTION(BlueprintCallable, Category = "Cashier")
	void StartShift();

	UFUNCTION(BlueprintCallable, Category = "Cashier")
	void StopShift();

	UFUNCTION(BlueprintPure, Category = "Cashier")
	float GetRemainingSeconds() const;

	UFUNCTION(BlueprintPure, Category = "Cashier")
	bool IsRunning() const;

	UFUNCTION(BlueprintPure, Category = "Cashier")
	bool IsTimeUp() const;

	UPROPERTY(BlueprintAssignable, Category = "Cashier")
	FOnCashierShiftEnded OnShiftEnded;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleTimeUp();

	FTimerHandle ShiftTimer;
	double ShiftEndWorldSeconds = 0.0;
	bool bRunning = false;
	bool bTimeUp = false;
};
