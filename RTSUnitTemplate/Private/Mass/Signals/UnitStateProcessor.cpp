// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Signals/UnitStateProcessor.h"

// Source: UnitStateProcessor.cpp
#include "AIController.h"
#include "CanvasTypes.h"
#include "MassSignalSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassActorSubsystem.h"
#include "MassNavigationSubsystem.h"
#include "Characters/Unit/UnitBase.h" // Include your AUnitBase header
#include "Mass/Signals/MySignals.h" // Include your signal definition header
#include "Widgets/UnitBaseHealthBar.h"
#include "MassExecutionContext.h" 
#include "MassNavigationFragments.h"
#include "Characters/Unit/BuildingBase.h"
#include "GameModes/ResourceGameMode.h"
#include "GameModes/RTSGameModeBase.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/GameViewportClient.h"

UUnitStateProcessor::UUnitStateProcessor()
{
    ProcessingPhase = EMassProcessingPhase::PostPhysics; // Run fairly late
	bRequiresGameThreadExecution = true;
    bAutoRegisterWithProcessingPhases = true; // Don't need ticking execute
}

void UUnitStateProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    // Get subsystems
	World = Owner.GetWorld();

	if (!World) return;
	
    SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
    EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();

    if (SignalSubsystem)
    {
        // List of signal names that should trigger ChangeUnitState

        // Register the same handler function for each signal in the list
        for (const FName& SignalName : StateChangeSignals)
        {
            // Use AddUFunction since ChangeUnitState is a UFUNCTION
            FDelegateHandle NewHandle = SignalSubsystem->GetSignalDelegateByName(SignalName)
                .AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, ChangeUnitState));

            StateChangeSignalDelegateHandle.Add(NewHandle);
            // Optional: Store handles if you need precise unregistration later
            // if (NewHandle.IsValid()) { StateSignalDelegateHandles.Add(NewHandle); }
        }

    	for (const FName& SignalName : SightChangeSignals)
    	{
    		// Use AddUFunction since ChangeUnitState is a UFUNCTION
    		FDelegateHandle NewHandle = SignalSubsystem->GetSignalDelegateByName(SignalName)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, HandleSightSignals));

    		SightChangeRequestDelegateHandle.Add(NewHandle);
    		// Optional: Store handles if you need precise unregistration later
    		// if (NewHandle.IsValid()) { StateSignalDelegateHandles.Add(NewHandle); }
    	}
    	FogParametersDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateFogMask)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, HandleUpdateFogMask));

    	
    	SyncUnitBaseDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SyncUnitBase)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, SyncUnitBase));
    	
    	SetUnitToChaseSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SetUnitToChase)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, SetUnitToChase));

    	MeleeAttackSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::MeleeAttack)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, UnitMeeleAttack));

    	RangedAttackSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::RangedAttack)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, UnitRangedAttack));

    	StartDeadSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::StartDead)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, HandleStartDead));

    	EndDeadSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::EndDead)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, HandleEndDead));
    	
    	IdlePatrolSwitcherDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::PISwitcher)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, IdlePatrolSwitcher));

    	UnitStatePlaceholderDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SetUnitStatePlaceholder)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, SetToUnitStatePlaceholder));
    	
    	SyncCastTimeDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SyncCastTime)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, SyncCastTime));

    	EndCastDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::EndCast)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, EndCast));
    	


    	ReachedBaseDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::ReachedBase)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, HandleReachedBase));

    	GetResourceDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::GetResource)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, HandleGetResource));
    	
    	StartBuildActionDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::GetClosestBase)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, HandleGetClosestBaseArea));

    	SpawnBuildingRequestDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SpawnBuildingRequest)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, HandleSpawnBuildingRequest));

    	SpawnSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UnitSpawned)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, HandleUnitSpawnedSignal));
    	
    }
}


void UUnitStateProcessor::BeginDestroy()
{
    
        // --- Perform your cleanup here ---
        // Unregister the signal handler
    for (int i = 0; i < StateChangeSignalDelegateHandle.Num(); i++)
    {
        if (SignalSubsystem && StateChangeSignalDelegateHandle[i].IsValid()) // Check if subsystem and handle are valid
        {
            SignalSubsystem->GetSignalDelegateByName(StateChangeSignals[i])
            .Remove(StateChangeSignalDelegateHandle[i]);
            
            StateChangeSignalDelegateHandle[i].Reset();
        }
    }

	for (int i = 0; i < SightChangeRequestDelegateHandle.Num(); i++)
	{
		if (SignalSubsystem && SightChangeRequestDelegateHandle[i].IsValid()) // Check if subsystem and handle are valid
		{
			SignalSubsystem->GetSignalDelegateByName(SightChangeSignals[i])
			.Remove(SightChangeRequestDelegateHandle[i]);
            
			SightChangeRequestDelegateHandle[i].Reset();
		}
	}
	
	if (SignalSubsystem && FogParametersDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateFogMask)
		.Remove(FogParametersDelegateHandle);
            
		FogParametersDelegateHandle.Reset();
	}
	
	if (SignalSubsystem && SyncUnitBaseDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::SyncUnitBase)
		.Remove(SyncUnitBaseDelegateHandle);
            
		SyncUnitBaseDelegateHandle.Reset();
	}
	
	if (SignalSubsystem && SetUnitToChaseSignalDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::SetUnitToChase)
		.Remove(SetUnitToChaseSignalDelegateHandle);
            
		SetUnitToChaseSignalDelegateHandle.Reset();
	}

	
	if (SignalSubsystem && MeleeAttackSignalDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::MeleeAttack)
		.Remove(MeleeAttackSignalDelegateHandle);
            
		MeleeAttackSignalDelegateHandle.Reset();
	}
	
	if (SignalSubsystem && RangedAttackSignalDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::RangedAttack)
		.Remove(RangedAttackSignalDelegateHandle);
            
		RangedAttackSignalDelegateHandle.Reset();
	}

	if (SignalSubsystem && StartDeadSignalDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::StartDead)
		.Remove(StartDeadSignalDelegateHandle);
            
		StartDeadSignalDelegateHandle.Reset();
	}

	if (SignalSubsystem && EndDeadSignalDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::EndDead)
		.Remove(EndDeadSignalDelegateHandle);
            
		EndDeadSignalDelegateHandle.Reset();
	}
	
	if (SignalSubsystem && IdlePatrolSwitcherDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::PISwitcher)
		.Remove(IdlePatrolSwitcherDelegateHandle);
            
		IdlePatrolSwitcherDelegateHandle.Reset();
	}

	if (SignalSubsystem && UnitStatePlaceholderDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::SetUnitStatePlaceholder)
		.Remove(UnitStatePlaceholderDelegateHandle);
            
		UnitStatePlaceholderDelegateHandle.Reset();
	}
	
	if (SignalSubsystem && SyncCastTimeDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::SyncCastTime)
		.Remove(SyncCastTimeDelegateHandle);
            
		SyncCastTimeDelegateHandle.Reset();
	}

	if (SignalSubsystem && EndCastDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::EndCast)
		.Remove(EndCastDelegateHandle);
            
		EndCastDelegateHandle.Reset();
	}


	
	if (SignalSubsystem && ReachedBaseDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::ReachedBase)
		.Remove(ReachedBaseDelegateHandle);
            
		ReachedBaseDelegateHandle.Reset();
	}

	if (SignalSubsystem && GetResourceDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::GetResource)
		.Remove(GetResourceDelegateHandle);
            
		GetResourceDelegateHandle.Reset();
	}
	
	if (SignalSubsystem && StartBuildActionDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::GetClosestBase)
		.Remove(StartBuildActionDelegateHandle);
            
		StartBuildActionDelegateHandle.Reset();
	}
	
	if (SignalSubsystem && SpawnBuildingRequestDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::SpawnBuildingRequest)
		.Remove(SpawnBuildingRequestDelegateHandle);
            
		SpawnBuildingRequestDelegateHandle.Reset();
	}

	if (SignalSubsystem && SpawnSignalDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::UnitSpawned)
		.Remove(SpawnSignalDelegateHandle);
            
		SpawnSignalDelegateHandle.Reset();
	}
	
    SignalSubsystem = nullptr; // Null out pointers if needed
    EntitySubsystem = nullptr;
    // --- End Cleanup ---
    Super::BeginDestroy();
}

void UUnitStateProcessor::ConfigureQueries()
{
    EntityQuery.RegisterWithProcessor(*this);
}

void UUnitStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
  
}

