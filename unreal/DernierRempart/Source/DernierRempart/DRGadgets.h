#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRGadgets.generated.h"

class ADRScrap;
class UAudioComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Leurre sonore (Godot : scripts/decoy.gd) : un petit boîtier qui bipe et attire les zombies à
 * moins de 16 m pendant 8 s. Les zombies l'attaquent ; il se casse s'il prend trop de coups (120 PV).
 */
UCLASS()
class DERNIERREMPART_API ADRDecoy : public AActor
{
	GENERATED_BODY()

public:
	ADRDecoy();

	/** Petit vol en cloche de From à Land, en 0,45 s. */
	void Launch(const FVector& From, const FVector& Land);
	/** Vrai tant qu'il bipe (les zombies ne s'intéressent plus à lui ensuite). */
	bool IsActive() const { return bActive; }

	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

private:
	void Stop();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Led;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Glow;

	FVector FlightStart = FVector::ZeroVector;
	FVector FlightEnd = FVector::ZeroVector;
	float FlightTime = -1.0f; // négatif : posé au sol
	float Age = 0.0f;
	float BeepCooldown = 0.0f;
	float Hp = 120.0f;
	float ShrinkTime = -1.0f; // négatif : pas encore en train de disparaître
	bool bActive = true;
};

/**
 * Drone de récolte (Godot : scripts/drone.gd) : il vole d'une pièce de ferraille à l'autre à moins
 * de 25 m du joueur et la ramasse à sa place, lentement, pendant 25 s, puis revient vers lui et disparaît.
 */
UCLASS()
class DERNIERREMPART_API ADRDrone : public AActor
{
	GENERATED_BODY()

public:
	ADRDrone();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	ADRScrap* FindScrap(const FVector& Around) const;
	void Collect(ADRScrap* Piece);

	UPROPERTY()
	TObjectPtr<USceneComponent> Body;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Rotors;

	UPROPERTY()
	TObjectPtr<UAudioComponent> Hum;

	TWeakObjectPtr<ADRScrap> Goal;
	float Age = 0.0f;
	bool bLeaving = false;
};
