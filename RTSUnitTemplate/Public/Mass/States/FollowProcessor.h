// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "FollowProcessor.generated.h"

struct FMassEntityManager;

/**
 * Dedicated CLIENT-side processor that is the SINGLE OWNER of a FOLLOWING unit's local move-prediction
 * (FMassClientPredictionFragment.Location / .bHasData).
 *
 * Background: follower prediction used to be written by many places at once (the move-command issuer, the
 * SyncAITarget stale-clear, the mover convergence-clear, IdleState/RunState branches). They raced each other -
 * most visibly, a move command pins Pred to the clicked point and a 0.6s command-grace window blocks the only
 * per-tick corrector, so a unit that is STILL following keeps Pred at the stale move target while its formation
 * slot is elsewhere (StaleDist up to thousands of cm).
 *
 * This processor runs every PrePhysics tick AFTER UnitActorToFragmentSyncProcessor (so FriendlyTargetEntity is
 * mirrored) and BEFORE UnitMovementProcessor (so the mover reads the value we just wrote). For each active
 * follower in Run it computes the unit's STABLE formation SLOT (RTSUnitUtils::ComputeFollowSlotOffset, seeded by
 * the replicated AUnitBase::UnitIndex, leader-relative) and pins Pred.Location to it with an ARRIVE profile
 * (speed scales to 0 over a FollowRadius-wide zone) so it stops smoothly AT its slot. Because the SAME UnitIndex
 * seed produces the IDENTICAL slot on the server (RunStateProcessor::ExecuteServer) the server-landed position
 * equals the client-predicted one -> the reconciler has nothing to fight (no snap-back). The slot is also mirrored
 * into MoveTarget.Center so the RunState/Idle arrival checks stay slot-based after the mover clears bHasData. When
 * the leader stops and the follower is at its slot it settles, and the RunState-arrival -> Idle path parks it.
 *
 * CVar-gated (RTS.FollowProcessor): with it off the legacy path is unchanged.
 */
UCLASS()
class RTSUNITTEMPLATE_API UFollowProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UFollowProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	// Client: owns the local move-prediction (FMassClientPredictionFragment) of each follower.
	void ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context);
	// Server / Standalone: authoritative follow target (FMassMoveTargetFragment.Center). Placeholder for now -
	// the authoritative slot is still driven by Run/IdleStateProcessor; it will migrate here once those paths
	// are gated off (kept separate so server- and client-specific follow code never tangle).
	void ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context);

	FMassEntityQuery EntityQuery;
};