void UUnitStateProcessor::SwitchState(FName SignalName, FMassEntityHandle& Entity, const FMassEntityManager& EntityManager)
{

	
	        // Check entity validity *on the game thread*
            if (!EntityManager.IsEntityValid(Entity)) 
            {
                return;
            }
            // Get fragments and actors *on the game thread*
            FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
			FMassAIStateFragment* StateFragment = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);

			//FMassCombatStatsFragment* CombatStatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity);
			//if (CombatStatsFrag->TeamId == 3) UE_LOG(LogTemp, Error, TEXT("SwitchState! %s"), *SignalName.ToString());
	
	
            if (ActorFragPtr)
            {
                AActor* Actor = ActorFragPtr->GetMutable(); 
                if (IsValid(Actor)) 
                {
                    AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
                    if (UnitBase)
                    {
                        // *** Mass Tag Modifications MUST be inside the Game Thread Task ***
                        // --- Remove old tags ---
                        // Note: Using EntityManager directly here, NOT a separate command buffer object usually.
                        EntityManager.Defer().RemoveTag<FMassStateIdleTag>(Entity);
                        EntityManager.Defer().RemoveTag<FMassStateChaseTag>(Entity);
                        EntityManager.Defer().RemoveTag<FMassStateAttackTag>(Entity);
                        EntityManager.Defer().RemoveTag<FMassStatePauseTag>(Entity);
                        EntityManager.Defer().RemoveTag<FMassStateDeadTag>(Entity); 
                        EntityManager.Defer().RemoveTag<FMassStateRunTag>(Entity);
                        EntityManager.Defer().RemoveTag<FMassStatePatrolRandomTag>(Entity);
                        EntityManager.Defer().RemoveTag<FMassStatePatrolIdleTag>(Entity);
                        EntityManager.Defer().RemoveTag<FMassStateCastingTag>(Entity);
                    	EntityManager.Defer().RemoveTag<FMassStateIsAttackedTag>(Entity);

                    	EntityManager.Defer().RemoveTag<FMassStateGoToBaseTag>(Entity);
                    	EntityManager.Defer().RemoveTag<FMassStateGoToBuildTag>(Entity);
                    	EntityManager.Defer().RemoveTag<FMassStateBuildTag>(Entity);
                    	EntityManager.Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(Entity);
                    	EntityManager.Defer().RemoveTag<FMassStateResourceExtractionTag>(Entity);
                    	
                        // --- Add new tag ---
                    	if (SignalName == UnitSignals::Idle)
                    	{
                    		EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);
                    		EntityManager.Defer().AddTag<FMassStateIdleTag>(Entity);
                    		StateFragment->PlaceholderSignal = UnitSignals::Idle;
                    		UnitBase->UnitStatePlaceholder = UnitData::Idle;
         
                    	}
                        else if (SignalName == UnitSignals::Chase)
                        {
                        	EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);
	                        EntityManager.Defer().AddTag<FMassStateChaseTag>(Entity);
                        }
                        else if (SignalName == UnitSignals::Attack)
                        {
                        	EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);
	                        EntityManager.Defer().AddTag<FMassStateAttackTag>(Entity);
                        }
                        else if (SignalName == UnitSignals::Dead) { EntityManager.Defer().AddTag<FMassStateDeadTag>(Entity); }
                        else if (SignalName == UnitSignals::PatrolIdle)
                        {
                        	UE_LOG(LogTemp, Log, TEXT("Set to PatrolIdle! Tag"));
                        	EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);
	                        EntityManager.Defer().AddTag<FMassStatePatrolIdleTag>(Entity);
                        }
                        else if (SignalName == UnitSignals::PatrolRandom)
                        {
                        	EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);
                        	EntityManager.Defer().AddTag<FMassStatePatrolRandomTag>(Entity);
                        	StateFragment->PlaceholderSignal = UnitSignals::PatrolRandom;
                        	UnitBase->UnitStatePlaceholder = UnitData::PatrolRandom;
                        }
                        else if (SignalName == UnitSignals::Pause)
                        {
                        	EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);
                        	EntityManager.Defer().AddTag<FMassStatePauseTag>(Entity);
                        }
                        else if (SignalName == UnitSignals::Run)
                        {
	                        EntityManager.Defer().AddTag<FMassStateRunTag>(Entity);
                        	StateFragment->PlaceholderSignal = UnitSignals::Run;
                        	UnitBase->UnitStatePlaceholder = UnitData::Run;
                        }
                        else if (SignalName == UnitSignals::Casting) { EntityManager.Defer().AddTag<FMassStateCastingTag>(Entity); }
                        else if (SignalName == UnitSignals::IsAttacked)
                        {
                        	EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);
                        	EntityManager.Defer().AddTag<FMassStateIsAttackedTag>(Entity);
                        }else if (SignalName == UnitSignals::GoToBase)
                        {
                        	EntityManager.Defer().AddTag<FMassStateGoToBaseTag>(Entity);
                        	StateFragment->PlaceholderSignal = UnitSignals::GoToBase;
                        	UnitBase->UnitStatePlaceholder = UnitData::GoToBase;
                        }else if (SignalName == UnitSignals::GoToBuild)
                        {
                        	EntityManager.Defer().AddTag<FMassStateGoToBuildTag>(Entity);
                        }else if (SignalName == UnitSignals::Build)
                        {
                        	EntityManager.Defer().AddTag<FMassStateBuildTag>(Entity);
                        }else if (SignalName == UnitSignals::GoToResourceExtraction)
                        {
                        	EntityManager.Defer().AddTag<FMassStateGoToResourceExtractionTag>(Entity);
                        	StateFragment->PlaceholderSignal = UnitSignals::GoToResourceExtraction;
                        	UnitBase->UnitStatePlaceholder = UnitData::GoToResourceExtraction;
                        }else if (SignalName == UnitSignals::ResourceExtraction)
                        {
                        	EntityManager.Defer().AddTag<FMassStateResourceExtractionTag>(Entity);
                        }


                    	if (SignalName == UnitSignals::Idle) { UnitBase->SetUnitState(UnitData::Idle); }
                        else if (SignalName == UnitSignals::Chase) { UnitBase->SetUnitState(UnitData::Chase); }
                        else if (SignalName == UnitSignals::Attack) { UnitBase->SetUnitState(UnitData::Attack); }
                        else if (SignalName == UnitSignals::Dead) { UnitBase->SetUnitState(UnitData::Dead); }
                        else if (SignalName == UnitSignals::PatrolIdle)
                        {
                        	UE_LOG(LogTemp, Log, TEXT("Set to PatrolIdle! State"));
	                        UnitBase->SetUnitState(UnitData::PatrolIdle);
                        }
                        else if (SignalName == UnitSignals::PatrolRandom){ UnitBase->SetUnitState(UnitData::PatrolRandom); }
                        else if (SignalName == UnitSignals::Pause) { UnitBase->SetUnitState(UnitData::Pause); }
                        else if (SignalName == UnitSignals::Run) { UnitBase->SetUnitState(UnitData::Run); }
                        else if (SignalName == UnitSignals::Casting) { UnitBase->SetUnitState(UnitData::Casting); }
                        else if (SignalName == UnitSignals::IsAttacked)
                        {
                        	//StateFragment->PlaceholderSignal = UnitSignals::Chase;
                        	//UnitBase->UnitStatePlaceholder = UnitData::Chase;
                        	UnitBase->SetUnitState(UnitData::IsAttacked);
                        }
                        else if (SignalName == UnitSignals::GoToBase){ UnitBase->SetUnitState(UnitData::GoToBase); }
                    	else if (SignalName == UnitSignals::GoToBuild ){ UnitBase->SetUnitState(UnitData::GoToBuild); }
                    	else if (SignalName == UnitSignals::Build){ UnitBase->SetUnitState(UnitData::Build); }
                    	else if (SignalName == UnitSignals::GoToResourceExtraction){ UnitBase->SetUnitState(UnitData::GoToResourceExtraction); }
                    	else if (SignalName == UnitSignals::ResourceExtraction) { UnitBase->SetUnitState(UnitData::ResourceExtraction); }

                    	//FMassAIStateFragment* State = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
                    	//State->StateTimer = 0.f;
                    }
                    // ... rest of your else logs for invalid casts/actors ...
                }
                 else
                 {
	                 //UE_LOG(LogTemp, Warning, TEXT("ChangeUnitState (GameThread): Actor pointer in ActorFragment for Entity %d:%d is invalid."), Entity.Index, Entity.SerialNumber);
                 }
            }
             else
             {
	             //UE_LOG(LogTemp, Warning, TEXT("ChangeUnitState (GameThread): Entity %d:%d has no ActorFragment."), Entity.Index, Entity.SerialNumber);
             }

	//if (CombatStatsFrag->TeamId == 3)UE_LOG(LogTemp, Error, TEXT("Set StateFragment->SwitchingState to FALSE!"));
	StateFragment->SwitchingState = false;
}

void UUnitStateProcessor::IdlePatrolSwitcher(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    // Vorab-Check außerhalb des Tasks
    if (!EntitySubsystem)
    {
        return;
    }

    // Kopie der Entities für sichere asynchrone Verarbeitung
    TArray<FMassEntityHandle> EntitiesCopy = Entities;

    // An den Game Thread senden, da NavSys verwendet wird
    AsyncTask(ENamedThreads::GameThread, [this, EntitiesCopy]() mutable
    {
        // --- Dieser Code läuft garantiert im Game Thread ---

        if (!EntitySubsystem) { return; }
        FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager(); // Mutable für Fragment-Zugriff
        UWorld* World = EntitySubsystem->GetWorld();
        if (!World) { return; }

        // Hole Subsysteme sicher im Game Thread
        UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
        if (!NavSys) { return; }
        UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
        if (!SignalSubsystem) { return; }

        for (const FMassEntityHandle& Entity : EntitiesCopy)
        {
            if (!EntityManager.IsEntityValid(Entity))
            {
                continue;
            }

            // --- Hole benötigte Fragmente für DIESE Entity ---
            FMassAIStateFragment* StateFragPtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
            FMassPatrolFragment* PatrolFragPtr = EntityManager.GetFragmentDataPtr<FMassPatrolFragment>(Entity);
            FMassMoveTargetFragment* MoveTargetPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
            const FMassCombatStatsFragment* StatsFragPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity);

            // Prüfe, ob alle nötigen Fragmente vorhanden sind
            if (!StateFragPtr || !PatrolFragPtr || !MoveTargetPtr || !StatsFragPtr)
            {
                continue;
            }

            // Dereferenziere Pointer für einfacheren Zugriff
            FMassAIStateFragment& StateFrag = *StateFragPtr;
            FMassPatrolFragment& PatrolFrag = *PatrolFragPtr; // Mutable Referenz
            FMassMoveTargetFragment& MoveTarget = *MoveTargetPtr; // Mutable Referenz
            const FMassCombatStatsFragment& StatsFrag = *StatsFragPtr;
        	
                    SetNewRandomPatrolTarget(PatrolFrag, MoveTarget, NavSys, World, StatsFrag.RunSpeed);
                    SignalSubsystem->SignalEntity(UnitSignals::PatrolRandom, Entity);
                    StateFrag.StateTimer = 0.f;
        } // Ende for each entity
    }); // Ende AsyncTask Lambda
}

void UUnitStateProcessor::ForceSetPatrolRandomTarget(FMassEntityHandle& Entity)
{
	 // Vorab-Check außerhalb des Tasks
    if (!EntitySubsystem)
    {
        return;
    }



    // An den Game Thread senden, da NavSys verwendet wird
    AsyncTask(ENamedThreads::GameThread, [this, Entity]() mutable
    {
        // --- Dieser Code läuft garantiert im Game Thread ---

        if (!EntitySubsystem) { return; }
        FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
        UWorld* World = EntitySubsystem->GetWorld();
        if (!World) { return; }

        UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
        if (!NavSys) {  return; }
        UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
        if (!SignalSubsystem) {  return; }

   
            if (!EntityManager.IsEntityValid(Entity))
            {
                return;
            }

            // --- Hole benötigte Fragmente für DIESE Entity ---
            FMassAIStateFragment* StateFragPtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
            FMassPatrolFragment* PatrolFragPtr = EntityManager.GetFragmentDataPtr<FMassPatrolFragment>(Entity);
            FMassMoveTargetFragment* MoveTargetPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
            const FMassCombatStatsFragment* StatsFragPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity);

            // Prüfe, ob alle nötigen Fragmente vorhanden sind
            if (!StateFragPtr || !PatrolFragPtr || !MoveTargetPtr || !StatsFragPtr)
            {
            	return;
            }

            // Dereferenziere Pointer
            FMassAIStateFragment& StateFrag = *StateFragPtr;
            FMassPatrolFragment& PatrolFrag = *PatrolFragPtr;
            FMassMoveTargetFragment& MoveTarget = *MoveTargetPtr;
            const FMassCombatStatsFragment& StatsFrag = *StatsFragPtr;
    	
            // Rufe die NavSys-abhängige Funktion sicher hier auf
            SetNewRandomPatrolTarget(PatrolFrag, MoveTarget, NavSys, World, StatsFrag.RunSpeed);

            // Signalisiere den Zustandswechsel
            // SignalSubsystem->SignalEntity(UnitSignals::PatrolRandom, Entity);

            // Setze den Timer für den *neuen* Zustand zurück.
            StateFrag.StateTimer = 0.f;

            // Keine weiteren Checks oder continue nötig, da wir immer wechseln wollen.

    
    }); // Ende AsyncTask Lambda
}


