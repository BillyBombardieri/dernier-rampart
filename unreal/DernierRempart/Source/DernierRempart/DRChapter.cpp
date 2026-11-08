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
#include "DRTower.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "UObject/ConstructorHelpers.h"

// Nommé (et pas anonyme) pour éviter les collisions quand Unreal regroupe les .cpp à la compilation.
namespace DRChapterLayout
{
	// Lumières. Godot n'a pas d'unités physiques : ces valeurs sont choisies pour garder le même
	// équilibre (projecteurs bien plus forts que le soleil couchant, braises orangées).
	constexpr float SunLuxPerEnergy = 10.0f;   // 10 lux = soleil par défaut d'Unreal
	constexpr float SpotCandelas = 2000.0f;
	constexpr float BarrelCandelas = 200.0f;
	constexpr float LampCandelas = 130.0f; // réverbère de la Ville Morte (Godot : énergie 1,6)
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

// Herbe et cailloux (Godot : scripts/ground_cover.gd) : petits maillages fabriqués au lancement,
// puis posés par milliers en exemplaires, rangés par carrés de 40 m.
namespace DRCoverMesh
{
	constexpr double ChunkSize = 4000.0;  // carrés de 40 m
	constexpr double HalfSize = 8000.0;   // de -80 à +80 m autour du centre
	constexpr int32 CullStart = 7500;     // l'herbe et les cailloux disparaissent entre 75 et 85 m
	constexpr int32 CullEnd = 8500;

	// Sommets, normales, UV et triangles (trois indices chacun) d'un petit maillage.
	struct FParts
	{
		TArray<FVector3f> Corners;
		TArray<FVector3f> Normals;
		TArray<FVector2f> Coords;
		TArray<int32> Triangles;

		int32 Corner(const FVector3f& At, const FVector3f& Facing, const FVector2f& Coord)
		{
			Corners.Add(At);
			Normals.Add(Facing);
			Coords.Add(Coord);
			return Corners.Num() - 1;
		}

		// Chaque triangle est mis dans les deux sens : visible des deux côtés, sans dépendre du sens
		// de rotation des sommets, et avec la même normale des deux côtés (comme dans Godot).
		void Face(int32 A, int32 B, int32 C)
		{
			Triangles.Append({ A, B, C, A, C, B });
		}
	};

	UStaticMesh* Build(UObject* Outer, const FParts& Parts)
	{
		FMeshDescription Description;
		FStaticMeshAttributes Attributes(Description);
		Attributes.Register();
		auto Positions = Attributes.GetVertexPositions();
		auto InstanceNormals = Attributes.GetVertexInstanceNormals();
		auto InstanceTangents = Attributes.GetVertexInstanceTangents();
		auto InstanceSigns = Attributes.GetVertexInstanceBinormalSigns();
		auto InstanceCoords = Attributes.GetVertexInstanceUVs();
		const FPolygonGroupID Group = Description.CreatePolygonGroup();

		TArray<FVertexInstanceID> Instances;
		Instances.Reserve(Parts.Corners.Num());
		for (int32 Index = 0; Index < Parts.Corners.Num(); ++Index)
		{
			const FVertexID Vertex = Description.CreateVertex();
			Positions[Vertex] = Parts.Corners[Index];
			const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
			const FVector3f Facing = Parts.Normals[Index];
			// Tangente quelconque perpendiculaire à la normale (pas de texture de relief sur ces pièces).
			const FVector3f Helper = FMath::Abs(Facing.Z) < 0.9f ? FVector3f(0.0f, 0.0f, 1.0f) : FVector3f(1.0f, 0.0f, 0.0f);
			InstanceNormals[Instance] = Facing;
			InstanceTangents[Instance] = FVector3f::CrossProduct(Helper, Facing).GetSafeNormal();
			InstanceSigns[Instance] = 1.0f;
			InstanceCoords.Set(Instance, 0, Parts.Coords[Index]);
			Instances.Add(Instance);
		}
		for (int32 Index = 0; Index + 2 < Parts.Triangles.Num(); Index += 3)
		{
			TArray<FVertexInstanceID> Three;
			Three.Add(Instances[Parts.Triangles[Index]]);
			Three.Add(Instances[Parts.Triangles[Index + 1]]);
			Three.Add(Instances[Parts.Triangles[Index + 2]]);
			Description.CreateTriangle(Group, Three);
		}

		UStaticMesh* Result = NewObject<UStaticMesh>(Outer);
		Result->GetStaticMaterials().Add(FStaticMaterial());
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		TArray<const FMeshDescription*> Descriptions;
		Descriptions.Add(&Description);
		Result->BuildFromMeshDescriptions(Descriptions, Params);
		return Result;
	}

	// Une touffe : 12 brins (triangles fins) de hauteur prise dans Heights (cm). UV : X = part de la
	// couleur du pied dans la pointe (chaque brin un peu différent), Y = 0 au pied, 1 à la pointe.
	UStaticMesh* Tuft(UObject* Outer, const FVector2D& Heights, FRandomStream& Rng)
	{
		FParts Leaves;
		const FVector3f Up(0.0f, 0.0f, 1.0f); // normales vers le haut : l'herbe prend la lumière comme le sol
		constexpr int32 BladeCount = 12;
		for (int32 Blade = 0; Blade < BladeCount; ++Blade)
		{
			const double Angle = 2.0 * PI * Blade / BladeCount + Rng.FRandRange(-0.3, 0.3);
			const double Spread = Rng.FRandRange(0.0, 16.0);
			const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
			const FVector Foot = Out * Spread;
			const double Tall = Rng.FRandRange(Heights.X, Heights.Y);
			const FVector Lean = Out * Tall * Rng.FRandRange(0.15, 0.45);
			const FVector Across = FVector(-Out.Y, Out.X, 0.0) * Rng.FRandRange(2.5, 4.5);
			const float Fade = Rng.FRandRange(0.0f, 0.35f);
			const int32 A = Leaves.Corner(FVector3f(Foot - Across), Up, FVector2f(Fade, 0.0f));
			const int32 B = Leaves.Corner(FVector3f(Foot + Across), Up, FVector2f(Fade, 0.0f));
			const int32 C = Leaves.Corner(FVector3f(Foot + Lean + FVector(0.0, 0.0, Tall)), Up, FVector2f(Fade, 1.0f));
			Leaves.Face(A, B, C);
		}
		return Build(Outer, Leaves);
	}

