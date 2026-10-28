#include "Cashier/CashierGameMode.h"

#include "Cashier/CashierDirector.h"
#include "Engine/World.h"
#include "GameFramework/SpectatorPawn.h"
#include "Kismet/GameplayStatics.h"

ACashierGameMode::ACashierGameMode()
{
	bStartPlayersAsSpectators = true;
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}

void ACashierGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	Director = Cast<ACashierDirector>(UGameplayStatics::GetActorOfClass(World, ACashierDirector::StaticClass()));
	if (Director)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Director = World->SpawnActor<ACashierDirector>(ACashierDirector::StaticClass(), FTransform::Identity, Params);
}

ACashierDirector* ACashierGameMode::GetDirector() const
{
	return Director;
}
