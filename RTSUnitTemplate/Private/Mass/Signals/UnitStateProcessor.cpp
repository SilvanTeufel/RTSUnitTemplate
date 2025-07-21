// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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
#include "NavModifierComponent.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Mass/States/ChaseStateProcessor.h"
#include "Async/Async.h"
#include "NavAreas/NavArea_Obstacle.h"

UUnitStateProcessor::UUnitStateProcessor(): EntityQuery()
{
    ProcessingPhase = EMassProcessingPhase::PostPhysics; // Run fairly late
	bRequiresGameThreadExecution = true;
    bAutoRegisterWithProcessingPhases = true; // Don't need ticking execute
}

void UUnitStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    // Get subsystems
	World = Owner.GetWorld();

	if (!World) return;
	
    SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
    EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();

	if (EntitySubsystem)
	{
		//UE_LOG(LogTemp, Warning, TEXT("EntitySubsystem FOUND!!!!!!"));
	}
	
    if (SignalSubsystem)
    {
    	//UE_LOG(LogTemp, Warning, TEXT("SignalSubsystem FOUND!!!!!!"));
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

    	FogParametersDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateSelectionCircle)
			.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, HandleUpdateSelectionCircle));
    	
    	SyncUnitBaseDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SyncUnitBase)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, SyncUnitBase));
    	
    	SetUnitToChaseSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SetUnitToChase)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, SetUnitToChase));


    
    	RangedAbilitySignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UseRangedAbilitys)
		  .AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, UnitActivateRangedAbilities));
    	
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

    	UpdateWorkerMovementDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateWorkerMovement)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, UpdateWorkerMovement));
    	
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

	if (SignalSubsystem && SelectionCircleDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateSelectionCircle)
		.Remove(SelectionCircleDelegateHandle);
            
		SelectionCircleDelegateHandle.Reset();
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

	if (SignalSubsystem && RangedAbilitySignalDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::UseRangedAbilitys)
		.Remove(RangedAbilitySignalDelegateHandle);
            
		RangedAbilitySignalDelegateHandle.Reset();
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

	if (SignalSubsystem && UpdateWorkerMovementDelegateHandle.IsValid()) // Check if subsystem and handle are valid
	{
		SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateWorkerMovement)
		.Remove(UpdateWorkerMovementDelegateHandle);
            
		UpdateWorkerMovementDelegateHandle.Reset();
	}
	
    SignalSubsystem = nullptr; // Null out pointers if needed
    EntitySubsystem = nullptr;
    // --- End Cleanup ---
    Super::BeginDestroy();
}

void UUnitStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
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
            	UE_LOG(LogTemp, Error, TEXT("Entity or Manager is not Valid!"));
                return;
            }
            // Get fragments and actors *on the game thread*
            FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
			FMassAIStateFragment* StateFragment = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);

	
			//FMassCombatStatsFragment* CombatStatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity);
			//if (CombatStatsFrag->TeamId == 3)
			//UE_LOG(LogTemp, Error, TEXT("SwitchState! %s"), *SignalName.ToString());
	
	
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
                        if (SignalName != UnitSignals::Idle)EntityManager.Defer().RemoveTag<FMassStateIdleTag>(Entity);
                        if (SignalName != UnitSignals::Chase)EntityManager.Defer().RemoveTag<FMassStateChaseTag>(Entity);
                        if (SignalName != UnitSignals::Attack)EntityManager.Defer().RemoveTag<FMassStateAttackTag>(Entity);
                        if (SignalName != UnitSignals::Pause)EntityManager.Defer().RemoveTag<FMassStatePauseTag>(Entity);
                        //if (SignalName != UnitSignals::Dead)EntityManager.Defer().RemoveTag<FMassStateDeadTag>(Entity); 
                        if (SignalName != UnitSignals::Run)EntityManager.Defer().RemoveTag<FMassStateRunTag>(Entity);
                        if (SignalName != UnitSignals::PatrolRandom)EntityManager.Defer().RemoveTag<FMassStatePatrolRandomTag>(Entity);
                        if (SignalName != UnitSignals::PatrolIdle)EntityManager.Defer().RemoveTag<FMassStatePatrolIdleTag>(Entity);
                        if (SignalName != UnitSignals::Casting)EntityManager.Defer().RemoveTag<FMassStateCastingTag>(Entity);
                    	if (SignalName != UnitSignals::IsAttacked)EntityManager.Defer().RemoveTag<FMassStateIsAttackedTag>(Entity);

                    	if (SignalName != UnitSignals::GoToBase) EntityManager.Defer().RemoveTag<FMassStateGoToBaseTag>(Entity);
                    	if (SignalName != UnitSignals::GoToBuild) EntityManager.Defer().RemoveTag<FMassStateGoToBuildTag>(Entity);
                    	if (SignalName != UnitSignals::Build) EntityManager.Defer().RemoveTag<FMassStateBuildTag>(Entity);
                    	if (SignalName != UnitSignals::GoToResourceExtraction) EntityManager.Defer().RemoveTag<FMassStateGoToResourceExtractionTag>(Entity);
                    	if (SignalName != UnitSignals::ResourceExtraction) EntityManager.Defer().RemoveTag<FMassStateResourceExtractionTag>(Entity);
                    	
                        // --- Add new tag ---
                    	if (SignalName == UnitSignals::Idle)
                    	{
                    		if (StateFragment->CanAttack && StateFragment->IsInitialized) EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);
                    		
                    		EntityManager.Defer().AddTag<FMassStateIdleTag>(Entity);
                    		StateFragment->PlaceholderSignal = UnitSignals::Idle;
                    		UnitBase->UnitStatePlaceholder = UnitData::Idle;
         
                    	}
                        else if (SignalName == UnitSignals::Chase)
                        {
                        	if (StateFragment->CanAttack && StateFragment->IsInitialized) EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);
                        	
	                        EntityManager.Defer().AddTag<FMassStateChaseTag>(Entity);
                        }
                        else if (SignalName == UnitSignals::Attack)
                        {
                        	if (StateFragment->CanAttack && StateFragment->IsInitialized) EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);
                        	
	                        EntityManager.Defer().AddTag<FMassStateAttackTag>(Entity);
                        }
                        else if (SignalName == UnitSignals::Dead)
                        {
                        	StateFragment->DeathTime  = World->GetTimeSeconds();
	                        EntityManager.Defer().AddTag<FMassStateDeadTag>(Entity);
                        }
                        else if (SignalName == UnitSignals::PatrolIdle)
                        {
                        	if (StateFragment->CanAttack && StateFragment->IsInitialized) EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);
                        	
	                        EntityManager.Defer().AddTag<FMassStatePatrolIdleTag>(Entity);
                        	StateFragment->PlaceholderSignal = UnitSignals::PatrolIdle;
                        	UnitBase->UnitStatePlaceholder = UnitData::PatrolIdle;
                        }
                        else if (SignalName == UnitSignals::PatrolRandom)
                        {
                        	if (StateFragment->CanAttack && StateFragment->IsInitialized) EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);

                        	EntityManager.Defer().AddTag<FMassStatePatrolRandomTag>(Entity);
                        	StateFragment->PlaceholderSignal = UnitSignals::PatrolRandom;
                        	UnitBase->UnitStatePlaceholder = UnitData::PatrolRandom;
                        }
                        else if (SignalName == UnitSignals::Pause)
                        {
                        	//if (!UnitBase->bUseSkeletalMovement) return;
                        	
                        	if (StateFragment->CanAttack && StateFragment->IsInitialized) EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);

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
                        	if (StateFragment->CanAttack && StateFragment->IsInitialized) EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);

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
                        	UnitBase->StartBuild();
                        	EntityManager.Defer().AddTag<FMassStateBuildTag>(Entity);
                        }else if (SignalName == UnitSignals::GoToResourceExtraction)
                        {
                        	EntityManager.Defer().AddTag<FMassStateGoToResourceExtractionTag>(Entity);
                        	StateFragment->PlaceholderSignal = UnitSignals::GoToResourceExtraction;
                        	UnitBase->UnitStatePlaceholder = UnitData::GoToResourceExtraction;
                        }else if (SignalName == UnitSignals::ResourceExtraction)
                        {
                        	EntityManager.Defer().AddTag<FMassStateResourceExtractionTag>(Entity);
                        	TArray<FMassEntityHandle> CapturedEntitys;
                        	CapturedEntitys.Emplace(Entity);
                        	HandleGetClosestBaseArea(UnitSignals::GetClosestBase, CapturedEntitys);
                        }
                    	
                    	if (SignalName == UnitSignals::Idle) { UnitBase->SetUnitState(UnitData::Idle); }
                        else if (SignalName == UnitSignals::Chase) { UnitBase->SetUnitState(UnitData::Chase); }
                        else if (SignalName == UnitSignals::Attack) { UnitBase->SetUnitState(UnitData::Attack); }
                        else if (SignalName == UnitSignals::Dead)
                        {
	                        UnitBase->SetUnitState(UnitData::Dead);
                        }
                        else if (SignalName == UnitSignals::PatrolIdle)
                        {
	                        UnitBase->SetUnitState(UnitData::PatrolIdle);
                        }
                        else if (SignalName == UnitSignals::PatrolRandom){ UnitBase->SetUnitState(UnitData::PatrolRandom); }
                        else if (SignalName == UnitSignals::Pause)
                        {
                        	UnitBase->SetUnitState(UnitData::Pause);
                        }
                        else if (SignalName == UnitSignals::Run) { UnitBase->SetUnitState(UnitData::Run); }
                        else if (SignalName == UnitSignals::Casting) { UnitBase->SetUnitState(UnitData::Casting); }
                        else if (SignalName == UnitSignals::IsAttacked)
                        {
                        	UnitBase->SetUnitState(UnitData::IsAttacked);
                        }
                        else if (SignalName == UnitSignals::GoToBase)
                        {
	                        UnitBase->SetUnitState(UnitData::GoToBase);
                        	UpdateUnitMovement(Entity , UnitBase);
                        }
                    	else if (SignalName == UnitSignals::GoToBuild )
                    	{
                    		UnitBase->SetUnitState(UnitData::GoToBuild);
                    		UpdateUnitMovement(Entity , UnitBase);
                    	}
                    	else if (SignalName == UnitSignals::Build)
                    	{
                    		UnitBase->SetUnitState(UnitData::Build);
                    	}
                    	else if (SignalName == UnitSignals::GoToResourceExtraction)
                    	{
                    		UnitBase->SetUnitState(UnitData::GoToResourceExtraction);
                    		UpdateUnitMovement(Entity , UnitBase);
                    	}
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

        	
                    SetNewRandomPatrolTarget(PatrolFrag, MoveTarget, StateFragPtr, NavSys, World, StatsFrag.RunSpeed);
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
            SetNewRandomPatrolTarget(PatrolFrag, MoveTarget, StateFragPtr, NavSys, World, StatsFrag.RunSpeed);

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

