// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/Avoidance/AttackPauseClumpSeparationProcessor.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"

namespace
{
	struct FClumpUnitInfo
	{
		FMassEntityHandle Entity;
		FVector Location = FVector::ZeroVector;
		int32 TeamId = INDEX_NONE;
		float CapsuleRadius = 0.f;
		FMassEntityHandle Target;
	};

	static FVector Horizontal(const FVector& V)
	{
		return FVector(V.X, V.Y, 0.f);
	}
}

UAttackPauseClumpSeparationProcessor::UAttackPauseClumpSeparationProcessor()
	: AttackQuery()
	, PauseQuery()
{
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Avoidance;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client);
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = false;
}

void UAttackPauseClumpSeparationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	AttackQuery.Initialize(EntityManager);
	AttackQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::All);
	AttackQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	AttackQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	AttackQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
	AttackQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	AttackQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	AttackQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	// We don't want dead or stopped entities
	AttackQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	AttackQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);
	AttackQuery.RegisterWithProcessor(*this);

	PauseQuery.Initialize(EntityManager);
	PauseQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::All);
	PauseQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	PauseQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	PauseQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
	PauseQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	PauseQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	PauseQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	PauseQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	PauseQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);
	PauseQuery.RegisterWithProcessor(*this);

	BuildQuery.Initialize(EntityManager);
	BuildQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::All);
	BuildQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	BuildQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	BuildQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
	BuildQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	BuildQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);
	BuildQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	BuildQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	BuildQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);
	BuildQuery.RegisterWithProcessor(*this);
}

void UAttackPauseClumpSeparationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TimeSinceLastRun += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastRun < ExecutionInterval)
	{
		return;
	}
	TimeSinceLastRun -= ExecutionInterval;

	// 1) Gather all units in Attack/Pause state
	TArray<FClumpUnitInfo> Units;
	Units.Reserve(256);

	auto GatherFromQuery = [this, &Units](FMassExecutionContext& LocalContext)
	{
		const int32 Num = LocalContext.GetNumEntities();
		const auto Transforms = LocalContext.GetFragmentView<FTransformFragment>();
		const auto CombatStats = LocalContext.GetFragmentView<FMassCombatStatsFragment>();
		const auto Characs = LocalContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
		const auto Targets = LocalContext.GetFragmentView<FMassAITargetFragment>();

		for (int32 i = 0; i < Num; ++i)
		{
			FClumpUnitInfo Info;
			Info.Entity = LocalContext.GetEntity(i);
			Info.Location = Transforms[i].GetTransform().GetLocation();
			Info.TeamId = CombatStats[i].TeamId;
			Info.CapsuleRadius = Characs[i].CapsuleRadius;
			Info.Target = Targets[i].TargetEntity;
			Units.Add(Info);
		}
	};

	AttackQuery.ForEachEntityChunk(Context, GatherFromQuery);
	PauseQuery.ForEachEntityChunk(Context, GatherFromQuery);
	BuildQuery.ForEachEntityChunk(Context, GatherFromQuery);
	
	if (Units.Num() <= 1)
	{
		return;
	}

	// 2) Compute repulsion pairs
	TMap<FMassEntityHandle, FVector> AccumPush;
	AccumPush.Reserve(Units.Num());

	for (int32 a = 0; a < Units.Num(); ++a)
	{
		const FClumpUnitInfo& A = Units[a];
		for (int32 b = a + 1; b < Units.Num(); ++b)
		{
			const FClumpUnitInfo& B = Units[b];

			const bool bSameTeam = (A.TeamId == B.TeamId);

			// Only enforce the "same target" restriction for same-team separation
			if (bOnlySeparateWhenSameTarget && bSameTeam)
			{
				if (A.Target != B.Target || !A.Target.IsSet())
				{
					continue; // Only if same known target
				}
			}

			const FVector Delta = Horizontal(B.Location - A.Location);
			const float DistSq = Delta.SizeSquared();

			// Use different multipliers for friendly vs enemy pairs
			const float DistanceMultiplier = bSameTeam ? DistanceMultiplierFriendly : DistanceMultiplierEnemy;
			const float Desired = FMath::Max(1.f, A.CapsuleRadius + B.CapsuleRadius) * DistanceMultiplier; // center distance

			if (MaxCheckRadius > 0.f && DistSq > FMath::Square(MaxCheckRadius))
			{
				continue; // cull
			}

			if (DistSq < Desired * Desired)
			{
				const float Dist = FMath::Sqrt(FMath::Max(1.f, DistSq));
				const FVector DirAB = (Dist > KINDA_SMALL_NUMBER)
					? (Delta / Dist)
					: FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal2D();
				const float Overlap = Desired - Dist; // how much too close
				const float Strength = bSameTeam ? RepulsionStrengthFriendly : RepulsionStrengthEnemy;
				const FVector Push = DirAB * Overlap * Strength; // push B away from A along AB, and A opposite

				AccumPush.FindOrAdd(A.Entity) -= Push; // A pushed opposite
				AccumPush.FindOrAdd(B.Entity) += Push; // B pushed along dir
			}
		}
	}

	if (AccumPush.Num() == 0)
	{
		return;
	}

	// 3) Apply pushes back to Force fragment (XY only)
	auto ApplyToQuery = [&AccumPush](FMassExecutionContext& LocalContext)
	{
		const int32 Num = LocalContext.GetNumEntities();
		auto ForceList = LocalContext.GetMutableFragmentView<FMassForceFragment>();
		for (int32 i = 0; i < Num; ++i)
		{
			const FMassEntityHandle Entity = LocalContext.GetEntity(i);
			if (const FVector* Push = AccumPush.Find(Entity))
			{
				FMassForceFragment& Force = ForceList[i];
				const FVector PushXY = Horizontal(*Push);
				Force.Value += PushXY;
			}
		}
	};

	AttackQuery.ForEachEntityChunk(Context, ApplyToQuery);
	PauseQuery.ForEachEntityChunk(Context, ApplyToQuery);
	BuildQuery.ForEachEntityChunk(Context, ApplyToQuery);
}
