#include "DRChapter.h"

#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "DRData.h"
#include "DRGameMode.h"
#include "DRLook.h"
#include "DRStructures.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

// Nommé (et pas anonyme) pour éviter les collisions quand Unreal regroupe les .cpp à la compilation.
namespace DRChapterLayout
{
	// Lumières. Godot n'a pas d'unités physiques : ces valeurs sont choisies pour garder le même
	// équilibre (projecteurs bien plus forts que le soleil couchant, braises orangées).
	constexpr float SunLuxPerEnergy = 10.0f;   // 10 lux = soleil par défaut d'Unreal
	constexpr float SpotCandelas = 2000.0f;
	constexpr float BarrelCandelas = 200.0f;
	constexpr float PortalCandelas = 300.0f;

	FRotator YawOf(const FVector& Direction)
	{
		return FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)), 0.0);
	}

	double DistanceToSegment2D(const FVector& P, const FVector& A, const FVector& B)
	{
		const FVector2D P2(P.X, P.Y);
		const FVector2D A2(A.X, A.Y);
		const FVector2D AB = FVector2D(B.X, B.Y) - A2;
		const double T = FMath::Clamp(FVector2D::DotProduct(P2 - A2, AB) / FMath::Max(AB.SizeSquared(), 1.0), 0.0, 1.0);
		return FVector2D::Distance(P2, A2 + AB * T);
	}
}

ADRChapter::ADRChapter()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	CubeMesh = Cube.Object;
	SphereMesh = Sphere.Object;
	CylinderMesh = Cyl.Object;
}

template <class T>
T* ADRChapter::NewPart(const TCHAR* InName, EComponentMobility::Type Mobility)
{
	T* Component = NewObject<T>(this, MakeUniqueObjectName(this, T::StaticClass(), FName(InName)));
	Component->SetMobility(Mobility);
	Component->SetupAttachment(RootComponent);
	return Component;
}

void ADRChapter::FinishPart(UActorComponent* Component)
{
	Component->RegisterComponent();
	AddInstanceComponent(Component);
}

// ---------------------------------------------------------------- construction

void ADRChapter::Build(int32 InChapter)
{
	const TSharedPtr<FJsonObject> Root = DRData::Load(TEXT("chapitres.json"));
	const TArray<TSharedPtr<FJsonValue>>* Chapters = nullptr;
	if (!Root.IsValid() || !Root->TryGetArrayField(TEXT("chapitres"), Chapters) || Chapters->Num() == 0)
	{
		// Sans les données, on pose au moins un sol et un ciel pour que le joueur ne tombe pas.
		BuildEnvironment(TSharedPtr<FJsonObject>());
		Box(Plain(FLinearColor(0.27f, 0.24f, 0.16f)), FVector(0.0, 0.0, -50.0), FVector(20000.0, 20000.0, 100.0));
		ChapterName = TEXT("Content/Data/chapitres.json introuvable");
		return;
	}
	Chapter = FMath::Clamp(InChapter, 0, Chapters->Num() - 1);
	const TSharedPtr<FJsonObject> Data = (*Chapters)[Chapter]->AsObject();
	Random.Initialize(2026 + Chapter);
	Data->TryGetStringField(TEXT("name"), ChapterName);
	Data->TryGetStringField(TEXT("place"), ChapterPlace);
	const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
	if (Data->TryGetArrayField(TEXT("path"), Points))
	{
		for (const TSharedPtr<FJsonValue>& Point : *Points)
		{
			Path.Add(DRData::Vector(Point));
		}
	}

	const TSharedPtr<FJsonObject>* Env = nullptr;
	BuildEnvironment(Data->TryGetObjectField(TEXT("env"), Env) ? *Env : TSharedPtr<FJsonObject>());
	BuildGround(Data);
	if (Path.Num() >= 2)
	{
		BuildPath(Data);
		BuildPortal();
	}
	BuildCore();
	BuildRings(Data);
	BuildWalls(Data);
	BuildLights(Data);
	BuildProps(Data);
}

ADRCore* ADRChapter::GetCore() const
{
	return Core;
}