FVector FindGroundLocationForActor(const UObject* WorldContextObject, AActor* TargetActor, TArray<AActor*> ActorsToIgnore, float TraceDistance = 10000.f)
{
	if (!TargetActor || !WorldContextObject)
	{
		return TargetActor ? TargetActor->GetActorLocation() : FVector::ZeroVector;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return TargetActor->GetActorLocation();
	}

	FVector ActorLocation = TargetActor->GetActorLocation();
	FVector StartTrace = ActorLocation + FVector(0.f, 0.f, TraceDistance);
	FVector EndTrace = ActorLocation - FVector(0.f, 0.f, TraceDistance);

	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(TargetActor);
	CollisionParams.AddIgnoredActors(ActorsToIgnore);
	// Use bTraceComplex for more accurate landscape hits, but it's slightly more expensive.
	// CollisionParams.bTraceComplex = true; 

	if (World->LineTraceSingleByChannel(HitResult, StartTrace, EndTrace, ECC_Visibility, CollisionParams))
	{
		// We hit something. Return a vector with the actor's X/Y and the hit's Z.
		return FVector(ActorLocation.X, ActorLocation.Y, HitResult.Location.Z);
	}

	// If the trace fails, return the original actor location as a fallback.
	return ActorLocation;
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

    if (!IsValid(UnitActor) || !UnitActor->Attributes)
    {
        //UE_LOG(LogTemp, Warning, TEXT("SynchronizeStatsFromActorToFragment: Konnte keinen gültigen AUnitBase Actor oder Attributes für Entity %d:%d finden."), Entity.Index, Entity.SerialNumber);
        return;
    }


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
			FMassAIStateFragment* AIStateFragment = GTEntityManager.GetFragmentDataPtr<FMassAIStateFragment>(CapturedEntity);
        	FMassAgentCharacteristicsFragment* CharFragment = GTEntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(CapturedEntity);

            UAttributeSetBase* AttributeSet = StrongUnitActor->Attributes;

        	if (StrongUnitActor && CharFragment)
        	{
        		CharFragment->bIsFlying = StrongUnitActor->IsFlying;
        		CharFragment->FlyHeight = StrongUnitActor->FlyHeight;
        		CharFragment->bCanOnlyAttackFlying = StrongUnitActor->CanOnlyAttackFlying;
        		CharFragment->bCanOnlyAttackGround = StrongUnitActor->CanOnlyAttackGround;
        		CharFragment->bCanDetectInvisible = StrongUnitActor->CanDetectInvisible;
        	}
        	
        	if (StrongUnitActor && AIStateFragment)
        	{
        		AIStateFragment->CanMove = StrongUnitActor->CanMove;
        		bool bHasDeadTag = DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateDeadTag::StaticStruct());
        			
        		if (AIStateFragment->CanMove && !bHasDeadTag)
        		{
        			GTEntityManager.Defer().RemoveTag<FMassStateStopMovementTag>(CapturedEntity);
        			UnregisterDynamicObstacle(StrongUnitActor);
        		}else if(!AIStateFragment->CanMove && !bHasDeadTag)
        		{
        			GTEntityManager.Defer().AddTag<FMassStateStopMovementTag>(CapturedEntity);
        			RegisterBuildingAsDynamicObstacle(StrongUnitActor);
        		}
        		AIStateFragment->CanAttack = StrongUnitActor->CanAttack;
        		AIStateFragment->IsInitialized = StrongUnitActor->IsInitialized;
        	}
        	
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
				CombatStatsFrag->bUseProjectile = StrongUnitActor->UseProjectile; // Assuming UsesProjectile() on Attributes
				CombatStatsFrag->CastTime = StrongUnitActor->CastTime;
				CombatStatsFrag->IsInitialized = StrongUnitActor->IsInitialized;

            	if (StrongUnitActor->MassActorBindingComponent)
            	{
            		CombatStatsFrag->SightRadius = StrongUnitActor->MassActorBindingComponent->SightRadius;
					CombatStatsFrag->LoseSightRadius = StrongUnitActor->MassActorBindingComponent->LoseSightRadius;
            	}

            	if (StrongUnitActor && StrongUnitActor->NextWaypoint) // Use config from Actor if available
            	{
            		if (PatrolFrag->TargetWaypointLocation != StrongUnitActor->NextWaypoint->GetActorLocation())
            		{
            			// <<< REPLACE Properties with your actual variable names >>>
						PatrolFrag->bLoopPatrol = StrongUnitActor->NextWaypoint->PatrolCloseToWaypoint; // Assuming direct property access
						PatrolFrag->RandomPatrolMinIdleTime = StrongUnitActor->NextWaypoint->PatrolCloseMinInterval;
						PatrolFrag->RandomPatrolMaxIdleTime = StrongUnitActor->NextWaypoint->PatrolCloseMaxInterval;
						PatrolFrag->TargetWaypointLocation = StrongUnitActor->NextWaypoint->GetActorLocation();
						PatrolFrag->RandomPatrolRadius = (StrongUnitActor->NextWaypoint->PatrolCloseOffset.X+StrongUnitActor->NextWaypoint->PatrolCloseOffset.Y)/2.f;
						PatrolFrag->IdleChance = StrongUnitActor->NextWaypoint->PatrolCloseIdlePercentage;


            		}
				}

            	if (StrongUnitActor && StrongUnitActor->IsWorker) // Use config from Actor if available
            	{
            		
            		bool UpdateMovement = false;
            		if (ResourceGameMode && !StrongUnitActor->Base)
            		{
            			StrongUnitActor->Base = ResourceGameMode->GetClosestBaseFromArray(StrongUnitActor, ResourceGameMode->WorkAreaGroups.BaseAreas);
            			UpdateMovement = true;
            		}
            		
            		if (StrongUnitActor->Base && StrongUnitActor->Base->GetUnitState() != UnitData::Dead)
            			WorkerStats->BaseAvailable = true;
            		else
            			WorkerStats->BaseAvailable = false;
            			
            		if (StrongUnitActor->Base)
            		{
            			WorkerStats->BasePosition = FindGroundLocationForActor(this, StrongUnitActor->Base, {StrongUnitActor, StrongUnitActor->Base}); //StrongUnitActor->Base->GetActorLocation();
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
            			WorkerStats->BuildAreaPosition = FindGroundLocationForActor(this, StrongUnitActor->BuildArea, {StrongUnitActor, StrongUnitActor->BuildArea}); // StrongUnitActor->BuildArea->GetActorLocation();
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
            			WorkerStats->ResourcePosition = FindGroundLocationForActor(this, StrongUnitActor->ResourcePlace, {StrongUnitActor, StrongUnitActor->ResourcePlace});
            		}
            		
            		WorkerStats->ResourceExtractionTime = StrongUnitActor->ResourceExtractionTime;
            		
            		if (WorkerStats->BaseAvailable)
            		{
            			WorkerStats->AutoMining	= StrongUnitActor->AutoMining;
            		}
		            else
		            {
		            	WorkerStats->AutoMining	= false;
		            }

            		//if (UpdateMovement)
            		{
            			//UpdateUnitMovement(CapturedEntity , StrongUnitActor);
            		}
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
        UE_LOG(LogTemp, Error, TEXT("SynchronizeUnitState: EntitySubsystem ist null!"));
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

    	if (!StrongUnitActor) return;

    	FMassEntityManager& GTEntityManager = EntitySubsystem->GetMutableEntityManager();
    	if( StrongUnitActor->GetUnitState() != UnitData::Idle && DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateIdleTag::StaticStruct())){
			StrongUnitActor->SetUnitState(UnitData::Idle);
		}
    	
    	if (!StrongUnitActor->IsWorker) return;
    		
        if (!EntitySubsystem)
        {
            //UE_LOG(LogTemp, Error, TEXT("SynchronizeUnitState (GameThread): EntitySubsystem wurde null!"));
            return;
        }
        
    	FMassAIStateFragment* State = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(CapturedEntity);
    	FMassWorkerStatsFragment* WorkerStats = GTEntityManager.GetFragmentDataPtr<FMassWorkerStatsFragment>(CapturedEntity);

				TArray<FMassEntityHandle> CapturedEntitys;
				CapturedEntitys.Emplace(CapturedEntity);

    			if (WorkerStats && WorkerStats->AutoMining
						   && StrongUnitActor->GetUnitState() == UnitData::Idle
						   && DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateIdleTag::StaticStruct())){
    				StrongUnitActor->SetUnitState(UnitData::GoToResourceExtraction);
				}
    	
    			if(StrongUnitActor->IsWorker && StrongUnitActor->GetUnitState() == UnitData::GoToBuild && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateGoToBuildTag::StaticStruct())){
					SwitchState(UnitSignals::GoToBuild, CapturedEntity, GTEntityManager);
    				//UpdateUnitMovement(CapturedEntity , StrongUnitActor);
    			}else if(StrongUnitActor->IsWorker && StrongUnitActor->GetUnitState() == UnitData::ResourceExtraction && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateResourceExtractionTag::StaticStruct())){
    				State->StateTimer = 0.f;
    				SwitchState(UnitSignals::ResourceExtraction, CapturedEntity, GTEntityManager);
    				//UpdateUnitMovement(CapturedEntity , StrongUnitActor);
    			}else if(StrongUnitActor->IsWorker && StrongUnitActor->GetUnitState() == UnitData::Build && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateBuildTag::StaticStruct())){
    				State->StateTimer = 0.f;
					SwitchState(UnitSignals::Build, CapturedEntity, GTEntityManager);
    				//UpdateUnitMovement(CapturedEntity , StrongUnitActor);
    			}else if(StrongUnitActor->IsWorker && StrongUnitActor->GetUnitState() == UnitData::GoToResourceExtraction && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateGoToResourceExtractionTag::StaticStruct())){
					SwitchState(UnitSignals::GoToResourceExtraction, CapturedEntity, GTEntityManager);
    				//UpdateUnitMovement(CapturedEntity , StrongUnitActor);
    			}else if(StrongUnitActor->IsWorker && StrongUnitActor->GetUnitState() == UnitData::GoToBase && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateGoToBaseTag::StaticStruct())){
					SwitchState(UnitSignals::GoToBase, CapturedEntity, GTEntityManager);
    				//UpdateUnitMovement(CapturedEntity , StrongUnitActor);
    			}


    			UpdateUnitMovement(CapturedEntity , StrongUnitActor); 
    }); // Ende AsyncTask Lambda
}

