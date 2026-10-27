// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/LogisticsRoad.h"

#include "Characters/Unit/BuildingBase.h"
#include "Components/SplineMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "VT/RuntimeVirtualTexture.h"
#include "VT/RuntimeVirtualTextureEnum.h"
#include "System/LogisticsSubsystem.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"
#include "EngineUtils.h"

namespace
{
	/** Pulls each inner point halfway towards the middle of its neighbours in XY (ends and heights kept). */
	void SmoothPolyline2D(TArray<FVector>& Points, int32 Passes, bool bDichtNachsetzen = false)
	{
		if (Points.Num() < 3 || Passes <= 0)
		{
			return;
		}

		// Erst dicht nachsetzen. Der Navmesh-Weg hat nur seine Eckpunkte; auf denen zog die Glaettung
		// eine Ecke fast bis auf die Luftlinie zwischen den Nachbarn - quer durch den Turm, um den
		// der Weg herumfuehrte. Mit dichten Punkten rundet sie nur noch im Umkreis weniger Abstaende.
		// Nur auf Wunsch: am Einmuendungsweg haengen Parallel-Arrays an den Punktindizes.
		if (bDichtNachsetzen)
		{
			constexpr float Abstand = 80.f;
			TArray<FVector> Dicht;
			Dicht.Reserve(Points.Num() * 4);
			Dicht.Add(Points[0]);
			for (int32 i = 1; i < Points.Num(); ++i)
			{
				const FVector A = Points[i - 1];
				const FVector B = Points[i];
				const int32 Teile = FMath::Clamp(FMath::CeilToInt(FVector::Dist2D(A, B) / Abstand), 1, 200);
				for (int32 k = 1; k <= Teile; ++k)
				{
					Dicht.Add(FMath::Lerp(A, B, static_cast<float>(k) / Teile));
				}
			}
			Points = MoveTemp(Dicht);
		}

		const int32 Num = Points.Num();
		TArray<FVector> Next = Points;
		for (int32 Pass = 0; Pass < Passes; ++Pass)
		{
			for (int32 i = 1; i + 1 < Num; ++i)
			{
				const FVector Mid = 0.5f * (Points[i - 1] + Points[i + 1]);
				Next[i].X = 0.5f * (Points[i].X + Mid.X);
				Next[i].Y = 0.5f * (Points[i].Y + Mid.Y);
			}
			Swap(Points, Next);
			Next = Points;
		}
	}
}

namespace RoadReveal
{
	// VisibleFrom / VisibleTo of a piece that is drawn whole.
	static constexpr float WholeFrom = -1.0e6f;
	static constexpr float WholeTo = 1.0e7f;
}

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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		RVTPlaneMesh = PlaneMesh.Object;
	}
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
	DOREPLIFETIME(ALogisticsRoad, bIsSupplyRoad);
	DOREPLIFETIME(ALogisticsRoad, RoadGrowStartTime);
	DOREPLIFETIME(ALogisticsRoad, RoadRetractStartTime);
	DOREPLIFETIME(ALogisticsRoad, RoadRetractEndTime);
	DOREPLIFETIME(ALogisticsRoad, bRoadRetractToSink);
	DOREPLIFETIME(ALogisticsRoad, RoadOrderTime);
}

void ALogisticsRoad::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The roads merged into this one show their own stretches again.
	if (EndPlayReason == EEndPlayReason::Destroyed && bMergeOverlappingRoads && RVTPath.Num() >= 2)
	{
		SolveRoadNetwork(GetWorld(), TeamId, bReplayRoad, this);
	}
	Super::EndPlay(EndPlayReason);
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

	if (RoadOrderTime < 0.0)
	{
		RoadOrderTime = GetRoadServerTime();
	}

	// A new RVT road is laid from the source to the sink - once; a later lane change does not lay it again.
	if (RoadGrowStartTime < 0.0 && !bLayWhole && RoadGrowSpeed > 0.f && bGroundLaneAsRVT && bHasGroundLane)
	{
		RoadGrowStartTime = GetRoadServerTime();
	}

	OnRoadInitialized();
	RebuildVisuals();
	ForceNetUpdate();
}

void ALogisticsRoad::InitializeReplayRoad(int32 InTeamId, const TArray<FVector>& InGroundPoints, bool bInHasGroundLane,
	float InAirHeight, bool bInSupplyRoad, bool bGrow)
{
	bReplayRoad = true;
	bIsSupplyRoad = bInSupplyRoad;
	bLayWhole = !bGrow;
	InitializeRoad(nullptr, nullptr, InTeamId, InGroundPoints, bInHasGroundLane, InAirHeight);
}

void ALogisticsRoad::SetHiddenForReplay(bool bHide)
{
	if (bHiddenForReplay != bHide)
	{
		bHiddenForReplay = bHide;
		UpdateTeamVisibility();
	}
}

