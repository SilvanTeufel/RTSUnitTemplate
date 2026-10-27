// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/LogisticsPipeline.h"

#include "Characters/Unit/BuildingBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RandomStream.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace PipelineParams
{
	static const FName PipeLength(TEXT("PipeLength"));
	static const FName PacketSpeed(TEXT("PacketSpeed"));
	static const FName TubeRadius(TEXT("TubeRadius"));
	static const FName GrowStart(TEXT("GrowStart"));
	static const FName GrowSpeed(TEXT("GrowSpeed"));
	static const FName GrowTail(TEXT("GrowTail"));
	static const FName MaterialSlot(TEXT("Pipe"));

	static FName PacketSlot(int32 Index)
	{
		static const FName Names[ALogisticsPipeline::MaxPacketSlots] = {
			TEXT("Packet0"), TEXT("Packet1"), TEXT("Packet2"), TEXT("Packet3"),
			TEXT("Packet4"), TEXT("Packet5"), TEXT("Packet6"), TEXT("Packet7") };
		return Names[Index];
	}
}

ALogisticsPipeline::ALogisticsPipeline()
{
	// The base class lanes stay off - the tube replaces them.
	bShowAirLane = false;
	bGroundLaneAsRVT = false;
	bGroundLaneAsDecal = false;
}

void ALogisticsPipeline::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALogisticsPipeline, Packets);
	DOREPLIFETIME(ALogisticsPipeline, PacketTravelTime);
	DOREPLIFETIME(ALogisticsPipeline, GrowStartTime);
	DOREPLIFETIME(ALogisticsPipeline, RetractStartTime);
	DOREPLIFETIME(ALogisticsPipeline, RetractEndTime);
	DOREPLIFETIME(ALogisticsPipeline, bRetractToSink);
}

void ALogisticsPipeline::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Gone with packets inside: pulled back into the sink, they are delivered there; otherwise they go back
	// where they came from (a CollectAndStore source credits its team). Lost only when that is gone too.
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed)
	{
		for (const FPipelinePacket& Packet : Packets)
		{
			float Left = Packet.Amount;
			if (bRetractToSink && IsValid(SinkBase))
			{
				if (SinkBase->IsConverter())
				{
					if (SinkBase->bHasConversion && SinkBase->ConversionInput == Packet.Type)
					{
						Left -= SinkBase->ReceiveConversionInput(Packet.Type, Left);
					}
				}
				else if (SinkBase->CreditsOnReceive())
				{
					Left -= SinkBase->ReceiveResource(Packet.Type, Left, TeamId);
				}
			}
			if (Left > 0.f && IsValid(SourceBase))
			{
				SourceBase->ReceiveResource(Packet.Type, Left, TeamId, /*bForce=*/true);
			}
		}
	}
	Packets.Reset();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GrowEndTimer);
		World->GetTimerManager().ClearTimer(RetractEndTimer);
	}
	// The pipelines merged into this one lie on their own again.
	if (EndPlayReason == EEndPlayReason::Destroyed && bMergeNearbyPipes)
	{
		SolvePipeNetwork(GetWorld(), TeamId, bReplayRoad, this);
	}
	ClearWalkCollision();
	Super::EndPlay(EndPlayReason);
}

double ALogisticsPipeline::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

// ---------------------------------------------------------------------------------------------
// Transport (server)
// ---------------------------------------------------------------------------------------------

void ALogisticsPipeline::OnRoadInitialized()
{
	// From building centre to building centre - the same stretch the tube is built along.
	float Length = RoadLength;
	if (GroundPoints.Num() >= 2)
	{
		if (SourceBase)
		{
			Length += FVector::Dist2D(SourceBase->GetActorLocation(), GroundPoints[0]);
		}
		if (SinkBase)
		{
			Length += FVector::Dist2D(GroundPoints.Last(), SinkBase->GetActorLocation());
		}
	}
	PacketTravelTime = FMath::Max(0.5f, Length / FMath::Max(10.f, FlowSpeed));

	// A new pipeline grows from the source to the sink - once; a later lane change does not regrow it.
	if (GrowStartTime < 0.0 && !bLayWhole && GrowSpeedFactor > 0.f)
	{
		GrowStartTime = GetServerTime();
	}
}

void ALogisticsPipeline::Retire()
{
	if (!HasAuthority() || RetractStartTime >= 0.0)
	{
		return;
	}
	if (RetractSpeedFactor <= 0.f)
	{
		Super::Retire();
		return;
	}

	auto IsAlive = [](const ABuildingBase* Base) { return IsValid(Base) && Base->GetUnitState() != UnitData::Dead; };
	bRetractToSink = !IsAlive(SourceBase) && IsAlive(SinkBase);

	// The whole tube with its roots, a little slower than the packets - the ones flowing back stay ahead of
	// the retreating tip, the ones carrying on stay ahead of the retreating tail.
	const double Now = GetServerTime();
	const float Length = FMath::Max(1.f, PacketTravelTime * FMath::Max(10.f, FlowSpeed));
	const float Extra = FMath::Max(RootLengthMin, RootLengthMax) + TubeRadius * 3.f;
	const double Duration = PacketTravelTime * (1.0 + Extra / Length) / FMath::Clamp(RetractSpeedFactor, 0.05f, 1.f);
	RetractStartTime = Now;
	RetractEndTime = Now + Duration;
	// No merge target any more (SolvePipeNetwork): the pipelines lying on this one draw - and grow - on their own.
	if (bMergeNearbyPipes)
	{
		bSolvedAsRetracting = true;
		SolvePipeNetwork(GetWorld(), TeamId, bReplayRoad, nullptr);
	}

	if (!bRetractToSink)
	{
		// Turn around where they are: mirrored start time, the material moves them backwards from it
		// (back at the source at the new SpawnTime).
		for (FPipelinePacket& Packet : Packets)
		{
			Packet.SpawnTime = 2.0 * Now - Packet.SpawnTime;
			Packet.bReturning = true;
		}
	}

	GetWorldTimerManager().SetTimer(RetractEndTimer, this, &ALogisticsPipeline::OnRetractFinished, static_cast<float>(Duration) + 0.2f, false);
	ForceNetUpdate();
	UpdatePacketParameters();
}