void UUnitStateProcessor::ChangeUnitState(FName SignalName, TArray<FMassEntityHandle>& Entities)
{

    if (!EntitySubsystem)
    {
        // UE_LOG(LogTemp, Error, TEXT("ChangeUnitState called but EntitySubsystem is null!"));
        return;
    }


    TArray<FMassEntityHandle> EntitiesCopy = Entities; 

    AsyncTask(ENamedThreads::GameThread, [this, SignalName, EntitiesCopy]() mutable // mutable if you modify captures (not needed here)
    {
        // Re-check EntitySubsystem just in case? Usually fine if 'this' is valid.
        if (!EntitySubsystem) 
        {
             // UE_LOG(LogTemp, Error, TEXT("ChangeUnitState (GameThread): EntitySubsystem became null!"));
             return;
        }

        const FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

        for (FMassEntityHandle& Entity : EntitiesCopy) // Iterate the captured copy
        {
        	
            // Check entity validity *on the game thread*
            if (!EntityManager.IsEntityValid(Entity)) 
            {
                continue;
            }

        	SwitchState(SignalName, Entity, EntityManager);

        	FMassAIStateFragment* State = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
       
        	if (!DoesEntityHaveTag(EntityManager,Entity, FMassStateDeadTag::StaticStruct()))
        		State->StateTimer = 0.f;
        } // End For loop
    }); // End AsyncTask Lambda
}

void UUnitStateProcessor::SyncUnitBase(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	// Gehe durch alle Entities, die mit dem Signal übergeben wurden
	for (const FMassEntityHandle& Entity : Entities)
	{
		// Rufe die Funktion auf, die die eigentliche Arbeit für eine einzelne Entity macht
		// Die internen Checks (Subsystem, Validität etc.) sind in der Hilfsfunktion enthalten.
		SynchronizeStatsFromActorToFragment(Entity);
		SynchronizeUnitState(Entity);
	}
}

void UUnitStateProcessor::SynchronizeStatsFromActorToFragment(FMassEntityHandle Entity)
{
    // --- Vorab-Checks außerhalb des AsyncTasks ---
    if (!EntitySubsystem)
    {
        //UE_LOG(LogTemp, Error, TEXT("SynchronizeStatsFromActorToFragment: EntitySubsystem ist null!"));
        return;
    }

    // Const EntityManager für Read-Only Checks vor dem Task
    const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager();

    if (!EntityManager.IsEntityValid(Entity))
    {
        //UE_LOG(LogTemp, Warning, TEXT("SynchronizeStatsFromActorToFragment: Entity %d:%d ist ungültig."), Entity.Index, Entity.SerialNumber);
        return;
    }

    // Den zugehörigen Actor holen (Read-Only Zugriff reicht hier)
    const FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
    const AActor* Actor = ActorFrag ? ActorFrag->Get() : nullptr;
    const AUnitBase* UnitActor = Cast<AUnitBase>(Actor); // Const Cast genügt

    // Prüfen, ob wir einen gültigen Actor und dessen Attributes haben
    // Ersetze 'Attributes', falls dein Member anders heißt
    if (!IsValid(UnitActor) || !UnitActor->Attributes)
    {
        //UE_LOG(LogTemp, Warning, TEXT("SynchronizeStatsFromActorToFragment: Konnte keinen gültigen AUnitBase Actor oder Attributes für Entity %d:%d finden."), Entity.Index, Entity.SerialNumber);
        return;
    }

    // --- Notwendige Daten für den AsyncTask erfassen ---
    // const_cast ist für TWeakObjectPtr nötig, da der Ctor non-const erwartet.
    TWeakObjectPtr<AUnitBase> WeakUnitActor(const_cast<AUnitBase*>(UnitActor));
    FMassEntityHandle CapturedEntity = Entity; // Handle per Wert kopieren

    // --- AsyncTask an den GameThread senden ---
    AsyncTask(ENamedThreads::GameThread, [this, WeakUnitActor, CapturedEntity]() mutable
    {
        // --- Code in dieser Lambda läuft jetzt im GameThread ---
        AUnitBase* StrongUnitActor = WeakUnitActor.Get();

        // Subsystem-Validität im GameThread prüfen
        if (!EntitySubsystem)
        {
            //UE_LOG(LogTemp, Error, TEXT("SynchronizeStatsFromActorToFragment (GameThread): EntitySubsystem wurde null!"));
            return;
        }
        // Mutable EntityManager holen, um Fragment schreiben zu können
        FMassEntityManager& GTEntityManager = EntitySubsystem->GetMutableEntityManager();

        // Actor- und Entity-Validität erneut im GameThread prüfen
        if (StrongUnitActor && GTEntityManager.IsEntityValid(CapturedEntity))
        {
            // Das MUTABLE Combat Stats Fragment holen
            FMassCombatStatsFragment* CombatStatsFrag = GTEntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(CapturedEntity);
        	FMassPatrolFragment* PatrolFrag = GTEntityManager.GetFragmentDataPtr<FMassPatrolFragment>(CapturedEntity);
			FMassWorkerStatsFragment* WorkerStats = GTEntityManager.GetFragmentDataPtr<FMassWorkerStatsFragment>(CapturedEntity);
        	// Das Attribute Set holen (erneut auf Gültigkeit prüfen)
            // Ersetze 'Attributes', falls dein Member anders heißt
            UAttributeSetBase* AttributeSet = StrongUnitActor->Attributes;

            // Fragment und AttributeSet auf Gültigkeit prüfen, BEVOR darauf zugegriffen wird
            if (CombatStatsFrag && AttributeSet)
            {
                // Sicherstellen, dass GetHealth/GetShield in deinem AttributeSet existieren
                CombatStatsFrag->Health = AttributeSet->GetHealth();
                CombatStatsFrag->Shield = AttributeSet->GetShield();
            	CombatStatsFrag->MaxHealth = AttributeSet->GetMaxHealth();
            	CombatStatsFrag->MaxShield = AttributeSet->GetMaxHealth();
            	CombatStatsFrag->AttackDamage = AttributeSet->GetAttackDamage();
            	CombatStatsFrag->AttackRange = AttributeSet->GetRange();
            	CombatStatsFrag->RunSpeed = AttributeSet->GetRunSpeed();
            	CombatStatsFrag->Armor = AttributeSet->GetArmor();
				CombatStatsFrag->MagicResistance = AttributeSet->GetMagicResistance();

            	CombatStatsFrag->PauseDuration = StrongUnitActor->PauseDuration;// We need to add this to Attributes i guess;
				CombatStatsFrag->AttackDuration = StrongUnitActor->AttackDuration;
				CombatStatsFrag->bCanAttack = StrongUnitActor->CanAttack; // Assuming CanAttack() on Attributes
				CombatStatsFrag->bUseProjectile = StrongUnitActor->UseProjectile; // Assuming UsesProjectile() on Attributes
				CombatStatsFrag->CastTime = StrongUnitActor->CastTime;
				CombatStatsFrag->IsInitialized = StrongUnitActor->IsInitialized;


            	if (StrongUnitActor && StrongUnitActor->NextWaypoint) // Use config from Actor if available
            	{
					// <<< REPLACE Properties with your actual variable names >>>
					PatrolFrag->bLoopPatrol = StrongUnitActor->NextWaypoint->PatrolCloseToWaypoint; // Assuming direct property access
					//PatrolFrag->bPatrolRandomAroundWaypoint = UnitOwner->NextWaypoint->PatrolCloseToWaypoint;
					//PatrolFrag->RandomPatrolRadius = UnitOwner->NextWaypoint->PatrolCloseMaxInterval;
					PatrolFrag->RandomPatrolMinIdleTime = StrongUnitActor->NextWaypoint->PatrolCloseMinInterval;
					PatrolFrag->RandomPatrolMaxIdleTime = StrongUnitActor->NextWaypoint->PatrolCloseMaxInterval;
					PatrolFrag->TargetWaypointLocation = StrongUnitActor->NextWaypoint->GetActorLocation();
					PatrolFrag->RandomPatrolRadius = (StrongUnitActor->NextWaypoint->PatrolCloseOffset.X+StrongUnitActor->NextWaypoint->PatrolCloseOffset.Y)/2.f;
					PatrolFrag->IdleChance = StrongUnitActor->NextWaypoint->PatrolCloseIdlePercentage;
				}

            	if (StrongUnitActor && StrongUnitActor->IsWorker) // Use config from Actor if available
            	{
            		WorkerStats->BaseAvailable = StrongUnitActor->Base? true: false;
            		if (StrongUnitActor->Base)
            		{
            			WorkerStats->BasePosition = StrongUnitActor->Base->GetActorLocation();
						FVector Origin, BoxExtent;

						StrongUnitActor->Base->GetActorBounds(true, Origin, BoxExtent);
						WorkerStats->BaseArrivalDistance = BoxExtent.Size()/2+100.f;
            		}

            		WorkerStats->BuildingAreaAvailable = StrongUnitActor->BuildArea? true : false;
            		if (StrongUnitActor->BuildArea)
            		{
            			FVector Origin, BoxExtent;
						StrongUnitActor->BuildArea->GetActorBounds( false, Origin, BoxExtent);

            			WorkerStats->BuildAreaArrivalDistance = BoxExtent.Size()/2+50.f;
            			WorkerStats->BuildingAvailable = StrongUnitActor->BuildArea->Building ? true : false;
            			WorkerStats->BuildAreaPosition = StrongUnitActor->BuildArea->GetActorLocation();
						WorkerStats->BuildTime = StrongUnitActor->BuildArea->BuildTime;
            		}

            		WorkerStats->ResourceAvailable = StrongUnitActor->ResourcePlace? true : false;
					if (StrongUnitActor->ResourcePlace && StrongUnitActor->ResourcePlace->AvailableResourceAmount <= 0.f)
					{
						WorkerStats->ResourceAvailable = false;
					}
            		
            		if (StrongUnitActor->ResourcePlace)
            		{
            			FVector Origin, BoxExtent;
						StrongUnitActor->ResourcePlace->GetActorBounds(false, Origin, BoxExtent);
            			WorkerStats->ResourceArrivalDistance = BoxExtent.Size()/2+50.f;
            			WorkerStats->ResourcePosition = StrongUnitActor->ResourcePlace->GetActorLocation();
            		}
            		WorkerStats->ResourceExtractionTime = StrongUnitActor->ResourceExtractionTime;
            	}
            }
        }
        else
        {
            //UE_LOG(LogTemp, Warning, TEXT("SynchronizeStatsFromActorToFragment (GameThread): Actor oder Entity %d:%d wurde ungültig vor der Synchronisation."), CapturedEntity.Index, CapturedEntity.SerialNumber);
        }
    }); // Ende AsyncTask Lambda
}

