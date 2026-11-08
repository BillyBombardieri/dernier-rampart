#include "DRWeather.h"

#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DRChapter.h"
#include "DRData.h"
#include "DRLook.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

namespace DRWeatherLook
{
	// Petit losange (octaèdre) de 1 m de haut et de large, réduit à la pose : flocon, cendre,
	// grain de poussière, luciole, ou trait de pluie une fois étiré en hauteur.
	UStaticMesh* MakeSpeck(UObject* Outer)
	{
		DRLook::FMeshParts Parts;
		const FVector3f Tips[6] = {
			FVector3f(50.0f, 0.0f, 0.0f), FVector3f(-50.0f, 0.0f, 0.0f),
			FVector3f(0.0f, 50.0f, 0.0f), FVector3f(0.0f, -50.0f, 0.0f),
			FVector3f(0.0f, 0.0f, 50.0f), FVector3f(0.0f, 0.0f, -50.0f) };
		int32 Ids[6];
		for (int32 Index = 0; Index < 6; ++Index)
		{
			Ids[Index] = Parts.Corner(Tips[Index], Tips[Index].GetSafeNormal(), FVector2f(0.5f, 0.5f));
		}
		// Huit faces : une pointe sur chaque axe (avant ou arrière, droite ou gauche, haut ou bas).
		for (int32 AlongX = 0; AlongX < 2; ++AlongX)
		{
			for (int32 AlongY = 2; AlongY < 4; ++AlongY)
			{
				for (int32 AlongZ = 4; AlongZ < 6; ++AlongZ)
				{
					Parts.Face(Ids[AlongX], Ids[AlongY], Ids[AlongZ]);
				}
			}
		}
		return DRLook::BuildMesh(Outer, Parts);
	}

	// Matériau M_Meteo du script d'import : sans ombrage et transparent, comme dans Godot. Avant
	// l'import : couleur unie.
	UMaterialInterface* MakeLook(UObject* Outer, const FLinearColor& ColorSRGB, float Opacity, float Glow)
	{
		if (UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Importe/Materiaux/M_Meteo.M_Meteo"), nullptr, LOAD_NoWarn))
		{
			UMaterialInstanceDynamic* Result = UMaterialInstanceDynamic::Create(Parent, Outer);
			Result->SetVectorParameterValue(TEXT("Couleur"), DRData::ToLinear(ColorSRGB));
			Result->SetScalarParameterValue(TEXT("Opacite"), Opacity);
			Result->SetScalarParameterValue(TEXT("Eclat"), Glow);
			return Result;
		}
		return DRLook::Plain(Outer, ColorSRGB);
	}
}

ADRWeather::ADRWeather()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Root;

	// Sons d'ambiance sans position : on les entend pareil partout.
	WindSound = CreateDefaultSubobject<UAudioComponent>(TEXT("Vent"));
	WindSound->SetupAttachment(RootComponent);
	WindSound->bAutoActivate = false;
	WindSound->bAllowSpatialization = false;

	RainSound = CreateDefaultSubobject<UAudioComponent>(TEXT("Pluie"));
	RainSound->SetupAttachment(RootComponent);
	RainSound->bAutoActivate = false;
	RainSound->bAllowSpatialization = false;
}