void ALogisticsRoad::Retire()
{
	if (RoadRetractStartTime >= 0.0)
	{
		return;
	}
	if (RoadRetractSpeed <= 0.f || !bGroundLaneAsRVT || !bHasGroundLane || GroundPoints.Num() < 2)
	{
		Destroy();
		return;
	}

	// Taken up from the sink back to the source; into the sink when the source is gone. The length is the
	// lane plus the stretches under both buildings (the clients map the time onto their own length).
	auto IsAlive = [](const ABuildingBase* Base) { return IsValid(Base) && Base->GetUnitState() != UnitData::Dead; };
	bRoadRetractToSink = !IsAlive(SourceBase) && IsAlive(SinkBase);
	float Length = RoadLength;
	if (SourceBase) Length += FVector::Dist2D(SourceBase->GetActorLocation(), GroundPoints[0]);
	if (SinkBase) Length += FVector::Dist2D(GroundPoints.Last(), SinkBase->GetActorLocation());
	const double Duration = FMath::Max(0.5, static_cast<double>(Length / RoadRetractSpeed));
	RoadRetractStartTime = GetRoadServerTime();
	RoadRetractEndTime = RoadRetractStartTime + Duration;

	GetWorldTimerManager().SetTimer(RoadRetractTimer, this, &ALogisticsRoad::OnRoadRetractFinished, static_cast<float>(Duration) + 0.2f, false);
	ForceNetUpdate();
	if (bMergeOverlappingRoads)
	{
		// Drawn whole while it is taken up; the roads merged into it show their own stretches again.
		SolveRoadNetwork(GetWorld(), TeamId, bReplayRoad, nullptr);
	}
	UpdateRoadReveal();
}

void ALogisticsRoad::OnRoadRetractFinished()
{
	Destroy();
}

double ALogisticsRoad::GetRoadServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

void ALogisticsRoad::OnRep_RoadReveal()
{
	if (bMergeOverlappingRoads && RoadRetractStartTime >= 0.0 && RVTPath.Num() >= 2)
	{
		// Retiring: drawn whole, the roads merged into it show their own stretches again (redraws only changes).
		SolveRoadNetwork(GetWorld(), TeamId, bReplayRoad, nullptr);
	}
	UpdateRoadReveal();
}

bool ALogisticsRoad::GetRoadRevealRange(float& OutFrom, float& OutTo) const
{
	OutFrom = RoadReveal::WholeFrom;
	OutTo = RoadReveal::WholeTo;
	const float Total = RVTTotalLength;
	const float Soft = FMath::Max(1.f, RoadRevealSoftness);
	const double Now = GetRoadServerTime();

	// How far the road was laid (whole: past its end including the soft edge).
	float Laid = Total + Soft;
	if (RoadGrowStartTime >= 0.0 && RoadGrowSpeed > 0.f)
	{
		const double Until = RoadRetractStartTime >= 0.0 ? FMath::Min(Now, RoadRetractStartTime) : Now;
		Laid = FMath::Min(Laid, static_cast<float>(FMath::Max(0.0, Until - RoadGrowStartTime) * RoadGrowSpeed));
	}

	if (RoadRetractStartTime >= 0.0 && RoadRetractEndTime > RoadRetractStartTime)
	{
		const float Alpha = static_cast<float>(FMath::Clamp((Now - RoadRetractStartTime) / (RoadRetractEndTime - RoadRetractStartTime), 0.0, 1.0));
		OutTo = Laid;
		if (bRoadRetractToSink)
		{
			OutFrom = FMath::Lerp(-Soft, Laid, Alpha);
		}
		else
		{
			OutTo = FMath::Lerp(Laid, -Soft, Alpha);
		}
		return Alpha < 1.f;
	}
	if (Laid < Total + Soft)
	{
		OutTo = Laid;
		return true;
	}
	return false;
}

void ALogisticsRoad::ApplyRoadReveal(int32 Piece, float From, float To)
{
	UInstancedStaticMeshComponent* Writer = RVTComponents.IsValidIndex(Piece) ? RVTComponents[Piece].Get() : nullptr;
	if (!IsValid(Writer) || !RVTPieceRanges.IsValidIndex(Piece))
	{
		return;
	}
	const FVector2f Range = RVTPieceRanges[Piece];
	const float Soft = FMath::Max(1.f, RoadRevealSoftness);
	const bool bShown = Range.Y > From && Range.X < To;

	// A piece wholly inside the visible stretch is drawn whole - it no longer changes with the moving end,
	// so only the piece at the end is redrawn in the virtual texture.
	const FVector3f Reveal(
		Range.X >= From + Soft ? RoadReveal::WholeFrom : From,
		Range.Y <= To - Soft ? RoadReveal::WholeTo : To,
		Soft);
	if (bShown && Reveal != RVTPieceReveal[Piece])
	{
		RVTPieceReveal[Piece] = Reveal;
		const int32 Count = Writer->GetInstanceCount();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Writer->SetCustomDataValue(Index, RVTCustomData::VisibleFrom, Reveal.X);
			Writer->SetCustomDataValue(Index, RVTCustomData::VisibleTo, Reveal.Y);
			Writer->SetCustomDataValue(Index, RVTCustomData::RevealSoftness, Reveal.Z, Index == Count - 1);
		}
	}
	const bool bVisible = bShown && bTeamVisible;
	if (Writer->IsVisible() != bVisible)
	{
		Writer->SetVisibility(bVisible);
	}
}

