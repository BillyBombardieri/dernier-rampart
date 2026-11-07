#include "DRZombie.h"

#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DRChapter.h"
#include "DRCharacter.h"
#include "DRData.h"
#include "DRGameMode.h"
#include "DRLook.h"
#include "DRStructures.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

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

bool ADRZombie::IsReachable(const FVector& Point) const
{
	return !Blocker || !Blocker->IsAlive() || !Blocker->IsBehind(Point);
}

void ADRZombie::Think()
{
	Blocker = FindBlocker();
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
	if (!Chapter || Chapter->GetPath().Num() < 2)
	{
		return;
	}
	AttackCooldown -= DeltaSeconds;
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
	ThinkCooldown -= DeltaSeconds;
	if (ThinkCooldown <= 0.0f)
	{
		ThinkCooldown = FMath::FRandRange(0.2f, 0.3f);
		Think();
	}

	AActor* Target = nullptr;
	FVector TargetPosition = GetActorLocation();
	double Reach = (1.4 + 0.4 * Size) * 100.0;
	const double BarrierReach = (0.9 + 0.35 * Size) * 100.0;
	const double ChaseRadius = (Type == TEXT("coureur") ? 18.0 : (Type == TEXT("cracheur") ? 4.0 : 7.0)) * 100.0;
	ADRCharacter* Player = Cast<ADRCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const bool bChase = Player && Player->IsAlive() && IsReachable(Player->GetActorLocation()) && FlatDistance(Player->GetActorLocation()) < ChaseRadius;
	const TArray<FVector>& Points = Chapter->GetPath();
	if (bChase)
	{
		Target = Player;
		TargetPosition = Player->GetActorLocation();
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
	if (Target && FlatDistance(TargetPosition) <= Reach)
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
	UGameplayStatics::ApplyDamage(Target, Damage, nullptr, this, UDamageType::StaticClass());
}

float ADRZombie::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}
	float Amount = DamageAmount;
	const bool bFromPlayer = EventInstigator && EventInstigator->IsPlayerController();
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& Point = static_cast<const FPointDamageEvent&>(DamageEvent);
		const FVector Where = Point.HitInfo.ImpactPoint;
		LastHitDirection = FVector(-Point.HitInfo.ImpactNormal.X, -Point.HitInfo.ImpactNormal.Y, 0.0);
		if (bFromPlayer && Where.Z > HeadHeight() - 2.0f * Size)
		{
			Amount *= DRZombieRules::HeadshotMultiplier;
		}
		if (bFromPlayer)
		{
			DRLook::PlayAt(this, TEXT("hit_flesh"), Where, -2.0f, 0.15f);
		}
	}
	Hp -= Amount;
	BarShown = 6.0f;
	if (Hp <= 0.0f)
	{
		Die();
	}
	return Amount;
}

void ADRZombie::Die()
{
	bDead = true;
	Hp = 0.0f;
	BarShown = 0.0f;
	BarAlpha = 0.0f;
	StrikeTarget = nullptr;
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
		PlayLoop(TEXT("idle"), 1.0f);
	}
}
