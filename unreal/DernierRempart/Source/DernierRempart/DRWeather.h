#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRWeather.generated.h"

class UAudioComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;

/** Un flocon, une goutte, une cendre... (position et vitesse en cm, âge en secondes). */
struct FDRFlake
{
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Age = 0.0f;
};

/**
 * Météo du chapitre autour du joueur (Godot : main.gd, _build_weather et _start_ambience) : neige
 * (Col Gelé), cendres (Raffinerie), lucioles (Marais), pluie (Ville Morte) ou poussière (Route Brûlée),
 * plus le vent dans tous les chapitres et le bruit de la pluie. Comme les particules de Godot, elles
 * naissent dans une grande boîte qui suit le joueur, puis vivent leur vie dans le monde. Ce sont de
 * petits losanges posés en exemplaires et déplacés à chaque image.
 */
UCLASS()
class DERNIERREMPART_API ADRWeather : public AActor
{
	GENERATED_BODY()

public:
	ADRWeather();

	/** « snow », « ash », « rain », « dust », « fireflies », ou vide (le vent seul). */
	void Setup(const FString& InKind);

	virtual void Tick(float DeltaSeconds) override;

private:
	/** Centre de la boîte où naissent les particules : au-dessus du joueur. */
	FVector EmitterLocation() const;
	void Respawn(FDRFlake& Flake, const FVector& Origin);
	/** Visibilité de 0 à 1 selon l'âge (0 = naissance, 1 = fin de vie), comme le dégradé de Godot. */
	float Fade(float LifeFraction) const;
	/** Garde un son d'ambiance en marche (s'il n'a pas été importé en boucle), au volume des réglages. */
	void KeepPlaying(UAudioComponent* Track, float Decibels);

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Particles;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SpeckMesh;

	UPROPERTY()
	TObjectPtr<UAudioComponent> WindSound;

	UPROPERTY()
	TObjectPtr<UAudioComponent> RainSound;

	TArray<FDRFlake> Flakes;
	TArray<FTransform> Frame; // positions envoyées à chaque image
	FRandomStream Rng;
	FVector Extents = FVector::ZeroVector;       // demi-tailles de la boîte de naissance (cm)
	FVector Direction = FVector(0.0, 0.0, -1.0); // direction de départ
	FVector Gravity = FVector::ZeroVector;       // cm/s²
	FVector Size = FVector(5.0);                 // taille d'une particule (cm)
	float Spread = 0.0f;   // écart autour de la direction (degrés)
	float SpeedMin = 0.0f; // cm/s
	float SpeedMax = 0.0f;
	float Lifetime = 1.0f; // s
	float Height = 800.0f; // hauteur de la boîte au-dessus du sol (cm)
	float WindDecibels = -14.0f;
	uint8 FadeShape = 0;   // 0 : s'efface peu à peu, 1 : reste visible, 2 : poussière, 3 : luciole
};
