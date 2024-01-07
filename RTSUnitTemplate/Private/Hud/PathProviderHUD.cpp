// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Hud/PathProviderHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"
#include "Characters/Unit/UnitBase.h"
#include "Algo/Reverse.h"

         
void APathProviderHUD::BeginPlay()
{
	Super::BeginPlay();

	if(StopLoading) return;
	
	TArray<AActor*> NoPathFindingAreasActors;
	
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ANoPathFindingArea::StaticClass(), NoPathFindingAreasActors);

	for(int i = 0; i < NoPathFindingAreasActors.Num(); i++)
	{
		ANoPathFindingArea* NPArea = Cast<ANoPathFindingArea>(NoPathFindingAreasActors[i]);
		if(NPArea)
		{
			FVector Center = NPArea->GetActorLocation();
			if(Debug)DrawDebugCircle(GetWorld(), Center, NPArea->Radius, 44, FColor::Red, false, 50, 0, 1, FVector(0, 1, 0), FVector(1, 0, 0));
			NoPathFindingAreas.Add(NPArea);
		}

	}
}

bool APathProviderHUD::IsLocationInNoPathFindingAreas(FVector Location)
{
	for(int i = 0; i < NoPathFindingAreas.Num(); i++)
	{
		FVector Center = NoPathFindingAreas[i]->GetActorLocation();
		float Distance = FVector::Dist(Center, Location);
		
		if(Distance < NoPathFindingAreas[i]->Radius)
		{
			return true;
		}
	}
	return false;
}

void APathProviderHUD::CreateGridsfromDataTable()
{
	if (GridDataTable)
	{
		int i = 0;
		for(auto it : GridDataTable->GetRowMap())
		{
			FString Key = it.Key.ToString();
			const FGridData* GridData = reinterpret_cast<FGridData*>(it.Value);
			if(GridData)
			{
				FPathMatrix PMatrix = { CreatePathMatrix(GridData->ColCount, GridData->RowCount, GridData->Delta, GridData->Offset) };
				PathMatrixes.Emplace(PMatrix);
			}
			i++;
		}
	}
}

void APathProviderHUD::CreateGridAndDijkstra()
{
	
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADijkstraCenter::StaticClass(), FoundCenterPoints);
	
	CreateGridsfromDataTable();
	int i = 0;
	int z = 0;

	if(FoundCenterPoints.Num() && PathMatrixes.Num() && FoundCenterPoints.Num() == PathMatrixes.Num())
	while( i < FoundCenterPoints.Num() && i < PathMatrixes.Num())
	{
		while(z < PathMatrixes.Num())
		{
			FVector3d ActorLocation = FoundCenterPoints[i]->GetActorLocation();
			FDijkstraMatrix DMatrix = { i+1,Dijkstra(ActorLocation, PathMatrixes[z].Matrix), ActorLocation };
			G_DijkstraPMatrixes.Emplace(DMatrix);
			i++;
			z++;
		}
	}
}

void APathProviderHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	StartTimer = StartTimer + DeltaSeconds;
	if(StartTimer >= StartTime && CreatedGridAndDijkstra == false && !StopLoading)
	{
		CreateGridAndDijkstra();
		CreatedGridAndDijkstra = true;
	}
    	
	//MoveUnitsThroughWayPoints(FriendlyUnits);
	//IsSpeakingUnitClose(FriendlyUnits, SpeakingUnits);
	if(!DisablePathFindingOnEnemy)PatrolUnitsThroughWayPoints(EnemyUnitBases);
	if(!DisablePathFindingOnEnemy)SetNextDijkstra(EnemyUnitBases, DeltaSeconds);

}



