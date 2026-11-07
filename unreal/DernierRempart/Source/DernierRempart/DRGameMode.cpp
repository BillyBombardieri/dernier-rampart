#include "DRGameMode.h"

#include "DRChapter.h"
#include "DRCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

ADRGameMode::ADRGameMode()
{
	DefaultPawnClass = ADRCharacter::StaticClass();
}

void ADRGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// Construit la carte avant l'arrivée du joueur, pour qu'il ait un sol sous les pieds.
	// « ?Chapitre=2 » dans les options de lancement choisit un autre chapitre (1 par défaut).
	UWorld* World = GetWorld();
	if (World && !TActorIterator<ADRChapter>(World))
	{
		Chapter = World->SpawnActor<ADRChapter>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (Chapter)
		{
			Chapter->Build(UGameplayStatics::GetIntOption(Options, TEXT("Chapitre"), 1) - 1);
		}
	}
}

void ADRGameMode::RestartPlayer(AController* NewPlayer)
{
	// Sans point de départ posé dans la carte, Unreal placerait le joueur au centre du monde :
	// on le pose devant le Cœur, comme dans la version Godot.
	if (NewPlayer && (Chapter || !Cast<APlayerStart>(FindPlayerStart(NewPlayer))))
	{
		RestartPlayerAtTransform(NewPlayer, ADRChapter::PlayerStart());
		return;
	}
	Super::RestartPlayer(NewPlayer);
}