void UUnitStateProcessor::UnitActivateRangedAbilities(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    if (!EntitySubsystem)
    {
        // UE_LOG(LogTemp, Error, TEXT("UnitActivateRangedAbilities called but EntitySubsystem is null!"));
        return;
    }

    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

    for (const FMassEntityHandle& Entity : Entities)
    {
        if (!EntityManager.IsEntityValid(Entity))
        {
            continue;
        }

        FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
        if (ActorFragPtr)
        {
            AActor* Actor = ActorFragPtr->GetMutable(); // GetMutable if you might change actor state directly, Get if just reading
            if (IsValid(Actor))
            {
                AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
                if (UnitBase)
                {
                    // At this point, UnitBase is valid.
                    // If your abilities require specific conditions checkable here (e.g., has a target, enough mana),
                    // you could read other fragments (like FMassAITargetFragment, FMassCombatStatsFragment)
                    // and perform those checks before dispatching the AsyncTask.

                    // For simplicity, this example proceeds directly to attempting activation on the GameThread.

                    TWeakObjectPtr<AUnitBase> WeakAttacker(UnitBase);
                    FMassEntityHandle AttackerEntity = Entity; // Capture for potential signals or logging

                    // Dispatch to GameThread for ability activation as it involves Actor UFUNCTIONs
                    AsyncTask(ENamedThreads::GameThread, [this, AttackerEntity, WeakAttacker]() mutable
                    {
                        AUnitBase* StrongAttacker = WeakAttacker.Get();

                        if (!EntitySubsystem) // Re-check EntitySubsystem on GameThread
                        {
                            // UE_LOG(LogTemp, Error, TEXT("UnitActivateRangedAbilities (GameThread): EntitySubsystem became null!"));
                            return;
                        }
                        // No need for GTEntityManager if not accessing fragments here, but good practice if you might add it later
                        // FMassEntityManager& GTEntityManager = EntitySubsystem->GetMutableEntityManager();

                        if (StrongAttacker /*&& GTEntityManager.IsEntityValid(AttackerEntity)*/) // Ensure Attacker is valid
                        {
                            bool bIsActivated = false;
                            // Attempt to activate Throw Ability
                            // Add any prerequisite checks for ThrowAbility here if needed
                            if (StrongAttacker->ThrowAbilityID != EGASAbilityInputID::None) // Good practice: check if ID is set
                            {
                                bIsActivated = StrongAttacker->ActivateAbilityByInputID(StrongAttacker->ThrowAbilityID, StrongAttacker->ThrowAbilities);
                            }

                            // If Throw Ability was not activated, attempt Offensive Ability
                            if (!bIsActivated && StrongAttacker->OffensiveAbilityID != EGASAbilityInputID::None) // Good practice: check if ID is set
                            {
                                bIsActivated = StrongAttacker->ActivateAbilityByInputID(StrongAttacker->OffensiveAbilityID, StrongAttacker->OffensiveAbilities);
                            }

                            if (bIsActivated)
                            {
                                UE_LOG(LogTemp, Verbose, TEXT("Unit %s activated a ranged/offensive ability."), *StrongAttacker->GetName());
                            }
                            else
                            {
                                // UE_LOG(LogTemp, Verbose, TEXT("Unit %s did not activate Throw or Offensive ability."), *StrongAttacker->GetName());
                            }
                        }
                    }); // End AsyncTask Lambda
                } // End if (UnitBase)
            } // End if (IsValid(Actor))
        } // End if (ActorFragPtr)
    } // End loop through Entities
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

                    	// --- Perform Core Actor Actions ---
							 StrongAttacker->ServerStartAttackEvent_Implementation();
                    	
							 bool bIsActivated = StrongAttacker->ActivateAbilityByInputID(StrongAttacker->AttackAbilityID, StrongAttacker->AttackAbilities);
                    	
                    		StrongAttacker->ServerMeeleImpactEvent();
							StrongTarget->ActivateAbilityByInputID(StrongTarget->DefensiveAbilityID, StrongTarget->DefensiveAbilities);
							StrongTarget->FireEffects(StrongAttacker->MeleeImpactVFX, StrongAttacker->MeleeImpactSound, StrongAttacker->ScaleImpactVFX, StrongAttacker->ScaleImpactSound, StrongAttacker->MeeleImpactVFXDelay, StrongAttacker->MeleeImpactSoundDelay);
                    	
							if (TargetAIStateFragment->CanAttack && TargetAIStateFragment->IsInitialized) GTEntityManager.Defer().AddTag<FMassStateDetectTag>(TargetEntity);

                    		if(StrongTarget->GetUnitState() == UnitData::Casting)
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
                FMassEntityHandle TargetEntity = AttackerTargetFrag->TargetEntity;
             	
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

                       // ========================================================================
                        // /// START: ADDED DISTANCE CHECK ///
                        // ========================================================================
                       const FTransformFragment* AttackerTransformFrag = GTEntityManager.GetFragmentDataPtr<FTransformFragment>(AttackerEntity);
					   const FTransformFragment* TargetTransformFrag = GTEntityManager.GetFragmentDataPtr<FTransformFragment>(TargetEntity);

					   if (AttackerTransformFrag && TargetTransformFrag)
					   {
						   const FVector CurrentAttackerLocation = AttackerTransformFrag->GetTransform().GetLocation();
						   const FVector CurrentTargetLocation = TargetTransformFrag->GetTransform().GetLocation();
						   const float DistanceSquared = FVector::DistSquared(CurrentAttackerLocation, CurrentTargetLocation);
						   const float AttackRangeSquared = AttackerRange * AttackerRange; // AttackerRange was captured
		   				

							if (DistanceSquared > AttackRangeSquared)
							{
	      
								FMassAIStateFragment* AttackerStateFrag = GTEntityManager.GetFragmentDataPtr<FMassAIStateFragment>(AttackerEntity);
								if(AttackerStateFrag)
								{
									 AttackerStateFrag->SwitchingState = false;
								}
								return; // Abort the attack as target is out of range
							}
						// ========================================================================
						// /// END: ADDED DISTANCE CHECK ///
						// ========================================================================
					   }
                    	
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
                    		/*
                                 UnitData::EState CurrentTargetState = StrongTarget->GetUnitState();

                    	
                    		if (CurrentTargetState != UnitData::Run &&
								   CurrentTargetState != UnitData::Pause &&
								   CurrentTargetState != UnitData::Casting &&
								   CurrentTargetState != UnitData::Rooted &&
								   CurrentTargetState != UnitData::Attack&&
									CurrentTargetState != UnitData::IsAttacked &&
									!DoesEntityHaveTag(GTEntityManager, TargetEntity, FMassStateChargingTag::StaticStruct()) &&
										!DoesEntityHaveTag(GTEntityManager, TargetEntity, FMassStateIsAttackedTag::StaticStruct())
									)
                                 {
                                     if (SignalSubsystem)
                                     {
                                     		//TargetAIStateFragment->StateTimer = 0.f;
                                     	//SwitchState(UnitSignals::IsAttacked, TargetEntity, GTEntityManager);
                                         //SignalSubsystem->SignalEntity(UnitSignals::IsAttacked, TargetEntity);
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
                                 }*/
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
                    	UnregisterDynamicObstacle(UnitBase);
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

		//HandleGetClosestBaseArea(UnitSignals::GetClosestBase,  EntitiesCopy);
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
					if (UnitBase && UnitBase->ResourcePlace && IsValid(UnitBase->ResourcePlace) && UnitBase->ResourcePlace->WorkResourceClass)
					{
						SpawnWorkResource(UnitBase->ExtractingWorkResourceType, UnitBase->GetActorLocation(), UnitBase->ResourcePlace->WorkResourceClass, UnitBase);
						//UnitBase->UnitControlTimer = 0;
						//UnitBase->SetUEPathfinding = true;
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
					
					if (UnitBase && UnitBase->IsWorker)
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

						UpdateUnitMovement(Entity , UnitBase);
						//SwitchState( UnitSignals::GoToBase, Entity, EntityManager);
						UnitBase->SwitchEntityTagByState(UnitBase->UnitState, UnitBase->UnitStatePlaceholder);
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
						UnitBase->Base = ResourceGameMode->GetClosestBaseFromArray(UnitBase, ResourceGameMode->WorkAreaGroups.BaseAreas);
						StateFrag->SwitchingState = false;
					}
				}
			}
		}
	}); 
}


