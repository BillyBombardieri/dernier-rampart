#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "InputCoreTypes.h"
#include "DRSave.generated.h"

/** Action dont on peut changer la touche (Godot : Settings.BINDABLE). */
struct FDRBinding
{
	FName Id;
	FString Label;
	FKey Default;
};

/**
 * Progression et réglages sauvegardés sur le PC (Godot : settings.gd, user://reglages.cfg) : niveaux
 * gagnés et débloqués, record, sensibilité, volume, champ de vision, axe inversé et touches. Gagner un
 * niveau ouvre le suivant ; gagner le 5e d'un chapitre ouvre le chapitre suivant.
 * Fichier : Saved/SaveGames/Progression.sav. (La qualité graphique est gardée par Unreal.)
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

	/** Actions dont on peut changer la touche, dans l'ordre d'affichage, avec leur touche AZERTY par défaut. */
	static const TArray<FDRBinding>& Bindable();
	static FKey DefaultKey(FName Id);
	/** Touche choisie pour une action (ou celle par défaut). */
	FKey GetKey(FName Id) const;
	/** Change la touche d'une action ; une autre action qui l'utilisait reçoit l'ancienne (échange). */
	void SetKey(FName Id, const FKey& NewKey);
	void ResetKeys() { Keys.Empty(); }
	/** Nom d'une touche en français pour les textes du jeu (Z, Clic gauche, Espace...). */
	static FString KeyLabel(const FKey& Key);
	/** Nom de la touche choisie pour une action. */
	FString KeyName(FName Id) const { return KeyLabel(GetKey(Id)); }
	/** Les quatre touches de déplacement, « ZQSD » par défaut. */
	FString MoveKeysName() const;

	UPROPERTY()
	int32 BestScore = 0;

	UPROPERTY()
	int32 BestWave = 0;

	/** Dernier niveau ouvert (0 = 1-1). */
	UPROPERTY()
	int32 UnlockedLevel = 0;

	UPROPERTY()
	TArray<int32> LevelsWon;

	// Réglages (Godot : sensitivity 1, volume 0,8, fov 78°, invert_y).
	UPROPERTY()
	float Sensitivity = 1.0f;

	UPROPERTY()
	float Volume = 0.8f;

	UPROPERTY()
	float Fov = 78.0f;

	UPROPERTY()
	bool bInvertY = false;

	/** Touches changées par le joueur (les autres gardent leur touche par défaut). */
	UPROPERTY()
	TMap<FName, FKey> Keys;
};
