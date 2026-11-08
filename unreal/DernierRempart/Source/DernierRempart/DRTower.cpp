#include "DRTower.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DRData.h"
#include "DRGameMode.h"
#include "DRLook.h"
#include "DRZombie.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace DRTowerParts
{
	constexpr float ModelScale = 0.75f; // les tours étaient un peu trop massives (Godot : MODEL_SCALE)
	constexpr int32 TracerCount = 6;

	// Godot (x droite, y haut, z arrière, en mètres) -> Unreal (X avant, Y droite, Z haut, en cm).
	FVector At(double X, double Y, double Z)
	{
		return FVector(-Z, X, Y) * 100.0;
	}

	// Boîte de Godot (largeur, hauteur, profondeur en m) -> échelle du cube d'Unreal (1 m de côté).
	FVector BoxScale(double X, double Y, double Z)
	{
		return FVector(Z, X, Y);
	}

	// Cylindre de Godot (rayon, hauteur en m) -> échelle du cylindre d'Unreal (1 m de diamètre et de haut).
	FVector CylinderScale(double Radius, double Height)
	{
		return FVector(Radius * 2.0, Radius * 2.0, Height);
	}

	UStaticMesh* Mesh(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Name, Name));
	}

	TArray<ADRZombie*> LivingZombies(UWorld* World)
	{
		TArray<ADRZombie*> Found;
		for (TActorIterator<ADRZombie> It(World); It; ++It)
		{
			// Un Fouisseur sous terre est hors d'atteinte.
			if (!It->IsDead() && !It->IsBurrowed())
			{
				Found.Add(*It);
			}
		}
		return Found;
	}

	bool GameOver(const UWorld* World)
	{
		const ADRGameMode* Mode = World ? World->GetAuthGameMode<ADRGameMode>() : nullptr;
		return Mode && Mode->IsOver();
	}
}

// ---------------------------------------------------------------- ancrage

ADRSocket::ADRSocket()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
}

ADRTower* ADRSocket::GetTower() const
{
	return Tower;
}

bool ADRSocket::Build(const FString& Type)
{
	ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	const TSharedPtr<FJsonObject> Stats = Mode ? Mode->GetTowerData(Type) : TSharedPtr<FJsonObject>();
	if (Tower || !Stats.IsValid() || !Mode->TrySpend((int32)DRData::Number(Stats, TEXT("cost"), 25.0)))
	{
		return false;
	}
	Tower = GetWorld()->SpawnActor<ADRTower>(ADRTower::StaticClass(), GetActorLocation() + FVector(0.0, 0.0, 30.0), FRotator::ZeroRotator);
	if (!Tower)
	{
		return false;
	}
	Tower->Setup(Type, this);
	if (!Tower->IsPowered())
	{
		Mode->Say(TEXT("Plus assez d'\u00E9nergie : la tour est pos\u00E9e mais \u00E9teinte (X pour couper une autre tour)."));
	}
	return true;
}

// ---------------------------------------------------------------- tour

ADRTower::ADRTower()
{
	PrimaryActorTick.bCanEverTick = true;
	// La tour bloque le passage, mais les balles du joueur la traversent (comme dans Godot).
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Corps"));
	RootComponent = Body;
	Body->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Model = CreateDefaultSubobject<USceneComponent>(TEXT("Modele"));
	Model->SetupAttachment(Body);
	Model->SetRelativeScale3D(FVector(DRTowerParts::ModelScale));
	Head = CreateDefaultSubobject<USceneComponent>(TEXT("Tete"));
	Head->SetupAttachment(Model);
	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Bouche"));
	Muzzle->SetupAttachment(Head);

	FlashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Eclair"));
	FlashLight->SetupAttachment(Muzzle);
	FlashLight->IntensityUnits = ELightUnits::Candelas;
	FlashLight->Intensity = 0.0f;
	FlashLight->AttenuationRadius = 600.0f;
	FlashLight->LightColor = FColor(255, 178, 102);
	FlashLight->CastShadows = false;
}

