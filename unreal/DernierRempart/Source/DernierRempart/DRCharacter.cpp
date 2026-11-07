#include "DRCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DRData.h"
#include "DRLook.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

// Nommé (et pas anonyme) pour éviter les collisions quand Unreal regroupe les .cpp à la compilation.
namespace DRPlayer
{
	// Valeurs de scripts/player.gd, converties en centimètres (l'unité d'Unreal).
	constexpr float WalkSpeed = 550.0f;
	constexpr float SprintSpeed = 850.0f;
	constexpr float AimSpeed = 320.0f;
	constexpr float JumpVelocity = 550.0f;
	constexpr float Gravity = 2000.0f;
	constexpr float Fov = 78.0f;
	constexpr float AimFov = 55.0f;
	// Godot : la vitesse visée est atteinte en 1/12 de seconde au sol, 4 fois plus lentement en l'air.
	constexpr float Acceleration = 12.0f;
	constexpr float AirAcceleration = 3.0f;
	// Godot tourne de 0,0025 radian par pixel de souris. Unreal multiplie déjà le mouvement de la
	// souris par 0,07 (réglage d'axe par défaut), d'où ce facteur pour garder la même vitesse,
	// baissé de 10 % à la demande de Billy après son premier essai (28 octobre).
	constexpr float DegreesPerMouseUnit = 0.9f * 0.0025f * 57.29578f / 0.07f;
	constexpr float Range = 15000.0f; // portée des balles : 150 m

	// Arme à la hanche, dans le repère de la caméra (Godot : (0.15, -0.13, -0.28) m).
	const FVector HipPosition(28.0, 15.0, -13.0);
	constexpr float GunScale = 0.6f;

	// Godot (x droite, y haut, z arrière, en mètres) -> Unreal (X avant, Y droite, Z haut, en cm).
	FVector FromGodot(double X, double Y, double Z)
	{
		return FVector(-Z, X, Y) * 100.0;
	}

	// Rotation de Godot (radians autour de x, y, z) -> rotation d'Unreal (degrés).
	FRotator RotationFromGodot(double X, double Y, double Z)
	{
		return FRotator(FMath::RadiansToDegrees(X), -FMath::RadiansToDegrees(Y), -FMath::RadiansToDegrees(Z));
	}

	float MoveToward(float From, float To, float Step)
	{
		return From < To ? FMath::Min(From + Step, To) : FMath::Max(From - Step, To);
	}
}

ADRCharacter::ADRCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(40.0f, 90.0f);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	// Yeux à 1,6 m du sol : le centre de la capsule est à 0,9 m.
	Camera->SetRelativeLocation(FVector(0.0, 0.0, 70.0));
	Camera->SetFieldOfView(DRPlayer::Fov);
	Camera->bUsePawnControlRotation = true;

	GunRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Arme"));
	GunRoot->SetupAttachment(Camera);
	GunRoot->SetRelativeLocation(DRPlayer::HipPosition);
	GunRoot->SetRelativeScale3D(FVector(DRPlayer::GunScale));

	// Éclair du coup de feu : une lumière très brève et une petite boule lumineuse au bout du canon.
	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Eclair"));
	MuzzleLight->SetupAttachment(GunRoot);
	MuzzleLight->IntensityUnits = ELightUnits::Candelas;
	MuzzleLight->Intensity = 0.0f;
	MuzzleLight->AttenuationRadius = 700.0f;
	MuzzleLight->LightColor = FColor(255, 178, 89);
	MuzzleLight->CastShadows = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	MuzzleFlash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flamme"));
	MuzzleFlash->SetupAttachment(GunRoot);
	MuzzleFlash->SetStaticMesh(Sphere.Object);
	MuzzleFlash->SetRelativeScale3D(FVector(0.16, 0.09, 0.09));
	MuzzleFlash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MuzzleFlash->SetCastShadow(false);
	MuzzleFlash->SetVisibility(false);

	// Balle traçante : un trait lumineux du canon jusqu'à l'impact, visible un vingtième de seconde.
	Tracer = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tracante"));
	Tracer->SetupAttachment(GetCapsuleComponent());
	Tracer->SetUsingAbsoluteLocation(true);
	Tracer->SetUsingAbsoluteRotation(true);
	Tracer->SetUsingAbsoluteScale(true);
	Tracer->SetStaticMesh(Cube.Object);
	Tracer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tracer->SetCastShadow(false);
	Tracer->SetVisibility(false);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->GravityScale = DRPlayer::Gravity / 980.0f;
	Movement->JumpZVelocity = DRPlayer::JumpVelocity;
	Movement->AirControl = DRPlayer::AirAcceleration / DRPlayer::Acceleration;
	Movement->bOrientRotationToMovement = false;
	SetSpeed(DRPlayer::WalkSpeed);
}

