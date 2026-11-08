#include "DRGameMode.h"

#include "DRChapter.h"
#include "DRCharacter.h"
#include "DRData.h"
#include "DRGadgets.h"
#include "DRHUD.h"
#include "DRLook.h"
#include "DRSave.h"
#include "DRScrap.h"
#include "DRStructures.h"
#include "DRTower.h"
#include "DRZombie.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace DRGameModeSounds
{
	// Comme dans Godot (unit_size 6, max_distance 80 ou 40 m) : plein volume tout près, puis de
	// plus en plus faible jusqu'à la distance maximale.
	void Configure(USoundAttenuation* Attenuation, float MaxDistance)
	{
		FSoundAttenuationSettings& Settings = Attenuation->Attenuation;
		Settings.bAttenuate = true;
		Settings.bSpatialize = true;
		Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		Settings.AttenuationShape = EAttenuationShape::Sphere;
		Settings.AttenuationShapeExtents = FVector(600.0, 0.0, 0.0);
		Settings.FalloffDistance = MaxDistance - 600.0f;
	}
}

namespace DRGameModeGadgets
{
	// Chiffres d'un gadget dans jeu.json (« gadgets »).
	TSharedPtr<FJsonObject> Find(const TSharedPtr<FJsonObject>& Data, FName Id)
	{
		const TSharedPtr<FJsonObject>* List = nullptr;
		const TSharedPtr<FJsonObject>* Entry = nullptr;
		if (Data.IsValid() && Data->TryGetObjectField(TEXT("gadgets"), List) && (*List)->TryGetObjectField(Id.ToString(), Entry))
		{
			return *Entry;
		}
		return TSharedPtr<FJsonObject>();
	}
}

ADRGameMode::ADRGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = ADRCharacter::StaticClass();
	HUDClass = ADRHUD::StaticClass();
	NearAttenuation = CreateDefaultSubobject<USoundAttenuation>(TEXT("SonsProches"));
	FarAttenuation = CreateDefaultSubobject<USoundAttenuation>(TEXT("SonsLointains"));
	DRGameModeSounds::Configure(NearAttenuation, 4000.0f);
	DRGameModeSounds::Configure(FarAttenuation, 8000.0f);
}

void ADRGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	GameData = DRData::Load(TEXT("jeu.json"));
	ChapterData = DRData::Load(TEXT("chapitres.json"));
	Progress = UDRSave::LoadOrCreate();
	DRLook::SetMasterVolume(Progress->Volume);
	LoadLevel(Options);
	// Tutoriel de la première partie (pas devant le menu principal).
	if (!bMenu && !Progress->bTutorialDone)
	{
		TutorialStep = 0;
		StartTutorialStep();
	}
	LastWave = (int32)Rule(TEXT("partie"), TEXT("last_wave"), 10.0);
	SiegeEvery = (int32)Rule(TEXT("vagues"), TEXT("siege_every"), 5.0);
	// Gadgets de départ (Godot : Game.gadget_slots).
	GadgetSlots = { FName(TEXT("barricade")), FName(TEXT("leurre")) };
	// Amélioration « Réserves » : plus de ferraille au départ.
	Scrap = (int32)Rule(TEXT("partie"), TEXT("start_scrap"), 55.0) + (int32)UpgradeBonus(TEXT("reserves"));
	// Ordre des tours dans le choix de construction (Godot : Tower.BUILD_ORDER).
	const TSharedPtr<FJsonObject>* TowerRules = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Order = nullptr;
	if (GameData.IsValid() && GameData->TryGetObjectField(TEXT("tours_regles"), TowerRules) && (*TowerRules)->TryGetArrayField(TEXT("build_order"), Order))
	{
		for (const TSharedPtr<FJsonValue>& Entry : *Order)
		{
			BuildOrder.Add(Entry->AsString());
		}
	}
	if (BuildOrder.Num() == 0)
	{
		BuildOrder = { TEXT("gun"), TEXT("cryo"), TEXT("flame"), TEXT("arc"), TEXT("mortar"), TEXT("beacon") };
	}
	// Construit la carte avant l'arrivée du joueur, pour qu'il ait un sol sous les pieds.
	UWorld* World = GetWorld();
	if (World && !TActorIterator<ADRChapter>(World))
	{
		Chapter = World->SpawnActor<ADRChapter>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (Chapter)
		{
			Chapter->Build(ChapterIndex);
		}
	}
}

void ADRGameMode::LoadLevel(const FString& Options)
{
	// « ?Niveau=N » (1 à 30) ; « ?Chapitre=N » reste accepté et prend le premier niveau du chapitre.
	const TArray<TSharedPtr<FJsonValue>>* Levels = nullptr;
	if (GameData.IsValid() && GameData->TryGetArrayField(TEXT("niveaux"), Levels) && Levels->Num() > 0)
	{
		LevelCount = Levels->Num();
	}
	const int32 AskedLevel = UGameplayStatics::GetIntOption(Options, TEXT("Niveau"), 0);
	const int32 AskedChapter = UGameplayStatics::GetIntOption(Options, TEXT("Chapitre"), 0);
	// Sans niveau demandé : menu principal, devant la carte du dernier niveau débloqué.
	bMenu = AskedLevel <= 0 && AskedChapter <= 0;
	Level = AskedLevel > 0 ? AskedLevel - 1 : (AskedChapter > 0 ? (AskedChapter - 1) * 5 : (Progress ? Progress->UnlockedLevel : 0));
	Level = FMath::Clamp(Level, 0, LevelCount - 1);
	ChapterIndex = Level / 5;
	LevelShort = FString::Printf(TEXT("%d-%d"), ChapterIndex + 1, Level % 5 + 1);
	LevelTitle = LevelShort;
	if (Levels && Levels->IsValidIndex(Level))
	{
		const TSharedPtr<FJsonObject> Entry = (*Levels)[Level]->AsObject();
		if (Entry.IsValid())
		{
			ChapterIndex = (int32)DRData::Number(Entry, TEXT("chapitre"), ChapterIndex);
			Entry->TryGetStringField(TEXT("titre"), LevelTitle);
			Entry->TryGetStringField(TEXT("court"), LevelShort);
			const TSharedPtr<FJsonObject>* Diff = nullptr;
			if (Entry->TryGetObjectField(TEXT("difficulte"), Diff))
			{
				Difficulty.Hp = (float)DRData::Number(*Diff, TEXT("hp"), 1.0);
				Difficulty.Damage = (float)DRData::Number(*Diff, TEXT("damage"), 1.0);
				Difficulty.Count = (float)DRData::Number(*Diff, TEXT("count"), 1.0);
				Difficulty.Spawn = (float)DRData::Number(*Diff, TEXT("spawn"), 1.0);
				Difficulty.Early = (int32)DRData::Number(*Diff, TEXT("early"), 0.0);
				Difficulty.Bosses = (int32)DRData::Number(*Diff, TEXT("bosses"), 1.0);
				Difficulty.Points = (float)DRData::Number(*Diff, TEXT("points"), 1.0);
			}
		}
	}
}

