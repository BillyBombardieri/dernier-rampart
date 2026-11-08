#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DRGameMode.generated.h"

class ADRChapter;
class ADRZombie;
class FJsonObject;
class UDRSave;
class UMaterialInterface;
class USoundAttenuation;

/** Phases d'une vague (Godot : wave_manager.gd). */
enum class EDRPhase : uint8
{
	Prep,    // préparation : on construit, Entrée lance la vague
	Assault, // les zombies arrivent
	Harvest, // vague repoussée : on ramasse la ferraille
};

/** Difficulté d'un niveau (Godot : levels.gd, exportée dans jeu.json, « niveaux »). */
struct FDRDifficulty
{
	float Hp = 1.0f;
	float Damage = 1.0f;
	float Count = 1.0f;
	float Spawn = 1.0f;
	int32 Early = 0;
	int32 Bosses = 1;
	float Points = 1.0f; // multiplicateur des insignes gagnés
};

/** Texte qui monte et s'efface au-dessus d'un zombie (« TÊTE », « BRISÉ ! »...). */
struct FDRPopup
{
	FString Text;
	FVector Location = FVector::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	float Size = 48.0f;
	float Time = 0.0f;
};

/** Onde qui s'élargit au sol puis s'efface (cri du Hurleur). */
struct FDRRing
{
	FVector Location = FVector::ZeroVector;
	FLinearColor Color = FLinearColor::White;
	float Radius = 100.0f; // cm, à la fin de l'onde
	float Duration = 0.6f;
	float Time = 0.0f;
};

/** Message affiché quelques secondes en haut de l'écran. */
struct FDRMessage
{
	FString Text;
	float Time = 0.0f;
};

/**
 * Mode de jeu de Dernier Rempart (Godot : game.gd et wave_manager.gd). Construit la carte du
 * chapitre du niveau choisi, enchaîne les vagues (préparation, assaut, récolte), compte la
 * ferraille et les points, et termine la partie : victoire après la vague 10, défaite si le Cœur
 * tombe. « ?Niveau=N » dans les options de lancement choisit le niveau (1 à 30) ; sans option,
 * c'est le menu principal (choix du niveau parmi ceux débloqués), devant la carte du dernier chapitre ouvert.
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
	/** Nombre de jeu.json (groupe « partie », « vagues »...), ou Default s'il manque. */
	double Rule(const TCHAR* Group, const TCHAR* Field, double Default) const;
	/** Réglage d'atténuation des sons dans le monde (MaxDistance en cm). */
	USoundAttenuation* GetAttenuation(float MaxDistance) const;
	/** Matériaux partagés par toutes les ferrailles (métal, reflet doré). */
	UMaterialInterface* GetScrapMaterial(bool bGlint);

	/** Entrée : lance la vague, passe la récolte, ou relance la partie quand elle est finie. */
	void SkipPhase();
	/** Échap ou P : met la partie en pause (menu) ou la reprend. */
	void TogglePause();
	/** Lance un niveau (0 = 1-1), revient au menu principal, ou quitte le jeu. */
	void PlayLevel(int32 LevelIndex);
	void OpenMainMenu();
	void QuitGame();
	void AddScrap(int32 Amount);
	/** Dépense de la ferraille, ou dit qu'il en manque et renvoie faux. */
	bool TrySpend(int32 Amount);
	void AddScore(int32 Points);
	void Say(const FString& Text);
	void Popup(const FString& Text, const FVector& Location, const FLinearColor& Color, float Size = 48.0f);
	void AddRing(const FVector& Location, float Radius, const FLinearColor& Color, float Duration);
	/** Les combos suivent la montée en puissance des zombies au fil des vagues. */
	float ComboScale() const { return 1.0f + 0.15f * FMath::Max(0, Wave - 1); }

	// Énergie des tours (Godot : 6 au départ, chaque tour allumée en consomme 1 à 3).
	int32 GetEnergyUsed() const { return EnergyUsed; }
	int32 GetEnergyCap() const;
	bool RequestEnergy(int32 Amount);
	void ReleaseEnergy(int32 Amount);

	/** Chiffres d'une tour (jeu.json, « tours ») et ordre du choix de construction. */
	TSharedPtr<FJsonObject> GetTowerData(const FString& Type) const;
	const TArray<FString>& GetBuildOrder() const { return BuildOrder; }
	/** Appelé par un zombie quand il meurt : points, ferraille au sol, fin de vague. */
	void ZombieDied(ADRZombie* Zombie);
	/** Appelé par le Cœur quand il tombe. */
	void CoreDestroyed();

	// Pour l'interface.
	int32 GetLevel() const { return Level; }
	const FString& GetLevelTitle() const { return LevelTitle; }
	const FString& GetLevelShort() const { return LevelShort; }
	int32 GetWave() const { return Wave; }
	int32 GetLastWave() const { return LastWave; }
	bool IsSiege(int32 InWave) const { return SiegeEvery > 0 && InWave % SiegeEvery == 0; }
	EDRPhase GetPhase() const { return Phase; }
	float GetPhaseTime() const { return PhaseTime; }
	float GetPhaseDuration() const { return PhaseDuration; }
	int32 GetRemaining() const { return Queue.Num() + Alive; }
	int32 GetWaveTotal() const { return WaveTotal; }
	int32 GetScrap() const { return Scrap; }
	int32 GetScore() const { return Score; }
	int32 GetKills() const { return Kills; }
	bool IsOver() const { return bOver; }
	bool IsVictory() const { return bVictory; }
	bool HasNextLevel() const { return Level + 1 < LevelCount; }
	int32 GetLevelCount() const { return LevelCount; }
	/** Menu principal (pas de partie en cours) ou menu de pause ouvert : le joueur ne bouge pas. */
	bool IsMainMenu() const { return bMenu; }
	bool IsPauseMenu() const { return bPauseMenu; }
	bool IsMenuOpen() const { return bMenu || bPauseMenu || bImplantChoice; }
	UDRSave* GetProgress() const { return Progress; }
	/** Nouveau record, ou niveau suivant débloqué, à la fin de la partie. */
	bool IsNewRecord() const { return bNewRecord; }
	bool HasUnlockedNext() const { return bUnlockedNext; }
	/** Nom, lieu et description d'un chapitre (chapitres.json), pour le choix du niveau. */
	int32 GetChapterCount() const;
	FString GetChapterText(int32 Index, const TCHAR* Field) const;
	const TArray<FDRMessage>& GetMessages() const { return Messages; }
	const TArray<FDRPopup>& GetPopups() const { return Popups; }
	const TArray<FDRRing>& GetRings() const { return Rings; }

	/**
	 * Améliorations permanentes (Godot : upgrades.gd, chiffres de jeu.json « ameliorations »),
	 * achetées avec les insignes dans le menu principal. Elles comptent dès la partie suivante.
	 */
	static const TArray<FName>& GetUpgradeIds();
	int32 GetUpgradeRank(FName Id) const;
	int32 GetUpgradeMaxRank(FName Id) const;
	/** Prix du prochain rang en insignes, -1 si l'amélioration est au maximum. */
	int32 GetUpgradeCost(FName Id) const;
	/** Bonus total (rang x bonus par rang), et multiplicateur 1 + bonus pour les pourcentages. */
	float UpgradeBonus(FName Id) const;
	float UpgradeMult(FName Id) const { return 1.0f + UpgradeBonus(Id); }
	/** « name » ou « desc » d'une amélioration. */
	FString GetUpgradeText(FName Id, const TCHAR* Field) const;
	bool BuyUpgrade(FName Id);
	/** Rend tous les insignes dépensés et remet les améliorations à zéro. */
	void RefundUpgrades();
	/** Insignes gagnés à la fin de cette partie. */
	int32 GetEarnedInsignes() const { return EarnedInsignes; }

	/**
	 * Implants (Godot : game.gd, IMPLANTS, chiffres de jeu.json « implants ») : après une Nuit de
	 * siège repoussée, on en choisit un parmi trois, avec un bonus et un malus, pour toute la partie.
	 */
	static const TArray<FName>& GetImplantIds();
	bool HasImplant(FName Id) const { return Implants.Contains(Id); }
	const TArray<FName>& GetImplants() const { return Implants; }
	/** Choix d'implant en cours : la partie est figée jusqu'au choix. */
	bool IsChoosingImplant() const { return bImplantChoice; }
	const TArray<FName>& GetImplantOptions() const { return ImplantOptions; }
	/** « name », « plus » ou « minus » d'un implant. */
	FString GetImplantText(FName Id, const TCHAR* Field) const;
	void ChooseImplant(FName Id);