void ADRCharacter::BeginPlay()
{
	Super::BeginPlay();
	LoadWeapons();
	MuzzleFlash->SetMaterial(0, DRLook::Textured(this, TEXT("metal"), 1.0f, FLinearColor(1.0f, 0.75f, 0.35f), 60.0f));
	Tracer->SetMaterial(0, DRLook::Textured(this, TEXT("metal"), 1.0f, FLinearColor(1.0f, 0.85f, 0.6f), 40.0f));
	SelectWeapon(0);
	SwitchAnim = 0.0f;
	GunBasePosition = DRPlayer::HipPosition;
}

void ADRCharacter::SetSpeed(float Speed)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = Speed;
	// On accélère et on freine progressivement : on ne s'arrête pas net.
	Movement->MaxAcceleration = DRPlayer::Acceleration * Speed;
	Movement->BrakingDecelerationWalking = DRPlayer::Acceleration * Speed;
}

// ---------------------------------------------------------------- armes

void ADRCharacter::LoadWeapons()
{
	// Chiffres de jeu.json ; modèles, bout du canon et visée d'après les modèles glTF de Godot.
	struct FModelInfo { const TCHAR* Id; const TCHAR* Model; FVector Muzzle; double SightHeight; double AimDepth; FVector ReloadOffset; FRotator ReloadRotation; };
	const FModelInfo Infos[] = {
		{ TEXT("pistol"), TEXT("arme_pistolet"), DRPlayer::FromGodot(0.0, 0.03, -0.182), 0.0575, -0.22,
			DRPlayer::FromGodot(-0.07, 0.06, 0.04), DRPlayer::RotationFromGodot(0.3, 0.35, 0.35) },
		{ TEXT("rifle"), TEXT("arme_fusil"), DRPlayer::FromGodot(0.0, 0.0, -0.665), 0.0725, -0.13,
			DRPlayer::FromGodot(-0.06, 0.06, 0.02), DRPlayer::RotationFromGodot(0.15, 0.3, 0.4) },
	};
	const TSharedPtr<FJsonObject> Data = DRData::Load(TEXT("jeu.json"));
	const TSharedPtr<FJsonObject>* Arms = nullptr;
	if (Data.IsValid())
	{
		Data->TryGetObjectField(TEXT("armes"), Arms);
	}
	for (const FModelInfo& Info : Infos)
	{
		FDRWeapon Weapon;
		Weapon.Id = Info.Id;
		const TSharedPtr<FJsonObject>* Stats = nullptr;
		if (Arms && (*Arms)->TryGetObjectField(Info.Id, Stats))
		{
			const TSharedPtr<FJsonObject>& S = *Stats;
			S->TryGetStringField(TEXT("name"), Weapon.Name);
			S->TryGetStringField(TEXT("sound"), Weapon.Sound);
			S->TryGetBoolField(TEXT("auto"), Weapon.bAuto);
			S->TryGetBoolField(TEXT("heavy"), Weapon.bHeavy);
			Weapon.Damage = (float)DRData::Number(S, TEXT("damage"), Weapon.Damage);
			Weapon.Rate = (float)DRData::Number(S, TEXT("rate"), Weapon.Rate);
			Weapon.Reload = (float)DRData::Number(S, TEXT("reload"), Weapon.Reload);
			Weapon.Spread = (float)DRData::Number(S, TEXT("spread"), Weapon.Spread);
			Weapon.KickUp = (float)DRData::Number(S, TEXT("kick_up"), Weapon.KickUp);
			Weapon.KickSide = (float)DRData::Number(S, TEXT("kick_side"), Weapon.KickSide);
			Weapon.Recovery = (float)DRData::Number(S, TEXT("recovery"), Weapon.Recovery);
			Weapon.Punch = (float)DRData::Number(S, TEXT("punch"), Weapon.Punch);
			Weapon.Mag = (int32)DRData::Number(S, TEXT("mag"), Weapon.Mag);
		}
		if (Weapon.Name.IsEmpty())
		{
			Weapon.Name = Info.Id;
		}
		Weapon.Muzzle = Info.Muzzle;
		// En visée, le point de visée de l'arme se place au centre de l'écran.
		Weapon.AimPosition = FVector(-Info.AimDepth * 100.0, 0.0, -Info.SightHeight * 100.0 * DRPlayer::GunScale);
		Weapon.ReloadOffset = Info.ReloadOffset;
		Weapon.ReloadRotation = Info.ReloadRotation;
		Weapons.Add(Weapon);
		Ammo.Add(Weapon.Mag);
		BuildWeaponModel(Weapons.Num() - 1, Info.Model);
	}
}