	// Un caillou : sphère de 7 côtés et 3 anneaux comme dans Godot, 1 m de diamètre comme la sphère
	// du moteur (on la réduit en la posant).
	UStaticMesh* Pebble(UObject* Outer)
	{
		FParts Stone;
		constexpr int32 Sides = 7;
		constexpr int32 Rows = 4; // du pôle du haut (0) au pôle du bas (4)
		TArray<int32> Grid;
		for (int32 Row = 0; Row <= Rows; ++Row)
		{
			const double Polar = PI * Row / Rows;
			for (int32 Side = 0; Side < Sides; ++Side)
			{
				const double Around = 2.0 * PI * Side / Sides;
				const FVector3f Facing((float)(FMath::Sin(Polar) * FMath::Cos(Around)), (float)(FMath::Sin(Polar) * FMath::Sin(Around)), (float)FMath::Cos(Polar));
				Grid.Add(Stone.Corner(Facing * 50.0f, Facing, FVector2f((float)Side / Sides, (float)Row / Rows)));
			}
		}
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Side = 0; Side < Sides; ++Side)
			{
				const int32 Next = (Side + 1) % Sides;
				const int32 A = Grid[Row * Sides + Side];
				const int32 B = Grid[Row * Sides + Next];
				const int32 C = Grid[(Row + 1) * Sides + Side];
				const int32 D = Grid[(Row + 1) * Sides + Next];
				if (Row > 0)
				{
					Stone.Face(A, B, C); // au pôle du haut, A et B sont au même endroit
				}
				if (Row < Rows - 1)
				{
					Stone.Face(B, D, C); // au pôle du bas, C et D sont au même endroit
				}
			}
		}
		return Build(Outer, Stone);
	}

	double Gauss(FRandomStream& Rng, double Sigma)
	{
		const double U1 = FMath::Max((double)Rng.FRand(), 1e-6);
		const double U2 = Rng.FRand();
		return Sigma * FMath::Sqrt(-2.0 * FMath::Loge(U1)) * FMath::Cos(2.0 * PI * U2);
	}

	double PathDistance(const FVector& P, const TArray<FVector>& Points)
	{
		double Best = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
		{
			Best = FMath::Min(Best, DRChapterLayout::DistanceToSegment2D(P, Points[Index], Points[Index + 1]));
		}
		return Best;
	}

	void AddToCell(TMap<FIntPoint, TArray<FTransform>>& Cells, const FTransform& Where)
	{
		const FVector At = Where.GetLocation();
		const FIntPoint Key((int32)FMath::Floor(At.X / ChunkSize), (int32)FMath::Floor(At.Y / ChunkSize));
		Cells.FindOrAdd(Key).Add(Where);
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	CubeMesh = Cube.Object;
	SphereMesh = Sphere.Object;
	CylinderMesh = Cyl.Object;
	ConeMesh = Cone.Object;
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
	Data->TryGetStringField(TEXT("weather"), Weather);
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
	BuildWorkbench();
	BuildRings(Data);
	BuildWalls(Data);
	BuildLights(Data);
	BuildProps(Data);
	BuildCover(Data);
}

void ADRChapter::AddBarrier(ADRBarrier* Barrier)
{
	if (Barrier)
	{
		Barriers.AddUnique(Barrier);
	}
}