void APathProviderHUD::SetNextDijkstra(TArray <AUnitBase*> Units, float DeltaSeconds)
{

	ControlTimer = ControlTimer + DeltaSeconds;
	
	if(ControlTimer > SetNextDijkstraPauseTime)
	{
		for (int32 i = 0; i < Units.Num(); i++) {
			if(Units[i])
			{
				float NextDijkstra = MaxCosts;
				int Id = 0;
				FVector ActorLocation = Units[i]->GetActorLocation();
			
				for(int x = 0; x < G_DijkstraPMatrixes.Num(); x++)
				{
					float Distance = FVector::Dist(G_DijkstraPMatrixes[x].CenterPoint, ActorLocation);
					
					if(Distance < NextDijkstra)
					{
						NextDijkstra = Distance;
						Id = G_DijkstraPMatrixes[x].Id;
					}
				}

				for(int x = 0; x < G_DijkstraPMatrixes.Num(); x++)
					if(Id == G_DijkstraPMatrixes[x].Id) Units[i]->Next_DijkstraPMatrixes = G_DijkstraPMatrixes[x];
			}
		}

		ControlTimer = 0.f;
	}
}

void APathProviderHUD::SetNextDijkstraTo(TArray <AUnitBase*> Units, FVector Location )
{
	
		for (int32 i = 0; i < Units.Num(); i++) {
			if(Units[i])
			{
				float NextDijkstra = MaxCosts;
				int Id = 0;
		
			
				for(int x = 0; x < G_DijkstraPMatrixes.Num(); x++)
				{
					float Distance = FVector::Dist(G_DijkstraPMatrixes[x].CenterPoint, Location);
					
					if(Distance < NextDijkstra)
					{
						NextDijkstra = Distance;
						Id = G_DijkstraPMatrixes[x].Id;
					}
				}

				for(int x = 0; x < G_DijkstraPMatrixes.Num(); x++)
					if(Id == G_DijkstraPMatrixes[x].Id) Units[i]->Next_DijkstraPMatrixes = G_DijkstraPMatrixes[x];
			}
		}

}

void APathProviderHUD::SetDijkstraWithClosestZDistance(AUnitBase* UnitBase, float Z)
{

			float NextDijkstra = MaxCosts;
			int Id = 0;
	
			for(int x = 0; x < G_DijkstraPMatrixes.Num(); x++)
			{
				float Distance = abs(G_DijkstraPMatrixes[x].CenterPoint.Z - Z);
				
				if(Distance < NextDijkstra)
				{
					NextDijkstra = Distance;
					Id = G_DijkstraPMatrixes[x].Id;
				}
			}

			for(int x = 0; x < G_DijkstraPMatrixes.Num(); x++)
				if(Id == G_DijkstraPMatrixes[x].Id) UnitBase->Next_DijkstraPMatrixes = G_DijkstraPMatrixes[x];
	
}

TArray<FPathMatrixRow> APathProviderHUD::CreatePathMatrix(int ColCount, int RowCount, float Delta, FVector Offset)
{
	TArray<FPathMatrixRow> MyPathMatrix;
	TArray<FPathPoint> PathPointsA;
	int NextID = 2;
	
	for (int32 i = 0; i < ColCount; i++) {
		
		float X = i*Delta+Offset.X;
		float Y = 0.f+Offset.Y;
		float Z = Offset.Z;
	
		FPathPoint PathPoint = { NextID, FVector(X,Y,Z) };
		FString ID = FString::FromInt(PathPoint.Id); 
		PathPointsA.Emplace(PathPoint);
		NextID++;
	
	}

	TArray<FPathPoint> PathPointsX = PathPointsA;

		for (int32 j = 0; j < RowCount; j++)
		{
			for (int32 i = 0; i < PathPointsX.Num(); i++)
			{
			float X = PathPointsX[i].Point.X;
			float Y = Delta+j*Delta+Offset.Y;
			float Z = Offset.Z;

			FPathPoint PathPoint = {NextID, FVector(X,Y,Z)};
			FString ID = FString::FromInt(PathPoint.Id);
			PathPointsA.Emplace(PathPoint);
			NextID++;
			}
		}
	

	TArray<FPathPoint> PathPointsB = PathPointsA;
	
	for (int32 i = 0; i < PathPointsB.Num(); i++)
	{
		for (int32 j = 0; j < PathPointsA.Num(); j++)
		{
			float Distance = sqrt((PathPointsA[j].Point.X-PathPointsB[i].Point.X)*(PathPointsA[j].Point.X-PathPointsB[i].Point.X)+(PathPointsA[j].Point.Y-PathPointsB[i].Point.Y)*(PathPointsA[j].Point.Y-PathPointsB[i].Point.Y));
			FPathMatrixRow Row = {PathPointsB[i].Id, PathPointsB[i].Point, PathPointsA[j].Id,  PathPointsA[j].Point, Distance, false};
			FHitResult Hit;

			for(int k = 0; k < AllUnits.Num(); k++)
			{
				QueryParams.AddIgnoredActor(AllUnits[k]);
			}
			
			GetWorld()->LineTraceSingleByChannel(Hit, Row.Point_A, Row.Point_B, TraceChannelProperty, QueryParams);

			if(!Hit.bBlockingHit && Distance > 0.f)
			{
				
				if(Debug) DrawDebugLine(GetWorld(), Row.Point_A, Row.Point_B, FColor::White , false, 10.0f, 0, 1.f);
				MyPathMatrix.Emplace(Row);
			}
		}
	}

	return MyPathMatrix;
}