void UUnitStateProcessor::SynchronizeUnitState(FMassEntityHandle Entity)
{
	// UE_LOG(LogTemp, Log, TEXT("SynchronizeUnitState!"));
    // --- Vorab-Checks außerhalb des AsyncTasks ---
    if (!EntitySubsystem)
    {
        //UE_LOG(LogTemp, Error, TEXT("SynchronizeUnitState: EntitySubsystem ist null!"));
        return;
    }

    const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager(); // Für Read-Only Checks

    if (!EntityManager.IsEntityValid(Entity))
    {
        //UE_LOG(LogTemp, Warning, TEXT("SynchronizeUnitState: Entity %d:%d ist ungültig."), Entity.Index, Entity.SerialNumber);
        return;
    }

    const FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
    const AActor* Actor = ActorFrag ? ActorFrag->Get() : nullptr;
    const AUnitBase* UnitActor = Cast<AUnitBase>(Actor);

    // Prüfen auf gültigen Actor
    if (!IsValid(UnitActor)) // Attributes Check hier vielleicht nicht nötig
    {
        //UE_LOG(LogTemp, Warning, TEXT("SynchronizeUnitState: Konnte keinen gültigen AUnitBase Actor für Entity %d:%d finden."), Entity.Index, Entity.SerialNumber);
        return;
    }

    // --- Notwendige Daten für den AsyncTask erfassen ---
    TWeakObjectPtr<AUnitBase> WeakUnitActor(const_cast<AUnitBase*>(UnitActor));
    FMassEntityHandle CapturedEntity = Entity;

    // --- AsyncTask an den GameThread senden ---
    // EntityManager aus Capture entfernt, nur 'this' benötigt
    AsyncTask(ENamedThreads::GameThread, [this, WeakUnitActor, CapturedEntity, &EntityManager]() mutable
    {
        // --- Code in dieser Lambda läuft jetzt im GameThread ---
        AUnitBase* StrongUnitActor = WeakUnitActor.Get();

        if (!EntitySubsystem)
        {
            //UE_LOG(LogTemp, Error, TEXT("SynchronizeUnitState (GameThread): EntitySubsystem wurde null!"));
            return;
        }
        FMassEntityManager& GTEntityManager = EntitySubsystem->GetMutableEntityManager();
    	FMassAIStateFragment* State = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(CapturedEntity);
    			
    			if(StrongUnitActor->GetUnitState() == UnitData::PatrolRandom && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStatePatrolRandomTag::StaticStruct())){ //  && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStatePatrolRandomTag::StaticStruct())
    				//UE_LOG(LogTemp, Log, TEXT("SynchronizeUnitState! To PatrolRandom"));

    				// --- Hole benötigte Fragmente für DIESE Entity ---
					FMassAIStateFragment* StateFragPtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(CapturedEntity);
					FMassPatrolFragment* PatrolFragPtr = EntityManager.GetFragmentDataPtr<FMassPatrolFragment>(CapturedEntity);
					FMassMoveTargetFragment* MoveTargetPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(CapturedEntity);
					FMassCombatStatsFragment* StatsFragPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(CapturedEntity);

					// Prüfe, ob alle nötigen Fragmente vorhanden sind
					if (!StateFragPtr || !PatrolFragPtr || !MoveTargetPtr || !StatsFragPtr)
					{
						return;
					}
    				UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
					if (!NavSys) {  return; }
        	
    				// Dereferenziere Pointer für einfacheren Zugriff
					FMassAIStateFragment& StateFrag = *StateFragPtr;
					FMassPatrolFragment& PatrolFrag = *PatrolFragPtr; // Mutable Referenz
					FMassMoveTargetFragment& MoveTarget = *MoveTargetPtr; // Mutable Referenz
					const FMassCombatStatsFragment& StatsFrag = *StatsFragPtr;
        	
					SetNewRandomPatrolTarget(PatrolFrag, MoveTarget, NavSys, World, StatsFrag.RunSpeed);
					SignalSubsystem->SignalEntity(UnitSignals::PatrolRandom, CapturedEntity);
					StateFrag.StateTimer = 0.f;
    				//SwitchState(UnitSignals::PatrolRandom, CapturedEntity, GTEntityManager);
				}
				else if(StrongUnitActor->GetUnitState() == UnitData::PatrolIdle && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStatePatrolIdleTag::StaticStruct())){

					//UE_LOG(LogTemp, Log, TEXT("Calling UnitData::PatrolIdle!"));
					SwitchState(UnitSignals::PatrolIdle, CapturedEntity, GTEntityManager);
				}
				else if(StrongUnitActor->IsWorker && StrongUnitActor->GetUnitState() == UnitData::GoToBase && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateGoToBaseTag::StaticStruct())){
					SwitchState(UnitSignals::GoToBase, CapturedEntity, GTEntityManager);
				}else if(StrongUnitActor->IsWorker && StrongUnitActor->GetUnitState() == UnitData::GoToResourceExtraction && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateGoToResourceExtractionTag::StaticStruct())){
					SwitchState(UnitSignals::GoToResourceExtraction, CapturedEntity, GTEntityManager);
				}else if(StrongUnitActor->IsWorker && StrongUnitActor->GetUnitState() == UnitData::GoToBuild && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateGoToBuildTag::StaticStruct())){
				   SwitchState(UnitSignals::GoToBuild, CapturedEntity, GTEntityManager);
				}else if(StrongUnitActor->IsWorker && StrongUnitActor->GetUnitState() == UnitData::Build && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateBuildTag::StaticStruct())){
						State->StateTimer = 0.f;
					SwitchState(UnitSignals::Build, CapturedEntity, GTEntityManager);
				}else if(StrongUnitActor->IsWorker && StrongUnitActor->GetUnitState() == UnitData::ResourceExtraction && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateResourceExtractionTag::StaticStruct())){
					State->StateTimer = 0.f;
				   SwitchState(UnitSignals::ResourceExtraction, CapturedEntity, GTEntityManager);
				}
			//}
        //}
    }); // Ende AsyncTask Lambda
}

void UUnitStateProcessor::UnitMeeleAttack(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    if (!EntitySubsystem)
    {
       //UE_LOG(LogTemp, Error, TEXT("UnitMeeleAttack called but EntitySubsystem is null!")); // Changed log message context
       return;
    }

    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

    for (const FMassEntityHandle& Entity : Entities)
    {
       if (!EntityManager.IsEntityValid(Entity)) continue;

       // Use mutable fragment ptr for Actor access needed later
       FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
       if (ActorFragPtr)
       {
          AActor* Actor = ActorFragPtr->GetMutable();
          if (IsValid(Actor))
          {
             AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
             // Read Target Fragment directly here
            FMassAITargetFragment* TargetFrag = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Entity);
          	const FMassAIStateFragment* TargetStateFrag = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
          	FMassMoveTargetFragment* MoveTargetFragmentPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
          	FMassCombatStatsFragment* TargetStatsFragmentPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity);
             // Ensure all prerequisites are met before proceeding and dispatching
             if (UnitBase && IsValid(UnitBase->UnitToChase) && !UnitBase->UseProjectile &&
                 TargetFrag && TargetFrag->bHasValidTarget && TargetFrag->TargetEntity.IsSet())
             {
                FMassEntityHandle AttackerEntity = Entity; // Attacker is the current entity in loop
                const FMassEntityHandle TargetEntity = TargetFrag->TargetEntity; // Get target from fragment

                // Check if target entity is valid *before* dispatching
                if (!EntityManager.IsEntityValid(TargetEntity))
                {
                    // UE_LOG(LogTemp, Warning, TEXT("UnitMeeleAttack: Target entity from TargetFragment is invalid for attacker %s."), *UnitBase->GetName());
                    continue; // Skip this attack
                }

                // Capture necessary variables for the AsyncTask
                TWeakObjectPtr<AUnitBase> WeakAttacker(UnitBase);
                TWeakObjectPtr<AUnitBase> WeakTarget(UnitBase->UnitToChase);
                bool bIsMagicDamage = UnitBase->IsDoingMagicDamage; // Capture simple types by value

                // *** FIX: Capture 'this', 'AttackerEntity', and 'TargetEntity' in the lambda ***
                AsyncTask(ENamedThreads::GameThread, [this, AttackerEntity, TargetEntity, WeakAttacker, WeakTarget, bIsMagicDamage, MoveTargetFragmentPtr, TargetStateFrag, TargetStatsFragmentPtr, TargetFrag]() mutable
                {
                    // Get Strong Pointers for Actor operations
                    AUnitBase* StrongAttacker = WeakAttacker.Get();
                    AUnitBase* StrongTarget = WeakTarget.Get();

                    // *** FIX: 'this' is now captured, so EntitySubsystem is accessible ***
                    if (!EntitySubsystem)
                    {
                         // UE_LOG(LogTemp, Error, TEXT("UnitMeeleAttack (GameThread): EntitySubsystem became null!"));
                         return;
                    }
                    FMassEntityManager& GTEntityManager = EntitySubsystem->GetMutableEntityManager();
                	
                	// Ensure Actors and Entities are still valid
                    if (StrongAttacker && StrongTarget && GTEntityManager.IsEntityValid(AttackerEntity) && GTEntityManager.IsEntityValid(TargetEntity))
                    {
                        // --- Access Fragments on Game Thread ---
                        // *** FIX: AttackerEntity and TargetEntity are now captured ***
                        const FMassCombatStatsFragment* AttackerStats = GTEntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(AttackerEntity);
                        FMassCombatStatsFragment* TargetStats = GTEntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TargetEntity); // Mutable required
                    	FMassAIStateFragment* TargetAIStateFragment = GTEntityManager.GetFragmentDataPtr<FMassAIStateFragment>(TargetEntity);
                    	
                    	if (!AttackerStats || !TargetStats)
                        {
                            return;
                        }
                        // --- Apply Damage using Fragment Stats (Example from previous answer) ---
                        float BaseDamage = AttackerStats->AttackDamage;
                        float Defense = bIsMagicDamage ? TargetStats->MagicResistance : TargetStats->Armor;
                        float DamageAfterDefense = FMath::Max(0.0f, BaseDamage - Defense);
                    	
                        float RemainingDamage = DamageAfterDefense;
                        if (TargetStats->Shield > 0)
                        {
                        	float ShieldDamage = FMath::Min(TargetStats->Shield, RemainingDamage);
                        	TargetStats->Shield -= ShieldDamage;
                        	StrongTarget->SetShield_Implementation(StrongTarget->Attributes->GetShield() - ShieldDamage);
                        	RemainingDamage -= ShieldDamage;
                        }
                        if (RemainingDamage > 0)
                        {
                        	float HealthDamage = FMath::Min(TargetStats->Health, RemainingDamage);
                        	TargetStats->Health -= HealthDamage;
                        	StrongTarget->SetHealth_Implementation(StrongTarget->Attributes->GetHealth() - HealthDamage);
                        }
                        TargetStats->Health = FMath::Max(0.0f, TargetStats->Health);


                        // --- Rest of the Actor logic (RPCs, Abilities, State Changes etc.) ---
                        StrongAttacker->LevelData.Experience++;

                        if(StrongTarget->HealthWidgetComp) // Update Target's health bar
                        {
                            if (UUnitBaseHealthBar* HealthBarWidget = Cast<UUnitBaseHealthBar>(StrongTarget->HealthWidgetComp->GetUserWidgetObject()))
                            {
                            	HealthBarWidget->UpdateWidget();
                            }
                        }
	
                        StrongAttacker->ServerMeeleImpactEvent();
                        StrongTarget->ActivateAbilityByInputID(StrongTarget->DefensiveAbilityID, StrongTarget->DefensiveAbilities);
                        StrongTarget->FireEffects(StrongAttacker->MeleeImpactVFX, StrongAttacker->MeleeImpactSound, StrongAttacker->ScaleImpactVFX, StrongAttacker->ScaleImpactSound, StrongAttacker->MeeleImpactVFXDelay, StrongAttacker->MeleeImpactSoundDelay);

                    	
                    	GTEntityManager.Defer().AddTag<FMassStateDetectTag>(TargetEntity);

                    	UnitData::EState CurrentTargetState = StrongTarget->GetUnitState();
			
						if (CurrentTargetState != UnitData::Run &&
							CurrentTargetState != UnitData::Pause &&
							CurrentTargetState != UnitData::Casting &&
							CurrentTargetState != UnitData::Rooted &&
							CurrentTargetState != UnitData::Attack &&
							CurrentTargetState != UnitData::IsAttacked)
						{
          
                        	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
							if (SignalSubsystem)
							{
								TargetAIStateFragment->StateTimer = 0.f;
								SignalSubsystem->SignalEntity(UnitSignals::IsAttacked, TargetEntity);
							}
                        }
                        else if(StrongTarget->GetUnitState() == UnitData::Casting)
                        {
                        	TargetAIStateFragment->StateTimer -= StrongTarget->ReduceCastTime;
	                        StrongTarget->UnitControlTimer -= StrongTarget->ReduceCastTime;
                        }
                        else if(StrongTarget->GetUnitState() == UnitData::Rooted)
                        {
                        	TargetAIStateFragment->StateTimer -= StrongTarget->ReduceRootedTime;
	                        StrongTarget->UnitControlTimer -= StrongTarget->ReduceRootedTime;
                        }
                    }
                }); // End AsyncTask Lambda
             } // End check for valid UnitBase, Target, etc.
          } // End IsValid(Actor)
       } // End ActorFragPtr check
    } // End loop through Entities
}

