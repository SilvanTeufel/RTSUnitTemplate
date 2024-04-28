// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Hud/HUDBase.h"
#include "Core/UnitData.h"
#include "Core/DijkstraMatrix.h"
#include "Math/Matrix.h"
#include "Engine/DataTable.h"
#include "Actors/DijkstraCenter.h"
#include "Actors/NoPathFindingArea.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "DrawDebugHelpers.h"
#include "PathProviderHUD.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API APathProviderHUD : public AHUDBase
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category="RTSUnitTemplate")
	TEnumAsByte<ECollisionChannel> TraceChannelProperty = ECC_Pawn;

	FCollisionQueryParams QueryParams;
	
	void BeginPlay() override;
	
	void Tick(float DeltaSeconds);

	
	UPROPERTY(EditAnywhere, Category= RTSUnitTemplate)
	uint64 MaxCosts = 99999.f;

	UPROPERTY(EditAnywhere, Category= RTSUnitTemplate)
	float MaxDistance = 99999.f;

	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	bool Debug = true;

	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	bool StopLoading = true;
	
	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	bool DisablePathFindingOnEnemy = true;

	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	bool DisablePathFindingOnFriendly = true;
	
	UPROPERTY(EditAnywhere, Category = RTSUnitTemplate)
	TArray<FPathMatrix> PathMatrixes;
	
	// Create the Grid for Dijkstra ///////////////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "FoundCenterPoints", Keywords = "RTSUnitTemplate FoundCenterPoints"), Category = RTSUnitTemplate)
	TArray<AActor*> FoundCenterPoints;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	class UDataTable* GridDataTable;

	UPROPERTY(BlueprintReadWrite, Category= RTSUnitTemplate)
	bool CreatedGridAndDijkstra = false;
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateGridAndDijkstra", Keywords = "RTSUnitTemplate CreateGridAndDijkstra"), Category = RTSUnitTemplate)
	void CreateGridAndDijkstra();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateGridsfromDataTable", Keywords = "RTSUnitTemplate CreateGridsfromDataTable"), Category = RTSUnitTemplate)
	void CreateGridsfromDataTable();
	
	UFUNCTION(meta = (DisplayName = "CreatePathMatrix", Keywords = "RTSUnitTemplate CreatePathMatrix"), Category = RTSUnitTemplate)
	TArray<FPathMatrixRow> CreatePathMatrix(int ColCount, int RowCount, float Delta, FVector Offset);
	
	////////////////////////////////////////////////////////////////////////////////////////////////////


	// Calc Dijkstra in Begin_Play ///////////////////////////////////////////////////////////////////////
	
	TArray<FDijkstraMatrix> G_DijkstraPMatrixes;

	UPROPERTY(EditAnywhere, Category= RTSUnitTemplate)
	TArray<FPathPoint> Done;

	UFUNCTION(meta = (DisplayName = "Dijkstra", Keywords = "RTSUnitTemplate Dijkstra"), Category = RTSUnitTemplate)
	TArray<FDijkstraRow> Dijkstra(FVector CenterPoint, TArray<FPathMatrixRow> PMatrix);
	
	UFUNCTION(meta = (DisplayName = "DijkstraInit", Keywords = "RTSUnitTemplate DijkstraInit"), Category = RTSUnitTemplate)
	TArray<FDijkstraRow> DijkstraInit(TArray<FPathMatrixRow> PMatrix, TArray<FPathPoint>& Ques, int StartId, FVector3d StartPoint);

	UFUNCTION(meta = (DisplayName = "CheckDistrajkLoop", Keywords = "RTSUnitTemplate CheckDistrajkLoop"), Category = RTSUnitTemplate)
	bool CheckDijkstraLoop(TArray<FDijkstraRow>& DMatrix, int k, TArray<FPathMatrixRow> PMatrix, int u, TArray<FPathPoint>& NewQue,TArray<FPathPoint> DoneQue, FPathPoint CurrentQue);

	////////////////////////////////////////////////////////////////////////////////////////////////////


	// Handle Dijkstra Matrix ///////////////////////////////////////////////////////////////////////
	
	UPROPERTY(EditAnywhere, Category= RTSUnitTemplate)
	int MaxPathIteration = 5000;
	
	UFUNCTION(meta = (DisplayName = "GetStartPoint", Keywords = "RTSUnitTemplate GetStartPoint"), Category = RTSUnitTemplate)
	void GetStartPoint(TArray<FPathMatrixRow> PMatrix ,int& StartId , float& ShortestDistanceToStart, FVector3d StartPoint);

	UFUNCTION(meta = (DisplayName = "GetEndPoint", Keywords = "RTSUnitTemplate GetEndPoint"), Category = RTSUnitTemplate)
	void GetEndPoint(TArray<FDijkstraRow> DMatrix, float& ShortestDistanceToEnd, int& EndId, FVector3d EndPoint);

	UFUNCTION(meta = (DisplayName = "CreatePathMatrix", Keywords = "RTSUnitTemplate CreatePathMatrix"), Category = RTSUnitTemplate)
	bool IsPointInsideArray(TArray<FPathPoint> Array, FPathPoint Point);

	UFUNCTION(meta = (DisplayName = "IsIdInsideMatrix", Keywords = "RTSUnitTemplate IsIdInsideMatrix"), Category = RTSUnitTemplate)
	bool IsIdInsideMatrix(TArray<FDijkstraRow> Matrix, int Id);

	UFUNCTION(meta = (DisplayName = "GivePointFromID", Keywords = "RTSUnitTemplate GivePointFromID"), Category = RTSUnitTemplate)
	FDijkstraRow GivePointFromID(TArray<FDijkstraRow> Matrix, int Id);

	////////////////////////////////////////////////////////////////////////////////////////////////////


	// On Real-Time ///////////////////////////////////////////////////////////////////////
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "SetNextDijkstraPauseTime", Keywords = "RTSUnitTemplate SetNextDijkstraPauseTime"), Category = RTSUnitTemplate)
	float SetNextDijkstraPauseTime = 3.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "ControlTimer", Keywords = "RTSUnitTemplate ControlTimer"), Category = RTSUnitTemplate)
	float ControlTimer = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "StartTimer", Keywords = "RTSUnitTemplate StartTimer"), Category = RTSUnitTemplate)
	float StartTimer = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "StartTime", Keywords = "RTSUnitTemplate StartTime"), Category = RTSUnitTemplate)
	float StartTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "UseDijkstraOnlyOnFirstUnit", Keywords = "RTSUnitTemplate UseDijkstraOnlyOnFirstUnit"), Category = RTSUnitTemplate)
	bool UseDijkstraOnlyOnFirstUnit = true;
	
	UFUNCTION(meta = (DisplayName = "GetDistrajkPath", Keywords = "RTSUnitTemplate GetDistrajkPath"), Category = RTSUnitTemplate)
	TArray<FPathPoint> GetDijkstraPath(TArray<FDijkstraRow>& DMatrix, int EndId);
	
	UFUNCTION(meta = (DisplayName = "GetPathReUseDijkstra", Keywords = "RTSUnitTemplate GetPathReUseDijkstra"), Category = RTSUnitTemplate)
	TArray<FPathPoint> GetPathReUseDijkstra(TArray<FDijkstraRow> DMatrix, FVector3d EndPoint, FVector3d StartPoint);
	
	////////////////////////////////////////////////////////////////////////////////////////////////////

	// SET Dijkstra on Character ///////////////////////////////////////////////////////////////////////
	
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetNextDijkstra", Keywords = "RTSUnitTemplate SetNextDijkstra"), Category = RTSUnitTemplate)
	void SetNextDijkstra(TArray <AUnitBase*> Units, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetNextDijkstraTo", Keywords = "RTSUnitTemplate SetNextDijkstraTo"), Category = RTSUnitTemplate)
	void SetNextDijkstraTo(TArray <AUnitBase*> Units, FVector Location );

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetDijkstraWithClosestZDistance", Keywords = "RTSUnitTemplate SetDijkstraWithClosestZDistance"), Category = RTSUnitTemplate)
	void SetDijkstraWithClosestZDistance(AUnitBase* UnitBase, float Z);////

	////////////////////////////////////////////////////////////////////////////////////////////////////

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "FoundCenterPoints", Keywords = "RTSUnitTemplate FoundCenterPoints"), Category = RTSUnitTemplate)
	TArray<ANoPathFindingArea*> NoPathFindingAreas;
	
	UPROPERTY(EditAnywhere, Category= RTSUnitTemplate)
	float RangeThreshold = 500.f;

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Location", Keywords = "RTSUnitTemplate Location"), Category = RTSUnitTemplate)
	bool IsLocationInNoPathFindingAreas(FVector Location);
};