void ADRChapter::RemoveBarrier(ADRBarrier* Barrier)
{
	Barriers.Remove(Barrier);
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

void ADRChapter::BuildWorkbench()
{
	// Établi (Godot : workbench.gd) : plateau en bois sur pieds en acier, étagère, panneau à outils
	// côté champ de bataille (+X), étau, arme en cours de montage, douilles et lampe d'atelier.
	// Le plan de travail fait face au Cœur (-X). Le panneau « ÉTABLI » est dessiné par l'interface.
	const FVector B = WorkbenchPosition();
	UMaterialInterface* Wood = Look(TEXT("cloth"), 2.0f, FLinearColor(0.55f, 0.4f, 0.25f));
	UMaterialInterface* Steel = Look(TEXT("metal"), 1.5f, FLinearColor(0.7f, 0.7f, 0.7f));
	UMaterialInterface* Dark = Look(TEXT("metal"), 1.5f, FLinearColor(0.35f, 0.35f, 0.35f));
	Box(Wood, B + FVector(0.0, 0.0, 95.0), FVector(100.0, 240.0, 10.0));
	for (const double LegY : { -110.0, 110.0 })
	{
		for (const double LegX : { -42.0, 42.0 })
		{
			Box(Dark, B + FVector(LegX, LegY, 45.0), FVector(8.0, 8.0, 90.0), FRotator::ZeroRotator, false);
		}
	}
	Box(Dark, B + FVector(0.0, 0.0, 30.0), FVector(85.0, 220.0, 5.0), FRotator::ZeroRotator, false);
	Box(Wood, B + FVector(50.0, 0.0, 175.0), FVector(6.0, 240.0, 130.0));
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const double ToolHeight = Random.FRandRange(25.0f, 50.0f);
		Box(Steel, B + FVector(45.0, 90.0 - Index * 36.0, 185.0), FVector(4.0, 5.0, ToolHeight), FRotator(0.0, 0.0, Random.FRandRange(-17.0f, 17.0f)), false);
	}
	Box(Dark, B + FVector(-10.0, 85.0, 110.0), FVector(30.0, 25.0, 20.0), FRotator::ZeroRotator, false);
	Box(Plain(FLinearColor(0.12f, 0.12f, 0.13f)), B + FVector(-5.0, -20.0, 105.0), FVector(12.0, 70.0, 9.0), FRotator(0.0, -17.0, 0.0), false);
	Box(Plain(FLinearColor(0.1f, 0.1f, 0.1f)), B + FVector(20.0, -55.0, 105.0), FVector(20.0, 5.0, 8.0), FRotator::ZeroRotator, false);
	UMaterialInterface* Brass = Plain(FLinearColor(0.8f, 0.6f, 0.25f));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Cylinder(Brass, B + FVector(-25.0, -70.0 - 5.0 * Index, 103.0), 1.5, 6.0, FRotator::ZeroRotator, false);
	}
	// Lampe d'atelier chaude au-dessus du plan de travail.
	Box(Look(TEXT("metal"), 1.0f, FLinearColor(1.0f, 0.85f, 0.6f), 20.0f), B + FVector(35.0, 0.0, 250.0), FVector(15.0, 30.0, 8.0), FRotator::ZeroRotator, false);
	Flicker(B + FVector(10.0, 0.0, 230.0), FLinearColor(1.0f, 0.82f, 0.55f), 120.0f, 500.0f, 0.0f, 0.0f, true);
	Blocked.Add(B);
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
		const TArray<TSharedPtr<FJsonValue>>* SocketList = nullptr;
		if (RingData->TryGetArrayField(TEXT("sockets"), SocketList))
		{
			for (const TSharedPtr<FJsonValue>& Spot : *SocketList)
			{
				BuildSocket(DRData::Vector(Spot));
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
				// Amélioration permanente « Maçonnerie » : +8 % de PV par rang.
				const ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
				const float GateHp = (float)DRData::Number(RingData, TEXT("hp"), 600.0) * (Mode ? Mode->UpgradeMult(TEXT("maconnerie")) : 1.0f);
				Barrier->Build(GateName, GateHp, Path[Segment + 1] - Path[Segment], 720.0, GateProgress);
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
	// L'ancrage lui-même : c'est lui qui reçoit la tour.
	if (ADRSocket* Socket = GetWorld()->SpawnActor<ADRSocket>(ADRSocket::StaticClass(), Position, FRotator::ZeroRotator))
	{
		Sockets.Add(Socket);
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
		else if (Kind == TEXT("pool"))
		{
			BuildPool(Position);
		}
		else if (Kind == TEXT("reeds"))
		{
			BuildReeds(Position);
		}
		else if (Kind == TEXT("container"))
		{
			BuildContainer(Position);
		}
		else if (Kind == TEXT("tank"))
		{
			BuildTank(Position);
		}
		else if (Kind == TEXT("pine"))
		{
			BuildPine(Position);
		}
		else if (Kind == TEXT("ruin"))
		{
			BuildRuin(Position);
		}
		else if (Kind == TEXT("building"))
		{
			BuildBuilding(Position);
		}
		else if (Kind == TEXT("lamp"))
		{
			BuildLamp(Position);
		}
		else if (Kind == TEXT("puddle"))
		{
			BuildPuddle(Position);
		}
		else if (Kind == TEXT("dune"))
		{
			BuildDune(Position);
		}
		else if (Kind == TEXT("post"))
		{
			BuildPost(Position);
		}
	}
}

void ADRChapter::BuildRock(const FVector& Position)
{
	const double Size = Random.FRandRange(60.0, 220.0);
	const FVector Scale(Size * 2.0 * Random.FRandRange(0.8, 1.4), Size * 2.0 * Random.FRandRange(0.8, 1.4), Size * 1.3 * Random.FRandRange(0.5, 0.9));
	const FRotator Yaw(0.0, Random.FRandRange(0.0, 360.0), 0.0);
	// Rochers plus gris et froids sous la neige (Godot : rock_mat).
	const FLinearColor Tint = Weather == TEXT("snow") ? FLinearColor(0.62f, 0.64f, 0.68f) : FLinearColor(0.5f, 0.48f, 0.45f);
	Add(SphereMesh, Look(TEXT("concrete"), 0.6f, Tint), FTransform(Yaw, Position + FVector(0.0, 0.0, Size * 0.2), Scale / 100.0), true);
}

void ADRChapter::BuildDeadTree(const FVector& Position)
{
	// Écorce verdâtre et mouillée dans le Marais (Godot : bark).
	UMaterialInterface* Bark = Look(TEXT("mud"), 0.8f, Weather == TEXT("fireflies") ? FLinearColor(0.3f, 0.32f, 0.24f) : FLinearColor(0.45f, 0.4f, 0.35f));
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

// ---------------------------------------------------------------- décors des chapitres 2 à 6

void ADRChapter::Solid(const FVector& Center, const FVector& Size, const FRotator& Rotation)
{
	UBoxComponent* Block = NewPart<UBoxComponent>(TEXT("Collision"), EComponentMobility::Static);
	Block->SetBoxExtent(Size * 0.5);
	Block->SetRelativeLocationAndRotation(Center, Rotation);
	Block->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	FinishPart(Block);
}

UMaterialInterface* ADRChapter::Water()
{
	// Godot : eau presque noire, métallique et lisse. Ici, le métal très sombre et étiré reflète le ciel.
	return Look(TEXT("metal"), 0.15f, FLinearColor(0.05f, 0.065f, 0.06f));
}

void ADRChapter::BuildPool(const FVector& Position)
{
	// Mare d'eau noire (Marais), avec une touffe de roseaux sur le bord.
	const double Radius = Random.FRandRange(180.0, 320.0);
	const FVector Scale(Radius / 50.0 * Random.FRandRange(1.0, 1.7), Radius / 50.0 * Random.FRandRange(0.8, 1.2), 0.02);
	Add(CylinderMesh, Water(), FTransform(FRotator(0.0, Random.FRandRange(0.0, 360.0), 0.0), Position + FVector(0.0, 0.0, 3.0), Scale), false);
	BuildReeds(Position + FVector(Radius * 0.9, 0.0, 0.0));
}

void ADRChapter::BuildReeds(const FVector& Position)
{
	// Touffe de roseaux secs : de fines tiges effilées, un peu penchées.
	UMaterialInterface* Dry = Plain(FLinearColor(0.32f, 0.3f, 0.2f));
	const int32 Count = Random.RandRange(7, 13);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const double Height = Random.FRandRange(80.0, 190.0);
		const FVector Center = Position + FVector(Random.FRandRange(-50.0, 50.0), Random.FRandRange(-50.0, 50.0), Height * 0.5);
		const FRotator Lean(Random.FRandRange(-14.0, 14.0), 0.0, Random.FRandRange(-14.0, 14.0));
		Add(ConeMesh, Dry, FTransform(Lean, Center, FVector(0.05, 0.05, Height / 100.0)), false);
	}
}

void ADRChapter::BuildContainer(const FVector& Position)
{
	// Conteneur maritime rouillé (Raffinerie), parfois deux empilés, avec les nervures de la tôle.
	const FLinearColor Paints[] = { FLinearColor(0.45f, 0.16f, 0.1f), FLinearColor(0.15f, 0.25f, 0.35f), FLinearColor(0.4f, 0.35f, 0.12f), FLinearColor(0.2f, 0.3f, 0.2f) };
	const int32 Stack = Random.FRand() < 0.3f ? 2 : 1;
	const double Yaw = Random.FRandRange(0.0, 360.0);
	for (int32 Tier = 0; Tier < Stack; ++Tier)
	{
		UMaterialInterface* Steel = Look(TEXT("metal"), 0.6f, Paints[Random.RandHelper(4)]);
		const FRotator Turn(0.0, Yaw + (Tier > 0 ? Random.FRandRange(-11.5, 11.5) : 0.0), 0.0);
		const FVector Center = Position + FVector(0.0, 0.0, 130.0 + Tier * 260.0);
		Box(Steel, Center, FVector(600.0, 240.0, 260.0), Turn, true);
		for (int32 Rib = 0; Rib < 7; ++Rib)
		{
			Box(Steel, Center + Turn.RotateVector(FVector(-270.0 + Rib * 90.0, 0.0, 0.0)), FVector(8.0, 246.0, 250.0), Turn, false);
		}
	}
}

void ADRChapter::BuildTank(const FVector& Position)
{
	// Cuve de stockage (Raffinerie) avec son échelle et un feu rouge au sommet.
	const double Radius = Random.FRandRange(200.0, 300.0);
	const double Height = Random.FRandRange(400.0, 700.0);
	const FRotator Turn(0.0, Random.FRandRange(0.0, 360.0), 0.0);
	Cylinder(Look(TEXT("metal"), 0.4f, FLinearColor(0.55f, 0.53f, 0.5f)), Position + FVector(0.0, 0.0, Height * 0.5), Radius, Height);
	UMaterialInterface* Dark = Plain(FLinearColor(0.12f, 0.12f, 0.12f));
	const double Ladder = Height + 80.0;
	for (const double Side : { -25.0, 25.0 })
	{
		Cylinder(Dark, Position + Turn.RotateVector(FVector(Radius + 25.0, Side, 0.0)) + FVector(0.0, 0.0, Ladder * 0.5), 4.0, Ladder, FRotator::ZeroRotator, false);
	}
	const FRotator Rung = (FQuat(Turn) * FQuat(FRotator(0.0, 0.0, 90.0))).Rotator();
	for (double Z = 30.0; Z < Ladder - 10.0; Z += 35.0)
	{
		Cylinder(Dark, Position + Turn.RotateVector(FVector(Radius + 25.0, 0.0, 0.0)) + FVector(0.0, 0.0, Z), 2.0, 50.0, Rung, false);
	}
	Box(Look(TEXT("metal"), 1.0f, FLinearColor(1.0f, 0.25f, 0.15f), 4.0f), Position + FVector(0.0, 0.0, Height + 10.0), FVector(20.0, 20.0, 20.0), FRotator::ZeroRotator, false);
}

void ADRChapter::BuildPine(const FVector& Position)
{
	// Sapin enneigé (Col Gelé) : tronc et quatre étages de branches coniques, la neige sur le dessus.
	const double Height = Random.FRandRange(600.0, 1100.0);
	Cylinder(Look(TEXT("mud"), 0.8f, FLinearColor(0.3f, 0.24f, 0.2f)), Position + FVector(0.0, 0.0, Height * 0.175), 20.0, Height * 0.35);
	UMaterialInterface* Needles = Plain(FLinearColor(0.06f, 0.11f, 0.08f));
	UMaterialInterface* Snow = Plain(FLinearColor(0.82f, 0.85f, 0.9f));
	const FRotator Turn(0.0, Random.FRandRange(0.0, 360.0), 0.0);
	for (int32 Tier = 0; Tier < 4; ++Tier)
	{
		const double Spread = FMath::Lerp(230.0, 80.0, Tier / 4.0) * Height / 900.0; // rayon du bas de l'étage
		const double TierHeight = Height * 0.32;
		const double Z = Height * (0.22 + 0.2 * Tier);
		Add(ConeMesh, Needles, FTransform(Turn, Position + FVector(0.0, 0.0, Z), FVector(Spread / 50.0, Spread / 50.0, TierHeight / 100.0)), false);
		Add(ConeMesh, Snow, FTransform(Turn, Position + FVector(0.0, 0.0, Z + TierHeight * 0.25), FVector(Spread * 0.75 / 50.0, Spread * 0.75 / 50.0, TierHeight * 0.55 / 100.0)), false);
	}
	// Les branches arrêtent le joueur et les balles comme un tronc plus large (Godot : rayon 0,4 m).
	Solid(Position + FVector(0.0, 0.0, Height * 0.5), FVector(80.0, 80.0, Height), FRotator::ZeroRotator);
}

void ADRChapter::BuildRuin(const FVector& Position)
{
	// Maison en ruine (Col Gelé, Route Brûlée ; Godot : Buildings.ruin) : deux ou trois murs dont le
	// haut s'est effondré en escalier, une fenêtre béante avec son linteau, une poutre tombée, des gravats.
	const double Size = Random.FRandRange(500.0, 700.0);
	const FTransform Frame(FRotator(0.0, Random.FRandRange(0.0, 360.0), 0.0), Position);
	UMaterialInterface* WallLook = Random.FRand() < 0.5f ? Look(TEXT("brick"), 0.8f, FLinearColor(0.8f, 0.78f, 0.76f)) : Look(TEXT("plaster"), 0.35f, FLinearColor(0.72f, 0.7f, 0.66f));
	UMaterialInterface* Wood = Look(TEXT("plaster"), 0.6f, FLinearColor(0.3f, 0.22f, 0.15f));
	UMaterialInterface* Stone = Look(TEXT("beton"), 0.5f, FLinearColor(0.6f, 0.59f, 0.57f));
	// Pièce posée dans un repère (centre et taille en cm dans ce repère).
	auto Piece = [this](UMaterialInterface* Material, const FTransform& Space, const FVector& Center, const FVector& Extent, const FRotator& Rotation)
	{
		Box(Material, Space.TransformPosition(Center), Extent, (Space.GetRotation() * FQuat(Rotation)).Rotator(), false);
	};
	// Repère de chaque mur : X à travers le mur, Y le long du mur.
	const FTransform Spots[] = {
		FTransform(FRotator::ZeroRotator, FVector(Size * 0.5, 0.0, 0.0)),
		FTransform(FRotator(0.0, 90.0, 0.0), FVector(0.0, -Size * 0.5, 0.0)),
		FTransform(FRotator(0.0, 90.0, 0.0), FVector(0.0, Size * 0.5, 0.0)),
	};
	const int32 Walls = Random.RandRange(2, 3);
	for (int32 Index = 0; Index < Walls; ++Index)
	{
		const FTransform Space = Spots[Index] * Frame;
		const double Top = Random.FRandRange(220.0, 380.0);
		const double Length = Size * Random.FRandRange(0.6, 1.0);
		// Le mur est fait de colonnes de 40 cm ; leur hauteur tombe vers les bouts cassés.
		const double Peak = Random.FRandRange(-0.3, 0.3) * Length;
		const bool bOpen = Random.FRand() < 0.6f && Length > 350.0;
		const double Gap = Random.FRandRange(-0.2, 0.2) * Length;
		const int32 Columns = FMath::Max(1, (int32)(Length / 40.0));
		const double ColumnWidth = Length / Columns;
		double Total = 0.0;
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			const double Along = -Length * 0.5 + (Column + 0.5) * ColumnWidth;
			const double Fall = FMath::Clamp(1.15 - FMath::Abs(Along - Peak) / Length * Random.FRandRange(0.8, 1.4), 0.15, 1.0);
			const double ColumnHeight = FMath::Max(30.0, FMath::GridSnap(Top * Fall + Random.FRandRange(-20.0, 20.0), 20.0));
			Total += ColumnHeight;
			if (bOpen && FMath::Abs(Along - Gap) < 60.0 && ColumnHeight > 100.0)
			{
				// Fenêtre : allège en bas, et le mur reprend au-dessus du linteau.
				Piece(WallLook, Space, FVector(0.0, Along, 45.0), FVector(50.0, ColumnWidth, 90.0), FRotator::ZeroRotator);
				if (ColumnHeight > 240.0)
				{
					Piece(WallLook, Space, FVector(0.0, Along, (220.0 + ColumnHeight) * 0.5), FVector(50.0, ColumnWidth, ColumnHeight - 220.0), FRotator::ZeroRotator);
				}
			}
			else
			{
				Piece(WallLook, Space, FVector(0.0, Along, ColumnHeight * 0.5), FVector(50.0, ColumnWidth, ColumnHeight), FRotator::ZeroRotator);
			}
		}
		if (bOpen)
		{
			Piece(Wood, Space, FVector(0.0, Gap, 215.0), FVector(40.0, 160.0, 18.0), FRotator::ZeroRotator);
		}
		const double Average = Total / Columns;
		Solid(Space.TransformPosition(FVector(0.0, 0.0, Average * 0.5)), FVector(50.0, Length, Average), Space.Rotator());
	}
	// Poutre du toit tombée en travers, gravats au pied des murs.
	Piece(Wood, Frame, FVector(Random.FRandRange(-100.0, 50.0), Random.FRandRange(-100.0, 100.0), 80.0), FVector(Size * 0.8, 22.0, 22.0),
		FRotator(17.0, Random.FRandRange(0.0, 360.0), 6.0));
	const int32 Rubble = Random.RandRange(8, 14);
	for (int32 Index = 0; Index < Rubble; ++Index)
	{
		const FVector Center(Random.FRandRange(-Size, Size) * 0.5, Random.FRandRange(-Size, Size) * 0.5, 8.0);
		const FVector Extent(Random.FRandRange(20.0, 45.0), Random.FRandRange(20.0, 55.0), Random.FRandRange(12.0, 30.0));
		const FRotator Tumble(Random.FRandRange(-23.0, 23.0), Random.FRandRange(0.0, 360.0), Random.FRandRange(-23.0, 23.0));
		Piece(Random.FRand() < 0.6f ? WallLook : Stone, Frame, Center, Extent, Tumble);
	}
}