TArray<FDijkstraRow> APathProviderHUD::Dijkstra(FVector3d CenterPoint, TArray<FPathMatrixRow> PMatrix)
{
	CenterPoint = FVector3d(CenterPoint.X, CenterPoint.Y, CenterPoint.Z);
	float ShortestDistanceToStart = MaxDistance; // This has to be Variable for Infinity Distance;
	int StartID = 1;
	
	//// GET START AND ENDPOINT ///////////////////////////////////////////////////////////////////////////
	GetStartPoint(PMatrix , StartID , ShortestDistanceToStart, CenterPoint);
	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	// Initialisation - In the First Row Point A is EndPoint //////////////////////////////////////////////////////////////////
	TArray<FPathPoint> Queue;
	TArray<FDijkstraRow> DijkstraPMatrix = DijkstraInit(PMatrix, Queue, StartID, CenterPoint);
	//////////////////////////////////////////////////////////////////////////////////////////////
	
	int QIndex = 0;
	TArray<FPathPoint> DoneQues;
	
	StartQue:
	TArray<FPathPoint> NewQues;
		
		while(QIndex < Queue.Num())
		{
			if(!IsPointInsideArray(DoneQues, Queue[QIndex]))
			{
				for(int u = 0; u < PMatrix.Num(); u++)
				{
					if(PMatrix[u].Id_B == Queue[QIndex].Id) 
					{
						for(int k = 0; k < DijkstraPMatrix.Num(); k++)
						{
							if(CheckDijkstraLoop(DijkstraPMatrix, k, PMatrix, u, NewQues, DoneQues, Queue[QIndex]))
							{
								u = 0;	
							}
						}
					}
				}
				if(!IsPointInsideArray(DoneQues, Queue[QIndex]))
					DoneQues.Emplace(Queue[QIndex]);
			}
			QIndex++;
		}
	
		if(NewQues.Num())
		{
			Queue = NewQues;
			QIndex = 0;
			goto StartQue;
		}

		if(Debug)
		for(int k = 0; k < DijkstraPMatrix.Num(); k++)
		{
			if(DijkstraPMatrix[k].Id_Previous != 0)
			{
				DrawDebugLine(GetWorld(), DijkstraPMatrix[k].Previous_Point, DijkstraPMatrix[k].End_Point, FColor::Blue, false, 50.0f, 0, 3.0f);
			}
		}
	
	return DijkstraPMatrix;
}