void ADRChapter::BeginPlay()
{
	Super::BeginPlay();
	if (ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>())
	{
		Mode->Say(FString::Printf(TEXT("Chapitre %d : %s. %s"), Chapter + 1, *ChapterName, *ChapterPlace));
	}
	if (GEngine)
	{
		if (bMissingTextures)
		{
			GEngine->AddOnScreenDebugMessage(-1, 30.0f, FColor::Orange,
				TEXT("Textures pas encore import\u00E9es : tape  py importer_ressources.py  dans la barre Cmd (voir LISEZMOI, \u00E9tape 2)."));
		}
	}
}

void ADRChapter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Time += DeltaSeconds;
	// Flammes et lampe de secours qui vacillent (Godot : scripts/flicker.gd).
	for (int32 Index = 0; Index < FlickerLights.Num(); ++Index)
	{
		const FVector& Settings = FlickerSettings[Index];
		const float Noise = FMath::PerlinNoise1D(Time * (float)Settings.Y + Index * 17.3f) * 0.5f + 0.5f;
		FlickerLights[Index]->SetIntensity((float)Settings.X * (1.0f - (float)Settings.Z * Noise));
	}
}

// ---------------------------------------------------------------- matériaux et pièces

UMaterialInterface* ADRChapter::Look(const FString& Set, float Scale, const FLinearColor& TintSRGB, float Glow)
{
	const FString Key = FString::Printf(TEXT("%s|%.3f|%.3f|%.3f|%.3f|%.2f"), *Set, Scale, TintSRGB.R, TintSRGB.G, TintSRGB.B, Glow);
	if (const TObjectPtr<UMaterialInterface>* Found = Materials.Find(Key))
	{
		return *Found;
	}
	UMaterialInterface* Result = DRLook::Textured(this, Set, Scale, TintSRGB, Glow, &bMissingTextures);
	Materials.Add(Key, Result);
	return Result;
}

UMaterialInterface* ADRChapter::Look(const TSharedPtr<FJsonObject>& Data)
{
	FString Set = TEXT("ground");
	if (Data.IsValid())
	{
		Data->TryGetStringField(TEXT("tex"), Set);
	}
	return Look(Set, (float)DRData::Number(Data, TEXT("scale"), 0.25), DRData::ColorSRGB(Data, TEXT("tint")));
}

UMaterialInterface* ADRChapter::Plain(const FLinearColor& ColorSRGB)
{
	const FString Key = FString::Printf(TEXT("uni|%.3f|%.3f|%.3f"), ColorSRGB.R, ColorSRGB.G, ColorSRGB.B);
	if (const TObjectPtr<UMaterialInterface>* Found = Materials.Find(Key))
	{
		return *Found;
	}
	UMaterialInterface* Material = DRLook::Plain(this, ColorSRGB);
	Materials.Add(Key, Material);
	return Material;
}