void ALogisticsPipeline::OnRetractFinished()
{
	Destroy();
}

bool ALogisticsPipeline::CanLaunchPacket() const
{
	return Packets.Num() < FMath::Clamp(MaxPacketsInFlight, 1, MaxPacketSlots)
		&& GetServerTime() - LastLaunchTime >= PacketInterval;
}

void ALogisticsPipeline::LaunchPacket(EResourceType Type, float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}
	FPipelinePacket& Packet = Packets.AddDefaulted_GetRef();
	Packet.SpawnTime = GetServerTime();
	Packet.Type = Type;
	Packet.Amount = Amount;
	LastLaunchTime = Packet.SpawnTime;

	ForceNetUpdate();
	UpdatePacketParameters();
}

void ALogisticsPipeline::TakeArrivedPackets(TArray<FPipelinePacket>& OutArrived)
{
	const double Now = GetServerTime();
	const int32 Before = Packets.Num();
	for (int32 i = Packets.Num() - 1; i >= 0; --i)
	{
		if (Now - Packets[i].SpawnTime >= PacketTravelTime)
		{
			OutArrived.Add(Packets[i]);
			Packets.RemoveAt(i);
		}
	}
	if (Packets.Num() != Before)
	{
		ForceNetUpdate();
		UpdatePacketParameters();
	}
}

float ALogisticsPipeline::GetInFlightAmount(EResourceType Type) const
{
	float Amount = 0.f;
	for (const FPipelinePacket& Packet : Packets)
	{
		if (Type == EResourceType::MAX || Packet.Type == Type)
		{
			Amount += Packet.Amount;
		}
	}
	return Amount;
}

// ---------------------------------------------------------------------------------------------
// Look (clients / listen server)
// ---------------------------------------------------------------------------------------------

void ALogisticsPipeline::OnRep_Packets()
{
	// Clients: the pipeline started to retract - the ones lying on it lie on their own from now on.
	if (RetractStartTime >= 0.0 && !bSolvedAsRetracting && bMergeNearbyPipes)
	{
		bSolvedAsRetracting = true;
		SolvePipeNetwork(GetWorld(), TeamId, bReplayRoad, nullptr);
	}
	UpdatePacketParameters();
}

void ALogisticsPipeline::UpdatePacketParameters()
{
	if (!PipeMID || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	PipeMID->SetScalarParameterValue(PipelineParams::PipeLength, VisualLength);
	PipeMID->SetScalarParameterValue(PipelineParams::PacketSpeed, VisualLength / FMath::Max(0.1f, PacketTravelTime));
	PipeMID->SetScalarParameterValue(PipelineParams::TubeRadius, TubeRadius);

	// The material runs on the local world time - shift the server's start times onto it once here.
	const double LocalNow = GetWorld()->GetTimeSeconds();
	const double ServerNow = GetServerTime();

	// Growth: a little faster than the packets, so the first packet never overtakes the tip. Already
	// grown (or joined later) = speed 0, the material then skips it.
	const float PacketSpeed = VisualLength / FMath::Max(0.1f, PacketTravelTime);
	const float GrowSpeed = PacketSpeed * GrowSpeedFactor;
	const float GrowTip = FMath::Max(TubeRadius * 3.f, 1.f);
	const double GrowAge = ServerNow - GrowStartTime;
	const double GrowLeft = GrowSpeed > 0.f ? (VisualGrowLength + GrowTip) / GrowSpeed - GrowAge : 0.0;
	const bool bGrowing = GrowStartTime >= 0.0 && GrowLeft > 0.0;
	const bool bRetracting = RetractStartTime >= 0.0 && RetractEndTime > RetractStartTime;
	if (bRetracting)
	{
		// Material: G = smoothstep(((T - GrowStart) * GrowSpeed - X) / GrowTip), X = UV0.x - GrowTail * 2 * UV1.x.
		const double Duration = RetractEndTime - RetractStartTime;
		const double LocalStart = LocalNow - (ServerNow - RetractStartTime);
		float Start = 0.f;
		float Speed = 0.f;
		if (bRetractToSink)
		{
			// The tail runs from the root tips near the source to beyond the sink end.
			const float Span = VisualLength + 2.f * GrowTip - VisualTailMin;
			Speed = Span / static_cast<float>(Duration);
			Start = static_cast<float>(LocalStart - (VisualTailMin - GrowTip) / Speed);
		}
		else
		{
			// From where the tip is now (still growing or not) back to the source.
			float Tip0 = VisualGrowLength + GrowTip;
			if (GrowStartTime >= 0.0 && GrowSpeed > 0.f)
			{
				Tip0 = FMath::Min(Tip0, static_cast<float>((RetractStartTime - GrowStartTime) * GrowSpeed));
			}
			Tip0 = FMath::Max(Tip0, 1.f);
			Speed = Tip0 / static_cast<float>(Duration);
			Start = static_cast<float>(LocalStart + Tip0 / Speed);
		}
		PipeMID->SetScalarParameterValue(PipelineParams::GrowStart, Start);
		PipeMID->SetScalarParameterValue(PipelineParams::GrowSpeed, -Speed);
		PipeMID->SetScalarParameterValue(PipelineParams::GrowTail, bRetractToSink ? 1.f : 0.f);
		GetWorldTimerManager().ClearTimer(GrowEndTimer);
		if (PipeComponent && PipeComponent->ShadowCacheInvalidationBehavior != EShadowCacheInvalidationBehavior::Auto)
		{
			PipeComponent->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Auto;
			PipeComponent->MarkRenderStateDirty();
		}
	}
	else
	{
		PipeMID->SetScalarParameterValue(PipelineParams::GrowStart, bGrowing ? static_cast<float>(LocalNow - GrowAge) : 0.f);
		PipeMID->SetScalarParameterValue(PipelineParams::GrowSpeed, bGrowing ? GrowSpeed : 0.f);
		PipeMID->SetScalarParameterValue(PipelineParams::GrowTail, 0.f);
	}
	if (bGrowing && !bRetracting && PipeComponent && !GetWorldTimerManager().IsTimerActive(GrowEndTimer))
	{
		// The shadow follows the growing tube; with Rigid it would stay at the first frame (no tube).
		PipeComponent->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Auto;
		PipeComponent->MarkRenderStateDirty();
		GetWorldTimerManager().SetTimer(GrowEndTimer, this, &ALogisticsPipeline::OnGrowFinished, static_cast<float>(GrowLeft) + 0.2f, false);
	}
	for (int32 Slot = 0; Slot < MaxPacketSlots; ++Slot)
	{
		FLinearColor Value(0.f, 0.f, 0.f, 0.f);
		if (Packets.IsValidIndex(Slot))
		{
			const FPipelinePacket& Packet = Packets[Slot];
			Value.R = static_cast<float>(LocalNow - (ServerNow - Packet.SpawnTime));
			Value.G = FMath::Clamp(Packet.Amount / FMath::Max(1.f, PacketAmount), 0.3f, 1.5f);
			Value.B = static_cast<float>(static_cast<uint8>(Packet.Type));
			Value.A = Packet.bReturning ? -1.f : 1.f;
		}
		PipeMID->SetVectorParameterValue(PipelineParams::PacketSlot(Slot), Value);
	}
}

void ALogisticsPipeline::OnGrowFinished()
{
	if (PipeComponent && RetractStartTime < 0.0)
	{
		PipeComponent->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Rigid;
		PipeComponent->MarkRenderStateDirty();
	}
	UpdatePacketParameters();
}

void ALogisticsPipeline::ClearVisuals()
{
	GetWorldTimerManager().ClearTimer(GrowEndTimer);
	if (IsValid(PipeComponent))
	{
		PipeComponent->DestroyComponent();
	}
	PipeComponent = nullptr;
	PipeMesh = nullptr;
	PipeMID = nullptr;
	ClearWalkCollision();
	BuiltLine.Reset();
	BuiltScale.Reset();
	VisualLength = 0.f;
	VisualGrowLength = 0.f;
	VisualTailMin = 0.f;

	Super::ClearVisuals();
}

FVector ALogisticsPipeline::TraceGround(const FVector& P) const
{
	UWorld* World = GetWorld();
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(LogisticsPipelineGround), false, this);
	if (SourceBase) TraceParams.AddIgnoredActor(SourceBase);
	if (SinkBase) TraceParams.AddIgnoredActor(SinkBase);

	// Not onto another pipeline's walk capsules - try again past them.
	FHitResult Hit;
	for (int32 Try = 0; Try < 4 && World; ++Try)
	{
		if (!World->LineTraceSingleByObjectType(Hit, P + FVector(0, 0, 300.f), P - FVector(0, 0, 600.f), FCollisionObjectQueryParams(ECC_WorldStatic), TraceParams))
		{
			break;
		}
		if (ALogisticsRoad* Other = Cast<ALogisticsRoad>(Hit.GetActor()))
		{
			TraceParams.AddIgnoredActor(Other);
			continue;
		}
		return Hit.Location;
	}
	return P;
}

