// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "System/LogisticsSubsystem.h"

#include "Actors/LogisticsRoad.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/UnitBase.h"
#include "Components/CapsuleComponent.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameModes/ResourceGameMode.h"
#include "GAS/AttributeSetBase.h"
#include "Mass/UnitMassTag.h"
#include "MassEntitySubsystem.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

static TAutoConsoleVariable<int32> CVarLogisticsDebug(
	TEXT("RTS.Logistics.Debug"),
	0,
	TEXT("1 = draw roads, stored amounts and the job of every logistics unit (server world)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarLogisticsLog(
	TEXT("RTS.Logistics.Log"),
	0,
	TEXT("1 = log every job change of the logistics units and a summary every 5 s."),
	ECVF_Default);

namespace
{
	const TCHAR* JobStateName(ELogisticsJobState State)
	{
		switch (State)
		{
		case ELogisticsJobState::Idle:      return TEXT("Idle");
		case ELogisticsJobState::ToPickup:  return TEXT("ToPickup");
		case ELogisticsJobState::Loading:   return TEXT("Loading");
		case ELogisticsJobState::ToDropoff: return TEXT("ToDropoff");
		case ELogisticsJobState::Unloading: return TEXT("Unloading");
		case ELogisticsJobState::Paused:    return TEXT("Paused");
		}
		return TEXT("?");
	}

	constexpr int32 NumResourceTypes = static_cast<int32>(EResourceType::MAX);

	/** Units that stopped short re-plan this often before the job is dropped. */
	constexpr int32 MaxMoveRetries = 4;

	/** No progress towards the target for this long counts as stuck - re-issue the move. */
	constexpr double StuckSeconds = 12.0;
}

// ---------------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------------

bool ULogisticsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void ULogisticsSubsystem::Deinitialize()
{
	Jobs.Reset();
	RoadsBySource.Reset();
	RouteCache.Reset();
	Super::Deinitialize();
}

TStatId ULogisticsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULogisticsSubsystem, STATGROUP_Tickables);
}

bool ULogisticsSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client && World->HasBegunPlay();
}

void ULogisticsSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	AResourceGameMode* GameMode = World ? Cast<AResourceGameMode>(World->GetAuthGameMode()) : nullptr;
	if (!GameMode || !GameMode->bEnableLogistics)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();

	if (bRoadsDirty || Now - LastRoadRefresh >= GameMode->LogisticsRoadRefreshInterval)
	{
		bRoadsDirty = false;
		LastRoadRefresh = Now;
		RefreshRoads(GameMode);
	}

	// Units given the role in a Blueprint after BeginPlay, or loaded from a save, are found here.
	if (Now - LastDiscovery >= 3.0)
	{
		LastDiscovery = Now;
		DiscoverLogisticsUnits();
	}

	if (Now - LastDispatch >= GameMode->LogisticsTickInterval)
	{
		LastDispatch = Now;
		TickDispatcher(GameMode);
	}

	if (CVarLogisticsDebug.GetValueOnGameThread() > 0)
	{
		DrawDebug(GameMode);
	}

	if (CVarLogisticsLog.GetValueOnGameThread() > 0 && Now - LastSummary >= 5.0)
	{
		LastSummary = Now;
		LogSummary();
	}
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

bool ULogisticsSubsystem::IsBaseAlive(const ABuildingBase* Base)
{
	return IsValid(Base) && Base->GetUnitState() != UnitData::Dead;
}

bool ULogisticsSubsystem::IsUnitAlive(const AUnitBase* Unit)
{
	return IsValid(Unit) && Unit->GetUnitState() != UnitData::Dead;
}

float ULogisticsSubsystem::GetBaseRadius(const ABuildingBase* Base)
{
	if (!Base)
	{
		return 0.f;
	}

	float Radius = 0.f;
	if (const UCapsuleComponent* Capsule = Base->GetCapsuleComponent())
	{
		Radius = Capsule->GetScaledCapsuleRadius();
	}

	// The capsule of a building is often much smaller than what you see. Take the colliding
	// footprint too, clamped so an oversized helper component cannot push the road ends away.
	const FBox Bounds = Base->GetComponentsBoundingBox(/*bNonColliding=*/false);
	if (Bounds.IsValid)
	{
		const FVector Extent = Bounds.GetExtent();
		Radius = FMath::Max(Radius, FMath::Min(FMath::Max(Extent.X, Extent.Y), 2000.f));
	}
	return FMath::Max(Radius, 100.f);
}

FVector ULogisticsSubsystem::GetBaseEdgePoint(const ABuildingBase* Base, const FVector& Toward, float Margin) const
{
	const FVector Center = Base->GetActorLocation();
	FVector Dir = Toward - Center;
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = FVector::ForwardVector;
	}
	return Center + Dir * (GetBaseRadius(Base) + Margin);
}