void ADRTower::Setup(const FString& InType, ADRSocket* InSocket)
{
	Type = InType;
	Socket = InSocket;
	ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	const TSharedPtr<FJsonObject> Stats = Mode ? Mode->GetTowerData(Type) : TSharedPtr<FJsonObject>();
	if (Stats.IsValid())
	{
		Stats->TryGetStringField(TEXT("name"), DisplayName);
		Cost = (int32)DRData::Number(Stats, TEXT("cost"), Cost);
		Energy = (int32)DRData::Number(Stats, TEXT("energy"), Energy);
		BaseRange = (float)DRData::Number(Stats, TEXT("range"), BaseRange);
		RangeUp = (float)DRData::Number(Stats, TEXT("range_up"), 2.0);
		MinRange = (float)DRData::Number(Stats, TEXT("min_range"), 0.0);
		Rate = (float)DRData::Number(Stats, TEXT("rate"), Rate);
		// Amélioration permanente « Ingénieur » : +5 % de dégâts par rang.
		BaseDamage = (float)DRData::Number(Stats, TEXT("damage"), BaseDamage) * Mode->UpgradeMult(TEXT("ingenieur"));
		FreezeTime = (float)DRData::Number(Stats, TEXT("freeze"), FreezeTime);
		BurnDps = (float)DRData::Number(Stats, TEXT("burn"), BurnDps);
		Chain = (int32)DRData::Number(Stats, TEXT("chain"), Chain);
		BlastRadius = (float)DRData::Number(Stats, TEXT("radius"), BlastRadius);
		Color = DRData::ColorSRGB(Stats, TEXT("color"), Color);
	}
	if (DisplayName.IsEmpty())
	{
		DisplayName = Type;
	}
	if (Mode)
	{
		MaxLevel = (int32)Mode->Rule(TEXT("tours_regles"), TEXT("max_level"), 3.0);
		MarkBonus = (float)Mode->Rule(TEXT("tours_regles"), TEXT("mark_bonus"), 1.25);
		BeaconReveal = (float)Mode->Rule(TEXT("tours_regles"), TEXT("beacon_reveal"), 24.0);
	}
	const double Height = (Type == TEXT("beacon") ? 3.8 : 2.2) * DRTowerParts::ModelScale * 100.0;
	const double Half = 0.7 * DRTowerParts::ModelScale * 100.0;
	Body->SetBoxExtent(FVector(Half, Half, Height * 0.5));
	// L'acteur est posé sur la dalle : on remonte la boîte pour que son bas touche la dalle.
	Body->SetRelativeLocation(FVector::ZeroVector);
	AddActorWorldOffset(FVector(0.0, 0.0, Height * 0.5));
	Model->SetRelativeLocation(FVector(0.0, 0.0, -Height * 0.5));
	BuildVisual();
	bPowered = Mode && Mode->RequestEnergy(Energy);
	DRLook::PlayAt(this, TEXT("build"), GetActorLocation(), -2.0f, 0.1f);
}

UStaticMeshComponent* ADRTower::Part(USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Location, const FVector& Scale, const FRotator& Rotation)
{
	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
	Piece->SetupAttachment(Parent);
	Piece->SetStaticMesh(Mesh);
	Piece->SetMaterial(0, Material);
	Piece->SetRelativeTransform(FTransform(Rotation, Location, Scale));
	Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Piece->RegisterComponent();
	return Piece;
}

