// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "GameModes/ProceduralPondComponent.h"

#include "Actors/ProceduralPond.h"
#include "Actors/ResourceClusterMarker.h"
#include "Actors/StoryTriggerActor.h"
#include "Actors/Waypoint.h"
#include "Actors/WorkArea.h"
#include "Characters/Unit/UnitBase.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeProxy.h"
#include "Math/RandomStream.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

UProceduralPondComponent::UProceduralPondComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

namespace
{
	/** Something the pond has to keep away from: a point and how far its shore must stay. */
	struct FPondKeepOut
	{
		FVector Location;
		float Clearance;
	};

	/** Ground below P: the hit point, and whether it is the landscape. */
	bool PondGround(UWorld* World, const FVector& P, float Up, float Down, FVector& OutHit, bool& bOutLandscape)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ProceduralPondGround), false);
		FHitResult Hit;
		if (!World->LineTraceSingleByObjectType(Hit, P + FVector(0.f, 0.f, Up), P - FVector(0.f, 0.f, Down),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			return false;
		}
		OutHit = Hit.ImpactPoint;
		bOutLandscape = Hit.GetActor() && Hit.GetActor()->IsA<ALandscapeProxy>();
		return true;
	}
}

void UProceduralPondComponent::GeneratePonds()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!bEnabled || !World || !Owner || !Owner->HasAuthority())
	{
		return;
	}
	ClearPonds();

	if (OnlyOnMaps.Num() > 0 && !OnlyOnMaps.Contains(UGameplayStatics::GetCurrentLevelName(this, true)))
	{
		return;
	}

	if (!PondCollection)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Ponds] No PondCollection set - the landscape could not draw the water. No ponds."));
		return;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
	if (!NavData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Ponds] No navmesh - no ponds."));
		return;
	}
	const FBox NavBounds = NavData->GetBounds();
	if (!NavBounds.IsValid)
	{
		return;
	}

	// Everything the ponds must stay clear of. Runs after the procedural resources: when they are on, the
	// placed resources are already gone (pending kill) and the new ones stand - IsValid takes care of both.
	TArray<FPondKeepOut> KeepOut;
	for (TActorIterator<AUnitBase> It(World); It; ++It)
	{
		if (IsValid(*It)) KeepOut.Add({ It->GetActorLocation(), UnitClearance + It->GetSimpleCollisionRadius() });
	}
	for (TActorIterator<AWorkArea> It(World); It; ++It)
	{
		if (IsValid(*It)) KeepOut.Add({ It->GetActorLocation(), ResourceClearance + It->GetSimpleCollisionRadius() });
	}
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (IsValid(*It)) KeepOut.Add({ It->GetActorLocation(), PlayerStartClearance });
	}
	for (TActorIterator<AWaypoint> It(World); It; ++It)
	{
		if (IsValid(*It)) KeepOut.Add({ It->GetActorLocation(), WaypointClearance });
	}
	for (TActorIterator<AResourceClusterMarker> It(World); It; ++It)
	{
		if (IsValid(*It)) KeepOut.Add({ It->GetActorLocation(), ResourceClearance });
	}
	// Story triggers often stand in the map as a visible object (e.g. the Philosophy monoliths).
	for (TActorIterator<AStoryTriggerActor> It(World); It; ++It)
	{
		if (IsValid(*It)) KeepOut.Add({ It->GetActorLocation(), StoryTriggerClearance });
	}

	FRandomStream Rand(Seed != 0 ? Seed : FMath::Rand());
	const int32 Lo = FMath::Clamp(FMath::Min(MinPonds, MaxPonds), 0, AProceduralPond::MaxPonds);
	const int32 Hi = FMath::Clamp(FMath::Max(MinPonds, MaxPonds), 0, AProceduralPond::MaxPonds);
	const int32 Wanted = Rand.RandRange(Lo, Hi);
	const float RMin = FMath::Max(100.f, FMath::Min(MinRadius, MaxRadius));
	const float RMax = FMath::Max(RMin, MaxRadius);

	// One ellipse of water. A pond is one, a river a chain of them (same Body).
	struct FPlanned { FVector Centre; float Rx; float Ry; float Yaw; int32 Body; };
	TArray<FPlanned> Planned;

	// Ground test for one ellipse: on the landscape, on the navmesh, flat. Returns the ground height range.
	auto CheckEllipse = [&](const FVector& C, float Rx, float Ry, float Yaw, float& OutZMin, float& OutZMax) -> bool
	{
		FVector Ground;
		bool bLandscape = false;
		if (!PondGround(World, C, 500.f, 1500.f, Ground, bLandscape) || (bLandscapeOnly && !bLandscape))
		{
			return false;
		}
		OutZMin = OutZMax = Ground.Z;
		const FTransform Xf(FRotator(0.f, Yaw, 0.f), Ground);
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = 2.f * PI * i / 12.f;
			const FVector Rim = Xf.TransformPosition(FVector((Rx + NavPadding) * 1.15f * FMath::Cos(A), (Ry + NavPadding) * 1.15f * FMath::Sin(A), 0.f));
			FVector RimGround;
			bool bRimLandscape = false;
			FNavLocation RimNav;
			if (!PondGround(World, Rim, 500.f, 1500.f, RimGround, bRimLandscape) || (bLandscapeOnly && !bRimLandscape)
				|| !NavSys->ProjectPointToNavigation(RimGround, RimNav, FVector(50.f, 50.f, 150.f), NavData))
			{
				return false;
			}
			OutZMin = FMath::Min(OutZMin, RimGround.Z);
			OutZMax = FMath::Max(OutZMax, RimGround.Z);
			if (OutZMax - OutZMin > MaxHeightDifference)
			{
				return false;
			}
		}
		return true;
	};

	// Clear of everything around it, and of the water of other bodies (shore to shore MinPondGap).
	auto IsFree = [&](const FVector& C, float Outer, int32 Body) -> bool
	{
		for (const FPondKeepOut& K : KeepOut)
		{
			if (FVector::Dist2D(K.Location, C) < Outer + K.Clearance) return false;
		}
		for (const FPlanned& P : Planned)
		{
			if (P.Body != Body && FVector::Dist2D(P.Centre, C) < Outer + FMath::Max(P.Rx, P.Ry) + NavPadding + MinPondGap) return false;
		}
		return true;
	};

	auto RandomOnNav = [&](float Outer, FVector& Out) -> bool
	{
		const FVector Candidate(Rand.FRandRange(NavBounds.Min.X + Outer, NavBounds.Max.X - Outer),
			Rand.FRandRange(NavBounds.Min.Y + Outer, NavBounds.Max.Y - Outer), NavBounds.GetCenter().Z);
		FNavLocation OnNav;
		if (!NavSys->ProjectPointToNavigation(Candidate, OnNav, FVector(50.f, 50.f, NavBounds.GetExtent().Z + 500.f), NavData))
		{
			return false;
		}
		Out = OnNav.Location;
		return true;
	};

	int32 Bodies = 0, Rivers = 0;
	const int32 SegMin = FMath::Clamp(FMath::Min(MinRiverSegments, MaxRiverSegments), 2, 16);
	const int32 SegMax = FMath::Clamp(FMath::Max(MinRiverSegments, MaxRiverSegments), 2, 16);

	// River: a winding chain of long, overlapping ellipses. Each segment tries a few bends; when none fits the
	// river ends there - long enough is kept, too short is thrown away.
	auto TryRiver = [&]() -> bool
	{
		const int32 SlotsLeft = AProceduralPond::MaxPonds - Planned.Num();
		if (SlotsLeft < SegMin)
		{
			return false;
		}
		const int32 Segments = FMath::Min(Rand.RandRange(SegMin, SegMax), SlotsLeft);
		const float BaseWidth = Rand.FRandRange(FMath::Min(MinRiverWidth, MaxRiverWidth), FMath::Max(MinRiverWidth, MaxRiverWidth));
		const float SegLen = FMath::Max(RiverSegmentLength, BaseWidth * 1.5f);
		const float Step = SegLen * 1.1f;
		FVector C;
		if (!RandomOnNav(SegLen + NavPadding, C))
		{
			return false;
		}
		float Dir = Rand.FRandRange(0.f, 360.f);
		float Turn = 0.f;
		float Width = BaseWidth;
		TArray<FPlanned> Chain;
		float ZLo = TNumericLimits<float>::Max(), ZHi = TNumericLimits<float>::Lowest();
		for (int32 k = 0; k < Segments; ++k)
		{
			bool bPlaced = false;
			for (int32 Attempt = 0; Attempt < 6 && !bPlaced; ++Attempt)
			{
				FVector Next = C;
				float NextDir = Dir;
				float NextTurn = Turn;
				float NextWidth = Width;
				if (k > 0)
				{
					// The bend drifts (meanders); later attempts allow sharper bends to get round obstacles.
					const float Spread = RiverCurviness * (1.f + Attempt * 0.5f);
					NextTurn = FMath::Clamp(Turn + Rand.FRandRange(-Spread, Spread) * 0.6f, -Spread, Spread);
					NextDir = Dir + NextTurn;
					const FVector Fwd = FRotator(0.f, (Dir + NextDir) * 0.5f, 0.f).Vector();
					FNavLocation OnNav;
					if (!NavSys->ProjectPointToNavigation(C + Fwd * Step, OnNav, FVector(80.f, 80.f, 400.f), NavData))
					{
						continue;
					}
					Next = OnNav.Location;
					NextWidth = FMath::Clamp(Width * Rand.FRandRange(0.85f, 1.15f), BaseWidth * 0.7f, BaseWidth * 1.3f);
				}
				float ZMin, ZMax;
				if (!IsFree(Next, SegLen + NavPadding, Bodies) || !CheckEllipse(Next, SegLen, NextWidth, NextDir, ZMin, ZMax))
				{
					if (k == 0) break;
					continue;
				}
				// A river may run down a gentle slope, but not down a hill.
				if (FMath::Max(ZHi, ZMax) - FMath::Min(ZLo, ZMin) > MaxHeightDifference * 2.f)
				{
					continue;
				}
				ZLo = FMath::Min(ZLo, ZMin);
				ZHi = FMath::Max(ZHi, ZMax);
				C = Next; Dir = NextDir; Turn = NextTurn; Width = NextWidth;
				Chain.Add({ FVector(C.X, C.Y, (ZMin + ZMax) * 0.5f), SegLen, Width, Dir, Bodies });
				bPlaced = true;
			}
			if (!bPlaced)
			{
				break;
			}
		}
		if (Chain.Num() < SegMin)
		{
			return false;
		}
		Planned.Append(Chain);
		++Bodies;
		++Rivers;
		return true;
	};

	// Pond: mostly small, now and then a big lake.
	auto TryPond = [&]() -> bool
	{
		if (Planned.Num() >= AProceduralPond::MaxPonds)
		{
			return false;
		}
		const float Major = FMath::Lerp(RMin, RMax, FMath::Pow(Rand.FRand(), FMath::Max(0.2f, SizeBias)));
		const float Minor = FMath::Max(RMin * 0.6f, Major / Rand.FRandRange(1.f, FMath::Max(1.f, MaxElongation)));
		const float Yaw = Rand.FRandRange(0.f, 360.f);
		FVector C;
		float ZMin, ZMax;
		if (!RandomOnNav(Major + NavPadding, C) || !IsFree(C, Major + NavPadding, Bodies)
			|| !CheckEllipse(C, Major, Minor, Yaw, ZMin, ZMax))
		{
			return false;
		}
		Planned.Add({ FVector(C.X, C.Y, (ZMin + ZMax) * 0.5f), Major, Minor, Yaw, Bodies });
		++Bodies;
		return true;
	};

	// Rivers first - they need the most room - then the ponds fill in.
	const int32 RiversWanted = RiverChance > 0.f && Wanted > 0 ? FMath::Max(1, FMath::RoundToInt(Wanted * RiverChance)) : 0;
	for (int32 r = 0; r < RiversWanted; ++r)
	{
		for (int32 Try = 0; Try < 400 && !TryRiver(); ++Try) {}
	}
	for (int32 Try = 0; Try < Wanted * 250 && Bodies < Wanted && Planned.Num() < AProceduralPond::MaxPonds; ++Try)
	{
		TryPond();
	}

	for (int32 i = 0; i < Planned.Num(); ++i)
	{
		const FPlanned& P = Planned[i];
		const FTransform SpawnXf(FRotator(0.f, P.Yaw, 0.f), P.Centre);
		AProceduralPond* Pond = World->SpawnActorDeferred<AProceduralPond>(AProceduralPond::StaticClass(), SpawnXf, Owner, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Pond)
		{
			continue;
		}
		Pond->RadiusX = P.Rx;
		Pond->RadiusY = P.Ry;
		Pond->Depth = Depth;
		Pond->NavPadding = NavPadding;
		Pond->Index = i;
		Pond->Collection = PondCollection;
		Pond->FinishSpawning(SpawnXf);
		Ponds.Add(Pond);
	}
	UE_LOG(LogTemp, Log, TEXT("[Ponds] %d water bodies (%d rivers) of %d wanted, %d slots."), Bodies, Rivers, Wanted, Planned.Num());

	if (Ponds.Num() > 0)
	{
		NavCheckRound = 0;
		World->GetTimerManager().SetTimer(NavCheckTimer, this, &UProceduralPondComponent::CheckNavBlocked, 10.f, false);
	}
}

