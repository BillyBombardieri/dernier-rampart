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
#include "Modules/ModuleManager.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

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

float DRLook::Volume(float Decibels)
{
	return FMath::Pow(10.0f, Decibels / 20.0f);
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