void UUnitStateProcessor::RegisterBuildingAsDynamicObstacle(AActor* BuildingActor)
{
    if (!IsValid(BuildingActor) || !World)
    {
        UE_LOG(LogTemp, Warning, TEXT("[NavObstacle] Invalid actor or world provided for registration."));
        return;
    }

    // Prevent registering the same actor twice
    if (RegisteredObstacles.Contains(BuildingActor))
    {
        // UE_LOG(LogTemp, Log, TEXT("[NavObstacle] Actor %s is already registered."), *BuildingActor->GetName());
        return;
    }

	// Try to find a capsule collision component first…
	UCapsuleComponent* Capsule = BuildingActor->FindComponentByClass<UCapsuleComponent>();
	FBox BoundsBox;

	if (Capsule)
	{
		// Pull radius & half‑height (accounting for scale)
		const float Radius     = Capsule->GetScaledCapsuleRadius();
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		const FVector Center   = Capsule->GetComponentLocation();

		// Build an FBox from center±extent
		const FVector Extents = FVector(Radius, Radius, HalfHeight);
		BoundsBox = FBox(Center - Extents, Center + Extents);

		UE_LOG(LogTemp, Log, TEXT("[NavObstacle] Using capsule bounds (R=%.1f H=%.1f) for %s."),
			   Radius, HalfHeight, *BuildingActor->GetName());
	}
	else
	{
		// Fallback to component bounding box
		FBox ComponentBB = BuildingActor->GetComponentsBoundingBox();
		if (!ComponentBB.IsValid)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NavObstacle] Could not calculate valid bounds for actor %s."),
				   *BuildingActor->GetName());
			return;
		}

		constexpr float Padding = 10.0f;
		BoundsBox = ComponentBB.ExpandBy(Padding);
	}
    
    // 2. Pad the bounds slightly to ensure full coverage
    constexpr float Padding = 10.0f; // Small padding
    const FBox PaddedBounds = BoundsBox.ExpandBy(FVector(Padding));
    const FVector Center = PaddedBounds.GetCenter();
    const FVector Extent = PaddedBounds.GetExtent();

    // 3. Spawn a dedicated, lightweight actor to hold the nav modifier
    AActor* NavObstacleActor = World->SpawnActor<AActor>();
    if (!NavObstacleActor)
    {
        UE_LOG(LogTemp, Error, TEXT("[NavObstacle] Failed to spawn proxy actor for nav obstacle."));
        return;
    }
    
    // For easier debugging, give it a clear name
    // NavObstacleActor->SetActorLabel(FString::Printf(TEXT("NavObstacle_for_%s"), *BuildingActor->GetName()));

    // 4. Create and configure the Box Component for the volume
	UBoxComponent* BoxComp = NewObject<UBoxComponent>(NavObstacleActor);
    BoxComp->SetWorldLocation(Center);
	BoxComp->SetBoxExtent(Extent, false);
    BoxComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);


    // --- ADD THESE LINES FOR DEBUGGING ---
    if(Debug)
    {
	    BoxComp->SetHiddenInGame(false);
    	BoxComp->SetVisibility(true);
    }
    // ------------------------------------
	
    NavObstacleActor->SetRootComponent(BoxComp);
    BoxComp->RegisterComponent();

    // 5. Create and configure the Nav Modifier Component
    UNavModifierComponent* ModComp = NewObject<UNavModifierComponent>(NavObstacleActor);
    ModComp->SetAreaClass(UNavArea_Obstacle::StaticClass());
	ModComp->FailsafeExtent = Extent;
    ModComp->RegisterComponent();

    // 6. Mark the area dirty to force a navmesh rebuild
    if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
    {
        NavSys->AddDirtyArea(PaddedBounds, ENavigationDirtyFlag::All);
        UE_LOG(LogTemp, Log, TEXT("[NavObstacle] Registered %s and marked dirty area."), *BuildingActor->GetName());
    }

    // 7. Track the new obstacle and bind to the building's destruction event for auto-cleanup
    RegisteredObstacles.Add(BuildingActor, NavObstacleActor);
    BuildingActor->OnDestroyed.AddDynamic(this, &UUnitStateProcessor::OnRegisteredActorDestroyed);
}

