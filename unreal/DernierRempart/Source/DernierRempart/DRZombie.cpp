#include "DRZombie.h"

#include "Animation/AnimSequence.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DRChapter.h"
#include "DRCharacter.h"
#include "DRData.h"
#include "DRGadgets.h"
#include "DRGameMode.h"
#include "DRLook.h"
#include "DRStructures.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace DRZombieRules
{
	// Godot : scripts/zombie.gd et scripts/zombie_models.gd.
	constexpr float HeadshotMultiplier = 2.0f;
	constexpr float AttackTime = 1.0f;   // un coup par seconde ; l'animation est calée sur cette durée
	constexpr float AttackHit = 0.42f;   // moment du coup dans l'animation
	constexpr float StrikeMargin = 120.0f; // le joueur esquive s'il s'éloigne d'autant pendant l'élan (cm)

	struct FModel { const TCHAR* Name; float Walk; float Run; };
	// Vitesse au sol d'un cycle de marche (et de course) joué à vitesse normale, modèle à l'échelle 1.
	const FModel Models[] = {
		{ TEXT("rodeur"), 1.585f, 0.0f }, { TEXT("rodeur_b"), 1.453f, 0.0f }, { TEXT("rodeur_c"), 1.429f, 0.0f },
		{ TEXT("coureur"), 1.474f, 3.75f }, { TEXT("brute"), 0.923f, 0.0f }, { TEXT("cracheur"), 1.524f, 0.0f },
		{ TEXT("hurleur"), 1.359f, 0.0f }, { TEXT("fouisseur"), 1.970f, 0.0f }, { TEXT("boss"), 0.469f, 0.0f },
	};

	// Talents (Godot : SPIT_RANGE, SCREAM_RADIUS, BUFF_*, DIG_*), distances en cm.
	constexpr double SpitRange = 1400.0;
	constexpr float SpitDuration = 0.9f;
	constexpr float SpitRelease = 0.5f;  // moment où l'acide part dans l'animation
	constexpr double ScreamRadius = 1000.0;
	constexpr float ScreamDuration = 1.3f;
	constexpr float BuffSpeed = 1.3f;
	constexpr float BuffDamage = 1.25f;
	constexpr double DigSpeed = 340.0;
	constexpr double DigDistance = 600.0; // le Fouisseur creuse à cette distance d'une barrière
	constexpr float DigCooldown = 4.0f;
	constexpr double DigDepth = 220.0;    // profondeur où descend le modèle (taille 1)
	constexpr double LureRadius = 1600.0; // un leurre sonore attire les zombies à moins de 16 m
	const FVector MoundScale(2.52, 1.8, 0.42);

	// Vrai si la cible (barrière, Cœur, joueur) existe et tient encore debout.
	bool IsStanding(const AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return false;
		}
		if (const ADRBarrier* Barrier = Cast<ADRBarrier>(Actor))
		{
			return Barrier->IsAlive();
		}
		if (const ADRCore* Core = Cast<ADRCore>(Actor))
		{
			return Core->IsAlive();
		}
		if (const ADRCharacter* Player = Cast<ADRCharacter>(Actor))
		{
			return Player->IsAlive();
		}
		if (const ADRDecoy* Decoy = Cast<ADRDecoy>(Actor))
		{
			return Decoy->IsActive();
		}
		return true;
	}
}

ADRZombie::ADRZombie()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::Disabled;

	// Les zombies se cognent au décor, aux barrières et au joueur, mais pas entre eux (comme dans
	// Godot) ; les balles (canal « Visibility ») touchent leur capsule.
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->InitCapsuleSize(35.0f, 95.0f);
	Capsule->SetCollisionObjectType(ECC_PhysicsBody);
	Capsule->SetCollisionResponseToAllChannels(ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bRunPhysicsWithNoController = true;
	Movement->bOrientRotationToMovement = false;
	Movement->MaxAcceleration = 2000.0f;
	Movement->BrakingDecelerationWalking = 2000.0f;
	Movement->GravityScale = 2000.0f / 980.0f;
	bUseControllerRotationYaw = false;
}