ABuildingBase* ULogisticsSubsystem::FindNearestSink(int32 TeamId, const FVector& From) const
{
	ABuildingBase* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ABuildingBase> It(GetWorld()); It; ++It)
	{
		ABuildingBase* Base = *It;
		if (!IsBaseAlive(Base) || Base->TeamId != TeamId || !Base->CreditsOnReceive())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Base->GetActorLocation(), From);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Base;
		}
	}
	return Best;
}

float ULogisticsSubsystem::GetReservedAt(const ABuildingBase* Source) const
{
	float Reserved = 0.f;
	for (const FLogisticsJob& Job : Jobs)
	{
		if (Job.ReservedAmount > 0.f && Job.TargetBase.Get() == Source)
		{
			Reserved += Job.ReservedAmount;
		}
	}
	return Reserved;
}

ELogisticsJobState ULogisticsSubsystem::GetJobState(const AUnitBase* Unit) const
{
	for (const FLogisticsJob& Job : Jobs)
	{
		if (Job.Unit.Get() == Unit)
		{
			return Job.State;
		}
	}
	return ELogisticsJobState::Idle;
}

TArray<ALogisticsRoad*> ULogisticsSubsystem::GetRoadsOfTeam(int32 TeamId) const
{
	TArray<ALogisticsRoad*> Result;
	for (const auto& Pair : RoadsBySource)
	{
		if (ALogisticsRoad* Road = Pair.Value.Get())
		{
			if (Road->TeamId == TeamId)
			{
				Result.Add(Road);
			}
		}
	}
	return Result;
}

// ---------------------------------------------------------------------------------------------
// Roads
// ---------------------------------------------------------------------------------------------

const FLogisticsRouteCache& ULogisticsSubsystem::GetOrComputeRoute(ABuildingBase* Source, ABuildingBase* Sink, AResourceGameMode* GameMode)
{
	const TPair<TWeakObjectPtr<ABuildingBase>, TWeakObjectPtr<ABuildingBase>> Key(Source, Sink);
	const double Now = GetWorld()->GetTimeSeconds();

	if (const FLogisticsRouteCache* Cached = RouteCache.Find(Key))
	{
		const bool bMoved =
			FVector::DistSquared2D(Cached->SourceLocation, Source->GetActorLocation()) > FMath::Square(200.f) ||
			FVector::DistSquared2D(Cached->SinkLocation, Sink->GetActorLocation()) > FMath::Square(200.f);
		// A failed ground search is retried now and then: the navmesh may simply not have been
		// built yet when the road was first made.
		const bool bRetryFailed = !Cached->bHasGroundLane && Now - Cached->ComputedAt > 10.0;
		if (!bMoved && !bRetryFailed)
		{
			return *Cached;
		}
	}

	FLogisticsRouteCache Route;
	Route.SourceLocation = Source->GetActorLocation();
	Route.SinkLocation = Sink->GetActorLocation();
	Route.ComputedAt = Now;

	const float Margin = GameMode->LogisticsEndMargin;
	FVector Start = GetBaseEdgePoint(Source, Sink->GetActorLocation(), Margin);
	FVector End = GetBaseEdgePoint(Sink, Source->GetActorLocation(), Margin);

	UWorld* World = GetWorld();
	if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World))
	{
		const FVector Extent(600.f, 600.f, 1500.f);
		FNavLocation StartNav;
		FNavLocation EndNav;
		const bool bStartOnNav = NavSys->ProjectPointToNavigation(Start, StartNav, Extent);
		const bool bEndOnNav = NavSys->ProjectPointToNavigation(End, EndNav, Extent);
		if (bStartOnNav) Start = StartNav.Location;
		if (bEndOnNav) End = EndNav.Location;

		if (bStartOnNav && bEndOnNav)
		{
			UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Start, End);
			if (Path && Path->IsValid() && !Path->IsPartial() && Path->PathPoints.Num() >= 2)
			{
				Route.Points = Path->PathPoints;
				Route.bHasGroundLane = true;
			}
		}
	}

	if (!Route.bHasGroundLane)
	{
		Route.Points = { Start, End };
	}

	for (int32 i = 1; i < Route.Points.Num(); ++i)
	{
		Route.Length += FVector::Dist(Route.Points[i - 1], Route.Points[i]);
	}

	return RouteCache.Add(Key, MoveTemp(Route));
}

