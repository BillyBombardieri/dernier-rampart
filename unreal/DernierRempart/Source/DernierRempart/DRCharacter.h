#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DRCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Le joueur, vu à la première personne. Mêmes valeurs que scripts/player.gd dans la version Godot :
 * marche 5,5 m/s, course 8,5 m/s (seulement en avançant), saut à 5,5 m/s, gravité 20 m/s²,
 * capsule de 1,8 m, yeux à 1,6 m. Touches par défaut en AZERTY : ZQSD, Espace, Maj.
 */
UCLASS()
class DERNIERREMPART_API ADRCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADRCharacter();

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
	void CreateInput();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void SetSpeed(float Speed);

	UPROPERTY(VisibleAnywhere, Category = "Joueur")
	TObjectPtr<UCameraComponent> Camera;

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

	/** Multiplicateur de sensibilité de la souris (1 = comme le réglage par défaut de Godot). */
	UPROPERTY(EditAnywhere, Category = "Joueur")
	float Sensitivity = 1.0f;

	bool bSprintHeld = false;
};
