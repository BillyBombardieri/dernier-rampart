#include "DRGadgets.h"

#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DRCharacter.h"
#include "DRGameMode.h"
#include "DRLook.h"
#include "DRScrap.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace DRGadgetParts
{
	// Petite pièce sans collision : les balles et les zombies passent au travers, comme dans Godot.
	UStaticMeshComponent* Piece(AActor* Owner, USceneComponent* Parent, const TCHAR* MeshPath, UMaterialInterface* Material, const FVector& Center, const FVector& SizeCm, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Owner);
		Mesh->SetupAttachment(Parent);
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
		Mesh->SetMaterial(0, Material);
		Mesh->SetRelativeTransform(FTransform(Rotation, Center, SizeCm / 100.0));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(true);
		Mesh->RegisterComponent();
		return Mesh;
	}

	const TCHAR* const Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const Cylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

	// Chiffres de scripts/decoy.gd et scripts/drone.gd (distances en cm).
	constexpr float DecoyLifetime = 8.0f;
	constexpr float DecoyBeep = 0.55f;
	constexpr float DecoyFlight = 0.45f;
	constexpr float DroneLifetime = 25.0f;
	constexpr double DroneSpeed = 400.0;
	constexpr double DroneSearch = 2500.0;
	constexpr double DroneHeight = 240.0;
}

// ---------------------------------------------------------------- leurre sonore

ADRDecoy::ADRDecoy()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	// Lueur rouge du voyant, qui clignote avec les bips.
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lueur"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetRelativeLocation(FVector(5.0, -12.0, 72.0));
	Glow->IntensityUnits = ELightUnits::Candelas;
	Glow->Intensity = 25.0f;
	Glow->AttenuationRadius = 400.0f;
	Glow->LightColor = FColor(255, 51, 26);
	Glow->CastShadows = false;
}

void ADRDecoy::Launch(const FVector& From, const FVector& Land)
{
	// Boîtier métallique, antenne, haut-parleur et voyant (Godot : decoy.gd, _ready).
	UMaterialInterface* Metal = DRLook::Textured(this, TEXT("metal"), 2.0f, FLinearColor(0.6f, 0.6f, 0.6f));
	UMaterialInterface* Black = DRLook::Plain(this, FLinearColor(0.08f, 0.08f, 0.08f));
	DRGadgetParts::Piece(this, RootComponent, DRGadgetParts::Cube, Metal, FVector(0.0, 0.0, 11.0), FVector(25.0, 35.0, 22.0));
	DRGadgetParts::Piece(this, RootComponent, DRGadgetParts::Cylinder, Black, FVector(5.0, -12.0, 45.0), FVector(2.4, 2.4, 50.0));
	DRGadgetParts::Piece(this, RootComponent, DRGadgetParts::Cylinder, Black, FVector(-13.0, 5.0, 12.0), FVector(16.0, 16.0, 2.0), FRotator(90.0, 0.0, 0.0));
	Led = DRGadgetParts::Piece(this, RootComponent, DRGadgetParts::Cube, DRLook::Textured(this, TEXT("metal"), 1.0f, FLinearColor(1.0f, 0.15f, 0.1f), 6.0f), FVector(5.0, -12.0, 72.0), FVector(5.0, 5.0, 5.0));

	FlightStart = From;
	FlightEnd = Land;
	FlightTime = 0.0f;
	SetActorLocation(From);
}

void ADRDecoy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (ShrinkTime >= 0.0f)
	{
		// Cassé ou à court de pile : il rapetisse en 0,3 s puis disparaît.
		ShrinkTime += DeltaSeconds;
		const float Left = 1.0f - ShrinkTime / 0.3f;
		if (Left <= 0.01f)
		{
			Destroy();
			return;
		}
		SetActorScale3D(FVector(Left));
		return;
	}
	if (FlightTime >= 0.0f)
	{
		FlightTime += DeltaSeconds;
		const float T = FMath::Min(1.0f, FlightTime / DRGadgetParts::DecoyFlight);
		SetActorLocation(FMath::Lerp(FlightStart, FlightEnd, (double)T) + FVector(0.0, 0.0, FMath::Sin(T * PI) * 150.0));
		if (T >= 1.0f)
		{
			FlightTime = -1.0f;
		}
	}
	Age += DeltaSeconds;
	BeepCooldown -= DeltaSeconds;
	if (BeepCooldown <= 0.0f)
	{
		BeepCooldown = DRGadgetParts::DecoyBeep;
		DRLook::PlayAt(this, TEXT("decoy"), GetActorLocation() + FVector(0.0, 0.0, 30.0), 2.0f, 0.02f, 6000.0f);
		if (ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>())
		{
			Mode->AddRing(GetActorLocation() + FVector(0.0, 0.0, 10.0), 300.0f, FLinearColor(1.0f, 0.3f, 0.2f, 0.35f), 0.4f);
		}
	}
	const bool bBlink = FMath::Fmod(Age, DRGadgetParts::DecoyBeep) < 0.12f;
	if (Led)
	{
		Led->SetVisibility(bBlink);
	}
	Glow->SetVisibility(bBlink);
	if (Age >= DRGadgetParts::DecoyLifetime)
	{
		Stop();
	}
}

