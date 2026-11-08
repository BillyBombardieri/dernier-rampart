#include "DRHUD.h"

#include "CollisionQueryParams.h"
#include "DRChapter.h"
#include "DRCharacter.h"
#include "DRData.h"
#include "DRGameMode.h"
#include "DRLook.h"
#include "DRSave.h"
#include "DRStructures.h"
#include "DRTower.h"
#include "DRZombie.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
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

	// Couleur des munitions de l'établi (Godot : workbench_panel.gd, _ammo_color).
	FLinearColor AmmoColor(FName Ammo)
	{
		if (Ammo == FName(TEXT("incendiaire")))
		{
			return FLinearColor(1.0f, 0.5f, 0.2f);
		}
		if (Ammo == FName(TEXT("perforante")))
		{
			return FLinearColor(0.85f, 0.85f, 0.9f);
		}
		if (Ammo == FName(TEXT("electrique")))
		{
			return FLinearColor(0.6f, 0.55f, 1.0f);
		}
		return Accent;
	}

	// Nom de la touche choisie pour une action, pour les textes d'aide.
	FString KeyOf(const ADRGameMode* Mode, const TCHAR* Id)
	{
		const UDRSave* Progress = Mode ? Mode->GetProgress() : nullptr;
		return Progress ? Progress->KeyName(Id) : UDRSave::KeyLabel(UDRSave::DefaultKey(Id));
	}
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
	ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	const bool bOver = Mode && Mode->IsOver();
	UpdateMouse(Mode);
	if (!Mode || !Mode->IsMenuOpen())
	{
		bSettingsOpen = false;
		WaitingKey = NAME_None;
	}
	// Point de vue d'abord : les sons des boutons du menu se jouent à la caméra.
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		FRotator View;
		PlayerController->GetPlayerViewPoint(ViewEye, View);
		ViewForward = View.Vector();
	}
	if (Mode && Mode->IsMainMenu())
	{
		DrawMainMenu(Mode);
		return;
	}

	DrawTowers(Player);
	DrawBarricades();
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
		// (Unreal veut un texte fixe comme modèle de Printf : un modèle par cas.)
		const FString SkipKey = DRHUDStyle::KeyOf(Mode, TEXT("vague"));
		const FString Hint = Mode->GetPhase() == EDRPhase::Prep
			? FString::Printf(TEXT("[%s]  lancer la vague maintenant"), *SkipKey)
			: FString::Printf(TEXT("[%s]  passer \u00E0 la vague suivante"), *SkipKey);
		DrawLabel(Hint, DRHUDStyle::Accent, Canvas->ClipX * 0.5f, Canvas->ClipY - 110.0f * Scale, 0.7f, 0.5f);
	}

	if (Mode && Mode->IsChoosingImplant())
	{
		DrawImplants(Mode);
		return;
	}
	if (Mode && Mode->IsWorkbenchOpen())
	{
		DrawWorkbench(Mode);
		return;
	}
	if (Mode && Mode->IsPauseMenu())
	{
		DrawPause(Mode);
		return;
	}
	// Rappel des touches pendant les premières secondes, en bas au milieu.
	// Les touches affichées sont celles choisies dans les réglages.
	const UDRSave* Progress = Mode ? Mode->GetProgress() : nullptr;
	if (Progress && GetWorld()->GetTimeSeconds() < 20.0)
	{
		auto Key = [Progress](const TCHAR* Id) { return Progress->KeyName(Id); };
		DrawLabel(FString::Printf(TEXT("%s bouger   %s courir   %s sauter   %s tirer   %s viser   %s recharger   %s / %s ou molette : arme   \u00C9chap ou %s pause"),
			*Progress->MoveKeysName(), *Key(TEXT("courir")), *Key(TEXT("sauter")), *Key(TEXT("tirer")), *Key(TEXT("viser")), *Key(TEXT("recharger")),
			*Key(TEXT("pistolet")), *Key(TEXT("fusil")), *Key(TEXT("pause"))),
			FLinearColor(1.0f, 1.0f, 1.0f, 0.8f), Canvas->ClipX * 0.5f, Canvas->ClipY - 70.0f * Scale, 0.75f, 0.5f);
		DrawLabel(FString::Printf(TEXT("%s construire / am\u00E9liorer (maintenir : r\u00E9parer)   %s ou molette : choisir la tour   %s allumer / \u00E9teindre   %s marquer   %s / %s gadgets   %s lancer la vague"),
			*Key(TEXT("interagir")), *Key(TEXT("tour")), *Key(TEXT("allumer")), *Key(TEXT("marquer")), *Key(TEXT("gadget1")), *Key(TEXT("gadget2")), *Key(TEXT("vague"))),
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
	// Les barricades du gadget ont leur propre étiquette (DrawBarricades) : seules les portes sont listées.
	TArray<const ADRBarrier*> Barriers;
	for (const TObjectPtr<ADRBarrier>& Barrier : Chapter->GetBarriers())
	{
		if (Barrier && !Barrier->IsTemporary())
		{
			Barriers.Add(Barrier.Get());
		}
	}
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
	for (const ADRBarrier* Barrier : Barriers)
	{
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
		: FString::Printf(TEXT("%d / %d"), Player->GetAmmo(), Player->GetMagSize());
	float Width = 0.0f;
	float Height = 0.0f;
	GetTextSize(Ammo, Width, Height, Font, 1.6f * Scale);
	const FLinearColor AmmoColor = Player->GetAmmo() <= Player->GetMagSize() / 4 ? FLinearColor(1.0f, 0.45f, 0.3f) : FLinearColor::White;
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
	// Gadgets juste au-dessus, puis les implants installés pendant cette partie.
	const ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	if (!Mode)
	{
		return;
	}
	DrawGadgets(Mode, Left, Top - 28.0f * Scale);
	if (Mode->GetImplants().Num() > 0)
	{
		TArray<FString> Names;
		for (const FName& Id : Mode->GetImplants())
		{
			Names.Add(Mode->GetImplantText(Id, TEXT("name")));
		}
		DrawLabel(TEXT("Implants : ") + FString::Join(Names, TEXT(", ")), FLinearColor(0.8f, 0.8f, 0.8f), Left, Top - 54.0f * Scale, 0.5f);
	}
}

void ADRHUD::DrawGadgets(const ADRGameMode* Mode, float X, float Y)
{
	// « [A] Barricade » en orange quand il est prêt, « [A] Barricade 12 s » en gris pendant la recharge.
	for (int32 Slot = 0; Slot < 2; ++Slot)
	{
		const FName Id = Mode->GetGadgetSlot(Slot);
		if (Id.IsNone())
		{
			continue;
		}
		const FString Key = DRHUDStyle::KeyOf(Mode, Slot == 0 ? TEXT("gadget1") : TEXT("gadget2"));
		const FString Name = Mode->GetGadgetText(Id, TEXT("name"));
		const float Left = Mode->GetGadgetCooldown(Id);
		const FString Text = Left > 0.0f
			? FString::Printf(TEXT("[%s] %s %d s"), *Key, *Name, FMath::CeilToInt(Left))
			: FString::Printf(TEXT("[%s] %s"), *Key, *Name);
		DrawLabel(Text, Left > 0.0f ? FLinearColor(0.55f, 0.55f, 0.55f) : DRHUDStyle::Accent, X, Y, 0.55f);
		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		GetTextSize(Text, TextWidth, TextHeight, Font, 0.55f * Scale);
		X += TextWidth + 24.0f * Scale;
	}
}

void ADRHUD::DrawBarricades()
{
	const ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	const ADRChapter* Chapter = Mode ? Mode->GetChapter() : nullptr;
	if (!Chapter)
	{
		return;
	}
	for (const TObjectPtr<ADRBarrier>& Barrier : Chapter->GetBarriers())
	{
		if (!Barrier || !Barrier->IsTemporary() || !Barrier->IsAlive())
		{
			continue;
		}
		const FVector Anchor = Barrier->GetActorLocation() + FVector(0.0, 0.0, Barrier->GetTopHeight() + 90.0);
		if (!IsInFront(Anchor) || FVector::DistSquared(Anchor, ViewEye) > FMath::Square(3000.0))
		{
			continue;
		}
		const FVector Screen = Project(Anchor);
		DrawLabel(FString::Printf(TEXT("Barricade  %d PV  %d s"), FMath::CeilToInt(Barrier->GetHp()), FMath::CeilToInt(Barrier->GetLifeLeft())),
			FLinearColor(1.0f, 0.85f, 0.4f), Screen.X, Screen.Y, 0.5f, 0.5f);
	}
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
	ADRGameMode* Mode = GetWorld()->GetAuthGameMode<ADRGameMode>();
	if (!Mode)
	{
		return;
	}
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	const float Width = 760.0f * Scale;
	const float Height = 420.0f * Scale;
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
	// Insignes gagnés, à dépenser dans les améliorations permanentes du menu principal.
	const UDRSave* Progress = Mode->GetProgress();
	DrawLabel(FString::Printf(TEXT("+%d insignes  (%d \u00E0 d\u00E9penser dans Am\u00E9liorations)"), Mode->GetEarnedInsignes(), Progress ? Progress->Insignes : 0),
		FLinearColor(0.85f, 0.85f, 0.85f), CenterX, Top + 198.0f * Scale, 0.62f, 0.5f);
	// Record et niveau débloqué (sauvegardés sur le PC).
	FString News;
	if (Mode->IsNewRecord())
	{
		News = TEXT("Nouveau record !");
	}
	if (Mode->HasUnlockedNext())
	{
		if (!News.IsEmpty())
		{
			News += TEXT("  \u00B7  ");
		}
		News += (Mode->GetLevel() + 1) % 5 == 0 ? TEXT("Chapitre suivant d\u00E9bloqu\u00E9 !") : TEXT("Niveau suivant d\u00E9bloqu\u00E9 !");
	}
	if (!News.IsEmpty())
	{
		DrawLabel(News, DRHUDStyle::Accent, CenterX, Top + 238.0f * Scale, 0.7f, 0.5f);
	}
	// Boutons : niveau suivant ou recommencer, et menu principal. Entrée marche toujours.
	const float ButtonWidth = 260.0f * Scale;
	const float ButtonHeight = 50.0f * Scale;
	const float ButtonY = Top + Height - 110.0f * Scale;
	const bool bNext = bWon && Mode->HasNextLevel();
	if (Button(bNext ? TEXT("NIVEAU SUIVANT") : TEXT("RECOMMENCER"), CenterX - ButtonWidth - 10.0f * Scale, ButtonY, ButtonWidth, ButtonHeight, 0.75f, true, true))
	{
		Mode->PlayLevel(bNext ? Mode->GetLevel() + 1 : Mode->GetLevel());
	}
	if (Button(TEXT("MENU PRINCIPAL"), CenterX + 10.0f * Scale, ButtonY, ButtonWidth, ButtonHeight, 0.75f))
	{
		Mode->OpenMainMenu();
	}
	const FString SkipKey = DRHUDStyle::KeyOf(Mode, TEXT("vague"));
	const FString Next = bNext ? FString::Printf(TEXT("[%s]  niveau suivant"), *SkipKey) : FString::Printf(TEXT("[%s]  recommencer"), *SkipKey);
	DrawLabel(Next, FLinearColor(1.0f, 1.0f, 1.0f, 0.55f), CenterX, Top + Height - 45.0f * Scale, 0.55f, 0.5f);
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
	// Panneau vert de l'établi (Godot : Label3D « ÉTABLI » au-dessus du panneau à outils).
	const FVector Sign = ADRChapter::WorkbenchPosition() + FVector(0.0, 0.0, 285.0);
	if (IsInFront(Sign) && FVector::DistSquared(Sign, ViewEye) < FMath::Square(3000.0))
	{
		const FVector SignScreen = Project(Sign);
		DrawLabel(TEXT("\u00C9TABLI"), FLinearColor(0.55f, 1.0f, 0.65f), SignScreen.X, SignScreen.Y, 0.7f, 0.5f);
	}
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

// ---------------------------------------------------------------- menus

void ADRHUD::UpdateMouse(const ADRGameMode* Mode)
{
	// Dans les menus et en fin de partie, la souris est libre et sert à cliquer sur les boutons.
	bClick = false;
	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController)
	{
		return;
	}
	const bool bWanted = Mode && (Mode->IsMenuOpen() || Mode->IsOver());
	// Le réglage de la souris reste d'un niveau à l'autre : on l'applique dès la première image.
	if (!bMouseSynced || bWanted != bCursorShown)
	{
		bMouseSynced = true;
		bCursorShown = bWanted;
		PlayerController->bShowMouseCursor = bWanted;
		if (bWanted)
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputMode);
		}
		else
		{
			PlayerController->SetInputMode(FInputModeGameOnly());
		}
	}
	if (bWanted && PlayerController->GetMousePosition(MouseX, MouseY))
	{
		bClick = PlayerController->WasInputKeyJustPressed(EKeys::LeftMouseButton);
	}
	else
	{
		MouseX = -1.0f;
		MouseY = -1.0f;
	}
}

