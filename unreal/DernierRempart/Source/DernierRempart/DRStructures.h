#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRStructures.generated.h"

class UBoxComponent;
class UMaterialInterface;
class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Barrière qui coupe le chemin des zombies (Godot : scripts/barrier.gd, porte d'avant-poste) :
 * blocs de béton, poteaux, fils et barrière rayée. Les zombies s'arrêtent devant pour la casser ;
 * détruite, elle s'effondre et les laisse passer. On la répare (ou on la relève) en maintenant E.
 */
UCLASS()
class DERNIERREMPART_API ADRBarrier : public AActor
{
	GENERATED_BODY()

public:
	ADRBarrier();

	/** Direction = sens du chemin vers le Cœur ; Progress = avancée sur le chemin (comme ADRZombie::Progress). */
	void Build(const FString& InName, float InHp, const FVector& Direction, double Width, double InProgress);

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsAlive() const { return bAlive; }
	float GetHp() const { return Hp; }
	float GetMaxHp() const { return MaxHp; }
	const FString& GetDisplayName() const { return DisplayName; }
	double GetProgress() const { return ProgressValue; }
	/** Vrai si le point est derrière la barrière (côté Cœur). */
	bool IsBehind(const FVector& Point) const;
	/** Distance entre un point situé devant la barrière et sa face avant (cm). */
	double FrontDistance(const FVector& Point) const;
	/** Point de la barrière le plus proche, pour que les zombies se répartissent sur sa largeur. */
	FVector ContactPoint(const FVector& Point) const;
	/** Ajoute des PV ; détruite, elle se relève une fois revenue à 30 % de ses PV. */
	void Repair(float Amount);
	/** PV à retrouver pour relever la barrière une fois détruite. */
	float GetRebuildHp() const { return MaxHp * RebuildRatio; }

private:
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Center, const FVector& Scale, const FRotator& Rotation, bool bHeavy);
	void Break();

	UPROPERTY()
	TObjectPtr<UBoxComponent> Blocker;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	TArray<FTransform> Rest;
	TArray<FTransform> Fallen;
	FString DisplayName;
	FVector Dir = FVector::ForwardVector;
	double HalfWidth = 360.0;
	double ProgressValue = 0.0;
	float MaxHp = 600.0f;
	float Hp = 600.0f;
	float FallAlpha = 0.0f;
	float RiseAlpha = 1.0f;
	float ShakeAmount = 0.0f;
	float RebuildRatio = 0.3f;
	bool bAlive = true;
};

/**
 * Le Cœur (Godot : scripts/structure.gd) : 1200 PV. S'il tombe, la partie est perdue (ADRGameMode).
 * Son bunker est dessiné par la carte (ADRChapter) ; cet acteur porte ses PV et ses voyants.
 */
UCLASS()
class DERNIERREMPART_API ADRCore : public AActor
{
	GENERATED_BODY()

public:
	ADRCore();

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	bool IsAlive() const { return bAlive; }
	float GetHp() const { return Hp; }
	float GetMaxHp() const { return MaxHp; }
	void Repair(float Amount);

private:
	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	float MaxHp = 1200.0f;
	float Hp = 1200.0f;
	bool bAlive = true;
};
