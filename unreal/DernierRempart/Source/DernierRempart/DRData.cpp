#include "DRData.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

TSharedPtr<FJsonObject> DRData::Load(const FString& FileName)
{
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"), FileName);
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogTemp, Error, TEXT("DernierRempart : fichier introuvable : %s"), *Path);
		return nullptr;
	}
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("DernierRempart : fichier illisible : %s"), *Path);
		return nullptr;
	}
	return Root;
}

FVector DRData::Vector(const TSharedPtr<FJsonValue>& Value)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Value.IsValid() && Value->TryGetArray(Values) && Values->Num() >= 3)
	{
		return FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber());
	}
	return FVector::ZeroVector;
}

FVector DRData::Vector(const TSharedPtr<FJsonObject>& Object, const FString& Field)
{
	return Object.IsValid() ? Vector(Object->TryGetField(Field)) : FVector::ZeroVector;
}

double DRData::Number(const TSharedPtr<FJsonObject>& Object, const FString& Field, double Default)
{
	double Value = Default;
	if (Object.IsValid())
	{
		Object->TryGetNumberField(Field, Value);
	}
	return Value;
}

FLinearColor DRData::ColorSRGB(const TSharedPtr<FJsonObject>& Object, const FString& Field, const FLinearColor& Default)
{
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Object.IsValid() && Object->TryGetArrayField(Field, Values) && Values->Num() >= 3)
	{
		const float Alpha = Values->Num() >= 4 ? (float)(*Values)[3]->AsNumber() : 1.0f;
		return FLinearColor((float)(*Values)[0]->AsNumber(), (float)(*Values)[1]->AsNumber(), (float)(*Values)[2]->AsNumber(), Alpha);
	}
	return Default;
}

FLinearColor DRData::ToLinear(const FLinearColor& SRGB)
{
	auto Channel = [](float C)
	{
		return C <= 0.04045f ? C / 12.92f : FMath::Pow((C + 0.055f) / 1.055f, 2.4f);
	};
	return FLinearColor(Channel(SRGB.R), Channel(SRGB.G), Channel(SRGB.B), SRGB.A);
}