bool ADRHUD::Button(const FString& Text, float X, float Y, float Width, float Height, float TextSize, bool bEnabled, bool bHighlight, bool bDone)
{
	const bool bHover = bEnabled && MouseX >= X && MouseX <= X + Width && MouseY >= Y && MouseY <= Y + Height;
	FLinearColor Fill(1.0f, 1.0f, 1.0f, bEnabled ? 0.07f : 0.025f);
	if (bDone)
	{
		Fill = FLinearColor(0.3f, 0.75f, 0.35f, 0.3f);
	}
	if (bHighlight)
	{
		Fill = FLinearColor(1.0f, 0.72f, 0.25f, 0.28f);
	}
	if (bHover)
	{
		Fill.A = FMath::Min(1.0f, Fill.A + 0.15f);
	}
	DrawRect(Fill, X, Y, Width, Height);
	const FLinearColor Edge = bHighlight ? DRHUDStyle::Accent : FLinearColor(1.0f, 1.0f, 1.0f, bHover ? 0.5f : 0.15f);
	DrawLine(X, Y, X + Width, Y, Edge, 1.0f);
	DrawLine(X, Y + Height, X + Width, Y + Height, Edge, 1.0f);
	DrawLine(X, Y, X, Y + Height, Edge, 1.0f);
	DrawLine(X + Width, Y, X + Width, Y + Height, Edge, 1.0f);
	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(Text, TextWidth, TextHeight, Font, TextSize * Scale);
	DrawLabel(Text, bEnabled ? FLinearColor::White : FLinearColor(1.0f, 1.0f, 1.0f, 0.3f), X + Width * 0.5f, Y + (Height - TextHeight) * 0.5f, TextSize, 0.5f);
	if (bHover && bClick)
	{
		bClick = false; // un seul bouton par clic
		DRLook::PlayAt(this, TEXT("ui_click"), ViewEye, -10.0f, 0.05f);
		return true;
	}
	return false;
}

