#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DRHUD.generated.h"

class ADRCharacter;
class ADRGameMode;
class UFont;

/**
 * Interface en jeu, dessinée directement à l'écran en attendant la vraie interface : vague et
 * phase, PV du Cœur et des barrières, ferraille et points, messages, viseur qui s'écarte avec la
 * dispersion, marque de touche, arme et munitions, vie du joueur, barres de vie au-dessus des
 * zombies blessés et leurs états (marqué, gelé, en feu, chargé, étourdi), noms et portée des
 * tours, choix de la tour à construire, ce qu'on peut faire avec ce qu'on vise, textes des combos,
 * écran rougi quand on est touché, message quand on tombe, fin de partie. Menus à la souris :
 * menu principal, choix du niveau et améliorations permanentes, pause, réglages et touches,
 * boutons de fin de partie.
 */
UCLASS()
class DERNIERREMPART_API ADRHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** Échap : annule l'attente d'une touche, ferme les réglages, le choix du niveau ou les améliorations. Vrai si c'était le cas. */
	bool Back();
	bool IsInSettings() const { return bSettingsOpen; }

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
	/** Souris visible dans les menus ; clic gauche de cette image. */
	void UpdateMouse(const ADRGameMode* Mode);
	/** Bouton cliquable ; vrai s'il vient d'être cliqué. */
	bool Button(const FString& Text, float X, float Y, float Width, float Height, float TextSize, bool bEnabled = true, bool bHighlight = false, bool bDone = false);
	void DrawMainMenu(ADRGameMode* Mode);
	void DrawLevels(ADRGameMode* Mode);
	/** Améliorations permanentes (Godot : upgrade_panel.gd), achetées avec les insignes. */
	void DrawUpgrades(ADRGameMode* Mode);
	void DrawPause(ADRGameMode* Mode);
	/** Réglages (Godot : settings_panel.gd) : souris, volume, champ de vision, qualité, touches. */
	void DrawSettings(ADRGameMode* Mode);
	/** Prend la touche ou le bouton de souris qu'on vient d'appuyer pour l'action choisie. */
	void PollRebind(ADRGameMode* Mode);
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
	float MouseX = -1.0f;
	float MouseY = -1.0f;
	bool bClick = false;
	bool bCursorShown = false;
	bool bMouseSynced = false;
	/** Menu principal : choix du niveau ouvert (sinon l'accueil). */
	bool bLevelsPage = false;
	/** Menu principal : améliorations permanentes ouvertes. */
	bool bUpgradesPage = false;
	/** Réglages ouverts, depuis le menu principal ou la pause. */
	bool bSettingsOpen = false;
	/** Action dont on attend la nouvelle touche (aucune sinon). */
	FName WaitingKey;
	/** Moment (temps réel) du dernier changement de touche. */
	double RebindTime = -10.0;
};