void ALogisticsPipeline::BuildCenterLine(TArray<FVector>& OutPoints) const
{
	auto Ground = [this](const FVector& P) { return TraceGround(P); };

	// Lane cut into short pieces, each point on the ground.
	const float StepLength = FMath::Max(50.f, GroundSegmentLength);
	TArray<FVector> Points;
	Points.Add(Ground(GroundPoints[0]) + FVector(0, 0, CenterHeight));
	for (int32 i = 1; i < GroundPoints.Num(); ++i)
	{
		const FVector A = GroundPoints[i - 1];
		const FVector B = GroundPoints[i];
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(A, B) / StepLength));
		for (int32 Step = 1; Step <= Steps; ++Step)
		{
			Points.Add(Ground(FMath::Lerp(A, B, float(Step) / float(Steps))) + FVector(0, 0, CenterHeight));
		}
	}

	// To the right of the flow (see LaneOffset). The building ends are not shifted: both tubes meet there.
	if (!FMath::IsNearlyZero(LaneOffset) && Points.Num() >= 2)
	{
		TArray<FVector> Shifted = Points;
		for (int32 i = 0; i < Points.Num(); ++i)
		{
			const FVector Forward = (Points[FMath::Min(i + 1, Points.Num() - 1)] - Points[FMath::Max(i - 1, 0)]).GetSafeNormal2D();
			Shifted[i] += FVector::CrossProduct(FVector::UpVector, Forward) * LaneOffset;
		}
		Points = MoveTemp(Shifted);
	}

	// On to the building centres, diving into the ground there.
	auto Centre = [&](const ABuildingBase* Base, const FVector& Near) -> FVector
	{
		const FVector Location = Base->GetActorLocation();
		return Ground(FVector(Location.X, Location.Y, Near.Z)) - FVector(0, 0, EndSinkDepth);
	};
	if (SourceBase)
	{
		Points.Insert(Centre(SourceBase, Points[0]), 0);
	}
	if (SinkBase)
	{
		Points.Add(Centre(SinkBase, Points.Last()));
	}

	// Round off the corners (Chaikin, ends kept).
	for (int32 Pass = 0; Pass < 3 && Points.Num() > 2; ++Pass)
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

	// Even rings: resample by distance.
	float Total = 0.f;
	for (int32 i = 1; i < Points.Num(); ++i)
	{
		Total += FVector::Dist(Points[i - 1], Points[i]);
	}
	const int32 Rings = FMath::Max(2, FMath::CeilToInt(Total / FMath::Max(5.f, RingSpacing)) + 1);
	const float Spacing = Total / (Rings - 1);
	OutPoints.Reset(Rings);
	OutPoints.Add(Points[0]);
	int32 Segment = 1;
	float SegmentStart = 0.f;
	for (int32 Ring = 1; Ring < Rings - 1; ++Ring)
	{
		const float Target = Ring * Spacing;
		while (Segment < Points.Num() - 1 && SegmentStart + FVector::Dist(Points[Segment - 1], Points[Segment]) < Target)
		{
			SegmentStart += FVector::Dist(Points[Segment - 1], Points[Segment]);
			++Segment;
		}
		const float SegmentLength = FMath::Max(KINDA_SMALL_NUMBER, FVector::Dist(Points[Segment - 1], Points[Segment]));
		OutPoints.Add(FMath::Lerp(Points[Segment - 1], Points[Segment], FMath::Clamp((Target - SegmentStart) / SegmentLength, 0.f, 1.f)));
	}
	OutPoints.Add(Points.Last());
}