void ADRHUD::DrawMainMenu(ADRGameMode* Mode)
{
	// Menu principal (Godot : menu.gd), devant la carte du dernier chapitre ouvert.
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	if (bLevelsPage)
	{
		DrawLevels(Mode);
		return;
	}
	if (bSettingsOpen)
	{
		DrawSettings(Mode);
		return;
	}
	if (bUpgradesPage)
	{
		DrawUpgrades(Mode);
		return;
	}
	const UDRSave* Progress = Mode->GetProgress();
	const float CenterX = Canvas->ClipX * 0.5f;
	float Y = Canvas->ClipY * 0.5f - 290.0f * Scale;
	DrawLabel(TEXT("DERNIER REMPART"), FLinearColor(0.95f, 0.92f, 0.88f), CenterX, Y, 3.0f, 0.5f);
	Y += 105.0f * Scale;
	DrawLabel(FString::Printf(TEXT("Tiens la base face aux zombies. %d chapitres, %d niveaux. Aucun renfort."), Mode->GetChapterCount(), Mode->GetLevelCount()),
		FLinearColor(0.85f, 0.75f, 0.65f), CenterX, Y, 0.7f, 0.5f);
	Y += 70.0f * Scale;
	const float Width = 420.0f * Scale;
	const float Left = CenterX - Width * 0.5f;
	if (Button(TEXT("JOUER"), Left, Y, Width, 56.0f * Scale, 0.95f, true, true))
	{
		bLevelsPage = true;
	}
	Y += 68.0f * Scale;
	if (Button(TEXT("AM\u00C9LIORATIONS"), Left, Y, Width, 50.0f * Scale, 0.8f))
	{
		bUpgradesPage = true;
	}
	Y += 62.0f * Scale;
	if (Button(TEXT("R\u00C9GLAGES"), Left, Y, Width, 50.0f * Scale, 0.8f))
	{
		bSettingsOpen = true;
	}
	Y += 62.0f * Scale;
	if (Button(TEXT("QUITTER"), Left, Y, Width, 50.0f * Scale, 0.8f))
	{
		Mode->QuitGame();
	}
	Y += 82.0f * Scale;
	const FString Record = Progress && Progress->BestScore > 0
		? FString::Printf(TEXT("Record : %d points  \u00B7  vague %d atteinte"), Progress->BestScore, Progress->BestWave)
		: FString(TEXT("Pas encore de record : \u00E0 toi de jouer."));
	DrawLabel(Record, DRHUDStyle::Accent, CenterX, Y, 0.6f, 0.5f);
	Y += 32.0f * Scale;
	DrawLabel(FString::Printf(TEXT("Niveaux d\u00E9bloqu\u00E9s : %d / %d   \u00B7   [%s] jouer le niveau %s"),
		(Progress ? Progress->UnlockedLevel : 0) + 1, Mode->GetLevelCount(), *DRHUDStyle::KeyOf(Mode, TEXT("vague")), *Mode->GetLevelShort()),
		FLinearColor(0.85f, 0.85f, 0.85f), CenterX, Y, 0.55f, 0.5f);
	Y += 28.0f * Scale;
	DrawLabel(FString::Printf(TEXT("%d insignes \u00E0 d\u00E9penser dans Am\u00E9liorations"), Progress ? Progress->Insignes : 0),
		FLinearColor(0.85f, 0.85f, 0.85f), CenterX, Y, 0.55f, 0.5f);
	DrawLabel(TEXT("Portage Unreal Engine 5 en cours  \u00B7  clavier AZERTY"), FLinearColor(1.0f, 1.0f, 1.0f, 0.4f), 16.0f * Scale, Canvas->ClipY - 34.0f * Scale, 0.45f);
}

void ADRHUD::DrawLevels(ADRGameMode* Mode)
{
	// Choix du niveau (Godot : level_panel.gd) : un chapitre par ligne, 5 niveaux chacun.
	const UDRSave* Progress = Mode->GetProgress();
	const int32 Unlocked = Progress ? Progress->UnlockedLevel : 0;
	const int32 Chapters = Mode->GetChapterCount();
	const float Width = 940.0f * Scale;
	const float Pad = 22.0f * Scale;
	const float RowHeight = FMath::Min(96.0f * Scale, (Canvas->ClipY - 230.0f * Scale) / FMath::Max(1, Chapters) - 8.0f * Scale);
	const float Height = Pad * 2.0f + 86.0f * Scale + Chapters * (RowHeight + 8.0f * Scale) + 56.0f * Scale;
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = FMath::Max(10.0f * Scale, (Canvas->ClipY - Height) * 0.5f);
	DrawRect(DRHUDStyle::Panel, Left, Top, Width, Height);
	float Y = Top + Pad;
	DrawLabel(TEXT("CHOISIS TON NIVEAU"), DRHUDStyle::Accent, Left + Pad, Y, 1.1f);
	Y += 48.0f * Scale;
	DrawLabel(TEXT("5 niveaux par chapitre, chacun plus dur que le pr\u00E9c\u00E9dent. Gagne le 5e pour ouvrir le chapitre suivant."), FLinearColor(0.7f, 0.7f, 0.7f), Left + Pad, Y, 0.5f);
	Y += 38.0f * Scale;
	const float ButtonSize = FMath::Min(52.0f * Scale, RowHeight - 16.0f * Scale);
	const float ButtonGap = 6.0f * Scale;
	const FLinearColor Dim(1.0f, 1.0f, 1.0f, 0.35f);
	for (int32 Index = 0; Index < Chapters; ++Index)
	{
		const int32 First = Index * 5;
		const bool bOpen = First <= Unlocked;
		DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, bOpen ? 0.04f : 0.015f), Left + Pad, Y, Width - 2.0f * Pad, RowHeight);
		DrawLabel(FString::Printf(TEXT("CHAPITRE %d \u00B7 %s"), Index + 1, *Mode->GetChapterText(Index, TEXT("name")).ToUpper()), bOpen ? FLinearColor::White : Dim, Left + Pad + 14.0f * Scale, Y + 10.0f * Scale, 0.62f);
		const FString About = bOpen
			? Mode->GetChapterText(Index, TEXT("place")) + TEXT(". ") + Mode->GetChapterText(Index, TEXT("desc"))
			: FString::Printf(TEXT("Gagne le niveau 5 de \u00AB %s \u00BB pour l'ouvrir."), *Mode->GetChapterText(Index - 1, TEXT("name")));
		const TArray<FString> Lines = Wrap(About, 0.45f, 520.0f * Scale);
		for (int32 Line = 0; Line < FMath::Min(2, Lines.Num()); ++Line)
		{
			DrawLabel(Lines[Line], bOpen ? FLinearColor(0.7f, 0.7f, 0.7f) : Dim, Left + Pad + 14.0f * Scale, Y + (40.0f + 20.0f * Line) * Scale, 0.45f);
		}
		float ButtonX = Left + Width - Pad - 14.0f * Scale - 5.0f * ButtonSize - 4.0f * ButtonGap;
		for (int32 Step = 0; Step < 5; ++Step)
		{
			const int32 LevelIndex = First + Step;
			const bool bWon = Progress && Progress->HasWon(LevelIndex);
			const bool bEnabled = LevelIndex <= Unlocked && LevelIndex < Mode->GetLevelCount();
			if (Button(FString::FromInt(Step + 1), ButtonX, Y + (RowHeight - ButtonSize) * 0.5f, ButtonSize, ButtonSize, 0.75f, bEnabled, LevelIndex == Unlocked && !bWon, bWon))
			{
				Mode->PlayLevel(LevelIndex);
			}
			ButtonX += ButtonSize + ButtonGap;
		}
		Y += RowHeight + 8.0f * Scale;
	}
	DrawLabel(TEXT("Vert : gagn\u00E9   \u00B7   orange : le prochain \u00E0 gagner"), FLinearColor(0.6f, 0.6f, 0.6f), Left + Pad, Y + 18.0f * Scale, 0.45f);
	if (Button(TEXT("RETOUR"), Left + Width - Pad - 170.0f * Scale, Y + 8.0f * Scale, 170.0f * Scale, 42.0f * Scale, 0.6f))
	{
		bLevelsPage = false;
	}
}

