#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DRGameMode.generated.h"

class ADRChapter;

/**
 * Mode de jeu de Dernier Rempart : donne au joueur le personnage à la première personne et construit
 * la carte du chapitre au lancement de la partie (chapitre 1, « La Brèche », pour l'instant).
 */
UCLASS()
class DERNIERREMPART_API ADRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADRGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	ADRChapter* GetChapter() const { return Chapter; }

private:
	UPROPERTY()
	TObjectPtr<ADRChapter> Chapter;
};