void UUnitStateProcessor::UnitRangedAttack(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    if (!EntitySubsystem)
    {
       return;
    }

    const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager(); // Const is fine for initial checks

    for (const FMassEntityHandle& Entity : Entities)
    {
       if (!EntityManager.IsEntityValid(Entity)) continue;

       const FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
       if (ActorFragPtr)
       {
          const AActor* Actor = ActorFragPtr->Get();
          if (IsValid(Actor))
          {
             const AUnitBase* AttackerUnitBase = Cast<AUnitBase>(Actor);
             AUnitBase* TargetUnitBase = AttackerUnitBase ? AttackerUnitBase->UnitToChase : nullptr;
             // Need Attacker's TargetFragment to get the TargetEntity Handle *before* the lambda
             const FMassAITargetFragment* AttackerTargetFrag = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Entity);

             // --- Prerequisites Check ---
             if (AttackerUnitBase && IsValid(TargetUnitBase) && AttackerUnitBase->UseProjectile &&
                 AttackerTargetFrag && AttackerTargetFrag->bHasValidTarget && AttackerTargetFrag->TargetEntity.IsSet())
             {
                // Get Entity Handles
                FMassEntityHandle AttackerEntity = Entity;
                const FMassEntityHandle TargetEntity = AttackerTargetFrag->TargetEntity;
             	
                // Check target entity validity *before* dispatching
                if (!EntityManager.IsEntityValid(TargetEntity))
                {
                    //UE_LOG(LogTemp, Warning, TEXT("UnitRangedAttack: Target entity %d:%d is invalid for attacker %s."), TargetEntity.Index, TargetEntity.SerialNumber, *AttackerUnitBase->GetName());
                    continue;
                }

                // --- Capture necessary data ---
                TWeakObjectPtr<AUnitBase> WeakAttacker(const_cast<AUnitBase*>(AttackerUnitBase));
                TWeakObjectPtr<AUnitBase> WeakTarget(TargetUnitBase);
                // Capture Ability info/range by value
                EGASAbilityInputID AttackAbilityID = AttackerUnitBase->AttackAbilityID;
                EGASAbilityInputID ThrowAbilityID = AttackerUnitBase->ThrowAbilityID;
                EGASAbilityInputID OffensiveAbilityID = AttackerUnitBase->OffensiveAbilityID;
                TArray<TSubclassOf<UGameplayAbilityBase>> AttackAbilities = AttackerUnitBase->AttackAbilities;
                TArray<TSubclassOf<UGameplayAbilityBase>> ThrowAbilities = AttackerUnitBase->ThrowAbilities;
                TArray<TSubclassOf<UGameplayAbilityBase>> OffensiveAbilities = AttackerUnitBase->OffensiveAbilities;
                float AttackerRange = AttackerUnitBase->Attributes ? AttackerUnitBase->Attributes->GetRange() : 0.0f;
             	
                // --- Dispatch AsyncTask ---
                AsyncTask(ENamedThreads::GameThread,
                   [this, AttackerEntity, TargetEntity, // Capture entity handles
                    WeakAttacker, WeakTarget,
                    AttackAbilityID, ThrowAbilityID, OffensiveAbilityID,
                    AttackAbilities, ThrowAbilities, OffensiveAbilities, AttackerRange]() mutable
                {
                    // --- Get Strong Pointers ---
                    AUnitBase* StrongAttacker = WeakAttacker.Get();
                    AUnitBase* StrongTarget = WeakTarget.Get();

                    if (!EntitySubsystem) { return; }
                    FMassEntityManager& GTEntityManager = EntitySubsystem->GetMutableEntityManager(); // Need mutable for fragments
                   	
                    // --- Check validity ON Game Thread ---
                    if (StrongAttacker && StrongTarget && GTEntityManager.IsEntityValid(AttackerEntity) && GTEntityManager.IsEntityValid(TargetEntity))
                    {
                        UWorld* World = StrongAttacker->GetWorld();
                        if (!World) { return; }
                        UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>(); // Get once


                        // --- Perform Core Actor Actions ---
                        StrongAttacker->ServerStartAttackEvent_Implementation();
                    	
						bool bIsActivated = StrongAttacker->ActivateAbilityByInputID(AttackAbilityID, AttackAbilities);

                    
						if (!bIsActivated)
						{
							bIsActivated = StrongAttacker->ActivateAbilityByInputID(ThrowAbilityID, ThrowAbilities);
						}
                    	if (!bIsActivated)
                    	{
							//if (AttackerRange >= 600.f)
							{
							   bIsActivated = StrongAttacker->ActivateAbilityByInputID(OffensiveAbilityID, OffensiveAbilities);
							}
                    	}
                    	
                    	if (bIsActivated)
                    	{
                    		FMassAIStateFragment* AttackerStats = GTEntityManager.GetFragmentDataPtr<FMassAIStateFragment>(AttackerEntity);
                    		AttackerStats->SwitchingState = false;
                    		return;
                    	}

       
                        StrongAttacker->SpawnProjectile(StrongTarget, StrongAttacker);
                    	
                        const FMassCombatStatsFragment* TargetStats = GTEntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TargetEntity);
                        FMassAIStateFragment* TargetAIStateFragment = GTEntityManager.GetFragmentDataPtr<FMassAIStateFragment>(TargetEntity);
 
                    	if (TargetStats)
                        {
     
                                 // Check fragment needed for state timer changes
                                 if (!TargetAIStateFragment)
                                 {
                                     // UE_LOG(LogTemp, Warning, TEXT("UnitRangedAttack (GameThread): Missing Target AIStateFragment for timer updates. Target Entity %d:%d"), TargetEntity.Index, TargetEntity.SerialNumber);
                                 }

                                 // Get target's current Actor state
                                 UnitData::EState CurrentTargetState = StrongTarget->GetUnitState();

                    		if (CurrentTargetState != UnitData::Run &&
								   CurrentTargetState != UnitData::Pause &&
								   CurrentTargetState != UnitData::Casting &&
								   CurrentTargetState != UnitData::Rooted &&
								   CurrentTargetState != UnitData::Attack&&
									CurrentTargetState != UnitData::IsAttacked)
                                 {
                                     if (SignalSubsystem)
                                     {
                                     		TargetAIStateFragment->StateTimer = 0.f;
                                         SignalSubsystem->SignalEntity(UnitSignals::IsAttacked, TargetEntity);
                                     }
                                     // Let IsAttacked signal handler change Actor state
                                 }
                                 else if (CurrentTargetState == UnitData::Casting)
                                 {
                                     if (TargetAIStateFragment) TargetAIStateFragment->StateTimer -= StrongTarget->ReduceCastTime;
                                     StrongTarget->UnitControlTimer -= StrongTarget->ReduceCastTime; // Assuming ok on game thread
                                 }
                                 else if (CurrentTargetState == UnitData::Rooted)
                                 {
                                     if (TargetAIStateFragment) TargetAIStateFragment->StateTimer -= StrongTarget->ReduceRootedTime;
                                     StrongTarget->UnitControlTimer -= StrongTarget->ReduceRootedTime; // Assuming ok on game thread
                                 }
                             // --- End Actual Post-Attack Logic ---
                    		SwitchState(UnitSignals::Attack, AttackerEntity, GTEntityManager);
                        } // End else block (TargetStats was valid)
                    } // End check Actors/Entities valid
                    else
                    {
                        // UE_LOG(LogTemp, Warning, TEXT("UnitRangedAttack (GameThread): Attacker/Target Actor/Entity became invalid before post-attack logic."));
                    }
                }); // End AsyncTask Lambda
             } // End prerequisite check
          } // End IsValid(Actor)
       } // End ActorFragPtr check
    } // End loop through Entities
}