double ADRGameMode::Rule(const TCHAR* Group, const TCHAR* Field, double Default) const
{
	const TSharedPtr<FJsonObject>* Values = nullptr;
	if (GameData.IsValid() && GameData->TryGetObjectField(Group, Values))
	{
		return DRData::Number(*Values, Field, Default);
	}
	return Default;
}

ADRChapter* ADRGameMode::GetChapter() const
{
	return Chapter;
}

USoundAttenuation* ADRGameMode::GetAttenuation(float MaxDistance) const
{
	return MaxDistance <= 5000.0f ? NearAttenuation : FarAttenuation;
}

UMaterialInterface* ADRGameMode::GetScrapMaterial(bool bGlint)
{
	if (!ScrapMetal)
	{
		ScrapMetal = DRLook::Textured(this, TEXT("metal"), 3.0f, FLinearColor(0.9f, 0.85f, 0.8f));
		ScrapGlint = DRLook::Textured(this, TEXT("metal"), 3.0f, FLinearColor(1.0f, 0.7f, 0.3f), 0.8f);
	}
	return bGlint ? ScrapGlint : ScrapMetal;
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

// ---------------------------------------------------------------- messages, ressources

void ADRGameMode::Say(const FString& Text)
{
	FDRMessage Message;
	Message.Text = Text;
	Message.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Messages.Add(Message);
	// Trois messages au plus à l'écran, comme dans Godot.
	while (Messages.Num() > 3)
	{
		Messages.RemoveAt(0);
	}
}

void ADRGameMode::AddScrap(int32 Amount)
{
	Scrap += Amount;
}

bool ADRGameMode::TrySpend(int32 Amount)
{
	if (Scrap < Amount)
	{
		Say(FString::Printf(TEXT("Pas assez de ferraille (%d n\u00E9cessaires)"), Amount));
		return false;
	}
	Scrap -= Amount;
	return true;
}

int32 ADRGameMode::GetEnergyCap() const
{
	// Générateur de l'établi : +2 par niveau. Implant Cœur de batterie : +2 ; « Dynamo » : +1 par rang.
	return (int32)Rule(TEXT("partie"), TEXT("energy_base"), 6.0) + 2 * Generator + (HasImplant(TEXT("batterie")) ? 2 : 0) + (int32)UpgradeBonus(TEXT("dynamo"));
}

bool ADRGameMode::RequestEnergy(int32 Amount)
{
	if (EnergyUsed + Amount > GetEnergyCap())
	{
		return false;
	}
	EnergyUsed += Amount;
	return true;
}

void ADRGameMode::ReleaseEnergy(int32 Amount)
{
	EnergyUsed = FMath::Max(0, EnergyUsed - Amount);
}

TSharedPtr<FJsonObject> ADRGameMode::GetTowerData(const FString& Type) const
{
	const TSharedPtr<FJsonObject>* Towers = nullptr;
	const TSharedPtr<FJsonObject>* Stats = nullptr;
	if (GameData.IsValid() && GameData->TryGetObjectField(TEXT("tours"), Towers) && (*Towers)->TryGetObjectField(Type, Stats))
	{
		return *Stats;
	}
	return TSharedPtr<FJsonObject>();
}

void ADRGameMode::Popup(const FString& Text, const FVector& Location, const FLinearColor& Color, float Size)
{
	FDRPopup Entry;
	Entry.Text = Text;
	Entry.Location = Location;
	Entry.Color = Color;
	Entry.Size = Size;
	Entry.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	// On oublie ceux qui ont fini de monter (0,9 s), et on en garde 24 au plus.
	Popups.RemoveAll([Now = Entry.Time](const FDRPopup& Old) { return Now - Old.Time > 0.9f; });
	if (Popups.Num() >= 24)
	{
		Popups.RemoveAt(0);
	}
	Popups.Add(Entry);
}

void ADRGameMode::AddRing(const FVector& Location, float Radius, const FLinearColor& Color, float Duration)
{
	FDRRing Entry;
	Entry.Location = Location;
	Entry.Radius = Radius;
	Entry.Color = Color;
	Entry.Duration = FMath::Max(0.05f, Duration);
	Entry.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Rings.RemoveAll([Now = Entry.Time](const FDRRing& Old) { return Now - Old.Time > Old.Duration; });
	if (Rings.Num() >= 16)
	{
		Rings.RemoveAt(0);
	}
	Rings.Add(Entry);
}

void ADRGameMode::AddScore(int32 Points)
{
	if (!bOver)
	{
		Score += Points;
	}
}

void ADRGameMode::PlaySfx2D(const FString& Name, float Decibels)
{
	if (USoundBase* Sound = DRLook::Sound(Name))
	{
		UGameplayStatics::PlaySound2D(this, Sound, DRLook::Volume(Decibels));
	}
}

// ---------------------------------------------------------------- vagues

void ADRGameMode::EnterPhase(EDRPhase NewPhase, float Duration)
{
	Phase = NewPhase;
	PhaseTime = Duration;
	PhaseDuration = FMath::Max(1.0f, Duration);
	switch (NewPhase)
	{
	case EDRPhase::Prep:
	{
		const FString Label = IsSiege(Wave) ? FString(TEXT("NUIT DE SI\u00C8GE")) : FString::Printf(TEXT("Vague %d"), Wave);
		Say(FString::Printf(TEXT("%s dans %d s. Construis tes d\u00E9fenses (%s pour lancer)."), *Label, FMath::RoundToInt(Duration),
			Progress ? *Progress->KeyName(TEXT("vague")) : TEXT("Entr\u00E9e")));
		break;
	}
	case EDRPhase::Assault:
		PlaySfx2D(TEXT("siren"), -8.0f);
		BuildQueue();
		WaveTotal = FMath::Max(1, Queue.Num());
		SpawnCooldown = 0.0f;
		Say(IsSiege(Wave) ? FString(TEXT("NUIT DE SI\u00C8GE : un boss arrive !")) : FString::Printf(TEXT("Vague %d : ils arrivent !"), Wave));
		break;
	case EDRPhase::Harvest:
		Say(FString::Printf(TEXT("Vague repouss\u00E9e : sant\u00E9 restaur\u00E9e et +%d ferraille de prime. Ramasse le reste avant qu'il disparaisse."), Bonus));
		break;
	}
}

void ADRGameMode::BuildQueue()
{
	// Même composition que _build_queue dans wave_manager.gd.
	Queue.Reset();
	const int32 Count = FMath::RoundToInt((8 + Wave * 4) * Difficulty.Count);
	// Dans les niveaux plus durs, les zombies spéciaux arrivent quelques vagues plus tôt.
	const int32 W = Wave + Difficulty.Early;
	const float RunnerChance = FMath::Min(0.15f + 0.03f * W, 0.4f);
	const float BruteChance = W < 2 ? 0.0f : FMath::Min(0.06f + 0.02f * W, 0.22f);
	const float SpitterChance = W < 3 ? 0.0f : FMath::Min(0.06f + 0.015f * (W - 3), 0.14f);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Roll = FMath::FRand();
		FString Type = TEXT("rodeur");
		if (Roll < RunnerChance)
		{
			Type = TEXT("coureur");
		}
		else if (Roll < RunnerChance + SpitterChance)
		{
			Type = TEXT("cracheur");
		}
		else if (Roll > 1.0f - BruteChance)
		{
			Type = TEXT("brute");
		}
		Queue.Add(Type);
	}
	// Hurleurs (dès la vague 4) au milieu de la horde ; fouisseurs (dès la vague 5).
	if (W >= 4)
	{
		for (int32 Index = 0; Index < 1 + (W - 4) / 3; ++Index)
		{
			Queue.Insert(FString(TEXT("hurleur")), FMath::RandRange(Count / 4, Count * 3 / 4));
		}
	}
	if (W >= 5)
	{
		for (int32 Index = 0; Index < 1 + (W - 5) / 2; ++Index)
		{
			Queue.Insert(FString(TEXT("fouisseur")), FMath::RandRange(Count / 5, Count * 4 / 5));
		}
	}
	// Fin de vague : une ruée de coureurs pour mettre la pression.
	if (W >= 4)
	{
		for (int32 Index = 0; Index < W - 2; ++Index)
		{
			Queue.Add(TEXT("coureur"));
		}
	}
	if (IsSiege(Wave))
	{
		const int32 Bosses = Wave >= LastWave ? Difficulty.Bosses : 1;
		for (int32 Index = 0; Index < Bosses; ++Index)
		{
			Queue.Insert(FString(TEXT("boss")), FMath::Min(Queue.Num(), Count / 2 + Index * Count / 4));
		}
	}
}