float ADRDecoy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!bActive || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}
	Hp -= DamageAmount;
	if (Hp <= 0.0f)
	{
		Stop();
	}
	return DamageAmount;
}

void ADRDecoy::Stop()
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	ShrinkTime = 0.0f;
	Glow->SetVisibility(false);
	DRLook::PlayAt(this, TEXT("barrier_hit"), GetActorLocation() + FVector(0.0, 0.0, 20.0), -10.0f, 0.2f);
}

// ---------------------------------------------------------------- drone de récolte

ADRDrone::ADRDrone()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	Body = CreateDefaultSubobject<USceneComponent>(TEXT("Corps"));
	Body->SetupAttachment(RootComponent);
	// Bourdonnement des hélices, relancé dès qu'il se termine (le son n'est pas en boucle).
	Hum = CreateDefaultSubobject<UAudioComponent>(TEXT("Bourdonnement"));
	Hum->SetupAttachment(RootComponent);
	Hum->bAutoActivate = false;
}

void ADRDrone::BeginPlay()
{
	Super::BeginPlay();
	// Corps sombre, objectif vert à l'avant (+X), quatre bras et quatre hélices (Godot : drone.gd, _ready).
	UMaterialInterface* Dark = DRLook::Textured(this, TEXT("metal"), 2.0f, FLinearColor(0.2f, 0.21f, 0.23f));
	UMaterialInterface* Blade = DRLook::Plain(this, FLinearColor(0.2f, 0.2f, 0.2f));
	DRGadgetParts::Piece(this, Body, DRGadgetParts::Cube, Dark, FVector::ZeroVector, FVector(36.0, 36.0, 12.0));
	DRGadgetParts::Piece(this, Body, DRGadgetParts::Cube, DRLook::Textured(this, TEXT("metal"), 1.0f, FLinearColor(0.3f, 1.0f, 0.6f), 4.0f), FVector(19.0, 0.0, -3.0), FVector(4.0, 10.0, 6.0));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const double Angle = 45.0 + 90.0 * Index;
		const FVector Out(FMath::Cos(FMath::DegreesToRadians(Angle)), FMath::Sin(FMath::DegreesToRadians(Angle)), 0.0);
		DRGadgetParts::Piece(this, Body, DRGadgetParts::Cube, Dark, Out * 22.0, FVector(42.0, 5.0, 3.0), FRotator(0.0, Angle, 0.0));
		USceneComponent* Rotor = NewObject<USceneComponent>(this);
		Rotor->SetupAttachment(Body);
		Rotor->SetRelativeLocation(Out * 42.0 + FVector(0.0, 0.0, 12.0));
		Rotor->RegisterComponent();
		DRGadgetParts::Piece(this, Rotor, DRGadgetParts::Cube, Blade, FVector::ZeroVector, FVector(32.0, 4.0, 1.0));
		Rotors.Add(Rotor);
	}
	UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this);
	Lamp->SetupAttachment(Body);
	Lamp->SetRelativeLocation(FVector(0.0, 0.0, -20.0));
	Lamp->IntensityUnits = ELightUnits::Candelas;
	Lamp->Intensity = 12.0f;
	Lamp->AttenuationRadius = 300.0f;
	Lamp->LightColor = FColor(102, 255, 179);
	Lamp->CastShadows = false;
	Lamp->RegisterComponent();

	if (USoundBase* Sound = DRLook::Sound(TEXT("drone")))
	{
		Hum->SetSound(Sound);
		Hum->SetVolumeMultiplier(DRLook::Volume(-14.0f));
		if (const ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>())
		{
			Hum->AttenuationSettings = Mode->GetAttenuation(3000.0f);
		}
		Hum->Play();
	}
}

