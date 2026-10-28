#include "Cashier/CashierLibrary.h"

#include "Cashier/CashierDirector.h"
#include "Cashier/CashierInputActions.h"
#include "Cashier/CashierGameMode.h"
#include "Cashier/CashierSaveGame.h"
#include "Cashier/CashierTypes.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* CashierBestSlot = TEXT("CashierShiftBest");
	constexpr int32 CashierBestUserIndex = 0;
}

ACashierDirector* UCashierLibrary::GetDirector(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	if (const ACashierGameMode* Mode = Cast<ACashierGameMode>(UGameplayStatics::GetGameMode(WorldContextObject)))
	{
		if (ACashierDirector* Director = Mode->GetDirector())
		{
			return Director;
		}
	}

	return Cast<ACashierDirector>(UGameplayStatics::GetActorOfClass(WorldContextObject, ACashierDirector::StaticClass()));
}

int32 UCashierLibrary::LoadBestScore()
{
	if (!UGameplayStatics::DoesSaveGameExist(CashierBestSlot, CashierBestUserIndex))
	{
		return 0;
	}

	const UCashierShiftSaveGame* Save = Cast<UCashierShiftSaveGame>(
		UGameplayStatics::LoadGameFromSlot(CashierBestSlot, CashierBestUserIndex));
	return Save ? Save->BestScore : 0;
}

bool UCashierLibrary::CommitBestScore(int32 Score)
{
	if (Score <= LoadBestScore())
	{
		return false;
	}

	UCashierShiftSaveGame* Save = Cast<UCashierShiftSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UCashierShiftSaveGame::StaticClass()));
	if (!Save)
	{
		return false;
	}

	Save->BestScore = Score;
	if (!UGameplayStatics::SaveGameToSlot(Save, CashierBestSlot, CashierBestUserIndex))
	{
		UE_LOG(LogCashier, Warning, TEXT("Failed to write cashier best score %d"), Score);
		return false;
	}
	return true;
}

TArray<FName> UCashierLibrary::GetLogicalActionNames()
{
	TArray<FName> Names;
	Names.Reserve(9);
	Names.Add(FCashierInputActions::ScanItem);
	Names.Add(FCashierInputActions::AddDenom1);
	Names.Add(FCashierInputActions::AddDenom5);
	Names.Add(FCashierInputActions::AddDenom10);
	Names.Add(FCashierInputActions::RemoveLastDenom);
	Names.Add(FCashierInputActions::Confirm);
	Names.Add(FCashierInputActions::FocusNext);
	Names.Add(FCashierInputActions::FocusPrev);
	Names.Add(FCashierInputActions::OpenSettle);
	return Names;
}

bool UCashierLibrary::IsLegalDenom(int32 Value)
{
	return FCashierInputActions::IsLegalDenom(Value);
}