void ALogisticsPipeline::SolvePipeNetwork(UWorld* World, int32 InTeamId, bool bInReplayRoads, const ALogisticsPipeline* Leaving)
{
	if (!World)
	{
		return;
	}
	TArray<ALogisticsPipeline*> Pipes;
	for (TActorIterator<ALogisticsPipeline> It(World); It; ++It)
	{
		ALogisticsPipeline* Pipe = *It;
		if (IsValid(Pipe) && Pipe != Leaving && Pipe->TeamId == InTeamId && Pipe->bReplayRoad == bInReplayRoads && !Pipe->IsActorBeingDestroyed())
		{
			if (Pipe->RawLine.Num() < 2 && Pipe->GroundPoints.Num() >= 2)
			{
				Pipe->BuildCenterLine(Pipe->RawLine);
			}
			Pipes.Add(Pipe);
		}
	}
	// Oldest first - the same order on every machine (RoadOrderTime replicates).
	Pipes.Sort([](const ALogisticsPipeline& A, const ALogisticsPipeline& B)
	{
		if (A.RoadOrderTime != B.RoadOrderTime) return A.RoadOrderTime < B.RoadOrderTime;
		const FVector PA = A.GroundPoints.Num() ? A.GroundPoints[0] : FVector::ZeroVector;
		const FVector PB = B.GroundPoints.Num() ? B.GroundPoints[0] : FVector::ZeroVector;
		return PA.X != PB.X ? PA.X < PB.X : PA.Y < PB.Y;
	});

	const int32 N = Pipes.Num();
	TArray<TArray<FVector>> Lines;
	TArray<TArray<float>> Wrap;        // per ring: 0..1 how far it lies on an older pipeline
	TArray<TArray<int32>> WrapPipe;    // ... on which one (index into Pipes)
	TArray<TArray<int32>> WrapRing;    // ... at which of its rings
	Lines.SetNum(N); Wrap.SetNum(N); WrapPipe.SetNum(N); WrapRing.SetNum(N);
	for (int32 k = 0; k < N; ++k)
	{
		Lines[k] = Pipes[k]->RawLine;
		Wrap[k].Init(0.f, Lines[k].Num());
		WrapPipe[k].Init(INDEX_NONE, Lines[k].Num());
		WrapRing[k].Init(INDEX_NONE, Lines[k].Num());
	}

	// Nearest point on pipeline j's (final) centre line to P: distance, ring and position.
	auto Nearest = [&](int32 j, const FVector& P, int32& OutRing, FVector& OutPos) -> float
	{
		float Best = TNumericLimits<float>::Max();
		const TArray<FVector>& L = Lines[j];
		for (int32 s = 0; s + 1 < L.Num(); ++s)
		{
			const FVector Q = FMath::ClosestPointOnSegment(P, L[s], L[s + 1]);
			const float D = FVector::DistSquared(P, Q);
			if (D < Best)
			{
				Best = D;
				OutPos = Q;
				OutRing = FVector::DistSquared(Q, L[s]) < FVector::DistSquared(Q, L[s + 1]) ? s : s + 1;
			}
		}
		return FMath::Sqrt(Best);
	};

	// Every younger pipeline onto the older ones it runs along.
	for (int32 k = 1; k < N; ++k)
	{
		const ALogisticsPipeline* Pipe = Pipes[k];
		const TArray<FVector>& Raw = Pipe->RawLine;
		if (!Pipe->bMergeNearbyPipes || Raw.Num() < 2)
		{
			continue;
		}
		const int32 Count = Raw.Num();
		TArray<float> Along;
		Along.SetNumZeroed(Count);
		for (int32 i = 1; i < Count; ++i)
		{
			Along[i] = Along[i - 1] + FVector::Dist(Raw[i - 1], Raw[i]);
		}

		// Every ring near an older pipeline (the nearest one).
		TArray<int32> HitPipe;
		HitPipe.Init(INDEX_NONE, Count);
		for (int32 i = 0; i < Count; ++i)
		{
			float BestDist = Pipe->PipeMergeDistance;
			for (int32 j = 0; j < k; ++j)
			{
				// A retracting pipeline is no merge target (like the roads): the reroute that replaces it runs
				// along it almost everywhere - merged onto it, the new one would stay hidden while it grows and
				// only pop in whole once the old one is gone.
				if (!Pipes[j]->bMergeNearbyPipes || Lines[j].Num() < 2 || Pipes[j]->RetractStartTime >= 0.0)
				{
					continue;
				}
				int32 Ring = INDEX_NONE;
				FVector Pos;
				const float D = Nearest(j, Raw[i], Ring, Pos);
				if (D < BestDist)
				{
					BestDist = D;
					HitPipe[i] = j;
				}
			}
		}

		// Runs along the same pipeline at least PipeMergeMinLength long - a crossing is not one. The pipeline
		// bends onto it over PipeMergeJoinLength before and after.
		TArray<float> Weight;
		Weight.Init(0.f, Count);
		TArray<int32> RunPipe;
		RunPipe.Init(INDEX_NONE, Count);
		const float Join = FMath::Max(1.f, Pipe->PipeMergeJoinLength);
		for (int32 i = 0; i < Count;)
		{
			if (HitPipe[i] == INDEX_NONE)
			{
				++i;
				continue;
			}
			int32 End = i;
			while (End + 1 < Count && HitPipe[End + 1] == HitPipe[i])
			{
				++End;
			}
			if (Along[End] - Along[i] >= Pipe->PipeMergeMinLength)
			{
				for (int32 r = 0; r < Count; ++r)
				{
					const float Outside = r < i ? Along[i] - Along[r] : r > End ? Along[r] - Along[End] : 0.f;
					const float W = 1.f - FMath::SmoothStep(0.f, Join, Outside);
					if (W > Weight[r])
					{
						Weight[r] = W;
						RunPipe[r] = HitPipe[i];
					}
				}
			}
			i = End + 1;
		}

		for (int32 i = 0; i < Count; ++i)
		{
			const int32 j = RunPipe[i];
			if (j == INDEX_NONE || Weight[i] <= 0.f)
			{
				continue;
			}
			int32 Ring = INDEX_NONE;
			FVector Pos = Raw[i];
			Nearest(j, Raw[i], Ring, Pos);
			Lines[k][i] = FMath::Lerp(Raw[i], Pos, Weight[i]);
			Wrap[k][i] = Weight[i];
			WrapPipe[k][i] = j;
			WrapRing[k][i] = Ring;
		}
	}

	// Thickness: the younger pipelines a pipeline carries (per ring, smoothed along it). Where a pipeline lies
	// on an older one it shrinks to just inside that one, so its bulges never poke through and its open ends
	// are hidden; the fully merged part is not drawn at all (BuildTube).
	TArray<TArray<float>> Scale;
	Scale.SetNum(N);
	for (int32 k = 0; k < N; ++k)
	{
		const ALogisticsPipeline* Pipe = Pipes[k];
		const int32 Count = Lines[k].Num();
		TArray<float> Num;
		Num.Init(0.f, Count);
		for (int32 y = k + 1; y < N; ++y)
		{
			TArray<float> Mark;
			Mark.Init(0.f, Count);
			for (int32 i = 0; i < Wrap[y].Num(); ++i)
			{
				if (WrapPipe[y][i] == k && Mark.IsValidIndex(WrapRing[y][i]))
				{
					Mark[WrapRing[y][i]] = FMath::Max(Mark[WrapRing[y][i]], Wrap[y][i]);
				}
			}
			for (int32 r = 0; r < Count; ++r)
			{
				Num[r] += Mark[r];
			}
		}
		// Close the gaps between marked rings and soften the ends.
		for (int32 Pass = 0; Pass < 6; ++Pass)
		{
			TArray<float> Next = Num;
			for (int32 r = 1; r + 1 < Count; ++r)
			{
				Next[r] = FMath::Max(Num[r], 0.5f * (Num[r - 1] + Num[r + 1]));
			}
			Num = MoveTemp(Next);
		}
		Scale[k].SetNum(Count);
		for (int32 r = 0; r < Count; ++r)
		{
			Scale[k][r] = FMath::Min(Pipe->PipeMergeMaxWidthScale, 1.f + Pipe->PipeMergeWidthPerPipe * Num[r]);
		}
	}
	for (int32 k = 0; k < N; ++k)
	{
		for (int32 i = 0; i < Scale[k].Num(); ++i)
		{
			const int32 j = WrapPipe[k][i];
			if (j != INDEX_NONE && Scale[j].IsValidIndex(WrapRing[k][i]))
			{
				const float Inner = Scale[j][WrapRing[k][i]] * Pipes[j]->TubeRadius / FMath::Max(1.f, Pipes[k]->TubeRadius) * 0.8f;
				Scale[k][i] = FMath::Lerp(Scale[k][i], Inner, Wrap[k][i]);
			}
		}
	}

	// Rebuild what changed.
	for (int32 k = 0; k < N; ++k)
	{
		ALogisticsPipeline* Pipe = Pipes[k];
		bool bSame = (Pipe->PipeComponent || Pipe->GetNetMode() == NM_DedicatedServer)
			&& Pipe->BuiltLine.Num() == Lines[k].Num() && Pipe->BuiltScale == Scale[k];
		for (int32 i = 0; bSame && i < Lines[k].Num(); ++i)
		{
			bSame = Pipe->BuiltLine[i].Equals(Lines[k][i], 0.5f);
		}
		if (!bSame)
		{
			TArray<uint8> Merged;
			Merged.SetNum(Lines[k].Num());
			for (int32 i = 0; i < Merged.Num(); ++i)
			{
				Merged[i] = Wrap[k][i] >= 0.97f ? 2 : Wrap[k][i] > 0.5f ? 1 : 0;
			}
			Pipe->BuildTube(Lines[k], Scale[k], Merged);
		}
	}
}