void ADRGameMode::SpawnZombie(const FString& Type)
{
	if (!Chapter || Chapter->GetPath().Num() < 2)
	{
		return;
	}
	// Au portail, un peu dispersés (Godot : ±2,5 m sur la largeur, ±1,5 m en profondeur).
	const FVector Portal = Chapter->GetPath()[0];
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FVector Spot = Portal + FVector(FMath::FRandRange(-150.0, 150.0), FMath::FRandRange(-250.0, 250.0), 300.0);
	const FRotator Facing(0.0, (Chapter->GetPath()[1] - Portal).Rotation().Yaw, 0.0);
	ADRZombie* Zombie = GetWorld()->SpawnActor<ADRZombie>(ADRZombie::StaticClass(), Spot, Facing, Params);
	if (!Zombie)
	{
		return;
	}
	// Montée en puissance de vague en vague, multipliée par la difficulté du niveau.
	const float Step = (float)(Wave - 1);
	Zombie->Setup(Type,
		(1.0f + (float)Rule(TEXT("vagues"), TEXT("hp_per_wave"), 0.2) * Step) * Difficulty.Hp,
		1.0f + (float)Rule(TEXT("vagues"), TEXT("speed_per_wave"), 0.02) * Step,
		(1.0f + (float)Rule(TEXT("vagues"), TEXT("damage_per_wave"), 0.08) * Step) * Difficulty.Damage);
	Zombie->SetActorLocation(FVector(Spot.X, Spot.Y, Portal.Z + Zombie->GetSimpleCollisionHalfHeight() + 5.0));
	++Alive;
	// Premier zombie d'un nouveau type : un message explique ce qu'il fait.
	const TSharedPtr<FJsonObject>* Waves = nullptr;
	const TSharedPtr<FJsonObject>* Intro = nullptr;
	FString Text;
	if (!Seen.Contains(Type) && GameData.IsValid() && GameData->TryGetObjectField(TEXT("vagues"), Waves)
		&& (*Waves)->TryGetObjectField(TEXT("intro"), Intro) && (*Intro)->TryGetStringField(Type, Text))
	{
		Seen.Add(Type);
		Say(Text);
	}
}

void ADRGameMode::ZombieDied(ADRZombie* Zombie)
{
	Alive = FMath::Max(0, Alive - 1);
	++Kills;
	if (!Zombie)
	{
		return;
	}
	// Points et ferraille du type de zombie (jeu.json, « zombies »).
	const TSharedPtr<FJsonObject>* Types = nullptr;
	const TSharedPtr<FJsonObject>* Stats = nullptr;
	int32 Value = 3;
	if (GameData.IsValid() && GameData->TryGetObjectField(TEXT("zombies"), Types) && (*Types)->TryGetObjectField(Zombie->GetZombieType(), Stats))
	{
		AddScore((int32)DRData::Number(*Stats, TEXT("points"), 10.0));
		Value = (int32)DRData::Number(*Stats, TEXT("scrap"), 3.0);
	}
	// La ferraille tombe en quelques tas autour du corps.
	const int32 Pieces = FMath::Clamp(Value / 3, 1, 8);
	const int32 PerPiece = FMath::CeilToInt((float)Value / Pieces);
	for (int32 Index = 0; Index < Pieces; ++Index)
	{
		const FVector Spot = Zombie->GetActorLocation() + FVector(FMath::FRandRange(-100.0, 100.0), FMath::FRandRange(-100.0, 100.0), 0.0);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ADRScrap* Pile = GetWorld()->SpawnActor<ADRScrap>(ADRScrap::StaticClass(), Spot, FRotator::ZeroRotator, Params))
		{
			Pile->Setup(PerPiece);
		}
	}
}

