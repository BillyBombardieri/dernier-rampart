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
 * Le gadget Barricade en pose une provisoire (panneaux d'acier rayés), qui disparaît au bout de 20 s.
 */
UCLASS()
class DERNIERREMPART_API ADRBarrier : public AActor
{
	GENERATED_BODY()

public:
	ADRBarrier();

	/** Direction = sens du chemin vers le Cœur ; Progress = avancée sur le chemin (comme ADRZombie::Progress). */
	void Build(const FString& InName, float InHp, const FVector& Direction, double Width, double InProgress);
	/** Barricade du gadget (Godot : style « barricade ») : 1,2 m de haut, elle disparaît au bout de Life secondes ou une fois cassée. */
	void BuildBarricade(float InHp, const FVector& Direction, double Width, double InProgress, float Life);

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
	/** Sens du chemin vers le Cœur, à plat. */
	const FVector& GetDirection() const { return Dir; }
	/** Ajoute des PV ; détruite, elle se relève une fois revenue à 30 % de ses PV. */
	void Repair(float Amount);
	/** PV à retrouver pour relever la barrière une fois détruite. */
	float GetRebuildHp() const { return MaxHp * RebuildRatio; }
	/** Barricade du gadget (provisoire), et les secondes qu'il lui reste. */
	bool IsTemporary() const { return bTemporary; }
	float GetLifeLeft() const { return LifeLeft; }
	/** Hauteur du haut de la barrière (cm), pour placer son étiquette. */
	double GetTopHeight() const { return bTemporary ? 120.0 : 250.0; }

private:
	/** Pièce du décor de la barrière. Sans Parent, elle tombe seule quand la barrière s'effondre. */
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Center, const FVector& Scale, const FRotator& Rotation, bool bHeavy, USceneComponent* Parent = nullptr);
	/** Groupe de pièces qui tombent ensemble (un panneau de la barricade). */
	USceneComponent* AddGroup(const FVector& Center, const FRotator& Rotation);
	void Setup(const FString& InName, float InHp, const FVector& Direction, double Width, double InProgress, double Height);
	/** Note la position debout et la position effondrée de chaque pièce. */
	void FinishParts();
	void Break();
	/** Barricade : elle s'aplatit au bout de Delay secondes, en Duration secondes, puis disparaît. */
	void Vanish(float Delay, float Duration);

	UPROPERTY()
	TObjectPtr<UBoxComponent> Blocker;

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Parts;

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
	// Barricade du gadget.
	bool bTemporary = false;
	float LifeLeft = 0.0f;
	float GrowAlpha = 1.0f;   // elle se déplie en 0,35 s quand on la pose
	float VanishDelay = -1.0f; // négatif : elle ne disparaît pas
	float VanishAlpha = 0.0f;
	float VanishDuration = 0.5f;
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
