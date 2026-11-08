#include "DRHUD.h"

#include "CollisionQueryParams.h"
#include "DRChapter.h"
#include "DRCharacter.h"
#include "DRData.h"
#include "DRGameMode.h"
#include "DRStructures.h"
#include "DRTower.h"
#include "DRZombie.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

namespace DRHUDStyle
{
	// Couleurs de l'interface de la version Godot (scripts/hud.gd).
	const FLinearColor Accent(1.0f, 0.72f, 0.25f);
	const FLinearColor Danger(1.0f, 0.28f, 0.22f);
	const FLinearColor CoreBlue(0.35f, 0.65f, 1.0f);
	const FLinearColor Panel(0.04f, 0.05f, 0.07f, 0.72f);
	const FLinearColor Track(0.0f, 0.0f, 0.0f, 0.5f);
	const FLinearColor Light(0.85f, 0.85f, 0.85f);

	// Barre de vie d'un zombie : verte, puis jaune, puis rouge (scripts/health_bar.gd).
	FLinearColor ZombieBar(float Fill)
	{
		const FLinearColor Red(0.85f, 0.12f, 0.08f);
		const FLinearColor Yellow(0.95f, 0.75f, 0.15f);
		const FLinearColor Green(0.35f, 0.8f, 0.25f);
		const FLinearColor Mid = Red + (Yellow - Red) * FMath::SmoothStep(0.15f, 0.5f, Fill);
		return Mid + (Green - Mid) * FMath::SmoothStep(0.5f, 0.85f, Fill);
	}

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}

	constexpr float ZombieBarRange = 4000.0f; // au-delà de 40 m, les barres seraient illisibles
}

void ADRHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GetWorld())
	{
		return;
	}
	Scale = Canvas->ClipY / 1080.0f;
	if (!Font && GEngine)
	{
		Font = GEngine->GetLargeFont();
	}
	const ADRCharacter* Player = Cast<ADRCharacter>(GetOwningPawn());
	const ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	const bool bOver = Mode && Mode->IsOver();
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		FRotator View;
		PlayerController->GetPlayerViewPoint(ViewEye, View);
		ViewForward = View.Vector();
	}

	DrawTowers(Player);
	DrawRings();
	DrawZombieBars();
	DrawPopups();
	if (Player)
	{
		DrawEdges(Player->IsAlive() ? Player->GetHurt() : 0.8f);
	}
	PanelBottom = 16.0f * Scale;
	DrawStructures();
	DrawResources();
	if (bOver)
	{
		DrawEnd();
		return;
	}
	if (Player && Player->IsAlive())
	{
		DrawWeapon(Player);
	}
	if (Player)
	{
		DrawHealth(Player);
		DrawFallen(Player);
		if (Player->IsAlive())
		{
			DrawBuildCards(Player);
			DrawHint(Player);
		}
	}
	DrawMessages(PanelBottom + 12.0f * Scale);

	// Entrée pour ne pas attendre la fin du compte à rebours.
	if (Mode && Mode->GetPhase() != EDRPhase::Assault)
	{
		const TCHAR* Hint = Mode->GetPhase() == EDRPhase::Prep ? TEXT("[Entr\u00E9e]  lancer la vague maintenant") : TEXT("[Entr\u00E9e]  passer \u00E0 la vague suivante");
		DrawLabel(Hint, DRHUDStyle::Accent, Canvas->ClipX * 0.5f, Canvas->ClipY - 110.0f * Scale, 0.7f, 0.5f);
	}

	// Rappel des touches pendant les premières secondes, en bas au milieu.
	if (GetWorld()->GetTimeSeconds() < 20.0)
	{
		DrawLabel(TEXT("ZQSD bouger   Maj courir   Espace sauter   Clic gauche tirer   Clic droit viser   R recharger   & / \u00E9 ou molette : arme   \u00C9chap quitter"),
			FLinearColor(1.0f, 1.0f, 1.0f, 0.8f), Canvas->ClipX * 0.5f, Canvas->ClipY - 70.0f * Scale, 0.75f, 0.5f);
		DrawLabel(TEXT("E construire / am\u00E9liorer (maintenir : r\u00E9parer)   C ou molette : choisir la tour   X allumer / \u00E9teindre   F marquer   Entr\u00E9e lancer la vague"),
			FLinearColor(1.0f, 1.0f, 1.0f, 0.65f), Canvas->ClipX * 0.5f, Canvas->ClipY - 46.0f * Scale, 0.65f, 0.5f);
	}
}