void ADRTower::BuildVisual()
{
	// Mêmes pièces que _build_visual dans tower.gd (positions en mètres de Godot).
	using namespace DRTowerParts;
	UStaticMesh* Cube = Mesh(TEXT("Cube"));
	UStaticMesh* Cyl = Mesh(TEXT("Cylinder"));
	UStaticMesh* Sphere = Mesh(TEXT("Sphere"));
	UStaticMesh* Cone = Mesh(TEXT("Cone"));
	UMaterialInterface* Metal = DRLook::Textured(this, TEXT("metal"), 1.2f, FLinearColor(0.85f, 0.85f, 0.85f));
	UMaterialInterface* DarkMetal = DRLook::Textured(this, TEXT("metal"), 1.2f, FLinearColor(0.45f, 0.45f, 0.45f));
	UMaterialInterface* Glow = DRLook::Textured(this, TEXT("metal"), 1.0f, Color, 3.0f);
	const FRotator Forward(90.0, 0.0, 0.0); // cylindre couché vers l'avant (Godot : rotation x de 90°)
	const FRotator Sideways(0.0, 0.0, 90.0); // cylindre couché sur le côté (Godot : rotation z de 90°)

	// Socle trapu commun à toutes les tours, et voyant vert quand la tour est allumée.
	Part(Model, Cube, DarkMetal, At(0.0, 0.25, 0.0), BoxScale(1.4, 0.5, 1.4));
	Lamp = Part(Model, Cube, DRLook::Textured(this, TEXT("metal"), 1.0f, FLinearColor(0.1f, 1.0f, 0.1f), 4.0f), At(0.5, 0.55, 0.5), BoxScale(0.12, 0.12, 0.12));

	auto Column = [&](double Height)
	{
		Part(Model, Cyl, Metal, At(0.0, 0.5 + Height * 0.5, 0.0), CylinderScale(0.35, Height));
	};
	FVector MuzzleAt = At(0.0, 0.05, -1.7);
	if (Type == TEXT("gun") || Type == TEXT("cryo"))
	{
		Column(1.0);
		Head->SetRelativeLocation(At(0.0, 1.7, 0.0));
		Part(Head, Cube, Metal, FVector::ZeroVector, BoxScale(0.9, 0.55, 1.0));
		if (Type == TEXT("gun"))
		{
			for (const double X : { -0.15, 0.15 })
			{
				Part(Head, Cyl, DarkMetal, At(X, 0.05, -1.0), CylinderScale(0.06, 1.3), Forward);
			}
			Part(Head, Cube, DarkMetal, At(0.6, -0.05, 0.1), BoxScale(0.35, 0.35, 0.5));
		}
		else
		{
			Part(Head, Cyl, DRLook::Textured(this, TEXT("metal"), 1.0f, Color, 1.5f), At(0.0, 0.45, 0.2), CylinderScale(0.28, 0.9), Sideways);
			Part(Head, Cyl, DarkMetal, At(0.0, 0.0, -0.9), CylinderScale(0.15, 0.9), Forward);
		}
	}
	else if (Type == TEXT("flame"))
	{
		Column(0.9);
		Head->SetRelativeLocation(At(0.0, 1.55, 0.0));
		Part(Head, Cube, Metal, FVector::ZeroVector, BoxScale(0.8, 0.5, 0.9));
		// Réservoirs de carburant rouges sur les côtés, buse devant, veilleuse bleue.
		UMaterialInterface* Red = DRLook::Textured(this, TEXT("metal"), 1.0f, FLinearColor(0.55f, 0.1f, 0.07f));
		for (const double X : { -0.55, 0.55 })
		{
			Part(Head, Cyl, Red, At(X, 0.05, 0.15), CylinderScale(0.2, 0.85), Forward);
		}
		Part(Head, Cyl, DarkMetal, At(0.0, 0.0, -0.85), CylinderScale(0.12, 0.9), Forward);
		Part(Head, Cyl, DRLook::Textured(this, TEXT("metal"), 1.0f, FLinearColor(0.4f, 0.6f, 1.0f), 4.0f), At(0.0, -0.12, -1.28), CylinderScale(0.05, 0.06));
		MuzzleAt = At(0.0, 0.0, -1.35);
		// Jet de feu : un cône lumineux (pointe sur la buse) qui vacille, et une lumière orangée devant.
		FlameJet = Part(Muzzle, Cone, DRLook::Textured(this, TEXT("metal"), 1.0f, FLinearColor(1.0f, 0.45f, 0.12f), 12.0f), FVector::ZeroVector, FVector(1.0), FRotator(90.0, 0.0, 0.0));
		FlameJet->SetCastShadow(false);
		FlameJet->SetVisibility(false);
		FlameLight = NewObject<UPointLightComponent>(this);
		FlameLight->SetupAttachment(Muzzle);
		FlameLight->SetRelativeLocation(FVector(250.0, 0.0, 0.0));
		FlameLight->IntensityUnits = ELightUnits::Candelas;
		FlameLight->SetIntensity(0.0f);
		FlameLight->SetAttenuationRadius(700.0f);
		FlameLight->SetLightColor(FLinearColor(1.0f, 0.55f, 0.2f));
		FlameLight->SetCastShadows(false);
		FlameLight->RegisterComponent();
	}
	else if (Type == TEXT("arc"))
	{
		// Bobine Tesla : noyau, anneaux de cuivre et sphère qui luit. La tête ne tourne pas.
		Head->SetRelativeLocation(At(0.0, 0.5, 0.0));
		UMaterialInterface* Copper = DRLook::Textured(this, TEXT("metal"), 1.0f, FLinearColor(0.72f, 0.42f, 0.25f));
		Part(Head, Cyl, DarkMetal, At(0.0, 0.8, 0.0), CylinderScale(0.16, 1.6));
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Part(Head, Cyl, Copper, At(0.0, 0.4 + Index * 0.32, 0.0), CylinderScale(0.34 - Index * 0.03, 0.08));
		}
		Part(Head, Sphere, Glow, At(0.0, 1.9, 0.0), FVector(0.6));
		UPointLightComponent* Orb = NewObject<UPointLightComponent>(this);
		Orb->SetupAttachment(Head);
		Orb->SetRelativeLocation(At(0.0, 1.9, 0.0));
		Orb->IntensityUnits = ELightUnits::Candelas;
		Orb->SetIntensity(30.0f);
		Orb->SetAttenuationRadius(400.0f);
		Orb->SetLightColor(Color);
		Orb->SetCastShadows(false);
		Orb->RegisterComponent();
		MuzzleAt = At(0.0, 1.9, 0.0);
	}
	else if (Type == TEXT("mortar"))
	{
		// Tube trapu incliné vers le ciel, sur une plaque, avec des caisses d'obus au pied.
		Part(Model, Cube, Metal, At(0.0, 0.58, 0.0), BoxScale(1.2, 0.15, 1.2));
		Head->SetRelativeLocation(At(0.0, 0.75, 0.0));
		USceneComponent* Pivot = NewObject<USceneComponent>(this);
		Pivot->SetupAttachment(Head);
		Pivot->SetRelativeRotation(FRotator(-55.0, 0.0, 0.0));
		Pivot->RegisterComponent();
		Part(Pivot, Cyl, DarkMetal, At(0.0, 0.55, 0.0), CylinderScale(0.19, 1.3));
		Part(Pivot, Cyl, Metal, At(0.0, 0.2, 0.0), CylinderScale(0.26, 0.1));
		for (const double X : { -0.3, 0.3 })
		{
			Part(Head, Cyl, Metal, At(X, 0.25, -0.35), CylinderScale(0.04, 0.9), FRotator(FMath::RadiansToDegrees(-0.6), 0.0, 0.0));
		}
		Part(Model, Cube, DRLook::Textured(this, TEXT("cloth"), 1.0f, FLinearColor(0.4f, 0.42f, 0.28f)), At(0.5, 0.65, 0.45), BoxScale(0.5, 0.3, 0.35));
		Muzzle->AttachToComponent(Pivot, FAttachmentTransformRules::KeepRelativeTransform);
		MuzzleAt = At(0.0, 1.2, 0.0);
	}
	else if (Type == TEXT("beacon"))
	{
		// Mât avec un projecteur qui tourne en permanence.
		Part(Model, Cyl, Metal, At(0.0, 2.1, 0.0), CylinderScale(0.1, 3.2));
		Head->SetRelativeLocation(At(0.0, 3.75, 0.0));
		Part(Head, Cyl, DarkMetal, FVector::ZeroVector, CylinderScale(0.28, 0.5), Forward);
		Part(Head, Cyl, DRLook::Textured(this, TEXT("metal"), 1.0f, Color, 5.0f), At(0.0, 0.0, -0.27), CylinderScale(0.24, 0.04), Forward);
		Beam = NewObject<USpotLightComponent>(this);
		Beam->SetupAttachment(Head);
		Beam->SetRelativeLocationAndRotation(At(0.0, 0.0, -0.3), FRotator(-12.0, 0.0, 0.0));
		Beam->IntensityUnits = ELightUnits::Candelas;
		Beam->SetIntensity(3000.0f);
		Beam->SetAttenuationRadius(3000.0f);
		Beam->SetOuterConeAngle(14.0f);
		Beam->SetInnerConeAngle(8.0f);
		Beam->SetLightColor(Color);
		Beam->SetVolumetricScatteringIntensity(3.0f);
		Beam->RegisterComponent();
		MuzzleAt = At(0.0, 0.0, -0.3);
	}
	Muzzle->SetRelativeLocation(MuzzleAt);

	// Traçantes : des traits lumineux étirés entre la bouche et la cible.
	UMaterialInterface* TracerLook = DRLook::Textured(this, TEXT("metal"), 1.0f, Type == TEXT("cryo") || Type == TEXT("arc") ? Color : FLinearColor(1.0f, 0.8f, 0.5f), 30.0f);
	for (int32 Index = 0; Index < DRTowerParts::TracerCount; ++Index)
	{
		UStaticMeshComponent* Line = NewObject<UStaticMeshComponent>(this);
		Line->SetupAttachment(Body);
		Line->SetUsingAbsoluteLocation(true);
		Line->SetUsingAbsoluteRotation(true);
		Line->SetUsingAbsoluteScale(true);
		Line->SetStaticMesh(Cube);
		Line->SetMaterial(0, TracerLook);
		Line->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Line->SetCastShadow(false);
		Line->SetVisibility(false);
		Line->RegisterComponent();
		Tracers.Add(Line);
		TracerLeft.Add(0.0f);
	}
}

