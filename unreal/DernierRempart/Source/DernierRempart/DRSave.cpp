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

bool UDRSave::FinishLevel(int32 LevelIndex, bool bWon, int32 LevelTotal)
{
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
