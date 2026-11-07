#include "DRGameMode.h"

#include "DRChapter.h"
#include "DRCharacter.h"
#include "DRData.h"
#include "DRHUD.h"
#include "DRZombie.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"

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

ADRChapter* ADRGameMode::GetChapter() const
{
	return Chapter;
}

USoundAttenuation* ADRGameMode::GetAttenuation(float MaxDistance) const
{
	return MaxDistance <= 5000.0f ? NearAttenuation : FarAttenuation;
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

ADRZombie* ADRGameMode::SpawnZombie(const FString& Type, float HpScale, float SpeedScale, float DamageScale)
{
	if (!Chapter || Chapter->GetPath().Num() < 2)
	{
		return nullptr;
	}
	// Au portail, un peu dispersés (Godot : ±2,5 m sur la largeur, ±1,5 m en profondeur).
	const FVector Portal = Chapter->GetPath()[0];
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FVector Spot = Portal + FVector(FMath::FRandRange(-150.0, 150.0), FMath::FRandRange(-250.0, 250.0), 300.0);
	const FRotator Facing(0.0, (Chapter->GetPath()[1] - Portal).Rotation().Yaw, 0.0);
	ADRZombie* Zombie = GetWorld()->SpawnActor<ADRZombie>(ADRZombie::StaticClass(), Spot, Facing, Params);
	if (Zombie)
	{
		Zombie->Setup(Type, HpScale, SpeedScale, DamageScale);
		Zombie->SetActorLocation(FVector(Spot.X, Spot.Y, Portal.Z + Zombie->GetSimpleCollisionHalfHeight() + 5.0));
	}
	return Zombie;
}

void ADRGameMode::ZombieDied(ADRZombie* Zombie)
{
	++Kills;
}

void ADRGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Étape 5 du portage : en attendant les vagues, un zombie toutes les 4 secondes (au plus 10
	// en même temps) pour tester le tir, les barrières et le Cœur.
	TestSpawnCooldown -= DeltaSeconds;
	if (TestSpawnCooldown > 0.0f)
	{
		return;
	}
	TestSpawnCooldown = 4.0f;
	int32 Alive = 0;
	for (TActorIterator<ADRZombie> It(GetWorld()); It; ++It)
	{
		Alive += It->IsDead() ? 0 : 1;
	}
	if (Alive < 10)
	{
		const float Roll = FMath::FRand();
		SpawnZombie(Roll < 0.6f ? TEXT("rodeur") : (Roll < 0.85f ? TEXT("coureur") : TEXT("brute")), 1.0f, 1.0f, 1.0f);
	}
}
