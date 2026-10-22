// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NavRelevantComponent.h"
#include "ProceduralPond.generated.h"

class UMaterialParameterCollection;

/** Key for one landscape grass exclusion box (the landscape keeps one box per owner object). */
UCLASS()
class RTSUNITTEMPLATE_API UPondGrassStrip : public UObject
{
	GENERATED_BODY()
};

/** Blocks the navmesh exactly where the pond is: an ellipse (convex polygon), not a box. */
UCLASS(ClassGroup = (Navigation))
class RTSUNITTEMPLATE_API UPondNavComponent : public UNavRelevantComponent
{
	GENERATED_BODY()

public:
	UPondNavComponent();

	virtual void CalcAndCacheBounds() const override;
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;

	/** Pond shape changed: let the navmesh rebuild the tiles under it. */
	void RefreshPond();

	/** Corners of the blocked ellipse. More = closer to the drawn shore, every point costs a little nav build time. */
	static constexpr int32 NumPoints = 24;

	/** How far above and below the pond centre the blocked prism reaches. */
	static constexpr float HalfHeight = 400.f;
};

/**
 * One procedural pond (UProceduralPondComponent spawns them). Draws nothing itself: the landscape material
 * shows the water from the slot in the Material Parameter Collection, this actor fills that slot on every
 * machine and blocks the navmesh with the same ellipse.
 * Ellipse: actor location = centre, actor yaw = rotation, RadiusX / RadiusY = half axes (water edge).
 */
UCLASS(NotPlaceable)
class RTSUNITTEMPLATE_API AProceduralPond : public AActor
{
	GENERATED_BODY()

public:
	AProceduralPond();

	/** Half axis along the actor's X (forward). */
	UPROPERTY(ReplicatedUsing = OnRep_Pond, VisibleAnywhere, BlueprintReadOnly, Category = "Pond")
	float RadiusX = 600.f;

	/** Half axis along the actor's Y (right). */
	UPROPERTY(ReplicatedUsing = OnRep_Pond, VisibleAnywhere, BlueprintReadOnly, Category = "Pond")
	float RadiusY = 600.f;

	/** How far the material lowers the water below the shore. */
	UPROPERTY(ReplicatedUsing = OnRep_Pond, VisibleAnywhere, BlueprintReadOnly, Category = "Pond")
	float Depth = 25.f;

	/** Added to the water radius for the blocked navmesh, so no unit stands with its centre in the water. */
	UPROPERTY(ReplicatedUsing = OnRep_Pond, VisibleAnywhere, BlueprintReadOnly, Category = "Pond")
	float NavPadding = 15.f;

	/** Slot in the collection: Pond<Index> / PondB<Index>. */
	UPROPERTY(ReplicatedUsing = OnRep_Pond, VisibleAnywhere, BlueprintReadOnly, Category = "Pond")
	int32 Index = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Pond, VisibleAnywhere, BlueprintReadOnly, Category = "Pond")
	TObjectPtr<UMaterialParameterCollection> Collection;

	UPROPERTY(VisibleAnywhere, Category = "Pond")
	TObjectPtr<UPondNavComponent> NavComponent;

	/** Most slots the landscape material reads (MPC parameters Pond0..Pond47). A river uses several. */
	static constexpr int32 MaxPonds = 48;

	/** Distance from the centre to the water edge in the given world direction (2D). */
	float EdgeRadiusTowards(const FVector& WorldDirection) const;

	/** True if the 2D point lies inside the ellipse grown by Margin. */
	bool ContainsPoint2D(const FVector& WorldPoint, float Margin = 0.f) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Pond();

private:
	/** No grass, foliage or PCG plants in the water: grass exclusion strips + removing instances (every machine). */
	void ClearVegetation();

	/** Removes foliage / PCG instances and hides small props in the water. Runs again a little later for PCG output that arrives after the pond. */
	void RemoveInstancedPlants();

	void OnPlantTimer();
	FTimerHandle PlantTimer;
	int32 PlantPass = 0;

	/**
	 * PCG graphs regenerate during the match (tracked actors, landscape changes) and bring their plants back.
	 * Every PCG component in the world gets "On Graph Generated" bound to the sweep. Found by reflection, so the
	 * plugin needs no PCG dependency - without the PCG plugin nothing is bound.
	 */
	void BindPCGGenerated();
	void UnbindPCGGenerated();

	UFUNCTION()
	void HandlePCGGraphGenerated(UActorComponent* PCGComponent);

	TArray<TWeakObjectPtr<UActorComponent>> BoundPCGComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPondGrassStrip>> GrassStrips;

	/** Writes the slot into the collection (not on a dedicated server - nobody draws there). */
	void ApplyToCollection(bool bClear);
	void ApplyPond();
};