private:
	TSharedPtr<FJsonObject> GetUpgradeData(FName Id) const;
	void LoadLevel(const FString& Options);
	void EnterPhase(EDRPhase NewPhase, float Duration);
	void BuildQueue();
	void SpawnZombie(const FString& Type);
	void WaveCleared();
	void EndGame(bool bWon);
	void PlaySfx2D(const FString& Name, float Decibels);

	UPROPERTY()
	TObjectPtr<ADRChapter> Chapter;

	UPROPERTY()
	TObjectPtr<USoundAttenuation> NearAttenuation;

	UPROPERTY()
	TObjectPtr<USoundAttenuation> FarAttenuation;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ScrapMetal;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ScrapGlint;

	UPROPERTY()
	TObjectPtr<UDRSave> Progress;

	TSharedPtr<FJsonObject> GameData;
	TSharedPtr<FJsonObject> ChapterData;

	// Niveau joué.
	int32 Level = 0;
	int32 LevelCount = 1;
	int32 ChapterIndex = 0;
	FString LevelTitle;
	FString LevelShort;
	FDRDifficulty Difficulty;

	// Vagues (chiffres de jeu.json, « vagues » et « partie »).
	int32 LastWave = 10;
	int32 SiegeEvery = 5;
	int32 Wave = 1;
	EDRPhase Phase = EDRPhase::Prep;
	float PhaseTime = 0.0f;
	float PhaseDuration = 1.0f;
	TArray<FString> Queue;
	TSet<FString> Seen;
	int32 WaveTotal = 1;
	int32 Alive = 0;
	float SpawnCooldown = 0.0f;
	int32 Bonus = 0;

	// Ressources et fin de partie.
	int32 Scrap = 55;
	int32 Score = 0;
	int32 Kills = 0;
	bool bOver = false;
	bool bVictory = false;
	bool bStarted = false;
	bool bMenu = false;
	bool bPauseMenu = false;
	bool bNewRecord = false;
	bool bUnlockedNext = false;
	int32 EarnedInsignes = 0;
	bool bImplantChoice = false;
	TArray<FName> Implants;
	TArray<FName> ImplantOptions;
	int32 EnergyUsed = 0;
	TArray<FString> BuildOrder;
	TArray<FDRMessage> Messages;
	TArray<FDRPopup> Popups;
	TArray<FDRRing> Rings;
};
