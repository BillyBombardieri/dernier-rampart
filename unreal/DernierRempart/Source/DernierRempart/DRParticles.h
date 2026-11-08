#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRParticles.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/** Réglages d'une gerbe de flammes (Godot : Fx.fire_particles et ses variantes). Distances en cm. */
struct FDRFlameStyle
{
	FVector Direction = FVector(0.0, 0.0, 1.0);
	float Spread = 18.0f;          // degrés autour de la direction
	float SpeedMin = 60.0f;        // cm/s
	float SpeedMax = 160.0f;
	FVector Gravity = FVector(0.0, 0.0, 150.0); // les flammes montent de plus en plus vite
	float Damping = 0.0f;          // perte de vitesse en cm/s par seconde (jet du lance-flammes)
	float Radius = 25.0f;          // sphère où naissent les flammes
	float Size = 30.0f;            // taille d'une flamme
	float Lifetime = 0.7f;
	bool bWidening = false;        // le jet s'élargit en s'éloignant ; sinon les flammes s'affinent en montant
};

/** Une flamme ou une étincelle en vol. */
struct FDRSpark
{
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector Gravity = FVector::ZeroVector;
	float Damping = 0.0f;
	float Age = 0.0f;
	float Lifetime = 1.0f;
	float Size = 10.0f;
	bool bWidening = false;
};

/** Foyer qui brûle tout seul, sans que personne n'appelle Flames à chaque image (barils). */
struct FDRFireSource
{
	FVector Origin = FVector::ZeroVector;
	FDRFlameStyle Style;
	float Rate = 30.0f; // flammes par seconde
};

/**
 * Feu et étincelles de toute la carte (Godot : CPUParticles3D des zombies en feu, du lance-flammes,
 * des barils et des zombies chargés par l'Arc électrique). Un seul acteur pour tout le niveau : les
 * flammes sont de petites boules additives aux bords doux (matériau M_Feu du script d'import), posées
 * en exemplaires et déplacées à chaque image. Les jeunes flammes sont jaunes, les vieilles rouges.
 */
UCLASS()
class DERNIERREMPART_API ADRParticles : public AActor
{
	GENERATED_BODY()

public:
	ADRParticles();

	/** L'acteur du niveau (créé au premier appel). */
	static ADRParticles* Get(const UObject* WorldContext);

	/** Fait naître des flammes en Origin, au rythme Rate par seconde, pendant DeltaSeconds. */
	void Flames(const FVector& Origin, const FDRFlameStyle& Style, float Rate, float DeltaSeconds);
	/** Étincelles violettes autour d'un zombie chargé (Rate par seconde). */
	void Sparks(const FVector& Origin, float Radius, float Rate, float DeltaSeconds);
	/** Foyer permanent (baril en feu). */
	void AddFireSource(const FVector& Origin, const FDRFlameStyle& Style, float Rate);
	/** Faux tant que le matériau M_Feu n'est pas importé : les flammes ne s'affichent pas. */
	bool CanDrawFire();
	/** Anneau partagé (étoiles des zombies étourdis). */
	UStaticMesh* GetRingMesh();

	virtual void Tick(float DeltaSeconds) override;

private:
	void Setup();
	int32 Emit(float Rate, float DeltaSeconds);
	void Spawn(TArray<FDRSpark>& Pool, int32& Next, int32 Capacity, const FDRSpark& Spark);
	UInstancedStaticMeshComponent* NewLayer(const TCHAR* LayerName, UStaticMesh* Mesh, UMaterialInterface* Material, int32 Capacity);
	void Fill(UInstancedStaticMeshComponent* Layer, TArray<FTransform>& Frame, int32 Capacity, int32& Shown);

	UPROPERTY()
	TObjectPtr<UStaticMesh> BlobMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SpeckMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> RingMesh;

	/** Flammes jeunes (jaunes) et vieilles (rouges), puis étincelles. */
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> YoungLayer;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> OldLayer;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> SparkLayer;

	TArray<FDRSpark> FlamePool;
	TArray<FDRSpark> SparkPool;
	TArray<FDRFireSource> Sources;
	TArray<FTransform> YoungFrame;
	TArray<FTransform> OldFrame;
	TArray<FTransform> SparkFrame;
	int32 NextFlame = 0;
	int32 NextSpark = 0;
	int32 ShownYoung = 0;  // exemplaires visibles à l'image précédente
	int32 ShownOld = 0;
	int32 ShownSparks = 0;
	FRandomStream Rng;
	bool bFireLook = false;
	bool bReady = false;
};
