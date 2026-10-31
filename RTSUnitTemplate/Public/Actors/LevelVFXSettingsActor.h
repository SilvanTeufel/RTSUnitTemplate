// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelVFXSettingsActor.generated.h"

class UNiagaraComponent;

/**
 * Per-level tweaks for one-shot VFX. Put one into a map that needs them - maps without this actor
 * play every effect exactly as authored.
 *
 * Use case: the prologue plays in space on a transparent landscape, so the blood decals of the
 * death effects (Blood_Starter_VFX_Pack, user parameter "User.Spawn Count Decal") must not appear
 * there, while every other map keeps them.
 */
UCLASS(meta = (DisplayName = "Level VFX Settings"))
class RTSUNITTEMPLATE_API ALevelVFXSettingsActor : public AActor
{
	GENERATED_BODY()

public:
	ALevelVFXSettingsActor();

	/** Int user parameters set to 0 on every effect spawned through ApplyToNiagaraComponent (missing ones are ignored). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
	TArray<FName> ZeroedIntParameters;

	/**
	 * Applies the settings of the map the component lives in. Cheap no-op on maps without this actor.
	 * Call it right after spawning, before the first tick - spawn bursts read the parameters then.
	 */
	UFUNCTION(BlueprintCallable, Category = "VFX")
	static void ApplyToNiagaraComponent(UNiagaraComponent* Component);
};