// ---------------------------------------------------------------- chiffres

int32 ADRTower::GetUpgradeCost() const
{
	return FMath::RoundToInt(Cost * 0.6f + 15.0f) * Level;
}

float ADRTower::GetRange() const
{
	return (BaseRange + RangeUp * (Level - 1)) * RangeMult * 100.0f;
}

float ADRTower::GetDamage() const
{
	return BaseDamage * FMath::Pow(1.5f, (float)(Level - 1)) * DamageMult;
}

float ADRTower::HitDamage(const ADRZombie* Zombie) const
{
	// Un zombie marqué prend 25 % de dégâts en plus.
	return GetDamage() * (Zombie->IsMarked() ? MarkBonus : 1.0f);
}

FVector ADRTower::GetLabelAnchor() const
{
	return GetActorLocation() + FVector(0.0, 0.0, Body->GetUnscaledBoxExtent().Z + 40.0);
}

double ADRTower::FlatDistance(const FVector& Point) const
{
	return FVector::Dist2D(GetActorLocation(), Point);
}

void ADRTower::Upgrade()
{
	ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	if (!Mode)
	{
		return;
	}
	if (Level >= MaxLevel)
	{
		Mode->Say(TEXT("Niveau maximum atteint."));
		return;
	}
	if (!Mode->TrySpend(GetUpgradeCost()))
	{
		return;
	}
	++Level;
	Head->SetRelativeScale3D(FVector(1.0f + 0.15f * (Level - 1)));
	Mode->Say(FString::Printf(TEXT("%s niveau %d"), *DisplayName, Level));
	DRLook::PlayAt(this, TEXT("upgrade"), GetActorLocation(), -2.0f, 0.05f);
}

