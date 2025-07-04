// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassNavigationSubsystem.h"
#include "MassProcessor.h"
#include "NavAreas/NavArea.h"
#include "DrawDebugHelpers.h"
#include "DynamicObstacleRegProcessor.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UDynamicObstacleRegProcessor : public UMassProcessor
{
	GENERATED_BODY()
	
public:
	UDynamicObstacleRegProcessor();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
	bool Debug = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = RTSUnitTemplate)
    float ExecutionInterval = 0.1f;
	
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
private:
	FMassEntityQuery ObstacleQuery;

	// Helper struct to hold data for static obstacles during processing.
	struct FStaticObstacleDesc
	{
		FVector Location;
		float Radius;
		FMassEntityHandle Entity;
		float BottomZ;
		bool bIsProcessed = false; // Used by the clustering algorithm
	};
    
	/** Gathers static obstacles and immediately processes all dynamic ones. */
	void CollectAndProcessObstacles(FMassExecutionContext& Context, UMassNavigationSubsystem& NavSys, TArray<FStaticObstacleDesc>& OutStaticObstacles); // Removed const
	
	/** Adds a single obstacle to the grid, subdividing if it's a large circle. */
	void AddSingleObstacleToGrid(UMassNavigationSubsystem& NavSys, const FMassEntityHandle Entity, const FVector& Location, const float Radius); // Removed const
	
	//void AddConvexHullClusterToGrid(UMassNavigationSubsystem& NavSys, const TArray<int32>& ClusterIndices, const TArray<FStaticObstacleDesc>& AllStaticObstacles); // Removed const
	UPROPERTY(EditAnywhere, Category = "Navigation")
	bool bForceImmediateNavMeshUpdate = true;
	
	UPROPERTY(EditDefaultsOnly, Category = "Navigation")
	TSubclassOf<UNavArea> NullNavAreaClass;

	/** Keeps track of the volumes we spawn so we can destroy them on the next run. */
	TArray<TWeakObjectPtr<AActor>> SpawnedNavVolumes;
	
	float TimeSinceLastRun = 0.0f;
};