// ---------------------------------------------------------------- outils

void ADRHUD::DrawBar(float X, float Y, float Width, float Height, float Fill, const FLinearColor& Color)
{
	DrawRect(DRHUDStyle::Track, X, Y, Width, Height);
	DrawRect(Color, X, Y, Width * FMath::Clamp(Fill, 0.0f, 1.0f), Height);
}

void ADRHUD::DrawLabel(const FString& Text, const FLinearColor& Color, float X, float Y, float Size, float Align)
{
	float Width = 0.0f;
	float Height = 0.0f;
	GetTextSize(Text, Width, Height, Font, Size * Scale);
	DrawText(Text, Color, X - Width * Align, Y, Font, Size * Scale);
}

TArray<FString> ADRHUD::Wrap(const FString& Text, float Size, float MaxWidth) const
{
	TArray<FString> Words;
	Text.ParseIntoArray(Words, TEXT(" "), true);
	TArray<FString> Lines;
	FString Line;
	for (const FString& Word : Words)
	{
		const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		float Width = 0.0f;
		float Height = 0.0f;
		GetTextSize(Candidate, Width, Height, Font, Size * Scale);
		if (Width > MaxWidth && !Line.IsEmpty())
		{
			Lines.Add(Line);
			Line = Word;
		}
		else
		{
			Line = Candidate;
		}
	}
	if (!Line.IsEmpty())
	{
		Lines.Add(Line);
	}
	return Lines;
}

void ADRHUD::DrawEdges(float Strength)
{
	// Bords de l'écran rougis quand on est touché (Godot : vignette de hud.gd), en bandes de plus
	// en plus transparentes vers le centre.
	if (Strength <= 0.01f)
	{
		return;
	}
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const int32 Bands = 12;
	const float Step = H * 0.025f;
	for (int32 Band = 0; Band < Bands; ++Band)
	{
		const float Inset = Band * Step;
		const float Fade = FMath::Square(1.0f - (float)Band / Bands);
		const FLinearColor Color(0.5f, 0.0f, 0.0f, FMath::Clamp(Strength * 0.55f * Fade, 0.0f, 1.0f));
		DrawRect(Color, Inset, Inset, W - 2.0f * Inset, Step);
		DrawRect(Color, Inset, H - Inset - Step, W - 2.0f * Inset, Step);
		DrawRect(Color, Inset, Inset + Step, Step, H - 2.0f * (Inset + Step));
		DrawRect(Color, W - Inset - Step, Inset + Step, Step, H - 2.0f * (Inset + Step));
	}
}

// ---------------------------------------------------------------- zombies