void ADRChapter::BuildBuilding(const FVector& Position)
{
	// Immeuble éventré (Ville Morte ; Godot : Buildings.city_building) de 2 à 4 niveaux : vraies
	// fenêtres en creux, vitres cassées ou condamnées, rideaux de fer, balcons, corniche, toit arraché.
	constexpr double FloorHeight = 320.0;
	constexpr double GroundHeight = 360.0; // rez-de-chaussée plus haut (boutiques)
	constexpr double WallThickness = 30.0;
	const double Width = Random.FRandRange(700.0, 1100.0);  // le long de Y
	const double Depth = Random.FRandRange(600.0, 900.0);   // le long de X
	const int32 Floors = Random.RandRange(2, 4);
	const double Height = GroundHeight + (Floors - 1) * FloorHeight;
	const FTransform Frame(FRotator(0.0, Random.RandRange(0, 3) * 90.0 + Random.FRandRange(-8.6, 8.6), 0.0), Position);

	const FLinearColor Tints[] = { FLinearColor(0.92f, 0.86f, 0.74f), FLinearColor(0.78f, 0.78f, 0.76f), FLinearColor(0.9f, 0.78f, 0.66f), FLinearColor(0.7f, 0.72f, 0.7f) };
	UMaterialInterface* WallLook = Random.FRand() < 0.4f ? Look(TEXT("brick"), 0.8f, FLinearColor(0.9f, 0.85f, 0.82f)) : Look(TEXT("plaster"), 0.35f, Tints[Random.RandHelper(4)]);
	UMaterialInterface* Trim = Look(TEXT("beton"), 0.5f, FLinearColor(0.85f, 0.84f, 0.8f));
	UMaterialInterface* Plinth = Look(TEXT("beton"), 0.5f, FLinearColor(0.55f, 0.54f, 0.52f));
	UMaterialInterface* Inside = Look(TEXT("plaster"), 0.35f, FLinearColor(0.3f, 0.29f, 0.28f));
	UMaterialInterface* Slab = Look(TEXT("beton"), 0.5f, FLinearColor(0.6f, 0.59f, 0.57f));
	UMaterialInterface* Glass = Look(TEXT("metal"), 0.2f, FLinearColor(0.05f, 0.06f, 0.07f));
	UMaterialInterface* Joinery = Plain(FLinearColor(0.12f, 0.11f, 0.1f));
	UMaterialInterface* Wood = Look(TEXT("plaster"), 0.6f, FLinearColor(0.42f, 0.3f, 0.2f));
	UMaterialInterface* Steel = Look(TEXT("metal"), 0.7f, FLinearColor(0.62f, 0.6f, 0.56f));
	UMaterialInterface* Lit = Look(TEXT("plaster"), 0.35f, FLinearColor(0.9f, 0.55f, 0.28f), 1.5f);
	UMaterialInterface* Dark = Plain(FLinearColor(0.025f, 0.025f, 0.03f));

	// Pièce dans le repère d'une façade : X vers l'extérieur (le mur va de -30 à 0), Y le long du mur,
	// Z vers le haut ; Roll la penche dans le plan du mur.
	auto Put = [this](UMaterialInterface* Material, const FTransform& Space, double Along, double Up, double Out, double SizeAlong, double SizeUp, double SizeOut, double Roll)
	{
		Box(Material, Space.TransformPosition(FVector(Out, Along, Up)), FVector(SizeOut, SizeAlong, SizeUp), (Space.GetRotation() * FQuat(FRotator(0.0, 0.0, Roll))).Rotator(), false);
	};
	// Pans de mur entre les ouvertures d'une rangée, de Bottom à Bottom + OpeningHeight.
	auto Piers = [&](UMaterialInterface* Material, const FTransform& Space, double Length, double Bottom, double OpeningHeight, int32 Bays, double OpeningWidth)
	{
		const double Bay = Length / Bays;
		double Start = -Length * 0.5;
		for (int32 Index = 0; Index <= Bays; ++Index)
		{
			const double End = Index == Bays ? Length * 0.5 : -Length * 0.5 + (Index + 0.5) * Bay - OpeningWidth * 0.5;
			if (End - Start > 1.0)
			{
				Put(Material, Space, (Start + End) * 0.5, Bottom + OpeningHeight * 0.5, -WallThickness * 0.5, End - Start, OpeningHeight, WallThickness, 0.0);
			}
			Start = End + OpeningWidth;
		}
	};
	// Fenêtre en creux : appui, cadre et traverse ; vitre, éclat de vitre brisée, planches, ou lueur.
	auto Window = [&](const FTransform& Space, double Center, double Bottom, double OpeningWidth, double OpeningHeight)
	{
		const double Back = -WallThickness * 0.65;
		const double Bar = 6.0;
		Put(Trim, Space, Center, Bottom - 4.0, 3.0, OpeningWidth + 24.0, 8.0, 16.0, 0.0);
		Put(Joinery, Space, Center, Bottom + Bar * 0.5, Back, OpeningWidth, Bar, 8.0, 0.0);
		Put(Joinery, Space, Center, Bottom + OpeningHeight - Bar * 0.5, Back, OpeningWidth, Bar, 8.0, 0.0);
		Put(Joinery, Space, Center - OpeningWidth * 0.5 + Bar * 0.5, Bottom + OpeningHeight * 0.5, Back, Bar, OpeningHeight, 8.0, 0.0);
		Put(Joinery, Space, Center + OpeningWidth * 0.5 - Bar * 0.5, Bottom + OpeningHeight * 0.5, Back, Bar, OpeningHeight, 8.0, 0.0);
		Put(Joinery, Space, Center, Bottom + OpeningHeight * 0.62, Back, OpeningWidth, 5.0, 7.0, 0.0);
		const float Roll = Random.FRand();
		if (Roll < 0.06f)
		{
			// Une lueur chaude derrière un rideau tiré à moitié : quelqu'un, encore ?
			Put(Lit, Space, Center, Bottom + OpeningHeight * 0.5, Back - 30.0, OpeningWidth - Bar, OpeningHeight - Bar, 1.0, 0.0);
			Put(Wood, Space, Center - OpeningWidth * 0.22, Bottom + OpeningHeight * 0.5, Back - 12.0, OpeningWidth * 0.5, OpeningHeight - Bar, 2.0, 0.0);
		}
		else if (Roll < 0.42f)
		{
			Put(Glass, Space, Center, Bottom + OpeningHeight * 0.5, Back, OpeningWidth - Bar, OpeningHeight - Bar, 1.0, 0.0);
		}
		else if (Roll < 0.58f)
		{
			// Planches clouées en travers.
			const int32 Boards = Random.RandRange(2, 4);
			for (int32 Board = 0; Board < Boards; ++Board)
			{
				Put(Wood, Space, Center + Random.FRandRange(-5.0, 5.0), Bottom + OpeningHeight * (0.2 + 0.22 * Board), 3.0, OpeningWidth + 30.0, 16.0, 3.0, Random.FRandRange(-14.3, 14.3));
			}
		}
		else
		{
			// Vitre brisée : il ne reste qu'un éclat dans un coin, le noir derrière.
			Put(Glass, Space, Center - OpeningWidth * 0.3, Bottom + OpeningHeight * 0.2, Back, OpeningWidth * 0.35, OpeningHeight * 0.18, 1.0, Random.FRandRange(28.6, 57.3));
		}
	};
	// Vitrine du rez-de-chaussée : rideau de fer (parfois à moitié levé sur le noir) ou vitrine éventrée.
	auto Shop = [&](const FTransform& Space, double Center, double OpeningWidth, double OpeningHeight)
	{
		Put(Steel, Space, Center, OpeningHeight + 17.0, -5.0, OpeningWidth + 10.0, 34.0, 36.0, 0.0);
		if (Random.FRand() < 0.65f)
		{
			const double Lift = Random.FRand() < 0.6f ? 0.0 : Random.FRandRange(60.0, 140.0);
			const double Shutter = OpeningHeight - 50.0 - Lift;
			Put(Steel, Space, Center, 50.0 + Lift + Shutter * 0.5, -10.0, OpeningWidth, Shutter, 3.0, 0.0);
			for (double Slat = 50.0 + Lift + 6.0; Slat < OpeningHeight - 5.0; Slat += 11.0)
			{
				Put(Joinery, Space, Center, Slat, -8.0, OpeningWidth, 2.5, 2.0, 0.0);
			}
			if (Lift > 0.0)
			{
				Put(Dark, Space, Center, 50.0 + Lift * 0.5, -WallThickness - 60.0, OpeningWidth, Lift, 5.0, 0.0);
			}
		}
		else
		{
			Put(Dark, Space, Center, (OpeningHeight + 50.0) * 0.5, -WallThickness - 60.0, OpeningWidth, OpeningHeight - 50.0, 5.0, 0.0);
			Put(Joinery, Space, Center, 55.0, -12.0, OpeningWidth, 10.0, 10.0, 0.0);
			Put(Joinery, Space, Center - OpeningWidth * 0.5 + 5.0, (OpeningHeight + 50.0) * 0.5, -12.0, 10.0, OpeningHeight - 50.0, 10.0, 0.0);
			Put(Joinery, Space, Center + OpeningWidth * 0.5 - 5.0, (OpeningHeight + 50.0) * 0.5, -12.0, 10.0, OpeningHeight - 50.0, 10.0, 0.0);
			Put(Glass, Space, Center - OpeningWidth * 0.3, 90.0, -12.0, OpeningWidth * 0.3, 60.0, 1.0, 0.0);
		}
	};
	// Balcon : dalle en saillie et garde-corps à barreaux.
	auto Balcony = [&](const FTransform& Space, double Center, double Bottom, double OpeningWidth)
	{
		const double Span = OpeningWidth + 70.0;
		Put(Slab, Space, Center, Bottom - 8.0, 45.0, Span, 16.0, 90.0, 0.0);
		Put(Joinery, Space, Center, Bottom + 100.0, 86.0, Span, 5.0, 5.0, 0.0);
		for (const double Side : { -1.0, 1.0 })
		{
			Put(Joinery, Space, Center + Side * Span * 0.5, Bottom + 100.0, 45.0, 5.0, 5.0, 85.0, 0.0);
		}
		for (double Rail = -Span * 0.5; Rail <= Span * 0.5 + 1.0; Rail += 13.0)
		{
			Put(Joinery, Space, Center + Rail, Bottom + 50.0, 86.0, 2.5, 100.0, 2.5, 0.0);
		}
	};

	// Quatre façades : devant et derrière sur toute la largeur (avec boutiques), les côtés entre les deux.
	struct FFacade
	{
		FTransform Space;
		double Length;
		bool bShops;
	};
	const FFacade Facades[] = {
		{ FTransform(FRotator::ZeroRotator, FVector(Depth * 0.5, 0.0, 0.0)) * Frame, Width, true },
		{ FTransform(FRotator(0.0, 180.0, 0.0), FVector(-Depth * 0.5, 0.0, 0.0)) * Frame, Width, true },
		{ FTransform(FRotator(0.0, 90.0, 0.0), FVector(0.0, Width * 0.5, 0.0)) * Frame, Depth - 2.0 * WallThickness, false },
		{ FTransform(FRotator(0.0, -90.0, 0.0), FVector(0.0, -Width * 0.5, 0.0)) * Frame, Depth - 2.0 * WallThickness, false },
	};
	for (const FFacade& Facade : Facades)
	{
		const FTransform& Space = Facade.Space;
		const double Length = Facade.Length;
		// Soubassement en béton, puis le rez-de-chaussée.
		Put(Plinth, Space, 0.0, 25.0, -WallThickness * 0.5 + 2.0, Length, 50.0, WallThickness + 4.0, 0.0);
		if (Facade.bShops)
		{
			const int32 Bays = FMath::Max(1, (int32)(Length / 360.0));
			const double Bay = Length / Bays;
			const double OpeningWidth = Bay - 80.0;
			const double OpeningHeight = 280.0;
			Piers(WallLook, Space, Length, 50.0, OpeningHeight - 50.0, Bays, OpeningWidth);
			Put(WallLook, Space, 0.0, (OpeningHeight + GroundHeight) * 0.5, -WallThickness * 0.5, Length, GroundHeight - OpeningHeight, WallThickness, 0.0);
			for (int32 Index = 0; Index < Bays; ++Index)
			{
				Shop(Space, -Length * 0.5 + (Index + 0.5) * Bay, OpeningWidth, OpeningHeight);
			}
		}
		else
		{
			Put(WallLook, Space, 0.0, (50.0 + GroundHeight) * 0.5, -WallThickness * 0.5, Length, GroundHeight - 50.0, WallThickness, 0.0);
		}
		Put(Trim, Space, 0.0, GroundHeight, 3.0, Length, 22.0, 8.0, 0.0);
		// Étages : allège, trumeaux entre les fenêtres, linteau.
		const int32 Bays = FMath::Max(1, (int32)(Length / 260.0));
		const double Bay = Length / Bays;
		const double OpeningWidth = FMath::Min(130.0, Bay - 90.0);
		const double OpeningHeight = 160.0;
		const double Sill = 85.0;
		for (int32 Storey = 1; Storey < Floors; ++Storey)
		{
			const double Bottom = GroundHeight + (Storey - 1) * FloorHeight;
			const double Top = Bottom + Sill + OpeningHeight;
			Put(WallLook, Space, 0.0, Bottom + Sill * 0.5, -WallThickness * 0.5, Length, Sill, WallThickness, 0.0);
			Put(WallLook, Space, 0.0, (Top + Bottom + FloorHeight) * 0.5, -WallThickness * 0.5, Length, Bottom + FloorHeight - Top, WallThickness, 0.0);
			Piers(WallLook, Space, Length, Bottom + Sill, OpeningHeight, Bays, OpeningWidth);
			if (Storey > 1)
			{
				Put(Trim, Space, 0.0, Bottom, 3.0, Length, 16.0, 6.0, 0.0);
			}
			for (int32 Index = 0; Index < Bays; ++Index)
			{
				const double Center = -Length * 0.5 + (Index + 0.5) * Bay;
				Window(Space, Center, Bottom + Sill, OpeningWidth, OpeningHeight);
				if (Facade.bShops && Storey >= 2 && Random.FRand() < 0.25f)
				{
					Balcony(Space, Center, Bottom, OpeningWidth);
				}
			}
		}
		// Corniche, et acrotère cassé par endroits.
		Put(Trim, Space, 0.0, Height + 15.0, 5.0, Length + 20.0, 30.0, 45.0, 0.0);
		for (double Along = -Length * 0.5; Along < Length * 0.5 - 50.0;)
		{
			const double Segment = FMath::Min(Random.FRandRange(150.0, 400.0), Length * 0.5 - Along);
			if (Random.FRand() < 0.8f)
			{
				const double Parapet = Random.FRandRange(70.0, 100.0);
				Put(WallLook, Space, Along + Segment * 0.5, Height + 30.0 + Parapet * 0.5, -15.0, Segment, Parapet, 25.0, 0.0);
			}
			Along += Segment;
		}
	}

	// Dedans (repère du bâtiment) : planchers, un refend qui coupe la vue, toit en partie arraché.
	auto Inner = [this, &Frame](UMaterialInterface* Material, const FVector& Center, const FVector& Extent, const FRotator& Rotation)
	{
		Box(Material, Frame.TransformPosition(Center), Extent, (Frame.GetRotation() * FQuat(Rotation)).Rotator(), false);
	};
	const double InnerDepth = Depth - 2.0 * WallThickness;
	const double InnerWidth = Width - 2.0 * WallThickness;
	for (int32 Storey = 1; Storey < Floors; ++Storey)
	{
		Inner(Slab, FVector(0.0, 0.0, GroundHeight + (Storey - 1) * FloorHeight - 12.0), FVector(InnerDepth, InnerWidth, 25.0), FRotator::ZeroRotator);
	}
	Inner(Inside, FVector(0.0, 0.0, Height * 0.5), FVector(20.0, InnerWidth, Height), FRotator::ZeroRotator);
	const double Roof = InnerDepth * Random.FRandRange(0.35, 0.8);
	Inner(Slab, FVector(InnerDepth * 0.5 - Roof * 0.5, 0.0, Height - 12.0), FVector(Roof, InnerWidth, 25.0), FRotator::ZeroRotator);
	// Un pan du toit pend dans le vide.
	Inner(Slab, FVector(InnerDepth * 0.5 - Roof - 60.0, Random.FRandRange(-Width, Width) * 0.2, Height - 100.0), FVector(220.0, Width * 0.4, 22.0), FRotator(Random.FRandRange(23.0, 46.0), 0.0, 0.0));
	// Gouttière, climatiseurs sur le toit, gravats au pied des façades.
	Inner(Joinery, FVector(-Depth * 0.5 - 8.0, -Width * 0.5 + 25.0, Height * 0.5), FVector(10.0, 10.0, Height), FRotator::ZeroRotator);
	const int32 Coolers = Random.RandRange(1, 2);
	for (int32 Index = 0; Index < Coolers; ++Index)
	{
		Inner(Steel, FVector(Depth * 0.35 - Index * 140.0, Random.FRandRange(-Width, Width) * 0.3, Height + 35.0), FVector(70.0, 110.0, 70.0), FRotator::ZeroRotator);
	}
	const int32 Rubble = Random.RandRange(5, 10);
	for (int32 Index = 0; Index < Rubble; ++Index)
	{
		const double Side = Random.FRand() < 0.5f ? 1.0 : -1.0;
		const FVector Center(Side * (Depth * 0.5 + Random.FRandRange(30.0, 160.0)), Random.FRandRange(-Width, Width) * 0.5, 10.0);
		const FVector Extent(Random.FRandRange(30.0, 80.0), Random.FRandRange(30.0, 90.0), Random.FRandRange(15.0, 45.0));
		const FRotator Tumble(Random.FRandRange(-17.0, 17.0), Random.FRandRange(0.0, 360.0), Random.FRandRange(-17.0, 17.0));
		Inner(Random.FRand() < 0.6f ? Trim : WallLook, Center, Extent, Tumble);
	}
	// Une seule boîte de collision pour tout l'immeuble.
	Solid(Frame.TransformPosition(FVector(0.0, 0.0, Height * 0.5)), FVector(Depth, Width, Height), Frame.Rotator());
}