void ADRCharacter::BuildWeaponModel(int32 Index, const FString& Model)
{
	USceneComponent* Holder = NewObject<USceneComponent>(this, *FString::Printf(TEXT("ModeleArme%d"), Index));
	Holder->SetupAttachment(GunRoot);
	Holder->RegisterComponent();
	WeaponModels.Add(Holder);

	const TArray<UStaticMesh*> Meshes = DRLook::StaticMeshes(Model);
	if (Meshes.Num() == 0)
	{
		// Modèle pas encore importé : une silhouette d'arme toute simple, pour voir où on tire.
		UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(this);
		Body->SetupAttachment(Holder);
		Body->SetStaticMesh(Tracer->GetStaticMesh());
		const double Length = Index == 0 ? 25.0 : 95.0;
		Body->SetRelativeLocation(FVector(Length * 0.5 - 15.0, 0.0, 0.0));
		Body->SetRelativeScale3D(FVector(Length, 4.0, 9.0) / 100.0);
		Body->SetMaterial(0, DRLook::Plain(this, FLinearColor(0.08f, 0.08f, 0.08f)));
		Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Body->SetCastShadow(false);
		Body->RegisterComponent();
		return;
	}

	// Le canon pointe vers l'avant de Godot (-Z), qu'Unreal range sur un autre axe à l'import :
	// le côté le plus long depuis l'origine du modèle est celui du canon, on le tourne vers l'avant.
	FBox Bounds(ForceInit);
	for (UStaticMesh* Mesh : Meshes)
	{
		Bounds += Mesh->GetBoundingBox();
	}
	const FVector Size = Bounds.GetSize();
	FVector Barrel = Size.X >= Size.Y
		? FVector(-Bounds.Min.X > Bounds.Max.X ? -1.0 : 1.0, 0.0, 0.0)
		: FVector(0.0, -Bounds.Min.Y > Bounds.Max.Y ? -1.0 : 1.0, 0.0);
	const FRotator Turn(0.0, -FMath::RadiansToDegrees(FMath::Atan2(Barrel.Y, Barrel.X)), 0.0);
	for (UStaticMesh* Mesh : Meshes)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
		Part->SetupAttachment(Holder);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeRotation(Turn);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->RegisterComponent();
	}
}

