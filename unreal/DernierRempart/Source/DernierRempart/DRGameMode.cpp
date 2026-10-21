#include "DRGameMode.h"

#include "DRCharacter.h"
#include "DRTestArena.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

ADRGameMode::ADRGameMode()
{
	DefaultPawnClass = ADRCharacter::StaticClass();
}

void ADRGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// Construit le terrain d'essai avant l'arrivée du joueur, pour qu'il ait un sol sous les pieds.
	UWorld* World = GetWorld();
	if (World && !TActorIterator<ADRTestArena>(World))
	{
		bBuiltTestArena = World->SpawnActor<ADRTestArena>(FVector::ZeroVector, FRotator::ZeroRotator) != nullptr;
	}
}

void ADRGameMode::RestartPlayer(AController* NewPlayer)
{
	// Sur le terrain d'essai (ou sans point de départ dans la carte), Unreal placerait le joueur au
	// centre du monde, dans le mur : on le pose au bord du terrain, tourné vers les blocs.
	if (NewPlayer && (bBuiltTestArena || !Cast<APlayerStart>(FindPlayerStart(NewPlayer))))
	{
		RestartPlayerAtTransform(NewPlayer, FTransform(FRotator(0.0, 90.0, 0.0), FVector(0.0, -1500.0, 120.0)));
		return;
	}
	Super::RestartPlayer(NewPlayer);
}