void ADRHUD::DrawZombieBars()
{
	// Fines barres au-dessus des zombies blessés, de la largeur du zombie (Godot : 62 cm sur 6,5),
	// qui s'estompent quand on ne les touche plus et restent cachées derrière les murs.
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController)
	{
		return;
	}
	FVector Eye;
	FRotator View;
	PlayerController->GetPlayerViewPoint(Eye, View);
	const FVector Forward = View.Vector();
	const FVector Right = FRotationMatrix(View).GetScaledAxis(EAxis::Y);
	for (TActorIterator<ADRZombie> It(GetWorld()); It; ++It)
	{
		ADRZombie* Zombie = *It;
		const float Alpha = Zombie->GetBarAlpha();
		const bool bStatus = Zombie->IsFrozen() || Zombie->IsBurning() || Zombie->IsCharged() || Zombie->IsStunned() || Zombie->IsBuffed();
		if (Zombie->IsDead() || Zombie->IsBurrowed() || (Alpha <= 0.0f && !bStatus && !Zombie->IsMarked()))
		{
			continue;
		}
		const FVector Anchor = Zombie->GetBarAnchor();
		const FVector ToBar = Anchor - Eye;
		if (FVector::DotProduct(ToBar, Forward) < 30.0 || ToBar.SizeSquared() > FMath::Square(DRHUDStyle::ZombieBarRange))
		{
			continue;
		}
		// Marqué : visible même derrière un mur, comme dans Godot.
		if (Zombie->IsMarked())
		{
			const FVector Mark = Project(Anchor + FVector(0.0, 0.0, 30.0 * Zombie->GetSize()));
			const FLinearColor MarkRed(1.0f, 0.25f, 0.25f);
			const float Tip = 6.0f * Scale;
			DrawLine(Mark.X - Tip, Mark.Y - Tip, Mark.X, Mark.Y, MarkRed, 2.0f * Scale);
			DrawLine(Mark.X + Tip, Mark.Y - Tip, Mark.X, Mark.Y, MarkRed, 2.0f * Scale);
			DrawLabel(TEXT("MARQU\u00C9"), MarkRed, Mark.X, Mark.Y - Tip - 22.0f * Scale, 0.5f, 0.5f);
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DRBarreDeVie), false, GetOwningPawn());
		Params.AddIgnoredActor(Zombie);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Anchor, ECC_Visibility, Params))
		{
			continue;
		}
		const float WorldWidth = Zombie->GetBarWidth();
		const FVector Center = Project(Anchor);
		const FVector Side = Project(Anchor + Right * (WorldWidth * 0.5f));
		const float Width = 2.0f * FVector2D::Distance(FVector2D(Center.X, Center.Y), FVector2D(Side.X, Side.Y));
		if (Width < 6.0f)
		{
			continue;
		}
		const float Height = FMath::Max(3.0f, Width * 6.5f / WorldWidth);
		const float X = Center.X - Width * 0.5f;
		const float Y = Center.Y - Height * 0.5f;
		// États sous la barre : gelé (bleu), en feu (orange), chargé (violet), étourdi (jaune),
		// enragé par un Hurleur (rouge).
		if (bStatus)
		{
			const float Dot = FMath::Max(4.0f, Height * 1.4f);
			float DotX = Center.X - Dot * 3.1f;
			const FLinearColor Colors[] = { FLinearColor(0.6f, 0.85f, 1.0f), FLinearColor(1.0f, 0.5f, 0.15f), FLinearColor(0.7f, 0.6f, 1.0f), FLinearColor(1.0f, 0.9f, 0.3f), FLinearColor(1.0f, 0.2f, 0.15f) };
			const bool States[] = { Zombie->IsFrozen(), Zombie->IsBurning(), Zombie->IsCharged(), Zombie->IsStunned(), Zombie->IsBuffed() };
			for (int32 Index = 0; Index < 5; ++Index)
			{
				if (States[Index])
				{
					DrawRect(Colors[Index], DotX, Y + Height + 3.0f, Dot, Dot);
				}
				DotX += Dot * 1.3f;
			}
		}
		if (Alpha <= 0.0f)
		{
			continue;
		}
		const float Fill = FMath::Clamp(Zombie->GetHp() / FMath::Max(1.0f, Zombie->GetMaxHp()), 0.0f, 1.0f);
		DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.75f * Alpha), X - 1.0f, Y - 1.0f, Width + 2.0f, Height + 2.0f);
		DrawRect(FLinearColor(0.08f, 0.06f, 0.06f, 0.5f * Alpha), X, Y, Width, Height);
		DrawRect(DRHUDStyle::WithAlpha(DRHUDStyle::ZombieBar(Fill), 0.95f * Alpha), X, Y, Width * Fill, Height);
	}
}

// ---------------------------------------------------------------- Cœur et barrières