void ADRHUD::DrawPause(ADRGameMode* Mode)
{
	// Pause (Godot : pause_menu.gd) : la partie est figée.
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	if (bSettingsOpen)
	{
		DrawSettings(Mode);
		return;
	}
	const float Width = 460.0f * Scale;
	const float Height = 532.0f * Scale;
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = (Canvas->ClipY - Height) * 0.5f;
	const float CenterX = Canvas->ClipX * 0.5f;
	DrawRect(DRHUDStyle::Panel, Left, Top, Width, Height);
	DrawLabel(TEXT("PAUSE"), DRHUDStyle::Accent, CenterX, Top + 24.0f * Scale, 1.5f, 0.5f);
	DrawLabel(FString::Printf(TEXT("%s  \u00B7  vague %d / %d"), *Mode->GetLevelTitle(), Mode->GetWave(), Mode->GetLastWave()), FLinearColor(0.8f, 0.8f, 0.8f), CenterX, Top + 90.0f * Scale, 0.55f, 0.5f);
	const float ButtonWidth = Width - 80.0f * Scale;
	const float ButtonHeight = 50.0f * Scale;
	const float ButtonLeft = Left + 40.0f * Scale;
	float Y = Top + 135.0f * Scale;
	if (Button(TEXT("REPRENDRE"), ButtonLeft, Y, ButtonWidth, ButtonHeight, 0.8f, true, true))
	{
		Mode->TogglePause();
	}
	Y += ButtonHeight + 12.0f * Scale;
	if (Button(TEXT("R\u00C9GLAGES"), ButtonLeft, Y, ButtonWidth, ButtonHeight, 0.7f))
	{
		bSettingsOpen = true;
	}
	Y += ButtonHeight + 12.0f * Scale;
	if (Button(TEXT("RECOMMENCER LE NIVEAU"), ButtonLeft, Y, ButtonWidth, ButtonHeight, 0.7f))
	{
		Mode->PlayLevel(Mode->GetLevel());
	}
	Y += ButtonHeight + 12.0f * Scale;
	if (Button(TEXT("MENU PRINCIPAL"), ButtonLeft, Y, ButtonWidth, ButtonHeight, 0.7f))
	{
		Mode->OpenMainMenu();
	}
	Y += ButtonHeight + 12.0f * Scale;
	if (Button(TEXT("QUITTER LE JEU"), ButtonLeft, Y, ButtonWidth, ButtonHeight, 0.7f))
	{
		Mode->QuitGame();
	}
	DrawLabel(FString::Printf(TEXT("\u00C9chap ou %s : reprendre"), *DRHUDStyle::KeyOf(Mode, TEXT("pause"))), FLinearColor(1.0f, 1.0f, 1.0f, 0.5f), CenterX, Top + Height - 40.0f * Scale, 0.5f, 0.5f);
}

bool ADRHUD::BenchRow(const FString& Title, int32 Done, int32 Total, const FString& About, int32 Cost, int32 Scrap, float X, float Y, float Width)
{
	// Niveaux : une case orange par niveau acheté (comme les améliorations permanentes).
	DrawLabel(Title, FLinearColor::White, X, Y, 0.6f);
	float TitleWidth = 0.0f;
	float TitleHeight = 0.0f;
	GetTextSize(Title, TitleWidth, TitleHeight, Font, 0.6f * Scale);
	const float Pip = 12.0f * Scale;
	for (int32 Step = 0; Step < Total; ++Step)
	{
		DrawRect(Step < Done ? DRHUDStyle::Accent : FLinearColor(1.0f, 1.0f, 1.0f, 0.15f), X + TitleWidth + 12.0f * Scale + Step * (Pip + 5.0f * Scale), Y + 6.0f * Scale, Pip, Pip);
	}
	DrawLabel(About, FLinearColor(0.6f, 0.6f, 0.6f), X, Y + 24.0f * Scale, 0.42f);
	const float ButtonWidth = 180.0f * Scale;
	if (Cost < 0)
	{
		DrawLabel(TEXT("MAX"), DRHUDStyle::Accent, X + Width - ButtonWidth * 0.5f, Y + 10.0f * Scale, 0.6f, 0.5f);
		return false;
	}
	return Button(FString::Printf(TEXT("Am\u00E9liorer (%d)"), Cost), X + Width - ButtonWidth, Y + 2.0f * Scale, ButtonWidth, 36.0f * Scale, 0.5f, Scrap >= Cost);
}

