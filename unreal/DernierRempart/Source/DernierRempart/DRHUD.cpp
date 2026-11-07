#include "DRHUD.h"

#include "DRCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"

void ADRHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	const ADRCharacter* Player = Cast<ADRCharacter>(GetOwningPawn());
	const float Scale = Canvas->ClipY / 1080.0f;
	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;

	if (Player && Player->HasWeapon())
	{
		// Viseur : quatre traits qui s'écartent quand on bouge ou qu'on tire en rafale ; un point en visée.
		const FLinearColor White(1.0f, 1.0f, 1.0f, 0.85f);
		if (Player->IsAiming())
		{
			DrawRect(White, CenterX - 1.5f * Scale, CenterY - 1.5f * Scale, 3.0f * Scale, 3.0f * Scale);
		}
		else
		{
			const float Gap = (6.0f + Player->GetSpread() * 600.0f) * Scale;
			const float Length = 9.0f * Scale;
			const float Thick = FMath::Max(1.0f, 2.0f * Scale);
			DrawLine(CenterX - Gap - Length, CenterY, CenterX - Gap, CenterY, White, Thick);
			DrawLine(CenterX + Gap, CenterY, CenterX + Gap + Length, CenterY, White, Thick);
			DrawLine(CenterX, CenterY - Gap - Length, CenterX, CenterY - Gap, White, Thick);
			DrawLine(CenterX, CenterY + Gap, CenterX, CenterY + Gap + Length, White, Thick);
		}
		// Marque de touche : une croix rouge qui s'efface vite.
		const float HitAge = Player->GetHitMarkerAge();
		if (HitAge < 0.2f)
		{
			const FLinearColor Red(1.0f, 0.25f, 0.2f, 1.0f - HitAge / 0.2f);
			const float A = 7.0f * Scale;
			const float B = 15.0f * Scale;
			const float Thick = FMath::Max(1.0f, 2.0f * Scale);
			DrawLine(CenterX - B, CenterY - B, CenterX - A, CenterY - A, Red, Thick);
			DrawLine(CenterX + A, CenterY - A, CenterX + B, CenterY - B, Red, Thick);
			DrawLine(CenterX - B, CenterY + B, CenterX - A, CenterY + A, Red, Thick);
			DrawLine(CenterX + A, CenterY + A, CenterX + B, CenterY + B, Red, Thick);
		}

		// Arme et munitions, en bas à droite.
		const FDRWeapon& Weapon = Player->GetWeapon();
		const FString Ammo = Player->IsReloading()
			? FString(TEXT("Rechargement..."))
			: FString::Printf(TEXT("%d / %d"), Player->GetAmmo(), Weapon.Mag);
		float Width = 0.0f;
		float Height = 0.0f;
		GetTextSize(Ammo, Width, Height, Font, 1.6f * Scale);
		const FLinearColor AmmoColor = Player->GetAmmo() <= Weapon.Mag / 4 ? FLinearColor(1.0f, 0.45f, 0.3f) : FLinearColor::White;
		DrawText(Ammo, AmmoColor, Canvas->ClipX - Width - 40.0f * Scale, Canvas->ClipY - Height - 40.0f * Scale, Font, 1.6f * Scale);
		float NameWidth = 0.0f;
		float NameHeight = 0.0f;
		GetTextSize(Weapon.Name, NameWidth, NameHeight, Font, Scale);
		DrawText(Weapon.Name, FLinearColor(0.8f, 0.8f, 0.8f), Canvas->ClipX - NameWidth - 40.0f * Scale, Canvas->ClipY - Height - NameHeight - 44.0f * Scale, Font, Scale);
	}

	// Rappel des touches pendant les premières secondes.
	const UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() < 20.0)
	{
		DrawText(TEXT("ZQSD bouger   Maj courir   Espace sauter   Clic gauche tirer   Clic droit viser   R recharger   & / é ou molette : arme   Échap quitter"),
			FLinearColor(1.0f, 1.0f, 1.0f, 0.8f), 40.0f * Scale, Canvas->ClipY - 60.0f * Scale, Font, 0.8f * Scale);
	}
}