void ALogisticsRoad::UpdateRoadReveal()
{
	if (GetNetMode() == NM_DedicatedServer || RVTComponents.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(RoadRevealTimer);
		return;
	}
	float From = 0.f;
	float To = 0.f;
	const bool bMoving = GetRoadRevealRange(From, To);
	for (int32 Piece = 0; Piece < RVTComponents.Num(); ++Piece)
	{
		ApplyRoadReveal(Piece, From, To);
	}
	if (!bMoving)
	{
		GetWorldTimerManager().ClearTimer(RoadRevealTimer);
	}
	else if (!GetWorldTimerManager().IsTimerActive(RoadRevealTimer))
	{
		GetWorldTimerManager().SetTimer(RoadRevealTimer, this, &ALogisticsRoad::UpdateRoadReveal, 1.f / FMath::Clamp(RoadRevealRate, 1.f, 60.f), true);
	}
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

	for (UDecalComponent* Decal : DecalComponents)
	{
		if (IsValid(Decal))
		{
			Decal->DestroyComponent();
		}
	}
	DecalComponents.Reset();

	ClearRVTWriters();
	RVTPath.Reset();
	RVTDrawPath.Reset();
	RVTVisible.Reset();
	RVTHalfWidths.Reset();
	RVTPathMaterial = nullptr;
	bRVTDrawn = false;
}

void ALogisticsRoad::AddRVTPolyline(const TArray<FVector>& InPoints, UMaterialInterface* Material, float FadeInLength, float FadeOutLength)
{
	if (InPoints.Num() < 2 || !Material || !TargetVirtualTexture || !RVTPlaneMesh)
	{
		return;
	}

	// Round off the corners (Chaikin, end points kept), so the straight quads do not leave wedge-shaped
	// gaps on the outside of a bend.
	TArray<FVector> Points = InPoints;
	SmoothPolyline2D(Points, RVTSmoothPasses, /*bDichtNachsetzen=*/true);
	// Nach der Glaettung auf dichten Punkten ist der Verlauf schon rund; Chaikin wuerde die Punktzahl
	// nur noch vervierfachen. Ohne Glaettung rundet Chaikin wie bisher.
	const int32 ChaikinPasses = RVTSmoothPasses > 0 ? 0 : 2;
	for (int32 Pass = 0; Pass < ChaikinPasses && Points.Num() > 2; ++Pass)
	{
		TArray<FVector> Next;
		Next.Reserve(Points.Num() * 2);
		Next.Add(Points[0]);
		for (int32 i = 0; i + 1 < Points.Num(); ++i)
		{
			Next.Add(FMath::Lerp(Points[i], Points[i + 1], 0.25f));
			Next.Add(FMath::Lerp(Points[i], Points[i + 1], 0.75f));
		}
		Next.Add(Points.Last());
		Points = MoveTemp(Next);
	}

	RVTPath = MoveTemp(Points);
	RVTPathMaterial = Material;
	RVTFadeInLength = FadeInLength;
	RVTFadeOutLength = FadeOutLength;
	RVTVisible.Reset();
	RVTHalfWidths.Reset();
	bRVTDrawn = false;
	// Merging: RebuildVisuals solves the team's roads (and draws this one) once the path is set.
	if (!bMergeOverlappingRoads)
	{
		DrawRVT();
	}
}

void ALogisticsRoad::ClearRVTWriters()
{
	for (UInstancedStaticMeshComponent* Writer : RVTComponents)
	{
		if (IsValid(Writer))
		{
			Writer->DestroyComponent();
		}
	}
	RVTComponents.Reset();
	RVTPieceRanges.Reset();
	RVTPieceReveal.Reset();
	RVTTotalLength = 0.f;
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoadRevealTimer);
	}
}

