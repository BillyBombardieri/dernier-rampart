#include "DRTestArena.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace DRTestArenaLayout
{
	struct FBlock
	{
		const TCHAR* Name;
		FVector Center;
		FVector Size;
		FLinearColor Color;
	};
}

ADRTestArena::ADRTestArena()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Racine"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;

	// Soleil d'après-midi, un peu chaud, qui colore aussi le ciel atmosphérique.
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Soleil"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-35.0, 45.0, 0.0));
	Sun->SetLightColor(FLinearColor(1.0f, 0.93f, 0.84f));
	Sun->SetAtmosphereSunLight(true);

	// Lumière du ciel recalculée en temps réel : les ombres ne sont jamais noires.
	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("LumiereDuCiel"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	// Léger voile au loin, et brouillard volumétrique qui prend la lumière du soleil.
	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Brouillard"));
	Fog->SetupAttachment(Root);
	Fog->SetFogDensity(0.02f);
	Fog->SetVolumetricFog(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BlockMaterial = ShapeMaterial.Object;

	// Positions et tailles en centimètres ; le dessus du sol est à 0. Le mur rouge sert à voir
	// la lumière rebondir (le sol rosit à côté), le toit sur piliers à voir l'ombre douce dessous.
	const FLinearColor Concrete(0.42f, 0.41f, 0.39f);
	const FLinearColor Wood(0.30f, 0.20f, 0.11f);
	const DRTestArenaLayout::FBlock Layout[] = {
		{ TEXT("Sol"), FVector(0.0, 0.0, -50.0), FVector(8000.0, 8000.0, 100.0), FLinearColor(0.27f, 0.24f, 0.20f) },
		{ TEXT("MurBeton"), FVector(0.0, 0.0, 150.0), FVector(1200.0, 30.0, 300.0), Concrete },
		{ TEXT("MurRouge"), FVector(-700.0, 400.0, 150.0), FVector(30.0, 800.0, 300.0), FLinearColor(0.55f, 0.06f, 0.04f) },
		{ TEXT("MuretBas"), FVector(-300.0, -900.0, 40.0), FVector(300.0, 50.0, 80.0), Concrete },
		{ TEXT("Caisse1"), FVector(350.0, -400.0, 50.0), FVector(100.0, 100.0, 100.0), Wood },
		{ TEXT("Caisse2"), FVector(470.0, -380.0, 50.0), FVector(100.0, 100.0, 100.0), Wood },
		{ TEXT("Caisse3"), FVector(410.0, -390.0, 150.0), FVector(100.0, 100.0, 100.0), Wood },
		{ TEXT("Pilier1"), FVector(700.0, 500.0, 200.0), FVector(50.0, 50.0, 400.0), Concrete },
		{ TEXT("Pilier2"), FVector(700.0, 900.0, 200.0), FVector(50.0, 50.0, 400.0), Concrete },
		{ TEXT("Pilier3"), FVector(1100.0, 500.0, 200.0), FVector(50.0, 50.0, 400.0), Concrete },
		{ TEXT("Pilier4"), FVector(1100.0, 900.0, 200.0), FVector(50.0, 50.0, 400.0), Concrete },
		{ TEXT("Toit"), FVector(900.0, 700.0, 410.0), FVector(500.0, 500.0, 20.0), Concrete },
	};
	for (const DRTestArenaLayout::FBlock& Block : Layout)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Block.Name);
		Mesh->SetupAttachment(Root);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetRelativeLocation(Block.Center);
		Mesh->SetRelativeScale3D(Block.Size / 100.0);
		Blocks.Add(Mesh);
		BlockColors.Add(Block.Color);
	}
}

void ADRTestArena::BeginPlay()
{
	Super::BeginPlay();
	for (int32 Index = 0; Index < Blocks.Num(); ++Index)
	{
		if (UMaterialInstanceDynamic* Material = Blocks[Index]->CreateDynamicMaterialInstance(0, BlockMaterial))
		{
			Material->SetVectorParameterValue(TEXT("Color"), BlockColors[Index]);
		}
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::White,
			TEXT("Dernier Rempart (Unreal 5) : terrain d'essai. ZQSD pour bouger, souris pour regarder, Espace pour sauter, Maj pour courir, Échap pour quitter."));
	}
}
