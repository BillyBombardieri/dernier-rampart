#include "DRSave.h"

#include "Kismet/GameplayStatics.h"

namespace DRSaveSlot
{
	const TCHAR* const Name = TEXT("Progression");
}

UDRSave* UDRSave::LoadOrCreate()
{
	if (UGameplayStatics::DoesSaveGameExist(DRSaveSlot::Name, 0))
	{
		if (UDRSave* Loaded = Cast<UDRSave>(UGameplayStatics::LoadGameFromSlot(DRSaveSlot::Name, 0)))
		{
			return Loaded;
		}
	}
	return Cast<UDRSave>(UGameplayStatics::CreateSaveGameObject(UDRSave::StaticClass()));
}

void UDRSave::Write()
{
	UGameplayStatics::SaveGameToSlot(this, DRSaveSlot::Name, 0);
}

bool UDRSave::SubmitScore(int32 FinalScore, int32 ReachedWave)
{
	const bool bRecord = FinalScore > BestScore;
	if (bRecord)
	{
		BestScore = FinalScore;
	}
	BestWave = FMath::Max(BestWave, ReachedWave);
	return bRecord;
}

bool UDRSave::FinishLevel(int32 LevelIndex, bool bWon, int32 LevelTotal, int32 Earned)
{
	Insignes += FMath::Max(0, Earned);
	if (!bWon)
	{
		return false;
	}
	LevelsWon.AddUnique(LevelIndex);
	if (LevelIndex + 1 < LevelTotal && UnlockedLevel < LevelIndex + 1)
	{
		UnlockedLevel = LevelIndex + 1;
		return true;
	}
	return false;
}

const TArray<FDRBinding>& UDRSave::Bindable()
{
	// Construite au premier appel : les touches d'Unreal (EKeys) ne sont prêtes qu'après le démarrage.
	static const TArray<FDRBinding> List = {
		{ TEXT("avancer"), TEXT("Avancer"), EKeys::Z },
		{ TEXT("reculer"), TEXT("Reculer"), EKeys::S },
		{ TEXT("gauche"), TEXT("Aller \u00E0 gauche"), EKeys::Q },
		{ TEXT("droite"), TEXT("Aller \u00E0 droite"), EKeys::D },
		{ TEXT("sauter"), TEXT("Sauter"), EKeys::SpaceBar },
		{ TEXT("courir"), TEXT("Courir"), EKeys::LeftShift },
		{ TEXT("tirer"), TEXT("Tirer"), EKeys::LeftMouseButton },
		{ TEXT("viser"), TEXT("Viser"), EKeys::RightMouseButton },
		{ TEXT("recharger"), TEXT("Recharger"), EKeys::R },
		{ TEXT("pistolet"), TEXT("Pistolet lourd"), EKeys::Ampersand },
		{ TEXT("fusil"), TEXT("Fusil d'assaut"), EKeys::E_AccentAigu },
		{ TEXT("marquer"), TEXT("Marquer un zombie"), EKeys::F },
		{ TEXT("interagir"), TEXT("Construire, am\u00E9liorer, r\u00E9parer"), EKeys::E },
		{ TEXT("tour"), TEXT("Changer de tour \u00E0 construire"), EKeys::C },
		{ TEXT("allumer"), TEXT("Allumer / \u00E9teindre une tour"), EKeys::X },
		{ TEXT("gadget1"), TEXT("Gadget 1"), EKeys::A },
		{ TEXT("gadget2"), TEXT("Gadget 2"), EKeys::G },
		{ TEXT("vague"), TEXT("Lancer la vague"), EKeys::Enter },
		{ TEXT("pause"), TEXT("Pause"), EKeys::P },
	};
	return List;
}

FKey UDRSave::DefaultKey(FName Id)
{
	for (const FDRBinding& Entry : Bindable())
	{
		if (Entry.Id == Id)
		{
			return Entry.Default;
		}
	}
	return EKeys::Invalid;
}

FKey UDRSave::GetKey(FName Id) const
{
	if (const FKey* Chosen = Keys.Find(Id))
	{
		if (Chosen->IsValid())
		{
			return *Chosen;
		}
	}
	return DefaultKey(Id);
}

void UDRSave::SetKey(FName Id, const FKey& NewKey)
{
	const FKey Old = GetKey(Id);
	for (const FDRBinding& Entry : Bindable())
	{
		if (Entry.Id != Id && GetKey(Entry.Id) == NewKey)
		{
			Keys.Add(Entry.Id, Old);
		}
	}
	Keys.Add(Id, NewKey);
}

FString UDRSave::KeyLabel(const FKey& Key)
{
	// Comme Settings.key_label dans Godot : des noms courts en français pour les touches courantes.
	struct FNamedKey
	{
		FKey Code;
		const TCHAR* Text;
	};
	static const TArray<FNamedKey> Names = {
		{ EKeys::LeftMouseButton, TEXT("Clic gauche") },
		{ EKeys::RightMouseButton, TEXT("Clic droit") },
		{ EKeys::MiddleMouseButton, TEXT("Clic molette") },
		{ EKeys::ThumbMouseButton, TEXT("Souris 4") },
		{ EKeys::ThumbMouseButton2, TEXT("Souris 5") },
		{ EKeys::SpaceBar, TEXT("Espace") },
		{ EKeys::LeftShift, TEXT("Maj") },
		{ EKeys::RightShift, TEXT("Maj droite") },
		{ EKeys::LeftControl, TEXT("Ctrl") },
		{ EKeys::RightControl, TEXT("Ctrl droit") },
		{ EKeys::LeftAlt, TEXT("Alt") },
		{ EKeys::RightAlt, TEXT("Alt Gr") },
		{ EKeys::Enter, TEXT("Entr\u00E9e") },
		{ EKeys::BackSpace, TEXT("Retour arri\u00E8re") },
		{ EKeys::Tab, TEXT("Tab") },
		{ EKeys::CapsLock, TEXT("Verr. maj") },
		{ EKeys::Up, TEXT("Fl\u00E8che haut") },
		{ EKeys::Down, TEXT("Fl\u00E8che bas") },
		{ EKeys::Left, TEXT("Fl\u00E8che gauche") },
		{ EKeys::Right, TEXT("Fl\u00E8che droite") },
		{ EKeys::Delete, TEXT("Suppr") },
		{ EKeys::Insert, TEXT("Inser") },
	};
	for (const FNamedKey& Entry : Names)
	{
		if (Entry.Code == Key)
		{
			return Entry.Text;
		}
	}
	return Key.IsValid() ? Key.GetDisplayName(false).ToString() : FString(TEXT("?"));
}

FString UDRSave::MoveKeysName() const
{
	const FString Parts[] = { KeyName(TEXT("avancer")), KeyName(TEXT("gauche")), KeyName(TEXT("reculer")), KeyName(TEXT("droite")) };
	bool bShort = true;
	for (const FString& Part : Parts)
	{
		bShort = bShort && Part.Len() == 1;
	}
	return bShort ? Parts[0] + Parts[1] + Parts[2] + Parts[3] : FString::Join(TArray<FString>(Parts, 4), TEXT(" / "));
}