void ULogisticsSubsystem::RefreshRoads(AResourceGameMode* GameMode)
{
	UWorld* World = GetWorld();

	TArray<ABuildingBase*> Sources;
	TArray<ABuildingBase*> Sinks;
	for (TActorIterator<ABuildingBase> It(World); It; ++It)
	{
		ABuildingBase* Base = *It;
		if (!IsBaseAlive(Base))
		{
			continue;
		}
		if (Base->StoresLocally())
		{
			Sources.Add(Base);
		}
		else if (Base->CreditsOnReceive())
		{
			Sinks.Add(Base);
		}
	}

	TSet<ABuildingBase*> SourcesWithRoad;
	const float MaxLengthSq = GameMode->LogisticsMaxRoadLength > 0.f ? FMath::Square(GameMode->LogisticsMaxRoadLength) : TNumericLimits<float>::Max();

	for (ABuildingBase* Source : Sources)
	{
		// The three nearest sinks by straight line; of those the shortest real route wins, with a
		// ground lane preferred - a road ground units cannot drive is the last resort.
		TArray<ABuildingBase*> Candidates;
		for (ABuildingBase* Sink : Sinks)
		{
			if (Sink->TeamId == Source->TeamId &&
				FVector::DistSquared2D(Sink->GetActorLocation(), Source->GetActorLocation()) <= MaxLengthSq)
			{
				Candidates.Add(Sink);
			}
		}
		if (Candidates.Num() == 0)
		{
			continue;
		}

		const FVector SourceLocation = Source->GetActorLocation();
		Candidates.Sort([&SourceLocation](const ABuildingBase& A, const ABuildingBase& B)
		{
			return FVector::DistSquared2D(A.GetActorLocation(), SourceLocation) < FVector::DistSquared2D(B.GetActorLocation(), SourceLocation);
		});
		Candidates.SetNum(FMath::Min(Candidates.Num(), 3));

		// By value: GetOrComputeRoute adds to RouteCache, which may move the entries a pointer
		// from an earlier iteration would point into.
		ABuildingBase* BestSink = nullptr;
		FLogisticsRouteCache BestRouteValue;
		for (ABuildingBase* Sink : Candidates)
		{
			const FLogisticsRouteCache& Route = GetOrComputeRoute(Source, Sink, GameMode);
			const bool bBetter = !BestSink
				|| (Route.bHasGroundLane && !BestRouteValue.bHasGroundLane)
				|| (Route.bHasGroundLane == BestRouteValue.bHasGroundLane && Route.Length < BestRouteValue.Length);
			if (bBetter)
			{
				BestSink = Sink;
				BestRouteValue = Route;
			}
		}
		if (!BestSink || BestRouteValue.Points.Num() < 2)
		{
			continue;
		}
		const FLogisticsRouteCache* BestRoute = &BestRouteValue;

		SourcesWithRoad.Add(Source);

		ALogisticsRoad* Existing = RoadsBySource.FindRef(Source).Get();
		const bool bUnchanged = IsValid(Existing)
			&& Existing->SinkBase == BestSink
			&& Existing->TeamId == Source->TeamId
			&& Existing->bHasGroundLane == BestRoute->bHasGroundLane
			&& Existing->GroundPoints == BestRoute->Points;
		if (bUnchanged)
		{
			continue;
		}

		if (IsValid(Existing))
		{
			Existing->Destroy();
		}

		UClass* RoadClass = GameMode->LogisticsRoadClass ? GameMode->LogisticsRoadClass.Get() : ALogisticsRoad::StaticClass();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ALogisticsRoad* Road = World->SpawnActor<ALogisticsRoad>(RoadClass, FTransform(BestRoute->Points[0]), Params);
		if (!Road)
		{
			continue;
		}
		Road->InitializeRoad(Source, BestSink, Source->TeamId, BestRoute->Points, BestRoute->bHasGroundLane, GameMode->LogisticsAirLaneHeight);
		RoadsBySource.Add(Source, Road);

		if (CVarLogisticsLog.GetValueOnGameThread() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[Logistics] Road team %d: %s -> %s, %s lane, %.0f uu, %d points"),
				Source->TeamId, *GetNameSafe(Source), *GetNameSafe(BestSink),
				BestRoute->bHasGroundLane ? TEXT("ground+air") : TEXT("air only"), BestRoute->Length, BestRoute->Points.Num());
		}
	}

	// Roads whose source is gone, dead, no longer CollectOnly, or has no sink any more.
	for (auto It = RoadsBySource.CreateIterator(); It; ++It)
	{
		ABuildingBase* Source = It.Key().Get();
		if (!Source || !SourcesWithRoad.Contains(Source))
		{
			if (ALogisticsRoad* Road = It.Value().Get())
			{
				Road->Destroy();
			}
			It.RemoveCurrent();
		}
	}

	// Cached routes of bases that no longer exist.
	for (auto It = RouteCache.CreateIterator(); It; ++It)
	{
		if (!It.Key().Key.IsValid() || !It.Key().Value.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Logistics units
// ---------------------------------------------------------------------------------------------

void ULogisticsSubsystem::RefreshLogisticsUnit(AAbilityUnit* InUnit)
{
	AUnitBase* Unit = Cast<AUnitBase>(InUnit);
	if (!Unit)
	{
		return;
	}

	const int32 Index = Jobs.IndexOfByPredicate([Unit](const FLogisticsJob& Job) { return Job.Unit.Get() == Unit; });

	if (Unit->IsLogisticsUnit())
	{
		if (Index == INDEX_NONE)
		{
			FLogisticsJob& Job = Jobs.AddDefaulted_GetRef();
			Job.Unit = Unit;
			Job.bIsFlyer = Unit->IsFlying;
			if (Unit->LogisticsCargo.Num() < NumResourceTypes)
			{
				Unit->LogisticsCargo.SetNumZeroed(NumResourceTypes);
			}
		}
	}
	else if (Index != INDEX_NONE)
	{
		if (Jobs[Index].bLanded)
		{
			Unit->IsFlying = true;   // no longer a hauler - do not leave it parked on the ground
		}
		Jobs.RemoveAtSwap(Index);
	}
}

void ULogisticsSubsystem::ResumeLogistics(AUnitBase* Unit)
{
	if (!IsValid(Unit) || !Unit->IsLogisticsUnit())
	{
		return;
	}

	RefreshLogisticsUnit(Unit);
	FLogisticsJob* Job = Jobs.FindByPredicate([Unit](const FLogisticsJob& J) { return J.Unit.Get() == Unit; });
	if (!Job)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	switch (Job->State)
	{
	case ELogisticsJobState::ToPickup:
	case ELogisticsJobState::ToDropoff:
		// Mid-trip: just carry on to where it was going.
		if (Job->bHasIssuedTarget)
		{
			IssueMove(*Job, Job->IssuedTarget);
			break;
		}
		// fall through - no target to return to
	default:
		ReleaseReservation(*Job);
		SetJobState(*Job, ELogisticsJobState::Idle, Now);
		Job->NextAssignTime = 0.0;
		Job->bResumeRequested = true;
		break;
	}
}

void ULogisticsSubsystem::DiscoverLogisticsUnits()
{
	for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
	{
		AUnitBase* Unit = *It;
		if (IsValid(Unit) && Unit->IsLogisticsUnit() && Unit->HasActorBegunPlay())
		{
			RefreshLogisticsUnit(Unit);
		}
	}
}

ACustomControllerBase* ULogisticsSubsystem::GetCommandController(int32 TeamId)
{
	// Any server-side controller can execute a move - ExecuteBatchMove works on the units it is given,
	// not on its own team. Prefer one of the unit's team, so logs and AI bookkeeping stay plausible.
	ACustomControllerBase* Fallback = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ACustomControllerBase* PC = Cast<ACustomControllerBase>(It->Get()))
		{
			if (PC->SelectableTeamId == TeamId)
			{
				return PC;
			}
			if (!Fallback)
			{
				Fallback = PC;
			}
		}
	}
	return Fallback;
}

