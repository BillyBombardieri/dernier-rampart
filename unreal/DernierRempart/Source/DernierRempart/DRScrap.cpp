#include "DRScrap.h"

#include "Components/StaticMeshComponent.h"
#include "DRCharacter.h"
#include "DRGameMode.h"
#include "DRLook.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace DRScrapRules
{
	constexpr float Lifetime = 30.0f;
	constexpr double Magnet = 250.0;      // cm
	constexpr double PullSpeed = 1400.0;  // cm/s
	constexpr double PickupRadius = 110.0;
}

ADRScrap::ADRScrap()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	Pile = CreateDefaultSubobject<USceneComponent>(TEXT("Tas"));
	Pile->SetupAttachment(RootComponent);
}

void ADRScrap::Setup(int32 InValue)
{
	Value = FMath::Max(1, InValue);
	// Posée sur le sol (le décor fixe), sous l'endroit où le zombie est tombé.
	FHitResult Hit;
	const FVector Here = GetActorLocation();
	FCollisionObjectQueryParams Ground(ECC_WorldStatic);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Here + FVector(0.0, 0.0, 150.0), Here - FVector(0.0, 0.0, 600.0), Ground))
	{
		SetActorLocation(Hit.ImpactPoint);
	}
	ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
		Piece->SetupAttachment(Pile);
		Piece->SetStaticMesh(Cube);
		if (Mode)
		{
			Piece->SetMaterial(0, Mode->GetScrapMaterial(Index == 0));
		}
		Piece->SetRelativeLocation(FVector(FMath::FRandRange(-10.0, 10.0), FMath::FRandRange(-10.0, 10.0), Index * 4.0));
		Piece->SetRelativeRotation(FRotator(FMath::FRandRange(-23.0, 23.0), FMath::FRandRange(0.0, 360.0), FMath::FRandRange(-23.0, 23.0)));
		Piece->SetRelativeScale3D(FVector(FMath::FRandRange(0.1, 0.3), FMath::FRandRange(0.12, 0.25), 0.05));
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->RegisterComponent();
	}
}

void ADRScrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age > DRScrapRules::Lifetime)
	{
		Destroy();
		return;
	}
	// Elle flotte et tourne doucement.
	Pile->SetRelativeLocationAndRotation(FVector(0.0, 0.0, 15.0 + FMath::Sin(Age * 3.0f) * 3.0), FRotator(0.0, FMath::RadiansToDegrees(Age * 0.8f), 0.0));

	ADRCharacter* Player = Cast<ADRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Player || !Player->IsAlive())
	{
		return;
	}
	const FVector Target = Player->GetActorLocation();
	FVector Offset = Target - GetActorLocation();
	const double Height = FMath::Abs(Offset.Z);
	Offset.Z = 0.0;
	const double Distance = Offset.Size();
	if (Distance < DRScrapRules::PickupRadius && Height < 250.0)
	{
		if (ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>())
		{
			Mode->AddScrap(Value);
			Mode->NoteScrapPicked(Value);
		}
		if (USoundBase* Sound = DRLook::Sound(TEXT("pickup")))
		{
			UGameplayStatics::PlaySound2D(this, Sound, DRLook::Volume(-8.0f), 1.0f + FMath::FRandRange(-0.1f, 0.1f));
		}
		Destroy();
		return;
	}
	// Implant Charognard : la ferraille vient à toi de 12 m au lieu de 2,5 m.
	const ADRGameMode* Game = GetWorld()->GetAuthGameMode<ADRGameMode>();
	const double Reach = Game && Game->HasImplant(TEXT("charognard")) ? 1200.0 : DRScrapRules::Magnet;
	if (Distance < Reach)
	{
		AddActorWorldOffset(Offset.GetSafeNormal() * FMath::Min(Distance, DRScrapRules::PullSpeed * DeltaSeconds));
	}
}
