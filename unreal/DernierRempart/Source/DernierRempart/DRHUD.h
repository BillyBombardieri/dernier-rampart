#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DRHUD.generated.h"

class ADRCharacter;
class UFont;

/**
 * Interface en jeu, dessinée directement à l'écran en attendant la vraie interface : vague et
 * phase, PV du Cœur et des barrières, ferraille et points, messages, viseur qui s'écarte avec la
 * dispersion, marque de touche, arme et munitions, vie du joueur, barres de vie au-dessus des
 * zombies blessés et leurs états (marqué, gelé, en feu, chargé, étourdi), noms et portée des
 * tours, choix de la tour à construire, ce qu'on peut faire avec ce qu'on vise, textes des combos,
 * écran rougi quand on est touché, message quand on tombe, fin de partie.
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
	void DrawResources();
	void DrawMessages(float Top);
	void DrawEnd();
	void DrawTowers(const ADRCharacter* Player);
	void DrawBuildCards(const ADRCharacter* Player);
	void DrawHint(const ADRCharacter* Player);
	void DrawPopups();
	void DrawRings();
	/** Anneau au sol (portée d'une tour), dessiné trait par trait. */
	void DrawRing(const FVector& Center, float Radius, const FLinearColor& Color, float Thickness);
	bool IsInFront(const FVector& Point) const;
	/** Coupe un texte en lignes d'au plus MaxWidth pixels. */
	TArray<FString> Wrap(const FString& Text, float Size, float MaxWidth) const;

	UPROPERTY(Transient)
	TObjectPtr<UFont> Font;

	float Scale = 1.0f;
	float PanelBottom = 0.0f; // bas du panneau du haut, où commencent les messages
	FVector ViewEye = FVector::ZeroVector;
	FVector ViewForward = FVector::ForwardVector;
};
