#include "DRCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

// Nommé (et pas anonyme) pour éviter les collisions quand Unreal regroupe les .cpp à la compilation.
namespace DRPlayer
{
	// Valeurs de scripts/player.gd, converties en centimètres (l'unité d'Unreal).
	constexpr float WalkSpeed = 550.0f;
	constexpr float SprintSpeed = 850.0f;
	constexpr float JumpVelocity = 550.0f;
	constexpr float Gravity = 2000.0f;
	// Godot : la vitesse visée est atteinte en 1/12 de seconde au sol, 4 fois plus lentement en l'air.
	constexpr float Acceleration = 12.0f;
	constexpr float AirAcceleration = 3.0f;
	// Godot tourne de 0,0025 radian par pixel de souris. Unreal multiplie déjà le mouvement de la
	// souris par 0,07 (réglage d'axe par défaut), d'où ce facteur pour garder la même vitesse,
	// baissé de 10 % à la demande de Billy après son premier essai (28 octobre).
	constexpr float DegreesPerMouseUnit = 0.9f * 0.0025f * 57.29578f / 0.07f;
}

ADRCharacter::ADRCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	GetCapsuleComponent()->InitCapsuleSize(40.0f, 90.0f);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	// Yeux à 1,6 m du sol : le centre de la capsule est à 0,9 m.
	Camera->SetRelativeLocation(FVector(0.0, 0.0, 70.0));
	Camera->SetFieldOfView(78.0f);
	Camera->bUsePawnControlRotation = true;

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

void ADRCharacter::SetSpeed(float Speed)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = Speed;
	// On accélère et on freine progressivement : on ne s'arrête pas net.
	Movement->MaxAcceleration = DRPlayer::Acceleration * Speed;
	Movement->BrakingDecelerationWalking = DRPlayer::Acceleration * Speed;
}

void ADRCharacter::CreateInput()
{
	if (Mapping)
	{
		return;
	}
	MoveAction = NewObject<UInputAction>(this, TEXT("Avancer"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	LookAction = NewObject<UInputAction>(this, TEXT("Regarder"));
	LookAction->ValueType = EInputActionValueType::Axis2D;
	JumpAction = NewObject<UInputAction>(this, TEXT("Sauter"));
	SprintAction = NewObject<UInputAction>(this, TEXT("Courir"));

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
}

void ADRCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	// Comme dans Godot, on ne court qu'en avançant.
	SetSpeed(bSprintHeld && Axis.Y > 0.0 ? DRPlayer::SprintSpeed : DRPlayer::WalkSpeed);
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void ADRCharacter::Look(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}
	const FVector2D Delta = Value.Get<FVector2D>() * (DRPlayer::DegreesPerMouseUnit * Sensitivity);
	FRotator Rotation = Controller->GetControlRotation();
	Rotation.Yaw += Delta.X;
	Rotation.Pitch = FMath::ClampAngle(Rotation.Pitch + Delta.Y, -89.0, 89.0);
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