void UUnitStateProcessor::SetUnitToChase(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    if (!EntitySubsystem) return; // Early exit if subsystem is invalid

    // Use GetMutableEntityManager to allow getting mutable fragment data/actor pointers
    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

    for (FMassEntityHandle& DetectorEntity : Entities)
    {
        // Minimal validation for the detector entity itself
        if (!EntityManager.IsEntityValid(DetectorEntity)) continue;

        // Get Detector's Actor (non-const)
        // Use GetMutableFragmentDataPtr to potentially get a non-const Actor pointer
        FMassActorFragment* DetectorActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(DetectorEntity);
        // Use GetMutable() to get AActor* instead of const AActor*
        AUnitBase* DetectorUnitBase = DetectorActorFrag ? Cast<AUnitBase>(DetectorActorFrag->GetMutable()) : nullptr;

        // We MUST have the detector's AUnitBase to proceed
        if (!IsValid(DetectorUnitBase)) continue;

        // --- Core Logic: Get Target from Detector's Fragment ---
        // Target Fragment is read-only here, so GetFragmentDataPtr (const) is fine
        const FMassAITargetFragment* TargetFrag = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(DetectorEntity);

        AUnitBase* TargetUnitBase = nullptr; // Assume no valid target initially

        // Check if fragment exists and indicates a valid target entity
        if (TargetFrag && TargetFrag->bHasValidTarget && TargetFrag->TargetEntity.IsSet())
        {
            const FMassEntityHandle TargetEntity = TargetFrag->TargetEntity;

            // Validate the target entity and get its Actor (non-const)
            if (EntityManager.IsEntityValid(TargetEntity))
            {
                // Use GetMutableFragmentDataPtr to potentially get a non-const Actor pointer
                FMassActorFragment* TargetActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(TargetEntity);
                // Use GetMutable() to get AActor* instead of const AActor*
                AActor* TargetActor = TargetActorFrag ? TargetActorFrag->GetMutable() : nullptr;

                if (IsValid(TargetActor) && TargetActor != DetectorUnitBase)
                {
                    // Attempt to cast target actor to AUnitBase
                    TargetUnitBase = Cast<AUnitBase>(TargetActor);
                    // If cast fails, TargetUnitBase will be nullptr, correctly clearing the chase target.
                }
            }
            // If TargetEntity is invalid, TargetActorFrag is null, or TargetActor is invalid/self, TargetUnitBase remains nullptr.
        }
        // If TargetFrag is null or doesn't indicate a valid target, TargetUnitBase remains nullptr.

        // --- Assign the result ---
        // Assign only if the target has actually changed
        if (DetectorUnitBase->UnitToChase != TargetUnitBase)
        {
            DetectorUnitBase->UnitToChase = TargetUnitBase;
            // Optional: Add very minimal log or trigger essential logic
        }

    	/*
    	if (!DoesEntityHaveTag(EntityManager , DetectorEntity, FMassStateIsAttackedTag::StaticStruct()) &&
    		!DoesEntityHaveTag(EntityManager , DetectorEntity, FMassStateAttackTag::StaticStruct()) &&
    		!DoesEntityHaveTag(EntityManager , DetectorEntity, FMassStatePauseTag::StaticStruct()) &&
    		EntityManager.IsEntityValid(TargetFrag->TargetEntity) &&
    		DetectorUnitBase->UnitToChase)
        {
	        SwitchState(UnitSignals::Chase, DetectorEntity, EntityManager);
        }*/
    }
}


void UUnitStateProcessor::HandleStartDead(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    // **Keep initial checks outside AsyncTask if possible and thread-safe**
    if (!EntitySubsystem)
    {
        // Log error - This check itself is generally safe
        return;
    }

    TArray<FMassEntityHandle> EntitiesCopy = Entities; 

    AsyncTask(ENamedThreads::GameThread, [this, SignalName, EntitiesCopy]() mutable // mutable if you modify captures (not needed here)
    {
        // **Code inside this lambda now runs on the Game Thread**

        // Re-check EntitySubsystem just in case? Usually fine if 'this' is valid.
        if (!EntitySubsystem) 
        {
             return;
        }

        FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

        for (const FMassEntityHandle& Entity : EntitiesCopy) // Iterate the captured copy
        {
            // Check entity validity *on the game thread*
            if (!EntityManager.IsEntityValid(Entity)) 
            {
                continue;
            }
            
            // Get fragments and actors *on the game thread*
            FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
        	FMassAgentCharacteristicsFragment* CharFragPtr = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity);
        	if (ActorFragPtr)
            {
                AActor* Actor = ActorFragPtr->GetMutable(); 
                if (IsValid(Actor)) 
                {
                    AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
                    if (UnitBase)
                    {
                    	UnitBase->FireEffects(UnitBase->DeadVFX, UnitBase->DeadSound, UnitBase->ScaleDeadVFX, UnitBase->ScaleDeadSound, UnitBase->DelayDeadVFX, UnitBase->DelayDeadSound);
                    	UnitBase->HideHealthWidget(); // Aus deinem Code
                    	UnitBase->KillLoadedUnits();
                    	UnitBase->CanActivateAbilities = false;
                    	UnitBase->GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    	
                    	ARTSGameModeBase* RTSGameMode = Cast<ARTSGameModeBase>(GetWorld()->GetAuthGameMode());
						

							if(RTSGameMode)
							{
								RTSGameMode->AllUnits.Remove(UnitBase);
							}
	
                    	UnitBase->SpawnPickupsArray();
                    	
                    }
                }
            }
        } // End For loop
    }); // End AsyncTask Lambda
}

void UUnitStateProcessor::HandleEndDead(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    // **Keep initial checks outside AsyncTask if possible and thread-safe**
    if (!EntitySubsystem)
    {
        // Log error - This check itself is generally safe
        return;
    }

    TArray<FMassEntityHandle> EntitiesCopy = Entities; 

    AsyncTask(ENamedThreads::GameThread, [this, SignalName, EntitiesCopy]() mutable // mutable if you modify captures (not needed here)
    {
        // **Code inside this lambda now runs on the Game Thread**

        // Re-check EntitySubsystem just in case? Usually fine if 'this' is valid.
        if (!EntitySubsystem) 
        {
             return;
        }

        FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

        for (const FMassEntityHandle& Entity : EntitiesCopy) // Iterate the captured copy
        {
            // Check entity validity *on the game thread*
            if (!EntityManager.IsEntityValid(Entity)) 
            {
                continue;
            }
            
            // Get fragments and actors *on the game thread*
            FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
        	if (ActorFragPtr)
            {
                AActor* Actor = ActorFragPtr->GetMutable(); 
                if (IsValid(Actor)) 
                {
                    AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
                    if (UnitBase)
                    {
                    		if (UnitBase->DestroyAfterDeath)
                    		{
                    			UnitBase->Destroy(true, false);
							}
                    }
                }
            }
        } // End For loop
    }); // End AsyncTask Lambda
}

void UUnitStateProcessor::HandleGetResource(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	// **Keep initial checks outside AsyncTask if possible and thread-safe**
	if (!EntitySubsystem)
	{
		// Log error - This check itself is generally safe
		return;
	}

	TArray<FMassEntityHandle> EntitiesCopy = Entities; 

	AsyncTask(ENamedThreads::GameThread, [this, SignalName, EntitiesCopy]() mutable // mutable if you modify captures (not needed here)
	{
		if (!EntitySubsystem) 
		{
			 return;
		}

		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

		for (FMassEntityHandle& Entity : EntitiesCopy) // Iterate the captured copy
		{
			// Check entity validity *on the game thread*
			if (!EntityManager.IsEntityValid(Entity)) 
			{
				continue;
			}
			// 4. Call the target method. This is now safe because:
			//    - We are on the Game Thread.
			//    - We have validated Worker and Worker->ResourcePlace pointers.
			FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
			if (ActorFragPtr)
			{
				AActor* Actor = ActorFragPtr->GetMutable(); 
				if (IsValid(Actor))
				{
					AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
					if (UnitBase && IsValid(UnitBase->ResourcePlace))
					{
						SpawnWorkResource(UnitBase->ExtractingWorkResourceType, UnitBase->GetActorLocation(), UnitBase->ResourcePlace->WorkResourceClass, UnitBase);
						UnitBase->UnitControlTimer = 0;
						UnitBase->SetUEPathfinding = true;
						SwitchState(UnitSignals::GoToBase, Entity, EntityManager);
					}
				}
			}
		}
    }); 
}

void UUnitStateProcessor::HandleReachedBase(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	// **Keep initial checks outside AsyncTask if possible and thread-safe**
	if (!EntitySubsystem)
	{
		// Log error - This check itself is generally safe
		return;
	}

	TArray<FMassEntityHandle> EntitiesCopy = Entities; 

	AsyncTask(ENamedThreads::GameThread, [this, SignalName, EntitiesCopy]() mutable // mutable if you modify captures (not needed here)
	{
		if (!EntitySubsystem) 
		{
			 return;
		}

		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

		for (const FMassEntityHandle& Entity : EntitiesCopy) // Iterate the captured copy
		{
			// Check entity validity *on the game thread*
			if (!EntityManager.IsEntityValid(Entity)) 
			{
				continue;
			}
			// 4. Call the target method. This is now safe because:
			//    - We are on the Game Thread.
			//    - We have validated Worker and Worker->ResourcePlace pointers.
			FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
			FMassAIStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
			if (ActorFragPtr)
			{
				AActor* Actor = ActorFragPtr->GetMutable(); 
				if (IsValid(Actor))
				{
					AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
					if (UnitBase && UnitBase->IsWorker && IsValid(UnitBase->ResourcePlace))
					{
						if (!ResourceGameMode)
							ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

						bool CanAffordConstruction = false;

						if(UnitBase->BuildArea && UnitBase->BuildArea->IsPaid)
							CanAffordConstruction = true;
						else	
							CanAffordConstruction = UnitBase->BuildArea? ResourceGameMode->CanAffordConstruction(UnitBase->BuildArea->ConstructionCost, UnitBase->TeamId) : false; //Worker->BuildArea->CanAffordConstruction(Worker->TeamId, ResourceGameMode->NumberOfTeams,ResourceGameMode->TeamResources) : false;
						
						if (ResourceGameMode)
							UnitBase->Base->HandleBaseArea(UnitBase, ResourceGameMode, CanAffordConstruction);

						StateFrag->SwitchingState = false;
					}
				}
			}
		}
	}); 
}