void ADRZombie::Setup(const FString& InType, float HpScale, float SpeedScale, float DamageScale)
{
	Type = InType;
	ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	Chapter = Mode ? Mode->GetChapter() : nullptr;
	float Bulk = 1.0f;
	const TSharedPtr<FJsonObject> Data = Mode ? Mode->GetGameData() : TSharedPtr<FJsonObject>();
	const TSharedPtr<FJsonObject>* Types = nullptr;
	const TSharedPtr<FJsonObject>* Stats = nullptr;
	if (Data.IsValid() && Data->TryGetObjectField(TEXT("zombies"), Types) && (*Types)->TryGetObjectField(Type, Stats))
	{
		MaxHp = (float)DRData::Number(*Stats, TEXT("hp"), MaxHp);
		Speed = (float)DRData::Number(*Stats, TEXT("speed"), Speed);
		Damage = (float)DRData::Number(*Stats, TEXT("damage"), Damage);
		Size = (float)DRData::Number(*Stats, TEXT("size"), Size);
		Bulk = (float)DRData::Number(*Stats, TEXT("bulk"), Bulk);
	}
	MaxHp *= HpScale;
	Hp = MaxHp;
	Speed *= SpeedScale * FMath::FRandRange(0.9f, 1.1f);
	Damage *= DamageScale;
	Lane = FMath::FRandRange(-1.4f, 1.4f);
	GroanCooldown = FMath::FRandRange(2.0f, 8.0f);
	GetCapsuleComponent()->SetCapsuleSize(35.0f * Size * Bulk, 95.0f * Size);
	GetCharacterMovement()->MaxWalkSpeed = Speed * 100.0f;

	// Modèle : plusieurs variantes pour le rôdeur.
	ModelName = Type;
	if (Type == TEXT("rodeur"))
	{
		static const TCHAR* Variants[] = { TEXT("rodeur"), TEXT("rodeur_b"), TEXT("rodeur_c") };
		ModelName = Variants[FMath::RandRange(0, 2)];
	}
	for (const DRZombieRules::FModel& Model : DRZombieRules::Models)
	{
		if (ModelName == Model.Name)
		{
			WalkCycle = Model.Walk;
			RunCycle = Model.Run;
		}
	}
	ModelScale = Size * FMath::FRandRange(0.96f, 1.04f);

	USkeletalMeshComponent* Body = GetMesh();
	USkeletalMesh* SkinnedModel = DRLook::SkeletalMesh(TEXT("zombie_") + ModelName);
	if (SkinnedModel)
	{
		Body->SetSkeletalMeshAsset(SkinnedModel);
		for (const TPair<FString, UAnimSequence*>& Entry : DRLook::Animations(TEXT("zombie_") + ModelName))
		{
			Animations.Add(Entry.Key, Entry.Value);
		}
		// Le modèle regarde vers l'avant de Godot, qu'Unreal range sur un autre axe à l'import :
		// le corps dépasse un peu plus devant (bras, visage) que derrière, on le tourne vers +X.
		const FBoxSphereBounds Bounds = SkinnedModel->GetImportedBounds();
		const bool bDepthOnX = Bounds.BoxExtent.X < Bounds.BoxExtent.Y;
		const double Offset = bDepthOnX ? Bounds.Origin.X : Bounds.Origin.Y;
		FVector Front = bDepthOnX ? FVector(Offset < 0.0 ? -1.0 : 1.0, 0.0, 0.0) : FVector(0.0, Offset < 0.0 ? -1.0 : 1.0, 0.0);
		if (FMath::Abs(Offset) < 0.5)
		{
			Front = FVector(0.0, 1.0, 0.0); // sans indice : glTF +Z devient +Y dans Unreal
		}
		const FRotator Turn(0.0, -FMath::RadiansToDegrees(FMath::Atan2(Front.Y, Front.X)), 0.0);
		const FVector Feet(0.0, 0.0, -95.0 * Size);
		Body->SetRelativeLocationAndRotation(Feet, Turn);
		Body->SetRelativeScale3D(FVector(ModelScale));
		CacheInitialMeshOffset(Feet, Turn);
	}
	else
	{
		// Pas encore importé : un pilier grisâtre à la taille du zombie.
		Placeholder = NewObject<UStaticMeshComponent>(this);
		Placeholder->SetupAttachment(GetCapsuleComponent());
		Placeholder->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Placeholder->SetRelativeScale3D(FVector(0.6 * Size * Bulk, 0.6 * Size * Bulk, 1.8 * Size));
		Placeholder->SetMaterial(0, DRLook::Plain(this, FLinearColor(0.35f, 0.4f, 0.3f)));
		Placeholder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Placeholder->RegisterComponent();
	}
	MeshBase = Body->GetRelativeLocation();
	SpitCooldown = FMath::FRandRange(0.5f, 1.5f);
	ScreamCooldown = FMath::FRandRange(2.0f, 4.0f);
	if (Type == TEXT("fouisseur"))
	{
		// Butte de terre (Godot : _build_mound), cachée tant qu'il marche en surface.
		DigState = EDRDig::Walk;
		Mound = NewObject<UStaticMeshComponent>(this);
		Mound->SetupAttachment(GetCapsuleComponent());
		Mound->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		Mound->SetRelativeLocation(FVector(0.0, 0.0, -95.0 * Size));
		Mound->SetRelativeScale3D(DRZombieRules::MoundScale);
		Mound->SetMaterial(0, DRLook::Textured(this, TEXT("mud"), 1.2f, FLinearColor(0.75f, 0.68f, 0.6f)));
		Mound->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mound->SetVisibility(false);
		Mound->RegisterComponent();
	}
	PlayLoop(TEXT("idle"), 1.0f);
}

double ADRZombie::FlatDistance(const FVector& Point) const
{
	return FVector::Dist2D(GetActorLocation(), Point);
}

double ADRZombie::Progress() const
{
	if (!Chapter)
	{
		return 0.0;
	}
	const TArray<FVector>& Points = Chapter->GetPath();
	if (PathIndex >= Points.Num())
	{
		return 1000.0 * Points.Num();
	}
	return 1000.0 * PathIndex - FlatDistance(Points[PathIndex]) / 100.0;
}

FVector ADRZombie::LanePoint(int32 Index) const
{
	const TArray<FVector>& Points = Chapter->GetPath();
	const FVector Point = Points[Index];
	const FVector Previous = Index > 0 ? Points[Index - 1] : Point;
	const FVector Along = (Point - Previous).GetSafeNormal2D();
	if (Along.IsNearlyZero())
	{
		return Point;
	}
	return Point + FVector(-Along.Y, Along.X, 0.0) * Lane * 100.0;
}

ADRBarrier* ADRZombie::FindBlocker() const
{
	ADRBarrier* Best = nullptr;
	if (!Chapter)
	{
		return Best;
	}
	const double Mine = Progress();
	for (ADRBarrier* Barrier : Chapter->GetBarriers())
	{
		if (Barrier && Barrier->IsAlive() && Mine < Barrier->GetProgress() && (!Best || Barrier->GetProgress() < Best->GetProgress()))
		{
			Best = Barrier;
		}
	}
	return Best;
}

ADRDecoy* ADRZombie::FindLure() const
{
	for (TActorIterator<ADRDecoy> It(GetWorld()); It; ++It)
	{
		ADRDecoy* Decoy = *It;
		if (Decoy->IsActive() && FlatDistance(Decoy->GetActorLocation()) < DRZombieRules::LureRadius && IsReachable(Decoy->GetActorLocation()))
		{
			return Decoy;
		}
	}
	return nullptr;
}