void ADRDrone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	for (USceneComponent* Rotor : Rotors)
	{
		Rotor->AddLocalRotation(FRotator(0.0, FMath::RadiansToDegrees(DeltaSeconds * 40.0f), 0.0));
	}
	if (Hum->Sound && !Hum->IsPlaying())
	{
		Hum->Play();
	}
	ADRCharacter* Player = Cast<ADRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player)
	{
		Destroy();
		return;
	}
	ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	if (Age >= DRGadgetParts::DroneLifetime && !bLeaving)
	{
		bLeaving = true;
		if (Mode)
		{
			Mode->Say(TEXT("Le drone de r\u00E9colte rentre."));
		}
	}
	// Pieds du joueur (le centre de sa capsule est 90 cm plus haut).
	const FVector Feet = Player->GetActorLocation() - FVector(0.0, 0.0, Player->GetSimpleCollisionHalfHeight());
	const FVector Here = GetActorLocation();
	FVector Destination = Here;
	ADRScrap* Piece = nullptr;
	if (bLeaving)
	{
		Destination = Feet + FVector(0.0, 0.0, 160.0);
		if (FVector::Dist(Here, Destination) < 100.0)
		{
			Destroy();
			return;
		}
	}
	else
	{
		Piece = Goal.Get();
		if (!IsValid(Piece) || Piece->IsActorBeingDestroyed())
		{
			Piece = FindScrap(Feet);
			Goal = Piece;
		}
		if (Piece)
		{
			Destination = Piece->GetActorLocation() + FVector(0.0, 0.0, 60.0);
			if (FVector::Dist2D(Here, Destination) < 50.0 && Here.Z < Destination.Z + 80.0)
			{
				Collect(Piece);
				Goal = nullptr;
				Piece = nullptr;
			}
		}
		else
		{
			// Rien à ramasser : il tourne au-dessus du joueur.
			Destination = Feet + FVector(FMath::Cos(Age) * 200.0, FMath::Sin(Age) * 200.0, DRGadgetParts::DroneHeight);
		}
	}
	// Il vole en hauteur jusqu'au-dessus de la pièce, puis descend la chercher.
	FVector Travel = Destination;
	if (Piece && FVector::Dist2D(Here, Destination) > 150.0)
	{
		Travel.Z = Destination.Z + DRGadgetParts::DroneHeight * 0.6;
	}
	const FVector To = Travel - Here;
	const double Length = To.Size();
	if (Length > KINDA_SMALL_NUMBER)
	{
		SetActorLocation(Here + To / Length * FMath::Min(Length, DRGadgetParts::DroneSpeed * DeltaSeconds));
	}
	if (To.Size2D() > 10.0)
	{
		const FRotator Facing(0.0, To.Rotation().Yaw, 0.0);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Facing, DeltaSeconds, 5.0f));
	}
	// Penché vers l'avant quand il file, à plat quand il s'arrête.
	const float Tilt = Length > 50.0 ? -14.0f : 0.0f;
	const FRotator Lean = Body->GetRelativeRotation();
	Body->SetRelativeRotation(FRotator(FMath::FInterpTo((float)Lean.Pitch, Tilt, DeltaSeconds, 4.0f), 0.0, 0.0));
}

ADRScrap* ADRDrone::FindScrap(const FVector& Around) const
{
	ADRScrap* Best = nullptr;
	double BestDistance = DRGadgetParts::DroneSearch;
	for (TActorIterator<ADRScrap> It(GetWorld()); It; ++It)
	{
		ADRScrap* Candidate = *It;
		if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed() || Candidate->IsClaimed())
		{
			continue;
		}
		const double Distance = FVector::Dist2D(Candidate->GetActorLocation(), Around);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Candidate;
		}
	}
	if (Best)
	{
		// Un seul drone par pièce.
		Best->Claim();
	}
	return Best;
}

void ADRDrone::Collect(ADRScrap* Piece)
{
	const int32 Value = Piece->GetValue();
	if (ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>())
	{
		Mode->AddScrap(Value);
		Mode->Popup(FString::Printf(TEXT("+%d"), Value), GetActorLocation() + FVector(0.0, 0.0, 60.0), FLinearColor(0.5f, 1.0f, 0.7f), 28.0f);
	}
	DRLook::PlayAt(this, TEXT("pickup"), GetActorLocation(), -6.0f, 0.1f);
	Piece->Destroy();
}