void ADRChapter::Add(UStaticMesh* Mesh, UMaterialInterface* Material, const FTransform& Transform, bool bCollision)
{
	if (!Mesh)
	{
		return;
	}
	const FString Key = FString::Printf(TEXT("%s|%s|%d"), *Mesh->GetName(), *GetNameSafe(Material), bCollision ? 1 : 0);
	TObjectPtr<UInstancedStaticMeshComponent>* Found = Groups.Find(Key);
	UInstancedStaticMeshComponent* Group = Found ? Found->Get() : nullptr;
	if (!Group)
	{
		Group = NewPart<UInstancedStaticMeshComponent>(TEXT("Pieces"), EComponentMobility::Static);
		Group->SetStaticMesh(Mesh);
		Group->SetMaterial(0, Material);
		Group->SetCollisionProfileName(bCollision ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
		FinishPart(Group);
		Groups.Add(Key, Group);
	}
	Group->AddInstance(Transform, true);
}

void ADRChapter::Box(UMaterialInterface* Material, const FVector& Center, const FVector& Size, const FRotator& Rotation, bool bCollision)
{
	// Le cube du moteur mesure 100 cm de côté.
	Add(CubeMesh, Material, FTransform(Rotation, Center, Size / 100.0), bCollision);
}

void ADRChapter::Cylinder(UMaterialInterface* Material, const FVector& Center, double Radius, double Height, const FRotator& Rotation, bool bCollision)
{
	// Le cylindre du moteur fait 100 cm de diamètre et 100 cm de haut, debout sur l'axe Z.
	Add(CylinderMesh, Material, FTransform(Rotation, Center, FVector(Radius / 50.0, Radius / 50.0, Height / 100.0)), bCollision);
}

UPointLightComponent* ADRChapter::Flicker(const FVector& Location, const FLinearColor& ColorSRGB, float Candelas, float Radius, float Speed, float Amount, bool bShadows)
{
	UPointLightComponent* Light = NewPart<UPointLightComponent>(TEXT("Flamme"), EComponentMobility::Movable);
	Light->SetRelativeLocation(Location);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(Candelas);
	Light->SetLightColor(ColorSRGB);
	Light->SetAttenuationRadius(Radius);
	Light->SetCastShadows(bShadows);
	FinishPart(Light);
	if (Amount > 0.0f)
	{
		FlickerLights.Add(Light);
		FlickerSettings.Add(FVector(Candelas, Speed, Amount));
	}
	return Light;
}

// ---------------------------------------------------------------- ciel, sol et chemin

void ADRChapter::BuildEnvironment(const TSharedPtr<FJsonObject>& Env)
{
	// Soleil couchant ou lune selon le chapitre : direction, couleur et force viennent de Godot.
	UDirectionalLightComponent* Sun = NewPart<UDirectionalLightComponent>(TEXT("Soleil"), EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(DRData::Number(Env, TEXT("sun_pitch"), -35.0), DRData::Number(Env, TEXT("sun_yaw"), 45.0), 0.0));
	Sun->SetLightColor(DRData::ColorSRGB(Env, TEXT("sun_color")));
	Sun->SetIntensity(DRChapterLayout::SunLuxPerEnergy * (float)DRData::Number(Env, TEXT("sun_energy"), 1.0));
	Sun->SetAtmosphereSunLight(true);
	Sun->SetVolumetricScatteringIntensity(0.4f);
	FinishPart(Sun);

	// Lumière du ciel recalculée en temps réel, plus ou moins forte selon l'ambiance du chapitre.
	USkyLightComponent* Sky = NewPart<USkyLightComponent>(TEXT("LumiereDuCiel"), EComponentMobility::Movable);
	Sky->bRealTimeCapture = true;
	Sky->SetIntensity(FMath::Max(0.3f, 2.0f * (float)DRData::Number(Env, TEXT("ambient"), 0.5)));
	FinishPart(Sky);

	FinishPart(NewPart<USkyAtmosphereComponent>(TEXT("Atmosphere"), EComponentMobility::Movable));

	// Brouillard au sol et brouillard volumétrique qui prend la lumière des projecteurs.
	UExponentialHeightFogComponent* Fog = NewPart<UExponentialHeightFogComponent>(TEXT("Brouillard"), EComponentMobility::Movable);
	Fog->SetFogDensity(2.5f * (float)DRData::Number(Env, TEXT("fog"), 0.008));
	Fog->SetFogInscatteringColor(DRData::ToLinear(DRData::ColorSRGB(Env, TEXT("fog_color"), FLinearColor(0.13f, 0.14f, 0.17f))));
	Fog->SetVolumetricFog(true);
	Fog->SetVolumetricFogAlbedo(DRData::ColorSRGB(Env, TEXT("vol_albedo"), FLinearColor(0.6f, 0.62f, 0.68f)).ToFColor(false));
	Fog->SetVolumetricFogExtinctionScale(FMath::Max(0.2f, (float)DRData::Number(Env, TEXT("vol_fog"), 0.01) / 0.01f));
	FinishPart(Fog);

	// Saturation et contraste de l'image, comme les réglages d'ambiance de Godot.
	UPostProcessComponent* Post = NewPart<UPostProcessComponent>(TEXT("Image"), EComponentMobility::Movable);
	Post->bUnbound = true;
	const float Saturation = (float)DRData::Number(Env, TEXT("saturation"), 1.0);
	Post->Settings.bOverride_ColorSaturation = true;
	Post->Settings.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.0f);
	Post->Settings.bOverride_ColorContrast = true;
	Post->Settings.ColorContrast = FVector4(1.08f, 1.08f, 1.08f, 1.0f);
	FinishPart(Post);
}

void ADRChapter::BuildGround(const TSharedPtr<FJsonObject>& Data)
{
	const TSharedPtr<FJsonObject>* Ground = nullptr;
	Data->TryGetObjectField(TEXT("ground"), Ground);
	// Sol de 200 m sur 200 m, dessus à 0.
	Box(Look(Ground ? *Ground : TSharedPtr<FJsonObject>()), FVector(0.0, 0.0, -50.0), FVector(20000.0, 20000.0, 100.0));
}