void ADRChapter::BuildLamp(const FVector& Position)
{
	// Réverbère (Ville Morte). Un sur trois éclaire encore, en grésillant ; les autres sont morts.
	UMaterialInterface* Steel = Look(TEXT("metal"), 1.0f, FLinearColor(0.2f, 0.21f, 0.22f));
	const FRotator Turn(0.0, Random.FRandRange(0.0, 360.0), 0.0);
	Cylinder(Steel, Position + FVector(0.0, 0.0, 300.0), 8.0, 600.0);
	Box(Steel, Position + Turn.RotateVector(FVector(75.0, 0.0, 595.0)), FVector(160.0, 8.0, 8.0), Turn, false);
	const bool bLit = Random.FRand() < 0.34f;
	UMaterialInterface* Head = bLit ? Look(TEXT("metal"), 1.0f, FLinearColor(1.0f, 0.68f, 0.35f), 3.0f) : Plain(FLinearColor(0.15f, 0.15f, 0.15f));
	Box(Head, Position + Turn.RotateVector(FVector(150.0, 0.0, 585.0)), FVector(60.0, 35.0, 12.0), Turn, false);
	if (bLit)
	{
		Flicker(Position + Turn.RotateVector(FVector(150.0, 0.0, 550.0)), FLinearColor(1.0f, 0.66f, 0.34f), DRChapterLayout::LampCandelas, 1100.0f, 9.0f, 0.15f, false);
	}
}