void ADRGameMode::WaveCleared()
{
	AddScore((int32)Rule(TEXT("partie"), TEXT("wave_points"), 50.0) * Wave);
	if (Wave >= LastWave)
	{
		EndGame(true);
		return;
	}
	// Amélioration « Récupération » : prime de vague plus grosse.
	Bonus = FMath::RoundToInt((float)(Rule(TEXT("partie"), TEXT("wave_scrap_base"), 10.0) + Rule(TEXT("partie"), TEXT("wave_scrap_per_wave"), 5.0) * Wave)
		* UpgradeMult(TEXT("recuperation")));
	AddScrap(Bonus);
	// Entre deux vagues, le joueur récupère toute sa santé.
	if (ADRCharacter* Player = Cast<ADRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->HealFull();
	}
	// Après une Nuit de siège : choix d'un implant parmi trois pas encore pris, partie figée.
	if (IsSiege(Wave))
	{
		ImplantOptions.Reset();
		for (const FName& Id : GetImplantIds())
		{
			if (!Implants.Contains(Id))
			{
				ImplantOptions.Add(Id);
			}
		}
		for (int32 Index = ImplantOptions.Num() - 1; Index > 0; --Index)
		{
			ImplantOptions.Swap(Index, FMath::RandRange(0, Index));
		}
		if (ImplantOptions.Num() > 3)
		{
			ImplantOptions.SetNum(3);
		}
		if (ImplantOptions.Num() > 0)
		{
			bImplantChoice = true;
			UGameplayStatics::SetGamePaused(this, true);
			return;
		}
	}
	EnterPhase(EDRPhase::Harvest, (float)Rule(TEXT("vagues"), TEXT("harvest_time"), 12.0));
}

void ADRGameMode::OpenWorkbench()
{
	if (bMenu || bOver || bPauseMenu || bImplantChoice || bWorkbenchOpen || Phase == EDRPhase::Assault)
	{
		return;
	}
	bWorkbenchOpen = true;
	bTutorialBench = true;
	UGameplayStatics::SetGamePaused(this, true);
	PlaySfx2D(TEXT("ui_click"), -6.0f);
}

void ADRGameMode::CloseWorkbench()
{
	if (!bWorkbenchOpen)
	{
		return;
	}
	bWorkbenchOpen = false;
	UGameplayStatics::SetGamePaused(this, false);
}

const TArray<FName>& ADRGameMode::GetAmmoIds()
{
	static const TArray<FName> Ids = { TEXT("standard"), TEXT("incendiaire"), TEXT("perforante"), TEXT("electrique") };
	return Ids;
}

FString ADRGameMode::GetAmmoText(FName Ammo, const TCHAR* Field) const
{
	FString Text = Ammo.ToString();
	const TSharedPtr<FJsonObject>* List = nullptr;
	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (GameData.IsValid() && GameData->TryGetObjectField(TEXT("munitions"), List) && (*List)->TryGetObjectField(Ammo.ToString(), Entry))
	{
		(*Entry)->TryGetStringField(Field, Text);
	}
	return Text;
}

int32 ADRGameMode::GetAmmoCost(FName Ammo) const
{
	const TSharedPtr<FJsonObject>* List = nullptr;
	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (GameData.IsValid() && GameData->TryGetObjectField(TEXT("munitions"), List) && (*List)->TryGetObjectField(Ammo.ToString(), Entry))
	{
		return (int32)DRData::Number(*Entry, TEXT("cost"), 60.0);
	}
	return 60;
}

FDRWeaponMods ADRGameMode::GetWeaponMods(const FString& WeaponId) const
{
	const FDRWeaponMods* Found = WeaponMods.Find(WeaponId);
	return Found ? *Found : FDRWeaponMods();
}

int32 ADRGameMode::GetBenchMax(const TCHAR* Kind) const
{
	const TSharedPtr<FJsonObject>* Bench = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Costs = nullptr;
	if (GameData.IsValid() && GameData->TryGetObjectField(TEXT("etabli"), Bench) && (*Bench)->TryGetArrayField(Kind, Costs))
	{
		return Costs->Num();
	}
	return 0;
}

int32 ADRGameMode::GetBenchCost(const TCHAR* Kind, int32 CurrentStep) const
{
	const TSharedPtr<FJsonObject>* Bench = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Costs = nullptr;
	if (GameData.IsValid() && GameData->TryGetObjectField(TEXT("etabli"), Bench) && (*Bench)->TryGetArrayField(Kind, Costs) && Costs->IsValidIndex(CurrentStep))
	{
		return (int32)(*Costs)[CurrentStep]->AsNumber();
	}
	return -1;
}

bool ADRGameMode::BuyBarrel(const FString& WeaponId)
{
	FDRWeaponMods& Mods = WeaponMods.FindOrAdd(WeaponId);
	const int32 Cost = GetBenchCost(TEXT("canon"), Mods.Barrel);
	if (Cost < 0 || !TrySpend(Cost))
	{
		return false;
	}
	++Mods.Barrel;
	return true;
}

bool ADRGameMode::BuyMag(const FString& WeaponId)
{
	FDRWeaponMods& Mods = WeaponMods.FindOrAdd(WeaponId);
	const int32 Cost = GetBenchCost(TEXT("chargeur"), Mods.Mag);
	if (Cost < 0 || !TrySpend(Cost))
	{
		return false;
	}
	++Mods.Mag;
	// Le plus grand chargeur arrive plein.
	if (ADRCharacter* Player = Cast<ADRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->Refill(WeaponId);
	}
	return true;
}

bool ADRGameMode::ChooseAmmo(const FString& WeaponId, FName Ammo)
{
	FDRWeaponMods& Mods = WeaponMods.FindOrAdd(WeaponId);
	if (!Mods.Owns(Ammo))
	{
		if (!TrySpend(GetAmmoCost(Ammo)))
		{
			return false;
		}
		Mods.Owned.Add(Ammo);
	}
	Mods.Ammo = Ammo;
	return true;
}

bool ADRGameMode::BuyGenerator()
{
	const int32 Cost = GetBenchCost(TEXT("generateur"), Generator);
	if (Cost < 0 || !TrySpend(Cost))
	{
		return false;
	}
	++Generator;
	Say(FString::Printf(TEXT("G\u00E9n\u00E9rateur am\u00E9lior\u00E9 : %d \u00E9nergie disponible."), GetEnergyCap()));
	return true;
}

// ---------------------------------------------------------------- gadgets

const TArray<FName>& ADRGameMode::GetGadgetIds()
{
	static const TArray<FName> Ids = { TEXT("barricade"), TEXT("leurre"), TEXT("drone") };
	return Ids;
}

void ADRGameMode::SetGadget(int32 Slot, FName Id)
{
	if (!GadgetSlots.IsValidIndex(Slot) || !GetGadgetIds().Contains(Id))
	{
		return;
	}
	const int32 Other = 1 - Slot;
	if (GadgetSlots.IsValidIndex(Other) && GadgetSlots[Other] == Id)
	{
		GadgetSlots[Other] = GadgetSlots[Slot];
	}
	GadgetSlots[Slot] = Id;
}