void ULogisticsSubsystem::SetJobState(FLogisticsJob& Job, ELogisticsJobState NewState, double Now)
{
	if (CVarLogisticsLog.GetValueOnGameThread() > 0 && Job.State != NewState)
	{
		UE_LOG(LogTemp, Log, TEXT("[Logistics] %s: %s -> %s (cargo %.0f, target %s)"),
			*GetNameSafe(Job.Unit.Get()), JobStateName(Job.State), JobStateName(NewState),
			Job.Unit.IsValid() ? Job.Unit->GetLogisticsCargoTotal() : 0.f, *GetNameSafe(Job.TargetBase.Get()));
	}

	const bool bWasStanding = Job.State == ELogisticsJobState::Loading || Job.State == ELogisticsJobState::Unloading;
	const bool bWillStand = NewState == ELogisticsJobState::Loading || NewState == ELogisticsJobState::Unloading;
	if (AUnitBase* Unit = Job.Unit.Get())
	{
		// Flying haulers land to load and unload. IsFlying is replicated and synced into Mass,
		// which lowers / raises the unit by itself.
		if (bWillStand && !bWasStanding && Job.bIsFlyer && Unit->LogisticsLandingTime > 0.f && Unit->IsFlying)
		{
			Unit->IsFlying = false;
			Job.bLanded = true;
		}
		else if (!bWillStand && Job.bLanded)
		{
			Unit->IsFlying = true;
			Job.bLanded = false;
		}
	}

	Job.State = NewState;
	Job.StateStartTime = Now;
	Job.IdleSince = -1.0;
	Job.Retries = 0;
	Job.BestDistance = TNumericLimits<float>::Max();
	Job.LastProgressTime = Now;
	// Every state but Paused keeps the last own target: it is what WasOverridden compares against.
	// Idle needs it too - a unit that just unloaded may still be rolling out from our order, and
	// that must not be mistaken for an order of the player.
	if (NewState == ELogisticsJobState::Paused)
	{
		Job.bHasIssuedTarget = false;
		Job.bResumeRequested = false;
	}

	if (AUnitBase* Unit = Job.Unit.Get())
	{
		Unit->LogisticsJobState = NewState;
	}
}

void ULogisticsSubsystem::ReleaseReservation(FLogisticsJob& Job)
{
	Job.ReservedAmount = 0.f;
}

