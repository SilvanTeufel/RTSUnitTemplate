// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/LevelVFXSettingsActor.h"
#include "NiagaraComponent.h"
#include "EngineUtils.h"

ALevelVFXSettingsActor::ALevelVFXSettingsActor()
{
	PrimaryActorTick.bCanEverTick = false;
	ZeroedIntParameters.Add(TEXT("User.Spawn Count Decal"));
}

void ALevelVFXSettingsActor::ApplyToNiagaraComponent(UNiagaraComponent* Component)
{
	if (!Component)
	{
		return;
	}
	UWorld* World = Component->GetWorld();
	if (!World)
	{
		return;
	}

	// Only on death / impact events, not per tick - and the class-filtered iterator only walks the
	// actors of this class (usually none), so maps without settings pay next to nothing.
	for (TActorIterator<ALevelVFXSettingsActor> It(World); It; ++It)
	{
		for (const FName& Parameter : It->ZeroedIntParameters)
		{
			if (!Parameter.IsNone())
			{
				Component->SetVariableInt(Parameter, 0);
			}
		}
	}
}
