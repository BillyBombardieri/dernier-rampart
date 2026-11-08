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

	/**
	 * Herbe qui ondule au vent (matériau M_Herbe du script d'import) : couleur du pied et de la pointe
	 * (sRGB) et amplitude du balancement en mètres, comme dans Godot. Nul tant que le script n'a pas tourné.
	 */
	UMaterialInterface* Grass(UObject* Outer, const FLinearColor& BaseSRGB, const FLinearColor& TipSRGB, float Sway);

	/**
	 * Petit maillage fabriqué au lancement (touffe d'herbe, caillou, flocon) : sommets, normales, UV
	 * et triangles. Chaque triangle est mis dans les deux sens : visible des deux côtés, avec la même
	 * normale, sans dépendre du sens de rotation des sommets.
	 */
	struct FMeshParts
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

		void Face(int32 A, int32 B, int32 C)
		{
			Triangles.Append({ A, B, C, A, C, B });
		}
	};

	/** Fait un maillage fixe (sans collision) à partir de ces pièces. */
	UStaticMesh* BuildMesh(UObject* Outer, const FMeshParts& Parts);

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