void ADRChapter::BuildPath(const TSharedPtr<FJsonObject>& Data)
{
	const TSharedPtr<FJsonObject>* PathMaterial = nullptr;
	Data->TryGetObjectField(TEXT("path_mat"), PathMaterial);
	UMaterialInterface* Mud = Look(PathMaterial ? *PathMaterial : TSharedPtr<FJsonObject>());
	// Bandes de 4,5 m de large, un peu plus longues que chaque tronçon pour bien se raccorder ;
	// chacune un poil plus haute que la précédente pour ne pas scintiller là où elles se croisent.
	for (int32 Index = 0; Index < Path.Num() - 1; ++Index)
	{
		const FVector A = Path[Index];
		const FVector B = Path[Index + 1];
		const FVector Middle = (A + B) * 0.5;
		const double Length = FVector::Dist2D(A, B) + 450.0;
		Box(Mud, FVector(Middle.X, Middle.Y, 1.0 + Index * 0.4), FVector(Length, 450.0, 2.0), DRChapterLayout::YawOf(B - A), false);
		Blocked.Add(A);
		Blocked.Add(Middle);
	}
}

void ADRChapter::BuildPortal()
{
	// Brèche dans un vieux mur d'enceinte, juste derrière le portail, éclairée par une lampe de secours.
	const FVector P = Path[0];
	const FVector Forward = (Path[1] - P).GetSafeNormal2D();
	const FVector Side(-Forward.Y, Forward.X, 0.0);
	const FRotator Yaw = DRChapterLayout::YawOf(Forward);
	UMaterialInterface* Concrete = Look(TEXT("concrete"), 0.3f, FLinearColor(0.75f, 0.72f, 0.7f));
	for (const double X : { -1400.0, 1400.0 })
	{
		Box(Concrete, P + Side * X - Forward * 300.0 + FVector(0.0, 0.0, 250.0), FVector(120.0, 2200.0, 500.0), Yaw);
	}
	for (const double X : { -330.0, 330.0 })
	{
		Box(Concrete, P + Side * X - Forward * 300.0 + FVector(0.0, 0.0, 300.0), FVector(140.0, 100.0, 600.0), Yaw);
	}
	Flicker(P - Forward * 200.0 + FVector(0.0, 0.0, 550.0), FLinearColor(1.0f, 0.15f, 0.08f), DRChapterLayout::PortalCandelas, 1400.0f, 4.0f, 0.5f, false);
}

// ---------------------------------------------------------------- base

void ADRChapter::BuildCore()
{
	// Bunker du Cœur (4 m de côté) : socle, bloc de béton, toit, porte blindée côté champ de bataille
	// (+X) et voyants bleus.
	const FVector C = CorePosition();
	UMaterialInterface* Concrete = Look(TEXT("beton"), 0.42f, FLinearColor(0.92f, 0.9f, 0.86f));
	UMaterialInterface* Steel = Look(TEXT("metal"), 0.9f, FLinearColor(0.5f, 0.52f, 0.5f));
	UMaterialInterface* Blue = Look(TEXT("metal"), 1.0f, FLinearColor(0.3f, 0.6f, 1.0f), 25.0f);
	Box(Concrete, C + FVector(0.0, 0.0, 15.0), FVector(450.0, 450.0, 30.0));
	Box(Concrete, C + FVector(0.0, 0.0, 175.0), FVector(400.0, 400.0, 290.0));
	Box(Concrete, C + FVector(0.0, 0.0, 332.0), FVector(370.0, 370.0, 25.0));
	Box(Plain(FLinearColor(0.07f, 0.07f, 0.075f)), C + FVector(204.0, 0.0, 140.0), FVector(10.0, 150.0, 230.0));
	Box(Steel, C + FVector(210.0, 0.0, 135.0), FVector(6.0, 120.0, 205.0));
	for (const double Y : { -110.0, 110.0 })
	{
		Box(Blue, C + FVector(212.0, Y, 260.0), FVector(4.0, 18.0, 6.0), FRotator::ZeroRotator, false);
	}
	// Ses points de vie et sa lumière bleue sont portés par ADRCore.
	Core = GetWorld()->SpawnActor<ADRCore>(C + FVector(0.0, 0.0, 175.0), FRotator::ZeroRotator);
	Blocked.Add(C);
}