void ADRCharacter::SelectWeapon(int32 Index)
{
	if (!Weapons.IsValidIndex(Index))
	{
		return;
	}
	const bool bChanged = Index != CurrentWeapon;
	CurrentWeapon = Index;
	for (int32 Other = 0; Other < WeaponModels.Num(); ++Other)
	{
		WeaponModels[Other]->SetVisibility(Other == Index, true);
	}
	MuzzleLight->SetRelativeLocation(Weapons[Index].Muzzle);
	MuzzleFlash->SetRelativeLocation(Weapons[Index].Muzzle);
	if (bChanged)
	{
		ReloadLeft = 0.0f;
		FireCooldown = 0.25f;
		SwitchAnim = 0.3f;
	}
}

void ADRCharacter::SelectPistol()
{
	SelectWeapon(0);
}

void ADRCharacter::SelectRifle()
{
	SelectWeapon(1);
}

void ADRCharacter::NextWeapon()
{
	SelectWeapon(CurrentWeapon == 0 ? 1 : 0);
}

void ADRCharacter::StartFire()
{
	bFireHeld = true;
	// Le pistolet tire un coup par clic ; le fusil tire tant qu'on garde le bouton enfoncé (voir Tick).
	TryFire();
}

void ADRCharacter::StopFire()
{
	bFireHeld = false;
}

void ADRCharacter::StartAim()
{
	bAimHeld = true;
}

void ADRCharacter::StopAim()
{
	bAimHeld = false;
}

void ADRCharacter::ReloadPressed()
{
	if (Weapons.IsValidIndex(CurrentWeapon) && ReloadLeft <= 0.0f && Ammo[CurrentWeapon] < Weapons[CurrentWeapon].Mag)
	{
		StartReload();
	}
}

void ADRCharacter::StartReload()
{
	ReloadLeft = Weapons[CurrentWeapon].Reload;
	PlaySfx(TEXT("reload"), -6.0f, 0.0f);
}

void ADRCharacter::TryFire()
{
	if (Weapons.IsValidIndex(CurrentWeapon) && ReloadLeft <= 0.0f && FireCooldown <= 0.0f && SwitchAnim <= 0.0f)
	{
		Shoot();
	}
}

float ADRCharacter::GetSpread() const
{
	if (!Weapons.IsValidIndex(CurrentWeapon))
	{
		return 0.0f;
	}
	const float Moving = GetVelocity().Size2D() / DRPlayer::WalkSpeed;
	return Weapons[CurrentWeapon].Spread * (bAiming ? 0.25f : 1.0f + Moving) + Recoil * 0.02f;
}

FRotator ADRCharacter::ViewRotation() const
{
	const FRotator Base = Controller ? Controller->GetControlRotation() : GetActorRotation();
	return Base + FRotator(FMath::RadiansToDegrees(Kick.X), -FMath::RadiansToDegrees(Kick.Y), 0.0);
}

void ADRCharacter::Shoot()
{
	const FDRWeapon& Weapon = Weapons[CurrentWeapon];
	if (Ammo[CurrentWeapon] <= 0)
	{
		StartReload();
		return;
	}
	FireCooldown = Weapon.Rate;
	--Ammo[CurrentWeapon];

	// Dispersion : précis en visée, moins en mouvement ou au jugé.
	const float Spread = GetSpread();
	const FRotator Offset(FMath::RadiansToDegrees(FMath::FRandRange(-Spread, Spread)), FMath::RadiansToDegrees(FMath::FRandRange(-Spread, Spread)), 0.0);
	const FVector Direction = (FQuat(ViewRotation()) * FQuat(Offset)).Vector();
	const FVector Start = Camera->GetComponentLocation();
	FVector Impact = Start + Direction * DRPlayer::Range;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DRTir), true, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Impact, ECC_Visibility, Params))
	{
		Impact = Hit.ImpactPoint;
		AActor* Target = Hit.GetActor();
		if (Target && Target->CanBeDamaged() && Target->IsA<APawn>())
		{
			UGameplayStatics::ApplyPointDamage(Target, Weapon.Damage, Direction, Hit, GetController(), this, UDamageType::StaticClass());
			HitMarkerAge = 0.0f;
		}
	}

	// Éclair, flamme, traçante et son.
	MuzzleLight->SetIntensity(1500.0f);
	MuzzleFlash->SetVisibility(true);
	MuzzleFlash->SetRelativeRotation(FRotator(0.0, 0.0, FMath::FRandRange(0.0, 360.0)));
	FlashLeft = 0.045f;
	const FVector From = MuzzleFlash->GetComponentLocation();
	const double Length = FVector::Dist(From, Impact);
	if (Length > 150.0)
	{
		Tracer->SetWorldLocationAndRotation((From + Impact) * 0.5, (Impact - From).Rotation());
		Tracer->SetWorldScale3D(FVector(Length / 100.0, 0.012, 0.012));
		Tracer->SetVisibility(true);
	}
	PlaySfx(Weapon.Sound, -2.0f, 0.06f);
	ApplyRecoil(Weapon);
	if (Ammo[CurrentWeapon] <= 0)
	{
		StartReload();
	}
}