void ALogisticsPipeline::ClearWalkCollision()
{
	for (UCapsuleComponent* Capsule : WalkCollision)
	{
		if (IsValid(Capsule))
		{
			Capsule->DestroyComponent();
		}
	}
	WalkCollision.Reset();
}

void ALogisticsPipeline::BuildWalkCollision(const TArray<FVector>& Line, const TArray<float>& RadiusScale)
{
	ClearWalkCollision();
	if (!bWalkableByGroundUnits || Line.Num() < 2)
	{
		return;
	}
	// Not inside the buildings (the ends dive into their centres).
	const float SourceClear = SourceBase && GroundPoints.Num() ? FVector::Dist2D(GroundPoints[0], SourceBase->GetActorLocation()) : 0.f;
	const float SinkClear = SinkBase && GroundPoints.Num() ? FVector::Dist2D(GroundPoints.Last(), SinkBase->GetActorLocation()) : 0.f;
	auto InOpen = [&](const FVector& P)
	{
		return (!SourceBase || FVector::Dist2D(P, SourceBase->GetActorLocation()) > SourceClear)
			&& (!SinkBase || FVector::Dist2D(P, SinkBase->GetActorLocation()) > SinkClear);
	};

	auto AddCapsule = [&](const FVector& A, const FVector& B, float Radius)
	{
		const FVector Dir = B - A;
		const float Length = Dir.Size();
		if (Length < 1.f)
		{
			return;
		}
		UCapsuleComponent* Capsule = NewObject<UCapsuleComponent>(this, NAME_None, RF_Transient);
		Capsule->SetMobility(EComponentMobility::Movable);
		Capsule->SetUsingAbsoluteLocation(true);
		Capsule->SetUsingAbsoluteRotation(true);
		Capsule->SetUsingAbsoluteScale(true);
		Capsule->InitCapsuleSize(Radius, 0.5f * Length + Radius);
		// Only the units' ground trace (a WorldStatic object query) sees it - no trace channel, no navigation.
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Capsule->SetCollisionObjectType(ECC_WorldStatic);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		Capsule->SetCanEverAffectNavigation(false);
		Capsule->SetGenerateOverlapEvents(false);
		Capsule->SetHiddenInGame(true);
		Capsule->SetupAttachment(GetRootComponent());
		Capsule->SetWorldLocationAndRotation(0.5f * (A + B), FRotationMatrix::MakeFromZ(Dir / Length).Rotator());
		Capsule->RegisterComponent();
		WalkCollision.Add(Capsule);
	};

	// Pieces of about WalkCollisionSegmentLength along the line, each as thick as the tube there.
	const float SegmentLength = FMath::Max(50.f, WalkCollisionSegmentLength);
	int32 Start = INDEX_NONE;
	float Run = 0.f;
	float RadiusMax = 0.f;
	for (int32 i = 0; i < Line.Num(); ++i)
	{
		if (Start == INDEX_NONE)
		{
			if (!InOpen(Line[i]))
			{
				continue;
			}
			Start = i;
			Run = 0.f;
			RadiusMax = 0.f;
		}
		RadiusMax = FMath::Max(RadiusMax, TubeRadius * (RadiusScale.IsValidIndex(i) ? RadiusScale[i] : 1.f));
		if (i > Start)
		{
			Run += FVector::Dist(Line[i - 1], Line[i]);
		}
		const bool bLast = i == Line.Num() - 1 || !InOpen(Line[i + 1]);
		if (i > Start && (Run >= SegmentLength || bLast))
		{
			AddCapsule(Line[Start], Line[i], RadiusMax);
			Start = bLast ? INDEX_NONE : i;
			Run = 0.f;
			RadiusMax = TubeRadius * (RadiusScale.IsValidIndex(i) ? RadiusScale[i] : 1.f);
		}
		else if (bLast)
		{
			Start = INDEX_NONE;
		}
	}
}