void ADRHUD::DrawStructures()
{
	// En haut au milieu : PV du Cœur, puis une ligne par barrière (on voit laquelle est attaquée).
	const ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	const ADRChapter* Chapter = Mode ? Mode->GetChapter() : nullptr;
	if (!Chapter)
	{
		return;
	}
	const ADRCore* Core = Chapter->GetCore();
	const TArray<TObjectPtr<ADRBarrier>>& Barriers = Chapter->GetBarriers();
	const float Width = 480.0f * Scale;
	const float Row = 22.0f * Scale;
	const float WaveRow = 30.0f * Scale;
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = 16.0f * Scale;
	const float Pad = 12.0f * Scale;
	const float Height = Pad * 2.0f + WaveRow + 12.0f * Scale + Row * (1 + Barriers.Num());
	DrawRect(DRHUDStyle::Panel, Left, Top, Width, Height);
	PanelBottom = Top + Height;
	float Y = Top + Pad;

	// Niveau, vague (en rouge les nuits de siège) et phase avec son compte à rebours.
	const bool bSiege = Mode->IsSiege(Mode->GetWave());
	FString WaveText = FString::Printf(TEXT("NIV. %s \u00B7 VAGUE %d / %d"), *Mode->GetLevelShort(), Mode->GetWave(), Mode->GetLastWave());
	if (bSiege)
	{
		WaveText += TEXT("  SI\u00C8GE");
	}
	DrawLabel(WaveText, bSiege ? DRHUDStyle::Danger : FLinearColor::White, Left + Pad, Y, 0.75f);
	FString PhaseText;
	float PhaseFill = 0.0f;
	switch (Mode->GetPhase())
	{
	case EDRPhase::Prep:
		PhaseText = FString::Printf(TEXT("PR\u00C9PARATION  %d s"), FMath::CeilToInt(Mode->GetPhaseTime()));
		PhaseFill = Mode->GetPhaseTime() / Mode->GetPhaseDuration();
		break;
	case EDRPhase::Assault:
		PhaseText = FString::Printf(TEXT("ASSAUT  %d"), Mode->GetRemaining());
		PhaseFill = (float)Mode->GetRemaining() / FMath::Max(1, Mode->GetWaveTotal());
		break;
	case EDRPhase::Harvest:
		PhaseText = FString::Printf(TEXT("R\u00C9COLTE  %d s"), FMath::CeilToInt(Mode->GetPhaseTime()));
		PhaseFill = Mode->GetPhaseTime() / Mode->GetPhaseDuration();
		break;
	}
	const bool bAssault = Mode->GetPhase() == EDRPhase::Assault;
	DrawLabel(PhaseText, bAssault ? DRHUDStyle::Danger : DRHUDStyle::Accent, Left + Width - Pad, Y + 3.0f * Scale, 0.55f, 1.0f);
	Y += WaveRow;
	DrawBar(Left + Pad, Y, Width - 2.0f * Pad, 5.0f * Scale, PhaseFill, DRHUDStyle::Accent);
	Y += 12.0f * Scale;

	const float BarLeft = Left + 112.0f * Scale;
	const float BarWidth = Width - 112.0f * Scale - 70.0f * Scale;
	if (Core)
	{
		DrawLabel(TEXT("C\u0152UR"), DRHUDStyle::CoreBlue, Left + Pad, Y, 0.6f);
		DrawBar(BarLeft, Y + Row * 0.3f, BarWidth, 8.0f * Scale, Core->GetHp() / Core->GetMaxHp(), Core->IsAlive() ? DRHUDStyle::CoreBlue : DRHUDStyle::Danger);
		DrawLabel(FString::FromInt(FMath::CeilToInt(FMath::Max(0.0f, Core->GetHp()))), FLinearColor(0.85f, 0.9f, 1.0f), Left + Width - Pad, Y, 0.6f, 1.0f);
	}
	Y += Row;
	for (const TObjectPtr<ADRBarrier>& Barrier : Barriers)
	{
		if (!Barrier)
		{
			continue;
		}
		FString Name = Barrier->GetDisplayName();
		Name.ReplaceInline(TEXT("Barri\u00E8re de l'"), TEXT(""));
		Name.ReplaceInline(TEXT("Barri\u00E8re de la "), TEXT(""));
		Name.ToUpperInline();
		const bool bAlive = Barrier->IsAlive();
		// Détruite : la barre montre où en est la remise en place (elle se relève une fois pleine).
		const float Fill = Barrier->GetHp() / FMath::Max(1.0f, bAlive ? Barrier->GetMaxHp() : Barrier->GetRebuildHp());
		const FLinearColor Color = bAlive && Fill > 0.35f ? FLinearColor(0.85f, 0.85f, 0.8f) : DRHUDStyle::Danger;
		DrawLabel(Name, DRHUDStyle::Light, Left + Pad, Y, 0.5f);
		DrawBar(BarLeft, Y + Row * 0.35f, BarWidth, 6.0f * Scale, Fill, Color);
		const FString Value = bAlive ? FString::FromInt(FMath::CeilToInt(Barrier->GetHp()))
			: (Barrier->GetHp() < 1.0f ? FString(TEXT("D\u00C9TRUITE")) : FString::Printf(TEXT("%d / %d"), FMath::FloorToInt(Barrier->GetHp()), FMath::RoundToInt(Barrier->GetRebuildHp())));
		DrawLabel(Value, bAlive ? DRHUDStyle::Light : DRHUDStyle::Danger, Left + Width - Pad, Y, 0.5f, 1.0f);
		Y += Row;
	}
}

