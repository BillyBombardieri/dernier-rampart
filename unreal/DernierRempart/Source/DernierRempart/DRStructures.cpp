#include "DRStructures.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DRGameMode.h"
#include "DRLook.h"
#include "Engine/World.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace DRStructureMeshes
{
	// Partie finie : les structures ne prennent plus de dégâts (Godot : structure.gd).
	bool GameOver(const AActor* Actor)
	{
		const UWorld* World = Actor->GetWorld();
		const ADRGameMode* Mode = World ? World->GetAuthGameMode<ADRGameMode>() : nullptr;
		return Mode && Mode->IsOver();
	}

	UStaticMesh* Cube()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}

	UStaticMesh* Cylinder()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	}
}

// ---------------------------------------------------------------- barrière

ADRBarrier::ADRBarrier()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Root;
	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocage"));
	Blocker->SetupAttachment(Root);
	Blocker->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	// Comme dans Godot, les balles passent au-dessus et à travers : on tire par-dessus la barrière.
	Blocker->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Blocker->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void ADRBarrier::Build(const FString& InName, float InHp, const FVector& Direction, double Width, double InProgress)
{
	DisplayName = InName;
	MaxHp = InHp;
	Hp = InHp;
	Dir = Direction.GetSafeNormal2D();
	HalfWidth = Width * 0.5;
	ProgressValue = InProgress;

	// Coordonnées locales de Godot en mètres : X = largeur, Y = hauteur, Z = vers la base.
	// L'acteur est tourné vers Dir : son X local = Z de Godot, son Y local = X de Godot.
	SetActorRotation(FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.0));
	const double W = Width / 100.0;
	Blocker->SetBoxExtent(FVector(45.0, Width * 0.5, 125.0));
	Blocker->SetRelativeLocation(FVector(0.0, 0.0, 125.0));

	UStaticMesh* Cube = DRStructureMeshes::Cube();
	UStaticMesh* Cyl = DRStructureMeshes::Cylinder();
	auto At = [](double X, double Y, double Z) { return FVector(Z, -X, Y) * 100.0; };
	auto GodotBox = [&](UMaterialInterface* Material, double X, double Y, double Z, double SizeX, double SizeY, double SizeZ, bool bHeavy)
	{
		AddPart(Cube, Material, At(X, Y, Z), FVector(SizeZ, SizeX, SizeY) * 100.0, FRotator::ZeroRotator, bHeavy);
	};
	UMaterialInterface* Concrete = DRLook::Textured(this, TEXT("concrete"), 0.6f, FLinearColor(0.86f, 0.84f, 0.8f));
	UMaterialInterface* Metal = DRLook::Textured(this, TEXT("metal"), 1.5f, FLinearColor(0.55f, 0.55f, 0.55f));
	UMaterialInterface* Reflector = DRLook::Textured(this, TEXT("metal"), 1.0f, FLinearColor(0.9f, 0.12f, 0.08f), 0.6f);

	// Blocs de béton type « glissière » posés côte à côte, avec une bande réfléchissante.
	const int32 Count = FMath::CeilToInt(W / 2.2);
	const double BlockLength = W / Count;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const double X = -W * 0.5 + BlockLength * (Index + 0.5);
		GodotBox(Concrete, X, 0.225, -0.05, BlockLength - 0.06, 0.45, 0.72, true);
		GodotBox(Concrete, X, 0.72, -0.05, BlockLength - 0.06, 0.55, 0.32, true);
		GodotBox(Reflector, X, 0.85, -0.22, BlockLength * 0.5, 0.08, 0.02, true);
	}
	// Poteaux du grillage et deux fils tendus.
	const int32 Posts = FMath::CeilToInt(W / 1.8) + 1;
	for (int32 Index = 0; Index < Posts; ++Index)
	{
		AddPart(Cyl, Metal, At(-W * 0.5 + W * Index / (Posts - 1), 1.25, 0.28), FVector(10.0, 10.0, 250.0), FRotator::ZeroRotator, false);
	}
	for (const double Y : { 0.95, 2.3 })
	{
		GodotBox(Metal, 0.0, Y, 0.28, W, 0.03, 0.03, false);
	}
	// Barrière levante rayée rouge et blanc, devant les blocs.
	const int32 Stripes = 10;
	const double StripeLength = (W + 0.4) / Stripes;
	UMaterialInterface* Red = DRLook::Plain(this, FLinearColor(0.75f, 0.08f, 0.06f));
	UMaterialInterface* White = DRLook::Plain(this, FLinearColor(0.9f, 0.9f, 0.88f));
	for (int32 Index = 0; Index < Stripes; ++Index)
	{
		const double X = -W * 0.5 - 0.2 + StripeLength * (Index + 0.5);
		AddPart(Cyl, Index % 2 == 0 ? Red : White, At(X, 1.15, -0.55), FVector(12.0, 12.0, StripeLength * 100.0), FRotator(0.0, 0.0, 90.0), false);
	}
	GodotBox(Metal, -W * 0.5 - 0.35, 0.575, -0.55, 0.3, 1.15, 0.3, true);

	// Position une fois effondrée : le grillage et les fils tombent à plat, les blocs glissent et pivotent un peu.
	for (UStaticMeshComponent* Part : Parts)
	{
		Rest.Add(Part->GetRelativeTransform());
		const bool bHeavy = Part->ComponentHasTag(TEXT("Lourd"));
		const double Tilt = FMath::RadiansToDegrees(bHeavy ? FMath::FRandRange(0.15, 0.35) : FMath::FRandRange(1.15, 1.5)) * (FMath::FRand() < 0.5f ? 1.0 : -1.0);
		FTransform Down = Part->GetRelativeTransform();
		const FQuat Turn = FQuat(FRotator(0.0, FMath::FRandRange(-28.0, 28.0), 0.0)) * FQuat(FRotator(Tilt, 0.0, 0.0));
		Down.SetRotation(Turn * Down.GetRotation());
		FVector Location = Down.GetLocation();
		Location += FVector(FMath::FRandRange(-60.0, 60.0), FMath::FRandRange(-30.0, 30.0), -Location.Z * 0.85);
		Down.SetLocation(Location);
		Fallen.Add(Down);
	}
}