bool ULogisticsSubsystem::IssueMove(FLogisticsJob& Job, const FVector& Target)
{
	AUnitBase* Unit = Job.Unit.Get();
	if (!Unit)
	{
		return false;
	}

	ACustomControllerBase* Controller = CachedController.Get();
	if (!Controller || Controller->SelectableTeamId != Unit->TeamId)
	{
		Controller = GetCommandController(Unit->TeamId);
		CachedController = Controller;
	}
	if (!Controller)
	{
		return false;
	}

	const TArray<AUnitBase*> Units = { Unit };
	const TArray<FVector> Validated = Controller->AdjustBatchTargetsForNav(Units, { Target });
	const float Speed = Unit->Attributes ? Unit->Attributes->GetRunSpeed() : 300.f;

	// A held Shift on that controller would append our order as a waypoint instead of replacing the
	// current one. The dispatcher's orders are never queued.
	const bool bShiftWasPressed = Controller->IsShiftPressed;
	Controller->IsShiftPressed = false;
	Controller->ExecuteBatchMove(Unit, Units, Validated, { Speed }, { Unit->MovementAcceptanceRadius },
		/*AttackT=*/false, /*bResetHoldPosition=*/true, /*bResetFollowTarget=*/true, /*bTargetsAlreadyValidated=*/true);
	Controller->NotifyClientsOfBatchMove(Units, Validated, { Speed }, { Unit->MovementAcceptanceRadius },
		/*AttackT=*/false, /*bResetHoldPosition=*/true, /*bResetFollowTarget=*/true, /*bOriginatorPredictsLocally=*/false);
	Controller->IsShiftPressed = bShiftWasPressed;

	Job.IssuedTarget = Validated.Num() > 0 ? Validated[0] : Target;
	Job.bHasIssuedTarget = true;
	Job.LastIssueTime = GetWorld()->GetTimeSeconds();

	// ExecuteBatchMove may still snap the point (last-resort search off a dirty nav area). What it
	// finally wrote into StoredLocation is the reference for WasOverridden - not our request.
	const FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	if (Unit->GetMassEntityData(EntityManager, Entity) && EntityManager && EntityManager->IsEntityValid(Entity))
	{
		if (const FMassAIStateFragment* State = EntityManager->GetFragmentDataPtr<FMassAIStateFragment>(Entity))
		{
			Job.IssuedTarget = State->StoredLocation;
		}
	}
	Job.LastProgressTime = GetWorld()->GetTimeSeconds();
	Job.BestDistance = TNumericLimits<float>::Max();
	return true;
}

bool ULogisticsSubsystem::HasArrived(const FLogisticsJob& Job, const FVector& UnitLocation, float Margin) const
{
	const ABuildingBase* Base = Job.TargetBase.Get();
	if (Job.bHasIssuedTarget &&
		FVector::Dist2D(UnitLocation, Job.IssuedTarget) <= FMath::Max(250.f, Job.Unit.IsValid() ? Job.Unit->MovementAcceptanceRadius * 2.f : 0.f))
	{
		return true;
	}
	return Base && FVector::Dist2D(UnitLocation, Base->GetActorLocation()) <= GetBaseRadius(Base) + Margin + 150.f;
}

bool ULogisticsSubsystem::WasOverridden(const FLogisticsJob& Job) const
{
	// Every move order, ours or the player's, ends up as the AI state's StoredLocation. If that no
	// longer is the point we sent the unit to, someone else gave it an order (or it went chasing).
	AUnitBase* Unit = Job.Unit.Get();
	if (!Unit || !Job.bHasIssuedTarget)
	{
		return false;
	}

	// Grace period: right after our own order the movement code may still settle the target
	// (measured: a flyer taking off read as "overridden" 0.7 s after its order). A real player order
	// is still seen a moment later - StoredLocation keeps pointing elsewhere.
	if (GetWorld()->GetTimeSeconds() - Job.LastIssueTime < 1.0)
	{
		return false;
	}

	const FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Entity;
	if (!Unit->GetMassEntityData(EntityManager, Entity) || !EntityManager || !EntityManager->IsEntityValid(Entity))
	{
		return false;
	}
	const FMassAIStateFragment* State = EntityManager->GetFragmentDataPtr<FMassAIStateFragment>(Entity);
	if (!State)
	{
		return false;
	}
	return FVector::Dist2D(State->StoredLocation, Job.IssuedTarget) > 300.f;
}