void ADRHUD::DrawWorkbench(ADRGameMode* Mode)
{
	// La partie est figée tant que l'établi est ouvert. Tout se paie en ferraille.
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	const ADRCharacter* Shooter = Cast<ADRCharacter>(GetOwningPawn());
	const int32 Scrap = Mode->GetScrap();
	const float Width = 980.0f * Scale;
	const float Pad = 22.0f * Scale;
	const float Gap = 12.0f * Scale;
	const float CardPad = 14.0f * Scale;
	const float WeaponHeight = 330.0f * Scale;
	const float LowerHeight = 236.0f * Scale;
	const float Height = Pad * 2.0f + 46.0f * Scale + 30.0f * Scale + WeaponHeight + Gap + LowerHeight + Gap + 44.0f * Scale;
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = FMath::Max(10.0f * Scale, (Canvas->ClipY - Height) * 0.5f);
	DrawRect(DRHUDStyle::Panel, Left, Top, Width, Height);
	float Y = Top + Pad;
	DrawLabel(TEXT("\u00C9TABLI"), DRHUDStyle::Accent, Left + Pad, Y, 1.2f);
	DrawLabel(FString::Printf(TEXT("%d ferraille"), Scrap), DRHUDStyle::Accent, Left + Width - Pad, Y + 8.0f * Scale, 0.85f, 1.0f);
	Y += 46.0f * Scale;
	DrawLabel(TEXT("Tout se paie en ferraille. Les munitions achet\u00E9es restent \u00E0 toi : tu peux en changer gratuitement ensuite."), FLinearColor(0.6f, 0.6f, 0.6f), Left + Pad, Y, 0.45f);
	Y += 30.0f * Scale;
	bool bBought = false;

	// Une carte par arme : canon, chargeur, munitions.
	const int32 WeaponCount = Shooter ? FMath::Min(2, Shooter->GetWeaponCount()) : 0;
	const float CardWidth = (Width - 2.0f * Pad - Gap) * 0.5f;
	for (int32 Index = 0; Index < WeaponCount; ++Index)
	{
		const FDRWeapon& Weapon = Shooter->GetWeaponAt(Index);
		const FDRWeaponMods Mods = Mode->GetWeaponMods(Weapon.Id);
		const float CardLeft = Left + Pad + Index * (CardWidth + Gap);
		const float Inner = CardWidth - 2.0f * CardPad;
		const float X = CardLeft + CardPad;
		float CardY = Y + CardPad;
		DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.04f), CardLeft, Y, CardWidth, WeaponHeight);
		DrawLabel(Weapon.Name.ToUpper(), FLinearColor::White, X, CardY, 0.7f);
		CardY += 30.0f * Scale;
		DrawLabel(FString::Printf(TEXT("%d d\u00E9g\u00E2ts par balle  \u00B7  %d balles  \u00B7  munitions %s"), FMath::RoundToInt(Shooter->WeaponDamage(Index)), Shooter->MagSize(Index),
			*Mode->GetAmmoText(Mods.Ammo, TEXT("name")).ToLower()), FLinearColor(0.6f, 0.6f, 0.6f), X, CardY, 0.42f);
		CardY += 28.0f * Scale;
		if (BenchRow(TEXT("Canon"), Mods.Barrel, Mode->GetBenchMax(TEXT("canon")), TEXT("+20 % de d\u00E9g\u00E2ts par niveau"), Mode->GetBenchCost(TEXT("canon"), Mods.Barrel), Scrap, X, CardY, Inner))
		{
			bBought = Mode->BuyBarrel(Weapon.Id);
		}
		CardY += 50.0f * Scale;
		if (BenchRow(TEXT("Chargeur"), Mods.Mag, Mode->GetBenchMax(TEXT("chargeur")), TEXT("+30 % de balles par niveau"), Mode->GetBenchCost(TEXT("chargeur"), Mods.Mag), Scrap, X, CardY, Inner))
		{
			bBought = Mode->BuyMag(Weapon.Id);
		}
		CardY += 52.0f * Scale;
		DrawLabel(TEXT("Munitions"), DRHUDStyle::Accent, X, CardY, 0.52f);
		CardY += 26.0f * Scale;
		const TArray<FName>& AmmoIds = ADRGameMode::GetAmmoIds();
		const float AmmoWidth = (Inner - 8.0f * Scale) * 0.5f;
		for (int32 Slot = 0; Slot < AmmoIds.Num(); ++Slot)
		{
			const FName AmmoId = AmmoIds[Slot];
			const bool bOwned = Mods.Owns(AmmoId);
			const int32 AmmoCost = Mode->GetAmmoCost(AmmoId);
			const FString Text = bOwned ? Mode->GetAmmoText(AmmoId, TEXT("name")) : FString::Printf(TEXT("%s  (%d)"), *Mode->GetAmmoText(AmmoId, TEXT("name")), AmmoCost);
			const float AmmoX = X + (Slot % 2) * (AmmoWidth + 8.0f * Scale);
			const float AmmoY = CardY + (Slot / 2) * 40.0f * Scale;
			if (Button(Text, AmmoX, AmmoY, AmmoWidth, 34.0f * Scale, 0.48f, bOwned || Scrap >= AmmoCost, Mods.Ammo == AmmoId))
			{
				bBought = Mode->ChooseAmmo(Weapon.Id, AmmoId);
			}
		}
		CardY += 2.0f * 40.0f * Scale + 4.0f * Scale;
		const TArray<FString> Lines = Wrap(Mode->GetAmmoText(Mods.Ammo, TEXT("desc")), 0.42f, Inner);
		const FLinearColor Tint = DRHUDStyle::AmmoColor(Mods.Ammo);
		for (int32 Line = 0; Line < FMath::Min(2, Lines.Num()); ++Line)
		{
			DrawLabel(Lines[Line], Tint + (FLinearColor::White - Tint) * 0.4f, X, CardY + 20.0f * Scale * Line, 0.42f);
		}
	}
	Y += WeaponHeight + Gap;

	// Gadgets (à gauche) : un gadget par emplacement, à choisir parmi les trois (Godot : _gadget_card).
	const float GadgetWidth = (Width - 2.0f * Pad - Gap) * 0.6f;
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.04f), Left + Pad, Y, GadgetWidth, LowerHeight);
	const float GadgetX = Left + Pad + CardPad;
	const float GadgetInner = GadgetWidth - 2.0f * CardPad;
	DrawLabel(TEXT("GADGETS"), FLinearColor::White, GadgetX, Y + CardPad, 0.7f);
	DrawLabel(TEXT("Deux emplacements. Chaque gadget se recharge apr\u00E8s usage."), FLinearColor(0.6f, 0.6f, 0.6f), GadgetX, Y + CardPad + 30.0f * Scale, 0.42f);
	const TArray<FName>& GadgetIds = ADRGameMode::GetGadgetIds();
	const float KeyWidth = 56.0f * Scale;
	const float ChoiceWidth = (GadgetInner - KeyWidth - 2.0f * 6.0f * Scale) / FMath::Max(1, GadgetIds.Num());
	for (int32 Slot = 0; Slot < 2; ++Slot)
	{
		const float RowY = Y + CardPad + 58.0f * Scale + Slot * 42.0f * Scale;
		DrawLabel(FString::Printf(TEXT("[%s]"), *DRHUDStyle::KeyOf(Mode, Slot == 0 ? TEXT("gadget1") : TEXT("gadget2"))), DRHUDStyle::Accent, GadgetX, RowY + 6.0f * Scale, 0.55f);
		for (int32 Choice = 0; Choice < GadgetIds.Num(); ++Choice)
		{
			const FName GadgetId = GadgetIds[Choice];
			if (Button(Mode->GetGadgetText(GadgetId, TEXT("name")), GadgetX + KeyWidth + Choice * (ChoiceWidth + 6.0f * Scale), RowY, ChoiceWidth, 34.0f * Scale, 0.46f, true, Mode->GetGadgetSlot(Slot) == GadgetId))
			{
				Mode->SetGadget(Slot, GadgetId);
			}
		}
	}
	float AboutY = Y + CardPad + 58.0f * Scale + 2.0f * 42.0f * Scale + 8.0f * Scale;
	for (const FName& GadgetId : GadgetIds)
	{
		DrawLabel(FString::Printf(TEXT("%s : %s Recharge %d s."), *Mode->GetGadgetText(GadgetId, TEXT("name")), *Mode->GetGadgetText(GadgetId, TEXT("desc")), FMath::RoundToInt(Mode->GetGadgetRecharge(GadgetId))),
			FLinearColor(0.6f, 0.6f, 0.6f), GadgetX, AboutY, 0.4f);
		AboutY += 21.0f * Scale;
	}

	// Générateur (à droite) : plus d'énergie = plus de tours allumées en même temps.
	const float GeneratorLeft = Left + Pad + GadgetWidth + Gap;
	const float GeneratorWidth = Width - 2.0f * Pad - Gap - GadgetWidth;
	DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.04f), GeneratorLeft, Y, GeneratorWidth, LowerHeight);
	const float GenX = GeneratorLeft + CardPad;
	const float GenInner = GeneratorWidth - 2.0f * CardPad;
	DrawLabel(TEXT("G\u00C9N\u00C9RATEUR"), FLinearColor::White, GenX, Y + CardPad, 0.7f);
	const TArray<FString> GenLines = Wrap(TEXT("Plus d'\u00E9nergie = plus de tours allum\u00E9es en m\u00EAme temps."), 0.42f, GenInner);
	for (int32 Line = 0; Line < FMath::Min(2, GenLines.Num()); ++Line)
	{
		DrawLabel(GenLines[Line], FLinearColor(0.6f, 0.6f, 0.6f), GenX, Y + CardPad + 30.0f * Scale + Line * 20.0f * Scale, 0.42f);
	}
	if (BenchRow(TEXT("Niveau"), Mode->GetGenerator(), Mode->GetBenchMax(TEXT("generateur")), FString(),
		Mode->GetBenchCost(TEXT("generateur"), Mode->GetGenerator()), Scrap, GenX, Y + CardPad + 84.0f * Scale, GenInner))
	{
		bBought = Mode->BuyGenerator();
	}
	DrawLabel(FString::Printf(TEXT("\u00C9nergie max : %d"), Mode->GetEnergyCap()), FLinearColor::White, GenX, Y + CardPad + 138.0f * Scale, 0.5f);
	DrawLabel(TEXT("+2 \u00E9nergie par niveau"), FLinearColor(0.6f, 0.6f, 0.6f), GenX, Y + CardPad + 164.0f * Scale, 0.42f);
	Y += LowerHeight + Gap;

	DrawLabel(FString::Printf(TEXT("%s ou \u00C9chap pour fermer"), *DRHUDStyle::KeyOf(Mode, TEXT("interagir"))), FLinearColor(0.6f, 0.6f, 0.6f),
		Left + Width - Pad - 180.0f * Scale, Y + 12.0f * Scale, 0.45f, 1.0f);
	if (Button(TEXT("FERMER"), Left + Width - Pad - 160.0f * Scale, Y, 160.0f * Scale, 40.0f * Scale, 0.6f, true, true))
	{
		Mode->CloseWorkbench();
	}
	if (bBought)
	{
		DRLook::PlayAt(this, TEXT("upgrade"), ViewEye, -4.0f, 0.05f);
	}
}

