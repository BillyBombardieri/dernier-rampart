#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "DRZombie.generated.h"

class ADRBarrier;
class ADRChapter;
class UAnimSequence;
class UStaticMeshComponent;

/** Dégâts d'une arme lourde (pistolet lourd) : brisent un zombie gelé (combo BRISÉ). */
UCLASS()
class DERNIERREMPART_API UDRHeavyDamage : public UDamageType
{
	GENERATED_BODY()
};

/**
 * Zombie qui suit le chemin jusqu'au Cœur (Godot : scripts/zombie.gd). Son corps est le modèle
 * sculpté, texturé et animé de la version Godot (marche, course, coups, morts). Il s'arrête devant
 * les barrières pour les casser, chasse le joueur s'il passe trop près, et frappe le Cœur au bout
 * du chemin. Il ne s'en prend jamais aux tours. Un tir à la tête fait deux fois plus de dégâts.
 * Les tours et le joueur lui donnent des états : marqué, gelé (ralenti), en feu, chargé, étourdi,
 * d'où les combos BRISÉ (tir lourd sur un zombie gelé) et SURCHARGE (tir sur un zombie chargé).
 * (Les talents du Cracheur, du Hurleur, du Fouisseur et du boss viendront ensuite.)
 */
UCLASS()
class DERNIERREMPART_API ADRZombie : public ACharacter
{
	GENERATED_BODY()

public:
	ADRZombie();

	/** Type de jeu.json (« rodeur », « coureur », « brute »...) et montée en puissance de la vague. */
	void Setup(const FString& InType, float HpScale, float SpeedScale, float DamageScale);

	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	bool IsDead() const { return bDead; }
	float GetHp() const { return Hp; }
	float GetMaxHp() const { return MaxHp; }
	const FString& GetZombieType() const { return Type; }
	/** Point au-dessus de la tête, où se dessine la barre de vie. */
	FVector GetBarAnchor() const;
	/** Largeur de la barre de vie (cm) et son opacité : visible 6 s après le dernier coup, puis s'estompe. */
	float GetBarWidth() const { return 62.0f * FMath::Sqrt(Size); }
	float GetBarAlpha() const { return BarAlpha; }
	/** Avancée sur le chemin (même échelle que dans Godot : 1000 par tronçon). */
	double Progress() const;
	float GetSize() const { return Size; }

	// États donnés par les tours et le joueur (Godot : mark, freeze, ignite, charge, stun).
	void Mark();
	void Freeze(float Duration);
	void Ignite(float Duration, float Dps);
	void Charge(float Duration);
	void Stun(float Duration);
	bool IsMarked() const { return MarkedTime > 0.0f; }
	bool IsFrozen() const { return FrozenTime > 0.0f; }
	bool IsBurning() const { return BurnTime > 0.0f; }
	bool IsCharged() const { return ChargedTime > 0.0f; }
	bool IsStunned() const { return StunTime > 0.0f; }

private:
	void Think();
	ADRBarrier* FindBlocker() const;
	bool IsReachable(const FVector& Point) const;
	FVector LanePoint(int32 Index) const;
	double FlatDistance(const FVector& Point) const;
	float HeadHeight() const;
	void Swing(AActor* Target);
	void Strike();
	void Die();
	void PlayLoop(const FString& Name, float Rate);
	void PlayAction(const FString& Name, float Duration);
	void UpdateAnimation(float DeltaSeconds);
	void UpdateStatus(float DeltaSeconds);
	void ApplyDot(float Amount);
	float Surcharge();

	UPROPERTY()
	TObjectPtr<ADRChapter> Chapter;

	UPROPERTY()
	TObjectPtr<ADRBarrier> Blocker;

	UPROPERTY()
	TObjectPtr<AActor> StrikeTarget;

	UPROPERTY()
	TMap<FString, TObjectPtr<UAnimSequence>> Animations;

	/** Silhouette provisoire quand les modèles ne sont pas encore importés. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Placeholder;

	FString Type = TEXT("rodeur");
	FString ModelName = TEXT("rodeur");
	FString CurrentAnimation;
	float MaxHp = 60.0f;
	float Hp = 60.0f;
	float Speed = 1.8f;   // m/s, comme dans Godot
	float Damage = 8.0f;
	float Size = 1.0f;
	float ModelScale = 1.0f;
	float WalkCycle = 1.585f; // vitesse au sol d'un cycle de marche à l'échelle 1 (m/s)
	float RunCycle = 0.0f;
	int32 PathIndex = 1;
	float Lane = 0.0f;    // décalage sur la largeur du chemin (m) : la horde ne marche pas en file
	float ThinkCooldown = 0.0f;
	float AttackCooldown = 0.0f;
	float StrikeTime = 0.0f;
	float GroanCooldown = 5.0f;
	float ActionTime = 0.0f;
	float DeadTime = 0.0f;
	float DeathLength = 1.5f;
	float BarShown = 0.0f;
	float BarAlpha = 0.0f;
	float MarkedTime = 0.0f;
	float FrozenTime = 0.0f;
	float BurnTime = 0.0f;
	float BurnDps = 0.0f;
	float BurnTick = 0.0f;
	float ChargedTime = 0.0f;
	float StunTime = 0.0f;
	FVector LastHitDirection = FVector::ZeroVector;
	bool bDead = false;
};