UStaticMeshComponent* ADRBarrier::AddPart(UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Center, const FVector& Scale, const FRotator& Rotation, bool bHeavy)
{
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetupAttachment(RootComponent);
	Part->SetStaticMesh(Mesh);
	Part->SetMaterial(0, Material);
	Part->SetRelativeTransform(FTransform(Rotation, Center, Scale / 100.0));
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (bHeavy)
	{
		Part->ComponentTags.Add(TEXT("Lourd"));
	}
	Part->RegisterComponent();
	Parts.Add(Part);
	return Part;
}

bool ADRBarrier::IsBehind(const FVector& Point) const
{
	return FVector::DotProduct(Point - GetActorLocation(), Dir) > 0.0;
}

double ADRBarrier::FrontDistance(const FVector& Point) const
{
	FVector Offset = Point - GetActorLocation();
	Offset.Z = 0.0;
	return -FVector::DotProduct(Offset, Dir);
}

FVector ADRBarrier::ContactPoint(const FVector& Point) const
{
	const FVector Side(Dir.Y, -Dir.X, 0.0);
	const double Lateral = FMath::Clamp(FVector::DotProduct(Point - GetActorLocation(), Side), -HalfWidth + 40.0, HalfWidth - 40.0);
	return GetActorLocation() + Side * Lateral;
}

float ADRBarrier::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!bAlive || DamageAmount <= 0.0f || DRStructureMeshes::GameOver(this))
	{
		return 0.0f;
	}
	Hp = FMath::Max(0.0f, Hp - DamageAmount);
	ShakeAmount = FMath::Min(1.0f, ShakeAmount + 0.35f);
	if (FMath::FRand() < 0.35f)
	{
		DRLook::PlayAt(this, TEXT("barrier_hit"), GetActorLocation() + FVector(0.0, 0.0, 100.0), -5.0f, 0.2f);
	}
	if (Hp <= 0.0f)
	{
		Break();
	}
	return DamageAmount;
}

void ADRBarrier::Break()
{
	bAlive = false;
	FallAlpha = 0.0f;
	// Les débris ne bloquent plus le passage.
	Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DRLook::PlayAt(this, TEXT("barrier_break"), GetActorLocation() + FVector(0.0, 0.0, 100.0), 4.0f, 0.1f);
}

void ADRBarrier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bAlive && FallAlpha < 1.0f)
	{
		// Effondrement en une demi-seconde, de plus en plus vite.
		FallAlpha = FMath::Min(1.0f, FallAlpha + DeltaSeconds * 2.0f);
		const float Ease = FallAlpha * FallAlpha;
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			FTransform Blend;
			Blend.Blend(Rest[Index], Fallen[Index], Ease);
			Parts[Index]->SetRelativeTransform(Blend);
		}
	}
	else if (bAlive && ShakeAmount > 0.0f)
	{
		// Elle tremble sous les coups.
		ShakeAmount = FMath::Max(0.0f, ShakeAmount - DeltaSeconds * 3.0f);
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			FTransform Shaken = Rest[Index];
			Shaken.AddToTranslation(FVector(FMath::FRandRange(-1.0, 1.0), FMath::FRandRange(-1.0, 1.0), 0.0) * ShakeAmount * 3.0);
			Parts[Index]->SetRelativeTransform(Shaken);
		}
	}
}

// ---------------------------------------------------------------- Cœur

ADRCore::ADRCore()
{
	PrimaryActorTick.bCanEverTick = false;
	UBoxComponent* Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Bunker"));
	RootComponent = Box;
	Box->SetBoxExtent(FVector(200.0, 200.0, 175.0));
	// Le bunker dessiné par la carte bloque déjà le passage : cette boîte sert seulement de cible.
	Box->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	// Lumière bleue qui montre que le Cœur est alimenté ; elle s'éteint s'il tombe.
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Voyant"));
	Light->SetupAttachment(Box);
	Light->SetRelativeLocation(FVector(300.0, 0.0, 85.0));
	Light->IntensityUnits = ELightUnits::Candelas;
	Light->Intensity = 60.0f;
	Light->AttenuationRadius = 700.0f;
	Light->LightColor = FColor(77, 153, 255);
	Light->CastShadows = false;
}

float ADRCore::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!bAlive || DamageAmount <= 0.0f || DRStructureMeshes::GameOver(this))
	{
		return 0.0f;
	}
	Hp = FMath::Max(0.0f, Hp - DamageAmount);
	if (FMath::FRand() < 0.3f)
	{
		DRLook::PlayAt(this, TEXT("structure_hit"), GetActorLocation(), -6.0f, 0.2f);
	}
	if (Hp <= 0.0f)
	{
		bAlive = false;
		Light->SetVisibility(false);
		DRLook::PlayAt(this, TEXT("structure_hit"), GetActorLocation(), 6.0f, 0.0f);
		if (ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>())
		{
			Mode->CoreDestroyed();
		}
	}
	return DamageAmount;
}