bool ADRZombie::IsReachable(const FVector& Point) const
{
	return !Blocker || !Blocker->IsAlive() || !Blocker->IsBehind(Point);
}

void ADRZombie::Think()
{
	Blocker = FindBlocker();
	Lure = FindLure();
	SpitTarget = Type == TEXT("cracheur") ? FindSpitTarget() : nullptr;
}

float ADRZombie::HeadHeight() const
{
	// Base du crâne (os « head », qui suit l'animation) : au-dessus, c'est un tir à la tête.
	const USkeletalMeshComponent* Body = GetMesh();
	if (Body && Body->GetSkeletalMeshAsset() && Body->GetBoneIndex(TEXT("head")) != INDEX_NONE)
	{
		return (float)Body->GetBoneLocation(TEXT("head")).Z;
	}
	return (float)(GetActorLocation().Z + (162.0 - 95.0) * Size);
}

FVector ADRZombie::GetBarAnchor() const
{
	return GetActorLocation() + FVector(0.0, 0.0, (208.0 - 95.0) * Size);
}

void ADRZombie::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDead)
	{
		// Le corps reste au sol un moment puis s'enfonce et disparaît.
		DeadTime += DeltaSeconds;
		if (DeadTime > DeathLength + 2.5f)
		{
			GetMesh()->AddRelativeLocation(FVector(0.0, 0.0, -40.0 * Size * DeltaSeconds));
		}
		if (DeadTime > DeathLength + 4.5f)
		{
			Destroy();
		}
		return;
	}
	// Barre de vie (Godot : health_bar.gd) : apparaît en 0,15 s, s'estompe en 1 s.
	BarShown = FMath::Max(0.0f, BarShown - DeltaSeconds);
	BarAlpha = BarShown > 0.0f
		? FMath::Min(1.0f, BarAlpha + DeltaSeconds / 0.15f)
		: FMath::Max(0.0f, BarAlpha - DeltaSeconds);
	UpdateStatus(DeltaSeconds);
	if (bDead)
	{
		return;
	}
	if (!Chapter || Chapter->GetPath().Num() < 2)
	{
		return;
	}
	// Le Fouisseur gère seul ses déplacements quand il creuse, passe sous terre ou ressort.
	if (DigState != EDRDig::None && UpdateDig(DeltaSeconds))
	{
		return;
	}
	// Gelé : il avance à 35 % de sa vitesse ; le cri d'un Hurleur le rend plus rapide.
	GetCharacterMovement()->MaxWalkSpeed = CurrentSpeed() * 100.0f;
	AttackCooldown -= DeltaSeconds;
	SpitCooldown -= DeltaSeconds;
	GroanCooldown -= DeltaSeconds;
	if (GroanCooldown <= 0.0f)
	{
		GroanCooldown = FMath::FRandRange(5.0f, 12.0f);
		DRLook::PlayAt(this, FString::Printf(TEXT("groan_%d"), FMath::RandRange(0, 2)), GetActorLocation() + FVector(0.0, 0.0, 60.0 * Size), -4.0f, 0.15f, 4000.0f);
	}
	// Le coup porte quand la main arrive sur la cible, pas au début du geste.
	if (StrikeTarget)
	{
		StrikeTime -= DeltaSeconds;
		if (StrikeTime <= 0.0f)
		{
			Strike();
		}
	}
	// Le crachat part quand la tête se projette en avant.
	if (SpitAim)
	{
		SpitTime -= DeltaSeconds;
		if (SpitTime <= 0.0f)
		{
			ReleaseSpit();
		}
	}
	HoldTime = FMath::Max(0.0f, HoldTime - DeltaSeconds);
	// Étourdi : il reste sur place, sans frapper.
	if (StunTime > 0.0f)
	{
		UpdateAnimation(DeltaSeconds);
		return;
	}
	ThinkCooldown -= DeltaSeconds;
	if (ThinkCooldown <= 0.0f)
	{
		ThinkCooldown = FMath::FRandRange(0.2f, 0.3f);
		Think();
	}
	if (Type == TEXT("hurleur"))
	{
		ScreamCooldown -= DeltaSeconds;
		if (ScreamCooldown <= 0.0f)
		{
			ScreamCooldown = 5.0f;
			Scream();
		}
	}

	AActor* Target = nullptr;
	FVector TargetPosition = GetActorLocation();
	double Reach = (1.4 + 0.4 * Size) * 100.0;
	bool bHold = HoldTime > 0.0f;
	const double BarrierReach = (0.9 + 0.35 * Size) * 100.0;
	const double ChaseRadius = (Type == TEXT("coureur") ? 18.0 : (Type == TEXT("cracheur") ? 4.0 : 7.0)) * 100.0;
	ADRCharacter* Player = Cast<ADRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const bool bChase = Player && Player->IsAlive() && IsReachable(Player->GetActorLocation()) && FlatDistance(Player->GetActorLocation()) < ChaseRadius;
	const TArray<FVector>& Points = Chapter->GetPath();
	ADRDecoy* Bait = Lure.Get();
	if (Bait && Bait->IsActive())
	{
		// Le leurre sonore passe avant tout : il fonce dessus et le frappe.
		Target = Bait;
		TargetPosition = Bait->GetActorLocation();
		Reach = (1.2 + 0.3 * Size) * 100.0;
	}
	else if (bChase)
	{
		Target = Player;
		TargetPosition = Player->GetActorLocation();
	}
	else if (Type == TEXT("cracheur") && DRZombieRules::IsStanding(SpitTarget.Get()))
	{
		// Reste à distance et crache.
		bHold = true;
		TargetPosition = AimPoint(SpitTarget.Get());
		if (SpitCooldown <= 0.0f)
		{
			Spit(SpitTarget.Get());
		}
	}
	else if (Blocker && Blocker->IsAlive() && Blocker->FrontDistance(GetActorLocation()) <= BarrierReach + 5.0)
	{
		Target = Blocker;
		TargetPosition = Blocker->ContactPoint(GetActorLocation());
		Reach = BarrierReach + 10.0;
	}
	else if (PathIndex < Points.Num())
	{
		TargetPosition = LanePoint(PathIndex);
		if (FlatDistance(TargetPosition) < 120.0)
		{
			++PathIndex;
		}
	}
	else if (ADRCore* Core = Chapter->GetCore())
	{
		if (Core->IsAlive())
		{
			Target = Core;
		}
		TargetPosition = Core->GetActorLocation();
		Reach = (4.0 + 0.4 * Size) * 100.0;
	}

	FVector Direction = TargetPosition - GetActorLocation();
	Direction.Z = 0.0;
	if (bHold)
	{
		// Sur place : le Hurleur crie, le Cracheur vise (il se tourne quand même vers sa cible).
	}
	else if (Target && FlatDistance(TargetPosition) <= Reach)
	{
		if (AttackCooldown <= 0.0f)
		{
			Swing(Target);
		}
	}
	else if (Direction.Size() > 5.0 && ActionTime <= 0.0f)
	{
		AddMovementInput(Direction.GetSafeNormal());
	}
	if (Direction.Size() > 5.0)
	{
		const FRotator Wanted(0.0, Direction.Rotation().Yaw, 0.0);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), Wanted, DeltaSeconds, 6.0f));
	}
	UpdateAnimation(DeltaSeconds);
}