// ---------------------------------------------------------------- joueur

void ADRHUD::DrawWeapon(const ADRCharacter* Player)
{
	if (!Player->HasWeapon())
	{
		return;
	}
	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;

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
	const float Right = Canvas->ClipX - 40.0f * Scale;
	DrawLabel(Ammo, AmmoColor, Right, Canvas->ClipY - Height - 40.0f * Scale, 1.6f, 1.0f);
	float NameWidth = 0.0f;
	float NameHeight = 0.0f;
	GetTextSize(Weapon.Name, NameWidth, NameHeight, Font, Scale);
	DrawLabel(Weapon.Name, FLinearColor(0.8f, 0.8f, 0.8f), Right, Canvas->ClipY - Height - NameHeight - 44.0f * Scale, 1.0f, 1.0f);
}

void ADRHUD::DrawHealth(const ADRCharacter* Player)
{
	// En bas à gauche : vie du joueur, verte, puis orange sous la moitié, rouge sous le quart.
	const float Width = 300.0f * Scale;
	const float Height = 58.0f * Scale;
	const float Left = 40.0f * Scale;
	const float Top = Canvas->ClipY - Height - 40.0f * Scale;
	const float Pad = 12.0f * Scale;
	const float Ratio = FMath::Clamp(Player->GetHp() / FMath::Max(1.0f, Player->GetMaxHp()), 0.0f, 1.0f);
	const FLinearColor Color = Ratio > 0.5f ? FLinearColor(0.35f, 0.9f, 0.45f) : (Ratio > 0.25f ? DRHUDStyle::Accent : DRHUDStyle::Danger);
	DrawRect(DRHUDStyle::Panel, Left, Top, Width, Height);
	DrawLabel(TEXT("VIE"), DRHUDStyle::Light, Left + Pad, Top + Pad * 0.6f, 0.55f);
	DrawLabel(FString::FromInt(FMath::CeilToInt(FMath::Max(0.0f, Player->GetHp()))), FLinearColor::White, Left + Width - Pad, Top + Pad * 0.3f, 0.9f, 1.0f);
	DrawBar(Left + Pad, Top + Height - Pad - 10.0f * Scale, Width - 2.0f * Pad, 10.0f * Scale, Ratio, Color);
}

void ADRHUD::DrawFallen(const ADRCharacter* Player)
{
	if (Player->IsAlive())
	{
		return;
	}
	// À terre : l'écran s'assombrit, et un compte à rebours annonce le retour au Cœur.
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;
	DrawLabel(TEXT("Tu es tomb\u00E9 !"), DRHUDStyle::Danger, CenterX, CenterY - 80.0f * Scale, 2.2f, 0.5f);
	const int32 Seconds = FMath::Max(1, FMath::CeilToInt(Player->GetRespawnLeft()));
	DrawLabel(FString::Printf(TEXT("Retour au C\u0153ur dans %d s. Les tours d\u00E9fendent seules."), Seconds),
		FLinearColor::White, CenterX, CenterY, 0.9f, 0.5f);
}

// ---------------------------------------------------------------- partie

void ADRHUD::DrawResources()
{
	// En haut à gauche : ferraille (pour les tours, l'établi et les réparations) et points.
	const ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	if (!Mode)
	{
		return;
	}
	const float Left = 24.0f * Scale;
	const float Top = 16.0f * Scale;
	const float Width = 230.0f * Scale;
	const float Row = 28.0f * Scale;
	const float Pad = 12.0f * Scale;
	DrawRect(DRHUDStyle::Panel, Left, Top, Width, Pad * 2.0f + Row * 3.0f);
	DrawLabel(TEXT("FERRAILLE"), DRHUDStyle::Light, Left + Pad, Top + Pad + 4.0f * Scale, 0.5f);
	DrawLabel(FString::FromInt(Mode->GetScrap()), DRHUDStyle::Accent, Left + Width - Pad, Top + Pad, 0.75f, 1.0f);
	DrawLabel(TEXT("POINTS"), DRHUDStyle::Light, Left + Pad, Top + Pad + Row + 4.0f * Scale, 0.5f);
	DrawLabel(FString::FromInt(Mode->GetScore()), FLinearColor::White, Left + Width - Pad, Top + Pad + Row, 0.75f, 1.0f);
	DrawLabel(TEXT("\u00C9NERGIE"), DRHUDStyle::Light, Left + Pad, Top + Pad + Row * 2.0f + 4.0f * Scale, 0.5f);
	DrawLabel(FString::Printf(TEXT("%d / %d"), Mode->GetEnergyUsed(), Mode->GetEnergyCap()), FLinearColor(0.45f, 0.8f, 1.0f), Left + Width - Pad, Top + Pad + Row * 2.0f, 0.75f, 1.0f);
}

