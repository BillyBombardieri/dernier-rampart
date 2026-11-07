#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DRCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class UPointLightComponent;
class USoundBase;
class UStaticMeshComponent;
struct FInputActionValue;

/** Une arme du joueur : chiffres de scripts/player.gd (WEAPONS), lus dans Content/Data/jeu.json. */
struct FDRWeapon
{
	FString Id;
	FString Name;
	FString Sound;
	float Damage = 10.0f;
	float Rate = 0.2f;     // secondes entre deux tirs
	float Reload = 1.5f;   // secondes
	float Spread = 0.01f;  // radians
	float KickUp = 0.01f;  // montée de la vue au tir (radians)
	float KickSide = 0.005f;
	float Recovery = 6.0f; // vitesse de retour de la vue
	float Punch = 0.5f;    // force du coup dans la main
	int32 Mag = 10;
	bool bAuto = false;
	bool bHeavy = false;
	FVector Muzzle = FVector::ZeroVector;      // sortie du canon, repère de l'arme (cm, avant = X)
	FVector AimPosition = FVector::ZeroVector; // position de l'arme devant l'œil en visée (cm)
	FVector ReloadOffset = FVector::ZeroVector;
	FRotator ReloadRotation = FRotator::ZeroRotator;
};

/**
 * Le joueur, vu à la première personne. Mêmes valeurs que scripts/player.gd dans la version Godot :
 * marche 5,5 m/s, course 8,5 m/s (seulement en avançant), 3,2 m/s en visant, saut à 5,5 m/s,
 * gravité 20 m/s², capsule de 1,8 m, yeux à 1,6 m. Deux armes (pistolet lourd, fusil d'assaut)
 * avec dispersion, recul de la vue, visée, rechargement, flash, traçante et sons.
 * Touches par défaut en AZERTY : ZQSD, Espace, Maj, clic gauche, clic droit, R, & et é (ou molette).
 */
UCLASS()
class DERNIERREMPART_API ADRCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADRCharacter();

	virtual void Tick(float DeltaSeconds) override;

	// Pour l'interface.
	bool HasWeapon() const { return Weapons.IsValidIndex(CurrentWeapon); }
	const FDRWeapon& GetWeapon() const { return Weapons[CurrentWeapon]; }
	int32 GetAmmo() const { return Ammo[CurrentWeapon]; }
	bool IsReloading() const { return ReloadLeft > 0.0f; }
	bool IsAiming() const { return bAiming; }
	float GetSpread() const;
	float GetHitMarkerAge() const { return HitMarkerAge; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;

private:
	void CreateInput();
	void LoadWeapons();
	void BuildWeaponModel(int32 Index, const FString& Model);
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void StartFire();
	void StopFire();
	void StartAim();
	void StopAim();
	void ReloadPressed();
	void SelectPistol();
	void SelectRifle();
	void NextWeapon();
	void SelectWeapon(int32 Index);
	void TryFire();
	void Shoot();
	void StartReload();
	void ApplyRecoil(const FDRWeapon& Weapon);
	void AnimateView(float DeltaSeconds);
	void PlaySfx(const FString& Name, float Decibels, float PitchJitter);
	void SetSpeed(float Speed);
	FRotator ViewRotation() const;

	UPROPERTY(VisibleAnywhere, Category = "Joueur")
	TObjectPtr<UCameraComponent> Camera;

	/** Les armes tenues en main, à 60 % de leur taille comme dans Godot. */
	UPROPERTY(VisibleAnywhere, Category = "Joueur")
	TObjectPtr<USceneComponent> GunRoot;

	UPROPERTY(VisibleAnywhere, Category = "Joueur")
	TObjectPtr<UPointLightComponent> MuzzleLight;

	UPROPERTY(VisibleAnywhere, Category = "Joueur")
	TObjectPtr<UStaticMeshComponent> MuzzleFlash;

	UPROPERTY(VisibleAnywhere, Category = "Joueur")
	TObjectPtr<UStaticMeshComponent> Tracer;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> WeaponModels;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<USoundBase>> Sounds;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Mapping;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Weapon1Action;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Weapon2Action;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> NextWeaponAction;

	/** Multiplicateur de sensibilité de la souris (1 = comme le réglage par défaut de Godot). */
	UPROPERTY(EditAnywhere, Category = "Joueur")
	float Sensitivity = 1.0f;

	TArray<FDRWeapon> Weapons;
	TArray<int32> Ammo;
	int32 CurrentWeapon = 0;
	bool bSprintHeld = false;
	bool bFireHeld = false;
	bool bAimHeld = false;
	bool bAiming = false;
	float FireCooldown = 0.0f;
	float ReloadLeft = 0.0f;
	float SwitchAnim = 0.0f;
	float FlashLeft = 0.0f;
	float HitMarkerAge = 10.0f;

	// Recul et mouvements de la vue et de l'arme (mêmes calculs que player.gd).
	FVector2D Kick = FVector2D::ZeroVector;       // (montée, côté) actuels, en radians
	FVector2D KickTarget = FVector2D::ZeroVector;
	float SinceShot = 1.0f;
	int32 Burst = 0;
	float Recoil = 0.0f;
	float VmKick = 0.0f;
	float VmKickVel = 0.0f;
	float FovPunch = 0.0f;
	float Shake = 0.0f;
	float AimBlend = 0.0f;
	float Bob = 0.0f;
	float StepTime = 0.0f;
	FVector GunBasePosition = FVector::ZeroVector;
	FRotator GunBaseRotation = FRotator::ZeroRotator;
};