void ADRHUD::DrawImplants(ADRGameMode* Mode)
{
	// Siège repoussé : la partie est figée jusqu'à ce qu'on clique sur un des implants proposés.
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
	const TArray<FName>& Options = Mode->GetImplantOptions();
	const float Width = 580.0f * Scale;
	const float Pad = 24.0f * Scale;
	const float CardHeight = 94.0f * Scale;
	const float CardGap = 12.0f * Scale;
	const float Height = Pad * 2.0f + 90.0f * Scale + Options.Num() * (CardHeight + CardGap) - CardGap;
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = (Canvas->ClipY - Height) * 0.5f;
	const float CenterX = Canvas->ClipX * 0.5f;
	DrawRect(DRHUDStyle::Panel, Left, Top, Width, Height);
	DrawLabel(TEXT("SI\u00C8GE REPOUSS\u00C9"), DRHUDStyle::Accent, CenterX, Top + Pad, 1.2f, 0.5f);
	DrawLabel(TEXT("Choisis un implant : chacun a un bonus ET un malus"), FLinearColor(0.85f, 0.85f, 0.85f), CenterX, Top + Pad + 52.0f * Scale, 0.55f, 0.5f);
	float Y = Top + Pad + 90.0f * Scale;
	FName Picked;
	for (const FName& Id : Options)
	{
		if (Button(FString(), Left + Pad, Y, Width - 2.0f * Pad, CardHeight, 0.5f))
		{
			Picked = Id;
		}
		const float TextLeft = Left + Pad + 16.0f * Scale;
		DrawLabel(Mode->GetImplantText(Id, TEXT("name")), FLinearColor::White, TextLeft, Y + 10.0f * Scale, 0.72f);
		DrawLabel(TEXT("+ ") + Mode->GetImplantText(Id, TEXT("plus")), FLinearColor(0.45f, 0.9f, 0.5f), TextLeft, Y + 42.0f * Scale, 0.52f);
		DrawLabel(TEXT("- ") + Mode->GetImplantText(Id, TEXT("minus")), FLinearColor(1.0f, 0.5f, 0.45f), TextLeft, Y + 66.0f * Scale, 0.52f);
		Y += CardHeight + CardGap;
	}
	// Après la boucle : le choix vide la liste qu'on parcourt.
	if (!Picked.IsNone())
	{
		Mode->ChooseImplant(Picked);
	}
}

bool ADRHUD::Back()
{
	// Une touche qu'on vient de donner à une action peut la déclencher juste après : on l'ignore.
	const UWorld* World = GetWorld();
	if (World && World->GetRealTimeSeconds() - RebindTime < 0.4)
	{
		return bSettingsOpen;
	}
	if (!WaitingKey.IsNone())
	{
		WaitingKey = NAME_None;
		return true;
	}
	if (bSettingsOpen)
	{
		bSettingsOpen = false;
		return true;
	}
	if (bLevelsPage || bUpgradesPage)
	{
		bLevelsPage = false;
		bUpgradesPage = false;
		return true;
	}
	return false;
}