void ADRHUD::DrawMessages(float Top)
{
	// Messages de la partie, sous le panneau du haut : ils apparaissent, restent 3,5 s et s'effacent.
	const ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	if (!Mode)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const float Width = 560.0f * Scale;
	const float Pad = 10.0f * Scale;
	const float Size = 0.62f;
	float Y = Top;
	for (const FDRMessage& Message : Mode->GetMessages())
	{
		const float Age = Now - Message.Time;
		if (Age < 0.0f || Age > 4.2f)
		{
			continue;
		}
		const float Alpha = Age < 0.2f ? Age / 0.2f : (Age > 3.7f ? (4.2f - Age) / 0.5f : 1.0f);
		const TArray<FString> Lines = Wrap(Message.Text, Size, Width - 2.0f * Pad);
		float LineWidth = 0.0f;
		float LineHeight = 0.0f;
		GetTextSize(TEXT("Ag"), LineWidth, LineHeight, Font, Size * Scale);
		const float Height = Pad * 2.0f + LineHeight * Lines.Num();
		const float Left = (Canvas->ClipX - Width) * 0.5f;
		DrawRect(DRHUDStyle::WithAlpha(DRHUDStyle::Panel, Alpha), Left, Y, Width, Height);
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			DrawLabel(Lines[Index], FLinearColor(1.0f, 0.92f, 0.75f, Alpha), Canvas->ClipX * 0.5f, Y + Pad + LineHeight * Index, Size, 0.5f);
		}
		Y += Height + 6.0f * Scale;
	}
}

void ADRHUD::DrawEnd()
{
	// Fin de partie : victoire après la dernière vague, défaite si le Cœur tombe.
	const ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	if (!Mode)
	{
		return;
	}
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	const float Width = 760.0f * Scale;
	const float Height = 300.0f * Scale;
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = (Canvas->ClipY - Height) * 0.5f;
	const float CenterX = Canvas->ClipX * 0.5f;
	DrawRect(DRHUDStyle::Panel, Left, Top, Width, Height);
	const bool bWon = Mode->IsVictory();
	DrawLabel(bWon ? FString(TEXT("VICTOIRE !")) : FString(TEXT("LE C\u0152UR EST TOMB\u00C9")), bWon ? DRHUDStyle::Accent : DRHUDStyle::Danger, CenterX, Top + 30.0f * Scale, 1.8f, 0.5f);
	const FString Summary = bWon
		? FString::Printf(TEXT("%s : tu as tenu les %d vagues."), *Mode->GetLevelTitle(), Mode->GetLastWave())
		: FString::Printf(TEXT("%s : tu as tenu jusqu'\u00E0 la vague %d."), *Mode->GetLevelTitle(), Mode->GetWave());
	DrawLabel(Summary, FLinearColor::White, CenterX, Top + 115.0f * Scale, 0.8f, 0.5f);
	DrawLabel(FString::Printf(TEXT("Score : %d points  \u00B7  %d zombies abattus"), Mode->GetScore(), Mode->GetKills()),
		FLinearColor(0.9f, 0.9f, 0.9f), CenterX, Top + 160.0f * Scale, 0.7f, 0.5f);
	const TCHAR* Next = bWon && Mode->HasNextLevel() ? TEXT("[Entr\u00E9e]  niveau suivant") : TEXT("[Entr\u00E9e]  recommencer");
	DrawLabel(Next, DRHUDStyle::Accent, CenterX, Top + Height - 60.0f * Scale, 0.8f, 0.5f);
}

// ---------------------------------------------------------------- tours

bool ADRHUD::IsInFront(const FVector& Point) const
{
	return FVector::DotProduct(Point - ViewEye, ViewForward) > 20.0;
}