void ADRZombie::Swing(AActor* Target)
{
	AttackCooldown = DRZombieRules::AttackTime;
	PlayAction(TEXT("attack"), DRZombieRules::AttackTime);
	StrikeTarget = Target;
	StrikeTime = DRZombieRules::AttackTime * DRZombieRules::AttackHit;
}

void ADRZombie::Strike()
{
	AActor* Target = StrikeTarget;
	StrikeTarget = nullptr;
	if (!IsValid(Target))
	{
		return;
	}
	if (ADRCharacter* Player = Cast<ADRCharacter>(Target))
	{
		// Le joueur qui recule pendant l'élan esquive le coup.
		if (!Player->IsAlive() || FlatDistance(Player->GetActorLocation()) > (1.4 + 0.4 * Size) * 100.0 + DRZombieRules::StrikeMargin)
		{
			return;
		}
	}
	UGameplayStatics::ApplyDamage(Target, CurrentDamage(), nullptr, this, UDamageType::StaticClass());
}

float ADRZombie::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead || bBurrowed || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}
	float Amount = DamageAmount;
	const bool bFromPlayer = EventInstigator && EventInstigator->IsPlayerController();
	ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	// Munitions de l'arme du joueur (établi) : les dégâts sont appliqués pendant son tir.
	const ADRCharacter* Shooter = bFromPlayer ? Cast<ADRCharacter>(EventInstigator->GetPawn()) : nullptr;
	const FName AmmoType = Shooter ? Shooter->GetAmmoType() : FName(TEXT("standard"));
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& Point = static_cast<const FPointDamageEvent&>(DamageEvent);
		const FVector Where = Point.HitInfo.ImpactPoint;
		LastHitDirection = FVector(-Point.HitInfo.ImpactNormal.X, -Point.HitInfo.ImpactNormal.Y, 0.0);
		if (bFromPlayer && Where.Z > HeadHeight() - 2.0f * Size)
		{
			Amount *= DRZombieRules::HeadshotMultiplier;
			if (Mode)
			{
				Mode->Popup(TEXT("T\u00CATE"), Where + FVector(0.0, 0.0, 40.0), FLinearColor(1.0f, 0.4f, 0.3f), 40.0f);
			}
		}
		if (bFromPlayer)
		{
			DRLook::PlayAt(this, TEXT("hit_flesh"), Where, -2.0f, 0.15f);
		}
	}
	// Combo BRISÉ : un tir d'arme lourde sur un zombie gelé fait trois fois plus de dégâts (cinq
	// avec l'implant Sang froid).
	const bool bHeavy = DamageEvent.DamageTypeClass && DamageEvent.DamageTypeClass->IsChildOf(UDRHeavyDamage::StaticClass());
	if (bFromPlayer && bHeavy && FrozenTime > 0.0f)
	{
		Amount *= (Mode && Mode->HasImplant(TEXT("sang_froid"))) ? 5.0f : 3.0f;
		FrozenTime = 0.0f;
		if (Mode)
		{
			Mode->AddScore(10);
			Mode->Popup(TEXT("BRIS\u00C9 !"), GetActorLocation() + FVector(0.0, 0.0, (220.0 - 95.0) * Size), FLinearColor(0.6f, 0.9f, 1.0f), 64.0f);
		}
	}
	// Combo SURCHARGE : un tir (pas électrique) sur un zombie chargé libère une onde électrique autour de lui.
	if (bFromPlayer && ChargedTime > 0.0f && AmmoType != FName(TEXT("electrique")))
	{
		Amount += Surcharge();
	}
	Hp -= Amount;
	BarShown = 6.0f;
	if (Hp <= 0.0f)
	{
		Die();
		return Amount;
	}
	// Munitions incendiaires : mettent le feu ; électriques : chargent la cible pour la SURCHARGE.
	if (bFromPlayer && AmmoType == FName(TEXT("incendiaire")))
	{
		Ignite(3.0f, 6.0f * (Mode ? Mode->ComboScale() : 1.0f));
	}
	else if (bFromPlayer && AmmoType == FName(TEXT("electrique")))
	{
		Charge(3.0f);
	}
	return Amount;
}

