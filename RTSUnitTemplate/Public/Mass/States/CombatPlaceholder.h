// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Mass/UnitMassTag.h"
#include "Mass/Signals/MySignals.h"
#include "Characters/Unit/UnitBase.h"

namespace RTSUnitUtils
{
	/**
	 * Resolve the state a unit falls back to when it leaves combat.
	 *
	 * FMassAIStateFragment::PlaceholderSignal is the unit's memory of what it was doing before a
	 * fight pulled it away - MassActorBindingComponent seeds it with PatrolRandom for every unit
	 * that owns a waypoint. Every combat-exit path used to hardcode UnitSignals::Idle here, which
	 * erased that memory for good: nothing restores it afterwards, so the unit stopped wherever
	 * the fight happened to end instead of resuming its trip. ChaseStateProcessor already did the
	 * right thing and restored the placeholder from the signal; this keeps Attack/Pause/Run
	 * consistent with it.
	 *
	 * Patrol signals are kept, everything else still falls back to Idle.
	 */
	inline void ResolvePlaceholderAfterCombat(FMassAIStateFragment& StateFrag, AActor* Actor)
	{
		const bool bResumesPatrol = StateFrag.PlaceholderSignal == UnitSignals::PatrolRandom
								 || StateFrag.PlaceholderSignal == UnitSignals::PatrolIdle;

		if (!bResumesPatrol)
		{
			StateFrag.PlaceholderSignal = UnitSignals::Idle;
		}

		if (AUnitBase* UnitBase = Cast<AUnitBase>(Actor))
		{
			// Same mapping ChaseStateProcessor uses: the PatrolRandom signal corresponds to the
			// actor-side UnitData::Patrol state, not a UnitData::PatrolRandom.
			UnitBase->UnitStatePlaceholder = bResumesPatrol
				? (StateFrag.PlaceholderSignal == UnitSignals::PatrolIdle ? UnitData::PatrolIdle : UnitData::Patrol)
				: UnitData::Idle;
		}
	}
}