void ADRWeather::Setup(const FString& InKind)
{
	Rng.Initialize(4242);
	int32 Amount = 0;
	FLinearColor Tone = FLinearColor::White; // sRGB, comme dans Godot
	float Opacity = 1.0f;
	float Glow = 1.0f;
	Extents = FVector(2200.0, 2200.0, 200.0);
	Height = 800.0f;
	FadeShape = 0;

	// Réglages de Godot (repère converti : X avant = -Z de Godot, Y droite = X, Z haut = Y ; mètres -> cm).
	if (InKind == TEXT("snow"))
	{
		Amount = 1400;
		Lifetime = 5.0f;
		Direction = FVector(-0.1, 0.3, -1.0);
		Spread = 15.0f;
		SpeedMin = 200.0f;
		SpeedMax = 350.0f;
		Gravity = FVector(-20.0, 60.0, -40.0);
		Size = FVector(6.0);
		Tone = FLinearColor(0.95f, 0.97f, 1.0f);
		Opacity = 0.9f;
	}
	else if (InKind == TEXT("ash"))
	{
		Amount = 500;
		Lifetime = 7.0f;
		Direction = FVector(0.0, 0.2, -1.0);
		Spread = 30.0f;
		SpeedMin = 50.0f;
		SpeedMax = 120.0f;
		Gravity = FVector(-5.0, 25.0, -15.0);
		Size = FVector(5.0);
		Tone = FLinearColor(0.55f, 0.52f, 0.5f);
		Opacity = 0.8f;
	}
	else if (InKind == TEXT("rain"))
	{
		// Traits fins et rapides, presque verticaux, un peu poussés par le vent.
		Amount = 1800;
		Lifetime = 0.9f;
		Direction = FVector(-0.04, 0.08, -1.0);
		Spread = 3.0f;
		SpeedMin = 1600.0f;
		SpeedMax = 2000.0f;
		Gravity = FVector(0.0, 0.0, -600.0);
		Size = FVector(1.2, 1.2, 42.0);
		Tone = FLinearColor(0.72f, 0.76f, 0.82f);
		Opacity = 0.25f;
		FadeShape = 1;
	}
	else if (InKind == TEXT("dust"))
	{
		// Poussière soulevée par le vent : de fines particules qui filent presque à l'horizontale.
		Amount = 700;
		Lifetime = 3.0f;
		Extents = FVector(2400.0, 2400.0, 150.0);
		Height = 150.0f;
		Direction = FVector(-0.3, 1.0, 0.05);
		Spread = 12.0f;
		SpeedMin = 500.0f;
		SpeedMax = 900.0f;
		Gravity = FVector(0.0, 0.0, -20.0);
		Size = FVector(5.0);
		Tone = FLinearColor(0.85f, 0.7f, 0.5f);
		Opacity = 0.5f;
		FadeShape = 2;
	}
	else if (InKind == TEXT("fireflies"))
	{
		// Lucioles du Marais : quelques points verts qui flottent, s'allument puis s'éteignent.
		Amount = 70;
		Lifetime = 6.0f;
		Extents = FVector(2500.0, 2500.0, 300.0);
		Height = 150.0f;
		Direction = FVector(0.0, 0.0, 1.0);
		Spread = 180.0f;
		SpeedMin = 10.0f;
		SpeedMax = 40.0f;
		Gravity = FVector::ZeroVector;
		Size = FVector(7.0);
		Tone = FLinearColor(0.8f, 1.0f, 0.4f);
		Opacity = 0.9f;
		Glow = 4.0f; // elles brillent (et le flou lumineux d'Unreal les auréole)
		FadeShape = 3;
	}

	// Le vent souffle dans tous les chapitres ; la pluie s'entend en plus dans la Ville Morte.
	WindDecibels = InKind == TEXT("rain") ? -20.0f : (InKind == TEXT("dust") ? -9.0f : -14.0f);
	if (USoundBase* Gust = DRLook::Sound(TEXT("wind")))
	{
		WindSound->SetSound(Gust);
		WindSound->SetVolumeMultiplier(DRLook::Volume(WindDecibels));
		WindSound->Play();
	}
	if (InKind == TEXT("rain"))
	{
		if (USoundBase* Shower = DRLook::Sound(TEXT("rain")))
		{
			RainSound->SetSound(Shower);
			RainSound->SetVolumeMultiplier(DRLook::Volume(-12.0f));
			RainSound->Play();
		}
	}

	if (Amount <= 0)
	{
		return;
	}
	Direction = Direction.GetSafeNormal();
	SpeckMesh = DRWeatherLook::MakeSpeck(this);
	Particles = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Particules"));
	Particles->SetMobility(EComponentMobility::Movable);
	Particles->SetupAttachment(RootComponent);
	Particles->SetStaticMesh(SpeckMesh);
	Particles->SetMaterial(0, DRWeatherLook::MakeLook(this, Tone, Opacity, Glow));
	Particles->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Particles->SetCanEverAffectNavigation(false);
	Particles->SetCastShadow(false);
	Particles->bAffectDistanceFieldLighting = false;
	Particles->RegisterComponent();
	AddInstanceComponent(Particles);

	// Départ échelonné, comme les particules de Godot : chacune a déjà vécu un moment au hasard.
	const FVector Origin = EmitterLocation();
	Flakes.SetNum(Amount);
	Frame.SetNum(Amount);
	for (int32 Index = 0; Index < Amount; ++Index)
	{
		FDRFlake& Flake = Flakes[Index];
		Respawn(Flake, Origin);
		const float Lived = Rng.FRandRange(0.0f, Lifetime);
		Flake.Location += Flake.Velocity * Lived + Gravity * (0.5 * Lived * Lived);
		Flake.Velocity += Gravity * Lived;
		Flake.Age = Lived;
		const float Shown = FMath::Max(FMath::Sqrt(Fade(Lived / Lifetime)), 0.02f);
		Frame[Index] = FTransform(FRotator::ZeroRotator, Flake.Location, Size * Shown);
	}
	Particles->AddInstances(Frame, false, true);
}

