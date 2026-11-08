#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DRSave.generated.h"

/**
 * Progression sauvegardée sur le PC (Godot : settings.gd, user://reglages.cfg) : niveaux gagnés et
 * débloqués, record. Gagner un niveau ouvre le suivant ; gagner le 5e d'un chapitre ouvre le chapitre
 * suivant. Fichier : Saved/SaveGames/Progression.sav.
 */
UCLASS()
class DERNIERREMPART_API UDRSave : public USaveGame
{
	GENERATED_BODY()

public:
	/** Lit la sauvegarde, ou en crée une neuve s'il n'y en a pas encore. */
	static UDRSave* LoadOrCreate();
	void Write();

	/** Garde le meilleur score et la vague la plus lointaine ; vrai si c'est un nouveau record. */
	bool SubmitScore(int32 FinalScore, int32 ReachedWave);
	/** Fin d'un niveau ; vrai si une victoire vient de débloquer le niveau suivant. */
	bool FinishLevel(int32 LevelIndex, bool bWon, int32 LevelTotal);
	bool HasWon(int32 LevelIndex) const { return LevelsWon.Contains(LevelIndex); }

	UPROPERTY()
	int32 BestScore = 0;

	UPROPERTY()
	int32 BestWave = 0;

	/** Dernier niveau ouvert (0 = 1-1). */
	UPROPERTY()
	int32 UnlockedLevel = 0;

	UPROPERTY()
	TArray<int32> LevelsWon;
};