void ADRTower::TogglePower()
{
	ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	if (!Mode)
	{
		return;
	}
	if (bPowered)
	{
		bPowered = false;
		Mode->ReleaseEnergy(Energy);
	}
	else
	{
		bPowered = Mode->RequestEnergy(Energy);
		if (!bPowered)
		{
			Mode->Say(TEXT("Pas assez d'\u00E9nergie. Coupe une autre tour avec X."));
		}
	}
}

// ---------------------------------------------------------------- chaque image

void ADRTower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	Cooldown -= DeltaSeconds;
	Lamp->SetVisibility(bPowered);
	for (int32 Index = 0; Index < Tracers.Num(); ++Index)
	{
		if (TracerLeft[Index] > 0.0f)
		{
			TracerLeft[Index] -= DeltaSeconds;
			if (TracerLeft[Index] <= 0.0f)
			{
				Tracers[Index]->SetVisibility(false);
			}
		}
	}
	if (FlashLeft > 0.0f)
	{
		FlashLeft -= DeltaSeconds;
		if (FlashLeft <= 0.0f)
		{
			FlashLight->SetIntensity(0.0f);
		}
	}
	BonusCooldown -= DeltaSeconds;
	if (BonusCooldown <= 0.0f)
	{
		BonusCooldown = 0.5f;
		UpdateBeaconBonus();
	}
	if (FlameJet)
	{
		FlameOn -= DeltaSeconds;
		const bool bOn = FlameOn > 0.0f;
		FlameJet->SetVisibility(bOn);
		if (bOn)
		{
			// Le jet vacille : longueur et largeur bougent un peu à chaque image.
			const float Length = GetRange() / DRTowerParts::ModelScale / 100.0f * FMath::FRandRange(0.85f, 1.0f);
			const float Width = FMath::FRandRange(0.7f, 1.0f) * Length * 0.3f;
			FlameJet->SetRelativeLocation(FVector(Length * 50.0f, 0.0, 0.0));
			FlameJet->SetRelativeScale3D(FVector(Width, Width, Length));
		}
		FlameLight->SetIntensity(bOn ? FMath::FRandRange(300.0f, 500.0f) : 0.0f);
	}
	if (Type == TEXT("beacon"))
	{
		UpdateBeacon(DeltaSeconds);
		return;
	}
	if (!bPowered || DRTowerParts::GameOver(GetWorld()))
	{
		return;
	}
	TargetCooldown -= DeltaSeconds;
	if (TargetCooldown <= 0.0f || !IsValidTarget(Target.Get()))
	{
		TargetCooldown = 0.15f;
		Target = FindTarget();
	}
	ADRZombie* Victim = Target.Get();
	if (!Victim)
	{
		return;
	}
	const FVector Aim = Victim->GetActorLocation() + FVector(0.0, 0.0, (100.0 - 95.0) * Victim->GetSize());
	if (Type != TEXT("arc"))
	{
		// La tête se tourne vers la cible.
		const FVector Flat = Aim - Head->GetComponentLocation();
		if (Flat.Size2D() > 10.0)
		{
			Head->SetWorldRotation(FRotator(0.0, Flat.Rotation().Yaw, 0.0));
		}
	}
	if (Cooldown > 0.0f)
	{
		return;
	}
	Cooldown = Rate;
	if (Type == TEXT("gun"))
	{
		FireGun(Victim, Aim);
	}
	else if (Type == TEXT("cryo"))
	{
		FireCryo(Victim, Aim);
	}
	else if (Type == TEXT("flame"))
	{
		FireFlame();
	}
	else if (Type == TEXT("arc"))
	{
		FireArc(Victim);
	}
	else if (Type == TEXT("mortar"))
	{
		FireMortar(Victim);
	}
}

ADRZombie* ADRTower::FindTarget() const
{
	// Priorité : marqué > Hurleur > (Cryo) pas encore gelé > (Mortier) groupe dense > le plus avancé.
	ADRZombie* Best = nullptr;
	double BestScore = -1.0e12;
	for (ADRZombie* Zombie : DRTowerParts::LivingZombies(GetWorld()))
	{
		if (!IsValidTarget(Zombie))
		{
			continue;
		}
		double Score = Zombie->Progress();
		if (Zombie->IsMarked())
		{
			Score += 100000.0;
		}
		if (Zombie->GetZombieType() == TEXT("hurleur"))
		{
			Score += 60000.0;
		}
		if (Type == TEXT("cryo") && !Zombie->IsFrozen())
		{
			Score += 50000.0;
		}
		if (Type == TEXT("mortar"))
		{
			Score += 4000.0 * Neighbors(Zombie, BlastRadius * 100.0);
		}
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Zombie;
		}
	}
	return Best;
}

