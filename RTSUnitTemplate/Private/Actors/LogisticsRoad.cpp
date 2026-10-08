// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/LogisticsRoad.h"

#include "Characters/Unit/BuildingBase.h"
#include "Components/SplineMeshComponent.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

ALogisticsRoad::ALogisticsRoad()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	bAlwaysRelevant = true;   // a few dozen bytes per road; roads must not pop in and out with relevancy
	SetNetUpdateFrequency(1.f);
	SetCanBeDamaged(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CubeMesh.Succeeded())
	{
		GroundLaneMesh = CubeMesh.Object;
		AirLaneMesh = CubeMesh.Object;
	}
	if (BasicMaterial.Succeeded())
	{
		GroundLaneMaterial = BasicMaterial.Object;
		AirLaneMaterial = BasicMaterial.Object;
	}
}

void ALogisticsRoad::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALogisticsRoad, SourceBase);
	DOREPLIFETIME(ALogisticsRoad, SinkBase);
	DOREPLIFETIME(ALogisticsRoad, TeamId);
	DOREPLIFETIME(ALogisticsRoad, GroundPoints);
	DOREPLIFETIME(ALogisticsRoad, bHasGroundLane);
	DOREPLIFETIME(ALogisticsRoad, AirHeight);
}

void ALogisticsRoad::BeginPlay()
{
	Super::BeginPlay();
	if (GroundPoints.Num() >= 2)
	{
		RebuildVisuals();
	}

	// The local team id replicates on its own schedule (and can change in the lobby / spectator
	// switch), so re-check now and then instead of trusting the value at BeginPlay.
	if (GetNetMode() != NM_DedicatedServer && bOnlyVisibleToOwnTeam)
	{
		FTimerHandle VisibilityHandle;
		GetWorldTimerManager().SetTimer(VisibilityHandle, this, &ALogisticsRoad::UpdateTeamVisibility, 2.f, true, 0.5f);
	}
}

void ALogisticsRoad::InitializeRoad(ABuildingBase* InSource, ABuildingBase* InSink, int32 InTeamId,
	const TArray<FVector>& InGroundPoints, bool bInHasGroundLane, float InAirHeight)
{
	SourceBase = InSource;
	SinkBase = InSink;
	TeamId = InTeamId;
	GroundPoints = InGroundPoints;
	bHasGroundLane = bInHasGroundLane;
	AirHeight = InAirHeight;

	RoadLength = 0.f;
	for (int32 i = 1; i < GroundPoints.Num(); ++i)
	{
		RoadLength += FVector::Dist(GroundPoints[i - 1], GroundPoints[i]);
	}

	if (GroundPoints.Num() >= 2)
	{
		SetActorLocation(GroundPoints[0]);
	}

	RebuildVisuals();
	ForceNetUpdate();
}

FVector ALogisticsRoad::GetSourceEnd() const
{
	return GroundPoints.Num() > 0 ? GroundPoints[0] : GetActorLocation();
}

FVector ALogisticsRoad::GetSinkEnd() const
{
	return GroundPoints.Num() > 0 ? GroundPoints.Last() : GetActorLocation();
}

void ALogisticsRoad::OnRep_RoadGeometry()
{
	RebuildVisuals();
}

void ALogisticsRoad::ClearVisuals()
{
	for (USplineMeshComponent* Segment : SegmentComponents)
	{
		if (IsValid(Segment))
		{
			Segment->DestroyComponent();
		}
	}
	SegmentComponents.Reset();
}

void ALogisticsRoad::AddSegment(const FVector& Start, const FVector& End, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector2D& Scale)
{
	if (!Mesh || FVector::DistSquared(Start, End) < 1.f)
	{
		return;
	}

	USplineMeshComponent* Segment = NewObject<USplineMeshComponent>(this);
	Segment->SetMobility(EComponentMobility::Movable);
	Segment->SetStaticMesh(Mesh);
	if (Material)
	{
		Segment->SetMaterial(0, Material);
	}
	// Pure decoration: nothing may collide with it, trace against it or carve the navmesh.
	Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Segment->SetCanEverAffectNavigation(false);
	Segment->SetCastShadow(false);
	Segment->SetupAttachment(Root);
	Segment->RegisterComponent();

	// Spline meshes work in component space; the root sits at the actor origin without rotation.
	const FVector Origin = GetActorLocation();
	const FVector LocalStart = Start - Origin;
	const FVector LocalEnd = End - Origin;
	const FVector Tangent = LocalEnd - LocalStart;
	Segment->SetStartAndEnd(LocalStart, Tangent, LocalEnd, Tangent, false);
	Segment->SetStartScale(Scale, false);
	Segment->SetEndScale(Scale, true);

	SegmentComponents.Add(Segment);
}