void ADRZombie::Die()
{
	bDead = true;
	Hp = 0.0f;
	BarShown = 0.0f;
	BarAlpha = 0.0f;
	MarkedTime = FrozenTime = BurnTime = ChargedTime = StunTime = BuffTime = 0.0f;
	StrikeTarget = nullptr;
	SpitAim = nullptr;
	SpitTarget = nullptr;
	// Un Fouisseur tué en sortant de terre remonte d'un coup.
	bBurrowed = false;
	DigState = EDRDig::None;
	SetRigDepth(0.0f);
	GetMesh()->SetVisibility(true, true);
	if (Placeholder)
	{
		Placeholder->SetVisibility(true);
	}
	if (Mound)
	{
		Mound->SetVisibility(false);
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	DRLook::PlayAt(this, TEXT("zombie_death"), GetActorLocation(), 0.0f, 0.2f);
	// Il s'effondre en arrière s'il est touché de face ; sans tir connu, le hasard décide.
	const bool bBackwards = LastHitDirection.IsNearlyZero()
		? FMath::FRand() < 0.6f
		: FVector::DotProduct(LastHitDirection, GetActorForwardVector()) < 0.0;
	const FString Name = bBackwards ? TEXT("die") : TEXT("die_front");
	if (const TObjectPtr<UAnimSequence>* Found = Animations.Find(Name))
	{
		GetMesh()->PlayAnimation(Found->Get(), false);
		GetMesh()->SetPlayRate(1.0f);
		DeathLength = (*Found)->GetPlayLength();
	}
	else if (Placeholder)
	{
		Placeholder->SetRelativeRotation(FRotator(bBackwards ? 80.0 : -80.0, 0.0, 0.0));
	}
	if (ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>())
	{
		Mode->ZombieDied(this);
	}
}

void ADRZombie::PlayLoop(const FString& Name, float Rate)
{
	const TObjectPtr<UAnimSequence>* Found = Animations.Find(Name);
	if (!Found)
	{
		return;
	}
	if (CurrentAnimation != Name)
	{
		GetMesh()->PlayAnimation(Found->Get(), true);
		CurrentAnimation = Name;
	}
	GetMesh()->SetPlayRate(FMath::Clamp(Rate, 0.3f, 2.5f));
}

void ADRZombie::PlayAction(const FString& Name, float Duration)
{
	const TObjectPtr<UAnimSequence>* Found = Animations.Find(Name);
	if (!Found)
	{
		return;
	}
	// Joue le geste en entier, calé sur la durée voulue, puis la marche reprend.
	GetMesh()->PlayAnimation(Found->Get(), false);
	GetMesh()->SetPlayRate((*Found)->GetPlayLength() / Duration);
	CurrentAnimation = Name;
	ActionTime = Duration - 0.12f;
}

void ADRZombie::UpdateAnimation(float DeltaSeconds)
{
	if (ActionTime > 0.0f)
	{
		ActionTime -= DeltaSeconds;
		return;
	}
	// Marche (ou course) jouée à la vitesse du déplacement : les pieds ne glissent pas sur le sol.
	const float Moving = GetVelocity().Size2D() / 100.0f;
	if (Moving > 0.1f)
	{
		const float Run = RunCycle * ModelScale;
		if (Run > 0.0f && Moving > 0.6f * Run && Animations.Contains(TEXT("run")))
		{
			PlayLoop(TEXT("run"), Moving / Run);
		}
		else
		{
			PlayLoop(TEXT("walk"), Moving / (WalkCycle * ModelScale));
		}
	}
	else
	{
		PlayLoop(TEXT("idle"), StunTime > 0.0f ? 0.5f : 1.0f);
	}
}

// ---------------------------------------------------------------- états

void ADRZombie::Mark()
{
	if (!bDead && !bBurrowed)
	{
		MarkedTime = 8.0f;
	}
}

void ADRZombie::Freeze(float Duration)
{
	if (!bDead && !bBurrowed)
	{
		FrozenTime = FMath::Max(FrozenTime, Duration);
	}
}

void ADRZombie::Ignite(float Duration, float Dps)
{
	if (!bDead && !bBurrowed)
	{
		BurnTime = FMath::Max(BurnTime, Duration);
		BurnDps = FMath::Max(BurnDps, Dps);
	}
}

void ADRZombie::Charge(float Duration)
{
	if (!bDead && !bBurrowed)
	{
		ChargedTime = FMath::Max(ChargedTime, Duration);
	}
}

void ADRZombie::Stun(float Duration)
{
	// Le boss ne se laisse pas étourdir. Un coup ou un crachat en préparation est interrompu.
	if (bDead || bBurrowed || Type == TEXT("boss"))
	{
		return;
	}
	StunTime = FMath::Max(StunTime, Duration);
	StrikeTarget = nullptr;
	SpitAim = nullptr;
	ActionTime = 0.0f;
}

void ADRZombie::UpdateStatus(float DeltaSeconds)
{
	FrozenTime = FMath::Max(0.0f, FrozenTime - DeltaSeconds);
	MarkedTime = FMath::Max(0.0f, MarkedTime - DeltaSeconds);
	StunTime = FMath::Max(0.0f, StunTime - DeltaSeconds);
	ChargedTime = FMath::Max(0.0f, ChargedTime - DeltaSeconds);
	BuffTime = FMath::Max(0.0f, BuffTime - DeltaSeconds);
	if (BurnTime > 0.0f)
	{
		// Le feu ronge ses PV par petites bouchées, quatre fois par seconde.
		BurnTime -= DeltaSeconds;
		BurnTick += DeltaSeconds;
		if (BurnTick >= 0.25f)
		{
			ApplyDot(BurnDps * BurnTick);
			BurnTick = 0.0f;
		}
	}
	else
	{
		BurnDps = 0.0f;
	}
}

void ADRZombie::ApplyDot(float Amount)
{
	if (bDead || bBurrowed)
	{
		return;
	}
	Hp -= Amount;
	BarShown = 6.0f;
	if (Hp <= 0.0f)
	{
		Die();
	}
}

float ADRZombie::Surcharge()
{
	// Décharge la cible, blesse et étourdit les zombies à moins de 5 m (Godot : _surcharge).
	ChargedTime = 0.0f;
	ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	const float Damage45 = 45.0f * (Mode ? Mode->ComboScale() : 1.0f);
	if (Mode)
	{
		Mode->AddScore(10);
		Mode->Popup(TEXT("SURCHARGE !"), GetActorLocation() + FVector(0.0, 0.0, (240.0 - 95.0) * Size), FLinearColor(0.75f, 0.65f, 1.0f), 64.0f);
	}
	DRLook::PlayAt(this, TEXT("zap"), GetActorLocation() + FVector(0.0, 0.0, 30.0 * Size), 3.0f, 0.1f);
	TArray<ADRZombie*> Around;
	for (TActorIterator<ADRZombie> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->IsDead() && !It->IsBurrowed() && FlatDistance(It->GetActorLocation()) < 500.0)
		{
			Around.Add(*It);
		}
	}
	for (ADRZombie* Other : Around)
	{
		Other->Stun(0.8f);
		UGameplayStatics::ApplyDamage(Other, Damage45, nullptr, this, UDamageType::StaticClass());
	}
	Stun(0.8f);
	return Damage45;
}