void ADRCharacter::ApplyRecoil(const FDRWeapon& Weapon)
{
	// La vue monte vite puis revient doucement. Au fusil, la montée grandit pendant une rafale et
	// dérive d'un côté puis de l'autre : il faut tirer par courtes rafales ou compenser à la souris.
	Burst = SinceShot < 0.3f ? Burst + 1 : 1;
	SinceShot = 0.0f;
	const float Control = bAiming ? 0.65f : 1.0f;
	float Up = Weapon.KickUp * Control;
	float Side = Weapon.KickSide * Control;
	if (Weapon.Id == TEXT("rifle"))
	{
		Up *= 1.0f + FMath::Min(Burst, 12) * 0.06f;
		Side *= FMath::Sin(Burst * 0.7f) * 1.5f + FMath::FRandRange(-0.5f, 0.5f);
	}
	else
	{
		Side *= FMath::FRandRange(-1.0f, 1.0f);
	}
	KickTarget += FVector2D(Up, Side);
	KickTarget.X = FMath::Min(KickTarget.X, 0.35);
	Recoil = FMath::Min(Recoil + Weapon.Punch * 0.5f, 1.0f);
	VmKickVel += Weapon.Punch * 2.2f;
	FovPunch = Weapon.Punch * 1.6f;
	Shake = FMath::Min(1.0f, Shake + Weapon.Punch * 0.08f);
}

void ADRCharacter::PlaySfx(const FString& Name, float Decibels, float PitchJitter)
{
	if (Name.IsEmpty())
	{
		return;
	}
	TObjectPtr<USoundBase>* Found = Sounds.Find(Name);
	USoundBase* Sound = Found ? Found->Get() : nullptr;
	if (!Found)
	{
		Sound = DRLook::Sound(Name);
		Sounds.Add(Name, Sound);
	}
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound, DRLook::Volume(Decibels), 1.0f + FMath::FRandRange(-PitchJitter, PitchJitter));
	}
}

// ---------------------------------------------------------------- chaque image

void ADRCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Weapons.IsValidIndex(CurrentWeapon))
	{
		return;
	}
	FireCooldown -= DeltaSeconds;
	HitMarkerAge += DeltaSeconds;
	if (ReloadLeft > 0.0f)
	{
		ReloadLeft -= DeltaSeconds;
		if (ReloadLeft <= 0.0f)
		{
			ReloadLeft = 0.0f;
			Ammo[CurrentWeapon] = Weapons[CurrentWeapon].Mag;
		}
	}
	bAiming = bAimHeld && ReloadLeft <= 0.0f;
	if (bFireHeld && Weapons[CurrentWeapon].bAuto)
	{
		TryFire();
	}
	if (FlashLeft > 0.0f)
	{
		FlashLeft -= DeltaSeconds;
		if (FlashLeft <= 0.0f)
		{
			MuzzleLight->SetIntensity(0.0f);
			MuzzleFlash->SetVisibility(false);
			Tracer->SetVisibility(false);
		}
	}
	AnimateView(DeltaSeconds);
}