void ADRWeather::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	KeepPlaying(WindSound, WindDecibels);
	KeepPlaying(RainSound, -12.0f);
	if (!Particles || Flakes.Num() == 0)
	{
		return;
	}
	const FVector Origin = EmitterLocation();
	const float Step = FMath::Min(DeltaSeconds, 0.1f);
	for (int32 Index = 0; Index < Flakes.Num(); ++Index)
	{
		FDRFlake& Flake = Flakes[Index];
		Flake.Age += Step;
		if (Flake.Age >= Lifetime)
		{
			Respawn(Flake, Origin);
		}
		Flake.Velocity += Gravity * Step;
		Flake.Location += Flake.Velocity * Step;
		// Une particule qui s'efface rapetisse : sur quelques centimètres, l'œil ne voit pas la différence.
		const float Shown = FMath::Max(FMath::Sqrt(Fade(Flake.Age / Lifetime)), 0.02f);
		Frame[Index] = FTransform(FRotator::ZeroRotator, Flake.Location, Size * Shown);
	}
	Particles->BatchUpdateInstancesTransforms(0, Frame, true, true, true);
	Particles->UpdateBounds();
}

FVector ADRWeather::EmitterLocation() const
{
	FVector Around = ADRChapter::PlayerStart().GetLocation();
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Around = Pawn->GetActorLocation();
	}
	// Le sol est plat, à 0 : la boîte reste à la même hauteur au-dessus du sol.
	return FVector(Around.X, Around.Y, Height);
}

void ADRWeather::Respawn(FDRFlake& Flake, const FVector& Origin)
{
	Flake.Location = Origin + FVector(Rng.FRandRange(-Extents.X, Extents.X), Rng.FRandRange(-Extents.Y, Extents.Y), Rng.FRandRange(-Extents.Z, Extents.Z));
	Flake.Velocity = Rng.VRandCone(Direction, FMath::DegreesToRadians(Spread)) * Rng.FRandRange(SpeedMin, SpeedMax);
	Flake.Age = 0.0f;
}

float ADRWeather::Fade(float LifeFraction) const
{
	const float T = FMath::Clamp(LifeFraction, 0.0f, 1.0f);
	switch (FadeShape)
	{
	case 1:
		return 1.0f;
	case 2:
		return T < 0.3f ? T / 0.3f : (1.0f - T) / 0.7f; // apparaît vite, s'efface lentement
	case 3:
		return 1.0f - FMath::Abs(2.0f * T - 1.0f); // s'allume puis s'éteint
	default:
		return 1.0f - T; // neige et cendres
	}
}

void ADRWeather::KeepPlaying(UAudioComponent* Track, float Decibels)
{
	if (!Track || !Track->Sound)
	{
		return;
	}
	// Suit le volume général s'il change dans les réglages.
	Track->SetVolumeMultiplier(DRLook::Volume(Decibels));
	if (!Track->IsPlaying())
	{
		Track->Play();
	}
}