void UProceduralPondComponent::CheckNavBlocked()
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!NavSys)
	{
		return;
	}
	// The navmesh rebuilds the tiles under the ponds in the background - with many dirty tiles at match start
	// that can take a while. Ask again before calling it a failure.
	const bool bFinalRound = ++NavCheckRound >= 9 || !NavSys->IsNavigationBuildInProgress();
	int32 Blocked = 0;
	TArray<int32> Open;
	for (const AProceduralPond* Pond : Ponds)
	{
		if (!IsValid(Pond)) continue;
		FNavLocation OnNav;
		// Small extent: only a navmesh right under the centre counts, not the shore.
		const bool bOnNav = NavSys->ProjectPointToNavigation(Pond->GetActorLocation(), OnNav, FVector(30.f, 30.f, 200.f));
		if (bOnNav)
		{
			Open.Add(Pond->Index);
		}
		else
		{
			++Blocked;
		}
	}
	if (Open.Num() > 0 && !bFinalRound)
	{
		World->GetTimerManager().SetTimer(NavCheckTimer, this, &UProceduralPondComponent::CheckNavBlocked, 10.f, false);
		return;
	}
	for (const int32 Open1 : Open)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Ponds] Pond %d: the navmesh is NOT blocked under the water (check %d)."), Open1, NavCheckRound);
	}
	UE_LOG(LogTemp, Log, TEXT("[Ponds] Navmesh blocked under %d of %d ponds (check %d, nav build running: %d)."),
		Blocked, Ponds.Num(), NavCheckRound, NavSys->IsNavigationBuildInProgress() ? 1 : 0);
}

void UProceduralPondComponent::ClearPonds()
{
	for (AProceduralPond* Pond : Ponds)
	{
		if (IsValid(Pond))
		{
			Pond->Destroy();
		}
	}
	Ponds.Reset();
}

bool UProceduralPondComponent::IsInPond(const FVector& Location, float Margin) const
{
	for (const AProceduralPond* Pond : Ponds)
	{
		if (IsValid(Pond) && Pond->ContainsPoint2D(Location, Margin))
		{
			return true;
		}
	}
	return false;
}
