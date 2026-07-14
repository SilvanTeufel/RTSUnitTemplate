// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RTSTurnResettable.generated.h"

/**
 * Generic "turn-based reset" contract implemented by any actor component that carries per-turn
 * state which should be cleared at the start of its owning unit's team turn.
 *
 * This interface deliberately lives in RTSUnitTemplate (the shared base plugin) so that an optional
 * turn-based layer (TurnBasedModule) can drive per-turn resets on components from OTHER optional
 * modules (e.g. WeaponModule's UWeaponComponent) WITHOUT any of them taking a hard module
 * dependency on each other. The turn layer discovers implementers via
 * UActorComponent::GetClass()->ImplementsInterface(URTSTurnResettable::StaticClass()) and calls
 * IRTSTurnResettable::Execute_OnTurnReset(...). If no module implements it, nothing happens.
 *
 * Contract: implementers reset ONLY transient per-turn state (weapon cooldowns, action counters,
 * per-turn shot counts). Persistent progression/economy (gold, ammo, magazines, talents) must NOT
 * be reset here. OnTurnReset is only ever invoked on the authority.
 */
// NOTE: use the module API macro (not MinimalAPI) on BOTH the U- and I-classes so the UHT-generated
// static Execute_OnTurnReset thunk is dll-exported and callable from other modules (TurnBasedModule
// invokes it by reflection). MinimalAPI would leave that thunk unexported -> LNK2019 across modules.
UINTERFACE(BlueprintType)
class RTSUNITTEMPLATE_API URTSTurnResettable : public UInterface
{
	GENERATED_BODY()
};

class RTSUNITTEMPLATE_API IRTSTurnResettable
{
	GENERATED_BODY()

public:
	/**
	 * Called on the authority at the start of the owning unit's team turn.
	 * @param TeamId       The team whose turn just started (== the owning unit's team).
	 * @param TurnCounter  Monotonically increasing turn index (useful for once-per-turn guards).
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "RTSUnitTemplate|Turn")
	void OnTurnReset(int32 TeamId, int32 TurnCounter);
};