// ---------------------------------------------------------------- talents

void ADRZombie::Buff(float Duration)
{
	if (!bDead)
	{
		BuffTime = FMath::Max(BuffTime, Duration);
	}
}

float ADRZombie::CurrentSpeed() const
{
	return Speed * (FrozenTime > 0.0f ? 0.35f : 1.0f) * (BuffTime > 0.0f ? DRZombieRules::BuffSpeed : 1.0f);
}

float ADRZombie::CurrentDamage() const
{
	return Damage * (BuffTime > 0.0f ? DRZombieRules::BuffDamage : 1.0f);
}

AActor* ADRZombie::FindSpitTarget() const
{
	// Le Cracheur crache de loin sur la barrière qui le bloque, ou sur le joueur s'il est assez près.
	// L'acide passe par-dessus la horde. Il ne vise jamais les tours.
	AActor* Best = nullptr;
	double BestScore = TNumericLimits<double>::Max();
	if (Blocker && Blocker->IsAlive())
	{
		const double Front = Blocker->FrontDistance(GetActorLocation());
		if (Front >= 0.0 && Front <= DRZombieRules::SpitRange * 0.6)
		{
			BestScore = Front;
			Best = Blocker.Get();
		}
	}
	ADRCharacter* Player = Cast<ADRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (Player && Player->IsAlive())
	{
		const double ToPlayer = FlatDistance(Player->GetActorLocation());
		if (ToPlayer <= 1100.0 && ToPlayer + 200.0 < BestScore)
		{
			Best = Player;
		}
	}
	return Best;
}

FVector ADRZombie::AimPoint(const AActor* Aimed) const
{
	// 1 m au-dessus du sol : milieu de la barrière, ou poitrine du joueur.
	if (const ADRBarrier* Barrier = Cast<ADRBarrier>(Aimed))
	{
		return Barrier->ContactPoint(GetActorLocation()) + FVector(0.0, 0.0, 100.0);
	}
	return Aimed->GetActorLocation() + FVector(0.0, 0.0, 10.0);
}

void ADRZombie::Spit(AActor* Aimed)
{
	// Il se cambre en gonflant sa poche, puis projette la tête en avant : l'acide part à ce moment.
	SpitCooldown = 2.6f;
	PlayAction(TEXT("spit"), DRZombieRules::SpitDuration);
	SpitAim = Aimed;
	SpitTime = DRZombieRules::SpitDuration * DRZombieRules::SpitRelease;
}

void ADRZombie::ReleaseSpit()
{
	AActor* Aimed = SpitAim.Get();
	SpitAim = nullptr;
	if (!DRZombieRules::IsStanding(Aimed))
	{
		return;
	}
	const FVector Mouth = GetActorLocation() + FVector(0.0, 0.0, 162.0 * ModelScale - 95.0 * Size) + GetActorForwardVector() * 30.0;
	const FVector To = AimPoint(Aimed);
	const float FlightTime = FMath::Clamp((float)(FVector::Dist(Mouth, To) / 1100.0), 0.5f, 1.4f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ADRAcid* Acid = GetWorld()->SpawnActor<ADRAcid>(ADRAcid::StaticClass(), Mouth, FRotator::ZeroRotator, Params))
	{
		Acid->Launch(To, FlightTime, CurrentDamage(), Aimed);
	}
	DRLook::PlayAt(this, TEXT("spit"), Mouth, -1.0f, 0.15f);
}

void ADRZombie::Scream()
{
	// Cri du Hurleur : il s'arrête, et les zombies à moins de 10 m sont plus rapides et plus forts pendant 6 s.
	PlayAction(TEXT("scream"), DRZombieRules::ScreamDuration);
	HoldTime = DRZombieRules::ScreamDuration;
	for (TActorIterator<ADRZombie> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->IsDead() && FlatDistance(It->GetActorLocation()) < DRZombieRules::ScreamRadius)
		{
			It->Buff(6.0f);
		}
	}
	const FVector Feet = GetActorLocation() - FVector(0.0, 0.0, 95.0 * Size);
	if (ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>())
	{
		Mode->AddRing(Feet + FVector(0.0, 0.0, 25.0), (float)DRZombieRules::ScreamRadius, FLinearColor(1.0f, 0.35f, 0.15f, 0.8f), 0.6f);
	}
	DRLook::PlayAt(this, TEXT("howl"), Feet + FVector(0.0, 0.0, 200.0), 3.0f, 0.1f, 8000.0f);
}