void ADRChapter::BuildRings(const TSharedPtr<FJsonObject>& Data)
{
	const TSharedPtr<FJsonObject>* Rings = nullptr;
	if (!Data->TryGetObjectField(TEXT("rings"), Rings))
	{
		return;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Ring : (*Rings)->Values)
	{
		const TSharedPtr<FJsonObject> RingData = Ring.Value->AsObject();
		if (!RingData.IsValid())
		{
			continue;
		}
		const TArray<TSharedPtr<FJsonValue>>* Sockets = nullptr;
		if (RingData->TryGetArrayField(TEXT("sockets"), Sockets))
		{
			for (const TSharedPtr<FJsonValue>& Socket : *Sockets)
			{
				BuildSocket(DRData::Vector(Socket));
			}
		}
		const FVector Gate = DRData::Vector(RingData, TEXT("gate"));
		const int32 Segment = FMath::Clamp((int32)DRData::Number(RingData, TEXT("segment")), 0, FMath::Max(0, Path.Num() - 2));
		if (Path.Num() >= 2)
		{
			// Porte d'avant-poste qui coupe le chemin (Godot : main.gd, _build_rings).
			const FString GateName = Ring.Key == TEXT("avant") ? TEXT("Barri\u00E8re de l'Avant-poste") : TEXT("Barri\u00E8re de la Muraille");
			const double GateProgress = 1000.0 * (Segment + 1) - FVector::Dist2D(Gate, Path[Segment + 1]) / 100.0;
			if (ADRBarrier* Barrier = GetWorld()->SpawnActor<ADRBarrier>(Gate, FRotator::ZeroRotator))
			{
				Barrier->Build(GateName, (float)DRData::Number(RingData, TEXT("hp"), 600.0), Path[Segment + 1] - Path[Segment], 720.0, GateProgress);
				Barriers.Add(Barrier);
			}
		}
		Blocked.Add(Gate);
	}
}

void ADRChapter::BuildSocket(const FVector& Position)
{
	// Ancrage de tour : dalle de béton ronde et quatre repères orangés.
	Cylinder(Look(TEXT("concrete"), 0.5f), Position + FVector(0.0, 0.0, 15.0), 105.0, 30.0);
	UMaterialInterface* Mark = Look(TEXT("metal"), 1.0f, FLinearColor(1.0f, 0.75f, 0.2f), 6.0f);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const double Angle = Index * 90.0;
		const FVector Offset = FRotator(0.0, Angle, 0.0).RotateVector(FVector(78.0, 0.0, 31.0));
		Box(Mark, Position + Offset, FVector(12.0, 40.0, 4.0), FRotator(0.0, Angle, 0.0), false);
	}
	Blocked.Add(Position);
}

void ADRChapter::BuildWalls(const TSharedPtr<FJsonObject>& Data)
{
	// Murs anti-souffle autour du Cœur (Godot : _build_base_walls), puis ceux du chapitre.
	FRandomStream WallRandom(404);
	struct FWall { FVector Center; FVector Size; };
	TArray<FWall> Walls = {
		{ FVector(-3000.0, -900.0, 100.0), FVector(80.0, 800.0, 200.0) },
		{ FVector(-4000.0, 900.0, 100.0), FVector(1200.0, 80.0, 200.0) },
		{ FVector(-4000.0, -900.0, 100.0), FVector(1200.0, 80.0, 200.0) },
		{ FVector(-4600.0, 0.0, 100.0), FVector(80.0, 1800.0, 200.0) },
	};
	const TArray<TSharedPtr<FJsonValue>>* LevelWalls = nullptr;
	if (Data->TryGetArrayField(TEXT("walls"), LevelWalls))
	{
		for (const TSharedPtr<FJsonValue>& Wall : *LevelWalls)
		{
			const TSharedPtr<FJsonObject> WallData = Wall->AsObject();
			Walls.Add({ DRData::Vector(WallData, TEXT("centre")), DRData::Vector(WallData, TEXT("taille")) });
		}
	}
	for (const FWall& Wall : Walls)
	{
		BuildBlastWall(Wall.Center, Wall.Size, WallRandom);
		Blocked.Add(Wall.Center);
	}

	// Sacs de sable : devant le Cœur, puis ceux du chapitre.
	BuildSandbags(FVector(-2900.0, 400.0, 0.0), 6);
	const TArray<TSharedPtr<FJsonValue>>* Sandbags = nullptr;
	if (Data->TryGetArrayField(TEXT("sandbags"), Sandbags))
	{
		for (const TSharedPtr<FJsonValue>& Start : *Sandbags)
		{
			BuildSandbags(DRData::Vector(Start), 6);
		}
	}
}