void ADRCharacter::AnimateView(float DeltaSeconds)
{
	// Mêmes calculs que _animate_view dans player.gd. Pas de temps plafonné pour que le ressort
	// de l'arme reste stable si une image prend du retard.
	const float Dt = FMath::Min(DeltaSeconds, 1.0f / 30.0f);
	const FDRWeapon& Weapon = Weapons[CurrentWeapon];
	Recoil = DRPlayer::MoveToward(Recoil, 0.0f, Dt * 4.0f);
	SinceShot += Dt;
	if (SinceShot > 0.08f)
	{
		KickTarget = FMath::Lerp(KickTarget, FVector2D::ZeroVector, FMath::Min(1.0f, Dt * Weapon.Recovery));
	}
	Kick = FMath::Lerp(Kick, KickTarget, FMath::Min(1.0f, Dt * 30.0f));
	// Ressort de l'arme : recule d'un coup, rebondit légèrement puis se stabilise.
	VmKickVel += (-VmKick * 260.0f - VmKickVel * 22.0f) * Dt;
	VmKick += VmKickVel * Dt;
	FovPunch = DRPlayer::MoveToward(FovPunch, 0.0f, Dt * 12.0f);
	Shake = DRPlayer::MoveToward(Shake, 0.0f, Dt * 2.5f);
	SwitchAnim = DRPlayer::MoveToward(SwitchAnim, 0.0f, Dt);
	AimBlend = DRPlayer::MoveToward(AimBlend, bAiming ? 1.0f : 0.0f, Dt * 7.0f);
	Camera->SetFieldOfView(FMath::Lerp(DRPlayer::Fov, DRPlayer::AimFov, AimBlend) + FovPunch);

	// Balancement de la tête et bruits de pas : un pas par demi-oscillation, cadence réaliste.
	const float Speed = GetVelocity().Size2D() / 100.0f;
	const bool bOnFloor = !GetCharacterMovement()->IsFalling();
	if (bOnFloor && Speed > 0.8f)
	{
		const bool bRunning = Speed > 6.0f;
		// Marche : environ 2 pas par seconde. Course : environ 2,9 pas par seconde.
		const float Interval = bRunning ? 0.34f : (bAiming ? 0.62f : 0.5f);
		Bob += Dt * PI / Interval;
		StepTime += Dt;
		if (StepTime >= Interval)
		{
			StepTime = 0.0f;
			const FString Step = FString(bRunning ? TEXT("step_run_") : TEXT("step_walk_")) + FString::FromInt(FMath::RandRange(0, 3));
			PlaySfx(Step, bRunning ? -9.0f : (bAiming ? -17.0f : -14.0f), 0.06f);
		}
	}
	else
	{
		StepTime = 0.3f;
	}
	const float BobAmount = (bAiming ? 0.006f : 0.03f) * FMath::Clamp(Speed / 5.5f, 0.0f, 1.4f) * 100.0f;
	const FVector ShakeOffset = FVector(0.0, FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f)) * Shake * 5.0f;
	Camera->ClearAdditiveOffset();
	Camera->AddAdditiveOffset(FTransform(
		FRotator(FMath::RadiansToDegrees(Kick.X), -FMath::RadiansToDegrees(Kick.Y), 0.0),
		FVector(0.0, FMath::Cos(Bob * 0.5f) * BobAmount * 0.6f, FMath::Abs(FMath::Sin(Bob)) * BobAmount) + ShakeOffset), 0.0f);

	// Position de l'arme : à la hanche, en visée, en rechargement, au changement d'arme.
	FVector Position = FMath::Lerp(DRPlayer::HipPosition, Weapon.AimPosition, AimBlend);
	Position += FVector(0.0, FMath::Cos(Bob * 0.5f) * BobAmount * 0.5f, -FMath::Abs(FMath::Sin(Bob)) * BobAmount * 0.5f);
	FRotator Rotation(0.0, -FMath::RadiansToDegrees(Kick.Y * 2.0f), -FMath::RadiansToDegrees(FMath::Sin(Bob) * BobAmount / 100.0f * 0.6f));
	if (ReloadLeft > 0.0f)
	{
		// L'arme remonte et pivote vers la main gauche, puis le chargeur claque en place.
		const float T = FMath::Clamp(1.0f - ReloadLeft / Weapon.Reload, 0.0f, 1.0f);
		const float K = FMath::SmoothStep(0.0f, 0.14f, T) * (1.0f - FMath::SmoothStep(0.86f, 1.0f, T));
		const float Seat = FMath::Max(0.0f, 1.0f - FMath::Abs(T - 0.8f) / 0.04f);
		Position += Weapon.ReloadOffset * K + FVector(0.0, 0.0, 1.2 * Seat);
		Rotation += Weapon.ReloadRotation * K + FRotator(FMath::RadiansToDegrees(0.06f * Seat), 0.0, 0.0);
	}
	if (SwitchAnim > 0.0f)
	{
		Position.Z -= SwitchAnim * 60.0f;
	}
	const float Follow = FMath::Min(1.0f, Dt * 18.0f);
	GunBasePosition = FMath::Lerp(GunBasePosition, Position, Follow);
	GunBaseRotation = FMath::Lerp(GunBaseRotation, Rotation, Follow);
	// Le coup de recul s'ajoute sans lissage pour rester sec.
	GunRoot->SetRelativeLocationAndRotation(
		GunBasePosition + FVector(-VmKick * 6.0f, 0.0, VmKick * 1.2f),
		GunBaseRotation + FRotator(FMath::RadiansToDegrees(VmKick * 0.35f), 0.0, 0.0));
}

void ADRCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	PlaySfx(TEXT("land"), -6.0f, 0.08f);
	Shake = FMath::Min(1.0f, Shake + 0.15f);
}

// ---------------------------------------------------------------- commandes

void ADRCharacter::CreateInput()
{
	if (Mapping)
	{
		return;
	}
	auto NewAction = [this](const TCHAR* InName, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, InName);
		Action->ValueType = Type;
		return Action;
	};
	MoveAction = NewAction(TEXT("Avancer"), EInputActionValueType::Axis2D);
	LookAction = NewAction(TEXT("Regarder"), EInputActionValueType::Axis2D);
	JumpAction = NewAction(TEXT("Sauter"), EInputActionValueType::Boolean);
	SprintAction = NewAction(TEXT("Courir"), EInputActionValueType::Boolean);
	FireAction = NewAction(TEXT("Tirer"), EInputActionValueType::Boolean);
	AimAction = NewAction(TEXT("Viser"), EInputActionValueType::Boolean);
	ReloadAction = NewAction(TEXT("Recharger"), EInputActionValueType::Boolean);
	Weapon1Action = NewAction(TEXT("Pistolet"), EInputActionValueType::Boolean);
	Weapon2Action = NewAction(TEXT("Fusil"), EInputActionValueType::Boolean);
	NextWeaponAction = NewAction(TEXT("ArmeSuivante"), EInputActionValueType::Boolean);

	Mapping = NewObject<UInputMappingContext>(this, TEXT("CommandesJoueur"));
	// ZQSD en AZERTY. L'action donne (droite, avant) : Z et S passent sur l'axe avant, S et Q sont inversés.
	auto MapMove = [this](const FKey& Key, bool bForwardAxis, bool bNegative)
	{
		FEnhancedActionKeyMapping& KeyMapping = Mapping->MapKey(MoveAction, Key);
		if (bForwardAxis)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Mapping);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			KeyMapping.Modifiers.Add(Swizzle);
		}
		if (bNegative)
		{
			KeyMapping.Modifiers.Add(NewObject<UInputModifierNegate>(Mapping));
		}
	};
	MapMove(EKeys::Z, true, false);
	MapMove(EKeys::S, true, true);
	MapMove(EKeys::Q, false, true);
	MapMove(EKeys::D, false, false);
	Mapping->MapKey(LookAction, EKeys::Mouse2D);
	Mapping->MapKey(JumpAction, EKeys::SpaceBar);
	Mapping->MapKey(SprintAction, EKeys::LeftShift);
	Mapping->MapKey(SprintAction, EKeys::RightShift);
	Mapping->MapKey(FireAction, EKeys::LeftMouseButton);
	Mapping->MapKey(AimAction, EKeys::RightMouseButton);
	Mapping->MapKey(ReloadAction, EKeys::R);
	// En AZERTY, la touche « 1 » donne « & » et la touche « 2 » donne « é » : on accepte les deux.
	Mapping->MapKey(Weapon1Action, EKeys::Ampersand);
	Mapping->MapKey(Weapon1Action, EKeys::One);
	Mapping->MapKey(Weapon2Action, EKeys::E_AccentAigu);
	Mapping->MapKey(Weapon2Action, EKeys::Two);
	Mapping->MapKey(NextWeaponAction, EKeys::MouseScrollUp);
	Mapping->MapKey(NextWeaponAction, EKeys::MouseScrollDown);
}

void ADRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	CreateInput();

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(Mapping, 0);
		}
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Error, TEXT("DernierRempart : Enhanced Input n'est pas actif, voir Config/DefaultInput.ini."));
		return;
	}
	EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADRCharacter::Move);
	EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADRCharacter::Look);
	EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
	EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &ADRCharacter::StartSprint);
	EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &ADRCharacter::StopSprint);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &ADRCharacter::StartFire);
	EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &ADRCharacter::StopFire);
	EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &ADRCharacter::StartAim);
	EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &ADRCharacter::StopAim);
	EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &ADRCharacter::ReloadPressed);
	EnhancedInput->BindAction(Weapon1Action, ETriggerEvent::Started, this, &ADRCharacter::SelectPistol);
	EnhancedInput->BindAction(Weapon2Action, ETriggerEvent::Started, this, &ADRCharacter::SelectRifle);
	EnhancedInput->BindAction(NextWeaponAction, ETriggerEvent::Started, this, &ADRCharacter::NextWeapon);
}

void ADRCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	// Comme dans Godot, on ne court qu'en avançant et sans viser ; on avance moins vite en visant.
	SetSpeed(bAiming ? DRPlayer::AimSpeed : (bSprintHeld && Axis.Y > 0.0 ? DRPlayer::SprintSpeed : DRPlayer::WalkSpeed));
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void ADRCharacter::Look(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}
	const FVector2D Delta = Value.Get<FVector2D>() * (DRPlayer::DegreesPerMouseUnit * Sensitivity * (bAiming ? 0.6f : 1.0f));
	FRotator Rotation = Controller->GetControlRotation();
	Rotation.Yaw += Delta.X;
	// Tirer la souris vers le bas compense d'abord le recul, puis bouge la visée.
	double PitchDelta = Delta.Y;
	if (PitchDelta < 0.0 && KickTarget.X > 0.0)
	{
		const double Used = FMath::Min((double)KickTarget.X, FMath::DegreesToRadians(-PitchDelta));
		KickTarget.X -= Used;
		Kick.X -= Used;
		PitchDelta += FMath::RadiansToDegrees(Used);
	}
	Rotation.Pitch = FMath::ClampAngle(Rotation.Pitch + PitchDelta, -89.0, 89.0);
	Controller->SetControlRotation(Rotation);
}

void ADRCharacter::StartSprint()
{
	bSprintHeld = true;
}

void ADRCharacter::StopSprint()
{
	bSprintHeld = false;
	SetSpeed(DRPlayer::WalkSpeed);
}