bool ADRTower::IsValidTarget(const ADRZombie* Zombie) const
{
	if (!IsValid(Zombie) || Zombie->IsDead() || Zombie->IsBurrowed())
	{
		return false;
	}
	const double Distance = FlatDistance(Zombie->GetActorLocation());
	return Distance <= GetRange() && Distance >= MinRange * 100.0;
}

int32 ADRTower::Neighbors(const ADRZombie* Zombie, double Radius) const
{
	int32 Count = 0;
	for (ADRZombie* Other : DRTowerParts::LivingZombies(GetWorld()))
	{
		if (Other != Zombie && FVector::Dist(Other->GetActorLocation(), Zombie->GetActorLocation()) < Radius)
		{
			++Count;
		}
	}
	return Count;
}

void ADRTower::ShowTracer(int32 Index, const FVector& From, const FVector& To, float Duration)
{
	if (!Tracers.IsValidIndex(Index))
	{
		return;
	}
	const double Length = FVector::Dist(From, To);
	if (Length < 20.0)
	{
		return;
	}
	Tracers[Index]->SetWorldLocationAndRotation((From + To) * 0.5, (To - From).Rotation());
	Tracers[Index]->SetWorldScale3D(FVector(Length / 100.0, 0.03, 0.03));
	Tracers[Index]->SetVisibility(true);
	TracerLeft[Index] = Duration;
}

void ADRTower::Flash(float Candelas)
{
	FlashLight->SetIntensity(Candelas);
	FlashLeft = 0.06f;
}

void ADRTower::FireGun(ADRZombie* Victim, const FVector& Aim)
{
	++Shots;
	ShowTracer(Shots % 2, Muzzle->GetComponentLocation(), Aim, 0.05f);
	if (Shots % 3 == 0)
	{
		Flash(400.0f);
		DRLook::PlayAt(this, TEXT("tower_gun"), Muzzle->GetComponentLocation(), -4.0f, 0.1f);
	}
	UGameplayStatics::ApplyDamage(Victim, HitDamage(Victim), nullptr, this, UDamageType::StaticClass());
}

void ADRTower::FireCryo(ADRZombie* Victim, const FVector& Aim)
{
	ShowTracer(0, Muzzle->GetComponentLocation(), Aim, 0.12f);
	DRLook::PlayAt(this, TEXT("cryo"), Muzzle->GetComponentLocation(), -4.0f, 0.1f);
	Victim->Freeze(FreezeTime + 0.5f * (Level - 1));
	UGameplayStatics::ApplyDamage(Victim, HitDamage(Victim), nullptr, this, UDamageType::StaticClass());
}

void ADRTower::FireFlame()
{
	// Brûle tous les zombies dans un cône de 28° devant la buse (et ceux collés à la tour).
	FlameOn = 0.3f;
	SoundCooldown -= Rate;
	if (SoundCooldown <= 0.0f)
	{
		SoundCooldown = 0.45f;
		DRLook::PlayAt(this, TEXT("flame"), Muzzle->GetComponentLocation(), -5.0f, 0.1f);
	}
	const FVector Origin = Head->GetComponentLocation();
	FVector Facing = Head->GetForwardVector();
	Facing.Z = 0.0;
	Facing.Normalize();
	const double Reach = GetRange();
	const double MinDot = FMath::Cos(FMath::DegreesToRadians(28.0));
	for (ADRZombie* Zombie : DRTowerParts::LivingZombies(GetWorld()))
	{
		FVector To = Zombie->GetActorLocation() - Origin;
		To.Z = 0.0;
		const double Distance = To.Size();
		if (Distance > Reach || Distance < 1.0)
		{
			continue;
		}
		if (FVector::DotProduct(Facing, To / Distance) < MinDot && Distance > 150.0)
		{
			continue;
		}
		Zombie->Ignite(3.0f, BurnDps * FMath::Pow(1.4f, (float)(Level - 1)));
		UGameplayStatics::ApplyDamage(Zombie, HitDamage(Zombie), nullptr, this, UDamageType::StaticClass());
	}
}

