#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DRCharacter.generated.h"

class ADRBarrier;
class ADRHUD;
class ADRSocket;
class ADRTower;
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
 * 100 PV : à terre, on revient au Cœur après quelques secondes pendant que les tours défendent.
 * Touches par défaut en AZERTY : ZQSD, Espace, Maj, clic gauche, clic droit, R, & et é (ou molette), E construire, améliorer ou réparer, C choisir la tour, X allumer ou
 * éteindre une tour, F marquer un zombie, A et G les gadgets, Entrée pour lancer la vague, Échap ou P pour la pause.
 * Toutes ces touches (sauf Échap et la molette) se changent dans les réglages.
 */
UCLASS()
class DERNIERREMPART_API ADRCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADRCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Santé au maximum (quand une vague est repoussée). */
	void HealFull();

	/** Touches choisies dans les réglages (sauvegarde), appliquées tout de suite. */
	void ApplyBindings();
	/** Sensibilité, champ de vision, axe vertical inversé et volume choisis dans les réglages. */
	void ApplySettings();
	/** Effets des implants et de l'amélioration Vitalité sur la vitesse et les PV. */
	void ApplyImplants();

	// Pour l'interface et les zombies.
	bool IsAlive() const { return bAlive; }
	float GetHp() const { return Hp; }
	float GetMaxHp() const { return MaxHp; }
	float GetRespawnLeft() const { return RespawnLeft; }
	/** Rougeur des bords de l'écran après un coup (0 à 1). */
	float GetHurt() const { return Hurt; }
	/** Ce qu'on peut faire avec ce qu'on vise (construire, améliorer, réparer...), ou vide. */
	const FString& GetHint() const { return Hint; }
	/** Ancrage visé (avec ou sans tour), et tour choisie pour la construction. */
	ADRSocket* GetSightSocket() const { return SightSocket.Get(); }
	int32 GetBuildChoice() const { return BuildChoice; }
	bool HasWeapon() const { return Weapons.IsValidIndex(CurrentWeapon); }
	const FDRWeapon& GetWeapon() const { return Weapons[CurrentWeapon]; }
	int32 GetAmmo() const { return Ammo[CurrentWeapon]; }
	bool IsReloading() const { return ReloadLeft > 0.0f; }
	bool IsAiming() const { return bAiming; }
	float GetSpread() const;
	float GetHitMarkerAge() const { return HitMarkerAge; }
	// Pour l'établi.
	int32 GetWeaponCount() const { return Weapons.Num(); }
	const FDRWeapon& GetWeaponAt(int32 Index) const { return Weapons[Index]; }
	/** Dégâts d'une arme, implants, établi et améliorations compris. */
	float WeaponDamage(int32 Index) const;
	/** Balles par chargeur, avec les chargeurs agrandis à l'établi. */
	int32 MagSize(int32 Index) const;
	int32 GetMagSize() const { return MagSize(CurrentWeapon); }
	/** Munitions de l'arme en main (« standard », « incendiaire », « perforante », « electrique »). */
	FName GetAmmoType() const;
	/** Recharge gratuitement une arme (après l'achat d'un plus grand chargeur). */
	void Refill(const FString& WeaponId);
	/** Position de l'œil et direction du regard, recul compris (pour lancer un gadget). */
	FVector GetEyeLocation() const;
	FVector GetAimDirection() const;

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
	/** Durée de rechargement d'une arme, implants compris. */
	float ReloadTime(int32 Index) const;
	void Die();
	void Respawn();
	void SkipPressed();
	void UpdateInteraction(float DeltaSeconds);
	void Repair(float DeltaSeconds, ADRBarrier* Barrier, bool bCore);
	void InteractPressed();
	void InteractReleased();
	void CyclePressed();
	void WheelUp();
	void WheelDown();
	void CycleBuild(int32 Step);
	void PowerPressed();
	void MarkPressed();
	void Gadget1Pressed();
	void Gadget2Pressed();
	void PausePressed();
	void JumpPressed();
	/** Menu principal ou pause : ni regard ni déplacement. */
	bool IsMenuOpen() const;
	/** L'interface (menus et réglages). */
	ADRHUD* GetScreen() const;
	/** Nom de la touche choisie pour une action, pour les textes d'aide. */
	FString KeyName(const TCHAR* Id) const;
	/** Vivant et partie pas finie : on peut bouger, tirer, recharger. */
	bool CanAct() const;

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

	/** Couleur de la traçante selon les munitions (Godot : _tracer_color). */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UMaterialInterface>> TracerLooks;

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

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SkipAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PreviousWeaponAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CycleAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PowerAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MarkAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PauseAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Gadget1Action;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> Gadget2Action;

	/** Multiplicateur de sensibilité de la souris (1 = comme le réglage par défaut de Godot). */
	UPROPERTY(EditAnywhere, Category = "Joueur")
	float Sensitivity = 1.0f;

	/** Champ de vision sans viser (réglage, 78° par défaut) et axe vertical inversé. */
	float BaseFov = 78.0f;
	bool bInvertY = false;
	/** Implant Cœur de batterie : on court 15 % moins vite. */
	float SpeedMult = 1.0f;

	// Santé (Godot : 100 PV ; retour au Cœur après 5 s, 3 s de plus à chaque nouvelle chute).
	float MaxHp = 100.0f;
	float Hp = 100.0f;
	bool bAlive = true;
	float RespawnLeft = 0.0f;
	int32 Deaths = 0;
	float Hurt = 0.0f;

	// Construction, améliorations et réparations (Godot : _handle_interaction).
	TWeakObjectPtr<ADRSocket> SightSocket;
	bool bBenchInSight = false;
	FString Hint;
	int32 BuildChoice = 0;
	bool bInteractHeld = false;
	float RepairBank = 0.0f;

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