float ADRGameMode::GetGadgetCooldown(FName Id) const
{
	const float* Left = GadgetCooldowns.Find(Id);
	return Left ? *Left : 0.0f;
}

float ADRGameMode::GetGadgetRecharge(FName Id) const
{
	return (float)DRData::Number(DRGameModeGadgets::Find(GameData, Id), TEXT("cooldown"), 30.0);
}

FString ADRGameMode::GetGadgetText(FName Id, const TCHAR* Field) const
{
	FString Text = Id.ToString();
	const TSharedPtr<FJsonObject> Entry = DRGameModeGadgets::Find(GameData, Id);
	if (Entry.IsValid())
	{
		Entry->TryGetStringField(Field, Text);
	}
	return Text;
}

bool ADRGameMode::UseGadget(int32 Slot, ADRCharacter* User)
{
	const FName Id = GetGadgetSlot(Slot);
	if (Id.IsNone() || !User || bOver || IsMenuOpen())
	{
		return false;
	}
	const float Left = GetGadgetCooldown(Id);
	if (Left > 0.0f)
	{
		Say(FString::Printf(TEXT("%s : encore %d s de recharge"), *GetGadgetText(Id, TEXT("name")), FMath::CeilToInt(Left)));
		return false;
	}
	bool bUsed = false;
	if (Id == FName(TEXT("barricade")))
	{
		bUsed = PlaceBarricade(User);
	}
	else if (Id == FName(TEXT("leurre")))
	{
		bUsed = ThrowDecoy(User);
	}
	else if (Id == FName(TEXT("drone")))
	{
		bUsed = LaunchDrone(User);
	}
	if (bUsed)
	{
		GadgetCooldowns.Add(Id, GetGadgetRecharge(Id));
	}
	return bUsed;
}

bool ADRGameMode::PlaceBarricade(ADRCharacter* User)
{
	// En travers du chemin, sur le point du chemin le plus proche de l'endroit 3 m devant soi.
	if (!Chapter)
	{
		return false;
	}
	FVector Forward = User->GetAimDirection();
	Forward.Z = 0.0;
	const FVector Spot = User->GetActorLocation() + Forward.GetSafeNormal() * 300.0;
	const TArray<FVector>& Points = Chapter->GetPath();
	double Best = TNumericLimits<double>::Max();
	int32 BestSegment = -1;
	FVector BestPoint = FVector::ZeroVector;
	for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
	{
		const FVector2D A(Points[Index].X, Points[Index].Y);
		const FVector2D B(Points[Index + 1].X, Points[Index + 1].Y);
		const FVector2D P(Spot.X, Spot.Y);
		const FVector2D AB = B - A;
		const double LengthSquared = AB.SizeSquared();
		if (LengthSquared < 1.0)
		{
			continue;
		}
		const double T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / LengthSquared, 0.02, 0.98);
		const FVector2D Closest = A + AB * T;
		const double Distance = FVector2D::Distance(P, Closest);
		if (Distance < Best)
		{
			Best = Distance;
			BestSegment = Index;
			BestPoint = FVector(Closest.X, Closest.Y, 0.0);
		}
	}
	if (BestSegment < 0 || Best > 450.0)
	{
		Say(TEXT("La barricade se pose en travers du chemin des zombies : approche-toi du chemin."));
		return false;
	}
	for (const TObjectPtr<ADRBarrier>& Other : Chapter->GetBarriers())
	{
		if (Other && FVector::Dist2D(Other->GetActorLocation(), BestPoint) < 300.0)
		{
			Say(TEXT("Il y a d\u00E9j\u00E0 une barri\u00E8re ici."));
			return false;
		}
	}
	ADRBarrier* Barricade = GetWorld()->SpawnActor<ADRBarrier>(BestPoint, FRotator::ZeroRotator);
	if (!Barricade)
	{
		return false;
	}
	// Plus solide au fil des vagues : 250 PV, +10 % par vague.
	const FVector& Next = Points[BestSegment + 1];
	const double Along = 1000.0 * (BestSegment + 1) - FVector::Dist2D(BestPoint, Next) / 100.0;
	Barricade->BuildBarricade(250.0f * (1.0f + 0.1f * FMath::Max(0, Wave - 1)), Next - Points[BestSegment], 360.0, Along, 20.0f);
	Chapter->AddBarrier(Barricade);
	DRLook::PlayAt(this, TEXT("build"), BestPoint + FVector(0.0, 0.0, 50.0), 0.0f, 0.1f);
	return true;
}

bool ADRGameMode::ThrowDecoy(ADRCharacter* User)
{
	// Lancé devant soi : il atterrit au sol, jusqu'à 14 m (ou devant le premier obstacle).
	const FVector From = User->GetEyeLocation();
	const FVector Aim = User->GetAimDirection();
	FVector Land = From + Aim * 1400.0;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DRLeurre), false, User);
	FCollisionObjectQueryParams Solid(ECC_WorldStatic);
	Solid.AddObjectTypesToQuery(ECC_WorldDynamic);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, From, Land, Solid, Params))
	{
		Land = Hit.ImpactPoint - Aim * 40.0;
	}
	Land.Z = 2.0;
	ADRDecoy* Decoy = GetWorld()->SpawnActor<ADRDecoy>(From, FRotator(0.0, Aim.Rotation().Yaw, 0.0));
	if (!Decoy)
	{
		return false;
	}
	Decoy->Launch(From, Land);
	PlaySfx2D(TEXT("throw"), -6.0f);
	return true;
}

bool ADRGameMode::LaunchDrone(ADRCharacter* User)
{
	// Il décolle 2 m au-dessus des pieds du joueur.
	const FVector Start = User->GetActorLocation() + FVector(0.0, 0.0, 200.0 - User->GetSimpleCollisionHalfHeight());
	ADRDrone* Drone = GetWorld()->SpawnActor<ADRDrone>(Start, FRotator(0.0, User->GetActorRotation().Yaw, 0.0));
	if (!Drone)
	{
		return false;
	}
	DRLook::PlayAt(this, TEXT("build"), Start, -6.0f, 0.1f);
	return true;
}

// ---------------------------------------------------------------- implants

const TArray<FName>& ADRGameMode::GetImplantIds()
{
	static const TArray<FName> Ids = { TEXT("batterie"), TEXT("sang_froid"), TEXT("charognard"), TEXT("main_lourde") };
	return Ids;
}

