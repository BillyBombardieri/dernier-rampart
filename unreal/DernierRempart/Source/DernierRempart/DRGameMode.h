#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DRGameMode.generated.h"

/**
 * Mode de jeu de Dernier Rempart. Pour l'instant, il donne au joueur le personnage
 * à la première personne et construit un terrain d'essai si la carte est vide.
 */
UCLASS()
class DERNIERREMPART_API ADRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADRGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

private:
	bool bBuiltTestArena = false;
};