void ALogisticsRoad::DrawRVT()
{
	ClearRVTWriters();
	bRVTDrawn = true;

	const TArray<FVector>& Points = RVTDrawPath.Num() == RVTPath.Num() ? RVTDrawPath : RVTPath;
	UMaterialInterface* Material = RVTPathMaterial;
	if (Points.Num() < 2 || !Material || !TargetVirtualTexture || !RVTPlaneMesh)
	{
		return;
	}

	float Total = 0.f;
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		Total += FVector::Dist2D(Points[i - 1], Points[i]);
	}
	if (Total < 1.f)
	{
		return;
	}

	// Merging: only the drawn stretches, each point with its own width.
	const float HalfWidth = RVTRoadWidth * 0.5f;
	const bool bMerged = RVTVisible.Num() == Points.Num() && RVTHalfWidths.Num() == Points.Num();
	auto IsShown = [&](int32 i) { return !bMerged || RVTVisible[i] != 0; };
	auto HalfWidthAt = [&](int32 i) { return bMerged ? RVTHalfWidths[i] : HalfWidth; };

	// Transient and created here (not as a default subobject): same reasoning as the Xeno creep writer - the
	// writer must not be part of the cooked Blueprint template.
	auto NewWriter = [&]() -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* Writer = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Writer->SetMobility(EComponentMobility::Movable);
		Writer->SetStaticMesh(RVTPlaneMesh);
		Writer->SetMaterial(0, Material);
		Writer->SetCastShadow(false);
		Writer->SetReceivesDecals(false);
		Writer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Writer->SetCanEverAffectNavigation(false);
		// Only the virtual texture pass renders it, never the main pass.
		// Never, not Exclusive: Exclusive still draws into the main passes unless an RVT volume in the level
		// hides its primitives. bRenderInMainPass only drops the base pass - the masked (miter-clipped) writer
		// then still wrote depth and showed up as black strips over the ground.
		Writer->VirtualTextureRenderPassType = ERuntimeVirtualTextureMainPassType::Never;
		Writer->bRenderInMainPass = false;
		Writer->bRenderInDepthPass = false;
		Writer->RuntimeVirtualTextures.Add(TargetVirtualTexture);
		// Under the area decals where the road runs on under a building; older roads over younger ones (merging).
		Writer->TranslucencySortPriority = RVTSortPriority - RVTRank;
		Writer->SetupAttachment(Root);
		Writer->SetNumCustomDataFloats(RVTCustomData::Num);
		return Writer;
	};

	// With growth the writer is cut into pieces along the road: every component change redraws its whole
	// bounds in the virtual texture, so a step of the growth only redraws the piece at the moving end.
	const bool bReveal = RoadGrowSpeed > 0.f || RoadRetractSpeed > 0.f;
	const float RevealPieceLength = bReveal ? FMath::Max(200.f, RoadRevealPieceLength) : TNumericLimits<float>::Max();
	RVTTotalLength = Total;
	float RevealFrom = RoadReveal::WholeFrom;
	float RevealTo = RoadReveal::WholeTo;
	GetRoadRevealRange(RevealFrom, RevealTo);

	// All instances go in BEFORE RegisterComponent: the RVT only invalidates the bounds the component has
	// when it registers. Adding them afterwards left just a stub at the first quad painted.
	UInstancedStaticMeshComponent* Writer = nullptr;
	float PieceStartV = 0.f;
	auto FinishWriter = [&](float PieceEndV)
	{
		if (!Writer)
		{
			return;
		}
		RVTComponents.Add(Writer);
		RVTPieceRanges.Add(FVector2f(PieceStartV, PieceEndV));
		RVTPieceReveal.Add(FVector3f(0.f, 0.f, -1.f));
		ApplyRoadReveal(RVTComponents.Num() - 1, RevealFrom, RevealTo);
		Writer->RegisterComponent();
		Writer = nullptr;
	};
	const FTransform RootTransform = Root->GetComponentTransform();

	// One quad per segment of the polyline (long segments split into pieces), mitred at the corners.
	// The writer material gets the road frame of every quad as per-instance custom data (layout in
	// LogisticsRoad.h, RVTCustomData), so it can
	//   - clip each quad at the bisectors of its two corners: neighbours tile exactly, no wedge overlaps,
	//   - work in road space: U = signed distance from the centre line, V = distance along the road,
	//     continuous across corners (V is clamped to the quad's own length, see the material),
	//   - fade the road in and out at its ends (V against TotalLength).
	// The quad is lengthened at both ends by just enough to cover the outer side of the corner.
	struct FSegment
	{
		FVector2D Start;
		FVector2D Dir;
		float Length;
		int32 From;   // index of the start point in Points
	};
	TArray<FSegment> Segments;
	Segments.Reserve(Points.Num());
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		const FVector2D A(Points[i - 1]);
		const FVector2D B(Points[i]);
		const float Length = FVector2D::Distance(A, B);
		if (Length > KINDA_SMALL_NUMBER)
		{
			Segments.Add({ A, (B - A) / Length, Length, i - 1 });
		}
	}
	auto IsDrawn = [&](int32 s) { return IsShown(Segments[s].From) && IsShown(Segments[s].From + 1); };

	// Half the turn at a corner, signed: positive turns towards +Y (right). Clamped so a hairpin cannot
	// blow the miter up to infinity.
	auto HalfTurnTan = [&Segments](int32 From, int32 To) -> float
	{
		const FVector2D& D0 = Segments[From].Dir;
		const FVector2D& D1 = Segments[To].Dir;
		const float Turn = FMath::Atan2(D0.X * D1.Y - D0.Y * D1.X, FVector2D::DotProduct(D0, D1));
		return FMath::Tan(FMath::Clamp(Turn, -FMath::DegreesToRadians(150.f), FMath::DegreesToRadians(150.f)) * 0.5f);
	};

	// Both fades together never longer than the road.
	const float FadeScale = FMath::Min(1.f, Total / FMath::Max(RVTFadeInLength + RVTFadeOutLength, KINDA_SMALL_NUMBER));
	const float FadeIn = FMath::Max(0.f, RVTFadeInLength) * FadeScale;
	const float FadeOut = FMath::Max(0.f, RVTFadeOutLength) * FadeScale;
	const float Step = FMath::Max(10.f, RVTPieceLength);
	float V = 0.f;
	for (int32 s = 0; s < Segments.Num(); ++s)
	{
		const FSegment& Seg = Segments[s];
		if (!IsDrawn(s))
		{
			// Merged into an older road here: V runs on, nothing is drawn.
			V += Seg.Length;
			continue;
		}
		const int32 PieceCount = FMath::Max(1, FMath::CeilToInt(Seg.Length / Step));
		const float PieceLength = Seg.Length / PieceCount;
		// Square cut where the neighbour is not drawn (the junction).
		const float SegmentStartTan = s > 0 && IsDrawn(s - 1) ? HalfTurnTan(s - 1, s) : 0.f;
		const float SegmentEndTan = s + 1 < Segments.Num() && IsDrawn(s + 1) ? HalfTurnTan(s, s + 1) : 0.f;
		const float Z = FMath::Lerp(Points[Seg.From].Z, Points[Seg.From + 1].Z, 0.5f);
		const float HalfWidth0 = HalfWidthAt(Seg.From);
		const float HalfWidth1 = HalfWidthAt(Seg.From + 1);

		for (int32 Piece = 0; Piece < PieceCount; ++Piece)
		{
			const float StartTan = Piece == 0 ? SegmentStartTan : 0.f;
			const float EndTan = Piece == PieceCount - 1 ? SegmentEndTan : 0.f;
			const float PieceHalfWidthStart = FMath::Lerp(HalfWidth0, HalfWidth1, float(Piece) / PieceCount);
			const float PieceHalfWidthEnd = FMath::Lerp(HalfWidth0, HalfWidth1, float(Piece + 1) / PieceCount);
			const float PieceHalfWidth = FMath::Max(PieceHalfWidthStart, PieceHalfWidthEnd);
			const float ExtendStart = PieceHalfWidth * FMath::Abs(StartTan);
			const float ExtendEnd = PieceHalfWidth * FMath::Abs(EndTan);

			if (Writer && V - PieceStartV >= RevealPieceLength)
			{
				FinishWriter(V);
			}
			if (!Writer)
			{
				Writer = NewWriter();
				PieceStartV = V;
			}

			const FVector2D Start = Seg.Start + Seg.Dir * (Piece * PieceLength);
			const FVector2D Centre = Start + Seg.Dir * ((PieceLength + ExtendEnd - ExtendStart) * 0.5f);

			// Plane mesh: 100 x 100 uu, X along the road.
			const FTransform Transform(FVector(Seg.Dir, 0.f).Rotation(), FVector(Centre, Z),
				FVector((PieceLength + ExtendStart + ExtendEnd) / 100.f, PieceHalfWidth * 2.f / 100.f, 1.f));
			const int32 Index = Writer->AddInstance(Transform.GetRelativeTransform(RootTransform), /*bWorldSpace=*/ false);

			float Data[RVTCustomData::Num];
			Data[RVTCustomData::StartX] = Start.X;
			Data[RVTCustomData::StartY] = Start.Y;
			Data[RVTCustomData::DirX] = Seg.Dir.X;
			Data[RVTCustomData::DirY] = Seg.Dir.Y;
			Data[RVTCustomData::Length] = PieceLength;
			Data[RVTCustomData::VStart] = V;
			Data[RVTCustomData::StartMiterTan] = StartTan;
			Data[RVTCustomData::EndMiterTan] = EndTan;
			Data[RVTCustomData::HalfWidth] = HalfWidth;
			Data[RVTCustomData::TotalLength] = Total;
			Data[RVTCustomData::FadeInLength] = FadeIn;
			Data[RVTCustomData::FadeOutLength] = FadeOut;
			Data[RVTCustomData::VisibleFrom] = RoadReveal::WholeFrom;
			Data[RVTCustomData::VisibleTo] = RoadReveal::WholeTo;
			Data[RVTCustomData::RevealSoftness] = 1.f;
			Data[RVTCustomData::HalfWidthStart] = PieceHalfWidthStart;
			Data[RVTCustomData::HalfWidthEnd] = PieceHalfWidthEnd;
			Writer->SetCustomData(Index, Data);

			V += PieceLength;
		}
	}

	FinishWriter(V);
	UpdateRoadReveal();
}

