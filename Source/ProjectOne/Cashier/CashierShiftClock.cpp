#include "Cashier/CashierShiftClock.h"

#include "Cashier/CashierTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"

UCashierShiftClock::UCashierShiftClock()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCashierShiftClock::SetShiftDuration(float Seconds)
{
	ShiftDurationSeconds = Seconds > 0.f ? Seconds : 120.f;
}

void UCashierShiftClock::StartShift()
{
	StopShift();
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogCashier, Warning, TEXT("Cashier shift clock has no world"));
		return;
	}

	const float Duration = ShiftDurationSeconds > 0.f ? ShiftDurationSeconds : 120.f;
	ShiftDurationSeconds = Duration;
	bRunning = true;
	bTimeUp = false;
	ShiftEndWorldSeconds = World->GetTimeSeconds() + Duration;
	World->GetTimerManager().SetTimer(ShiftTimer, this, &UCashierShiftClock::HandleTimeUp, Duration, false);
}

void UCashierShiftClock::StopShift()
{
	bRunning = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShiftTimer);
	}
}

float UCashierShiftClock::GetRemainingSeconds() const
{
	if (bTimeUp)
	{
		return 0.f;
	}
	if (!bRunning)
	{
		return ShiftDurationSeconds;
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(ShiftEndWorldSeconds - World->GetTimeSeconds()));
}

bool UCashierShiftClock::IsRunning() const
{
	return bRunning;
}

bool UCashierShiftClock::IsTimeUp() const
{
	return bTimeUp;
}

void UCashierShiftClock::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopShift();
	Super::EndPlay(EndPlayReason);
}

void UCashierShiftClock::HandleTimeUp()
{
	if (!bRunning)
	{
		return;
	}
	bRunning = false;
	bTimeUp = true;
	OnShiftEnded.Broadcast();
}
