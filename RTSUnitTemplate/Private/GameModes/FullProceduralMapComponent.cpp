// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "GameModes/FullProceduralMapComponent.h"
#include "GameModes/RTSProceduralTerrain.h"
#include "GameModes/RTSGameModeBase.h"
#include "GameStates/ResourceGameState.h"
#include "Core/RTSGround.h"
#include "Core/UnitData.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/UnitBase.h"
#include "PlayerStart/PlayerStartBase.h"
#include "Actors/Waypoint.h"
#include "Actors/WorkArea.h"
#include "Actors/ResourceClusterMarker.h"
#include "Actors/AIExpansionSite.h"
#include "Actors/BossWaveSpawner.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Math/RandomStream.h"

const FName UFullProceduralMapComponent::RelocateTag(TEXT("ProceduralRelocate"));

namespace
{
	/** Height of the ground (landscape or RTSGround actor) under P; false if there is none. */
	bool FindGroundZ(UWorld* World, const FVector& P, float& OutZ)
	{
		TArray<FHitResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(FullProceduralGround), true);
		World->LineTraceMultiByObjectType(Hits, FVector(P.X, P.Y, P.Z + 20000.f), FVector(P.X, P.Y, P.Z - 20000.f),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params);
		for (const FHitResult& Hit : Hits)
		{
			// Not the hidden old landscape (its collision can still answer during the match start).
			if (RTSGround::IsGround(Hit.GetActor()) && !Hit.GetActor()->IsHidden())
			{
				OutZ = Hit.ImpactPoint.Z;
				return true;
			}
		}
		return false;
	}

	const FVector NavExtent(150.f, 150.f, 800.f);
}

UFullProceduralMapComponent::UFullProceduralMapComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFullProceduralMapComponent::FindTerrainProviders(UWorld* World, TArray<AActor*>& Out)
{
	if (!World) return;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (IsValid(*It) && It->Implements<URTSProceduralTerrain>())
		{
			Out.Add(*It);
		}
	}
}

void UFullProceduralMapComponent::MoveActor(AActor* Actor, const FVector& Location)
{
	if (!IsValid(Actor)) return;
	if (USceneComponent* Root = Actor->GetRootComponent())
	{
		if (Root->Mobility != EComponentMobility::Movable)
		{
			Root->SetMobility(EComponentMobility::Movable);
		}
	}
	Actor->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
}

bool UFullProceduralMapComponent::IsRelocatable(const AActor* Actor) const
{
	if (!IsValid(Actor) || Actor->IsA<ABossWaveSpawner>()) return false;
	if (Actor->IsA<AUnitBase>() || Actor->IsA<APlayerStart>() || Actor->IsA<AWaypoint>() || Actor->IsA<AWorkArea>()
		|| Actor->IsA<AResourceClusterMarker>() || Actor->IsA<AAIExpansionSite>() || Actor->ActorHasTag(RelocateTag))
	{
		return true;
	}
	for (const TSubclassOf<AActor>& Class : ExtraRelocateClasses)
	{
		if (*Class && Actor->IsA(Class)) return true;
	}
	return false;
}

bool UFullProceduralMapComponent::PathExists(const FVector& From, const FVector& To) const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	const ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
	if (!NavData) return true;
	FNavLocation A, B;
	if (!NavSys->ProjectPointToNavigation(From, A, NavExtent) || !NavSys->ProjectPointToNavigation(To, B, NavExtent))
	{
		return false;
	}
	FPathFindingQuery Query(GetOwner(), *NavData, A.Location, B.Location);
	Query.bAllowPartialPaths = false;
	return NavSys->TestPathSync(Query, EPathFindingMode::Regular);
}

bool UFullProceduralMapComponent::IsReachable(const FVector& P) const
{
	return HumanBases.Num() == 0 || PathExists(HumanBases[0], P);
}

bool UFullProceduralMapComponent::IsOpenGround(const FVector& P, float Radius, float MaxHeightDifference) const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!NavSys) return false;
	// Two rings (full and half radius), so a hole or a step inside the circle fails as well.
	for (int32 Ring = 0; Ring < 2; ++Ring)
	{
		const float R = Ring == 0 ? Radius : 0.5f * Radius;
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector Q = P + FVector(1.f, 0.f, 0.f).RotateAngleAxis(45.f * i + 22.5f * Ring, FVector::UpVector) * R;
			FNavLocation OnNav;
			if (!NavSys->ProjectPointToNavigation(Q, OnNav, FVector(60.f, 60.f, 400.f))
				|| FVector::Dist2D(OnNav.Location, Q) > 40.f
				|| FMath::Abs(OnNav.Location.Z - P.Z) > MaxHeightDifference)
			{
				return false;
			}
			FVector Hit;
			if (UNavigationSystemV1::NavigationRaycast(World, P, OnNav.Location, Hit))
			{
				return false;
			}
		}
	}
	return true;
}

