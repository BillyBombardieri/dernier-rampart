#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DRGameMode.generated.h"

class ADRChapter;
class ADRZombie;
class FJsonObject;
class USoundAttenuation;

/**
 * Mode de jeu de Dernier Rempart : donne au joueur le personnage à la première personne, construit
 * la carte du chapitre au lancement de la partie (chapitre 1, « La Brèche », pour l'instant) et
 * fait venir les zombies.
 */
UCLASS()
class DERNIERREMPART_API ADRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADRGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void Tick(float DeltaSeconds) override;

	ADRChapter* GetChapter() const;
	/** Chiffres du jeu exportés de Godot (Content/Data/jeu.json). */
	TSharedPtr<FJsonObject> GetGameData() const { return GameData; }
	/** Réglage d'atténuation des sons dans le monde (MaxDistance en cm). */
	USoundAttenuation* GetAttenuation(float MaxDistance) const;
	/** Appelé par un zombie quand il meurt. */
	void ZombieDied(ADRZombie* Zombie);

	int32 GetKills() const { return Kills; }

private:
	ADRZombie* SpawnZombie(const FString& Type, float HpScale, float SpeedScale, float DamageScale);

	UPROPERTY()
	TObjectPtr<ADRChapter> Chapter;

	UPROPERTY()
	TObjectPtr<USoundAttenuation> NearAttenuation;

	UPROPERTY()
	TObjectPtr<USoundAttenuation> FarAttenuation;

	TSharedPtr<FJsonObject> GameData;
	int32 Kills = 0;
	float TestSpawnCooldown = 6.0f;
};
