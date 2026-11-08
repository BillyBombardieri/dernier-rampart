#include "DRParticles.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "DRData.h"
#include "DRLook.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace DRParticleRules
{
	constexpr int32 FlameCapacity = 1500; // flammes en vol au plus, toutes sources confondues
	constexpr int32 SparkCapacity = 300;
	const FVector Parked(0.0, 0.0, -500.0); // exemplaires inutilisés : minuscules, sous le sol

	// Taille d'une flamme au cours de sa vie (Godot : courbes scale_amount_curve).
	float Taper(float T, bool bWidening)
	{
		if (bWidening)
		{
			// Jet du lance-flammes : 0,3 à la buse, 1,2 aux six dixièmes, 1,5 au bout.
			return T < 0.6f ? FMath::Lerp(0.3f, 1.2f, T / 0.6f) : FMath::Lerp(1.2f, 1.5f, (T - 0.6f) / 0.4f);
		}
		// Flamme : 0,7 à la naissance, 1 au quart de sa vie, 0,25 à la fin (elle s'affine en montant).
		return T < 0.25f ? FMath::Lerp(0.7f, 1.0f, T / 0.25f) : FMath::Lerp(1.0f, 0.25f, (T - 0.25f) / 0.75f);
	}

	UMaterialInterface* MakeLook(UObject* Outer, const TCHAR* AssetPath, const FLinearColor& ColorSRGB, float Opacity, float Glow)
	{
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, AssetPath, nullptr, LOAD_NoWarn);
		if (!Parent)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Result = UMaterialInstanceDynamic::Create(Parent, Outer);
		Result->SetVectorParameterValue(TEXT("Couleur"), DRData::ToLinear(ColorSRGB));
		Result->SetScalarParameterValue(TEXT("Opacite"), Opacity);
		Result->SetScalarParameterValue(TEXT("Eclat"), Glow);
		return Result;
	}
}

ADRParticles::ADRParticles()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	RootComponent = Root;
}

ADRParticles* ADRParticles::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	static TWeakObjectPtr<ADRParticles> Cached;
	if (Cached.IsValid() && Cached->GetWorld() == World)
	{
		return Cached.Get();
	}
	ADRParticles* Found = nullptr;
	for (TActorIterator<ADRParticles> It(World); It; ++It)
	{
		Found = *It;
		break;
	}
	if (!Found)
	{
		Found = World->SpawnActor<ADRParticles>(FVector::ZeroVector, FRotator::ZeroRotator);
	}
	Cached = Found;
	return Found;
}

void ADRParticles::Setup()
{
	bReady = true;
	Rng.Initialize(1789);
	BlobMesh = DRLook::BuildSphere(this, 10, 6);
	SpeckMesh = DRLook::BuildSphere(this, 4, 2); // petit losange
	// Couleurs du dégradé de Godot : jaune orangé au départ, rouge sombre à la fin.
	UMaterialInterface* Young = DRParticleRules::MakeLook(this, TEXT("/Game/Importe/Materiaux/M_Feu.M_Feu"), FLinearColor(1.0f, 0.65f, 0.2f), 1.0f, 2.5f);
	UMaterialInterface* Old = DRParticleRules::MakeLook(this, TEXT("/Game/Importe/Materiaux/M_Feu.M_Feu"), FLinearColor(0.75f, 0.25f, 0.05f), 1.0f, 1.2f);
	bFireLook = Young && Old;
	if (bFireLook)
	{
		YoungLayer = NewLayer(TEXT("FlammesJeunes"), BlobMesh, Young, DRParticleRules::FlameCapacity);
		OldLayer = NewLayer(TEXT("FlammesVieilles"), BlobMesh, Old, DRParticleRules::FlameCapacity);
	}
	UMaterialInterface* Violet = DRParticleRules::MakeLook(this, TEXT("/Game/Importe/Materiaux/M_Meteo.M_Meteo"), FLinearColor(0.65f, 0.6f, 1.0f), 1.0f, 6.0f);
	if (!Violet)
	{
		Violet = DRLook::Plain(this, FLinearColor(0.65f, 0.6f, 1.0f));
	}
	SparkLayer = NewLayer(TEXT("Etincelles"), SpeckMesh, Violet, DRParticleRules::SparkCapacity);
}

UInstancedStaticMeshComponent* ADRParticles::NewLayer(const TCHAR* LayerName, UStaticMesh* Mesh, UMaterialInterface* Material, int32 Capacity)
{
	UInstancedStaticMeshComponent* Layer = NewObject<UInstancedStaticMeshComponent>(this, LayerName);
	Layer->SetMobility(EComponentMobility::Movable);
	Layer->SetupAttachment(RootComponent);
	Layer->SetStaticMesh(Mesh);
	Layer->SetMaterial(0, Material);
	Layer->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Layer->SetCanEverAffectNavigation(false);
	Layer->SetCastShadow(false);
	Layer->bAffectDistanceFieldLighting = false;
	Layer->RegisterComponent();
	AddInstanceComponent(Layer);
	TArray<FTransform> Start;
	Start.Init(FTransform(FRotator::ZeroRotator, DRParticleRules::Parked, FVector(0.001)), Capacity);
	Layer->AddInstances(Start, false, true);
	return Layer;
}

int32 ADRParticles::Emit(float Rate, float DeltaSeconds)
{
	// Nombre de naissances de cette image : la partie fractionnaire est tirée au sort.
	return FMath::Max(0, FMath::FloorToInt(Rate * DeltaSeconds + Rng.FRand()));
}

void ADRParticles::Spawn(TArray<FDRSpark>& Pool, int32& Next, int32 Capacity, const FDRSpark& Spark)
{
	if (Pool.Num() < Capacity)
	{
		Pool.Add(Spark);
		return;
	}
	// Plein : on remplace une ancienne.
	Next = Next % Capacity;
	Pool[Next] = Spark;
	Next = (Next + 1) % Capacity;
}

