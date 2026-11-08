#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "DRZombie.generated.h"

class ADRBarrier;
class ADRDecoy;
class ADRChapter;
class UAnimSequence;
class UPointLightComponent;
class UStaticMeshComponent;

/** Fouisseur : en surface, en train de creuser, sous terre, puis en train de ressortir. */
enum class EDRDig : uint8
{
	None, // pas un Fouisseur
	Walk,
	Dig,
	Under,
	Rise,
};

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
 * Talents : le Cracheur crache de l'acide de loin, le Hurleur rend la horde plus rapide et plus
 * forte par son cri, le Fouisseur passe sous les barrières en creusant, le boss ne se laisse pas
 * étourdir.
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
	/** Cri du Hurleur : plus rapide et plus fort pendant quelques secondes. */
	void Buff(float Duration);
	bool IsBuffed() const { return BuffTime > 0.0f; }
	/** Fouisseur sous terre : invisible, intouchable, ignoré des tours. */
	bool IsBurrowed() const { return bBurrowed; }
	/** Le Phare force un Fouisseur à sortir de terre. */
	void ForceSurface();

private:
	void Think();
	ADRBarrier* FindBlocker() const;
	/** Leurre sonore actif à moins de 16 m, que rien ne sépare du zombie (Godot : _find_decoy). */
	ADRDecoy* FindLure() const;
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
	float CurrentSpeed() const;
	float CurrentDamage() const;
	AActor* FindSpitTarget() const;
	FVector AimPoint(const AActor* Aimed) const;
	void Spit(AActor* Aimed);
	void ReleaseSpit();
	void Scream();
	/** Machine à états du Fouisseur ; vrai quand elle gère seule le déplacement. */
	bool UpdateDig(float DeltaSeconds);
	bool WantsToDig() const;
	void StartDig();
	void StartRise();
	void SnapToPath();
	/** Enfonce le modèle sous le sol (Fouisseur) : 0 = à sa place. */
	void SetRigDepth(float Depth);

	UPROPERTY()
	TObjectPtr<ADRChapter> Chapter;

	UPROPERTY()
	TObjectPtr<ADRBarrier> Blocker;

	UPROPERTY()
	TObjectPtr<AActor> StrikeTarget;

	/** Leurre sonore qui l'attire (prioritaire sur tout le reste). */
	TWeakObjectPtr<ADRDecoy> Lure;

	UPROPERTY()
	TMap<FString, TObjectPtr<UAnimSequence>> Animations;

	/** Silhouette provisoire quand les modèles ne sont pas encore importés. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Placeholder;

	/** Butte de terre qui avance quand le Fouisseur creuse. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mound;

	/** Cracheur : ce qu'il vise (barrière ou joueur), et la cible du crachat en préparation. */
	UPROPERTY()
	TObjectPtr<AActor> SpitTarget;

	UPROPERTY()
	TObjectPtr<AActor> SpitAim;

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
	float BuffTime = 0.0f;
	float HoldTime = 0.0f;     // le Hurleur s'arrête le temps de son cri
	float SpitCooldown = 1.0f;
	float SpitTime = 0.0f;
	float ScreamCooldown = 3.0f;
	EDRDig DigState = EDRDig::None;
	float DigTimer = 0.0f;
	float UnderTime = 0.0f;
	float RumbleCooldown = 0.0f;
	FVector DigDestination = FVector::ZeroVector;
	FVector MeshBase = FVector::ZeroVector;
	FVector LastHitDirection = FVector::ZeroVector;
	bool bBurrowed = false;
	bool bDead = false;
};

/** Crachat d'acide du Cracheur (Godot : projectile.gd) : vol en cloche, s'écrase sur le premier obstacle. */
UCLASS()
class DERNIERREMPART_API ADRAcid : public AActor
{
	GENERATED_BODY()

public:
	ADRAcid();

	void Launch(const FVector& To, float InFlight, float InDamage, AActor* InTarget);
	virtual void Tick(float DeltaSeconds) override;

private:
	void Splash(const FVector& Where);

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Ball;

	/** Flaque d'acide qui reste un moment au sol. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Puddle;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Shine;

	TWeakObjectPtr<AActor> Target;
	FVector Velocity = FVector::ZeroVector;
	float Flight = 1.0f;
	float Age = 0.0f;
	float Damage = 14.0f;
	float PuddleLeft = -1.0f;
};
