#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Cashier/CashierDirector.h"
#include "CashierGameMode.generated.h"

/** Spawns the cashier director. The pawn stays a spectator until a cashier map exists. */
UCLASS()
class PROJECTONE_API ACashierGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ACashierGameMode();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "Cashier")
	ACashierDirector* GetDirector() const;

private:
	UPROPERTY()
	TObjectPtr<ACashierDirector> Director;
};
