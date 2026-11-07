#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
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

	/** Volume en décibels (comme dans Godot) -> multiplicateur de volume d'Unreal. */
	float Volume(float Decibels);
}