void ALogisticsRoad::SolveRoadNetwork(UWorld* World, int32 InTeamId, bool bInReplayRoads, const ALogisticsRoad* Leaving)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// The team's merging RVT roads with a path. Retiring ones are drawn whole and carry nobody - the others
	// show their own stretches again right away while it is taken up.
	TArray<ALogisticsRoad*> Roads;
	for (TActorIterator<ALogisticsRoad> It(World); It; ++It)
	{
		ALogisticsRoad* Road = *It;
		// Replay roads merge only with each other - a live road still standing under a replay is no part of it.
		if (Road != Leaving && IsValid(Road) && !Road->IsActorBeingDestroyed() && Road->TeamId == InTeamId
			&& Road->bReplayRoad == bInReplayRoads && Road->bMergeOverlappingRoads && Road->bGroundLaneAsRVT && Road->RVTPath.Num() >= 2)
		{
			Roads.Add(Road);
		}
	}
	// Older first; the same order on every machine (replicated times, then the source position).
	Roads.Sort([](const ALogisticsRoad& A, const ALogisticsRoad& B)
	{
		const bool bRetiringA = A.RoadRetractStartTime >= 0.0;
		const bool bRetiringB = B.RoadRetractStartTime >= 0.0;
		if (bRetiringA != bRetiringB) return bRetiringB;
		if (A.RoadOrderTime != B.RoadOrderTime) return A.RoadOrderTime < B.RoadOrderTime;
		const FVector SA = A.GetSourceEnd();
		const FVector SB = B.GetSourceEnd();
		if (SA.X != SB.X) return SA.X < SB.X;
		return SA.Y < SB.Y;
	});

	const int32 Count = Roads.Num();
	TArray<TArray<uint8>> Visible;
	TArray<TArray<float>> Carried;   // roads merged into each point
	TArray<TArray<float>> Along;     // distance along the path per point
	TArray<TArray<int32>> HitRoad;   // per point: the older road it lies on (INDEX_NONE = none) ...
	TArray<TArray<int32>> HitPoint;  // ... its nearest point there ...
	TArray<TArray<float>> HitDist;   // ... the distance of the centre lines ...
	TArray<TArray<FVector2D>> HitPos; // ... and the nearest spot on the older road's centre line
	TArray<TArray<FIntPoint>> Runs;  // merged stretches (first, last point)
	Visible.SetNum(Count);
	Carried.SetNum(Count);
	Along.SetNum(Count);
	HitRoad.SetNum(Count);
	HitPoint.SetNum(Count);
	HitDist.SetNum(Count);
	HitPos.SetNum(Count);
	Runs.SetNum(Count);
	for (int32 k = 0; k < Count; ++k)
	{
		const TArray<FVector>& Path = Roads[k]->RVTPath;
		Visible[k].Init(1, Path.Num());
		Carried[k].Init(0.f, Path.Num());
		HitRoad[k].Init(INDEX_NONE, Path.Num());
		HitPoint[k].Init(INDEX_NONE, Path.Num());
		HitDist[k].Init(0.f, Path.Num());
		HitPos[k].Init(FVector2D::ZeroVector, Path.Num());
		Along[k].SetNumZeroed(Path.Num());
		for (int32 i = 1; i < Path.Num(); ++i)
		{
			Along[k][i] = Along[k][i - 1] + FVector::Dist2D(Path[i - 1], Path[i]);
		}
	}

	for (int32 k = 0; k < Count; ++k)
	{
		ALogisticsRoad* Road = Roads[k];
		if (Road->RoadRetractStartTime >= 0.0)
		{
			continue;
		}
		const TArray<FVector>& Path = Road->RVTPath;
		const float Threshold = Road->MergeOverlapDistance * Road->RVTRoadWidth;

		// Every point: the nearest drawn point of an older road closer than Threshold.
		for (int32 i = 0; i < Path.Num(); ++i)
		{
			const FVector2D P(Path[i]);
			float Best = FMath::Square(Threshold);
			for (int32 j = 0; j < k; ++j)
			{
				const ALogisticsRoad* Other = Roads[j];
				if (Other->RoadRetractStartTime >= 0.0 || Other->bIsSupplyRoad != Road->bIsSupplyRoad)
				{
					continue;
				}
				const TArray<FVector>& OtherPath = Other->RVTPath;
				for (int32 m = 1; m < OtherPath.Num(); ++m)
				{
					if (!Visible[j][m - 1] || !Visible[j][m])
					{
						continue;
					}
					const FVector2D A(OtherPath[m - 1]);
					const FVector2D AB = FVector2D(OtherPath[m]) - A;
					const float LengthSq = AB.SizeSquared();
					const float T = LengthSq > KINDA_SMALL_NUMBER ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / LengthSq, 0.f, 1.f) : 0.f;
					const float DistSq = FVector2D::DistSquared(P, A + AB * T);
					if (DistSq < Best)
					{
						Best = DistSq;
						HitRoad[k][i] = j;
						HitPoint[k][i] = T < 0.5f ? m - 1 : m;
						HitDist[k][i] = FMath::Sqrt(DistSq);
						HitPos[k][i] = A + AB * T;
					}
				}
			}
		}

		// Overlapping runs long enough (not a crossing): not drawn, and the older road carries this one there.
		for (int32 i = 0; i < Path.Num();)
		{
			if (HitRoad[k][i] == INDEX_NONE)
			{
				++i;
				continue;
			}
			const int32 RunStart = i;
			while (i < Path.Num() && HitRoad[k][i] != INDEX_NONE)
			{
				++i;
			}
			const int32 RunEnd = i - 1;
			if (Along[k][RunEnd] - Along[k][RunStart] < Road->MergeMinLength)
			{
				continue;
			}
			Runs[k].Add(FIntPoint(RunStart, RunEnd));
			TMap<int32, FIntPoint> Spans;   // older road -> point range it carries this one over
			for (int32 t = RunStart; t <= RunEnd; ++t)
			{
				Visible[k][t] = 0;
				FIntPoint& Span = Spans.FindOrAdd(HitRoad[k][t], FIntPoint(HitPoint[k][t], HitPoint[k][t]));
				Span.X = FMath::Min(Span.X, HitPoint[k][t]);
				Span.Y = FMath::Max(Span.Y, HitPoint[k][t]);
			}
			for (const TPair<int32, FIntPoint>& Span : Spans)
			{
				for (int32 m = Span.Value.X; m <= Span.Value.Y; ++m)
				{
					Carried[Span.Key][m] += 1.f;
				}
			}
		}
	}

	TArray<TArray<float>> AllHalfWidths;
	AllHalfWidths.SetNum(Count);
	for (int32 k = 0; k < Count; ++k)
	{
		ALogisticsRoad* Road = Roads[k];
		const int32 Num = Road->RVTPath.Num();
		const float Base = Road->RVTRoadWidth * 0.5f;
		const float Blend = FMath::Max(1.f, Road->MergeWidthBlendLength);

		// Width per point, then widened and narrowed smoothly over the blend length (the widest within reach,
		// falling off with the distance).
		TArray<float> Target;
		Target.SetNumUninitialized(Num);
		for (int32 i = 0; i < Num; ++i)
		{
			Target[i] = Base * FMath::Min(Road->MergeMaxWidthScale, 1.f + Road->MergeWidthPerRoad * Carried[k][i]);
		}
		TArray<float> HalfWidths = Target;
		for (int32 i = 0; i < Num; ++i)
		{
			if (Target[i] <= Base)
			{
				continue;
			}
			for (int32 Dir = -1; Dir <= 1; Dir += 2)
			{
				for (int32 j = i + Dir; j >= 0 && j < Num; j += Dir)
				{
					const float Fall = FMath::Abs(Along[k][j] - Along[k][i]) / Blend;
					if (Fall >= 1.f)
					{
						break;
					}
					HalfWidths[j] = FMath::Max(HalfWidths[j], FMath::Lerp(Target[i], Base, FMath::SmoothStep(0.f, 1.f, Fall)));
				}
			}
		}

		AllHalfWidths[k] = MoveTemp(HalfWidths);
	}

	// A merging road does not stop where it meets the older one: it bends onto the older road's centre line
	// over MergeJoinLength (a junction curve) and runs on under it (drawn below, see RVTRank) until the older
	// road covers it completely, so its cut end stays hidden - the same backwards where they part again.
	// Never covered: it stays drawn under the older one, no cut.
	const float CoverMargin = 4.f;
	TArray<TArray<FVector>> DrawPaths;
	DrawPaths.SetNum(Count);
	for (int32 k = 0; k < Count; ++k)
	{
		const TArray<FVector>& Path = Roads[k]->RVTPath;
		DrawPaths[k] = Path;
		TArray<float> Bend;
		Bend.Init(0.f, Path.Num());
		const float Join = FMath::Max(1.f, Roads[k]->MergeJoinLength);
		for (const FIntPoint& Run : Runs[k])
		{
			for (int32 t = Run.X; t <= Run.Y; ++t)
			{
				// Into the older road from where they meet, out of it towards where they part; a run that
				// begins or ends with the path (both under a building) has no curve there.
				const float In = Run.X == 0 ? 1.f : FMath::SmoothStep(0.f, 1.f, (Along[k][t] - Along[k][Run.X]) / Join);
				const float Out = Run.Y == Path.Num() - 1 ? 1.f : FMath::SmoothStep(0.f, 1.f, (Along[k][Run.Y] - Along[k][t]) / Join);
				Bend[t] = FMath::Min(In, Out);
				const FVector2D Bent = FMath::Lerp(FVector2D(Path[t]), HitPos[k][t], Bend[t]);
				DrawPaths[k][t] = FVector(Bent, Path[t].Z);
			}
		}
		// Every point snapped to its own nearest piece of the older road: where that one bends, neighbours
		// land on different pieces - smooth the bent path so the junction curve runs clean.
		if (Runs[k].Num() > 0)
		{
			SmoothPolyline2D(DrawPaths[k], Roads[k]->MergeSmoothPasses);
		}
		auto IsCovered = [&](int32 t)
		{
			const int32 j = HitRoad[k][t];
			return j != INDEX_NONE && HitDist[k][t] * (1.f - Bend[t]) + AllHalfWidths[k][t] + CoverMargin <= AllHalfWidths[j][HitPoint[k][t]];
		};
		for (const FIntPoint& Run : Runs[k])
		{
			int32 First = INDEX_NONE;
			for (int32 t = Run.X; t <= Run.Y && First == INDEX_NONE; ++t)
			{
				if (IsCovered(t)) First = t;
			}
			int32 Last = INDEX_NONE;
			for (int32 t = Run.Y; t >= Run.X && Last == INDEX_NONE; --t)
			{
				if (IsCovered(t)) Last = t;
			}
			if (First == INDEX_NONE)
			{
				First = Run.Y;
				Last = Run.X;
			}
			for (int32 t = Run.X; t <= First; ++t)
			{
				Visible[k][t] = 1;
			}
			for (int32 t = Last; t <= Run.Y; ++t)
			{
				Visible[k][t] = 1;
			}
		}
	}

	for (int32 k = 0; k < Count; ++k)
	{
		ALogisticsRoad* Road = Roads[k];
		const int32 Rank = FMath::Min(k, 50);
		if (!Road->bRVTDrawn || Road->RVTRank != Rank || Road->RVTVisible != Visible[k] || Road->RVTHalfWidths != AllHalfWidths[k]
			|| Road->RVTDrawPath != DrawPaths[k])
		{
			Road->RVTRank = Rank;
			Road->RVTDrawPath = MoveTemp(DrawPaths[k]);
			Road->RVTVisible = Visible[k];
			Road->RVTHalfWidths = MoveTemp(AllHalfWidths[k]);
			Road->DrawRVT();
		}
	}
}