void ADRChapter::BuildBlastWall(const FVector& Center, const FVector& Size, FRandomStream& Rng)
{
	// Godot : Buildings.blast_wall. Éléments de 1,5 m environ : semelle, talon, voile et chaperon.
	UMaterialInterface* Concrete = Look(TEXT("beton"), 0.42f, FLinearColor(0.92f, 0.9f, 0.86f));
	const bool bAlongY = Size.Y >= Size.X;
	const double Length = bAlongY ? Size.Y : Size.X;
	const double Thick = bAlongY ? Size.X : Size.Y;
	const FVector Axis = bAlongY ? FVector(0.0, 1.0, 0.0) : FVector(1.0, 0.0, 0.0);
	const FRotator Turn(0.0, bAlongY ? 90.0 : 0.0, 0.0);
	const int32 Count = FMath::Max(1, FMath::RoundToInt(Length / 150.0));
	const double Segment = Length / Count;
	const double Bottom = Center.Z - Size.Z * 0.5;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector Base = FVector(Center.X, Center.Y, Bottom) + Axis * (-Length * 0.5 + (Index + 0.5) * Segment);
		const double Height = Size.Z + Rng.FRandRange(-5.0, 4.0);
		const double W = Segment - 3.5;
		Box(Concrete, Base + FVector(0.0, 0.0, 17.0), FVector(W, Thick, 34.0), Turn);
		Box(Concrete, Base + FVector(0.0, 0.0, 50.0), FVector(W, Thick * 0.7, 34.0), Turn);
		Box(Concrete, Base + FVector(0.0, 0.0, (60.0 + Height - 12.0) * 0.5), FVector(W, Thick * 0.45, Height - 72.0), Turn);
		Box(Concrete, Base + FVector(0.0, 0.0, Height - 6.0), FVector(W - 4.0, Thick * 0.32, 12.0), Turn);
	}
}

void ADRChapter::BuildSandbags(const FVector& Start, int32 Count)
{
	// Trois rangées de sacs en quinconce, alignées vers +Y (Godot : +X).
	UMaterialInterface* Cloth = Look(TEXT("cloth"), 1.2f, FLinearColor(0.55f, 0.48f, 0.34f));
	for (int32 Row = 0; Row < 3; ++Row)
	{
		for (int32 Index = 0; Index < Count - (Row % 2); ++Index)
		{
			const FVector Center = Start + FVector(0.0, Index * 75.0 + (Row % 2) * 37.5, 18.0 + Row * 33.0);
			Add(SphereMesh, Cloth, FTransform(FRotator::ZeroRotator, Center, FVector(0.64, 0.8, 0.35)), false);
		}
	}
	UBoxComponent* Block = NewPart<UBoxComponent>(TEXT("SacsDeSable"), EComponentMobility::Static);
	Block->SetBoxExtent(FVector(35.0, Count * 37.5, 50.0));
	Block->SetRelativeLocation(Start + FVector(0.0, Count * 37.5 - 37.5, 50.0));
	Block->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	FinishPart(Block);
	Blocked.Add(Start);
}

// ---------------------------------------------------------------- lumières