FString ADRGameMode::GetImplantText(FName Id, const TCHAR* Field) const
{
	FString Text = Id.ToString();
	const TSharedPtr<FJsonObject>* List = nullptr;
	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (GameData.IsValid() && GameData->TryGetObjectField(TEXT("implants"), List) && (*List)->TryGetObjectField(Id.ToString(), Entry))
	{
		(*Entry)->TryGetStringField(Field, Text);
	}
	return Text;
}

void ADRGameMode::ChooseImplant(FName Id)
{
	if (!bImplantChoice || !ImplantOptions.Contains(Id))
	{
		return;
	}
	bImplantChoice = false;
	ImplantOptions.Reset();
	Implants.AddUnique(Id);
	UGameplayStatics::SetGamePaused(this, false);
	if (ADRCharacter* Player = Cast<ADRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Player->ApplyImplants();
	}
	Say(FString::Printf(TEXT("Implant install\u00E9 : %s"), *GetImplantText(Id, TEXT("name"))));
	EnterPhase(EDRPhase::Harvest, (float)Rule(TEXT("vagues"), TEXT("harvest_time"), 12.0));
}

void ADRGameMode::SkipPhase()
{
	if (bMenu)
	{
		// Menu principal : Entrée lance le dernier niveau débloqué.
		PlayLevel(Level);
		return;
	}
	if (bPauseMenu)
	{
		return;
	}
	if (bOver)
	{
		// Victoire : niveau suivant ; défaite : on recommence le même niveau.
		PlayLevel(bVictory && HasNextLevel() ? Level + 1 : Level);
		return;
	}
	if (Phase == EDRPhase::Prep || Phase == EDRPhase::Harvest)
	{
		PhaseTime = 0.0f;
	}
}

void ADRGameMode::TogglePause()
{
	// Échap ou P referment l'établi au lieu d'ouvrir la pause.
	if (bWorkbenchOpen)
	{
		CloseWorkbench();
		return;
	}
	if (bMenu || bOver || bImplantChoice)
	{
		return;
	}
	bPauseMenu = !bPauseMenu;
	UGameplayStatics::SetGamePaused(this, bPauseMenu);
}

void ADRGameMode::PlayLevel(int32 LevelIndex)
{
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)), true,
		FString::Printf(TEXT("Niveau=%d"), FMath::Clamp(LevelIndex, 0, LevelCount - 1) + 1));
}

void ADRGameMode::OpenMainMenu()
{
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)), true, FString());
}

void ADRGameMode::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
}

int32 ADRGameMode::GetChapterCount() const
{
	const TArray<TSharedPtr<FJsonValue>>* Chapters = nullptr;
	if (ChapterData.IsValid() && ChapterData->TryGetArrayField(TEXT("chapitres"), Chapters))
	{
		return FMath::Min(Chapters->Num(), (LevelCount + 4) / 5);
	}
	return (LevelCount + 4) / 5;
}

FString ADRGameMode::GetChapterText(int32 Index, const TCHAR* Field) const
{
	FString Text;
	const TArray<TSharedPtr<FJsonValue>>* Chapters = nullptr;
	if (ChapterData.IsValid() && ChapterData->TryGetArrayField(TEXT("chapitres"), Chapters) && Chapters->IsValidIndex(Index))
	{
		const TSharedPtr<FJsonObject> Entry = (*Chapters)[Index]->AsObject();
		if (Entry.IsValid())
		{
			Entry->TryGetStringField(Field, Text);
		}
	}
	return Text;
}

void ADRGameMode::CoreDestroyed()
{
	EndGame(false);
}

void ADRGameMode::EndGame(bool bWon)
{
	if (bOver)
	{
		return;
	}
	if (bWon)
	{
		const ADRCore* Core = Chapter ? Chapter->GetCore() : nullptr;
		Score += (int32)Rule(TEXT("partie"), TEXT("victory_points"), 1000.0) + (Core ? (int32)Core->GetHp() : 0);
	}
	bOver = true;
	bVictory = bWon;
	// Insignes (Godot : Upgrades.earned) : 1 par vague repoussée, multiplié par la difficulté du
	// niveau, plus une prime en cas de victoire.
	const int32 Cleared = bWon ? LastWave : Wave - 1;
	EarnedInsignes = FMath::FloorToInt(FMath::Max(0, Cleared) * Difficulty.Points);
	if (bWon)
	{
		const TSharedPtr<FJsonValue> Prize = GameData.IsValid() ? GameData->TryGetField(TEXT("insignes_victoire")) : TSharedPtr<FJsonValue>();
		EarnedInsignes += Prize.IsValid() ? (int32)Prize->AsNumber() : 5;
	}
	// Sauvegarde : record, insignes, niveau gagné, niveau suivant débloqué.
	if (Progress)
	{
		bNewRecord = Progress->SubmitScore(Score, Wave);
		bUnlockedNext = Progress->FinishLevel(Level, bWon, LevelCount, EarnedInsignes);
		Progress->Write();
	}
}

const TArray<FName>& ADRGameMode::GetUpgradeIds()
{
	// Même ordre que Upgrades.LIST dans Godot (l'ordre du JSON n'est pas gardé par Unreal).
	static const TArray<FName> Ids = { TEXT("vitalite"), TEXT("armurier"), TEXT("ingenieur"), TEXT("maconnerie"), TEXT("reserves"), TEXT("recuperation"), TEXT("dynamo") };
	return Ids;
}

TSharedPtr<FJsonObject> ADRGameMode::GetUpgradeData(FName Id) const
{
	const TSharedPtr<FJsonObject>* List = nullptr;
	const TSharedPtr<FJsonObject>* Entry = nullptr;
	if (GameData.IsValid() && GameData->TryGetObjectField(TEXT("ameliorations"), List) && (*List)->TryGetObjectField(Id.ToString(), Entry))
	{
		return *Entry;
	}
	return TSharedPtr<FJsonObject>();
}

int32 ADRGameMode::GetUpgradeMaxRank(FName Id) const
{
	const TSharedPtr<FJsonObject> Data = GetUpgradeData(Id);
	const TArray<TSharedPtr<FJsonValue>>* Costs = nullptr;
	return Data.IsValid() && Data->TryGetArrayField(TEXT("costs"), Costs) ? Costs->Num() : 0;
}

int32 ADRGameMode::GetUpgradeRank(FName Id) const
{
	return Progress ? FMath::Clamp(Progress->Upgrades.FindRef(Id), 0, GetUpgradeMaxRank(Id)) : 0;
}