void ALogisticsRoad::AddDecalPolyline(const TArray<FVector>& Points, UMaterialInterface* Material)
{
	if (Points.Num() < 2 || !Material)
	{
		return;
	}

	float Total = 0.f;
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		Total += FVector::Dist(Points[i - 1], Points[i]);
	}
	if (Total < 1.f)
	{
		return;
	}

	// Whole pieces only, stretched a little, so the texture never ends in a cut-off half tile.
	const int32 PieceCount = FMath::Max(1, FMath::RoundToInt(Total / FMath::Max(10.f, GroundDecalPieceLength)));
	const float PieceLength = Total / PieceCount;

	// Walks the polyline by distance (monotonic, so the segment cursor only moves forward).
	int32 Segment = 1;
	float SegmentStart = 0.f;
	auto PointAt = [&](float Distance) -> FVector
	{
		while (Segment < Points.Num() - 1 && SegmentStart + FVector::Dist(Points[Segment - 1], Points[Segment]) < Distance)
		{
			SegmentStart += FVector::Dist(Points[Segment - 1], Points[Segment]);
			++Segment;
		}
		const float Length = FVector::Dist(Points[Segment - 1], Points[Segment]);
		const float Alpha = Length > KINDA_SMALL_NUMBER ? FMath::Clamp((Distance - SegmentStart) / Length, 0.f, 1.f) : 0.f;
		return FMath::Lerp(Points[Segment - 1], Points[Segment], Alpha);
	};

	FVector From = Points[0];
	for (int32 Piece = 1; Piece <= PieceCount; ++Piece)
	{
		const FVector To = Piece == PieceCount ? Points.Last() : PointAt(Piece * PieceLength);
		const FVector Along = To - From;
		if (Along.SizeSquared2D() < 1.f)
		{
			continue;
		}

		UDecalComponent* Decal = NewObject<UDecalComponent>(this);
		Decal->SetDecalMaterial(Material);
		Decal->SetupAttachment(Root);
		Decal->RegisterComponent();
		// Pitch -90: the decal's X (projection) points down, its Z runs along the road and Y across it.
		Decal->SetWorldLocationAndRotation((From + To) * 0.5f, FRotator(-90.f, Along.Rotation().Yaw, 0.f));
		// Half extents. The pieces overlap by ~10% of their length: the material fades its edges (soft
		// blend mask like the building foundation), and the overlapping fades add up to a seamless join.
		Decal->DecalSize = FVector(GroundDecalDepth, GroundDecalWidth * 0.5f, Along.Size() * 0.55f + 1.f);
		Decal->SortOrder = GroundDecalSortOrder;
		Decal->SetFadeScreenSize(0.f);   // stay visible when zoomed out
		Decal->MarkRenderStateDirty();
		DecalComponents.Add(Decal);

		From = To;
	}
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
		if (bGroundLaneAsDecal || bGroundLaneAsRVT)
		{
			// One strip from building edge to building edge (the lane itself stops LogisticsEndMargin short).
			auto EdgePoint = [&](const ABuildingBase* Base, const FVector& LaneEnd) -> FVector
			{
				FVector ToBase = Base->GetActorLocation() - LaneEnd;
				ToBase.Z = 0.f;
				const float Gap = ToBase.Size() - ULogisticsSubsystem::GetBaseRadius(Base);
				return Gap > 1.f ? DropToGround(LaneEnd + ToBase.GetSafeNormal() * Gap) : LaneEnd;
			};
			// RVT: on from the edge to the building's centre, under the building and its area, and fade out
			// there - over at most the stretch under the building, so the fade stays hidden.
			const bool bUnder = bGroundLaneAsRVT && bRVTRunUnderBuildings;
			float FadeIn = 0.f;
			float FadeOut = 0.f;
			auto RunUnder = [&](const ABuildingBase* Base, const FVector& Edge, float& OutFade) -> FVector
			{
				const FVector Centre = DropToGround(FVector(Base->GetActorLocation().X, Base->GetActorLocation().Y, Edge.Z - GroundLaneZOffset));
				OutFade = FMath::Min(RVTEndFadeLength, FVector::Dist2D(Edge, Centre));
				return Centre;
			};
			if (SourceBase)
			{
				Dense.Insert(EdgePoint(SourceBase, Dense[0]), 0);
				if (bUnder)
				{
					Dense.Insert(RunUnder(SourceBase, Dense[0], FadeIn), 0);
				}
			}
			if (SinkBase)
			{
				Dense.Add(EdgePoint(SinkBase, Dense.Last()));
				if (bUnder)
				{
					Dense.Add(RunUnder(SinkBase, Dense.Last(), FadeOut));
				}
			}
			if (bGroundLaneAsRVT)
			{
				AddRVTPolyline(Dense, (bIsSupplyRoad && SupplyRVTRoadMaterial) ? SupplyRVTRoadMaterial.Get() : RVTRoadMaterial.Get(), FadeIn, FadeOut);
			}
			else
			{
				AddDecalPolyline(Dense, (bIsSupplyRoad && SupplyGroundDecalMaterial) ? SupplyGroundDecalMaterial.Get() : GroundDecalMaterial.Get());
			}
		}
		else
		{
			AddDashedPolyline(Dense, GroundDashLength, bDashedGroundLane ? GroundGapLength : 0.f,
				GroundLaneMesh, (bIsSupplyRoad && SupplyGroundLaneMaterial) ? SupplyGroundLaneMaterial.Get() : GroundLaneMaterial.Get(), GroundLaneScale);
		}
	}

	if (bShowAirLane)
	{
		const FVector Start = GroundPoints[0];
		const FVector End = GroundPoints.Last();
		const float Z = FMath::Max(Start.Z, End.Z) + AirHeight;

		// Dashed like a flight route - and clearly not a second ground road.
		AddDashedPolyline({ FVector(Start.X, Start.Y, Z), FVector(End.X, End.Y, Z) }, AirDashLength, AirGapLength,
			AirLaneMesh, (bIsSupplyRoad && SupplyAirLaneMaterial) ? SupplyAirLaneMaterial.Get() : AirLaneMaterial.Get(), AirLaneScale);
	}

	if (bMergeOverlappingRoads)
	{
		// This road against the team's others (draws it, and redraws the ones it changes - also when it has
		// no RVT path any more).
		SolveRoadNetwork(World, TeamId, bReplayRoad, nullptr);
		if (!bRVTDrawn && RVTPath.Num() >= 2)
		{
			DrawRVT();
		}
	}

	UpdateTeamVisibility();
	UpdateRoadReveal();
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
	bVisible = bVisible && !bHiddenForReplay;
	SetActorHiddenInGame(!bVisible);
	bTeamVisible = bVisible;
	if (RVTComponents.Num() > 0)
	{
		float From = 0.f;
		float To = 0.f;
		GetRoadRevealRange(From, To);
		for (int32 Piece = 0; Piece < RVTComponents.Num(); ++Piece)
		{
			ApplyRoadReveal(Piece, From, To);
		}
	}
	// Decals are not primitives - hide them explicitly.
	for (UDecalComponent* Decal : DecalComponents)
	{
		if (IsValid(Decal))
		{
			Decal->SetVisibility(bVisible);
		}
	}
}
