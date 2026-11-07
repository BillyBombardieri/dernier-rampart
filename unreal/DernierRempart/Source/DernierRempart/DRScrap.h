#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRScrap.generated.h"

/**
 * Ferraille laissée par un zombie (Godot : scripts/scrap.gd) : un petit tas de pièces de métal,
 * avec un reflet doré pour la repérer la nuit. Elle vient à toi quand tu passes à moins de 2,5 m
 * et disparaît au bout de 30 s, ou à la fin de la récolte.
 */
UCLASS()
class DERNIERREMPART_API ADRScrap : public AActor
{
	GENERATED_BODY()

public:
	ADRScrap();

	void Setup(int32 InValue);
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> Pile;

	int32 Value = 1;
	float Age = 0.0f;
};