void ALogisticsRoad::AddDashedPolyline(const TArray<FVector>& Points, float Dash, float Gap, UStaticMesh* Mesh, UMaterialInterface* Material, const FVector2D& Scale)
{
	if (Points.Num() < 2)
	{
		return;
	}

	// Solid: one piece per polyline segment.
	if (Gap <= 0.f)
	{
		for (int32 i = 1; i < Points.Num(); ++i)
		{
			AddSegment(Points[i - 1], Points[i], Mesh, Material, Scale);
		}
		return;
	}

	Dash = FMath::Max(10.f, Dash);
	bool bDrawing = true;            // start with a dash at the source end
	float Remaining = Dash;          // length left in the current dash or gap
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		FVector From = Points[i - 1];
		const FVector To = Points[i];
		float SegmentLeft = FVector::Dist(From, To);
		const FVector Dir = (To - From).GetSafeNormal();

		while (SegmentLeft > KINDA_SMALL_NUMBER)
		{
			const float Step = FMath::Min(Remaining, SegmentLeft);
			const FVector Next = From + Dir * Step;
			if (bDrawing)
			{
				// A dash that bends around a corner becomes two pieces - fine at these lengths.
				AddSegment(From, Next, Mesh, Material, Scale);
			}
			From = Next;
			SegmentLeft -= Step;
			Remaining -= Step;
			if (Remaining <= KINDA_SMALL_NUMBER)
			{
				bDrawing = !bDrawing;
				Remaining = bDrawing ? Dash : Gap;
			}
		}
	}
}

void ALogisticsRoad::RebuildVisuals()
{
	ClearVisuals();

	if (GetNetMode() == NM_DedicatedServer || GroundPoints.Num() < 2)
	{
		return;
	}

	UWorld* World = GetWorld();
	FCollisionObjectQueryParams GroundObjects(ECC_WorldStatic);
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(LogisticsRoadGround), false, this);
	if (SourceBase) TraceParams.AddIgnoredActor(SourceBase);
	if (SinkBase) TraceParams.AddIgnoredActor(SinkBase);

	// Navmesh points float a little above or below the real ground and are far apart on open terrain.
	// Cut into short pieces and drop each point onto the ground so the lane follows the surface.
	auto DropToGround = [&](const FVector& P) -> FVector
	{
		FHitResult Hit;
		if (World && World->LineTraceSingleByObjectType(Hit, P + FVector(0, 0, 300.f), P - FVector(0, 0, 600.f), GroundObjects, TraceParams))
		{
			return Hit.Location + FVector(0, 0, GroundLaneZOffset);
		}
		return P + FVector(0, 0, GroundLaneZOffset);
	};

	if (bHasGroundLane)
	{
		// Dense, ground-hugging polyline first; dashes are then laid along it by distance.
		const float StepLength = FMath::Max(50.f, GroundSegmentLength);
		TArray<FVector> Dense;
		Dense.Add(DropToGround(GroundPoints[0]));
		for (int32 i = 1; i < GroundPoints.Num(); ++i)
		{
			const FVector A = GroundPoints[i - 1];
			const FVector B = GroundPoints[i];
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(A, B) / StepLength));
			for (int32 Step = 1; Step <= Steps; ++Step)
			{
				Dense.Add(DropToGround(FMath::Lerp(A, B, float(Step) / float(Steps))));
			}
		}
		AddDashedPolyline(Dense, GroundDashLength, bDashedGroundLane ? GroundGapLength : 0.f,
			GroundLaneMesh, GroundLaneMaterial, GroundLaneScale);
	}

	if (bShowAirLane)
	{
		const FVector Start = GroundPoints[0];
		const FVector End = GroundPoints.Last();
		const float Z = FMath::Max(Start.Z, End.Z) + AirHeight;

		// Dashed like a flight route - and clearly not a second ground road.
		AddDashedPolyline({ FVector(Start.X, Start.Y, Z), FVector(End.X, End.Y, Z) }, AirDashLength, AirGapLength,
			AirLaneMesh, AirLaneMaterial, AirLaneScale);
	}

	UpdateTeamVisibility();
	OnRoadVisualsRebuilt();
}

void ALogisticsRoad::UpdateTeamVisibility()
{
	bool bVisible = true;
	if (bOnlyVisibleToOwnTeam && GetWorld())
	{
		if (const AControllerBase* LocalController = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController()))
		{
			// 0 = spectator (sees everything), -1 = team not replicated yet - show until it is known.
			bVisible = LocalController->SelectableTeamId <= 0 || LocalController->SelectableTeamId == TeamId;
		}
	}
	SetActorHiddenInGame(!bVisible);
}