void ADRHUD::DrawRing(const FVector& Center, float Radius, const FLinearColor& Color, float Thickness)
{
	const int32 Segments = 72;
	FVector Previous = FVector::ZeroVector;
	bool bPrevious = false;
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float Angle = 2.0f * PI * Index / Segments;
		const FVector Point = Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0);
		const bool bFront = IsInFront(Point);
		const FVector Screen = bFront ? Project(Point) : FVector::ZeroVector;
		if (bFront && bPrevious)
		{
			DrawLine(Previous.X, Previous.Y, Screen.X, Screen.Y, Color, Thickness);
		}
		Previous = Screen;
		bPrevious = bFront;
	}
}

void ADRHUD::DrawTowers(const ADRCharacter* Player)
{
	// Nom de chaque tour au-dessus d'elle, halo discret des Phares, portée de la tour ou de
	// l'ancrage visés (Godot : Label3D et range_ring de tower.gd).
	const ADRSocket* Sight = Player && Player->IsAlive() ? Player->GetSightSocket() : nullptr;
	for (TActorIterator<ADRTower> It(GetWorld()); It; ++It)
	{
		const ADRTower* Tower = *It;
		const FVector Ground = Tower->GetActorLocation() - FVector(0.0, 0.0, Tower->GetSimpleCollisionHalfHeight() - 8.0);
		if (Tower->GetType() == TEXT("beacon") && Tower->IsPowered())
		{
			DrawRing(Ground, Tower->GetRange(), DRHUDStyle::WithAlpha(Tower->GetColor(), 0.25f), 1.5f * Scale);
		}
		const FVector Anchor = Tower->GetLabelAnchor();
		if (!IsInFront(Anchor) || FVector::DistSquared(Anchor, ViewEye) > FMath::Square(3000.0))
		{
			continue;
		}
		FString Label = FString::Printf(TEXT("%s niv.%d"), *Tower->GetDisplayName(), Tower->GetLevel());
		if (!Tower->IsPowered())
		{
			Label += TEXT("  [\u00C9TEINTE]");
		}
		if (Tower->IsBoosted())
		{
			Label += TEXT("  (Phare)");
		}
		const FVector Screen = Project(Anchor);
		DrawLabel(Label, Tower->IsPowered() ? FLinearColor(0.95f, 0.95f, 0.95f, 0.9f) : DRHUDStyle::Danger, Screen.X, Screen.Y, 0.5f, 0.5f);
	}
	if (!Sight)
	{
		return;
	}
	if (const ADRTower* Tower = Sight->GetTower())
	{
		const FVector Ground = Tower->GetActorLocation() - FVector(0.0, 0.0, Tower->GetSimpleCollisionHalfHeight() - 8.0);
		DrawRing(Ground, Tower->GetRange(), DRHUDStyle::WithAlpha(Tower->GetColor(), 0.7f), 2.5f * Scale);
		return;
	}
	const ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	if (Mode && Mode->GetBuildOrder().IsValidIndex(Player->GetBuildChoice()))
	{
		const TSharedPtr<FJsonObject> Stats = Mode->GetTowerData(Mode->GetBuildOrder()[Player->GetBuildChoice()]);
		const FLinearColor Color = DRData::ColorSRGB(Stats, TEXT("color"), FLinearColor::White);
		DrawRing(Sight->GetActorLocation() + FVector(0.0, 0.0, 8.0), (float)DRData::Number(Stats, TEXT("range"), 10.0) * 100.0f, DRHUDStyle::WithAlpha(Color, 0.7f), 2.5f * Scale);
	}
}