TArray<FPathPoint> APathProviderHUD::GetPathReUseDijkstra(TArray<FDijkstraRow> DMatrix, FVector3d EndPoint, FVector3d StartPoint)
{
	float ShortestDistanceToEnd = MaxDistance;
	float ShortestDistanceToStart = MaxDistance; // This has to be Variable for Infinity Distance;
	int EndID = 0;
	int StartID = 0;
	GetEndPoint(DMatrix, ShortestDistanceToEnd, EndID, EndPoint);
	GetEndPoint(DMatrix, ShortestDistanceToStart, StartID, StartPoint);
	
	TArray<FPathPoint> PathFromCenter = GetDijkstraPath(DMatrix, EndID);
	TArray<FPathPoint> PathToCenter = GetDijkstraPath(DMatrix, StartID);

	if(Debug)
	for(int x = 0; x < PathToCenter.Num(); x++)
		if(x>0)DrawDebugLine(GetWorld(), PathToCenter[x-1].Point, PathToCenter[x].Point, FColor::Yellow, false, 6.0f, 0, 10.0f);
	
	TArray<FPathPoint> PathThroughCenterA;
	PathThroughCenterA.Append(PathToCenter);
	PathThroughCenterA.Append(PathFromCenter);
	FHitResult Hit;

	TArray<FPathPoint> PathThroughCenterB = PathThroughCenterA;
	TArray<FPathMatrixRow> RealTimePMatrix;


	
	for(int x = 0; x < PathThroughCenterA.Num(); x++)
	{
		for(int i = 0; i < PathThroughCenterB.Num(); i++)
		{
			float Distance = sqrt((PathThroughCenterA[x].Point.X-PathThroughCenterB[i].Point.X)*(PathThroughCenterA[x].Point.X-PathThroughCenterB[i].Point.X)+(PathThroughCenterA[x].Point.Y-PathThroughCenterB[i].Point.Y)*(PathThroughCenterA[x].Point.Y-PathThroughCenterB[i].Point.Y));
			GetWorld()->LineTraceSingleByChannel(Hit, PathThroughCenterA[x].Point, PathThroughCenterB[i].Point, TraceChannelProperty, QueryParams);
			if(!Hit.bBlockingHit && Distance > 0.f)
			{
				FPathMatrixRow Row = {PathThroughCenterA[x].Id, PathThroughCenterA[x].Point, PathThroughCenterB[i].Id,  PathThroughCenterB[i].Point, Distance, false};
				RealTimePMatrix.Emplace(Row);
			}
		}
	}

	TArray<FDijkstraRow> RealTimeDMatrix = Dijkstra(StartPoint, RealTimePMatrix);
	TArray<FPathPoint> RealTimePath = GetDijkstraPath(RealTimeDMatrix, EndID);
	Algo::Reverse(RealTimePath);

	if(Debug)
	for(int i = 0; i < RealTimePath.Num(); i++)
	{
		if(i>0)DrawDebugLine(GetWorld(), RealTimePath[i-1].Point, RealTimePath[i].Point, FColor::Purple, false, 5.0f, 0, 10.0f);
		DrawDebugString(GetWorld(), RealTimePath[i].Point, FString::FromInt(i), 0, FColor::Black, 5, false, 3);
	}
	
	return RealTimePath;
}

bool APathProviderHUD::CheckDijkstraLoop(TArray<FDijkstraRow>& DMatrix, int k, TArray<FPathMatrixRow> PMatrix, int u, TArray<FPathPoint>& NewQue,TArray<FPathPoint> DoneQue, FPathPoint CurrentQue)
{
	FDijkstraRow Row;
	
	if(PMatrix[u].Id_A == DMatrix[k].Id_End && DMatrix[k].Id_Previous != 1) 
	{
							
		FDijkstraRow LastRow = GivePointFromID(DMatrix, PMatrix[u].Id_B);
		
		uint64 NewCosts;

		FPathPoint Que = { PMatrix[u].Id_A, PMatrix[u].Point_A };
		NewCosts = LastRow.Costs + PMatrix[u].Distance;				
			if(NewCosts < DMatrix[k].Costs && PMatrix[u].Id_A != LastRow.Id_Previous)
			{
				Row = {  DMatrix[k].Id_End,  DMatrix[k].End_Point, PMatrix[u].Id_B , PMatrix[u].Point_B ,  NewCosts};
				DMatrix[k] = Row;
				if(!IsPointInsideArray(NewQue, Que) && !IsPointInsideArray(DoneQue, Que))
				{
					NewQue.Emplace(Que); 
					return true;
				}
			}
		
		}
	return false;
}

