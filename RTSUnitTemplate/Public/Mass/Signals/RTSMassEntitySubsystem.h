// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassEntitySubsystem.h"
#include "RTSMassEntitySubsystem.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API URTSMassEntitySubsystem : public UMassEntitySubsystem
{
	GENERATED_BODY()

public:
	/** This function will be called by our PIE cleanup helper to guarantee a fresh start. */
	void ForceResetForPIE()
	{
		// EntityManager is a 'protected' member of the parent UMassEntitySubsystem,
		// so we can access it here.

		// 1. Destroy the old, stale EntityManager instance.
		EntityManager.Reset();

		// 2. Create a brand new, empty EntityManager, just like the engine does on initial startup.
		EntityManager = MakeShared<FMassEntityManager>();
        
		UE_LOG(LogTemp, Warning, TEXT("MyMassEntitySubsystem: EntityManager has been forcibly reset for PIE."));
	}

};
