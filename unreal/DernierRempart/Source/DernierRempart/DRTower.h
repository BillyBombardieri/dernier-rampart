#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRTower.generated.h"

class ADRTower;
class ADRZombie;
class UBoxComponent;
class UMaterialInterface;
class UPointLightComponent;
class USpotLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Ancrage (Godot : scripts/socket.gd) : le seul endroit où l'on peut poser une tour. Sa dalle est dessinée par la carte. */
UCLASS()
class DERNIERREMPART_API ADRSocket : public AActor
{
	GENERATED_BODY()

public:
	ADRSocket();

	ADRTower* GetTower() const;
	/** Pose une tour de ce type si on a la ferraille. Elle reste éteinte s'il manque de l'énergie. */
	bool Build(const FString& Type);

private:
	UPROPERTY()
	TObjectPtr<ADRTower> Tower;
};

/**
 * Tour posée sur un ancrage (Godot : scripts/tower.gd) : Mitrailleuse, Cryo, Lance-flammes, Arc
 * électrique, Mortier ou Phare. Elle consomme de l'énergie et tire en priorité sur les zombies
 * marqués, puis sur les Hurleurs, puis sur le plus avancé vers le Cœur. Trois niveaux. Les zombies
 * ne l'attaquent jamais.
 */
UCLASS()
class DERNIERREMPART_API ADRTower : public AActor
{
	GENERATED_BODY()

public:
	ADRTower();

	void Setup(const FString& InType, ADRSocket* InSocket);
	virtual void Tick(float DeltaSeconds) override;

	void Upgrade();
	void TogglePower();

	const FString& GetType() const { return Type; }
	const FString& GetDisplayName() const { return DisplayName; }
	int32 GetLevel() const { return Level; }
	int32 GetMaxLevel() const { return MaxLevel; }
	bool IsPowered() const { return bPowered; }
	bool IsBoosted() const { return RangeMult > 1.0f; }
	int32 GetEnergyCost() const { return Energy; }
	int32 GetUpgradeCost() const;
	/** Portée actuelle (cm), niveau et Phare compris. */
	float GetRange() const;
	FLinearColor GetColor() const { return Color; }
	/** Point au-dessus de la tour, où s'écrit son nom. */
	FVector GetLabelAnchor() const;
	/** Bonus d'un Phare aux tours dans son halo : (portée, dégâts). */
	FVector2D GetBeaconBonus() const { return FVector2D(1.2f + 0.05f * Level, 1.1f + 0.05f * Level); }

private:
	UStaticMeshComponent* Part(USceneComponent* Parent, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator);
	void BuildVisual();
	ADRZombie* FindTarget() const;
	bool IsValidTarget(const ADRZombie* Zombie) const;
	int32 Neighbors(const ADRZombie* Zombie, double Radius) const;
	float GetDamage() const;
	float HitDamage(const ADRZombie* Zombie) const;
	void ShowTracer(int32 Index, const FVector& From, const FVector& To, float Duration);
	void Flash(float Candelas);
	void FireGun(ADRZombie* Victim, const FVector& Aim);
	void FireCryo(ADRZombie* Victim, const FVector& Aim);
	void FireFlame();
	void FireArc(ADRZombie* Victim);
	void FireMortar(ADRZombie* Victim);
	void UpdateBeacon(float DeltaSeconds);
	void UpdateBeaconBonus();
	double FlatDistance(const FVector& Point) const;

	UPROPERTY()
	TObjectPtr<UBoxComponent> Body;

	UPROPERTY()
	TObjectPtr<USceneComponent> Model;

	UPROPERTY()
	TObjectPtr<USceneComponent> Head;

	UPROPERTY()
	TObjectPtr<USceneComponent> Muzzle;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Lamp;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> FlashLight;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> FlameJet;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> FlameLight;

	UPROPERTY()
	TObjectPtr<USpotLightComponent> Beam;

	/** Traçantes et éclairs (l'Arc en enchaîne plusieurs). */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Tracers;

	UPROPERTY()
	TObjectPtr<ADRSocket> Socket;

	TWeakObjectPtr<ADRZombie> Target;
	TArray<float> TracerLeft;

	FString Type = TEXT("gun");
	FString DisplayName;
	FLinearColor Color = FLinearColor::White;
	int32 Cost = 25;
	int32 Energy = 2;
	float BaseRange = 16.0f;  // m
	float RangeUp = 2.0f;     // m par niveau
	float MinRange = 0.0f;    // m
	float Rate = 0.2f;
	float BaseDamage = 10.0f;
	float FreezeTime = 2.5f;
	float BurnDps = 6.0f;
	int32 Chain = 3;
	float BlastRadius = 3.5f; // m
	int32 Level = 1;
	int32 MaxLevel = 3;
	float MarkBonus = 1.25f;
	float BeaconReveal = 24.0f; // m
	bool bPowered = false;
	float Cooldown = 0.0f;
	float TargetCooldown = 0.0f;
	float BonusCooldown = 0.0f;
	float RangeMult = 1.0f;
	float DamageMult = 1.0f;
	float FlameOn = 0.0f;
	float SoundCooldown = 0.0f;
	float RevealCooldown = 0.0f;
	float FlashLeft = 0.0f;
	float Time = 0.0f;
	int32 Shots = 0;
};

/** Obus du Mortier (Godot : projectile.gd) : vol en cloche, explosion qui blesse et étourdit. */
UCLASS()
class DERNIERREMPART_API ADRShell : public AActor
{
	GENERATED_BODY()

public:
	ADRShell();

	void Launch(const FVector& To, float InFlight, float InDamage, float InRadius, float InStun);
	virtual void Tick(float DeltaSeconds) override;

private:
	void Explode(const FVector& Where);

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Ball;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Blast;

	FVector Velocity = FVector::ZeroVector;
	float Flight = 1.0f;
	float Age = 0.0f;
	float Damage = 55.0f;
	float Radius = 350.0f; // cm
	float StunTime = 1.2f;
	float BlastLeft = -1.0f;
};