void UUnitStateProcessor::UnregisterDynamicObstacle(AActor* BuildingActor)
{
	if (!IsValid(BuildingActor))
	{
		return;
	}
    
	// Unbind the delegate first, as we need the BuildingActor pointer to do it.
	BuildingActor->OnDestroyed.RemoveDynamic(this, &UUnitStateProcessor::OnRegisteredActorDestroyed);

	TObjectPtr<AActor> NavObstacleActor = RegisteredObstacles.FindRef(BuildingActor);

	if (IsValid(NavObstacleActor))
	{
		// Get the bounds *before* destroying the actor
		const FBox BoundsToDirty = NavObstacleActor->GetComponentsBoundingBox();

		// Destroy our proxy actor
		NavObstacleActor->Destroy();
		UE_LOG(LogTemp, Log, TEXT("[NavObstacle] Unregistered and destroyed nav obstacle for %s."), *BuildingActor->GetName());

		// Mark the area dirty again so the navmesh can reclaim the space
		if (World)
		{
			if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
			{
				NavSys->AddDirtyArea(BoundsToDirty, ENavigationDirtyFlag::All);
			}
		}
	}
    
	// Now it's safe to stop tracking it.
	RegisteredObstacles.Remove(BuildingActor);
}

void UUnitStateProcessor::OnRegisteredActorDestroyed(AActor* DestroyedActor)
{
    UE_LOG(LogTemp, Log, TEXT("[NavObstacle] Detected %s was destroyed, cleaning up nav obstacle."), *DestroyedActor->GetName());
    //UnregisterDynamicObstacle(DestroyedActor);
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
									UnitBase->FinishedBuild();
								}
		
			
								if(NewUnit)
								{
									ABuildingBase* Building = Cast<ABuildingBase>(NewUnit);
									if (Building && UnitBase->BuildArea && UnitBase->BuildArea->NextWaypoint)
										Building->NextWaypoint = UnitBase->BuildArea->NextWaypoint;

									if (Building && UnitBase->BuildArea && !UnitBase->BuildArea->DestroyAfterBuild)
										UnitBase->BuildArea->Building = Building;

									RegisterBuildingAsDynamicObstacle(NewUnit);
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

void UUnitStateProcessor::SpawnWorkResource(
    EResourceType ResourceType,
    FVector Location,
    TSubclassOf<AWorkResource>   WRClass,
    AUnitBase*                   ActorToLockOn)
{
    if (!WRClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnWorkResource: no WRClass!"));
        return;
    }

    if (!ActorToLockOn)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnWorkResource: ActorToLockOn == nullptr!"));
        return;
    }

    if (!ActorToLockOn->ResourcePlace || !ActorToLockOn->ResourcePlace->Mesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnWorkResource: ActorToLockOn->ResourcePlace or Mesh is null!"));
        return;
    }
	
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnWorkResource: GetWorld() == nullptr!"));
        return;
    }

    // -- SETUP TRANSFORM --
    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat::Identity);

    // -- DEFERRED SPAWN --
    AWorkResource* NewRes = Cast<AWorkResource>(
        UGameplayStatics::BeginDeferredActorSpawnFromClass(
            World,
            WRClass,
            SpawnTransform,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn));

    if (!NewRes)
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnWorkResource: BeginDeferredActorSpawnFromClass returned null!"));
        return;
    }

    // Initialize properties *before* finishing spawn
    NewRes->IsAttached   = true;
    NewRes->ResourceType = ResourceType;

    UGameplayStatics::FinishSpawningActor(NewRes, SpawnTransform);

    // -- TIDY UP any existing resource on the unit --
    if (ActorToLockOn->WorkResource)
    {
        ActorToLockOn->WorkResource->Destroy();
        ActorToLockOn->WorkResource = nullptr;
    }

    // -- ASSIGN the freshly spawned resource to the unit --
    ActorToLockOn->WorkResource = NewRes;

    // -- UPDATE the place’s remaining amount --
    float& Remaining     = ActorToLockOn->ResourcePlace->AvailableResourceAmount;
    const float MaxRemain = ActorToLockOn->ResourcePlace->MaxAvailableResourceAmount;

    Remaining = FMath::Max(0.f, Remaining - NewRes->Amount);

    // If we’ve completely depleted it, just destroy it and bail
    if (Remaining <= KINDA_SMALL_NUMBER)
    {
        ActorToLockOn->ResourcePlace->Destroy();
        ActorToLockOn->ResourcePlace = nullptr;
        return;
    }

    // Otherwise, update the visual scale of the “ResourcePlace” mesh
    float Ratio    = MaxRemain > KINDA_SMALL_NUMBER ? Remaining / MaxRemain : 0.f;
    float NewScale = FMath::Lerp(0.4f, 1.0f, FMath::Clamp(Ratio, 0.f, 1.f));
    ActorToLockOn->ResourcePlace->Multicast_SetScale(FVector(NewScale));

    // -- FINALLY, ATTACH to the unit’s mesh socket --
    static const FName SocketName(TEXT("ResourceSocket"));
    if (ActorToLockOn->GetMesh()->DoesSocketExist(SocketName))
    {
        NewRes->AttachToComponent(
            ActorToLockOn->GetMesh(),
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            SocketName);

        // Offset if needed
        NewRes->SetActorRelativeLocation(NewRes->SocketOffset, false, nullptr, ETeleportType::TeleportPhysics);
    }
}