void UUnitStateProcessor::HandleGetClosestBaseArea(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	// **Keep initial checks outside AsyncTask if possible and thread-safe**
	if (!EntitySubsystem)
	{
		// Log error - This check itself is generally safe
		return;
	}

	TArray<FMassEntityHandle> EntitiesCopy = Entities; 

	AsyncTask(ENamedThreads::GameThread, [this, SignalName, EntitiesCopy]() mutable // mutable if you modify captures (not needed here)
	{
		if (!EntitySubsystem) 
		{
			 return;
		}

		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

		for (const FMassEntityHandle& Entity : EntitiesCopy) // Iterate the captured copy
		{
			// Check entity validity *on the game thread*
			if (!EntityManager.IsEntityValid(Entity)) 
			{
				continue;
			}
			// 4. Call the target method. This is now safe because:
			//    - We are on the Game Thread.
			//    - We have validated Worker and Worker->ResourcePlace pointers.
			FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
			FMassAIStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
			if (ActorFragPtr)
			{
				AActor* Actor = ActorFragPtr->GetMutable(); 
				if (IsValid(Actor))
				{
					AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
					if (UnitBase )
					{
						if (!ResourceGameMode)
							ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());

						UnitBase->Base = ResourceGameMode->GetClosestBaseFromArray(UnitBase, ResourceGameMode->WorkAreaGroups.BaseAreas);
						StateFrag->SwitchingState = false;
					}
				}
			}
		}
	}); 
}

void UUnitStateProcessor::HandleSpawnBuildingRequest(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	// **Keep initial checks outside AsyncTask if possible and thread-safe**
    	if (!EntitySubsystem)
    	{
    		// Log error - This check itself is generally safe
    		return;
    	}
    
    	TArray<FMassEntityHandle> EntitiesCopy = Entities; 
    
    	AsyncTask(ENamedThreads::GameThread, [this, SignalName, EntitiesCopy]() mutable // mutable if you modify captures (not needed here)
    	{
    		if (!EntitySubsystem) 
    		{
    			 return;
    		}
    
    		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    
    		for (FMassEntityHandle& Entity : EntitiesCopy) // Iterate the captured copy
    		{
    			// Check entity validity *on the game thread*
    			if (!EntityManager.IsEntityValid(Entity)) 
    			{
    				continue;
    			}
    			// 4. Call the target method. This is now safe because:
    			//    - We are on the Game Thread.
    			//    - We have validated Worker and Worker->ResourcePlace pointers.
    			FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
    			FMassAIStateFragment* StateFragment = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
    			if (ActorFragPtr)
    			{
    				AActor* Actor = ActorFragPtr->GetMutable(); 
    				if (IsValid(Actor))
    				{
    					AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
    					if (UnitBase && UnitBase->BuildArea)
    					{
    						if(!UnitBase->BuildArea->Building)
    						{
								FUnitSpawnParameter SpawnParameter;
								SpawnParameter.UnitBaseClass = UnitBase->BuildArea->BuildingClass;
								SpawnParameter.UnitControllerBaseClass = UnitBase->BuildArea->BuildingController;
								SpawnParameter.UnitOffset = FVector(0.f, 0.f, UnitBase->BuildArea->BuildZOffset);
								SpawnParameter.UnitMinRange = FVector(0.f);
								SpawnParameter.UnitMaxRange = FVector(0.f);
								SpawnParameter.ServerMeshRotation = UnitBase->BuildArea->ServerMeshRotationBuilding;
								SpawnParameter.State = UnitData::Idle;
								SpawnParameter.StatePlaceholder = UnitData::Idle;
								SpawnParameter.Material = nullptr;

								FVector ActorLocation = UnitBase->BuildArea->GetActorLocation();
								if(UnitBase->BuildArea && UnitBase->BuildArea->DestroyAfterBuild)
								{
									UnitBase->BuildArea->RemoveAreaFromGroup();
									UnitBase->BuildArea->Destroy(false, true);
									UnitBase->BuildArea = nullptr;
								}

    							if(!ControllerBase)
    							{
									ControllerBase = Cast<AExtendedControllerBase>(GetWorld()->GetFirstPlayerController());
								}

								AUnitBase* NewUnit = SpawnSingleUnit(SpawnParameter, ActorLocation, nullptr, UnitBase->TeamId, nullptr);

								if (NewUnit && ControllerBase)
								{
									NewUnit->IsMyTeam = true;
									NewUnit->SpawnFogOfWarManager(ControllerBase);
									UnitBase->FinishedBuild();
								}
		
			
								if(NewUnit)
								{
									ABuildingBase* Building = Cast<ABuildingBase>(NewUnit);
									if (Building && UnitBase->BuildArea && UnitBase->BuildArea->NextWaypoint)
										Building->NextWaypoint = UnitBase->BuildArea->NextWaypoint;

									if (Building && UnitBase->BuildArea && !UnitBase->BuildArea->DestroyAfterBuild)
										UnitBase->BuildArea->Building = Building;
								}
							}
    					}
    					SwitchState(StateFragment->PlaceholderSignal, Entity, EntityManager);
    				}
    			}
    		}
    	}); 
}

AUnitBase* UUnitStateProcessor::SpawnSingleUnit(
    FUnitSpawnParameter SpawnParameter,
    FVector Location,
    AUnitBase* UnitToChase,
    int TeamId,
    AWaypoint* Waypoint)
{
    // 1) Transformation mit (vorläufigem) Spawn-Ort
    FTransform EnemyTransform;
    EnemyTransform.SetLocation(
        FVector(Location.X + SpawnParameter.UnitOffset.X,
                Location.Y + SpawnParameter.UnitOffset.Y,
                Location.Z + SpawnParameter.UnitOffset.Z)
    );

    // 2) Deferrten Actor-Spawn starten
    AUnitBase* UnitBase = Cast<AUnitBase>(
        UGameplayStatics::BeginDeferredActorSpawnFromClass(
            this,
            SpawnParameter.UnitBaseClass,
            EnemyTransform,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn
        )
    );

    if (!UnitBase)
    {
        return nullptr;
    }
	
    // 3) (Optional) AI-Controller spawnen und zuweisen
	/*
    if (SpawnParameter.UnitControllerBaseClass)
    {
        AAIController* UnitController = GetWorld()->SpawnActor<AAIController>(
            SpawnParameter.UnitControllerBaseClass, 
            FTransform()
        );
        if (!UnitController) 
        {
            return nullptr;
        }
        UnitController->Possess(UnitBase);
    }*/

    // --------------------------------------------
    // 4) Hier folgt nun der Part mit dem LineTrace
    // --------------------------------------------

    // Wir versuchen, das Mesh zu holen (falls vorhanden),
    // um dessen Bounds und Höhe zu bestimmen
    USkeletalMeshComponent* MeshComponent = UnitBase->GetMesh();
    if (MeshComponent)
    {
        // Bounds in Weltkoordinaten (inkl. eventuell eingestellter Scale)
        FBoxSphereBounds MeshBounds = MeshComponent->Bounds;
        float HalfHeight = MeshBounds.BoxExtent.Z;

        // Wir machen einen sehr simplen Trace von "hoch oben" nach "weit unten"
        FVector TraceStart = Location + FVector(0.f, 0.f, 100.f);
        FVector TraceEnd   = Location - FVector(0.f, 0.f, 100.f);

        FHitResult HitResult;
        FCollisionQueryParams TraceParams(FName(TEXT("UnitSpawnTrace")), true, UnitBase);
        TraceParams.bTraceComplex = true;
        TraceParams.AddIgnoredActor(UnitBase);
    	
    	if (ControllerBase)
    		TraceParams.AddIgnoredActor(ControllerBase->GetPawn());

        bool bHit = GetWorld()->LineTraceSingleByChannel(
            HitResult,
            TraceStart,
            TraceEnd,
            ECC_Visibility,
            TraceParams
        );

        if (bHit)
        {
            // Wenn wir etwas treffen (z.B. Boden),
            // setzen wir den Z-Wert so, dass die Unterseite des Meshes
            // auf der Boden-Kollisionsstelle aufsitzt.
            // -> ImpactPoint.Z plus "halbe Mesh-Höhe"
            Location.Z = HitResult.ImpactPoint.Z + HalfHeight;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SpawnSingleUnit: Kein Boden getroffen, verwende Standard-Z"));
        }

        // Aktualisiere EnemyTransform mit dem angepassten Z-Wert
        EnemyTransform.SetLocation(Location);
    }
	
	if (TeamId)
	{
		UnitBase->TeamId = TeamId;
	}
	
    // --------------------------------------------
    // 5) Jetzt wird der Actor final in die Welt gesetzt
    // --------------------------------------------
    UGameplayStatics::FinishSpawningActor(UnitBase, EnemyTransform);


    // 6) Ab hier folgt dein bisheriger Setup-Code
    if (UnitBase->UnitToChase)
    {
        UnitBase->UnitToChase = UnitToChase;
        UnitBase->SetUnitState(UnitData::Chase);
    }

    if (TeamId)
    {
        UnitBase->TeamId = TeamId;
    }

    UnitBase->ServerMeshRotation = SpawnParameter.ServerMeshRotation;
    UnitBase->OnRep_MeshAssetPath();
    UnitBase->OnRep_MeshMaterialPath();

    UnitBase->SetReplicateMovement(true);
	UnitBase->SetReplicates(true);
    //UnitBase->GetMesh()->SetIsReplicated(true);

    // Meshrotation-Server
    UnitBase->SetMeshRotationServer();

    UnitBase->UnitState = SpawnParameter.State;
    UnitBase->UnitStatePlaceholder = SpawnParameter.StatePlaceholder;

    // Attribute & GameMode-Handling
    UnitBase->InitializeAttributes();
    AResourceGameMode* GameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());
    if (!GameMode)
    {
        UnitBase->SetUEPathfinding = true;
        UnitBase->SetUnitState(UnitData::GoToResourceExtraction);
        return nullptr;
    }

    GameMode->AddUnitIndexAndAssignToAllUnitsArray(UnitBase);
	UnitBase->ScheduleDelayedNavigationUpdate();
    return UnitBase;
}