// ---------------------------------------------------------------- Fouisseur

bool ADRZombie::UpdateDig(float DeltaSeconds)
{
	switch (DigState)
	{
	case EDRDig::Walk:
		// En surface : il creuse dès qu'il arrive devant une barrière debout.
		DigTimer -= DeltaSeconds;
		if (DigTimer <= 0.0f && GetCharacterMovement()->IsMovingOnGround() && StunTime <= 0.0f && WantsToDig())
		{
			StartDig();
			return true;
		}
		return false;
	case EDRDig::Dig:
		DigTimer -= DeltaSeconds;
		SetRigDepth(FMath::Lerp((float)(-DRZombieRules::DigDepth * Size), 0.0f, FMath::Clamp(DigTimer, 0.0f, 1.0f)));
		if (DigTimer <= 0.0f)
		{
			// Sous terre : invisible, intouchable, il traverse tout en ligne droite.
			DigState = EDRDig::Under;
			bBurrowed = true;
			MarkedTime = 0.0f;
			GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GetCharacterMovement()->DisableMovement();
			GetMesh()->SetVisibility(false, true);
			if (Placeholder)
			{
				Placeholder->SetVisibility(false);
			}
			Mound->SetVisibility(true);
		}
		return true;
	case EDRDig::Under:
	{
		UnderTime += DeltaSeconds;
		FVector To = DigDestination - GetActorLocation();
		To.Z = 0.0;
		const double Left = To.Size();
		if (Left < 60.0 || UnderTime > 25.0f)
		{
			StartRise();
			return true;
		}
		SetActorLocation(GetActorLocation() + To / Left * FMath::Min(Left, DRZombieRules::DigSpeed * DeltaSeconds));
		SetActorRotation(FRotator(0.0, To.Rotation().Yaw, 0.0));
		// La butte palpite un peu pendant qu'elle avance.
		Mound->SetRelativeScale3D(DRZombieRules::MoundScale * (1.0 + FMath::Sin(GetWorld()->GetTimeSeconds() * 9.0) * 0.06));
		RumbleCooldown -= DeltaSeconds;
		if (RumbleCooldown <= 0.0f)
		{
			RumbleCooldown = 1.3f;
			DRLook::PlayAt(this, TEXT("dig"), GetActorLocation(), -8.0f, 0.2f, 3500.0f);
		}
		return true;
	}
	case EDRDig::Rise:
		DigTimer -= DeltaSeconds;
		SetRigDepth(FMath::Lerp(0.0f, (float)(-DRZombieRules::DigDepth * Size), FMath::Clamp(DigTimer / 0.9f, 0.0f, 1.0f)));
		if (DigTimer <= 0.0f)
		{
			DigState = EDRDig::Walk;
			DigTimer = DRZombieRules::DigCooldown;
			SetRigDepth(0.0f);
			SnapToPath();
			UpdateAnimation(DeltaSeconds);
		}
		return true;
	default:
		return false;
	}
}

bool ADRZombie::WantsToDig() const
{
	if (!Blocker || !Blocker->IsAlive())
	{
		return false;
	}
	const double Front = Blocker->FrontDistance(GetActorLocation());
	return Front >= -50.0 && Front <= DRZombieRules::DigDistance;
}

void ADRZombie::StartDig()
{
	// Passe sous la barrière en ligne droite et ressort juste derrière, côté Cœur.
	DigDestination = Blocker->ContactPoint(GetActorLocation()) + Blocker->GetDirection() * 350.0;
	DigDestination.Z = GetActorLocation().Z;
	DigState = EDRDig::Dig;
	DigTimer = 1.0f;
	UnderTime = 0.0f;
	GetCharacterMovement()->StopMovementImmediately();
	ActionTime = 0.0f;
	StrikeTarget = nullptr;
	PlayLoop(TEXT("dig"), 1.3f);
	DRLook::PlayAt(this, TEXT("dig"), GetActorLocation(), 0.0f, 0.1f);
}

void ADRZombie::StartRise()
{
	DigState = EDRDig::Rise;
	DigTimer = 0.9f;
	bBurrowed = false;
	// Se pose sur le sol là où il ressort.
	const FVector Here = GetActorLocation();
	FHitResult Floor;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DRFouisseur), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Floor, Here + FVector(0.0, 0.0, 50.0), Here - FVector(0.0, 0.0, 600.0), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		SetActorLocation(FVector(Here.X, Here.Y, Floor.ImpactPoint.Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0));
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetMesh()->SetVisibility(true, true);
	if (Placeholder)
	{
		Placeholder->SetVisibility(true);
	}
	Mound->SetVisibility(false);
	PlayLoop(TEXT("dig"), 1.0f);
	DRLook::PlayAt(this, TEXT("dig"), GetActorLocation(), 4.0f, 0.1f);
}

void ADRZombie::ForceSurface()
{
	if (DigState != EDRDig::Under)
	{
		return;
	}
	StartRise();
	if (ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>())
	{
		Mode->Popup(TEXT("D\u00C9BUSQU\u00C9 !"), GetActorLocation() + FVector(0.0, 0.0, 250.0 - 95.0 * Size), FLinearColor(1.0f, 0.9f, 0.5f), 48.0f);
	}
}

void ADRZombie::SnapToPath()
{
	// Après être sorti de terre, il reprend le chemin depuis le tronçon le plus proche.
	const TArray<FVector>& Points = Chapter->GetPath();
	const FVector2D Here(GetActorLocation().X, GetActorLocation().Y);
	double Best = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
	{
		const FVector2D A(Points[Index].X, Points[Index].Y);
		const FVector2D B(Points[Index + 1].X, Points[Index + 1].Y);
		const double Distance = FVector2D::Distance(Here, FMath::ClosestPointOnSegment2D(Here, A, B));
		if (Distance < Best)
		{
			Best = Distance;
			PathIndex = Index + 1;
		}
	}
}