void ADRTower::FireArc(ADRZombie* Victim)
{
	// L'éclair saute de zombie en zombie (6 m au plus entre deux) et les charge.
	TArray<ADRZombie*> Hit = { Victim };
	const float Damage = HitDamage(Victim);
	const TArray<ADRZombie*> Living = DRTowerParts::LivingZombies(GetWorld());
	ADRZombie* Current = Victim;
	for (int32 Jump = 0; Jump < Chain + (Level - 1); ++Jump)
	{
		ADRZombie* Next = nullptr;
		double Best = 600.0;
		for (ADRZombie* Zombie : Living)
		{
			if (Hit.Contains(Zombie))
			{
				continue;
			}
			const double Distance = FVector::Dist(Current->GetActorLocation(), Zombie->GetActorLocation());
			if (Distance < Best)
			{
				Best = Distance;
				Next = Zombie;
			}
		}
		if (!Next)
		{
			break;
		}
		Hit.Add(Next);
		Current = Next;
	}
	FVector Previous = Muzzle->GetComponentLocation();
	for (int32 Index = 0; Index < Hit.Num(); ++Index)
	{
		ADRZombie* Zombie = Hit[Index];
		const FVector Point = Zombie->GetActorLocation() + FVector(0.0, 0.0, (120.0 - 95.0) * Zombie->GetSize());
		ShowTracer(Index, Previous, Point, 0.15f);
		Previous = Point;
		Zombie->Charge(4.0f);
		UGameplayStatics::ApplyDamage(Zombie, Damage * FMath::Pow(0.75f, (float)Index), nullptr, this, UDamageType::StaticClass());
	}
	Flash(600.0f);
	DRLook::PlayAt(this, TEXT("zap"), Muzzle->GetComponentLocation(), -3.0f, 0.15f);
}

void ADRTower::FireMortar(ADRZombie* Victim)
{
	// Obus en cloche, en anticipant la marche de la cible.
	const FVector From = Muzzle->GetComponentLocation();
	const double Distance = FVector::Dist(From, Victim->GetActorLocation()) / 100.0;
	const float Flight = 1.0f + (float)Distance / 28.0f * 0.8f;
	FVector Lead = Victim->GetVelocity() * Flight;
	Lead.Z = 0.0;
	FVector Aim = Victim->GetActorLocation() + Lead;
	Aim.Z = Victim->GetActorLocation().Z - Victim->GetSimpleCollisionHalfHeight() + 5.0;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ADRShell* Shell = GetWorld()->SpawnActor<ADRShell>(ADRShell::StaticClass(), From, FRotator::ZeroRotator, Params))
	{
		Shell->Launch(Aim, Flight, GetDamage(), (BlastRadius + 0.25f * (Level - 1)) * 100.0f, 1.2f);
	}
	Flash(800.0f);
	DRLook::PlayAt(this, TEXT("mortar_fire"), From, 0.0f, 0.1f, 10000.0f);
}

void ADRTower::UpdateBeacon(float DeltaSeconds)
{
	// Phare : la lumière tourne, et il marque régulièrement le zombie le plus avancé (ou le plus dangereux).
	if (bPowered)
	{
		Head->AddLocalRotation(FRotator(0.0, FMath::RadiansToDegrees(1.4f * DeltaSeconds), 0.0));
	}
	Beam->SetVisibility(bPowered);
	if (!bPowered || DRTowerParts::GameOver(GetWorld()))
	{
		return;
	}
	// Il débusque les Fouisseurs qui passent sous terre dans son halo.
	for (TActorIterator<ADRZombie> It(GetWorld()); It; ++It)
	{
		if (It->IsBurrowed() && FlatDistance(It->GetActorLocation()) < BeaconReveal * 100.0)
		{
			It->ForceSurface();
		}
	}
	RevealCooldown -= DeltaSeconds;
	if (RevealCooldown > 0.0f)
	{
		return;
	}
	RevealCooldown = Rate - (Level - 1);
	ADRZombie* Best = nullptr;
	double BestScore = -1.0e12;
	for (ADRZombie* Zombie : DRTowerParts::LivingZombies(GetWorld()))
	{
		if (Zombie->IsMarked() || FlatDistance(Zombie->GetActorLocation()) > BeaconReveal * 100.0)
		{
			continue;
		}
		const FString& Kind = Zombie->GetZombieType();
		const double Score = Zombie->Progress() + (Kind == TEXT("hurleur") || Kind == TEXT("brute") || Kind == TEXT("boss") ? 5000.0 : 0.0);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Zombie;
		}
	}
	if (Best)
	{
		Best->Mark();
		ShowTracer(0, Head->GetComponentLocation(), Best->GetActorLocation() + FVector(0.0, 0.0, (140.0 - 95.0) * Best->GetSize()), 0.12f);
		DRLook::PlayAt(this, TEXT("ping"), Head->GetComponentLocation(), -6.0f, 0.05f);
	}
}