void ADRChapter::BuildPuddle(const FVector& Position)
{
	// Flaque d'eau de pluie sur l'asphalte : un miroir sombre et mince.
	const double Radius = Random.FRandRange(120.0, 260.0);
	const FVector Scale(Radius / 50.0 * Random.FRandRange(1.0, 1.8), Radius / 50.0 * Random.FRandRange(0.6, 1.0), 0.01);
	Add(CylinderMesh, Water(), FTransform(FRotator(0.0, Random.FRandRange(0.0, 360.0), 0.0), Position + FVector(0.0, 0.0, 2.0), Scale), false);
}

void ADRChapter::BuildDune(const FVector& Position)
{
	// Bosse de sable (Route Brûlée) : large et très basse (moins d'un mètre), sans collision.
	const double Radius = Random.FRandRange(400.0, 800.0);
	const double Rise = Random.FRandRange(40.0, 80.0);
	const FVector Scale(Radius / 50.0 * Random.FRandRange(0.6, 1.0), Radius / 50.0, Rise / 50.0);
	Add(SphereMesh, Look(TEXT("ground"), 0.12f, FLinearColor(0.95f, 0.76f, 0.55f)), FTransform(FRotator(0.0, Random.FRandRange(0.0, 360.0), 0.0), Position - FVector(0.0, 0.0, 15.0), Scale), false);
}