void ADRHUD::DrawUpgrades(ADRGameMode* Mode)
{
	// Améliorations permanentes (Godot : upgrade_panel.gd) : on dépense les insignes gagnés en jouant.
	// Les achats sont sauvegardés tout de suite et comptent dès la partie suivante.
	const UDRSave* Progress = Mode->GetProgress();
	const int32 Insignes = Progress ? Progress->Insignes : 0;
	const TArray<FName>& Ids = ADRGameMode::GetUpgradeIds();
	const float Width = 900.0f * Scale;
	const float Pad = 24.0f * Scale;
	const float RowHeight = 44.0f * Scale;
	const float RowGap = 8.0f * Scale;
	const float Height = Pad * 2.0f + 50.0f * Scale + 56.0f * Scale + Ids.Num() * (RowHeight + RowGap) + 8.0f * Scale + 42.0f * Scale;
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = FMath::Max(10.0f * Scale, (Canvas->ClipY - Height) * 0.5f);
	DrawRect(DRHUDStyle::Panel, Left, Top, Width, Height);
	float Y = Top + Pad;
	DrawLabel(TEXT("AM\u00C9LIORATIONS PERMANENTES"), DRHUDStyle::Accent, Left + Pad, Y, 1.1f);
	DrawLabel(FString::Printf(TEXT("%d insignes"), Insignes), DRHUDStyle::Accent, Left + Width - Pad, Y + 8.0f * Scale, 0.85f, 1.0f);
	Y += 50.0f * Scale;
	const TArray<FString> Lines = Wrap(TEXT("Chaque partie rapporte des insignes : 1 par vague repouss\u00E9e (plus dans les niveaux durs), et une prime en cas de victoire."),
		0.48f, Width - 2.0f * Pad);
	for (int32 Line = 0; Line < FMath::Min(2, Lines.Num()); ++Line)
	{
		DrawLabel(Lines[Line], FLinearColor(0.65f, 0.65f, 0.65f), Left + Pad, Y + 22.0f * Scale * Line, 0.48f);
	}
	Y += 56.0f * Scale;
	const float BuyWidth = 190.0f * Scale;
	const float Pip = 14.0f * Scale;
	const float PipGap = 6.0f * Scale;
	for (const FName& Id : Ids)
	{
		DrawRect(FLinearColor(1.0f, 1.0f, 1.0f, 0.04f), Left + Pad, Y, Width - 2.0f * Pad, RowHeight);
		DrawLabel(Mode->GetUpgradeText(Id, TEXT("name")), FLinearColor::White, Left + Pad + 12.0f * Scale, Y + 11.0f * Scale, 0.65f);
		DrawLabel(Mode->GetUpgradeText(Id, TEXT("desc")) + TEXT(" par rang"), FLinearColor(0.85f, 0.85f, 0.85f), Left + Pad + 210.0f * Scale, Y + 13.0f * Scale, 0.5f);
		// Rangs : une case orange par rang acheté, grise pour ceux qui restent.
		const int32 Rank = Mode->GetUpgradeRank(Id);
		const int32 MaxRank = Mode->GetUpgradeMaxRank(Id);
		const float PipsLeft = Left + Width - Pad - 12.0f * Scale - BuyWidth - 24.0f * Scale - 3.0f * (Pip + PipGap);
		for (int32 Step = 0; Step < MaxRank; ++Step)
		{
			DrawRect(Step < Rank ? DRHUDStyle::Accent : FLinearColor(1.0f, 1.0f, 1.0f, 0.15f), PipsLeft + Step * (Pip + PipGap), Y + (RowHeight - Pip) * 0.5f, Pip, Pip);
		}
		const int32 Cost = Mode->GetUpgradeCost(Id);
		const FString BuyText = Cost < 0 ? FString(TEXT("MAXIMUM")) : FString::Printf(TEXT("ACHETER (%d)"), Cost);
		if (Button(BuyText, Left + Width - Pad - 6.0f * Scale - BuyWidth, Y + 5.0f * Scale, BuyWidth, RowHeight - 10.0f * Scale, 0.5f, Cost >= 0 && Insignes >= Cost))
		{
			if (Mode->BuyUpgrade(Id))
			{
				DRLook::PlayAt(this, TEXT("upgrade"), ViewEye, -6.0f, 0.05f);
			}
		}
		Y += RowHeight + RowGap;
	}
	Y += 8.0f * Scale;
	if (Button(TEXT("TOUT REMBOURSER"), Left + Pad, Y, 240.0f * Scale, 42.0f * Scale, 0.55f))
	{
		Mode->RefundUpgrades();
	}
	if (Button(TEXT("RETOUR"), Left + Width - Pad - 170.0f * Scale, Y, 170.0f * Scale, 42.0f * Scale, 0.6f, true, true))
	{
		bUpgradesPage = false;
	}
}

void ADRHUD::PollRebind(ADRGameMode* Mode)
{
	APlayerController* PlayerController = GetOwningPlayerController();
	UDRSave* Progress = Mode->GetProgress();
	if (WaitingKey.IsNone() || !PlayerController || !Progress)
	{
		return;
	}
	TArray<FKey> AllKeys;
	EKeys::GetAllKeys(AllKeys);
	for (const FKey& Pressed : AllKeys)
	{
		// Ni la molette (arme ou tour), ni Échap (annuler), ni les axes, la manette ou l'écran tactile.
		if (!Pressed.IsValid() || Pressed.IsAxis1D() || Pressed.IsAxis2D() || Pressed.IsAxis3D() || Pressed.IsGamepadKey() || Pressed.IsTouch()
			|| Pressed == EKeys::AnyKey || Pressed == EKeys::Escape || Pressed == EKeys::MouseScrollUp || Pressed == EKeys::MouseScrollDown)
		{
			continue;
		}
		if (PlayerController->WasInputKeyJustPressed(Pressed))
		{
			Progress->SetKey(WaitingKey, Pressed);
			Progress->Write();
			WaitingKey = NAME_None;
			RebindTime = GetWorld()->GetRealTimeSeconds();
			bClick = false; // ce clic était la nouvelle touche, pas un clic sur un bouton
			if (ADRCharacter* Shooter = Cast<ADRCharacter>(GetOwningPawn()))
			{
				Shooter->ApplyBindings();
			}
			return;
		}
	}
}