void ADRTower::UpdateBeaconBonus()
{
	// Les tours dans le halo d'un Phare allumé gagnent de la portée et des dégâts (sans cumul).
	FVector2D Bonus(1.0f, 1.0f);
	if (Type != TEXT("beacon"))
	{
		for (TActorIterator<ADRTower> It(GetWorld()); It; ++It)
		{
			const ADRTower* Other = *It;
			if (Other->Type == TEXT("beacon") && Other->bPowered && FlatDistance(Other->GetActorLocation()) <= Other->GetRange())
			{
				const FVector2D Given = Other->GetBeaconBonus();
				Bonus = FVector2D(FMath::Max(Bonus.X, Given.X), FMath::Max(Bonus.Y, Given.Y));
			}
		}
	}
	RangeMult = (float)Bonus.X;
	DamageMult = (float)Bonus.Y;
}

// ---------------------------------------------------------------- obus

ADRShell::ADRShell()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	Ball = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Obus"));
	Ball->SetupAttachment(RootComponent);
	Ball->SetStaticMesh(SphereMesh.Object);
	Ball->SetRelativeScale3D(FVector(0.36, 0.26, 0.26));
	Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ball->SetCastShadow(false);
	Blast = CreateDefaultSubobject<UPointLightComponent>(TEXT("Explosion"));
	Blast->SetupAttachment(RootComponent);
	Blast->IntensityUnits = ELightUnits::Candelas;
	Blast->Intensity = 0.0f;
	Blast->AttenuationRadius = 1400.0f;
	Blast->LightColor = FColor(255, 153, 77);
	Blast->CastShadows = false;
}

void ADRShell::Launch(const FVector& To, float InFlight, float InDamage, float InRadius, float InStun)
{
	Flight = FMath::Max(0.2f, InFlight);
	Damage = InDamage;
	Radius = InRadius;
	StunTime = InStun;
	// Vitesse de départ pour arriver pile sur la cible : To = From + V*t + g*t²/2 (gravité de 14 m/s²).
	const FVector Gravity(0.0, 0.0, -1400.0);
	Velocity = (To - GetActorLocation() - 0.5 * Gravity * Flight * Flight) / Flight;
	Ball->SetMaterial(0, DRLook::Textured(this, TEXT("metal"), 1.0f, FLinearColor(0.12f, 0.12f, 0.1f)));
}

void ADRShell::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (BlastLeft >= 0.0f)
	{
		// Éclair de l'explosion, puis l'obus disparaît.
		BlastLeft -= DeltaSeconds;
		Blast->SetIntensity(FMath::Max(0.0f, BlastLeft / 0.12f) * 4000.0f);
		if (BlastLeft <= 0.0f)
		{
			Destroy();
		}
		return;
	}
	Age += DeltaSeconds;
	Velocity.Z -= 1400.0 * DeltaSeconds;
	AddActorWorldOffset(Velocity * DeltaSeconds);
	if (!Velocity.IsNearlyZero())
	{
		SetActorRotation(Velocity.Rotation());
	}
	if (Age >= Flight)
	{
		Explode(GetActorLocation());
	}
}

void ADRShell::Explode(const FVector& Where)
{
	Ball->SetVisibility(false);
	BlastLeft = 0.12f;
	DRLook::PlayAt(this, TEXT("explosion"), Where, 0.0f, 0.1f, 10000.0f);
	ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	// Zombies pris dans le souffle ; s'il y en a un en feu, c'est l'EMBRASEMENT.
	struct FVictim { ADRZombie* Zombie; double Distance; bool bBurning; };
	TArray<FVictim> Victims;
	bool bBurning = false;
	for (ADRZombie* Zombie : DRTowerParts::LivingZombies(GetWorld()))
	{
		const double Distance = FVector::Dist2D(Zombie->GetActorLocation(), Where);
		if (Distance <= Radius)
		{
			Victims.Add({ Zombie, Distance, Zombie->IsBurning() });
			bBurning = bBurning || Zombie->IsBurning();
		}
	}
	if (bBurning && Mode)
	{
		Mode->AddScore(10);
		Mode->Popup(TEXT("EMBRASEMENT !"), Where + FVector(0.0, 0.0, 250.0), FLinearColor(1.0f, 0.55f, 0.15f), 64.0f);
		for (ADRZombie* Zombie : DRTowerParts::LivingZombies(GetWorld()))
		{
			if (FVector::Dist2D(Zombie->GetActorLocation(), Where) <= Radius + 250.0)
			{
				Zombie->Ignite(4.0f, 10.0f * Mode->ComboScale());
			}
		}
	}
	for (const FVictim& Victim : Victims)
	{
		if (Victim.Zombie->IsDead())
		{
			continue;
		}
		const float Falloff = FMath::Lerp(1.0f, 0.4f, (float)(Victim.Distance / FMath::Max(Radius, 10.0f)));
		const float Amount = Damage * Falloff * (bBurning && Victim.bBurning ? 1.5f : 1.0f);
		Victim.Zombie->Stun(StunTime);
		UGameplayStatics::ApplyDamage(Victim.Zombie, Amount, nullptr, this, UDamageType::StaticClass());
	}
}
