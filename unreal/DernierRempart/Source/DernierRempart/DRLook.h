#pragma once

#include "CoreMinimal.h"

class UAnimSequence;
class UMaterialInterface;
class USkeletalMesh;
class USoundBase;
class UStaticMesh;

/**
 * Accès aux ressources importées par Content/Python/importer_ressources.py (/Game/Importe) :
 * matériaux texturés, sons et modèles. Tout marche aussi sans import (couleurs unies, pas de son),
 * pour que le jeu reste testable.
 */
namespace DRLook
{
	/**
	 * Matériau texturé d'après un jeu de textures de Godot (« concrete », « metal »...), avec le nombre
	 * de répétitions par mètre, une teinte sRGB et un éclat (0 = ne brille pas). Sans import : couleur
	 * unie proche, et bMissing passe à vrai s'il est fourni.
	 */
	UMaterialInterface* Textured(UObject* Outer, const FString& Set, float Scale, const FLinearColor& TintSRGB, float Glow = 0.0f, bool* bMissing = nullptr);

	/** Couleur unie (sRGB), sans texture. */
	UMaterialInterface* Plain(UObject* Outer, const FLinearColor& ColorSRGB);

	/** Son importé de assets/sounds (nom sans extension : « pistol », « reload »...), ou nul. */
	USoundBase* Sound(const FString& Name);

	/** Maillages fixes importés d'un modèle de assets/models (« arme_pistolet »...), ou liste vide. */
	TArray<UStaticMesh*> StaticMeshes(const FString& Model);

	/** Maillage animé importé d'un modèle (« zombie_rodeur »...), ou nul. */
	USkeletalMesh* SkeletalMesh(const FString& Model);

	/**
	 * Animations importées d'un modèle, rangées par leur nom dans Godot (« walk », « attack »,
	 * « die_front »...), quel que soit le préfixe ajouté par Unreal à l'import.
	 */
	TMap<FString, UAnimSequence*> Animations(const FString& Model);

	/** Volume en décibels (comme dans Godot) -> multiplicateur de volume d'Unreal, volume général compris. */
	float Volume(float Decibels);

	/** Volume général des réglages (0 à 1, 0,8 par défaut comme dans Godot). */
	void SetMasterVolume(float Linear);

	/** Joue un son dans le monde, plus faible avec la distance (Godot : Sfx.play_at). Distances en cm. */
	void PlayAt(const UObject* WorldContext, const FString& Name, const FVector& Location, float Decibels, float PitchJitter, float MaxDistance = 8000.0f);
}
