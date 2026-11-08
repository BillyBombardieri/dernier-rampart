#include "DRLook.h"

#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DRData.h"
#include "DRGameMode.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "StaticMeshAttributes.h"

namespace DRLookColors
{
	// Couleur moyenne de chaque jeu de textures (sRGB), pour les couleurs unies avant l'import.
	FLinearColor Average(const FString& Set)
	{
		if (Set == TEXT("beton")) return FLinearColor(0.53f, 0.53f, 0.50f);
		if (Set == TEXT("brick")) return FLinearColor(0.46f, 0.31f, 0.25f);
		if (Set == TEXT("cloth")) return FLinearColor(0.49f, 0.49f, 0.49f);
		if (Set == TEXT("concrete")) return FLinearColor(0.47f, 0.46f, 0.44f);
		if (Set == TEXT("ground")) return FLinearColor(0.27f, 0.24f, 0.16f);
		if (Set == TEXT("metal")) return FLinearColor(0.31f, 0.29f, 0.22f);
		if (Set == TEXT("mud")) return FLinearColor(0.22f, 0.17f, 0.13f);
		if (Set == TEXT("plaster")) return FLinearColor(0.65f, 0.62f, 0.56f);
		return FLinearColor(0.43f, 0.45f, 0.36f);
	}
}

UMaterialInterface* DRLook::Textured(UObject* Outer, const FString& Set, float Scale, const FLinearColor& TintSRGB, float Glow, bool* bMissing)
{
	const FString AssetPath = FString::Printf(TEXT("/Game/Importe/Materiaux/MI_%s.MI_%s"), *Set, *Set);
	if (UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, *AssetPath, nullptr, LOAD_NoWarn))
	{
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, Outer);
		Material->SetScalarParameterValue(TEXT("Echelle"), Scale);
		Material->SetVectorParameterValue(TEXT("Teinte"), DRData::ToLinear(TintSRGB));
		Material->SetScalarParameterValue(TEXT("Lumineux"), Glow);
		return Material;
	}
	if (bMissing)
	{
		*bMissing = true;
	}
	const FLinearColor Average = DRLookColors::Average(Set);
	return Plain(Outer, FLinearColor(Average.R * TintSRGB.R, Average.G * TintSRGB.G, Average.B * TintSRGB.B));
}

UMaterialInterface* DRLook::Plain(UObject* Outer, const FLinearColor& ColorSRGB)
{
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, Outer);
	Material->SetVectorParameterValue(TEXT("Color"), DRData::ToLinear(ColorSRGB));
	return Material;
}

UMaterialInterface* DRLook::Grass(UObject* Outer, const FLinearColor& BaseSRGB, const FLinearColor& TipSRGB, float Sway)
{
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Importe/Materiaux/M_Herbe.M_Herbe"), nullptr, LOAD_NoWarn);
	if (!Parent)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Parent, Outer);
	Material->SetVectorParameterValue(TEXT("Pied"), DRData::ToLinear(BaseSRGB));
	Material->SetVectorParameterValue(TEXT("Pointe"), DRData::ToLinear(TipSRGB));
	Material->SetScalarParameterValue(TEXT("Balancement"), Sway);
	return Material;
}

UStaticMesh* DRLook::BuildMesh(UObject* Outer, const FMeshParts& Parts)
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
	Params.bFastBuild = true; // le seul chemin possible hors de l'éditeur
	TArray<const FMeshDescription*> Descriptions;
	Descriptions.Add(&Description);
	Result->BuildFromMeshDescriptions(Descriptions, Params);
	return Result;
}

UStaticMesh* DRLook::BuildSphere(UObject* Outer, int32 Sides, int32 Rows)
{
	FMeshParts Ball;
	TArray<int32> Grid;
	for (int32 Row = 0; Row <= Rows; ++Row)
	{
		const double Polar = PI * Row / Rows;
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const double Around = 2.0 * PI * Side / Sides;
			const FVector3f Facing((float)(FMath::Sin(Polar) * FMath::Cos(Around)), (float)(FMath::Sin(Polar) * FMath::Sin(Around)), (float)FMath::Cos(Polar));
			Grid.Add(Ball.Corner(Facing * 50.0f, Facing, FVector2f((float)Side / Sides, (float)Row / Rows)));
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
				Ball.Face(A, B, C); // au pôle du haut, A et B sont au même endroit
			}
			if (Row < Rows - 1)
			{
				Ball.Face(B, D, C); // au pôle du bas, C et D sont au même endroit
			}
		}
	}
	return BuildMesh(Outer, Ball);
}