void ULogisticsSubsystem::DoLoad(FLogisticsJob& Job, double Now)
{
	AUnitBase* Unit = Job.Unit.Get();
	ABuildingBase* Source = Job.TargetBase.Get();
	ReleaseReservation(Job);
	if (!Unit || !IsBaseAlive(Source))
	{
		SetJobState(Job, ELogisticsJobState::Idle, Now);
		return;
	}

	if (Unit->LogisticsCargo.Num() < NumResourceTypes)
	{
		Unit->LogisticsCargo.SetNumZeroed(NumResourceTypes);
	}

	// Largest stack first, so a mixed store empties evenly and the most-needed type moves first.
	TArray<int32> Order;
	for (int32 i = 0; i < NumResourceTypes; ++i)
	{
		Order.Add(i);
	}
	Order.Sort([Source](int32 A, int32 B)
	{
		return Source->GetStoredResourceAmount(static_cast<EResourceType>(A)) > Source->GetStoredResourceAmount(static_cast<EResourceType>(B));
	});

	// Leave what other haulers on their way here have claimed (ours was released above), or the
	// first to arrive takes everything and the next one drives back empty.
	float Free = FMath::Max(0.f, Unit->LogisticsCapacity - Unit->GetLogisticsCargoTotal());
	Free = FMath::Min(Free, FMath::Max(0.f, Source->GetStoredResourceTotal() - GetReservedAt(Source)));
	for (int32 Index : Order)
	{
		if (Free <= 0.f)
		{
			break;
		}
		const float Taken = Source->TakeStoredResource(static_cast<EResourceType>(Index), Free);
		Unit->LogisticsCargo[Index] += Taken;
		Free -= Taken;
	}

	if (Unit->GetLogisticsCargoTotal() <= 0.f)
	{
		// Someone else emptied it first. Try again shortly.
		Job.NextAssignTime = Now + 1.0;
		SetJobState(Job, ELogisticsJobState::Idle, Now);
		return;
	}

	ALogisticsRoad* Road = Job.Road.Get();
	ABuildingBase* Sink = (Road && IsBaseAlive(Road->SinkBase)) ? Road->SinkBase.Get() : FindNearestSink(Unit->TeamId, Unit->GetMassActorLocation());
	if (!Sink)
	{
		// Loaded, but nowhere to go - wait at the source; Idle with cargo retries the delivery.
		SetJobState(Job, ELogisticsJobState::Idle, Now);
		return;
	}

	Job.TargetBase = Sink;
	SetJobState(Job, ELogisticsJobState::ToDropoff, Now);
	const FVector DropOff = (Road && Road->SinkBase == Sink) ? Road->GetSinkEnd() : GetBaseEdgePoint(Sink, Unit->GetMassActorLocation(), 250.f);
	IssueMove(Job, DropOff);
}

void ULogisticsSubsystem::DoUnload(FLogisticsJob& Job, double Now)
{
	AUnitBase* Unit = Job.Unit.Get();
	ABuildingBase* Sink = Job.TargetBase.Get();
	if (!Unit || !IsBaseAlive(Sink) || !Sink->CreditsOnReceive())
	{
		// The sink died or changed type while we were unloading - deliver elsewhere.
		SetJobState(Job, ELogisticsJobState::Idle, Now);
		return;
	}

	float Delivered = 0.f;
	for (int32 i = 0; i < Unit->LogisticsCargo.Num(); ++i)
	{
		if (Unit->LogisticsCargo[i] > 0.f)
		{
			Delivered += Sink->ReceiveResource(static_cast<EResourceType>(i), Unit->LogisticsCargo[i], Unit->TeamId);
			Unit->LogisticsCargo[i] = 0.f;
		}
	}

	if (CVarLogisticsLog.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Logistics] %s delivered %.0f to %s (team %d)"),
			*GetNameSafe(Unit), Delivered, *GetNameSafe(Sink), Unit->TeamId);
	}

	Job.NextAssignTime = Now;
	SetJobState(Job, ELogisticsJobState::Idle, Now);
}

bool ULogisticsSubsystem::StartDelivery(FLogisticsJob& Job, double Now, float Margin)
{
	AUnitBase* Unit = Job.Unit.Get();
	ABuildingBase* Sink = Unit ? FindNearestSink(Unit->TeamId, Unit->GetMassActorLocation()) : nullptr;
	if (!Sink)
	{
		return false;
	}
	Job.TargetBase = Sink;
	Job.Road = nullptr;
	SetJobState(Job, ELogisticsJobState::ToDropoff, Now);
	return IssueMove(Job, GetBaseEdgePoint(Sink, Unit->GetMassActorLocation(), Margin));
}