void ADRChapter::BuildLights(const TSharedPtr<FJsonObject>& Data)
{
	// Projecteurs de chantier qui éclairent les couloirs.
	UMaterialInterface* Metal = Look(TEXT("metal"), 1.0f, FLinearColor(0.7f, 0.7f, 0.7f));
	UMaterialInterface* Lamp = Look(TEXT("metal"), 1.0f, FLinearColor(1.0f, 0.95f, 0.8f), 20.0f);
	const FLinearColor SpotColor = DRData::ColorSRGB(Data, TEXT("spot_color"), FLinearColor(1.0f, 0.92f, 0.78f));
	const TArray<TSharedPtr<FJsonValue>>* Spots = nullptr;
	if (Data->TryGetArrayField(TEXT("spots"), Spots))
	{
		for (const TSharedPtr<FJsonValue>& SpotValue : *Spots)
		{
			const TSharedPtr<FJsonObject> SpotData = SpotValue->AsObject();
			const FVector Base = DRData::Vector(SpotData, TEXT("position"));
			const FVector Target = DRData::Vector(SpotData, TEXT("cible"));
			Cylinder(Metal, Base + FVector(0.0, 0.0, 350.0), 12.0, 700.0);
			const FVector Head = Base + FVector(0.0, 0.0, 720.0);
			const FRotator Aim = (Target - Head).Rotation();
			USpotLightComponent* Spot = NewPart<USpotLightComponent>(TEXT("Projecteur"), EComponentMobility::Movable);
			Spot->SetRelativeLocationAndRotation(Head, Aim);
			Spot->SetIntensityUnits(ELightUnits::Candelas);
			Spot->SetIntensity(DRChapterLayout::SpotCandelas);
			Spot->SetLightColor(SpotColor);
			Spot->SetAttenuationRadius(4500.0f);
			Spot->SetOuterConeAngle(32.0f);
			Spot->SetInnerConeAngle(22.0f);
			Spot->SetCastShadows(true);
			Spot->SetVolumetricScatteringIntensity(2.0f);
			FinishPart(Spot);
			Box(Lamp, Head - Aim.Vector() * 16.0, FVector(30.0, 60.0, 40.0), Aim, false);
			Blocked.Add(Base);
		}
	}

	// Barils en feu : braises rougeoyantes et lumière qui vacille.
	UMaterialInterface* Embers = Look(TEXT("metal"), 1.0f, FLinearColor(1.0f, 0.45f, 0.12f), 15.0f);
	const TArray<TSharedPtr<FJsonValue>>* Barrels = nullptr;
	if (Data->TryGetArrayField(TEXT("barrels"), Barrels))
	{
		for (const TSharedPtr<FJsonValue>& BarrelValue : *Barrels)
		{
			const FVector Position = DRData::Vector(BarrelValue);
			Cylinder(Metal, Position + FVector(0.0, 0.0, 50.0), 35.0, 100.0);
			Add(SphereMesh, Embers, FTransform(FRotator::ZeroRotator, Position + FVector(0.0, 0.0, 100.0), FVector(0.55, 0.55, 0.12)), false);
			Flicker(Position + FVector(0.0, 0.0, 160.0), FLinearColor(1.0f, 0.55f, 0.2f), DRChapterLayout::BarrelCandelas, 900.0f, 8.0f, 0.35f, true);
			Blocked.Add(Position);
		}
	}
}

// ---------------------------------------------------------------- décor

bool ADRChapter::IsFreeSpot(const FVector& Position, double Margin) const
{
	for (int32 Index = 0; Index < Path.Num() - 1; ++Index)
	{
		if (DRChapterLayout::DistanceToSegment2D(Position, Path[Index], Path[Index + 1]) < Margin)
		{
			return false;
		}
	}
	for (const FVector& Spot : Blocked)
	{
		if (FVector::Dist2D(Position, Spot) < Margin * 0.7)
		{
			return false;
		}
	}
	return FVector::Dist2D(Position, CorePosition()) > 1400.0;
}