void ADRHUD::DrawBuildCards(const ADRCharacter* Player)
{
	// Devant un ancrage vide : une carte par tour, la tour choisie en surbrillance.
	const ADRSocket* Sight = Player->GetSightSocket();
	const ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	if (!Sight || Sight->GetTower() || !Mode)
	{
		return;
	}
	const TArray<FString>& Order = Mode->GetBuildOrder();
	const float CardWidth = 150.0f * Scale;
	const float CardHeight = 62.0f * Scale;
	const float Gap = 6.0f * Scale;
	const float Total = Order.Num() * CardWidth + (Order.Num() - 1) * Gap;
	float X = (Canvas->ClipX - Total) * 0.5f;
	const float Y = Canvas->ClipY - 338.0f * Scale;
	for (int32 Index = 0; Index < Order.Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Stats = Mode->GetTowerData(Order[Index]);
		FString Name = Order[Index];
		if (Stats.IsValid())
		{
			Stats->TryGetStringField(TEXT("name"), Name);
		}
		const int32 Cost = (int32)DRData::Number(Stats, TEXT("cost"), 0.0);
		const int32 Energy = (int32)DRData::Number(Stats, TEXT("energy"), 0.0);
		const bool bChosen = Index == Player->GetBuildChoice();
		if (bChosen)
		{
			DrawRect(DRHUDStyle::Accent, X - 2.0f * Scale, Y - 2.0f * Scale, CardWidth + 4.0f * Scale, CardHeight + 4.0f * Scale);
		}
		DrawRect(FLinearColor(0.04f, 0.05f, 0.07f, 0.9f), X, Y, CardWidth, CardHeight);
		const FLinearColor Color = DRData::ColorSRGB(Stats, TEXT("color"), FLinearColor::White);
		DrawRect(Color, X, Y, 4.0f * Scale, CardHeight);
		DrawLabel(Name, bChosen ? FLinearColor::White : DRHUDStyle::Light, X + 12.0f * Scale, Y + 8.0f * Scale, 0.5f);
		const FLinearColor CostColor = Mode->GetScrap() >= Cost ? DRHUDStyle::Accent : DRHUDStyle::Danger;
		DrawLabel(FString::Printf(TEXT("%d ferraille"), Cost), CostColor, X + 12.0f * Scale, Y + 30.0f * Scale, 0.42f);
		DrawLabel(FString::Printf(TEXT("%d \u00E9n."), Energy), FLinearColor(0.45f, 0.8f, 1.0f), X + CardWidth - 10.0f * Scale, Y + 30.0f * Scale, 0.42f, 1.0f);
		X += CardWidth + Gap;
	}
}

void ADRHUD::DrawHint(const ADRCharacter* Player)
{
	// Ce qu'on peut faire avec ce qu'on vise : première ligne en clair, la suite en gris.
	const FString& Hint = Player->GetHint();
	if (Hint.IsEmpty())
	{
		return;
	}
	TArray<FString> Lines;
	Hint.ParseIntoArray(Lines, TEXT("\n"), true);
	float Y = Canvas->ClipY - 250.0f * Scale;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		DrawLabel(Lines[Index], Index == 0 ? FLinearColor::White : FLinearColor(0.6f, 0.6f, 0.6f), Canvas->ClipX * 0.5f, Y, Index == 0 ? 0.65f : 0.5f, 0.5f);
		Y += (Index == 0 ? 30.0f : 24.0f) * Scale;
	}
}

void ADRHUD::DrawPopups()
{
	// « TÊTE », « BRISÉ ! », « SURCHARGE ! », « EMBRASEMENT ! » : montent d'1,5 m et s'effacent en 0,9 s.
	const ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	if (!Mode)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	for (const FDRPopup& Entry : Mode->GetPopups())
	{
		const float Age = Now - Entry.Time;
		if (Age < 0.0f || Age > 0.9f)
		{
			continue;
		}
		const FVector Where = Entry.Location + FVector(0.0, 0.0, 150.0 * Age / 0.9f);
		if (!IsInFront(Where))
		{
			continue;
		}
		const FVector Screen = Project(Where);
		DrawLabel(Entry.Text, DRHUDStyle::WithAlpha(Entry.Color, 1.0f - Age / 0.9f), Screen.X, Screen.Y, Entry.Size / 48.0f * 0.9f, 0.5f);
	}
}

void ADRHUD::DrawRings()
{
	// Ondes au sol (cri du Hurleur) : elles s'élargissent vite puis s'effacent.
	const ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	if (!Mode)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	for (const FDRRing& Ring : Mode->GetRings())
	{
		const float Age = (Now - Ring.Time) / Ring.Duration;
		if (Age < 0.0f || Age > 1.0f)
		{
			continue;
		}
		const float Grow = 1.0f - FMath::Square(1.0f - Age);
		DrawRing(Ring.Location, FMath::Max(20.0f, Ring.Radius * Grow), DRHUDStyle::WithAlpha(Ring.Color, 1.0f - Age), 3.0f * Scale);
	}
}