int32 ADRGameMode::GetUpgradeCost(FName Id) const
{
	const TSharedPtr<FJsonObject> Data = GetUpgradeData(Id);
	const TArray<TSharedPtr<FJsonValue>>* Costs = nullptr;
	const int32 Rank = GetUpgradeRank(Id);
	if (Data.IsValid() && Data->TryGetArrayField(TEXT("costs"), Costs) && Costs->IsValidIndex(Rank))
	{
		return (int32)(*Costs)[Rank]->AsNumber();
	}
	return -1;
}

float ADRGameMode::UpgradeBonus(FName Id) const
{
	return GetUpgradeRank(Id) * (float)DRData::Number(GetUpgradeData(Id), TEXT("per"), 0.0);
}

FString ADRGameMode::GetUpgradeText(FName Id, const TCHAR* Field) const
{
	FString Text = Id.ToString();
	if (const TSharedPtr<FJsonObject> Data = GetUpgradeData(Id))
	{
		Data->TryGetStringField(Field, Text);
	}
	return Text;
}

bool ADRGameMode::BuyUpgrade(FName Id)
{
	const int32 Cost = GetUpgradeCost(Id);
	if (!Progress || Cost < 0 || Progress->Insignes < Cost)
	{
		return false;
	}
	Progress->Insignes -= Cost;
	Progress->Upgrades.Add(Id, GetUpgradeRank(Id) + 1);
	Progress->Write();
	return true;
}

void ADRGameMode::RefundUpgrades()
{
	if (!Progress)
	{
		return;
	}
	int32 Spent = 0;
	for (const FName& Id : GetUpgradeIds())
	{
		const TSharedPtr<FJsonObject> Data = GetUpgradeData(Id);
		const TArray<TSharedPtr<FJsonValue>>* Costs = nullptr;
		if (Data.IsValid() && Data->TryGetArrayField(TEXT("costs"), Costs))
		{
			for (int32 Step = 0; Step < GetUpgradeRank(Id) && Costs->IsValidIndex(Step); ++Step)
			{
				Spent += (int32)(*Costs)[Step]->AsNumber();
			}
		}
	}
	Progress->Insignes += Spent;
	Progress->Upgrades.Empty();
	Progress->Write();
}

void ADRGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bMenu)
	{
		return;
	}
	// Recharge des gadgets (le jeu en pause ne compte pas).
	for (TPair<FName, float>& Cooldown : GadgetCooldowns)
	{
		Cooldown.Value = FMath::Max(0.0f, Cooldown.Value - DeltaSeconds);
	}
	if (!bStarted)
	{
		bStarted = true;
		Wave = 1;
		EnterPhase(EDRPhase::Prep, (float)Rule(TEXT("vagues"), TEXT("first_prep_time"), 40.0));
	}
	if (bOver)
	{
		return;
	}
	UpdateTutorial(DeltaSeconds);
	switch (Phase)
	{
	case EDRPhase::Prep:
		// Pendant le tutoriel, la première préparation attend que le joueur lance la vague.
		if (!IsTutorialHolding())
		{
			PhaseTime -= DeltaSeconds;
		}
		if (PhaseTime <= 0.0f)
		{
			EnterPhase(EDRPhase::Assault, 0.0f);
		}
		break;
	case EDRPhase::Assault:
		SpawnCooldown -= DeltaSeconds;
		if (Queue.Num() > 0 && SpawnCooldown <= 0.0f)
		{
			const FString Type = Queue[0];
			Queue.RemoveAt(0);
			SpawnZombie(Type);
			SpawnCooldown = FMath::Max(0.3f, 1.3f - 0.11f * Wave) * Difficulty.Spawn;
			// À partir de la vague 3, les zombies arrivent parfois en meute serrée.
			if (Wave >= 3 && FMath::FRand() < 0.15f + 0.03f * Wave)
			{
				SpawnCooldown = 0.15f;
			}
		}
		if (Queue.Num() == 0 && Alive <= 0)
		{
			WaveCleared();
		}
		break;
	case EDRPhase::Harvest:
		PhaseTime -= DeltaSeconds;
		if (PhaseTime <= 0.0f)
		{
			// La ferraille pas ramassée disparaît.
			for (TActorIterator<ADRScrap> It(GetWorld()); It; ++It)
			{
				It->Destroy();
			}
			++Wave;
			EnterPhase(EDRPhase::Prep, (float)Rule(TEXT("vagues"), TEXT("prep_time"), 25.0));
		}
		break;
	}
}

// ---------------------------------------------------------------- tutoriel