UStaticMesh* DRLook::BuildTorus(UObject* Outer, float RingRadius, float TubeRadius, int32 Segments, int32 Sides)
{
	FMeshParts Ring;
	TArray<int32> Grid;
	for (int32 Segment = 0; Segment < Segments; ++Segment)
	{
		const double Around = 2.0 * PI * Segment / Segments;
		const FVector3f Out((float)FMath::Cos(Around), (float)FMath::Sin(Around), 0.0f);
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const double Turn = 2.0 * PI * Side / Sides;
			const FVector3f Facing = Out * (float)FMath::Cos(Turn) + FVector3f(0.0f, 0.0f, (float)FMath::Sin(Turn));
			Grid.Add(Ring.Corner(Out * RingRadius + Facing * TubeRadius, Facing, FVector2f((float)Segment / Segments, (float)Side / Sides)));
		}
	}
	for (int32 Segment = 0; Segment < Segments; ++Segment)
	{
		const int32 NextSegment = (Segment + 1) % Segments;
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const int32 NextSide = (Side + 1) % Sides;
			const int32 A = Grid[Segment * Sides + Side];
			const int32 B = Grid[Segment * Sides + NextSide];
			const int32 C = Grid[NextSegment * Sides + Side];
			const int32 D = Grid[NextSegment * Sides + NextSide];
			Ring.Face(A, B, C);
			Ring.Face(B, D, C);
		}
	}
	return BuildMesh(Outer, Ring);
}

USoundBase* DRLook::Sound(const FString& Name)
{
	const FString AssetPath = FString::Printf(TEXT("/Game/Importe/Sons/%s.%s"), *Name, *Name);
	return LoadObject<USoundBase>(nullptr, *AssetPath, nullptr, LOAD_NoWarn);
}

namespace DRLookAssets
{
	// Ressources d'un dossier de /Game/Importe/Modeles, d'une classe donnée.
	template <class T>
	TArray<T*> InModel(const FString& Model)
	{
		TArray<T*> Found;
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(FName(*(FString(TEXT("/Game/Importe/Modeles/")) + Model)), Assets, true);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetClassPath == T::StaticClass()->GetClassPathName())
			{
				if (T* Object = Cast<T>(Asset.GetAsset()))
				{
					Found.Add(Object);
				}
			}
		}
		return Found;
	}
}

TArray<UStaticMesh*> DRLook::StaticMeshes(const FString& Model)
{
	return DRLookAssets::InModel<UStaticMesh>(Model);
}

USkeletalMesh* DRLook::SkeletalMesh(const FString& Model)
{
	const TArray<USkeletalMesh*> Meshes = DRLookAssets::InModel<USkeletalMesh>(Model);
	return Meshes.Num() > 0 ? Meshes[0] : nullptr;
}

TMap<FString, UAnimSequence*> DRLook::Animations(const FString& Model)
{
	// « die_front » avant « die » : un nom qui finit par « _die_front » ne doit pas passer pour « die ».
	static const TCHAR* Names[] = { TEXT("die_front"), TEXT("die"), TEXT("attack"), TEXT("idle"), TEXT("walk"), TEXT("run"), TEXT("spit"), TEXT("scream"), TEXT("dig") };
	TMap<FString, UAnimSequence*> Result;
	for (UAnimSequence* Sequence : DRLookAssets::InModel<UAnimSequence>(Model))
	{
		const FString AssetName = Sequence->GetName().ToLower();
		for (const TCHAR* Name : Names)
		{
			const FString Key(Name);
			if (AssetName == Key || AssetName.EndsWith(TEXT("_") + Key))
			{
				if (!Result.Contains(Key))
				{
					Result.Add(Key, Sequence);
				}
				break;
			}
		}
	}
	return Result;
}

namespace DRLookAudio
{
	float Master = 0.8f;
}

float DRLook::Volume(float Decibels)
{
	return FMath::Pow(10.0f, Decibels / 20.0f) * DRLookAudio::Master;
}

void DRLook::SetMasterVolume(float Linear)
{
	DRLookAudio::Master = FMath::Clamp(Linear, 0.0f, 1.0f);
}

void DRLook::PlayAt(const UObject* WorldContext, const FString& Name, const FVector& Location, float Decibels, float PitchJitter, float MaxDistance)
{
	USoundBase* Played = Sound(Name);
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!Played || !World)
	{
		return;
	}
	const ADRGameMode* Mode = World->GetAuthGameMode<ADRGameMode>();
	UGameplayStatics::PlaySoundAtLocation(WorldContext, Played, Location, FRotator::ZeroRotator, Volume(Decibels),
		1.0f + FMath::FRandRange(-PitchJitter, PitchJitter), 0.0f, Mode ? Mode->GetAttenuation(MaxDistance) : nullptr);
}