bool UFullProceduralMapComponent::FindReachableNavPoint(const FVector& Desired, FVector& Out, float OpenRadius) const
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!NavSys) return false;
	static const float Radii[] = { 0.f, 400.f, 800.f, 1300.f, 1900.f, 2600.f, 3500.f, 4600.f, 6000.f, 8000.f };
	for (const float R : Radii)
	{
		const int32 Directions = R <= 0.f ? 1 : 12;
		for (int32 i = 0; i < Directions; ++i)
		{
			const FVector Q = Desired + FVector(1.f, 0.f, 0.f).RotateAngleAxis(30.f * i, FVector::UpVector) * R;
			FNavLocation OnNav;
			if (!NavSys->ProjectPointToNavigation(Q, OnNav, FVector(300.f, 300.f, 5000.f)))
			{
				continue;
			}
			if (OpenRadius > 0.f && !IsOpenGround(OnNav.Location, OpenRadius, 120.f))
			{
				continue;
			}
			if (!IsReachable(OnNav.Location))
			{
				continue;
			}
			Out = OnNav.Location;
			return true;
		}
	}
	return false;
}

FVector UFullProceduralMapComponent::RemapSpawnOffset(const FVector& Offset) const
{
	if (SpawnPoints.Num() == 0)
	{
		return Offset;
	}
	// The same table position always gets the same point: rows that spawned together keep spawning together.
	const uint32 Key = HashCombine(GetTypeHash(FMath::RoundToInt(Offset.X / 10.f)), GetTypeHash(FMath::RoundToInt(Offset.Y / 10.f)));
	return SpawnPoints[Key % uint32(SpawnPoints.Num())] + FVector(0.f, 0.f, SpawnHeight);
}