FString ADRGameMode::GetTutorialText() const
{
	// Les touches affichées sont celles choisies dans les réglages.
	auto Key = [this](const TCHAR* Id) { return Progress ? Progress->KeyName(Id) : UDRSave::KeyLabel(UDRSave::DefaultKey(Id)); };
	const FString MoveKeys = Progress ? Progress->MoveKeysName() : FString(TEXT("ZQSD"));
	switch (TutorialStep)
	{
	case 0:
		return FString::Printf(TEXT("D\u00E9place-toi avec %s et cours avec %s."), *MoveKeys, *Key(TEXT("courir")));
	case 1:
		return TEXT("Regarde autour de toi avec la souris.");
	case 2:
		return TEXT("Les zombies arrivent du PORTAIL, la br\u00E8che \u00E9clair\u00E9e en rouge au bout du chemin de boue. Ils suivent ce chemin jusqu'au C\u0152UR, qu'il faut prot\u00E9ger.");
	case 3:
		return FString::Printf(TEXT("Le chemin est coup\u00E9 par la barri\u00E8re de l'Avant-poste (grillage, barbel\u00E9s et blocs de b\u00E9ton). Les zombies doivent la casser pour passer, pendant que tes tours tirent. Si elle tombe, maintiens %s dessus pour la relever. Une deuxi\u00E8me barri\u00E8re prot\u00E8ge la Muraille."), *Key(TEXT("interagir")));
	case 4:
		return FString::Printf(TEXT("Approche d'un ancrage (dalle de b\u00E9ton avec des rep\u00E8res orange) pr\u00E8s de la barri\u00E8re. La tour choisie s'affiche en bas : appuie sur %s pour poser une Mitrailleuse."), *Key(TEXT("interagir")));
	case 5:
		return FString::Printf(TEXT("Sur un autre ancrage, choisis la Cryo avec la molette ou %s, puis %s. Elle g\u00E8le les zombies. Il existe 6 tours : Mitrailleuse, Cryo, Lance-flammes, Arc \u00E9lectrique, Mortier et Phare."), *Key(TEXT("tour")), *Key(TEXT("interagir")));
	case 6:
		return FString::Printf(TEXT("Chaque tour allum\u00E9e consomme de l'\u00E9nergie (en haut \u00E0 gauche). Vise une tour et appuie sur %s pour l'\u00E9teindre et r\u00E9cup\u00E9rer son \u00E9nergie, puis rallume-la."), *Key(TEXT("allumer")));
	case 7:
		return FString::Printf(TEXT("Pr\u00E8s du C\u0153ur, l'\u00C9TABLI am\u00E9liore tes armes (canon, chargeur, munitions sp\u00E9ciales), choisit tes gadgets et renforce le g\u00E9n\u00E9rateur. Il s'utilise entre les vagues avec %s."), *Key(TEXT("interagir")));
	case 8:
		return FString::Printf(TEXT("Pr\u00EAt ? Appuie sur %s pour lancer la premi\u00E8re vague."), *Key(TEXT("vague")));
	case 9:
		return FString::Printf(TEXT("Vise avec %s et tire avec %s. Un tir \u00E0 la t\u00EAte fait x2 d\u00E9g\u00E2ts."), *Key(TEXT("viser")), *Key(TEXT("tirer")));
	case 10:
		return FString::Printf(TEXT("Vise un zombie et appuie sur %s pour le marquer : tes tours le ciblent en priorit\u00E9 (+25 %% de d\u00E9g\u00E2ts)."), *Key(TEXT("marquer")));
	case 11:
		return FString::Printf(TEXT("Gadgets : %s pose une barricade en travers du chemin, %s lance un leurre sonore qui attire les zombies. Leur recharge s'affiche en bas \u00E0 gauche."), *Key(TEXT("gadget1")), *Key(TEXT("gadget2")));
	case 12:
		return FString::Printf(TEXT("Combos : un tir de pistolet lourd (%s) sur un zombie gel\u00E9 le BRISE (x3). Un zombie charg\u00E9 par l'Arc \u00E9lectrique touch\u00E9 par une balle d\u00E9clenche une SURCHARGE. Un obus de Mortier sur un zombie en feu provoque un EMBRASEMENT."), *Key(TEXT("pistolet")));
	case 13:
		return TEXT("Les zombies laissent de la ferraille. Va la ramasser : elle sert \u00E0 construire, r\u00E9parer et am\u00E9liorer.");
	default:
		return FString::Printf(TEXT("Tutoriel termin\u00E9 ! Ta sant\u00E9 revient \u00E0 fond apr\u00E8s chaque vague repouss\u00E9e. Si une barri\u00E8re tombe, maintiens %s pr\u00E8s d'elle pour la relever. \u00C9chap ou %s met en pause. Bonne chance !"), *Key(TEXT("interagir")), *Key(TEXT("pause")));
	}
}

void ADRGameMode::SkipTutorial()
{
	if (!IsTutorialActive())
	{
		return;
	}
	Say(TEXT("Tutoriel pass\u00E9."));
	FinishTutorial();
}

void ADRGameMode::ReplayTutorial()
{
	if (Progress)
	{
		Progress->bTutorialDone = false;
		Progress->Write();
	}
	PlayLevel(0);
}

void ADRGameMode::UpdateTutorial(float DeltaSeconds)
{
	if (!IsTutorialActive())
	{
		return;
	}
	TutorialTimer += DeltaSeconds;
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		// Le joueur n'existe pas encore quand la partie se prépare : l'étape démarre à son arrivée.
		if (!bTutorialPrimed)
		{
			StartTutorialStep();
		}
		const float Yaw = (float)Pawn->GetControlRotation().Yaw;
		TutorialTurned += FMath::DegreesToRadians(FMath::Abs(FMath::FindDeltaAngleDegrees(TutorialLastYaw, Yaw)));
		TutorialLastYaw = Yaw;
	}
	if (IsTutorialStepDone())
	{
		++TutorialStep;
		if (TutorialStep >= GetTutorialStepCount())
		{
			FinishTutorial();
			return;
		}
		StartTutorialStep();
	}
}

void ADRGameMode::StartTutorialStep()
{
	TutorialTimer = 0.0f;
	TutorialTurned = 0.0f;
	TutorialHits = 0;
	TutorialPicked = 0;
	bTutorialSeenOff = false;
	const APawn* Pawn = GetWorld() ? UGameplayStatics::GetPlayerPawn(this, 0) : nullptr;
	bTutorialPrimed = Pawn != nullptr;
	if (Pawn)
	{
		TutorialStart = Pawn->GetActorLocation();
		TutorialLastYaw = (float)Pawn->GetControlRotation().Yaw;
	}
}

bool ADRGameMode::IsTutorialStepDone()
{
	UWorld* World = GetWorld();
	auto Built = [World](const TCHAR* TowerType)
	{
		for (TActorIterator<ADRTower> It(World); It; ++It)
		{
			if (It->GetType() == TowerType)
			{
				return true;
			}
		}
		return false;
	};
	switch (TutorialStep)
	{
	case 0:
	{
		const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
		return bTutorialPrimed && Pawn && FVector::Dist(Pawn->GetActorLocation(), TutorialStart) > 400.0;
	}
	case 1:
		return TutorialTurned > 1.5f;
	case 2:
		return TutorialTimer > 7.0f;
	case 3:
		return TutorialTimer > 9.0f;
	case 4:
		return Built(TEXT("gun"));
	case 5:
		return Built(TEXT("cryo"));
	case 6:
	{
		// Validé quand une tour a été éteinte puis rallumée (ou après un moment, pour ne pas bloquer).
		bool bAllOn = true;
		for (TActorIterator<ADRTower> It(World); It; ++It)
		{
			if (!It->IsPowered())
			{
				bTutorialSeenOff = true;
				bAllOn = false;
			}
		}
		return (bTutorialSeenOff && bAllOn) || TutorialTimer > 25.0f;
	}
	case 7:
		return bTutorialBench || TutorialTimer > 14.0f;
	case 8:
		return Phase == EDRPhase::Assault;
	case 9:
		return TutorialHits >= 3;
	case 10:
	{
		for (TActorIterator<ADRZombie> It(World); It; ++It)
		{
			if (It->IsMarked())
			{
				return true;
			}
		}
		return TutorialTimer > 30.0f;
	}
	case 11:
	{
		for (const TPair<FName, float>& Cooldown : GadgetCooldowns)
		{
			if (Cooldown.Value > 0.0f)
			{
				return true;
			}
		}
		return TutorialTimer > 20.0f;
	}
	case 12:
		return TutorialTimer > 12.0f;
	case 13:
		return TutorialPicked > 0 || TutorialTimer > 40.0f;
	default:
		return TutorialTimer > 9.0f;
	}
}

void ADRGameMode::FinishTutorial()
{
	TutorialStep = -1;
	if (Progress)
	{
		Progress->bTutorialDone = true;
		Progress->Write();
	}
}