void ADRZombie::SetRigDepth(float Depth)
{
	GetMesh()->SetRelativeLocation(MeshBase + FVector(0.0, 0.0, Depth));
	if (Placeholder)
	{
		Placeholder->SetRelativeLocation(FVector(0.0, 0.0, Depth));
	}
}

// ---------------------------------------------------------------- crachat d'acide

ADRAcid::ADRAcid()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	Ball = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Goutte"));
	Ball->SetupAttachment(RootComponent);
	Ball->SetStaticMesh(SphereMesh.Object);
	Ball->SetRelativeScale3D(FVector(0.32));
	Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ball->SetCastShadow(false);
	Puddle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flaque"));
	Puddle->SetupAttachment(RootComponent);
	Puddle->SetStaticMesh(CylinderMesh.Object);
	Puddle->SetRelativeScale3D(FVector(1.4, 1.4, 0.02));
	Puddle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Puddle->SetCastShadow(false);
	Puddle->SetVisibility(false);
	Shine = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lueur"));
	Shine->SetupAttachment(RootComponent);
	Shine->IntensityUnits = ELightUnits::Candelas;
	Shine->Intensity = 20.0f;
	Shine->AttenuationRadius = 300.0f;
	Shine->LightColor = FColor(115, 242, 51);
	Shine->CastShadows = false;
}

void ADRAcid::Launch(const FVector& To, float InFlight, float InDamage, AActor* InTarget)
{
	Flight = FMath::Max(0.2f, InFlight);
	Damage = InDamage;
	Target = InTarget;
	// Vitesse de départ pour arriver pile sur la cible avec une gravité de 14 m/s² (comme le Mortier).
	const FVector Gravity(0.0, 0.0, -1400.0);
	Velocity = (To - GetActorLocation() - 0.5 * Gravity * Flight * Flight) / Flight;
	Ball->SetMaterial(0, DRLook::Textured(this, TEXT("metal"), 1.0f, FLinearColor(0.45f, 0.95f, 0.2f), 3.0f));
	Puddle->SetMaterial(0, DRLook::Textured(this, TEXT("mud"), 1.0f, FLinearColor(0.5f, 1.0f, 0.25f), 1.5f));
}

void ADRAcid::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (PuddleLeft >= 0.0f)
	{
		// La flaque rétrécit puis s'évapore.
		PuddleLeft -= DeltaSeconds;
		const float Left = FMath::Clamp(PuddleLeft / 3.5f, 0.0f, 1.0f);
		Puddle->SetRelativeScale3D(FVector(1.4 * (0.4 + 0.6 * Left), 1.4 * (0.4 + 0.6 * Left), 0.02));
		if (PuddleLeft <= 0.0f)
		{
			Destroy();
		}
		return;
	}
	Age += DeltaSeconds;
	const FVector From = GetActorLocation();
	Velocity.Z -= 1400.0 * DeltaSeconds;
	const FVector Next = From + Velocity * DeltaSeconds;
	bool bArrived = Age >= Flight;
	FVector Where = Next;
	if (!bArrived)
	{
		// L'acide s'écrase sur le premier obstacle rencontré (sol, mur, barrière, tour, joueur), pas sur les zombies.
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		Objects.AddObjectTypesToQuery(ECC_Pawn);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DRAcide), false, this);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByObjectType(Hit, From, Next, Objects, Params))
		{
			bArrived = true;
			Where = Hit.ImpactPoint;
		}
	}
	SetActorLocation(Next);
	if (!Velocity.IsNearlyZero())
	{
		SetActorRotation(Velocity.Rotation());
	}
	if (bArrived || Next.Z < -50.0)
	{
		Splash(Where);
	}
}

void ADRAcid::Splash(const FVector& Where)
{
	// Touche la cible si l'acide tombe à moins de 2 m d'elle, sinon le joueur s'il est juste à côté.
	AActor* Aimed = Target.Get();
	bool bHit = false;
	if (DRZombieRules::IsStanding(Aimed))
	{
		FVector Point = Aimed->GetActorLocation();
		if (const ADRBarrier* Barrier = Cast<ADRBarrier>(Aimed))
		{
			Point = Barrier->ContactPoint(Where);
		}
		if (FVector::Dist2D(Point, Where) < 200.0)
		{
			UGameplayStatics::ApplyDamage(Aimed, Damage, nullptr, this, UDamageType::StaticClass());
			bHit = true;
		}
	}
	ADRCharacter* Player = Cast<ADRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!bHit && Player && Player->IsAlive() && FVector::Dist2D(Player->GetActorLocation(), Where) < 150.0)
	{
		UGameplayStatics::ApplyDamage(Player, Damage, nullptr, this, UDamageType::StaticClass());
	}
	DRLook::PlayAt(this, TEXT("acid_hit"), Where, -3.0f, 0.15f);
	// Flaque verte au sol, qui fume un peu (lueur) puis s'évapore.
	FVector Ground = Where;
	FHitResult Floor;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DRFlaque), false, this);
	if (GetWorld()->LineTraceSingleByObjectType(Floor, Where + FVector(0.0, 0.0, 30.0), Where - FVector(0.0, 0.0, 500.0), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Ground = Floor.ImpactPoint;
	}
	SetActorLocationAndRotation(Ground + FVector(0.0, 0.0, 3.0), FRotator::ZeroRotator);
	Ball->SetVisibility(false);
	Puddle->SetVisibility(true);
	Shine->SetIntensity(8.0f);
	PuddleLeft = 3.5f;
}