bool UFullProceduralMapComponent::PrepareMap()
{
	UWorld* World = GetWorld();
	if (!bEnabled || bTried || !World || World->GetNetMode() == NM_Client)
	{
		return bActive;
	}
	bTried = true;   // once per match, also when it fails (AResourceGameMode and ARTSGameModeBase both call this)

	TArray<AActor*> Providers;
	FindTerrainProviders(World, Providers);
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (Providers.Num() == 0 || !NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FullProcedural] %s - the map stays as it is."),
			Providers.Num() == 0 ? TEXT("no actor with IRTSProceduralTerrain in the level") : TEXT("no navigation system"));
		return false;
	}
	Provider = Providers[0];

	// 1) The groups, measured on the old ground: every main base (the building next to a player start,
	//    CollectAndStore first) and what stands around it. Heights are kept as height above the ground.
	struct FMember
	{
		TWeakObjectPtr<AActor> Actor;
		FVector2D Offset;
		float Above = 0.f;
	};
	struct FGroup
	{
		TWeakObjectPtr<ABuildingBase> Base;
		FVector OldPos;
		FVector NewPos = FVector::ZeroVector;
		bool bHuman = false;
		bool bPlaced = false;
		TArray<FMember> Members;
		TArray<TWeakObjectPtr<APlayerStart>> Starts;
	};
	TArray<FGroup> Groups;

	auto HeightAbove = [&](const AActor* Actor) -> float
	{
		float GroundZ = 0.f;
		const FVector L = Actor->GetActorLocation();
		return FindGroundZ(World, L, GroundZ) ? FMath::Clamp(float(L.Z - GroundZ), -100.f, 5000.f) : 100.f;
	};

	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
		ABuildingBase* Best = nullptr;
		float BestScore = FMath::Square(MainBaseReach);
		for (TActorIterator<ABuildingBase> B(World); B; ++B)
		{
			// A CollectAndStore building wins over any other one in reach.
			const float DistSq = FVector::DistSquared2D(B->GetActorLocation(), Start->GetActorLocation())
				+ (B->BaseType == EBaseType::CollectAndStore ? 0.f : FMath::Square(MainBaseReach));
			if (DistSq < BestScore)
			{
				BestScore = DistSq;
				Best = *B;
			}
		}
		if (!Best)
		{
			for (TActorIterator<ABuildingBase> B(World); B; ++B)
			{
				const float DistSq = FVector::DistSquared2D(B->GetActorLocation(), Start->GetActorLocation());
				if (DistSq < FMath::Square(MainBaseReach) && (!Best || DistSq < FVector::DistSquared2D(Best->GetActorLocation(), Start->GetActorLocation())))
				{
					Best = *B;
				}
			}
		}
		const APlayerStartBase* StartBase = Cast<APlayerStartBase>(Start);
		const bool bHuman = !(StartBase && StartBase->bIsAi);
		FGroup* Group = Best ? Groups.FindByPredicate([Best](const FGroup& G) { return G.Base.Get() == Best; }) : nullptr;
		if (Best && !Group)
		{
			Group = &Groups.AddDefaulted_GetRef();
			Group->Base = Best;
			Group->OldPos = Best->GetActorLocation();
		}
		if (Group)
		{
			Group->bHuman |= bHuman;
			Group->Starts.Add(Start);
		}
	}
	// Human bases first: they pick their high ground, the others keep their distance to them.
	Groups.StableSort([](const FGroup& A, const FGroup& B) { return A.bHuman && !B.bHuman; });

	TArray<TPair<TWeakObjectPtr<AActor>, float>> Loose;
	TArray<TWeakObjectPtr<ABossWaveSpawner>> BossSpawners;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (ABossWaveSpawner* Boss = Cast<ABossWaveSpawner>(Actor))
		{
			BossSpawners.Add(Boss);
			continue;
		}
		if (!IsRelocatable(Actor) || Actor->IsA<APlayerStart>())
		{
			continue;
		}
		FGroup* Nearest = nullptr;
		float NearestDistSq = FMath::Square(RelocateRadius);
		for (FGroup& G : Groups)
		{
			const float DistSq = FVector::DistSquared2D(G.OldPos, Actor->GetActorLocation());
			if (DistSq <= NearestDistSq)
			{
				NearestDistSq = DistSq;
				Nearest = &G;
			}
		}
		if (Nearest)
		{
			FMember& M = Nearest->Members.AddDefaulted_GetRef();
			M.Actor = Actor;
			M.Offset = FVector2D(Actor->GetActorLocation() - Nearest->OldPos);
			M.Above = HeightAbove(Actor);
		}
		else
		{
			Loose.Emplace(Actor, HeightAbove(Actor));
		}
	}
	struct FStartInfo { TWeakObjectPtr<APlayerStart> Start; FVector2D Offset; float Above; };
	TArray<TArray<FStartInfo>> StartInfos;
	for (const FGroup& G : Groups)
	{
		// The player starts go over the base: their spread around their own centre stays, the centre moves onto the base.
		FVector2D Centre = FVector2D::ZeroVector;
		for (const TWeakObjectPtr<APlayerStart>& S : G.Starts) Centre += FVector2D(S->GetActorLocation());
		Centre /= float(FMath::Max(1, G.Starts.Num()));
		TArray<FStartInfo>& Infos = StartInfos.AddDefaulted_GetRef();
		for (const TWeakObjectPtr<APlayerStart>& S : G.Starts)
		{
			Infos.Add({ S, FVector2D(S->GetActorLocation()) - Centre, HeightAbove(S.Get()) });
		}
	}

	// 2) The terrain, then the navmesh on it (blocking - everything below queries it).
	const int32 WantedSeed = Seed != 0 ? Seed
		: int32((uint32(FPlatformTime::Cycles64()) ^ uint32(FDateTime::Now().GetTicks())) & 0x7fffffff) | 1;
	UsedSeed = IRTSProceduralTerrain::Execute_GenerateProceduralTerrain(Provider, WantedSeed, true);
	if (UsedSeed == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FullProcedural] the terrain could not be built - the map stays as it is."));
		return false;
	}
	const double NavStart = FPlatformTime::Seconds();
	NavSys->ProcessPendingOctreeUpdates();
	NavSys->Build();
	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	if (!NavData)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FullProcedural] no navmesh after the build."));
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[FullProcedural] seed %d, navmesh built in %.2f s."), UsedSeed, FPlatformTime::Seconds() - NavStart);

	// 3) Navmesh samples on a grid; their heights give the high ground.
	FRandomStream Random(UsedSeed ^ 0x2545F491);
	const FBox Bounds = NavData->GetBounds();
	TArray<FVector> Samples;
	float MinZ = TNumericLimits<float>::Max(), MaxZ = -TNumericLimits<float>::Max();
	if (Bounds.IsValid)
	{
		const float Step = FMath::Max(200.f, SampleSpacing);
		const FVector ProbeExtent(Step * 0.3f, Step * 0.3f, Bounds.GetSize().Z + 1000.f);
		for (float X = Bounds.Min.X + Step * 0.5f; X < Bounds.Max.X; X += Step)
		{
			for (float Y = Bounds.Min.Y + Step * 0.5f; Y < Bounds.Max.Y; Y += Step)
			{
				FNavLocation OnNav;
				if (NavSys->ProjectPointToNavigation(FVector(X, Y, Bounds.GetCenter().Z), OnNav, ProbeExtent))
				{
					Samples.Add(OnNav.Location);
					MinZ = FMath::Min(MinZ, float(OnNav.Location.Z));
					MaxZ = FMath::Max(MaxZ, float(OnNav.Location.Z));
				}
			}
		}
	}
	if (Samples.Num() < 8)
	{
		UE_LOG(LogTemp, Warning, TEXT("[FullProcedural] only %d navmesh samples - no placement."), Samples.Num());
		return false;
	}
	// Random order once; every pick below walks it from the front.
	for (int32 i = Samples.Num() - 1; i > 0; --i)
	{
		Samples.Swap(i, Random.RandRange(0, i));
	}
	const float HighZ = MinZ + (MaxZ - MinZ) * (1.f - FMath::Clamp(TopPlateauShare, 0.05f, 1.f)) - 1.f;

	// The first base must lie in the big connected part of the map, not on a cut-off plateau or a scrap of navmesh
	// on a slope. Anchor of that part: an open spot near the middle that reaches most of a few other open spots.
	TArray<FVector> OpenSamples;
	for (const FVector& S : Samples)
	{
		if (OpenSamples.Num() >= 24) break;
		if (IsOpenGround(S, 500.f, 150.f)) OpenSamples.Add(S);
	}
	OpenSamples.Sort([Centre = Bounds.GetCenter()](const FVector& A, const FVector& B)
	{
		return FVector::DistSquared2D(A, Centre) < FVector::DistSquared2D(B, Centre);
	});
	FVector MainAnchor = OpenSamples.Num() ? OpenSamples[0] : Samples[0];
	int32 BestReach = -1;
	for (int32 a = 0; a < FMath::Min(6, OpenSamples.Num()); ++a)
	{
		int32 Reach = 0;
		for (int32 b = 0; b < OpenSamples.Num(); b += 2)
		{
			Reach += b != a && PathExists(OpenSamples[a], OpenSamples[b]) ? 1 : 0;
		}
		if (Reach > BestReach)
		{
			BestReach = Reach;
			MainAnchor = OpenSamples[a];
		}
		if (Reach * 3 >= OpenSamples.Num()) break;   // reaches most of them (every second one is tested): good enough
	}
	auto IsInMainRegion = [&](const FVector& P) -> bool
	{
		return PathExists(MainAnchor, P);
	};

	FRTSProceduralMapState State;
	State.Seed = UsedSeed;
	auto Place = [&](AActor* Actor, const FVector& Location)
	{
		MoveActor(Actor, Location);
		State.Relocations.Add({ Actor, Location });
	};
	auto GroundAt = [&](const FVector2D& XY, float FallbackZ) -> float
	{
		float Z = FallbackZ;
		FindGroundZ(World, FVector(XY, FallbackZ), Z);
		return Z;
	};

	// 4) The main bases. bPreferHighestLevel: the samples from the top down (random order within 50 uu of height).
	TArray<FVector> Candidates = Samples;
	if (bPreferHighestLevel)
	{
		Candidates.StableSort([](const FVector& A, const FVector& B) { return FMath::FloorToInt(A.Z / 50.f) > FMath::FloorToInt(B.Z / 50.f); });
	}
	TArray<FVector> Placed;
	for (int32 g = 0; g < Groups.Num(); ++g)
	{
		FGroup& G = Groups[g];
		if (!G.Base.IsValid()) continue;
		bool bFound = false;
		// Round 0..2: high ground, the full distance and then less; round 3: any height, a third of the distance;
		// round 4: anywhere apart from the other bases' open ground.
		for (int32 Round = 0; Round < 5 && !bFound; ++Round)
		{
			const float MinDist = Round < 3 ? MinBaseDistance * FMath::Pow(0.7f, float(Round))
				: (Round == 3 ? MinBaseDistance * 0.33f : 2.f * BaseOpenRadius);
			int32 Tests = 0;
			for (const FVector& C : Candidates)
			{
				if (Round < 3 && C.Z < HighZ) continue;
				bool bFar = true;
				for (const FVector& O : Placed) bFar &= FVector::DistSquared2D(O, C) >= FMath::Square(MinDist);
				if (!bFar) continue;
				if (!IsOpenGround(C, BaseOpenRadius * (Round < 3 ? 1.f : 0.6f), BaseMaxHeightDifference)) continue;
				if (++Tests > 200) break;
				if (Placed.Num() == 0 ? !IsInMainRegion(C) : !PathExists(Placed[0], C)) continue;
				G.NewPos = C;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			UE_LOG(LogTemp, Warning, TEXT("[FullProcedural] no spot for main base %s - it stays."), *G.Base->GetName());
			continue;
		}
		G.bPlaced = true;
		Placed.Add(G.NewPos);
		if (G.bHuman) HumanBases.Add(G.NewPos);
	}
	if (HumanBases.Num() == 0 && Placed.Num() > 0)
	{
		// No human base (an AI-only map): the first base is the anchor of the reachability test.
		HumanBases.Add(Placed[0]);
	}
	// How much of the navmesh the base reaches - the procedural resources scale their number with it.
	if (HumanBases.Num() > 0)
	{
		int32 Tested = 0, Reached = 0;
		const int32 Stride = FMath::Max(1, Samples.Num() / 80);
		for (int32 i = 0; i < Samples.Num(); i += Stride)
		{
			++Tested;
			Reached += PathExists(HumanBases[0], Samples[i]) ? 1 : 0;
		}
		ReachableShare = Tested > 0 ? float(Reached) / float(Tested) : 1.f;
	}

	// 5) Everything of the groups moves with its base.
	for (int32 g = 0; g < Groups.Num(); ++g)
	{
		FGroup& G = Groups[g];
		if (!G.bPlaced || !G.Base.IsValid()) continue;
		ABuildingBase* Base = G.Base.Get();
		for (const FMember& M : G.Members)
		{
			AActor* Actor = M.Actor.Get();
			if (!Actor) continue;
			FVector2D XY = FVector2D(G.NewPos) + M.Offset;
			if (Actor == Base)
			{
				XY = FVector2D(G.NewPos);
			}
			FVector Target(XY, GroundAt(XY, G.NewPos.Z) + M.Above);
			if (Actor != Base)
			{
				// Off the navmesh or cut off (a cliff, another plateau): the nearest reachable navmesh instead.
				FNavLocation OnNav;
				const bool bOnNav = NavSys->ProjectPointToNavigation(Target, OnNav, FVector(120.f, 120.f, M.Above + 400.f))
					&& FVector::Dist2D(OnNav.Location, Target) < 100.f;
				if (!bOnNav || !PathExists(G.NewPos, OnNav.Location))
				{
					FVector Free;
					const float Open = Actor->IsA<ABuildingBase>() ? 400.f : (Actor->IsA<AAIExpansionSite>() ? 500.f : 0.f);
					if (FindReachableNavPoint(FVector(XY, G.NewPos.Z), Free, Open) || FindReachableNavPoint(FVector(XY, G.NewPos.Z), Free, 0.f))
					{
						Target = FVector(Free.X, Free.Y, GroundAt(FVector2D(Free), Free.Z) + M.Above);
					}
				}
			}
			Place(Actor, Target);
		}
		for (const FStartInfo& Info : StartInfos[g])
		{
			if (APlayerStart* Start = Info.Start.Get())
			{
				const FVector2D XY = FVector2D(G.NewPos) + Info.Offset;
				Place(Start, FVector(XY, GroundAt(XY, G.NewPos.Z) + Info.Above));
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[FullProcedural] main base %s%s: %s -> %s, %d actors along."), *Base->GetName(),
			G.bHuman ? TEXT(" (human)") : TEXT(""), *G.OldPos.ToCompactString(), *G.NewPos.ToCompactString(), G.Members.Num());
	}

	// 6) The other placed actors: same spot, but on reachable navmesh.
	int32 LooseMoved = 0;
	for (const TPair<TWeakObjectPtr<AActor>, float>& L : Loose)
	{
		AActor* Actor = L.Key.Get();
		if (!Actor) continue;
		FVector Free;
		const FVector Old = Actor->GetActorLocation();
		const float Open = Actor->IsA<AAIExpansionSite>() ? 500.f : 0.f;
		if (FindReachableNavPoint(Old, Free, Open) || (Open > 0.f && FindReachableNavPoint(Old, Free, 0.f)))
		{
			Place(Actor, FVector(Free.X, Free.Y, GroundAt(FVector2D(Free), Free.Z) + L.Value));
			++LooseMoved;
		}
	}

	// 7) Spawn points: far from every human base, open, reachable, apart from each other.
	SpawnPoints.Reset();
	const int32 WantedPoints = FMath::Clamp(SpawnPointCount, 1, 12);
	for (int32 Round = 0; Round < 5 && SpawnPoints.Num() < WantedPoints; ++Round)
	{
		const float MinDist = SpawnMinDistance * FMath::Pow(0.8f, float(Round));
		const float Spacing = SpawnPointSpacing * FMath::Pow(0.8f, float(Round));
		int32 Tests = 0;
		for (const FVector& C : Samples)
		{
			if (SpawnPoints.Num() >= WantedPoints) break;
			bool bOk = true;
			for (const FVector& H : HumanBases) bOk &= FVector::DistSquared2D(H, C) >= FMath::Square(MinDist);
			for (const FVector& O : SpawnPoints) bOk &= FVector::DistSquared2D(O, C) >= FMath::Square(Spacing);
			for (const FVector& O : Placed) bOk &= FVector::DistSquared2D(O, C) >= FMath::Square(2500.f);
			if (!bOk || !IsOpenGround(C, 600.f, 150.f)) continue;
			if (++Tests > 80) break;
			if (!IsReachable(C)) continue;
			SpawnPoints.Add(C);
		}
	}
	if (bMoveBossSpawners && SpawnPoints.Num() > 0)
	{
		for (int32 i = 0; i < BossSpawners.Num(); ++i)
		{
			if (ABossWaveSpawner* Boss = BossSpawners[i].Get())
			{
				Place(Boss, SpawnPoints[i % SpawnPoints.Num()] + FVector(0.f, 0.f, 50.f));
			}
		}
	}

	// 8) Waypoints: those of the spawn tables follow the human main base; every following one jumps to its target now.
	TSet<FString> TableTags;
	if (const ARTSGameModeBase* GameMode = Cast<ARTSGameModeBase>(GetOwner()))
	{
		for (UDataTable* Table : GameMode->UnitSpawnParameters)
		{
			if (!Table) continue;
			Table->ForeachRow<FUnitSpawnParameter>(TEXT("FullProcedural"), [&TableTags](const FName&, const FUnitSpawnParameter& Row)
			{
				if (!Row.WaypointTag.IsEmpty()) TableTags.Add(Row.WaypointTag);
			});
		}
	}
	ABuildingBase* HumanMainBase = nullptr;
	for (const FGroup& G : Groups)
	{
		if (G.bHuman && G.bPlaced && G.Base.IsValid()) { HumanMainBase = G.Base.Get(); break; }
	}
	for (TActorIterator<AWaypoint> It(World); It; ++It)
	{
		AWaypoint* Waypoint = *It;
		if (bWaypointsFollowHumanBase && HumanMainBase && TableTags.Contains(Waypoint->Tag))
		{
			Waypoint->FollowCharacter = HumanMainBase;
		}
		if (IsValid(Waypoint->FollowCharacter))
		{
			Place(Waypoint, Waypoint->FollowCharacter->GetActorLocation());
		}
	}

	bActive = true;
	if (AResourceGameState* GS = World->GetGameState<AResourceGameState>())
	{
		GS->ProceduralMap = State;
		GS->bProceduralTerrainBuilt = true;
	}
	FString Points;
	for (const FVector& P : SpawnPoints) Points += P.ToCompactString() + TEXT(" ");
	UE_LOG(LogTemp, Log, TEXT("[FullProcedural] ready: %d samples (height %.0f..%.0f), %d bases placed, %.0f%% of the navmesh reachable, %d other actors moved, spawn points %s, %d relocations sent."),
		Samples.Num(), MinZ, MaxZ, Placed.Num(), ReachableShare * 100.f, LooseMoved, *Points, State.Relocations.Num());
	return true;
}

void UFullProceduralMapComponent::FinishMap()
{
	if (bActive && IsValid(Provider))
	{
		IRTSProceduralTerrain::Execute_OnProceduralMapReady(Provider);
	}
}