TArray<FPathPoint> APathProviderHUD::GetDijkstraPath(TArray<FDijkstraRow>& DMatrix, int EndId)
{
	int LastEndId = EndId;
	int z = 0;
	int k = 0;
	TArray<FPathPoint> Path;

	if(DMatrix.Num())
	while(LastEndId != 1 && z <= MaxPathIteration)
	{
		if(DMatrix[k].Id_End == LastEndId && DMatrix[k].Id_Previous != 0)
		{
			LastEndId = DMatrix[k].Id_Previous;
			FPathPoint Point = { DMatrix[k].Id_End, DMatrix[k].End_Point };
			Path.Emplace(Point);
		}
		
		k++;
		if(k >= DMatrix.Num())
		{
			k = 0;
			z++;
		}
	}
	return Path;
}

bool APathProviderHUD::IsPointInsideArray(TArray<FPathPoint> Array, FPathPoint Point)
{
	for(int u = 0; u < Array.Num(); u++)
	{
		if(Array[u].Id == Point.Id) return true;
	}
	return false;
}

bool APathProviderHUD::IsIdInsideMatrix(TArray<FDijkstraRow> Matrix, int Id)
{
	for(int u = 0; u < Matrix.Num(); u++)
	{
		if(Matrix[u].Id_End == Id) return true;
	}
	return false;
}

FDijkstraRow APathProviderHUD::GivePointFromID(TArray<FDijkstraRow> Matrix, int Id)
{
	for(int u = 0; u < Matrix.Num(); u++)
	{
		if(Matrix[u].Id_End == Id) return Matrix[u];
	}
	return {0 };
}

TArray<FDijkstraRow> APathProviderHUD::DijkstraInit(TArray<FPathMatrixRow> PMatrix, TArray<FPathPoint>& Ques, int StartId, FVector3d StartPoint)
{
	TArray<FDijkstraRow> DijkstraPMatrix;
	
	for(int k = 0; k < PMatrix.Num(); k++)
	{
		if(!IsIdInsideMatrix(DijkstraPMatrix, PMatrix[k].Id_A))
		{
			if(PMatrix[k].Id_A == StartId)
			{
				FDijkstraRow Row = { PMatrix[k].Id_A, PMatrix[k].Point_A, 1, StartPoint,  0}; // FVector3d(0.f,0.f,0.f)
				DijkstraPMatrix.Emplace(Row);
				FPathPoint Que = { PMatrix[k].Id_A, PMatrix[k].Point_A };

				TArray<FPathPoint> QuesA = { Que };
				Ques = QuesA;
			}else
			{
				FDijkstraRow Row = { PMatrix[k].Id_A, PMatrix[k].Point_A,  0, FVector3d(0.f,0.f,0.f),  9999999999};
				DijkstraPMatrix.Emplace(Row);
			}
		}
	}

	return DijkstraPMatrix;
}


void APathProviderHUD::GetStartPoint(TArray<FPathMatrixRow> PMatrix ,int& StartId , float& ShortestDistanceToStart, FVector3d StartPoint)
 {
 	for(int k = 0; k < PMatrix.Num(); k++)
 	{
 		const float DistanceToStart = FVector::Dist(StartPoint, PMatrix[k].Point_A);
 		
 		if(DistanceToStart < ShortestDistanceToStart)
 		{
 			ShortestDistanceToStart = DistanceToStart;
 			StartId = PMatrix[k].Id_A;
 		}
 	}
 }

void APathProviderHUD::GetEndPoint(TArray<FDijkstraRow> DMatrix, float& ShortestDistanceToEnd, int& EndId, FVector3d EndPoint)
{
	for(int k = 0; k < DMatrix.Num(); k++)
	{
		const float DistanceToEnd = FVector::Dist(EndPoint, DMatrix[k].End_Point);
		if(DistanceToEnd < ShortestDistanceToEnd && DMatrix[k].Id_Previous != 0)
		{
			ShortestDistanceToEnd = DistanceToEnd;
			EndId = DMatrix[k].Id_End;
		}
	}
}
