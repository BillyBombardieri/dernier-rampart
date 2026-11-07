#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * Lecture des données exportées de la version Godot (Content/Data/jeu.json et chapitres.json,
 * faits par tools/export_unreal_data.tscn) : on garde exactement les mêmes chiffres et les mêmes cartes.
 */
namespace DRData
{
	/** Lit un fichier JSON de Content/Data. Renvoie un pointeur vide (et l'écrit dans le journal) en cas d'erreur. */
	TSharedPtr<FJsonObject> Load(const FString& FileName);

	/** [x, y, z] -> vecteur (les positions des cartes sont déjà en centimètres, dans le repère d'Unreal). */
	FVector Vector(const TSharedPtr<FJsonValue>& Value);
	FVector Vector(const TSharedPtr<FJsonObject>& Object, const FString& Field);

	/** Nombre d'un champ, ou Default s'il manque. */
	double Number(const TSharedPtr<FJsonObject>& Object, const FString& Field, double Default = 0.0);

	/** [r, g, b, a] tel qu'écrit dans Godot (sRGB, de 0 à 1), sans conversion. */
	FLinearColor ColorSRGB(const TSharedPtr<FJsonObject>& Object, const FString& Field, const FLinearColor& Default = FLinearColor::White);

	/** Couleur sRGB (comme dans Godot) -> couleur linéaire, celle qu'attendent les matériaux d'Unreal. */
	FLinearColor ToLinear(const FLinearColor& SRGB);
}