void ULogisticsSubsystem::TickDispatcher(AResourceGameMode* GameMode)
{
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	const float Margin = GameMode->LogisticsEndMargin;

	// Forget the dead and the converted. Their claims go with them; cargo of a dead unit is lost.
	for (FLogisticsJob& Job : Jobs)
	{
		AUnitBase* Unit = Job.Unit.Get();
		if (Job.bLanded && IsUnitAlive(Unit) && !Unit->IsLogisticsUnit())
		{
			Unit->IsFlying = true;
		}
	}
	Jobs.RemoveAllSwap([](const FLogisticsJob& Job)
	{
		const AUnitBase* Unit = Job.Unit.Get();
		return !IsUnitAlive(Unit) || !Unit->IsLogisticsUnit();
	});

	for (FLogisticsJob& Job : Jobs)
	{
		AUnitBase* Unit = Job.Unit.Get();
		if (Unit->IsInsideTransport)
		{
			continue;
		}

		const FVector Location = Unit->GetMassActorLocation();
		const bool bUnitIdle = Unit->GetUnitState() == UnitData::Idle || Unit->GetUnitState() == UnitData::PatrolIdle;

		switch (Job.State)
		{
		case ELogisticsJobState::Paused:
		{
			if (Unit->LogisticsResumeDelay < 0.f)
			{
				break;
			}
			if (!bUnitIdle)
			{
				Job.IdleSince = -1.0;
				break;
			}
			if (Job.IdleSince < 0.0)
			{
				Job.IdleSince = Now;
			}
			if (Now - Job.IdleSince >= Unit->LogisticsResumeDelay)
			{
				SetJobState(Job, ELogisticsJobState::Idle, Now);
			}
			break;
		}

		case ELogisticsJobState::ToPickup:
		case ELogisticsJobState::ToDropoff:
		{
			const bool bPickup = Job.State == ELogisticsJobState::ToPickup;
			ABuildingBase* Target = Job.TargetBase.Get();
			const bool bTargetValid = IsBaseAlive(Target) && (bPickup ? Target->StoresLocally() : Target->CreditsOnReceive());
			if (!bTargetValid)
			{
				ReleaseReservation(Job);
				SetJobState(Job, ELogisticsJobState::Idle, Now);
				break;
			}

			if (WasOverridden(Job))
			{
				ReleaseReservation(Job);
				SetJobState(Job, ELogisticsJobState::Paused, Now);
				break;
			}

			if (HasArrived(Job, Location, Margin))
			{
				SetJobState(Job, bPickup ? ELogisticsJobState::Loading : ELogisticsJobState::Unloading, Now);
				// Stand still while loading - an arrival a little short of the point would otherwise
				// keep the unit creeping on.
				break;
			}

			const float Distance = FVector::Dist2D(Location, Job.IssuedTarget);
			if (Distance < Job.BestDistance - 50.f)
			{
				Job.BestDistance = Distance;
				Job.LastProgressTime = Now;
			}

			// Stopped short, or no progress for a while: send it again, give up after a few tries.
			const bool bStoppedShort = bUnitIdle && Now - Job.StateStartTime > 1.0;
			const bool bStuck = Now - Job.LastProgressTime > StuckSeconds;
			if (bStoppedShort || bStuck)
			{
				if (++Job.Retries > MaxMoveRetries)
				{
					ReleaseReservation(Job);
					Job.NextAssignTime = Now + 5.0;
					SetJobState(Job, ELogisticsJobState::Idle, Now);
					break;
				}
				const int32 Retries = Job.Retries;
				IssueMove(Job, Job.IssuedTarget);
				Job.Retries = Retries;
				Job.StateStartTime = Now;
			}
			break;
		}

		case ELogisticsJobState::Loading:
		case ELogisticsJobState::Unloading:
		{
			// Ordered away mid-load: the player wants the unit for something else.
			if (WasOverridden(Job))
			{
				ReleaseReservation(Job);
				SetJobState(Job, ELogisticsJobState::Paused, Now);
				break;
			}
			const float StandTime = Job.bLanded ? FMath::Max(Unit->LogisticsLoadTime, Unit->LogisticsLandingTime) : Unit->LogisticsLoadTime;
			if (Now - Job.StateStartTime < StandTime)
			{
				break;
			}
			if (Job.State == ELogisticsJobState::Loading)
			{
				DoLoad(Job, Now);
			}
			else
			{
				DoUnload(Job, Now);
			}
			break;
		}

		case ELogisticsJobState::Idle:
		default:
		{
			// Moving without an order of ours (or away from it): the player sent it somewhere. Assigning
			// a route now would silently overwrite that order - step back until it stands idle again.
			if (!bUnitIdle && !Job.bResumeRequested && (!Job.bHasIssuedTarget || WasOverridden(Job)))
			{
				SetJobState(Job, ELogisticsJobState::Paused, Now);
				break;
			}

			if (Now < Job.NextAssignTime)
			{
				break;
			}

			// Still carrying something (interrupted trip, sink died): deliver that first.
			if (Unit->GetLogisticsCargoTotal() > 0.f)
			{
				Job.bResumeRequested = false;
				if (!StartDelivery(Job, Now, Margin))
				{
					Job.NextAssignTime = Now + 2.0;
				}
				break;
			}

			const bool bCanFly = Job.bIsFlyer;
			ALogisticsRoad* BestRoad = nullptr;
			float BestScore = 0.f;
			float BestAvailable = 0.f;
			for (const auto& Pair : RoadsBySource)
			{
				ALogisticsRoad* Road = Pair.Value.Get();
				ABuildingBase* Source = Pair.Key.Get();
				if (!Road || !IsBaseAlive(Source) || Road->TeamId != Unit->TeamId)
				{
					continue;
				}
				if (!Road->bHasGroundLane && !bCanFly)
				{
					continue;
				}

				const float Available = Source->GetStoredResourceTotal() - GetReservedAt(Source);
				// A full store is collected whatever the minimum says - otherwise a store that filled up
				// below the minimum would stay full for good and keep turning the workers away.
				const float Needed = Source->IsStorageFull() ? 1.f : FMath::Min(GameMode->LogisticsMinPickupAmount, Unit->LogisticsCapacity);
				if (Available < FMath::Max(Needed, 1.f))
				{
					continue;
				}

				// Most waiting, discounted by how far away it is - a nearly full store across the map
				// still beats an almost empty one next door, but not by an unlimited margin.
				const float Score = FMath::Min(Available, Unit->LogisticsCapacity) / (1.f + FVector::Dist2D(Location, Road->GetSourceEnd()) / 5000.f);
				if (Score > BestScore)
				{
					BestScore = Score;
					BestRoad = Road;
					BestAvailable = Available;
				}
			}

			if (!BestRoad)
			{
				Job.NextAssignTime = Now + 1.0;
				break;
			}

			Job.bResumeRequested = false;
			Job.Road = BestRoad;
			Job.TargetBase = BestRoad->SourceBase;
			Job.ReservedAmount = FMath::Min(BestAvailable, Unit->LogisticsCapacity);
			SetJobState(Job, ELogisticsJobState::ToPickup, Now);
			if (!IssueMove(Job, BestRoad->GetSourceEnd()))
			{
				ReleaseReservation(Job);
				Job.NextAssignTime = Now + 2.0;
				SetJobState(Job, ELogisticsJobState::Idle, Now);
			}
			break;
		}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------------------------

void ULogisticsSubsystem::DrawDebug(AResourceGameMode* GameMode) const
{
	UWorld* World = GetWorld();
	for (const auto& Pair : RoadsBySource)
	{
		const ALogisticsRoad* Road = Pair.Value.Get();
		const ABuildingBase* Source = Pair.Key.Get();
		if (!Road || !Source)
		{
			continue;
		}
		for (int32 i = 1; i < Road->GroundPoints.Num(); ++i)
		{
			DrawDebugLine(World, Road->GroundPoints[i - 1] + FVector(0, 0, 40), Road->GroundPoints[i] + FVector(0, 0, 40),
				Road->bHasGroundLane ? FColor::Yellow : FColor::Cyan, false, -1.f, 0, 6.f);
		}
		DrawDebugString(World, Source->GetActorLocation() + FVector(0, 0, 500),
			FString::Printf(TEXT("Store %.0f%s (claimed %.0f)"), Source->GetStoredResourceTotal(),
				Source->StorageCapacity > 0.f ? *FString::Printf(TEXT("/%.0f"), Source->StorageCapacity) : TEXT(""),
				GetReservedAt(Source)),
			nullptr, FColor::Yellow, 0.f, true, 1.4f);
	}

	for (const FLogisticsJob& Job : Jobs)
	{
		const AUnitBase* Unit = Job.Unit.Get();
		if (!Unit)
		{
			continue;
		}
		DrawDebugString(World, Unit->GetMassActorLocation() + FVector(0, 0, 250),
			FString::Printf(TEXT("%s %.0f/%.0f"), JobStateName(Job.State), Unit->GetLogisticsCargoTotal(), Unit->LogisticsCapacity),
			nullptr, FColor::Green, 0.f, true, 1.2f);
		if (Job.bHasIssuedTarget)
		{
			DrawDebugLine(World, Unit->GetMassActorLocation(), Job.IssuedTarget, FColor::Green, false, -1.f, 0, 2.f);
		}
	}
}

void ULogisticsSubsystem::LogSummary() const
{
	int32 Counts[6] = {};
	float Cargo = 0.f;
	for (const FLogisticsJob& Job : Jobs)
	{
		Counts[FMath::Clamp(static_cast<int32>(Job.State), 0, 5)]++;
		if (Job.Unit.IsValid())
		{
			Cargo += Job.Unit->GetLogisticsCargoTotal();
		}
	}

	float Stored = 0.f;
	for (const auto& Pair : RoadsBySource)
	{
		if (const ABuildingBase* Source = Pair.Key.Get())
		{
			Stored += Source->GetStoredResourceTotal();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Logistics] Summary: roads=%d units=%d (Idle %d, ToPickup %d, Loading %d, ToDropoff %d, Unloading %d, Paused %d) cargo=%.0f stored=%.0f"),
		RoadsBySource.Num(), Jobs.Num(), Counts[0], Counts[1], Counts[2], Counts[3], Counts[4], Counts[5], Cargo, Stored);
}