void UUnitStateProcessor::SpawnWorkResource(EResourceType ResourceType, FVector Location, TSubclassOf<class AWorkResource> WRClass, AUnitBase* ActorToLockOn)
{


	if (!WRClass) return;
	
	FTransform Transform;

	Transform.SetLocation(Location);
	Transform.SetRotation(FQuat(FRotator::ZeroRotator)); // FRotator::ZeroRotator

		
	const auto MyWorkResource = Cast<AWorkResource>
						(UGameplayStatics::BeginDeferredActorSpawnFromClass
						(this, WRClass, Transform,  ESpawnActorCollisionHandlingMethod::AlwaysSpawn));
	
	if (MyWorkResource != nullptr)
	{

		if (!ActorToLockOn || !ActorToLockOn->ResourcePlace || !ActorToLockOn->ResourcePlace->Mesh)
		{
			return;
		}
		
			if(ActorToLockOn->WorkResource) ActorToLockOn->WorkResource->Destroy(true);
		
	
			//MyWorkResource->AttachToComponent(ActorToLockOn->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("ResourceSocket"));
			MyWorkResource->IsAttached = true;
			MyWorkResource->ResourceType = ResourceType;
			
			UGameplayStatics::FinishSpawningActor(MyWorkResource, Transform);
			
			ActorToLockOn->WorkResource = MyWorkResource;

			
			ActorToLockOn->ResourcePlace->AvailableResourceAmount = ActorToLockOn->ResourcePlace->AvailableResourceAmount - ActorToLockOn->WorkResource->Amount;

			if (ActorToLockOn->ResourcePlace->AvailableResourceAmount == 0.f)
			{
				ActorToLockOn->ResourcePlace->Destroy(true);
				ActorToLockOn->ResourcePlace = nullptr;
			}else
			{
				// Retrieve the available and maximum resource amounts
				float Available = ActorToLockOn->ResourcePlace->AvailableResourceAmount;
				float MaxAvailable = ActorToLockOn->ResourcePlace->MaxAvailableResourceAmount;

				// Default scale if MaxAvailable is zero
				float NewScale = 0.4f;

				if (!FMath::IsNearlyZero(MaxAvailable))
				{
					// Compute the ratio and clamp it between 0.0 and 1.0 for safety
					float Ratio = FMath::Clamp(Available / MaxAvailable, 0.0f, 1.0f);

					// Linear interpolation: scale will be 0.4 when Ratio is 0 and 1.0 when Ratio is 1
					NewScale = 0.4f + Ratio * (1.0f - 0.4f);
				}
			
				// Set the mesh's uniform scale using the computed linear scale
				ActorToLockOn->ResourcePlace->Multicast_SetScale(FVector(NewScale));
			}

	
			/// Attach Socket with Delay //////////////
			FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);
			FName SocketName = FName("ResourceSocket");
			
			auto AttachWorkResource = [MyWorkResource, ActorToLockOn, AttachmentRules, SocketName]()
			{
				if (ActorToLockOn->GetMesh()->DoesSocketExist(SocketName))
				{
					MyWorkResource->AttachToComponent(ActorToLockOn->GetMesh(), AttachmentRules, SocketName);
					MyWorkResource->IsAttached = true;
					// Now attempt to set the actor's relative location after attachment
					MyWorkResource->SetActorRelativeLocation(MyWorkResource->SocketOffset, false, nullptr, ETeleportType::TeleportPhysics);
				}
			};

			AttachWorkResource();
		
	}
	
}

void UUnitStateProcessor::DespawnWorkResource(AWorkResource* WorkResource)
{
	if (WorkResource != nullptr)
	{
		WorkResource->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		WorkResource->Destroy();
		WorkResource = nullptr;
	}
}

void UUnitStateProcessor::SyncCastTime(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	// **Keep initial checks outside AsyncTask if possible and thread-safe**
	if (!EntitySubsystem)
	{
		// Log error - This check itself is generally safe
		return;
	}
    
	TArray<FMassEntityHandle> EntitiesCopy = Entities; 
    
	AsyncTask(ENamedThreads::GameThread, [this, SignalName, EntitiesCopy]() mutable // mutable if you modify captures (not needed here)
	{
		if (!EntitySubsystem) 
		{
			 return;
		}
    
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    
		for (const FMassEntityHandle& Entity : EntitiesCopy) // Iterate the captured copy
		{
			// Check entity validity *on the game thread*
			if (!EntityManager.IsEntityValid(Entity)) 
			{
				continue;
			}
			// 4. Call the target method. This is now safe because:
			//    - We are on the Game Thread.
			//    - We have validated Worker and Worker->ResourcePlace pointers.
			FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
			FMassAIStateFragment* StateFrag =  EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);

			if (ActorFragPtr)
			{
				AActor* Actor = ActorFragPtr->GetMutable(); 
				if (IsValid(Actor))
				{
					AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
					if (UnitBase )
					{
						
    					UnitBase->UnitControlTimer = StateFrag->StateTimer;
						/*UE_LOG(LogTemp, Error,
												 TEXT("SyncCastTime: [%s] (EntityHandle=%u) UnitControlTimer set to %.3f"),
												 *UnitBase->GetName(),
												 Entity.Index,             // or Entity.HashValue depending on your handle impl
												 StateFrag->StateTimer
											 );*/
					}
				}
			}
		}
	}); 
}


void UUnitStateProcessor::EndCast(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	// **Keep initial checks outside AsyncTask if possible and thread-safe**
	if (!EntitySubsystem)
	{
		// Log error - This check itself is generally safe
		return;
	}
    
	TArray<FMassEntityHandle> EntitiesCopy = Entities; 
    
	AsyncTask(ENamedThreads::GameThread, [this, SignalName, EntitiesCopy]() mutable // mutable if you modify captures (not needed here)
	{
		if (!EntitySubsystem) 
		{
			 return;
		}
    
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    
		for (FMassEntityHandle& Entity : EntitiesCopy) // Iterate the captured copy
		{
			// Check entity validity *on the game thread*
			if (!EntityManager.IsEntityValid(Entity)) 
			{
				continue;
			}
			// 4. Call the target method. This is now safe because:
			//    - We are on the Game Thread.
			//    - We have validated Worker and Worker->ResourcePlace pointers.
			FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
			FMassAIStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
			if (ActorFragPtr)
			{
				AActor* Actor = ActorFragPtr->GetMutable(); 
				if (IsValid(Actor))
				{
					AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
					if (UnitBase )
					{
						if (UnitBase->ActivatedAbilityInstance)
						{
							UnitBase->ActivatedAbilityInstance->OnAbilityCastComplete();
						}
						StateFrag->StateTimer = 0.f;
						UnitBase->UnitControlTimer = 0.f;

						UE_LOG(LogTemp, Error, TEXT("Switching state with PlaceholderSignal: %s"), *StateFrag->PlaceholderSignal.ToString());
						SwitchState(StateFrag->PlaceholderSignal, Entity, EntityManager);
					}
				}
			}
		}
	}); 
}


void UUnitStateProcessor::SetToUnitStatePlaceholder(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	// **Keep initial checks outside AsyncTask if possible and thread-safe**
	if (!EntitySubsystem)
	{
		// Log error - This check itself is generally safe
		return;
	}
    
	TArray<FMassEntityHandle> EntitiesCopy = Entities; 
    
	AsyncTask(ENamedThreads::GameThread, [this, SignalName, EntitiesCopy]() mutable // mutable if you modify captures (not needed here)
	{
		if (!EntitySubsystem) 
		{
			 return;
		}
    
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    
		for (FMassEntityHandle& Entity : EntitiesCopy) // Iterate the captured copy
		{
			// Check entity validity *on the game thread*
			if (!EntityManager.IsEntityValid(Entity)) 
			{
				continue;
			}
			// 4. Call the target method. This is now safe because:
			//    - We are on the Game Thread.
			//    - We have validated Worker and Worker->ResourcePlace pointers.
			FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
			FMassAIStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
			if (ActorFragPtr)
			{
				AActor* Actor = ActorFragPtr->GetMutable(); 
				if (IsValid(Actor))
				{
					AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
					if (UnitBase )
					{
						StateFrag->StateTimer = 0.f;
						UnitBase->UnitControlTimer = 0.f;
						//UE_LOG(LogTemp, Error, TEXT("A PlaceholderSignal = %s"), *StateFrag->PlaceholderSignal.ToString());
						SwitchState(StateFrag->PlaceholderSignal, Entity, EntityManager);
					}
				}
			}
		}
	});
}


void UUnitStateProcessor::HandleSightSignals(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	if (!EntitySubsystem) 
	{
		return;
	}
    
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    
	if (EntityManager.IsEntityValid(Entities[0]) && EntityManager.IsEntityValid(Entities[1]) ) // Iterate the captured copy
	{
		// Check entity validity *on the game thread*

		FMassActorFragment* TargetActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entities[0]);
		FMassActorFragment* DetectorActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entities[1]);
		if (TargetActorFragPtr)
		{
			AActor* TargetActor = TargetActorFragPtr->GetMutable();
			AActor* DetectorActor = DetectorActorFragPtr->GetMutable(); 
			if (IsValid(TargetActor))
			{
				AUnitBase* TargetUnitBase = Cast<AUnitBase>(TargetActor);
				AUnitBase* DetectorUnitBase = Cast<AUnitBase>(DetectorActor);
				if (TargetUnitBase )
				{
					// Check Signal Name
					if (SignalName == UnitSignals::UnitEnterSight)
					{
						//UE_LOG(LogTemp, Log, TEXT("/// -> DetectorUnitBase>TeamId: %d"), DetectorUnitBase->TeamId)
						TargetUnitBase->MulticastSetEnemyVisibility(DetectorUnitBase, true);
					}
					// Check Signal Name
						
					if (SignalName == UnitSignals::UnitExitSight)
					{
						//if (DetectorUnitBase->TeamId == 1)
						//UE_LOG(LogTemp, Log, TEXT("/// -> DetectorUnitBase>TeamId: %d"), DetectorUnitBase->TeamId)
						
						TargetUnitBase->MulticastSetEnemyVisibility(DetectorUnitBase,false);
					}
				}
			}
		}
	}
	
}


void UUnitStateProcessor::HandleUpdateFogMask(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	if (!EntitySubsystem || !World) return;

	APlayerController* PC = World->GetFirstPlayerController(); // Local controller
	if (!PC) return;

	ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(PC);
	if (!CustomPC) return;

	// Must be run on GameThread to access UObjects
	TArray<FMassEntityHandle> CopiedEntities = Entities;

	AsyncTask(ENamedThreads::GameThread, [CustomPC, CopiedEntities = MoveTemp(CopiedEntities)]()
	{
		CustomPC->UpdateFogMaskWithCircles(CopiedEntities);
	});
}
/*
void UUnitStateProcessor::HandleUpdateFogMask(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	if (!EntitySubsystem) return;
	if (!World) return;

	ARTSGameModeBase* GM = Cast<ARTSGameModeBase>(World->GetAuthGameMode());
	if (!GM) return;

	// 1. Call multicast from GameThread
	TArray<FMassEntityHandle> CopiedEntities = Entities;

	AsyncTask(ENamedThreads::GameThread, [GM, CopiedEntities = MoveTemp(CopiedEntities)]()
	{
		GM->MulticastUpdateFogMaskWithCircles(CopiedEntities);
	});
}
*/

void UUnitStateProcessor::HandleUnitSpawnedSignal(
	FName SignalName,
	TArray<FMassEntityHandle>& Entities)
{
	const float Now = World->GetTimeSeconds();

	if (!EntitySubsystem) { return; }
    
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	
	for (FMassEntityHandle& E : Entities)
	{
		;
		// Grab the *target* fragment on that entity and stamp it
		FMassAIStateFragment* StateFragment = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(E);
        
		if (StateFragment)
		{
			StateFragment->BirthTime = Now;

			FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(E);
			
			if (ActorFragPtr)
			{
				AActor* TargetActor = ActorFragPtr->GetMutable();
				AUnitBase* Unit = Cast<AUnitBase>(TargetActor);
				Unit->SwitchEntityTagByState(Unit->UnitState, Unit->UnitStatePlaceholder);
			}
		}
	}
}
