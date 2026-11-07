#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DRHUD.generated.h"

/**
 * Interface en jeu, dessinée directement à l'écran en attendant la vraie interface (étape 8) :
 * viseur qui s'écarte avec la dispersion, marque de touche, arme et munitions, rappel des touches.
 */
UCLASS()
class DERNIERREMPART_API ADRHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
