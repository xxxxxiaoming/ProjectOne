#include "Cashier/CashierDirector.h"

namespace CashierPayGate
{
	static const FName ScanningPhase(TEXT("Scanning"));
	static const TCHAR* Placeholder = TEXT("？？");
}

ACashierDirector::ACashierDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	ShiftClock = CreateDefaultSubobject<UCashierShiftClock>(TEXT("ShiftClock"));
}

FString ACashierDirector::GetModuleName_Implementation() const
{
	return TEXT("Cashier.CashierDirector");
}

void ACashierDirector::BeginPlay()
{
	if (ShiftClock)
	{
		ShiftClock->OnShiftEnded.AddUniqueDynamic(this, &ACashierDirector::HandleShiftEnded);
	}
	Super::BeginPlay();
}

void ACashierDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ShiftClock)
	{
		ShiftClock->StopShift();
	}
	Super::EndPlay(EndPlayReason);
}

void ACashierDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	OnCashierTick(DeltaSeconds);
}

void ACashierDirector::HandleShiftEnded()
{
	OnShiftTimeUp();
}

void ACashierDirector::PushBoardView(const FCashierBoardView& InView)
{
	FCashierBoardView Clamped = InView;
	if (!Clamped.bPayRevealed)
	{
		Clamped.PayValue = 0;
		if (Clamped.Phase == CashierPayGate::ScanningPhase)
		{
			Clamped.PayDisplay = CashierPayGate::Placeholder;
		}
		else
		{
			Clamped.PayDisplay.Reset();
		}
	}
	if (!Clamped.bShowChangeSum)
	{
		Clamped.ChangeSum = 0;
	}
	if (!Clamped.bSettleValid)
	{
		Clamped.bHudScoreVisible = false;
		Clamped.HudScore = 0;
		Clamped.ShiftScore = 0;
		Clamped.CorrectCount = 0;
		Clamped.OptimalCount = 0;
		Clamped.WrongAbsSum = 0;
		Clamped.WrongPenalty = 0;
		Clamped.bNewBest = false;
		Clamped.BestScore = 0;
	}
	BoardView = Clamped;
}

void ACashierDirector::OnCashierTick_Implementation(float)
{
}

void ACashierDirector::OnShiftTimeUp_Implementation()
{
}

bool ACashierDirector::Input_ScanItem_Implementation()
{
	return false;
}

bool ACashierDirector::Input_AddDenom_1_Implementation()
{
	return false;
}

bool ACashierDirector::Input_AddDenom_5_Implementation()
{
	return false;
}

bool ACashierDirector::Input_AddDenom_10_Implementation()
{
	return false;
}

bool ACashierDirector::Input_RemoveLastDenom_Implementation()
{
	return false;
}

bool ACashierDirector::Input_ClearDenoms_Implementation()
{
	return false;
}

bool ACashierDirector::Input_Confirm_Implementation()
{
	return false;
}

bool ACashierDirector::Input_FocusNext_Implementation()
{
	return false;
}

bool ACashierDirector::Input_FocusPrev_Implementation()
{
	return false;
}

bool ACashierDirector::Input_OpenSettle_Implementation()
{
	return false;
}

bool ACashierDirector::ScanItem_Implementation(int32)
{
	return false;
}

bool ACashierDirector::AddDenom_Implementation(int32)
{
	return false;
}