void ADRChapter::BuildPost(const FVector& Position)
{
	// Poteau télégraphique en bois (Route Brûlée), un peu penché, avec deux bouts de câble qui pendent.
	UMaterialInterface* Wood = Look(TEXT("mud"), 0.8f, FLinearColor(0.42f, 0.33f, 0.24f));
	UMaterialInterface* Cable = Plain(FLinearColor(0.05f, 0.05f, 0.05f));
	const double Height = Random.FRandRange(600.0, 750.0);
	const FTransform Frame(FRotator(Random.FRandRange(-6.9, 6.9), Random.FRandRange(0.0, 360.0), Random.FRandRange(-6.9, 6.9)), Position);
	const FQuat Turn = Frame.GetRotation();
	Add(CylinderMesh, Wood, FTransform(Turn, Frame.TransformPosition(FVector(0.0, 0.0, Height * 0.5)), FVector(13.0 / 50.0, 13.0 / 50.0, Height / 100.0)), true);
	Box(Wood, Frame.TransformPosition(FVector(0.0, 0.0, Height - 50.0)), FVector(12.0, 180.0, 12.0), Turn.Rotator(), false);
	for (const double Side : { -75.0, 75.0 })
	{
		const double Hang = Random.FRandRange(150.0, 350.0);
		// Le câble pend tout droit sous la traverse, à peine écarté par le vent.
		const FVector Top = Frame.TransformPosition(FVector(0.0, Side, Height - 50.0));
		const FRotator Sway(0.0, 0.0, Random.FRandRange(-11.5, 11.5));
		Cylinder(Cable, Top - Sway.RotateVector(FVector(0.0, 0.0, Hang * 0.5)), 1.2, Hang, Sway, false);
	}
}

// ---------------------------------------------------------------- herbe et cailloux