void ADRParticles::Flames(const FVector& Origin, const FDRFlameStyle& Style, float Rate, float DeltaSeconds)
{
	if (!bReady)
	{
		Setup();
	}
	if (!bFireLook)
	{
		return;
	}
	const int32 Count = Emit(Rate, DeltaSeconds);
	const FVector Aim = Style.Direction.GetSafeNormal();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FDRSpark Flame;
		Flame.Location = Origin + Rng.VRand() * (Style.Radius * Rng.FRand());
		Flame.Velocity = Rng.VRandCone(Aim, FMath::DegreesToRadians(Style.Spread)) * Rng.FRandRange(Style.SpeedMin, Style.SpeedMax);
		Flame.Gravity = Style.Gravity;
		Flame.Damping = Style.Damping;
		Flame.Lifetime = Style.Lifetime;
		Flame.Size = Style.Size * Rng.FRandRange(0.6f, 1.4f);
		Flame.bWidening = Style.bWidening;
		Spawn(FlamePool, NextFlame, DRParticleRules::FlameCapacity, Flame);
	}
}

void ADRParticles::Sparks(const FVector& Origin, float Radius, float Rate, float DeltaSeconds)
{
	if (!bReady)
	{
		Setup();
	}
	const int32 Count = Emit(Rate, DeltaSeconds);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FDRSpark Spark;
		Spark.Location = Origin + Rng.VRand() * (Radius * Rng.FRand());
		Spark.Velocity = Rng.VRand() * Rng.FRandRange(100.0f, 300.0f);
		Spark.Lifetime = 0.25f;
		Spawn(SparkPool, NextSpark, DRParticleRules::SparkCapacity, Spark);
	}
}

void ADRParticles::AddFireSource(const FVector& Origin, const FDRFlameStyle& Style, float Rate)
{
	if (!bReady)
	{
		Setup();
	}
	FDRFireSource Source;
	Source.Origin = Origin;
	Source.Style = Style;
	Source.Rate = Rate;
	Sources.Add(Source);
}

bool ADRParticles::CanDrawFire()
{
	if (!bReady)
	{
		Setup();
	}
	return bFireLook;
}

UStaticMesh* ADRParticles::GetRingMesh()
{
	if (!RingMesh)
	{
		// Godot : tore de 22 à 26 cm de rayon.
		RingMesh = DRLook::BuildTorus(this, 24.0f, 2.0f, 24, 6);
	}
	return RingMesh;
}

void ADRParticles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bReady)
	{
		return;
	}
	const float Step = FMath::Min(DeltaSeconds, 0.1f);
	for (const FDRFireSource& Source : Sources)
	{
		Flames(Source.Origin, Source.Style, Source.Rate, Step);
	}

	YoungFrame.Reset();
	OldFrame.Reset();
	for (int32 Index = FlamePool.Num() - 1; Index >= 0; --Index)
	{
		FDRSpark& Flame = FlamePool[Index];
		Flame.Age += Step;
		if (Flame.Age >= Flame.Lifetime)
		{
			FlamePool.RemoveAtSwap(Index);
			continue;
		}
		Flame.Velocity += Flame.Gravity * Step;
		if (Flame.Damping > 0.0f)
		{
			// Le jet ralentit vite (Godot : damping).
			const double Speed = Flame.Velocity.Size();
			if (Speed > 1.0)
			{
				Flame.Velocity *= FMath::Max(0.0, Speed - Flame.Damping * Step) / Speed;
			}
		}
		Flame.Location += Flame.Velocity * Step;
		const float T = Flame.Age / Flame.Lifetime;
		const FTransform Where(FRotator::ZeroRotator, Flame.Location, FVector(Flame.Size * DRParticleRules::Taper(T, Flame.bWidening) / 100.0f));
		(T < 0.4f ? YoungFrame : OldFrame).Add(Where);
	}

	SparkFrame.Reset();
	for (int32 Index = SparkPool.Num() - 1; Index >= 0; --Index)
	{
		FDRSpark& Spark = SparkPool[Index];
		Spark.Age += Step;
		if (Spark.Age >= Spark.Lifetime)
		{
			SparkPool.RemoveAtSwap(Index);
			continue;
		}
		Spark.Location += Spark.Velocity * Step;
		// Petits bâtonnets de 3 x 3 x 12 cm (Godot : BoxMesh des étincelles).
		SparkFrame.Add(FTransform(FRotator::ZeroRotator, Spark.Location, FVector(0.03, 0.03, 0.12)));
	}

	Fill(YoungLayer, YoungFrame, DRParticleRules::FlameCapacity, ShownYoung);
	Fill(OldLayer, OldFrame, DRParticleRules::FlameCapacity, ShownOld);
	Fill(SparkLayer, SparkFrame, DRParticleRules::SparkCapacity, ShownSparks);
}

void ADRParticles::Fill(UInstancedStaticMeshComponent* Layer, TArray<FTransform>& Frame, int32 Capacity, int32& Shown)
{
	const int32 Used = FMath::Min(Frame.Num(), Capacity);
	if (!Layer || (Used == 0 && Shown == 0))
	{
		return; // rien à montrer, rien à cacher
	}
	Shown = Used;
	const FTransform Hidden(FRotator::ZeroRotator, DRParticleRules::Parked, FVector(0.001));
	Frame.SetNum(Capacity);
	for (int32 Index = Used; Index < Capacity; ++Index)
	{
		Frame[Index] = Hidden;
	}
	Layer->BatchUpdateInstancesTransforms(0, Frame, true, true, true);
	Layer->UpdateBounds();
}
