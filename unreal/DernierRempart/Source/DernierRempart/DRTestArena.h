#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRTestArena.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInterface;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMeshComponent;

/**
 * Terrain d'essai de la première étape du portage : un sol, des murs, des caisses, des piliers et un
 * petit toit, sous un ciel atmosphérique avec soleil, lumière du ciel en temps réel et brouillard
 * volumétrique. Il sert à vérifier Lumen (la lumière qui rebondit) et les ombres virtuelles avant de
 * reconstruire les vraies cartes.
 */
UCLASS()
class DERNIERREMPART_API ADRTestArena : public AActor
{
	GENERATED_BODY()

public:
	ADRTestArena();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TArray<TObjectPtr<UStaticMeshComponent>> Blocks;

	UPROPERTY()
	TArray<FLinearColor> BlockColors;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BlockMaterial;
};
