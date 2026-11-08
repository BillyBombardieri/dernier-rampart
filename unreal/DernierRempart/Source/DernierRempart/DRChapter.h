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
 * projecteurs, barils en feu et décor : rochers, arbres morts, carcasses de voitures, et selon le
 * chapitre mares et roseaux, conteneurs et cuves, sapins enneigés, ruines, immeubles éventrés,
 * réverbères, flaques, dunes et poteaux télégraphiques ; enfin l'herbe qui ondule et les cailloux.
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
	/** Barricades du gadget, ajoutées quand on les pose et retirées quand elles disparaissent. */
	void AddBarrier(ADRBarrier* Barrier);
	void RemoveBarrier(ADRBarrier* Barrier);
	/** Ancrages où l'on peut poser une tour. */
	const TArray<TObjectPtr<ADRSocket>>& GetSockets() const { return Sockets; }
	ADRCore* GetCore() const;

	/** Le Cœur, au milieu de la base (Godot : CORE_POS). */
	static FVector CorePosition() { return FVector(-3500.0, 0.0, 0.0); }

	/** L'établi, à gauche du Cœur, plan de travail tourné vers lui (Godot : WORKBENCH_POS). */
	static FVector WorkbenchPosition() { return FVector(-3300.0, -550.0, 0.0); }

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
	void BuildWorkbench();
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
	// Décors propres aux chapitres 2 à 6 (Godot : main.gd, _pool, _reeds... et buildings.gd).
	void BuildPool(const FVector& Position);
	void BuildReeds(const FVector& Position);
	void BuildContainer(const FVector& Position);
	void BuildTank(const FVector& Position);
	void BuildPine(const FVector& Position);
	void BuildRuin(const FVector& Position);
	void BuildBuilding(const FVector& Position);
	void BuildLamp(const FVector& Position);
	void BuildPuddle(const FVector& Position);
	void BuildDune(const FVector& Position);
	void BuildPost(const FVector& Position);
	/** Boîte de collision invisible (une seule pour tout un bâtiment, comme dans Godot). */
	void Solid(const FVector& Center, const FVector& Size, const FRotator& Rotation);
	/** Eau sombre et brillante des mares et des flaques. */
	UMaterialInterface* Water();
	/**
	 * Herbe sèche qui ondule au vent et cailloux semés sur toute la carte, serrés au bord du chemin
	 * (Godot : scripts/ground_cover.gd), loin des ancrages, des barrières et de la base.
	 */
	void BuildCover(const TSharedPtr<FJsonObject>& Data);
	/** Un groupe par carré de 40 m, qui disparaît au loin comme dans Godot. */
	void AddCoverChunks(UStaticMesh* Mesh, UMaterialInterface* Material, const TMap<FIntPoint, TArray<FTransform>>& Cells, bool bShadows);
	bool IsFreeSpot(const FVector& Position, double Margin) const;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	/** Touffe d'herbe et caillou, fabriqués au lancement (BuildCover). */
	UPROPERTY()
	TObjectPtr<UStaticMesh> TuftMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PebbleMesh;

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
	FString Weather; // « snow », « rain », « fireflies »... (vide au chapitre 1)
	bool bMissingTextures = false;
	float Time = 0.0f;
};
