#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DRHUD.generated.h"

class ADRCharacter;
class UFont;

/**
 * Interface en jeu, dessinée directement à l'écran en attendant la vraie interface : viseur qui
 * s'écarte avec la dispersion, marque de touche, arme et munitions, vie du joueur, PV du Cœur et
 * des barrières, barres de vie au-dessus des zombies blessés, écran rougi quand on est touché,
 * message quand on tombe, rappel des touches.
 */
UCLASS()
class DERNIERREMPART_API ADRHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawBar(float X, float Y, float Width, float Height, float Fill, const FLinearColor& Color);
	/** Align : 0 = X est le bord gauche du texte, 0,5 = son milieu, 1 = son bord droit. */
	void DrawLabel(const FString& Text, const FLinearColor& Color, float X, float Y, float Size, float Align = 0.0f);
	void DrawEdges(float Strength);
	void DrawZombieBars();
	void DrawStructures();
	void DrawWeapon(const ADRCharacter* Player);
	void DrawHealth(const ADRCharacter* Player);
	void DrawFallen(const ADRCharacter* Player);

	UPROPERTY(Transient)
	TObjectPtr<UFont> Font;

	float Scale = 1.0f;
};
