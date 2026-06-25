// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/States/FollowProcessor.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"        // FTransformFragment
#include "MassMovementFragments.h"      // FMassVelocityFragment
#include "MassNavigationFragments.h"    // FMassMoveTargetFragment
#include "MassActorSubsystem.h"         // FMassActorFragment
#include "./Mass/UnitMassTag.h"           // FMassClientPredictionFragment, FMassAITargetFragment, FMassCombatStatsFragment, FMassAIStateFragment, state tags
#include "Mass/Traits/UnitReplicationFragments.h" // FUnitReplicatedTransformFragment (leader's authoritative server pos on the client)
#include "Characters/Unit/UnitBase.h"   // AUnitBase (UnitIndex)
#include "Core/RTSUnitUtils.h"          // ComputeFollowSlotOffset
#include "HAL/IConsoleManager.h"
#include "Runtime/Mass/MassCore/Public/Mass/EntityFragments.h"
#include "Runtime/MassEntity/Public/MassProcessingTypes.h"



UFollowProcessor::UFollowProcessor() : EntityQuery()
{
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks; // same group as UnitMovementProcessor
	ExecutionOrder.ExecuteAfter.Add(TEXT("UnitActorToFragmentSyncProcessor")); // FriendlyTargetEntity + follow tag mirrored first
	ExecutionOrder.ExecuteBefore.Add(TEXT("UnitMovementProcessor"));            // mover reads Pred AFTER we write it
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true; // reads AUnitBase via the actor fragment
}

void UFollowProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);

	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadWrite); // mirror the slot into Center so RunState/Idle arrival stays slot-based even after the mover clears bHasData
	EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassClientPredictionFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);

	EntityQuery.AddTagRequirement<FUnitMassTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::All);    // only while moving in Run

	EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateCastingTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateBuildTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateResourceExtractionTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateRepairTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateStopMovementTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassRotateToMouseTag>(EMassFragmentPresence::None);

	EntityQuery.RegisterWithProcessor(*this);
}

void UFollowProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

	UWorld* World = Context.GetWorld();
	if (!World)
	{
		return;
	}

	if (World->IsNetMode(NM_Client))
	{
		ExecuteClient(EntityManager, Context);
	}
	else
	{
		ExecuteServer(EntityManager, Context);
	}
}

void UFollowProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{

}

void UFollowProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Authoritative follow (MoveTarget.Center = the SAME per-unit slot) is driven by Run/IdleStateProcessor::
	// ExecuteServer using RTSUnitUtils::ComputeFollowSlotOffset with the SAME UnitIndex -> identical slot -> the
	// server lands each unit where the client predicts -> reconciler error ~0.
}