void ADRChapter::BuildProps(const TSharedPtr<FJsonObject>& Data)
{
	// Types de décor du chapitre et leur poids : {"count": 70, "rock": 5, "tree": 3, ...}.
	const TSharedPtr<FJsonObject>* Props = nullptr;
	if (!Data->TryGetObjectField(TEXT("props"), Props))
	{
		return;
	}
	TArray<TPair<FString, int32>> Kinds;
	int32 Total = 0;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Kind : (*Props)->Values)
	{
		if (Kind.Key != TEXT("count"))
		{
			const int32 Weight = FMath::Max(0, (int32)Kind.Value->AsNumber());
			Kinds.Emplace(Kind.Key, Weight);
			Total += Weight;
		}
	}
	const int32 Wanted = (int32)DRData::Number(*Props, TEXT("count"));
	int32 Placed = 0;
	for (int32 Tries = 0; Placed < Wanted && Total > 0 && Tries < 3000; ++Tries)
	{
		const FVector Position(Random.FRandRange(-7500.0, 7500.0), Random.FRandRange(-7500.0, 7500.0), 0.0);
		if (!IsFreeSpot(Position, 600.0))
		{
			continue;
		}
		++Placed;
		Blocked.Add(Position);
		int32 Roll = Random.RandHelper(Total);
		FString Kind = Kinds[0].Key;
		for (const TPair<FString, int32>& Entry : Kinds)
		{
			if (Roll < Entry.Value)
			{
				Kind = Entry.Key;
				break;
			}
			Roll -= Entry.Value;
		}
		// Les autres décors (cuves, conteneurs, sapins, ruines...) viendront avec les autres chapitres.
		if (Kind == TEXT("rock"))
		{
			BuildRock(Position);
		}
		else if (Kind == TEXT("tree"))
		{
			BuildDeadTree(Position);
		}
		else if (Kind == TEXT("wreck"))
		{
			BuildWreck(Position);
		}
	}
}

void ADRChapter::BuildRock(const FVector& Position)
{
	const double Size = Random.FRandRange(60.0, 220.0);
	const FVector Scale(Size * 2.0 * Random.FRandRange(0.8, 1.4), Size * 2.0 * Random.FRandRange(0.8, 1.4), Size * 1.3 * Random.FRandRange(0.5, 0.9));
	const FRotator Yaw(0.0, Random.FRandRange(0.0, 360.0), 0.0);
	Add(SphereMesh, Look(TEXT("concrete"), 0.6f, FLinearColor(0.5f, 0.48f, 0.45f)), FTransform(Yaw, Position + FVector(0.0, 0.0, Size * 0.2), Scale / 100.0), true);
}

void ADRChapter::BuildDeadTree(const FVector& Position)
{
	UMaterialInterface* Bark = Look(TEXT("mud"), 0.8f, FLinearColor(0.45f, 0.4f, 0.35f));
	const double Height = Random.FRandRange(500.0, 900.0);
	Cylinder(Bark, Position + FVector(0.0, 0.0, Height * 0.5), 19.0, Height);
	const int32 Branches = Random.RandRange(3, 6);
	for (int32 Index = 0; Index < Branches; ++Index)
	{
		const double Length = Random.FRandRange(150.0, 350.0);
		const double Fork = Random.FRandRange(Height * 0.45, Height * 0.95);
		const double Tilt = Random.FRandRange(0.5, 1.1);
		const double Around = Random.FRandRange(0.0, 2.0 * PI);
		const FVector Direction(FMath::Sin(Tilt) * FMath::Cos(Around), FMath::Sin(Tilt) * FMath::Sin(Around), FMath::Cos(Tilt));
		Cylinder(Bark, Position + FVector(0.0, 0.0, Fork) + Direction * Length * 0.5, 6.0, Length, FRotationMatrix::MakeFromZ(Direction).Rotator(), false);
	}
}

void ADRChapter::BuildWreck(const FVector& Position)
{
	// Carcasse de voiture rouillée : caisse, habitacle et quatre roues.
	UMaterialInterface* Rust = Look(TEXT("metal"), 0.5f, FLinearColor(0.8f, 0.6f, 0.5f));
	UMaterialInterface* Tyre = Plain(FLinearColor(0.06f, 0.06f, 0.06f));
	const FRotator Yaw(0.0, Random.FRandRange(0.0, 360.0), 0.0);
	const FVector Center = Position + FVector(0.0, 0.0, 75.0);
	Box(Rust, Center, FVector(420.0, 190.0, 100.0), Yaw);
	Box(Rust, Center + Yaw.RotateVector(FVector(20.0, 0.0, 85.0)), FVector(200.0, 170.0, 70.0), Yaw);
	const FRotator WheelTurn = (FRotationMatrix(FRotator(0.0, 0.0, 90.0)) * FRotationMatrix(Yaw)).Rotator();
	for (const double Along : { -130.0, 130.0 })
	{
		for (const double Across : { -95.0, 95.0 })
		{
			Cylinder(Tyre, Center + Yaw.RotateVector(FVector(Along, Across, -30.0)), 38.0, 30.0, WheelTurn, false);
		}
	}
}