void ADRHUD::DrawSettings(ADRGameMode* Mode)
{
	// Réglages (Godot : settings_panel.gd). Tout est sauvegardé tout de suite.
	UDRSave* Progress = Mode->GetProgress();
	if (!Progress)
	{
		bSettingsOpen = false;
		return;
	}
	PollRebind(Mode); // avant les boutons : un clic peut être la nouvelle touche
	ADRCharacter* Shooter = Cast<ADRCharacter>(GetOwningPawn());
	const TArray<FDRBinding>& Bindings = UDRSave::Bindable();
	const int32 KeyRows = (Bindings.Num() + 1) / 2;
	const float Width = 880.0f * Scale;
	const float Pad = 24.0f * Scale;
	const float RowHeight = 38.0f * Scale;
	const float RowGap = 8.0f * Scale;
	const float KeyHeight = 32.0f * Scale;
	const float KeyGap = 6.0f * Scale;
	const float Height = Pad * 2.0f + 52.0f * Scale + 5.0f * (RowHeight + RowGap) + 44.0f * Scale + KeyRows * (KeyHeight + KeyGap) + 40.0f * Scale + 42.0f * Scale;
	const float Left = (Canvas->ClipX - Width) * 0.5f;
	const float Top = FMath::Max(10.0f * Scale, (Canvas->ClipY - Height) * 0.5f);
	DrawRect(DRHUDStyle::Panel, Left, Top, Width, Height);
	float Y = Top + Pad;
	DrawLabel(TEXT("R\u00C9GLAGES"), DRHUDStyle::Accent, Left + Pad, Y, 1.1f);
	Y += 52.0f * Scale;

	// Un réglage chiffré : - valeur + (mêmes bornes que les curseurs de Godot).
	bool bChanged = false;
	const float ControlLeft = Left + 420.0f * Scale;
	const float ControlWidth = Left + Width - Pad - ControlLeft;
	const float SmallWidth = 48.0f * Scale;
	const float StepperWidth = SmallWidth * 2.0f + 140.0f * Scale;
	auto Stepper = [&](const TCHAR* Caption, const FString& Shown, float& Setting, float Step, float Low, float High)
	{
		DrawLabel(Caption, FLinearColor::White, Left + Pad, Y + 8.0f * Scale, 0.6f);
		if (Button(TEXT("-"), ControlLeft, Y, SmallWidth, RowHeight, 0.8f, Setting > Low + 0.001f))
		{
			Setting = FMath::Clamp(FMath::GridSnap(Setting - Step, Step), Low, High);
			bChanged = true;
		}
		DrawLabel(Shown, DRHUDStyle::Accent, ControlLeft + StepperWidth * 0.5f, Y + 8.0f * Scale, 0.6f, 0.5f);
		if (Button(TEXT("+"), ControlLeft + StepperWidth - SmallWidth, Y, SmallWidth, RowHeight, 0.8f, Setting < High - 0.001f))
		{
			Setting = FMath::Clamp(FMath::GridSnap(Setting + Step, Step), Low, High);
			bChanged = true;
		}
		Y += RowHeight + RowGap;
	};
	Stepper(TEXT("Sensibilit\u00E9 de la souris"), FString::Printf(TEXT("%.2f x"), Progress->Sensitivity), Progress->Sensitivity, 0.1f, 0.2f, 3.0f);
	Stepper(TEXT("Volume"), FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Progress->Volume * 100.0f)), Progress->Volume, 0.05f, 0.0f, 1.0f);
	Stepper(TEXT("Champ de vision"), FString::Printf(TEXT("%d\u00B0"), FMath::RoundToInt(Progress->Fov)), Progress->Fov, 2.0f, 60.0f, 100.0f);

	DrawLabel(TEXT("Inverser l'axe vertical"), FLinearColor::White, Left + Pad, Y + 8.0f * Scale, 0.6f);
	if (Button(Progress->bInvertY ? TEXT("OUI") : TEXT("NON"), ControlLeft, Y, StepperWidth, RowHeight, 0.6f, true, Progress->bInvertY))
	{
		Progress->bInvertY = !Progress->bInvertY;
		bChanged = true;
	}
	Y += RowHeight + RowGap;

	// Qualité d'Unreal (gardée par Unreal dans GameUserSettings.ini). Basse et moyenne coupent Lumen.
	DrawLabel(TEXT("Qualit\u00E9 graphique"), FLinearColor::White, Left + Pad, Y + 8.0f * Scale, 0.6f);
	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		const TCHAR* const QualityNames[] = { TEXT("BASSE"), TEXT("MOYENNE"), TEXT("HAUTE"), TEXT("\u00C9PIQUE") };
		const int32 Current = UserSettings->GetOverallScalabilityLevel();
		const float QualityGap = 8.0f * Scale;
		const float QualityWidth = (ControlWidth - 3.0f * QualityGap) / 4.0f;
		for (int32 Choice = 0; Choice < 4; ++Choice)
		{
			if (Button(QualityNames[Choice], ControlLeft + Choice * (QualityWidth + QualityGap), Y, QualityWidth, RowHeight, 0.5f, true, Choice == Current))
			{
				UserSettings->SetOverallScalabilityLevel(Choice);
				UserSettings->ApplyNonResolutionSettings();
				UserSettings->SaveSettings();
			}
		}
	}
	Y += RowHeight + RowGap;

	// Touches : deux colonnes, comme le tableau de Godot.
	Y += 6.0f * Scale;
	DrawLabel(TEXT("TOUCHES  (clique sur une touche puis appuie sur la nouvelle)"), DRHUDStyle::Accent, Left + Pad, Y, 0.6f);
	Y += 38.0f * Scale;
	const float ColumnGap = 28.0f * Scale;
	const float ColumnWidth = (Width - 2.0f * Pad - ColumnGap) * 0.5f;
	const float KeyWidth = 150.0f * Scale;
	for (int32 Index = 0; Index < Bindings.Num(); ++Index)
	{
		const FDRBinding& Entry = Bindings[Index];
		const float ColumnLeft = Left + Pad + (Index / KeyRows) * (ColumnWidth + ColumnGap);
		const float RowY = Y + (Index % KeyRows) * (KeyHeight + KeyGap);
		const bool bWaiting = WaitingKey == Entry.Id;
		DrawLabel(Entry.Label, FLinearColor(0.85f, 0.85f, 0.85f), ColumnLeft, RowY + 7.0f * Scale, 0.5f);
		if (Button(bWaiting ? FString(TEXT("...")) : Progress->KeyName(Entry.Id), ColumnLeft + ColumnWidth - KeyWidth, RowY, KeyWidth, KeyHeight, 0.5f, true, bWaiting))
		{
			WaitingKey = Entry.Id;
		}
	}
	Y += KeyRows * (KeyHeight + KeyGap) + 6.0f * Scale;
	DrawLabel(WaitingKey.IsNone()
		? TEXT("Si la touche est d\u00E9j\u00E0 prise, les deux actions \u00E9changent leurs touches. La molette change toujours d'arme ou de tour.")
		: TEXT("Appuie sur une touche ou un bouton de souris (\u00C9chap pour annuler)."), FLinearColor(0.65f, 0.65f, 0.65f), Left + Pad, Y, 0.45f);
	Y += 34.0f * Scale;

	if (Button(TEXT("TOUCHES PAR D\u00C9FAUT"), Left + Pad, Y, 260.0f * Scale, 42.0f * Scale, 0.55f))
	{
		Progress->ResetKeys();
		Progress->Write();
		WaitingKey = NAME_None;
		if (Shooter)
		{
			Shooter->ApplyBindings();
		}
	}
	if (Button(TEXT("RETOUR"), Left + Width - Pad - 170.0f * Scale, Y, 170.0f * Scale, 42.0f * Scale, 0.6f, true, true))
	{
		bSettingsOpen = false;
		WaitingKey = NAME_None;
	}
	if (bChanged)
	{
		Progress->Write();
		DRLook::SetMasterVolume(Progress->Volume);
		if (Shooter)
		{
			Shooter->ApplySettings();
		}
	}
}