/*
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
*/

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

						if (!UnitBase->IsWorker) return;
						
						if (!UnitBase->BuildArea || !DoesEntityHaveTag(EntityManager, Entity, FMassStateBuildTag::StaticStruct())) return;

						if (UnitBase->BuildArea->CurrentBuildTime > UnitBase->UnitControlTimer)
						{
							StateFrag->StateTimer = UnitBase->BuildArea->CurrentBuildTime;
							UnitBase->UnitControlTimer = UnitBase->BuildArea->CurrentBuildTime;
						}
						else
						{
							UnitBase->BuildArea->CurrentBuildTime = UnitBase->UnitControlTimer;
						}
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

						//UE_LOG(LogTemp, Warning, TEXT("Switch to Placeholder Signal %s:"), *StateFrag->PlaceholderSignal.ToString());
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
		CustomPC->UpdateMinimap(CopiedEntities);
	});
}


void UUnitStateProcessor::HandleUpdateSelectionCircle(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	if (!EntitySubsystem || !World) return;

	APlayerController* PC = World->GetFirstPlayerController(); // Local controller
	if (!PC) return;

	ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(PC);
	if (!CustomPC) return;



	AsyncTask(ENamedThreads::GameThread, [CustomPC]()
	{
		CustomPC->UpdateSelectionCircles();
	});
}

void UUnitStateProcessor::HandleUnitSpawnedSignal(
	FName SignalName,
	TArray<FMassEntityHandle>& Entities)
{
	const float Now = World->GetTimeSeconds();
	
	if (!EntitySubsystem) { return; }

	if (!ResourceGameMode)
		ResourceGameMode = Cast<AResourceGameMode>(GetWorld()->GetAuthGameMode());
	
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();


	//TArray<FMassEntityHandle> Worker;
	for (FMassEntityHandle& E : Entities)
	{
		;
		// Grab the *target* fragment on that entity and stamp it
		FMassAIStateFragment* StateFragment = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(E);
		FTransformFragment* TransformFragPtr = EntityManager.GetFragmentDataPtr<FTransformFragment>(E);
		if (StateFragment)
		{
			StateFragment->BirthTime = Now;
			
			FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(E);
	
			if (ActorFragPtr)
			{
				AActor* TargetActor = ActorFragPtr->GetMutable();
				AUnitBase* Unit = Cast<AUnitBase>(TargetActor);

				if (!Unit || !IsValid(TargetActor)) return;


				// --- Hole benötigte Fragmente für DIESE Entity ---
				FMassAIStateFragment* StateFragPtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(E);
				FMassPatrolFragment* PatrolFragPtr = EntityManager.GetFragmentDataPtr<FMassPatrolFragment>(E);
				FMassMoveTargetFragment* MoveTargetPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(E);
				const FMassCombatStatsFragment* StatsFragPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(E);

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
				
				if (Unit->UnitState == UnitData::PatrolRandom)
				{
					if (!EntityManager.IsEntityValid(E))
					{
						continue;
					}

					UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
					if (!NavSys) { continue; }

					if (IsValid(Unit->NextWaypoint))
					{
						PatrolFrag.TargetWaypointLocation = Unit->NextWaypoint->GetActorLocation();
						SetNewRandomPatrolTarget(PatrolFrag, MoveTarget, StateFragPtr, NavSys, World, StatsFrag.RunSpeed);
					}
				}
				
				
				StateFrag.CanMove = Unit->CanMove;
				StateFrag.CanAttack = Unit->CanAttack;
				StateFrag.IsInitialized = Unit->IsInitialized;
				StateFrag.StoredLocation = Unit->GetActorLocation();
				
				Unit->SwitchEntityTagByState(Unit->UnitState, Unit->UnitStatePlaceholder);

				if (Unit->IsWorker && ResourceGameMode)
				{
					FMassWorkerStatsFragment* WorkerStatsFrag = EntityManager.GetFragmentDataPtr<FMassWorkerStatsFragment>(E);
					ResourceGameMode->AssignWorkAreasToWorker(Unit);
					if (Unit->ResourcePlace)
					{
						FVector ResourcePosition = FindGroundLocationForActor(this, Unit->ResourcePlace, {Unit, Unit->ResourcePlace});
						WorkerStatsFrag->ResourcePosition = ResourcePosition;
						UpdateMoveTarget(MoveTarget, ResourcePosition, StatsFrag.RunSpeed, World);
						SwitchState(UnitSignals::GoToResourceExtraction, E, EntityManager);
					}else if (Unit->Base)
					{
						FVector BasePosition = FindGroundLocationForActor(this, Unit->Base, {Unit, Unit->Base});
						WorkerStatsFrag->ResourcePosition = BasePosition;
						UpdateMoveTarget(MoveTarget, BasePosition, StatsFrag.RunSpeed, World);
						SwitchState(UnitSignals::GoToBase, E, EntityManager);
					}else
					{
						SwitchState(UnitSignals::Idle, E, EntityManager);
					}
					//Worker.Emplace(E);
				}
				
				if (TransformFragPtr)
				{
					if (Unit) 
					{
						const FVector ActorInitialLocation = Unit->GetActorLocation();
						FTransform MyTransform = TransformFragPtr->GetMutableTransform();
						MyTransform.SetLocation(ActorInitialLocation);
					}
				}
			}
		}
	}
	
	//HandleGetClosestBaseArea(UnitSignals::GetClosestBase,  Worker);
}