void ALogisticsPipeline::RebuildVisuals()
{
	RawLine.Reset();
	if (GroundPoints.Num() >= 2)
	{
		BuildCenterLine(RawLine);
	}
	if (RawLine.Num() < 2)
	{
		ClearVisuals();
	}
	// This one and every other of the team (merging), each rebuilt only if it changed.
	SolvePipeNetwork(GetWorld(), TeamId, bReplayRoad, nullptr);
}

void ALogisticsPipeline::BuildTube(const TArray<FVector>& Line, const TArray<float>& RadiusScale, const TArray<uint8>& Merged)
{
	ClearVisuals();
	if (Line.Num() < 2)
	{
		return;
	}
	BuiltLine = Line;
	BuiltScale = RadiusScale;
	BuildWalkCollision(Line, RadiusScale);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// One mesh for the tube and its roots (one draw call). Every tube is rings of vertices; the seam is
	// doubled for the UVs. UV layout: see the class comment.
	const FVector Origin = GetActorLocation();

	FMeshDescription Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();
	TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
	TPolygonGroupAttributesRef<FName> SlotNames = Attributes.GetPolygonGroupMaterialSlotNames();
	UVs.SetNumChannels(4);

	const FPolygonGroupID Group = Description.CreatePolygonGroup();
	SlotNames[Group] = PipelineParams::MaterialSlot;

	struct FTubeRing
	{
		float Radius = 0.f;
		float OwnDistance = 0.f;   // UV0.x
		FVector2f Main;            // UV1
		FVector2f MainSide;        // UV2
	};

	auto AddTube = [&](const TArray<FVector>& Points, const TArray<FTubeRing>& RingData, int32 Sides)
	{
		const int32 Count = Points.Num();
		TArray<FVertexInstanceID> Instances;
		Instances.Reserve(Count * (Sides + 1));
		FVector Side = FVector::RightVector;
		for (int32 Ring = 0; Ring < Count; ++Ring)
		{
			const FVector Forward = (Points[FMath::Min(Ring + 1, Count - 1)] - Points[FMath::Max(Ring - 1, 0)]).GetSafeNormal();
			const FVector NewSide = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
			if (!NewSide.IsNearlyZero())
			{
				Side = NewSide;   // a vertical piece keeps the last frame instead of spinning
			}
			const FVector Up = FVector::CrossProduct(Forward, Side).GetSafeNormal();
			const FTubeRing& Data = RingData[Ring];

			for (int32 i = 0; i <= Sides; ++i)
			{
				const float Angle = 2.f * PI * float(i) / float(Sides);
				const FVector Radial = Side * FMath::Cos(Angle) + Up * FMath::Sin(Angle);
				const FVertexID Vertex = Description.CreateVertex();
				Positions[Vertex] = FVector3f(Points[Ring] + Radial * Data.Radius - Origin);

				const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
				Normals[Instance] = FVector3f(Radial);
				Tangents[Instance] = FVector3f(Forward);
				// Bitangent along +V (around the tube).
				const FVector Around = -Side * FMath::Sin(Angle) + Up * FMath::Cos(Angle);
				BinormalSigns[Instance] = FVector::DotProduct(FVector::CrossProduct(Radial, Forward), Around) >= 0.f ? 1.f : -1.f;
				UVs.Set(Instance, 0, FVector2f(Data.OwnDistance, float(i) / float(Sides)));
				UVs.Set(Instance, 1, Data.Main);
				UVs.Set(Instance, 2, Data.MainSide);
				UVs.Set(Instance, 3, FVector2f(Data.Radius, 0.f));
				Instances.Add(Instance);
			}
			VisualGrowLength = FMath::Max(VisualGrowLength, Data.OwnDistance);
			VisualTailMin = FMath::Min(VisualTailMin, 2.f * Data.Main.X - Data.OwnDistance);
		}

		for (int32 Ring = 0; Ring + 1 < Count; ++Ring)
		{
			for (int32 i = 0; i < Sides; ++i)
			{
				const FVertexInstanceID A = Instances[Ring * (Sides + 1) + i];
				const FVertexInstanceID B = Instances[Ring * (Sides + 1) + i + 1];
				const FVertexInstanceID C = Instances[(Ring + 1) * (Sides + 1) + i];
				const FVertexInstanceID D = Instances[(Ring + 1) * (Sides + 1) + i + 1];
				Description.CreateTriangle(Group, { A, C, B });
				Description.CreateTriangle(Group, { B, C, D });
			}
		}
	};

	// --- Main tube ---
	const int32 Rings = Line.Num();
	TArray<float> MainDistance;
	TArray<FVector> MainSide;
	TArray<FTubeRing> MainData;
	MainDistance.SetNumUninitialized(Rings);
	MainSide.SetNumUninitialized(Rings);
	MainData.SetNum(Rings);
	{
		float Distance = 0.f;
		FVector Side = FVector::RightVector;
		for (int32 Ring = 0; Ring < Rings; ++Ring)
		{
			if (Ring > 0)
			{
				Distance += FVector::Dist(Line[Ring - 1], Line[Ring]);
			}
			const FVector Forward = Line[FMath::Min(Ring + 1, Rings - 1)] - Line[FMath::Max(Ring - 1, 0)];
			const FVector NewSide = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal2D();
			if (!NewSide.IsNearlyZero())
			{
				Side = NewSide;
			}
			MainDistance[Ring] = Distance;
			MainSide[Ring] = Side;
			MainData[Ring].Radius = TubeRadius * (RadiusScale.IsValidIndex(Ring) ? RadiusScale[Ring] : 1.f);
			MainData[Ring].OwnDistance = Distance;
			MainData[Ring].Main = FVector2f(Distance, 0.f);
			MainData[Ring].MainSide = FVector2f(Side.X, Side.Y);
		}
		VisualLength = Distance;
	}
	{
		// Only the parts outside other pipelines; each piece reaches one ring into the hidden part, so its
		// open end lies inside the pipeline it joins.
		const int32 Sides = FMath::Clamp(TubeSides, 4, 32);
		auto Hidden = [&](int32 Ring) { return Merged.IsValidIndex(Ring) && Merged[Ring] == 2; };
		for (int32 Ring = 0; Ring < Rings;)
		{
			if (Hidden(Ring))
			{
				++Ring;
				continue;
			}
			int32 End = Ring;
			while (End + 1 < Rings && !Hidden(End + 1))
			{
				++End;
			}
			const int32 First = FMath::Max(Ring - 1, 0);
			const int32 Last = FMath::Min(End + 1, Rings - 1);
			if (Last > First)
			{
				AddTube(TArray<FVector>(Line.GetData() + First, Last - First + 1), TArray<FTubeRing>(MainData.GetData() + First, Last - First + 1), Sides);
			}
			Ring = End + 1;
		}
	}

	// --- Roots ---
	if (RootSpacing > 0.f && VisualLength > 0.f)
	{
		const int32 Sides = FMath::Clamp(RootSides, 3, 16);
		// Same seed on every client: from the replicated lane ends.
		auto Cell = [](const FVector& P) { return GetTypeHash(FIntVector(FMath::RoundToInt(P.X / 50.f), FMath::RoundToInt(P.Y / 50.f), 0)); };
		FRandomStream Random(int32(HashCombine(Cell(GroundPoints[0]), Cell(GroundPoints.Last()))));

		// Only where the tube lies in the open: the lane starts at the building edges.
		const float SourceClear = SourceBase ? FVector::Dist2D(GroundPoints[0], SourceBase->GetActorLocation()) + TubeRadius : 0.f;
		const float SinkClear = SinkBase ? FVector::Dist2D(GroundPoints.Last(), SinkBase->GetActorLocation()) + TubeRadius : 0.f;

		// Start (on the main tube's centre line or a parent root), flat direction, length, radius there,
		// main-tube ring it belongs to, progress along the root at the start (UV1.y - 1), UV0.x at the start,
		// and whether it may fork once.
		auto AddRoot = [&](auto& Self, const FVector& Start, const FVector& Dir, float Length, float StartRadius,
			int32 Ring, float Progress0, float Own0, bool bFork) -> void
		{
			const int32 Steps = FMath::Clamp(FMath::CeilToInt(Length / 22.f), 4, 16);
			const FVector Across = FVector::CrossProduct(FVector::UpVector, Dir);
			const float Phase = Random.FRand() * 2.f * PI;
			const float Bend = Random.FRandRange(-0.2f, 0.2f) * Length;
			const float Arch = StartRadius * Random.FRandRange(0.3f, 1.f);

			TArray<FVector> Points;
			TArray<FTubeRing> Data;
			Points.Reserve(Steps + 1);
			Data.Reserve(Steps + 1);
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const float T = float(Step) / float(Steps);
				FVector P = Start + Dir * (Length * T) + Across * (Bend * T * T + FMath::Sin(T * 2.6f * PI + Phase) * Length * 0.05f * T);
				// From the tube down into the ground: lies on it in the middle, the tip is buried.
				const float GroundZ = TraceGround(P).Z - RootTipDepth;
				P.Z = FMath::Lerp(Start.Z, GroundZ, FMath::SmoothStep(0.2f, 1.f, T)) + Arch * FMath::Sin(PI * T) * (1.f - T);
				Points.Add(P);

				FTubeRing& R = Data.AddDefaulted_GetRef();
				R.Radius = StartRadius * FMath::Lerp(1.f, 0.1f, T);
				R.OwnDistance = Own0 + Length * T;
				R.Main = FVector2f(MainDistance[Ring], 1.f + FMath::Lerp(Progress0, 1.f, T));
				R.MainSide = FVector2f(MainSide[Ring].X, MainSide[Ring].Y);
			}
			AddTube(Points, Data, Sides);

			if (bFork && Random.FRand() < RootForkChance)
			{
				const float ForkT = Random.FRandRange(0.3f, 0.6f);
				const int32 ForkStep = FMath::RoundToInt(ForkT * Steps);
				const float Turn = FMath::DegreesToRadians(Random.FRandRange(25.f, 50.f)) * (Random.FRand() < 0.5f ? -1.f : 1.f);
				const FVector ForkDir = Dir.RotateAngleAxisRad(Turn, FVector::UpVector);
				Self(Self, Points[ForkStep], ForkDir, Length * (1.f - ForkT) * Random.FRandRange(0.6f, 1.f), Data[ForkStep].Radius * 0.7f,
					Ring, FMath::Lerp(Progress0, 1.f, ForkT), Data[ForkStep].OwnDistance, false);
			}
		};

		const bool bHasPartnerSide = !FMath::IsNearlyZero(LaneOffset);
		const float MaxLength = FMath::Max(RootLengthMin, RootLengthMax);
		float Distance = Random.FRandRange(0.3f, 1.f) * RootSpacing;
		int32 Ring = 0;
		while (Distance < VisualLength)
		{
			while (Ring < Rings - 1 && MainDistance[Ring] < Distance)
			{
				++Ring;
			}
			const FVector& P = Line[Ring];
			// Not where it lies on another pipeline: that one has its own roots there.
			const bool bInOpen = (!SourceBase || FVector::Dist2D(P, SourceBase->GetActorLocation()) > SourceClear)
				&& (!SinkBase || FVector::Dist2D(P, SinkBase->GetActorLocation()) > SinkClear)
				&& !(Merged.IsValidIndex(Ring) && Merged[Ring]);
			if (bInOpen)
			{
				const float SideSign = Random.FRand() < 0.5f ? 1.f : -1.f;
				float Length = Random.FRandRange(RootLengthMin, MaxLength);
				// Short on the side of a pipeline in the other direction, and no fork there.
				const bool bInner = SideSign < 0.f && bHasPartnerSide;
				if (bInner)
				{
					Length = FMath::Max(Length * InnerRootScale, TubeRadius * 1.6f);
				}
				// Mostly leaning with the flow.
				const FVector Forward = FVector::CrossProduct(MainSide[Ring], FVector::UpVector);
				const float Yaw = FMath::DegreesToRadians(Random.FRandRange(-25.f, 55.f));
				const FVector Dir = (MainSide[Ring] * SideSign * FMath::Cos(Yaw) + Forward * FMath::Sin(Yaw)).GetSafeNormal2D();
				AddRoot(AddRoot, P, Dir, Length, TubeRadius * RootRadiusScale, Ring, 0.f, MainDistance[Ring], !bInner);
			}
			Distance += RootSpacing * Random.FRandRange(0.5f, 1.5f);
		}
	}

	UMaterialInterface* Material = (bIsSupplyRoad && SupplyPipeMaterial) ? SupplyPipeMaterial.Get() : PipeMaterial.Get();

	// Transient, built on the fast path (the only one in a cooked game). Rebuilt only when the lane changes.
	PipeMesh = NewObject<UStaticMesh>(this, NAME_None, RF_Transient);
	FStaticMaterial& Slot = PipeMesh->GetStaticMaterials().Add_GetRef(FStaticMaterial(Material, PipelineParams::MaterialSlot));
