#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRChapter.generated.h"

class ADRBarrier;
class ADRCore;
class ADRSocket;
class FJsonObject;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;

/**
 * Carte d'un chapitre, construite au lancement de la partie d'après Content/Data/chapitres.json
 * (exporté de scripts/main.gd dans la version Godot) : ciel, soleil et brouillard, sol, chemin des
 * zombies, brèche du portail, Cœur, barrières et ancrages des tours, murs anti-souffle, sacs de sable,
 * projecteurs, barils en feu et décor (rochers, arbres morts, carcasses de voitures).
 *
 * Les textures viennent de /Game/Importe (script Content/Python/importer_ressources.py). Tant qu'elles
 * ne sont pas importées, la carte s'affiche quand même, en couleurs unies.
 */
UCLASS()
class DERNIERREMPART_API ADRChapter : public AActor
{
	GENERATED_BODY()

public:
	ADRChapter();

	/** Construit le chapitre (0 = La Brèche). À appeler juste après l'apparition de l'acteur. */
	void Build(int32 InChapter);

	virtual void Tick(float DeltaSeconds) override;

	/** Chemin des zombies, du portail au Cœur (centimètres). */
	const TArray<FVector>& GetPath() const { return Path; }

	const TArray<TObjectPtr<ADRBarrier>>& GetBarriers() const { return Barriers; }
	/** Ancrages où l'on peut poser une tour. */
	const TArray<TObjectPtr<ADRSocket>>& GetSockets() const { return Sockets; }
	ADRCore* GetCore() const;

	/** Le Cœur, au milieu de la base (Godot : CORE_POS). */
	static FVector CorePosition() { return FVector(-3500.0, 0.0, 0.0); }

	/** Le joueur commence devant le Cœur, tourné vers le champ de bataille (Godot : (0, 0.2, 27)). */
	static FTransform PlayerStart() { return FTransform(FRotator::ZeroRotator, FVector(-2700.0, 0.0, 110.0)); }

protected:
	virtual void BeginPlay() override;

private:
	template <class T>
	T* NewPart(const TCHAR* InName, EComponentMobility::Type Mobility);
	void FinishPart(UActorComponent* Component);

	/** Matériau texturé (jeu de textures de Godot, répétitions par mètre, teinte sRGB, éclat). */
	UMaterialInterface* Look(const FString& Set, float Scale, const FLinearColor& TintSRGB = FLinearColor::White, float Glow = 0.0f);
	UMaterialInterface* Look(const TSharedPtr<FJsonObject>& Data);
	/** Couleur unie, sans texture (pneus, bandes peintes). */
	UMaterialInterface* Plain(const FLinearColor& ColorSRGB);

	/** Ajoute un exemplaire d'un maillage (les pièces identiques sont regroupées pour aller vite). */
	void Add(UStaticMesh* Mesh, UMaterialInterface* Material, const FTransform& Transform, bool bCollision);
	/** Boîte de taille Size (cm, dans son propre repère), centrée en Center, tournée de Rotation. */
	void Box(UMaterialInterface* Material, const FVector& Center, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator, bool bCollision = true);
	/** Cylindre debout (ou couché selon Rotation) de rayon et de hauteur donnés (cm). */
	void Cylinder(UMaterialInterface* Material, const FVector& Center, double Radius, double Height, const FRotator& Rotation = FRotator::ZeroRotator, bool bCollision = true);
	UPointLightComponent* Flicker(const FVector& Location, const FLinearColor& ColorSRGB, float Candelas, float Radius, float Speed, float Amount, bool bShadows);

	void BuildEnvironment(const TSharedPtr<FJsonObject>& Env);
	void BuildGround(const TSharedPtr<FJsonObject>& Data);
	void BuildPath(const TSharedPtr<FJsonObject>& Data);
	void BuildPortal();
	void BuildCore();
	void BuildRings(const TSharedPtr<FJsonObject>& Data);
	void BuildSocket(const FVector& Position);
	void BuildWalls(const TSharedPtr<FJsonObject>& Data);
	void BuildBlastWall(const FVector& Center, const FVector& Size, FRandomStream& Rng);
	void BuildSandbags(const FVector& Start, int32 Count);
	void BuildLights(const TSharedPtr<FJsonObject>& Data);
	void BuildProps(const TSharedPtr<FJsonObject>& Data);
	void BuildRock(const FVector& Position);
	void BuildDeadTree(const FVector& Position);
	void BuildWreck(const FVector& Position);
	bool IsFreeSpot(const FVector& Position, double Margin) const;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TArray<TObjectPtr<ADRBarrier>> Barriers;

	UPROPERTY()
	TObjectPtr<ADRCore> Core;

	UPROPERTY()
	TArray<TObjectPtr<ADRSocket>> Sockets;

	/** Pièces regroupées par maillage, matériau et collision. */
	UPROPERTY()
	TMap<FString, TObjectPtr<UInstancedStaticMeshComponent>> Groups;

	UPROPERTY()
	TMap<FString, TObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY()
	TArray<TObjectPtr<UPointLightComponent>> FlickerLights;

	TArray<FVector> FlickerSettings; // (intensité, vitesse, amplitude) de chaque lumière qui vacille

	TArray<FVector> Path;
	TArray<FVector> Blocked; // Endroits où ne pas poser de décor.
	FRandomStream Random;
	int32 Chapter = 0;
	FString ChapterName;
	FString ChapterPlace;
	bool bMissingTextures = false;
	float Time = 0.0f;
};