void ADRChapter::BuildCover(const TSharedPtr<FJsonObject>& Data)
{
	// Réglages du chapitre : {"grass": 10400, "grass_base": [...], "grass_tip": [...], "grass_h": [0.25, 0.6],
	// "stones": 1800, "stone_tint": [...], "stone_shape": "box", "sway": 0.16} (hauteurs en mètres).
	const TSharedPtr<FJsonObject>* CoverData = nullptr;
	if (!Data->TryGetObjectField(TEXT("cover"), CoverData) || !CoverData->IsValid())
	{
		return;
	}
	const TSharedPtr<FJsonObject>& Cover = *CoverData;
	FRandomStream Sow(77 + Chapter);

	// Rien sur les ancrages (2,2 m), les barrières (3,5 m) ni dans la base autour du Cœur (9 m).
	TArray<FVector> Clear; // (x, y, rayon), en cm
	Clear.Add(FVector(CorePosition().X, CorePosition().Y, 900.0));
	for (const ADRBarrier* Barrier : Barriers)
	{
		if (Barrier)
		{
			Clear.Add(FVector(Barrier->GetActorLocation().X, Barrier->GetActorLocation().Y, 350.0));
		}
	}
	for (const ADRSocket* Socket : Sockets)
	{
		if (Socket)
		{
			Clear.Add(FVector(Socket->GetActorLocation().X, Socket->GetActorLocation().Y, 220.0));
		}
	}
	auto IsClear = [&Clear](const FVector& Spot)
	{
		for (const FVector& Zone : Clear)
		{
			if (FVector2D::Distance(FVector2D(Spot.X, Spot.Y), FVector2D(Zone.X, Zone.Y)) < Zone.Z)
			{
				return false;
			}
		}
		return true;
	};

	// Herbe : elle pousse en plaques, un centre puis 3 à 9 touffes serrées autour.
	const int32 GrassWanted = (int32)DRData::Number(Cover, TEXT("grass"));
	if (GrassWanted > 0)
	{
		const FLinearColor GrassBase = DRData::ColorSRGB(Cover, TEXT("grass_base"), FLinearColor(0.25f, 0.22f, 0.12f));
		const FLinearColor GrassTip = DRData::ColorSRGB(Cover, TEXT("grass_tip"), FLinearColor(0.6f, 0.52f, 0.3f));
		FVector2D Heights(25.0, 55.0);
		const TArray<TSharedPtr<FJsonValue>>* HeightRange = nullptr;
		if (Cover->TryGetArrayField(TEXT("grass_h"), HeightRange) && HeightRange->Num() >= 2)
		{
			Heights = FVector2D((*HeightRange)[0]->AsNumber() * 100.0, (*HeightRange)[1]->AsNumber() * 100.0);
		}
		TuftMesh = DRCoverMesh::Tuft(this, Heights, Sow);
		UMaterialInterface* GrassLook = DRLook::Grass(this, GrassBase, GrassTip, (float)DRData::Number(Cover, TEXT("sway"), 0.12));
		if (!GrassLook)
		{
			// Sans le matériau M_Herbe (script d'import pas encore relancé) : herbe immobile, couleur unie.
			bMissingTextures = true;
			GrassLook = Plain(GrassBase * 0.4f + GrassTip * 0.6f);
		}
		TMap<FIntPoint, TArray<FTransform>> GrassCells;
		FVector Patch = FVector::ZeroVector;
		int32 Left = 0;
		int32 Placed = 0;
		for (int32 Tries = 0; Placed < GrassWanted && Tries < GrassWanted * 4; ++Tries)
		{
			if (Left <= 0)
			{
				Patch = FVector(Sow.FRandRange(-DRCoverMesh::HalfSize, DRCoverMesh::HalfSize), Sow.FRandRange(-DRCoverMesh::HalfSize, DRCoverMesh::HalfSize), 0.0);
				Left = Sow.RandRange(3, 9);
			}
			--Left;
			const FVector Spot = Patch + FVector(DRCoverMesh::Gauss(Sow, 90.0), DRCoverMesh::Gauss(Sow, 90.0), 0.0);
			if (DRCoverMesh::PathDistance(Spot, Path) < 270.0 || !IsClear(Spot))
			{
				continue;
			}
			++Placed;
			const double Size = Sow.FRandRange(0.6, 1.25);
			const FVector Stretch(Size, Size, Size * Sow.FRandRange(0.8, 1.3));
			DRCoverMesh::AddToCell(GrassCells, FTransform(FRotator(0.0, Sow.FRandRange(0.0, 360.0), 0.0), Spot, Stretch));
		}
		AddCoverChunks(TuftMesh, GrassLook, GrassCells, false);
	}

	// Cailloux (ou éclats de béton) : partout, et serrés le long des bords du chemin.
	const int32 StonesWanted = (int32)DRData::Number(Cover, TEXT("stones"));
	if (StonesWanted > 0)
	{
		FString Shape;
		Cover->TryGetStringField(TEXT("stone_shape"), Shape);
		const bool bShards = Shape == TEXT("box");
		if (!bShards)
		{
			PebbleMesh = DRCoverMesh::Pebble(this);
		}
		UStaticMesh* StoneMesh = bShards ? CubeMesh.Get() : PebbleMesh.Get();
		// Godot : boîte de 12 x 7 x 9 cm ou sphère de 12 cm de large et 8 cm de haut.
		const FVector StoneSize = bShards ? FVector(9.0, 12.0, 7.0) : FVector(12.0, 12.0, 8.0);
		UMaterialInterface* StoneLook = Look(TEXT("concrete"), 2.0f, DRData::ColorSRGB(Cover, TEXT("stone_tint"), FLinearColor(0.5f, 0.48f, 0.45f)));
		TMap<FIntPoint, TArray<FTransform>> StoneCells;
		int32 Laid = 0;
		for (int32 Attempt = 0; Laid < StonesWanted && Attempt < StonesWanted * 4; ++Attempt)
		{
			FVector Spot = FVector::ZeroVector;
			if (Sow.FRand() < 0.45f && Path.Num() > 1)
			{
				const int32 Segment = Sow.RandRange(0, Path.Num() - 2);
				const FVector A = Path[Segment];
				const FVector B = Path[Segment + 1];
				const FVector Along = A + (B - A) * (double)Sow.FRand();
				const FVector Dir = (B - A).GetSafeNormal2D();
				const double Offset = Sow.FRandRange(190.0, 320.0) * (Sow.FRand() < 0.5f ? 1.0 : -1.0);
				Spot = FVector(Along.X - Dir.Y * Offset, Along.Y + Dir.X * Offset, 0.0);
			}
			else
			{
				Spot = FVector(Sow.FRandRange(-DRCoverMesh::HalfSize, DRCoverMesh::HalfSize), Sow.FRandRange(-DRCoverMesh::HalfSize, DRCoverMesh::HalfSize), 0.0);
				if (DRCoverMesh::PathDistance(Spot, Path) < 220.0)
				{
					continue;
				}
			}
			if (!IsClear(Spot))
			{
				continue;
			}
			++Laid;
			const double Size = Sow.FRandRange(0.5, 2.4) * (Sow.FRand() < 0.06f ? 2.0 : 1.0);
			const FRotator Tumble(Sow.FRandRange(-23.0, 23.0), Sow.FRandRange(0.0, 360.0), Sow.FRandRange(-23.0, 23.0));
			const FVector Stretch = StoneSize / 100.0 * FVector(Size, Size * Sow.FRandRange(0.7, 1.3), Size * Sow.FRandRange(0.5, 0.9));
			DRCoverMesh::AddToCell(StoneCells, FTransform(Tumble, Spot - FVector(0.0, 0.0, 2.0 * Size), Stretch));
		}
		AddCoverChunks(StoneMesh, StoneLook, StoneCells, true);
	}
}

void ADRChapter::AddCoverChunks(UStaticMesh* Mesh, UMaterialInterface* Material, const TMap<FIntPoint, TArray<FTransform>>& Cells, bool bShadows)
{
	if (!Mesh)
	{
		return;
	}
	for (const TPair<FIntPoint, TArray<FTransform>>& Cell : Cells)
	{
		UInstancedStaticMeshComponent* Chunk = NewPart<UInstancedStaticMeshComponent>(TEXT("Couverture"), EComponentMobility::Static);
		Chunk->SetStaticMesh(Mesh);
		Chunk->SetMaterial(0, Material);
		Chunk->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Chunk->SetCanEverAffectNavigation(false);
		Chunk->SetCastShadow(bShadows);
		Chunk->bAffectDistanceFieldLighting = false; // trop petits pour l'éclairage global
		Chunk->SetCullDistances(DRCoverMesh::CullStart, DRCoverMesh::CullEnd);
		FinishPart(Chunk);
		Chunk->AddInstances(Cell.Value, false, true);
	}
}