void UUnitStateProcessor::UpdateWorkerMovement(FName SignalName, TArray<FMassEntityHandle>& Entities)
{


	// **Keep initial checks outside AsyncTask if possible and thread-safe**
	if (!EntitySubsystem)
	{
		// Log error - This check itself is generally safe
		return;
	}
    
	TArray<FMassEntityHandle> EntitiesCopy = Entities; 
    
	//AsyncTask(ENamedThreads::GameThread, [this, SignalName, EntitiesCopy]() mutable // mutable if you modify captures (not needed here)
	//{
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
			
			
			SynchronizeStatsFromActorToFragment(Entity);
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
					if (UnitBase && UnitBase->IsWorker)
					{
						FMassWorkerStatsFragment* WorkerStatsFrag = EntityManager.GetFragmentDataPtr<FMassWorkerStatsFragment>(Entity);
						FMassMoveTargetFragment* MoveTargetPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
						const FMassCombatStatsFragment* StatsFragPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity);

						if (!WorkerStatsFrag || !MoveTargetPtr || !StatsFragPtr) return;
						
						FMassMoveTargetFragment& MoveTarget = *MoveTargetPtr;
						const FMassCombatStatsFragment& StatsFrag = *StatsFragPtr;
				
						if (UnitBase->UnitState == UnitData::GoToResourceExtraction)
						{
							UpdateMoveTarget(MoveTarget, WorkerStatsFrag->ResourcePosition, StatsFrag.RunSpeed, World);
						}else if (UnitBase->UnitState == UnitData::GoToBase)
						{
							TArray<FMassEntityHandle> CapturedEntitys;
							CapturedEntitys.Emplace(Entity);
							HandleGetClosestBaseArea(UnitSignals::GetClosestBase, CapturedEntitys);
		
							if (UnitBase->WorkResource)
							UpdateMoveTarget(MoveTarget,  WorkerStatsFrag->BasePosition, StatsFrag.RunSpeed, World);
						}else if (UnitBase->UnitState == UnitData::GoToBuild)
						{
							UpdateMoveTarget(MoveTarget, WorkerStatsFrag->BuildAreaPosition,  StatsFrag.RunSpeed, World);
						}
					}
				}
			}
		}
	//});
}

void UUnitStateProcessor::UpdateUnitMovement(FMassEntityHandle& Entity, AUnitBase* UnitBase)
{
	
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	if (!EntityManager.IsEntityValid(Entity)) 
	{
		return;
	}
	
	if (UnitBase && UnitBase->IsWorker)
	{
		FMassWorkerStatsFragment* WorkerStatsFrag = EntityManager.GetFragmentDataPtr<FMassWorkerStatsFragment>(Entity);
		FMassMoveTargetFragment* MoveTargetPtr = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
		FMassAIStateFragment* StateFraggPtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
		const FMassCombatStatsFragment* StatsFragPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity);

		if (!WorkerStatsFrag || !MoveTargetPtr || !StatsFragPtr) return;

		SynchronizeStatsFromActorToFragment(Entity);
		
		FMassMoveTargetFragment& MoveTarget = *MoveTargetPtr;
		const FMassCombatStatsFragment& StatsFrag = *StatsFragPtr;
				
		if (UnitBase->UnitState == UnitData::GoToResourceExtraction)
		{
			StateFraggPtr->StoredLocation = WorkerStatsFrag->ResourcePosition;
			UpdateMoveTarget(MoveTarget, WorkerStatsFrag->ResourcePosition, StatsFrag.RunSpeed, World);
		}else if (UnitBase->UnitState == UnitData::GoToBase)
		{
			StateFraggPtr->StoredLocation = WorkerStatsFrag->BasePosition;
			UpdateMoveTarget(MoveTarget,  WorkerStatsFrag->BasePosition, StatsFrag.RunSpeed, World);
		}else if (UnitBase->UnitState == UnitData::GoToBuild)
		{
			StateFraggPtr->StoredLocation = WorkerStatsFrag->BuildAreaPosition;
			UpdateMoveTarget(MoveTarget, WorkerStatsFrag->BuildAreaPosition,  StatsFrag.RunSpeed, World);
		}
	}
}