#if WITH_EDITORONLY_DATA
	Slot.ImportedMaterialSlotName = PipelineParams::MaterialSlot;
#endif
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = false;
	Params.bCommitMeshDescription = false;
	Params.bFastBuild = true;
	Params.bBuildSimpleCollision = false;
	// UV0.x / UV1.x are distances in uu - 16 bit would make the bulges jitter on long tubes.
	UStaticMesh::FBuildMeshDescriptionsLODParams LODParams;
	LODParams.bUseFullPrecisionUVs = true;
	Params.PerLODOverrides.Add(LODParams);
	if (!PipeMesh->BuildFromMeshDescriptions({ &Description }, Params))
	{
		PipeMesh = nullptr;
		return;
	}

	PipeComponent = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	PipeComponent->SetMobility(EComponentMobility::Movable);
	PipeComponent->SetStaticMesh(PipeMesh);
	PipeComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PipeComponent->SetCanEverAffectNavigation(false);
	PipeComponent->SetReceivesDecals(false);
	// The wobble is only in the material: keep it out of the shadow cache invalidation.
	PipeComponent->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Rigid;
	PipeComponent->SetWorldPositionOffsetDisableDistance(WobbleDisableDistance);
	// The bulge pushes the surface out by up to its radius.
	PipeComponent->SetBoundsScale(1.25f);
	PipeComponent->SetupAttachment(Root);
	PipeComponent->SetUsingAbsoluteRotation(true);
	PipeComponent->SetUsingAbsoluteScale(true);
	if (Material)
	{
		PipeMID = UMaterialInstanceDynamic::Create(Material, this);
		PipeComponent->SetMaterial(0, PipeMID);
	}
	PipeComponent->RegisterComponent();

	UpdatePacketParameters();
	UpdateTeamVisibility();
	OnRoadVisualsRebuilt();
}
