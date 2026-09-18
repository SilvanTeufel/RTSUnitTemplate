// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/ZeroStateTagRecoveryProcessor.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"                    // FTransformFragment (not required, kept for parity)
#include "MassMovementFragments.h"                  // FMassVelocityFragment
#include "MassNavigationFragments.h"                // FMassMoveTargetFragment (replicated move slot)
#include "Mass/UnitMassTag.h"                       // FMassAIStateFragment + FMassClientPredictionFragment + all FMassState* tags
#include "Mass/Replication/ReplicationSettings.h"   // RTSReplicationSettings
#include "ProfilingDebugging/CsvProfiler.h"

UZeroStateTagRecoveryProcessor::UZeroStateTagRecoveryProcessor(): EntityQuery()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;          // client (primary) + server + standalone
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;            // after the PrePhysics replication apply flushed
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = false;
}

void UZeroStateTagRecoveryProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);

	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	// CLIENT decision inputs. Velocity is unusable on the client here (see Execute).
	// Both Optional, so archetype matching is unchanged - nothing recovered before stops being recovered.
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

	// Not-yet-initialized / transient exclusions.
	EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateNeedsInitialKickTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);

	// Match ONLY the freeze archetype: NONE of the tags ComputeState maps to a real (non-None) state.
	// FMassStateDeadTag is included -> corpses are never recovered/resurrected.
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStatePatrolIdleTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateChargingTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateRootedTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateEvasionTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateContinuousAttackTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);        // ComputeState -> Aim
	EntityQuery.AddTagRequirement<FRunAnimationTag>(EMassFragmentPresence::None);             // ComputeState -> AnimationState
	EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateGoToRepairTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::None);

	EntityQuery.RegisterWithProcessor(*this);
}

void UZeroStateTagRecoveryProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

	// Siehe mass_scopes: macht diesen Prozessor als Spalte Exclusive/UZeroStateTagRecoveryProcessor im CSV sichtbar.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UZeroStateTagRecoveryProcessor);

	// Only relevant to the Mass replication path (mirrors UnitClientTagSyncProcessor / ClientReplicationProcessor).
	if (RTSReplicationSettings::GetReplicationMode() != RTSReplicationSettings::Mass)
	{
		return;
	}

	TimeSinceLastRun += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastRun < ExecutionInterval)
	{
		return;
	}
	TimeSinceLastRun = 0.f;

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	const float MinAge = MinAgeSeconds;
	const float MoveSq = MovingSpeedSq;

	// CLIENT: FMassVelocityFragment cannot decide this. Every processor that WRITES it requires one of
	// the very state tags this archetype is missing by definition, so the value is a frozen leftover that
	// UClientReplicationProcessor then only ever damps down (*=0.8f, *=0.05f). It is an OUTPUT of the state
	// we are trying to infer - the decision would be circular. Decide from the REPLICATED move slot
	// instead: the same (DesiredSpeed > 10 || Pred.bHasData) test the reconciler itself uses. MoveTarget is
	// safe to trust here because the bubble apply forces DesiredSpeed = 0 whenever the server reports no
	// move slot.
	// SERVER / STANDALONE keep the original velocity rule - there velocity IS authoritative.
	const bool bIsNetClient = World->IsNetMode(NM_Client);

	EntityQuery.ForEachEntityChunk(Context,
		[Now, MinAge, MoveSq, bIsNetClient](FMassExecutionContext& ChunkContext)
	{
		const int32 Num = ChunkContext.GetNumEntities();
		const TConstArrayView<FMassAIStateFragment> States = ChunkContext.GetFragmentView<FMassAIStateFragment>();
		const TConstArrayView<FMassVelocityFragment> Vels = ChunkContext.GetFragmentView<FMassVelocityFragment>();
		const bool bHasVel = (Vels.Num() == Num);
		const TConstArrayView<FMassMoveTargetFragment> Moves = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
		const bool bHasMove = (Moves.Num() == Num);
		const TConstArrayView<FMassClientPredictionFragment> Preds = ChunkContext.GetFragmentView<FMassClientPredictionFragment>();
		const bool bHasPred = (Preds.Num() == Num);

		for (int32 i = 0; i < Num; ++i)
		{
			// Skip the spawn/init window so we never race a one-frame deferred tag swap.
			if (Now - States[i].BirthTime < MinAge)
			{
				continue;
			}

			const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

			bool bMoving;
			if (bIsNetClient)
			{
				// Mirrors the reconciler's own bIsMoving test, so the tag we inject agrees with the pass
				// that will immediately re-evaluate it. A live client prediction counts as moving:
				// ApplyReplicatedTagBits refuses to stamp Idle while bPredicting for the same reason - a
				// wrong Idle turns on bIsStationaryAttack and the hard velocity brake, killing the
				// prediction and making the wrong state self-reinforcing.
				bMoving = (bHasMove && Moves[i].DesiredSpeed.Get() > 10.f) ||
				          (bHasPred && Preds[i].bHasData);
			}
			else
			{
				bMoving = bHasVel && (Vels[i].Value.SizeSquared() > MoveSq);
			}

			if (bMoving)
			{
				ChunkContext.Defer().AddTag<FMassStateRunTag>(Entity);
			}
			else
			{
				ChunkContext.Defer().AddTag<FMassStateIdleTag>(Entity);
			}
		}
	});
}
