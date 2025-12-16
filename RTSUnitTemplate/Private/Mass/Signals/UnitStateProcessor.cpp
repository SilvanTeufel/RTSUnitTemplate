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
#include "Characters/Unit/MassUnitBase.h"
#include "Widgets/UnitBaseHealthBar.h"
#include "MassMovementFragments.h"
#include "MassCommonFragments.h"
#include "Mass/UnitMassTag.h"
#include "Math/Vector2D.h"
#include "MassExecutionContext.h" 
#include "MassNavigationFragments.h"
#include "Characters/Unit/BuildingBase.h"
#include "Characters/Unit/ConstructionUnit.h"
#include "Components/StaticMeshComponent.h"
#include "Actors/WorkArea.h"
#include "Characters/Unit/WorkingUnitBase.h"
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
#include "EngineUtils.h"
#include "MassReplicationFragments.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"
#include "Mass/Replication/UnitRegistryReplicator.h"
#include "Characters/Unit/PerformanceUnit.h"
#include "Characters/Unit/BuildingBase.h"
#include "Components/CapsuleComponent.h"

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

   		// Sight change signals are now handled by UnitSightProcessor on both client and server.
   		// Intentionally not binding here to avoid server-only handling.
     if (GetWorld() && GetWorld()->IsNetMode(NM_Client))
     {
         UE_LOG(LogTemp, Warning, TEXT("[UnitStateProcessor] Sight signals are handled by UnitSightProcessor now (NetMode=%d)."), World ? (int32)World->GetNetMode() : -1);
     }
    	SelectionCircleDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateSelectionCircle)
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
    
    // Repair time sync
    if (SignalSubsystem)
    {
        FDelegateHandle NewRepairSyncHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SyncRepairTime)
            .AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, SyncRepairTime));
        SightChangeRequestDelegateHandle.Add(NewRepairSyncHandle);
    }
    	


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

		// Follow feature delegates removed: following now uses FriendlyTargetEntity on FMassAITargetFragment
		}
}


void UUnitStateProcessor::UnbindDelegates_Internal()
{
	if (bDelegatesUnbound)
	{
		return;
	}
	bDelegatesUnbound = true;

	if (SignalSubsystem)
	{
		// Unregister all signal handlers safely (must run on GameThread)
		for (int i = 0; i < StateChangeSignalDelegateHandle.Num(); i++)
		{
			if (StateChangeSignalDelegateHandle[i].IsValid())
			{
				auto& Delegate = SignalSubsystem->GetSignalDelegateByName(StateChangeSignals[i]);
				Delegate.Remove(StateChangeSignalDelegateHandle[i]);
				StateChangeSignalDelegateHandle[i].Reset();
			}
		}

		for (int i = 0; i < SightChangeRequestDelegateHandle.Num(); i++)
		{
			if (SightChangeRequestDelegateHandle[i].IsValid())
			{
				auto& Delegate = SignalSubsystem->GetSignalDelegateByName(SightChangeSignals[i]);
				Delegate.Remove(SightChangeRequestDelegateHandle[i]);
				SightChangeRequestDelegateHandle[i].Reset();
			}
		}
		
		if (FogParametersDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateFogMask);
			Delegate.Remove(FogParametersDelegateHandle);
			FogParametersDelegateHandle.Reset();
		}

		if (SelectionCircleDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateSelectionCircle);
			Delegate.Remove(SelectionCircleDelegateHandle);
			SelectionCircleDelegateHandle.Reset();
		}

		if (SyncUnitBaseDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SyncUnitBase);
			Delegate.Remove(SyncUnitBaseDelegateHandle);
			SyncUnitBaseDelegateHandle.Reset();
		}
		
		if (SetUnitToChaseSignalDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SetUnitToChase);
			Delegate.Remove(SetUnitToChaseSignalDelegateHandle);
			SetUnitToChaseSignalDelegateHandle.Reset();
		}

		if (RangedAbilitySignalDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UseRangedAbilitys);
			Delegate.Remove(RangedAbilitySignalDelegateHandle);
			RangedAbilitySignalDelegateHandle.Reset();
		}
		
		if (MeleeAttackSignalDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::MeleeAttack);
			Delegate.Remove(MeleeAttackSignalDelegateHandle);
			MeleeAttackSignalDelegateHandle.Reset();
		}
		
		if (RangedAttackSignalDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::RangedAttack);
			Delegate.Remove(RangedAttackSignalDelegateHandle);
			RangedAttackSignalDelegateHandle.Reset();
		}

		if (StartDeadSignalDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::StartDead);
			Delegate.Remove(StartDeadSignalDelegateHandle);
			StartDeadSignalDelegateHandle.Reset();
		}

		if (EndDeadSignalDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::EndDead);
			Delegate.Remove(EndDeadSignalDelegateHandle);
			EndDeadSignalDelegateHandle.Reset();
		}
		
		if (IdlePatrolSwitcherDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::PISwitcher);
			Delegate.Remove(IdlePatrolSwitcherDelegateHandle);
			IdlePatrolSwitcherDelegateHandle.Reset();
		}

		if (UnitStatePlaceholderDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SetUnitStatePlaceholder);
			Delegate.Remove(UnitStatePlaceholderDelegateHandle);
			UnitStatePlaceholderDelegateHandle.Reset();
		}
		
		if (SyncCastTimeDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SyncCastTime);
			Delegate.Remove(SyncCastTimeDelegateHandle);
			SyncCastTimeDelegateHandle.Reset();
		}

		if (EndCastDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::EndCast);
			Delegate.Remove(EndCastDelegateHandle);
			EndCastDelegateHandle.Reset();
		}

		if (ReachedBaseDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::ReachedBase);
			Delegate.Remove(ReachedBaseDelegateHandle);
			ReachedBaseDelegateHandle.Reset();
		}

		if (GetResourceDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::GetResource);
			Delegate.Remove(GetResourceDelegateHandle);
			GetResourceDelegateHandle.Reset();
		}
		
		if (StartBuildActionDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::GetClosestBase);
			Delegate.Remove(StartBuildActionDelegateHandle);
			StartBuildActionDelegateHandle.Reset();
		}
		
		if (SpawnBuildingRequestDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SpawnBuildingRequest);
			Delegate.Remove(SpawnBuildingRequestDelegateHandle);
			SpawnBuildingRequestDelegateHandle.Reset();
		}

		if (SpawnSignalDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UnitSpawned);
			Delegate.Remove(SpawnSignalDelegateHandle);
			SpawnSignalDelegateHandle.Reset();
		}

		if (UpdateWorkerMovementDelegateHandle.IsValid())
		{
			auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::UpdateWorkerMovement);
			Delegate.Remove(UpdateWorkerMovementDelegateHandle);
			UpdateWorkerMovementDelegateHandle.Reset();
		}
	}

	SignalSubsystem = nullptr;
	EntitySubsystem = nullptr;
}

void UUnitStateProcessor::UnbindDelegates_GameThread()
{
	// Always defer to the next GameThread tick to avoid removing delegates while they may be broadcasting
	TWeakObjectPtr<UUnitStateProcessor> WeakThis(this);
	AsyncTask(ENamedThreads::GameThread, [WeakThis]()
	{
		if (UUnitStateProcessor* StrongThis = WeakThis.Get())
		{
			if (StrongThis->World)
			{
				FTimerDelegate TimerDel;
				TWeakObjectPtr<UUnitStateProcessor> InnerWeak = StrongThis;
				TimerDel.BindLambda([InnerWeak]()
				{
					if (UUnitStateProcessor* InnerStrong = InnerWeak.Get())
					{
						InnerStrong->UnbindDelegates_Internal();
					}
				});
				StrongThis->World->GetTimerManager().SetTimerForNextTick(TimerDel);
			}
			else
			{
				StrongThis->UnbindDelegates_Internal();
			}
		}
	});
}

void UUnitStateProcessor::BeginDestroy()
{
	// Mark shutting down so incoming signals early-out
	bIsShuttingDown = true;
	// Ensure delegate unbinding happens on the game thread and deferred to next tick to avoid races
	UnbindDelegates_GameThread();
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

void UUnitStateProcessor::HandleUpdateFollowMovement(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	// Deprecated: Follow movement is now handled directly by state processors via FriendlyTargetEntity.
	return;
}

void UUnitStateProcessor::HandleCheckFollowAssigned(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	// Deprecated: Follow assignment check is now handled directly in state processors using FriendlyTargetEntity.
	return;
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
                        
                     // Repair-specific
                     if (SignalName != UnitSignals::GoToRepair) EntityManager.Defer().RemoveTag<FMassStateGoToRepairTag>(Entity);
                     if (SignalName != UnitSignals::Repair) EntityManager.Defer().RemoveTag<FMassStateRepairTag>(Entity);
                        	
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

                            // If a ConstructionUnit dies: destroy its WorkArea and send its Worker back to placeholder
                            if (AConstructionUnit* CU = Cast<AConstructionUnit>(UnitBase))
                            {
                            	if (World && World->GetNetMode() != NM_Client)
                            	{
                         					if (CU->WorkArea)
                         					{
                         						// Clear back-reference and destroy the area
                         						if (CU->WorkArea->ConstructionUnit == CU)
                         						{
                         							CU->WorkArea->ConstructionUnit = nullptr;
                         						}
                         						CU->WorkArea->Destroy(false, true);
                         						CU->WorkArea = nullptr;
                         					}
                         					if (CU->Worker)
                         					{
                         						// Optionally clear the worker's BuildArea to avoid dangling refs
                         						CU->Worker->BuildArea = nullptr;
                         						FMassEntityManager* WorkerMgr = nullptr;
                         						FMassEntityHandle WorkerEntity;
                         						if (CU->Worker->GetMassEntityData(WorkerMgr, WorkerEntity))
                         						{
                         							if (SignalSubsystem && WorkerMgr && WorkerMgr->IsEntityValid(WorkerEntity))
                         							{
                         								// Split call to avoid macro parsing issues
                         								UMassSignalSubsystem* LocalSignalSubsystem = SignalSubsystem;
                         								LocalSignalSubsystem->SignalEntity(UnitSignals::SetUnitStatePlaceholder, WorkerEntity);
                         							}
                         						}
                         					}
                            	}
                            }

                            // Unregister from Mass replication when the unit dies (server only)
                            if (World && World->GetNetMode() != NM_Client)
                            {
                                // 1) Remove from client bubble replication list by NetID
                                const FMassNetworkIDFragment* NetIDFrag = EntityManager.GetFragmentDataPtr<FMassNetworkIDFragment>(Entity);
                                if (NetIDFrag)
                                {
                                    const FMassNetworkID NetID = NetIDFrag->NetID;
                                    for (TActorIterator<AUnitClientBubbleInfo> It(World); It; ++It)
                                    {
                                        AUnitClientBubbleInfo* Bubble = *It;
                                        if (Bubble && Bubble->Agents.RemoveItemByNetID(NetID))
                                        {
                                            Bubble->Agents.MarkArrayDirty();
                                            Bubble->ForceNetUpdate();
                                        }
                                    }
                                }

                                // 2) Remove from the authoritative UnitRegistry so new clients do not link this unit
                                if (AUnitRegistryReplicator* Reg = AUnitRegistryReplicator::GetOrSpawn(*World))
                                {
                                    bool bRemoved = false;
                                    if (UnitBase->UnitIndex != INDEX_NONE)
                                    {
                                        bRemoved |= Reg->Registry.RemoveByUnitIndex(UnitBase->UnitIndex);
                                    }
                                    bRemoved |= Reg->Registry.RemoveByOwner(UnitBase->GetFName());
                                    if (bRemoved)
                                    {
                                        Reg->Registry.MarkArrayDirty();
                                        Reg->ForceNetUpdate();
                                    }
                                }
                            }
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
                        else if (SignalName == UnitSignals::GoToRepair)
                        {
                            EntityManager.Defer().AddTag<FMassStateGoToRepairTag>(Entity);
                            StateFragment->PlaceholderSignal = UnitSignals::GoToRepair;
                            UnitBase->UnitStatePlaceholder = UnitData::GoToRepair;
                        }
                        else if (SignalName == UnitSignals::Repair)
                        {
                            EntityManager.Defer().AddTag<FMassStateRepairTag>(Entity);
                            StateFragment->PlaceholderSignal = UnitSignals::Repair;
                            UnitBase->UnitStatePlaceholder = UnitData::Repair;
                        }
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
                        	// Add worker to BuildArea worker list when starting to go build
                        	bool SetTag = false;
                        	if (UnitBase && UnitBase->IsWorker)
                        	{
                        		if (AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(UnitBase))
                        		{
                        			if (Worker->BuildArea && Worker->BuildArea->Workers.Num() > Worker->BuildArea->MaxWorkerCount)
                        			{
                        				EntityManager.Defer().AddTag<FMassStateGoToBaseTag>(Entity);
                        				Worker->BuildArea->RemoveWorkerFromArray(Worker);
                        				SetTag = true;
                        			}
                        		}
                        	}

                        	if (!SetTag)EntityManager.Defer().AddTag<FMassStateGoToBuildTag>(Entity);
                        	
                        }else if (SignalName == UnitSignals::Build)
                        {
                        	UnitBase->StartBuild();
                        	EntityManager.Defer().AddTag<FMassStateBuildTag>(Entity);
                        	// Ensure worker is registered with the BuildArea while building
                        	
                        	if (UnitBase && UnitBase->IsWorker)
                        	{
                        		if (AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(UnitBase))
                        		{
                        			if (Worker->BuildArea)
                        			{
                        				Worker->BuildArea->AddWorkerToArray(Worker);
                        			}
                        		}
                        	}
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
                    		bool SetTag = false;
                    		if (UnitBase && UnitBase->IsWorker)
                    		{
                    			if (AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(UnitBase))
                    			{
                    				if (Worker->BuildArea && Worker->BuildArea->Workers.Num() > Worker->BuildArea->MaxWorkerCount)
                    				{
                    					UnitBase->SetUnitState(UnitData::GoToBase);
                    					UpdateUnitMovement(Entity , UnitBase);
                    					SetTag = true;
                    				}
                    			}
                    		}

                    		if (!SetTag)
                    		{
                    			UnitBase->SetUnitState(UnitData::GoToBuild);
                    			UpdateUnitMovement(Entity , UnitBase);
                    		}
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
                  		else if (SignalName == UnitSignals::GoToRepair)
                  		{
                  			UnitBase->SetUnitState(UnitData::GoToRepair);
                  			UpdateUnitMovement(Entity , UnitBase);
                  		}
                  		else if (SignalName == UnitSignals::Repair)
                  		{
                  			UnitBase->SetUnitState(UnitData::Repair);
                  		}

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
        	
        	if (StrongUnitActor && AIStateFragment && CharFragment)
        	{
        		AIStateFragment->CanMove = StrongUnitActor->CanMove;
        		bool bHasDeadTag = DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateDeadTag::StaticStruct());
        		
        		if (DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateDisableNavManipulationTag::StaticStruct()))
        		{
        			UnregisterObstacle(StrongUnitActor);
        		}else if (AIStateFragment->CanMove && !bHasDeadTag)
        		{
        			GTEntityManager.Defer().RemoveTag<FMassStateStopMovementTag>(CapturedEntity);
        			UnregisterObstacle(StrongUnitActor);
        		}else if(!AIStateFragment->CanMove && !bHasDeadTag && CharFragment->CanManipulateNavMesh)
        		{
        			GTEntityManager.Defer().AddTag<FMassStateStopMovementTag>(CapturedEntity);
        			RegisterBuildingAsObstacle(StrongUnitActor);
        		}
        		AIStateFragment->CanAttack = StrongUnitActor->CanAttack;
        		
        		AIStateFragment->IsInitialized = StrongUnitActor->IsInitialized;
        		AIStateFragment->HoldPosition = StrongUnitActor->bHoldPosition;
        	}
        	
            if (CombatStatsFrag && AttributeSet)
            {
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
            		
            		if (ResourceGameMode && !StrongUnitActor->Base)
            		{
            			StrongUnitActor->Base = ResourceGameMode->GetClosestBaseFromArray(StrongUnitActor, ResourceGameMode->WorkAreaGroups.BaseAreas);
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
						WorkerStats->BaseArrivalDistance = BoxExtent.Size()/2+170.f;
            		}

            		WorkerStats->BuildingAreaAvailable = StrongUnitActor->BuildArea? true : false;
            		if (StrongUnitActor->BuildArea)
            		{
            			FVector Origin, BoxExtent;
						StrongUnitActor->BuildArea->GetActorBounds( false, Origin, BoxExtent);

            			WorkerStats->BuildAreaArrivalDistance = BoxExtent.Size()/2+150.f;
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
            		
            		if (WorkerStats->BaseAvailable && WorkerStats->ResourceAvailable)
            		{
            			WorkerStats->AutoMining	= StrongUnitActor->AutoMining;
            		}
		            else
		            {
		            	WorkerStats->AutoMining	= false;
		            }
            	}
            }
        }
    }); 
}

void UUnitStateProcessor::SynchronizeUnitState(FMassEntityHandle Entity)
{
    if (!EntitySubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("SynchronizeUnitState: EntitySubsystem ist null!"));
        return;
    }

    const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager(); // Für Read-Only Checks

    if (!EntityManager.IsEntityValid(Entity))
    {
        return;
    }

    const FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
    const AActor* Actor = ActorFrag ? ActorFrag->Get() : nullptr;
    const AUnitBase* UnitActor = Cast<AUnitBase>(Actor);


    if (!IsValid(UnitActor))
    {
        return;
    }
	
    TWeakObjectPtr<AUnitBase> WeakUnitActor(const_cast<AUnitBase*>(UnitActor));
    FMassEntityHandle CapturedEntity = Entity;
	
    AsyncTask(ENamedThreads::GameThread, [this, WeakUnitActor, CapturedEntity, &EntityManager]() mutable
    {
        AUnitBase* StrongUnitActor = WeakUnitActor.Get();

    	if (!StrongUnitActor) return;

    	FMassEntityManager& GTEntityManager = EntitySubsystem->GetMutableEntityManager();
    	if( StrongUnitActor->GetUnitState() != UnitData::Idle && DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateIdleTag::StaticStruct())){
			StrongUnitActor->SetUnitState(UnitData::Idle);
		}

    	UpdateUnitArrayMovement(CapturedEntity , StrongUnitActor);
    	
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
    	
    			if (WorkerStats && WorkerStats->AutoMining && !StrongUnitActor->Base->IsFlying
						   && StrongUnitActor->GetUnitState() == UnitData::Idle
						   && !StrongUnitActor->FollowUnit
						   && DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateIdleTag::StaticStruct())){
    				StrongUnitActor->SetUnitState(UnitData::GoToResourceExtraction);
				}
    	
    			if(StrongUnitActor->GetUnitState() == UnitData::GoToBuild && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateGoToBuildTag::StaticStruct())){
					SwitchState(UnitSignals::GoToBuild, CapturedEntity, GTEntityManager);
    			}else if(StrongUnitActor->GetUnitState() == UnitData::ResourceExtraction && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateResourceExtractionTag::StaticStruct())){
    				State->StateTimer = 0.f;
    				SwitchState(UnitSignals::ResourceExtraction, CapturedEntity, GTEntityManager);
    			}else if(StrongUnitActor->GetUnitState() == UnitData::Build && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateBuildTag::StaticStruct())){
    				State->StateTimer = 0.f;
					SwitchState(UnitSignals::Build, CapturedEntity, GTEntityManager);
    			}else if(StrongUnitActor->GetUnitState() == UnitData::GoToResourceExtraction && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateGoToResourceExtractionTag::StaticStruct())){
					SwitchState(UnitSignals::GoToResourceExtraction, CapturedEntity, GTEntityManager);
    			}else if(StrongUnitActor->GetUnitState() == UnitData::GoToBase && !DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateGoToBaseTag::StaticStruct())){
					SwitchState(UnitSignals::GoToBase, CapturedEntity, GTEntityManager);
    			}


    			if(StrongUnitActor->GetUnitState() != UnitData::GoToBuild && DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateGoToBuildTag::StaticStruct())){
    				StrongUnitActor->SetUnitState(UnitData::GoToBuild);
				}else if(StrongUnitActor->GetUnitState() != UnitData::ResourceExtraction && DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateResourceExtractionTag::StaticStruct())){
					StrongUnitActor->SetUnitState(UnitData::ResourceExtraction);
				}else if(StrongUnitActor->GetUnitState() != UnitData::Build && DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateBuildTag::StaticStruct())){
					StrongUnitActor->SetUnitState(UnitData::Build);
				}else if(StrongUnitActor->GetUnitState() != UnitData::GoToResourceExtraction && DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateGoToResourceExtractionTag::StaticStruct())){
					StrongUnitActor->SetUnitState(UnitData::GoToResourceExtraction);
				}else if(StrongUnitActor->GetUnitState() != UnitData::GoToBase && DoesEntityHaveTag(GTEntityManager,CapturedEntity, FMassStateGoToBaseTag::StaticStruct())){
					StrongUnitActor->SetUnitState(UnitData::GoToBase);
				}
    	
    	
   				// Debug logging for UnitState, Tags, and BuildArea before movement update

   		
   				UpdateUnitMovement(CapturedEntity , StrongUnitActor); 

   				// Log current UnitState at the end of SynchronizeUnitState to trace state changes
   				{
   					const UEnum* UnitStateEnum = StaticEnum<UnitData::EState>();
   					const uint8 RawState = (uint8)StrongUnitActor->GetUnitState();
   					const FString StateName = UnitStateEnum ? UnitStateEnum->GetNameStringByValue((int64)RawState) : FString::FromInt(RawState);
   					UE_LOG(LogTemp, Warning, TEXT("[SynchronizeUnitState][END] Unit=%s State=%s (%d)"), *StrongUnitActor->GetName(), *StateName, RawState);
   				}
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
                            if (StrongAttacker->ThrowAbilityID != EGASAbilityInputID::None && StrongAttacker->ThrowAbilities.Num() > 0) // Good practice: check if ID is set
                            {
                                bIsActivated = StrongAttacker->ActivateAbilityByInputID(StrongAttacker->ThrowAbilityID, StrongAttacker->ThrowAbilities);
                            }

                            // If Throw Ability was not activated, attempt Offensive Ability
                            if (!bIsActivated && StrongAttacker->OffensiveAbilityID != EGASAbilityInputID::None && StrongAttacker->OffensiveAbilities.Num() > 0) // Good practice: check if ID is set
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
    if (!EntitySubsystem) return;

    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

    for (const FMassEntityHandle& Entity : Entities)
    {
        if (!EntityManager.IsEntityValid(Entity)) continue;

        FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
        // Retrieve fragments needed for logic immediately
        const FMassCombatStatsFragment* AttackerStats = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity);
        FMassCombatStatsFragment* TargetStatsFrag = nullptr; // Will fetch below
        FMassAITargetFragment* TargetFrag = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Entity);

        if (ActorFragPtr && AttackerStats && TargetFrag && TargetFrag->bHasValidTarget && TargetFrag->TargetEntity.IsSet())
        {
             AUnitBase* UnitBase = Cast<AUnitBase>(ActorFragPtr->GetMutable());
             FMassEntityHandle TargetEntity = TargetFrag->TargetEntity;
             
             if (UnitBase && IsValid(UnitBase->UnitToChase) && !UnitBase->UseProjectile && EntityManager.IsEntityValid(TargetEntity))
             {
                // 1. HANDLE MATH & LOGIC HERE (While pointers are valid)
                TargetStatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TargetEntity);
                if (!TargetStatsFrag) continue;

                bool bIsMagicDamage = UnitBase->IsDoingMagicDamage;
                float BaseDamage = AttackerStats->AttackDamage;
                float Defense = bIsMagicDamage ? TargetStatsFrag->MagicResistance : TargetStatsFrag->Armor;
                float DamageAfterDefense = FMath::Max(0.0f, BaseDamage - Defense);
                
                float DamageToApply = DamageAfterDefense;
                float ShieldDamage = 0.0f;
                float HealthDamage = 0.0f;

                // Calculate Shield/Health splits
                if (TargetStatsFrag->Shield > 0)
                {
                    ShieldDamage = FMath::Min(TargetStatsFrag->Shield, DamageToApply);
                    TargetStatsFrag->Shield -= ShieldDamage;
                    DamageToApply -= ShieldDamage;
                }
                if (DamageToApply > 0)
                {
                    HealthDamage = FMath::Min(TargetStatsFrag->Health, DamageToApply);
                    TargetStatsFrag->Health -= HealthDamage;
                }
                
                // Clamp Health
                TargetStatsFrag->Health = FMath::Max(0.0f, TargetStatsFrag->Health);

                // 2. PREPARE ASYNC DATA
                // We capture VALUES (HealthDamage, ShieldDamage), not pointers to fragments.
                TWeakObjectPtr<AUnitBase> WeakAttacker(UnitBase);
                TWeakObjectPtr<AUnitBase> WeakTarget(UnitBase->UnitToChase);
                
                // Capture Tag requirement logic as a bool or ID, don't pass the StateFragment pointer
                bool bShouldApplyRootReduction = false; // Logic for this needs to stay on game thread or be pre-calculated
                // Note: Updating StateTimer is complex across threads. It's safer to defer a Command or use a Signal.

                // 3. DISPATCH GAME THREAD TASK (Visuals & Actor Updates only)
                AsyncTask(ENamedThreads::GameThread, [WeakAttacker, WeakTarget, HealthDamage, ShieldDamage]()
                {
                    AUnitBase* StrongAttacker = WeakAttacker.Get();
                    AUnitBase* StrongTarget = WeakTarget.Get();

                    if (StrongAttacker && StrongTarget)
                    {
                        // Update Actor specific attributes (Syncing Mass data to Actor)
                        if (ShieldDamage > 0)
                        {
                            StrongTarget->SetShield_Implementation(StrongTarget->Attributes->GetShield() - ShieldDamage);
                        }
                        if (HealthDamage > 0)
                        {
                            StrongTarget->SetHealth_Implementation(StrongTarget->Attributes->GetHealth() - HealthDamage);
                        }

                        // UI Update
                        if(StrongTarget->HealthWidgetComp)
                        {
                            if (UUnitBaseHealthBar* HealthBarWidget = Cast<UUnitBaseHealthBar>(StrongTarget->HealthWidgetComp->GetUserWidgetObject()))
                            {
                                HealthBarWidget->UpdateWidget();
                            }
                        }

                        // Notify Blueprint
                        StrongTarget->Attacked(StrongAttacker);

                        // GAS / Abilities
                        StrongAttacker->ServerStartAttackEvent_Implementation();

                        // --- THIS LINE SHOULD NOW BE SAFE ---
                        // Because we didn't corrupt memory with invalid fragment pointers earlier
                        if (StrongAttacker->AttackAbilityID != EGASAbilityInputID::None && StrongAttacker->AttackAbilities.Num() > 0)
                        {
                            StrongAttacker->ActivateAbilityByInputID(StrongAttacker->AttackAbilityID, StrongAttacker->AttackAbilities);
                        }
                        
                        StrongAttacker->ServerMeeleImpactEvent();
                        
                        // Fire melee impact VFX/SFX at the target's location via multicast RPC
                        if (APerformanceUnit* PerfAttacker = Cast<APerformanceUnit>(StrongAttacker))
                        {
                            if (PerfAttacker->HasAuthority())
                            {
                                const FVector ImpactLocation = StrongTarget->GetMassActorLocation();

                                const float KillDelay = 2.0f; // Reasonable lifetime for spawned components
                                PerfAttacker->FireEffectsAtLocation(
                                    PerfAttacker->MeleeImpactVFX,
                                    PerfAttacker->MeleeImpactSound,
                                    PerfAttacker->ScaleImpactVFX,
                                    PerfAttacker->ScaleImpactSound,
                                    ImpactLocation,
                                    KillDelay,
                                    PerfAttacker->RotateImpactVFX);
                            }
                        }
                    }
                });
             }
        }
    }
}

void UUnitStateProcessor::UnitRangedAttack(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    if (!EntitySubsystem) return;

    // Use Mutable immediately as we might change State tags/fragments
    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager(); 

    for (FMassEntityHandle& Entity : Entities)
    {
        if (!EntityManager.IsEntityValid(Entity)) continue;

        // 1. Access Fragments Needed for Logic
        FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
        FMassAITargetFragment* TargetFrag = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Entity);
        const FTransformFragment* AttackerTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
        const FMassAgentCharacteristicsFragment* CharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(Entity);
        FMassAIStateFragment* AttackerStateFrag = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);

        // 2. Validate Prereqs
        if (ActorFragPtr && TargetFrag && TargetFrag->bHasValidTarget && TargetFrag->TargetEntity.IsSet() &&
            AttackerTransformFrag && CharFrag && AttackerStateFrag)
        {
             AUnitBase* AttackerUnitBase = Cast<AUnitBase>(ActorFragPtr->GetMutable());
             FMassEntityHandle TargetEntity = TargetFrag->TargetEntity;

             if (AttackerUnitBase && IsValid(AttackerUnitBase->UnitToChase) && AttackerUnitBase->UseProjectile && EntityManager.IsEntityValid(TargetEntity))
             {
                // 3. CHECK RANGE HERE (In Mass Thread)
                // Checking range inside the Async task is risky and inefficient. Do it now.
                const FTransformFragment* TargetTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetEntity);
                const FMassAgentCharacteristicsFragment* TargetCharFrag = EntityManager.GetFragmentDataPtr<FMassAgentCharacteristicsFragment>(TargetEntity);

                if (!TargetTransformFrag || !TargetCharFrag) continue;

                float AttackerRange = AttackerUnitBase->Attributes ? AttackerUnitBase->Attributes->GetRange() : 0.0f;
                float RangeWithCapsule = AttackerRange + CharFrag->CapsuleRadius + TargetCharFrag->CapsuleRadius;
                float AttackRangeSquared = FMath::Square(RangeWithCapsule);

                FVector AttackerLoc = AttackerTransformFrag->GetTransform().GetLocation();
                FVector TargetLoc = TargetTransformFrag->GetTransform().GetLocation();
                float DistSquared = FVector::DistSquared2D(AttackerLoc, TargetLoc);

                // If out of range, cancel attack and stop here
                if (DistSquared > AttackRangeSquared)
                {
                    AttackerStateFrag->SwitchingState = false;
                    continue;
                }

                // 4. CAPTURE DATA FOR ASYNC
                // We know we are in range and valid. Capture what we need for the Visuals/Gameplay.
                TWeakObjectPtr<AUnitBase> WeakAttacker(AttackerUnitBase);
                TWeakObjectPtr<AUnitBase> WeakTarget(AttackerUnitBase->UnitToChase); // Explicitly use the Actor we verified
                
                // Capture IDs by value (safer than checking StrongTarget->ID inside the lambda)
                EGASAbilityInputID AttackAbilityID = AttackerUnitBase->AttackAbilityID;
                EGASAbilityInputID ThrowAbilityID = AttackerUnitBase->ThrowAbilityID;
                EGASAbilityInputID OffensiveAbilityID = AttackerUnitBase->OffensiveAbilityID;
                
                // Copy arrays (Heavy operation, but required if accessing on another thread safely)
                TArray<TSubclassOf<UGameplayAbilityBase>> AttackAbilities = AttackerUnitBase->AttackAbilities;
                TArray<TSubclassOf<UGameplayAbilityBase>> ThrowAbilities = AttackerUnitBase->ThrowAbilities;
                TArray<TSubclassOf<UGameplayAbilityBase>> OffensiveAbilities = AttackerUnitBase->OffensiveAbilities;

                // 5. HANDLE MASS STATE CHANGE
                // Ideally, you switch state here in the processor, not in the async task.
                // However, based on your flow, we will signal the state change in the lambda context 
                // ONLY because we need to pass the entity handle safely. 
                
                // 6. DISPATCH VISUAL/GAMEPLAY TASK
                AsyncTask(ENamedThreads::GameThread, [this, Entity, TargetEntity, WeakAttacker, WeakTarget,
                    AttackAbilityID, ThrowAbilityID, OffensiveAbilityID,
                    AttackAbilities, ThrowAbilities, OffensiveAbilities]() mutable
                {
                    if (!EntitySubsystem) return; // Safety Check

                    AUnitBase* StrongAttacker = WeakAttacker.Get();
                    AUnitBase* StrongTarget = WeakTarget.Get();
                    
                    if (StrongAttacker && StrongTarget)
                    {
                        // --- Core Actor Actions ---
                        StrongAttacker->ServerStartAttackEvent_Implementation();
                        
                        if (IsValid(StrongTarget))
                        {
                            StrongTarget->Attacked(StrongAttacker);
                        }

                        bool bIsActivated = false;

                        // --- FIX: Use Captured IDs, don't check StrongTarget for Attacker's ID ---
                        if(AttackAbilityID != EGASAbilityInputID::None && AttackAbilities.Num() > 0)
                        {
                           bIsActivated = StrongAttacker->ActivateAbilityByInputID(AttackAbilityID, AttackAbilities);
                        }
                    
                        if (!bIsActivated && ThrowAbilityID != EGASAbilityInputID::None && ThrowAbilities.Num() > 0)
                        {
                           bIsActivated = StrongAttacker->ActivateAbilityByInputID(ThrowAbilityID, ThrowAbilities);
                        }
                        
                        if (!bIsActivated && OffensiveAbilityID != EGASAbilityInputID::None && OffensiveAbilities.Num() > 0)
                        {
                            // Range check already passed in Processor
                            bIsActivated = StrongAttacker->ActivateAbilityByInputID(OffensiveAbilityID, OffensiveAbilities);
                        }

                        // Access Entity Manager on GameThread safely
                        FMassEntityManager& GTEntityManager = EntitySubsystem->GetMutableEntityManager();
                        if (!GTEntityManager.IsEntityValid(Entity)) return;

                        if (bIsActivated)
                        {
                           FMassAIStateFragment* AsyncStateFrag = GTEntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
                           if (AsyncStateFrag) AsyncStateFrag->SwitchingState = false;
                           return;
                        }

                        // Fallback: Spawn Projectile manually if no ability activated
                        StrongAttacker->SpawnProjectile(StrongTarget, StrongAttacker);
                        
                        // Switch State (assuming UnitSignals::Attack moves to Cooldown or similar)
                        SwitchState(UnitSignals::Attack, Entity, GTEntityManager);
                    }
                });
             }
        }
    }
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

        if (DetectorUnitBase->UnitToChase != TargetUnitBase)
        {
            DetectorUnitBase->UnitToChase = TargetUnitBase;
        }
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
                    	UnregisterObstacle(UnitBase);
                    	UnitBase->HideHealthWidget(); // Aus deinem Code
                    	UnitBase->KillLoadedUnits();
                    	UnitBase->CanActivateAbilities = false;
                    	UnitBase->GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

                    	if (UnitBase->WorkResource)
                    	{
                    		UnitBase->WorkResource->Destroy(true,true);
                    		UnitBase->WorkResource = nullptr;
                    	}


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
						WorkAreaData::WorkAreaType PlaceType = UnitBase->ResourcePlace->Type;
						EResourceType CorrectResourceType = ConvertToResourceType(PlaceType);
						UnitBase->ExtractingWorkResourceType = CorrectResourceType;
						SpawnWorkResource(CorrectResourceType, UnitBase->GetActorLocation(), UnitBase->ResourcePlace->WorkResourceClass, UnitBase);
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

						if (UnitBase->Base->IsFlying)
						{
							UnitBase->Multicast_SwitchToIdle();
							return;
						}
						
						if (ResourceGameMode)
							UnitBase->Base->HandleBaseArea(UnitBase, ResourceGameMode, CanAffordConstruction);

						
						if (!UnitBase->ResourcePlace)
						{
							UnitBase->SwitchEntityTagByState(UnitData::Idle, UnitData::Idle);
							StateFrag->SwitchingState = false;
							return;
						}

						if (UnitBase->WorkResource)
						{
							UnitBase->WorkResource = nullptr;
						}
						
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


void UUnitStateProcessor::RegisterBuildingAsObstacle(AActor* BuildingActor)
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
   // BuildingActor->OnDestroyed.AddDynamic(this, &UUnitStateProcessor::OnRegisteredActorDestroyed);
}

void UUnitStateProcessor::UnregisterObstacle(AActor* BuildingActor)
{
	if (!IsValid(BuildingActor))
	{
		return;
	}
    
	// Unbind the delegate first, as we need the BuildingActor pointer to do it.
	//BuildingActor->OnDestroyed.RemoveDynamic(this, &UUnitStateProcessor::OnRegisteredActorDestroyed);

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
   							// Ensure the WorkArea is valid and not already finalized for spawning
   							if (IsValid(UnitBase->BuildArea) && !UnitBase->BuildArea->bFinalBuildingSpawned && !UnitBase->BuildArea->Building)
   			   				{
   			   							// Mark as spawned to prevent duplicate spawns from multiple workers
   								UnitBase->BuildArea->bFinalBuildingSpawned = true;
   			   							// If a construction site exists, remove it now
   								float SavedHealth = 0.f;
   								float SavedShield = 0.f;
   								bool bHasSavedStats = false;
   								if (UnitBase->BuildArea->ConstructionUnit)
   								{
   									if (UnitBase->BuildArea->ConstructionUnit->Attributes)
   									{
   										SavedHealth = UnitBase->BuildArea->ConstructionUnit->Attributes->GetHealth();
   										SavedShield = UnitBase->BuildArea->ConstructionUnit->Attributes->GetShield();
   										bHasSavedStats = true;
   									}
   									UnitBase->BuildArea->ConstructionUnit->Destroy(false, true);
   									UnitBase->BuildArea->ConstructionUnit = nullptr;
   								}
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

								// If spawn failed, allow future attempts (keep single-spawn guarantee only on success)
								if (!NewUnit)
								{
									if (UnitBase->BuildArea)
									{
										UnitBase->BuildArea->bFinalBuildingSpawned = false;
									}
								}

								// After InitializeAttributes() inside SpawnSingleUnit, apply preserved stats to the spawned building
								if (bHasSavedStats && NewUnit)
								{
									if (ABuildingBase* SpawnedBuilding = Cast<ABuildingBase>(NewUnit))
									{
										if (SpawnedBuilding->Attributes)
										{
											SpawnedBuilding->SetHealth_Implementation(SavedHealth);
											SpawnedBuilding->SetShield_Implementation(SavedShield);
										}
									}
								}

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

									RegisterBuildingAsObstacle(NewUnit);

									if (NewUnit->bUseSkeletalMovement)
									{
										NewUnit->InitializeUnitMode();
									}
								}
							}
    					}
    					// StateFragment->PlaceholderSignal
    					SwitchState(UnitSignals::GoToBase, Entity, EntityManager);
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

        // Use an object-type trace that only considers WorldStatic geometry (e.g., terrain, static meshes)
        // to avoid hitting Pawns or other dynamic actors which can make buildings spawn mid-air.
        FCollisionObjectQueryParams ObjectParams;
        ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
        bool bHit = GetWorld()->LineTraceSingleByObjectType(
            HitResult,
            TraceStart,
            TraceEnd,
            ObjectParams,
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

    //UnitBase->SetReplicateMovement(true);
	//UnitBase->SetReplicates(true);
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

    // -- ATTACH to the unit’s mesh socket FIRST so the worker won't drop it even if the resource place gets destroyed --
    static const FName SocketName(TEXT("ResourceSocket"));
    if (ActorToLockOn->GetMesh() && ActorToLockOn->GetMesh()->DoesSocketExist(SocketName))
    {
        NewRes->AttachToComponent(
            ActorToLockOn->GetMesh(),
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            SocketName);

        // Offset if needed
        NewRes->SetActorRelativeLocation(NewRes->SocketOffset, false, nullptr, ETeleportType::TeleportPhysics);
    }

    // -- UPDATE the place’s remaining amount --
    float& Remaining     = ActorToLockOn->ResourcePlace->AvailableResourceAmount;
    const float MaxRemain = ActorToLockOn->ResourcePlace->MaxAvailableResourceAmount;

    Remaining = FMath::Max(0.f, Remaining - NewRes->Amount);

    // If we’ve completely depleted it, destroy the place after we've attached the resource
    if (Remaining <= KINDA_SMALL_NUMBER)
    {
        ActorToLockOn->ResourcePlace->Destroy();
        ActorToLockOn->ResourcePlace = nullptr;
    }
    else
    {
        // Otherwise, update the visual scale of the “ResourcePlace” mesh
        float Ratio    = MaxRemain > KINDA_SMALL_NUMBER ? Remaining / MaxRemain : 0.f;
        float NewScale = FMath::Lerp(0.4f, 1.0f, FMath::Clamp(Ratio, 0.f, 1.f));
        ActorToLockOn->ResourcePlace->Multicast_SetScale(FVector(NewScale));
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

						if (!UnitBase->IsWorker) return;
						
						if (!UnitBase->BuildArea || !DoesEntityHaveTag(EntityManager, Entity, FMassStateBuildTag::StaticStruct())) return;
						
						// Ensure worker is registered in the WorkArea while building
						if (AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(UnitBase))
						{
 							if (Worker->BuildArea)
 							{
 								Worker->BuildArea->AddWorkerToArray(Worker);
 							}
 						}
 						// If more than one worker is currently building at this area (clamped by MaxWorkerCount),
 						// increase this worker's build timer additively by 0.5x per second using stored DeltaTime.
 						if (UnitBase->BuildArea)
 						{
 							const int32 MaxAllowed = UnitBase->BuildArea->MaxWorkerCount > 0 ? UnitBase->BuildArea->MaxWorkerCount : UnitBase->BuildArea->Workers.Num();
 							const int32 EffectiveWorkers = FMath::Min(UnitBase->BuildArea->Workers.Num(), MaxAllowed);
 							if (EffectiveWorkers > 1)
 							{
 								StateFrag->StateTimer += 0.5f * StateFrag->DeltaTime*(EffectiveWorkers-1.f);
 								UnitBase->UnitControlTimer = StateFrag->StateTimer;
 							}
 						}
 							if (UnitBase->BuildArea->CurrentBuildTime > UnitBase->UnitControlTimer)
 							{
 								StateFrag->StateTimer = UnitBase->BuildArea->CurrentBuildTime;
 								UnitBase->UnitControlTimer = UnitBase->BuildArea->CurrentBuildTime;
 							}
 							else
 							{
 								UnitBase->BuildArea->CurrentBuildTime = UnitBase->UnitControlTimer;
 							}

							// Construction site handling (optional)
							if (UnitBase->BuildArea && UnitBase->BuildArea->ConstructionUnitClass)
							{
								//const float TotalDuration = (UnitBase->CastTime > 0.f) ? UnitBase->CastTime : FMath::Max(0.01f, UnitBase->BuildArea->BuildTime);
								const float Progress = FMath::Clamp(UnitBase->UnitControlTimer / UnitBase->BuildArea->BuildTime, 0.f, 1.f);
								// Early safeguard: if a construction unit exists but is dead, hide and prevent any respawn
								if (UnitBase->BuildArea->ConstructionUnit && UnitBase->BuildArea->ConstructionUnit->Attributes)
								{
									const float CurrHP = UnitBase->BuildArea->ConstructionUnit->Attributes->GetHealth();
									if (CurrHP <= 0.f)
									{
										UnitBase->BuildArea->ConstructionUnit->SetHidden(true);
										UnitBase->BuildArea->bConstructionUnitSpawned = true; // lock out respawn
										continue;
									}
								}
								// spawn at 5-10% only once
								if (!UnitBase->BuildArea->ConstructionUnit && !UnitBase->BuildArea->bConstructionUnitSpawned && Progress >= 0.05f && Progress <= 0.10f)
								{
 								// location from work area, adjusted later to ground
 								const FVector BaseLoc = UnitBase->BuildArea->GetActorLocation();
 								const FTransform SpawnTM(FRotator::ZeroRotator, BaseLoc);
 								AUnitBase* NewConstruction = GetWorld()->SpawnActorDeferred<AUnitBase>(UnitBase->BuildArea->ConstructionUnitClass, SpawnTM, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 								if (NewConstruction)
 								{
												// Assign critical properties BEFORE finishing spawn so they're valid during BeginPlay/replication
												NewConstruction->TeamId = UnitBase->TeamId;

												// Assign the BuildingClass DefaultAttributeEffect to the ConstructionUnit so it gets the same attributes
												if (UnitBase->BuildArea && UnitBase->BuildArea->BuildingClass)
												{
													if (ABuildingBase* BuildingCDO = UnitBase->BuildArea->BuildingClass->GetDefaultObject<ABuildingBase>())
													{
														NewConstruction->DefaultAttributeEffect = BuildingCDO->DefaultAttributeEffect;
													}
												}

												// assign pointers if it is a AConstructionUnit
												if (AConstructionUnit* CU = Cast<AConstructionUnit>(NewConstruction))
												{
													CU->Worker = UnitBase;
													CU->WorkArea = UnitBase->BuildArea;
												}
												// Finish spawning after initializing properties
												NewConstruction->FinishSpawning(SpawnTM);
												if (NewConstruction->AbilitySystemComponent)
												{
													NewConstruction->AbilitySystemComponent->InitAbilityActorInfo(NewConstruction, NewConstruction);
													NewConstruction->InitializeAttributes();
												}

									

												// Fit, center, and ground-align construction unit to WorkArea footprint
												{
													// Determine target area bounds and center
													FBox AreaBox = UnitBase->BuildArea->Mesh ? UnitBase->BuildArea->Mesh->Bounds.GetBox() : UnitBase->BuildArea->GetComponentsBoundingBox(true);
													const FVector AreaCenter = AreaBox.GetCenter();
													const FVector AreaSize = AreaBox.GetSize();
													
													// Ground trace at the area center to find floor Z
													FHitResult Hit;
													FVector Start = AreaCenter + FVector(0, 0, 10000.f);
													FVector End   = AreaCenter - FVector(0, 0, 10000.f);
													FCollisionQueryParams Params;
													Params.AddIgnoredActor(UnitBase->BuildArea);
													Params.AddIgnoredActor(NewConstruction);
													bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
													float GroundZ = bHit ? Hit.Location.Z : NewConstruction->GetActorLocation().Z;
													
													// Compute uniform scale to fit XY footprint (use full sizes)
													FBox PreBox = NewConstruction->GetComponentsBoundingBox(true);
													FVector UnitSize = PreBox.GetSize();
													if (!UnitSize.IsNearlyZero(1e-3f) && AreaSize.X > KINDA_SMALL_NUMBER && AreaSize.Y > KINDA_SMALL_NUMBER)
													{
														// Slight margin keeps it visually inside
     													const float Margin = 0.98f;
     													const float ScaleX = (AreaSize.X * Margin) / UnitSize.X;
     													const float ScaleY = (AreaSize.Y * Margin) / UnitSize.Y;
														const float ScaleZ = (AreaSize.Z * Margin) / UnitSize.Z;
     													const float Uniform = FMath::Max(FMath::Min(ScaleX, ScaleY), 0.1f);
     													// Apply XY scaling; optionally scale Z if ConstructionUnit.ScaleZ is true
     													FVector NewScale = NewConstruction->GetActorScale3D();
     													NewScale.X = Uniform;
     													NewScale.Y = Uniform;
														NewScale.Z = Uniform;
     													if (const AConstructionUnit* CU_ScaleCheck = Cast<AConstructionUnit>(NewConstruction))
     													{
     														if (CU_ScaleCheck->ScaleZ)
     														{
     															NewScale.Z = ScaleZ;
     														}
     													}
     													NewConstruction->SetActorScale3D(NewScale*2.f);
													}
													
    												// Recompute bounds after scale and compute single final location
    												FBox ScaledBox = NewConstruction->GetComponentsBoundingBox(true);
    												const FVector UnitCenter = ScaledBox.GetCenter();
    												const float BottomZ = ScaledBox.Min.Z;
    												FVector FinalLoc = NewConstruction->GetActorLocation();
    												FinalLoc.X += (AreaCenter.X - UnitCenter.X);
    												FinalLoc.Y += (AreaCenter.Y - UnitCenter.Y);
    												FinalLoc.Z += (GroundZ - BottomZ);
    												NewConstruction->SetActorLocation(FinalLoc);
												}

															// store pointer on area
															UnitBase->BuildArea->ConstructionUnit = NewConstruction;
															UnitBase->BuildArea->bConstructionUnitSpawned = true;

													// start construction animations (rotate + oscillate) for remaining build time (95%)
    									if (AConstructionUnit* CU_Anim = Cast<AConstructionUnit>(NewConstruction))
    									{
    										const float AnimDuration = UnitBase->BuildArea->BuildTime * 0.95f; // BuildTime minus 5%
    										CU_Anim->MulticastStartRotateVisual(CU_Anim->DefaultRotateAxis, CU_Anim->DefaultRotateDegreesPerSecond, AnimDuration);
    										CU_Anim->MulticastStartOscillateVisual(CU_Anim->DefaultOscOffsetA, CU_Anim->DefaultOscOffsetB, CU_Anim->DefaultOscillationCyclesPerSecond, AnimDuration);
    										// Start multiplicative pulsating scale around base (configured on construction unit)
    										if (CU_Anim->bPulsateScaleDuringBuild)
    										{
    											CU_Anim->MulticastPulsateScale(CU_Anim->PulsateMinMultiplier, CU_Anim->PulsateMaxMultiplier, CU_Anim->PulsateTimeMinToMax, true);
    										}
    									}

													// set initial health (>=5%)
													if (NewConstruction->Attributes)
													{
    													const float MaxHP = NewConstruction->Attributes->GetMaxHealth();
														const float MaxShield = NewConstruction->Attributes->GetMaxShield();
    													NewConstruction->SetHealth(MaxHP * FMath::Max(Progress, 0.05f));
														NewConstruction->SetShield(MaxShield * FMath::Max(Progress, 0.05f));
    													UnitBase->BuildArea->LastAppliedBuildProgress = FMath::Max(Progress, 0.05f);
													}
												}
											}

								// Update health and shield over time additively based on progress delta
								if (UnitBase->BuildArea->ConstructionUnit && UnitBase->BuildArea->ConstructionUnit->Attributes)
								{
									const float MaxHP = UnitBase->BuildArea->ConstructionUnit->Attributes->GetMaxHealth();
									const float MaxShield = UnitBase->BuildArea->ConstructionUnit->Attributes->GetMaxShield();
									float PreviousProgress = UnitBase->BuildArea->LastAppliedBuildProgress;
									const float CurrentHealth = UnitBase->BuildArea->ConstructionUnit->Attributes->GetHealth();
									const float CurrentShield = UnitBase->BuildArea->ConstructionUnit->Attributes->GetShield();
									// If we resume mid-build and have non-zero health but no tracked progress yet, infer it from current health
									if (CurrentHealth <= 0.f)
									{
										UnitBase->BuildArea->ConstructionUnit->SetHidden(true);
										UnitBase->BuildArea->bConstructionUnitSpawned = true;
										continue;
									}
									
									if (PreviousProgress <= 0.f && MaxHP > KINDA_SMALL_NUMBER)
									{
										PreviousProgress = FMath::Clamp(CurrentHealth / MaxHP, 0.f, 1.f);
									}
									const float StepProgress = FMath::Max(0.f, Progress - PreviousProgress);
									if (StepProgress > 0.f)
									{
										// Health
										const float StepHealth = MaxHP * StepProgress;
										const float NewHealth = FMath::Clamp(CurrentHealth + StepHealth, 0.f, MaxHP);
										UnitBase->BuildArea->ConstructionUnit->SetHealth(NewHealth);
										
										// Shield
										const float StepShield = MaxShield * StepProgress;
										const float NewShield = FMath::Clamp(CurrentShield + StepShield, 0.f, MaxShield);
										UnitBase->BuildArea->ConstructionUnit->SetShield(NewShield);
										
										UnitBase->BuildArea->LastAppliedBuildProgress = Progress;
									}
								}
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

							// Stop ConstructionUnit pulsation when casting ends
							if (UnitBase->BuildArea && UnitBase->BuildArea->ConstructionUnit)
							{
								if (AConstructionUnit* CU_Stop = Cast<AConstructionUnit>(UnitBase->BuildArea->ConstructionUnit))
								{
									CU_Stop->MulticastPulsateScale(CU_Stop->PulsateMinMultiplier, CU_Stop->PulsateMaxMultiplier, CU_Stop->PulsateTimeMinToMax, false);
								}
							}

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
						
						SwitchState(StateFrag->PlaceholderSignal, Entity, EntityManager);
					}
				}
			}
		}
	});
}

void UUnitStateProcessor::HandleSightSignals(FName /*SignalName*/, TArray<FMassEntityHandle>& /*Entities*/)
{
	// Sight handling moved to UnitSightProcessor to run on both client and server.
	if (World)
	{
 	if (GetWorld() && GetWorld()->IsNetMode(NM_Client))
 	{
 		UE_LOG(LogTemp, Warning, TEXT("[UnitStateProcessor::HandleSightSignals] Deprecated. Sight signals are now handled by UnitSightProcessor. NetMode=%d"), (int32)World->GetNetMode());
 	}
	}
	return;
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
						// Mirror this initial patrol move to clients
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
					      StateFrag.StoredLocation = ResourcePosition;
						
					    UpdateMoveTarget(MoveTarget, StateFrag.StoredLocation, StatsFrag.RunSpeed, World);
						SwitchState(UnitSignals::GoToResourceExtraction, E, EntityManager);
					}else if (Unit->Base)
					{
						FVector BasePosition = FindGroundLocationForActor(this, Unit->Base, {Unit, Unit->Base});
						WorkerStatsFrag->ResourcePosition = BasePosition;
					    StateFrag.StoredLocation = BasePosition;
						
					    UpdateMoveTarget(MoveTarget, StateFrag.StoredLocation, StatsFrag.RunSpeed, World);
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
	// Helper to always send signals on the next GameThread tick to avoid re-entrancy in MassSignals
	auto SendSignalSafe = [this](const FName InSignal, const FMassEntityHandle InEntity)
	{
		TWeakObjectPtr<UMassSignalSubsystem> WeakSignal = SignalSubsystem;
		AsyncTask(ENamedThreads::GameThread, [WeakSignal, InSignal, InEntity]()
		{
			if (UMassSignalSubsystem* Strong = WeakSignal.Get())
			{
				Strong->SignalEntity(InSignal, InEntity);
			}
		});
	};

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
						FMassAIStateFragment* StateFragPtr = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
						const FMassCombatStatsFragment* StatsFragPtr = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity);

						if (!WorkerStatsFrag || !MoveTargetPtr || !StatsFragPtr || !StateFragPtr) return;
						
						FMassMoveTargetFragment& MoveTarget = *MoveTargetPtr;
						FMassAIStateFragment& StateFrag = *StateFragPtr;
						const FMassCombatStatsFragment& StatsFrag = *StatsFragPtr;
				
						if (UnitBase->UnitState == UnitData::GoToResourceExtraction)
						{
							StateFrag.StoredLocation = WorkerStatsFrag->ResourcePosition;
							UpdateMoveTarget(MoveTarget, StateFrag.StoredLocation, StatsFrag.RunSpeed, World);
						}
						else if (UnitBase->UnitState == UnitData::GoToBase)
						{
							TArray<FMassEntityHandle> CapturedEntitys;
							CapturedEntitys.Emplace(Entity);
							HandleGetClosestBaseArea(UnitSignals::GetClosestBase, CapturedEntitys);
							
							if (UnitBase->WorkResource)
							{
								StateFrag.StoredLocation = WorkerStatsFrag->BasePosition;
								UpdateMoveTarget(MoveTarget, StateFrag.StoredLocation, StatsFrag.RunSpeed, World);
							}
						}
						else if (UnitBase->UnitState == UnitData::GoToBuild)
						{
							StateFrag.StoredLocation = WorkerStatsFrag->BuildAreaPosition;
							UpdateMoveTarget(MoveTarget, StateFrag.StoredLocation, StatsFrag.RunSpeed, World);
						}
					}
				}
			}
		}
	//});
}


void UUnitStateProcessor::UpdateUnitArrayMovement(FMassEntityHandle& Entity, AUnitBase* UnitBase)
{
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// Helper to avoid re-entrant writes inside MassSignals by deferring to GT
	auto SendSignalSafe = [this](const FName InSignal, const FMassEntityHandle InEntity)
	{
		TWeakObjectPtr<UMassSignalSubsystem> WeakSignal = SignalSubsystem;
		AsyncTask(ENamedThreads::GameThread, [WeakSignal, InSignal, InEntity]()
		{
			if (UMassSignalSubsystem* Strong = WeakSignal.Get())
			{
				Strong->SignalEntity(InSignal, InEntity);
			}
		});
	};

	if (!EntityManager.IsEntityValid(Entity) || !UnitBase)
	{
		return;
	}

	if (UnitBase->RunLocationArray.Num())
	{
		// Get the required fragments from the entity
		const FTransformFragment* TransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity);
		FMassMoveTargetFragment* MoveTargetFrag = EntityManager.GetFragmentDataPtr<FMassMoveTargetFragment>(Entity);
		const FMassCombatStatsFragment* StatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity);

		if (!TransformFrag || !MoveTargetFrag || !StatsFrag) return;

		const FVector CurrentLocation = TransformFrag->GetTransform().GetLocation();
		const FVector CurrentWaypoint = UnitBase->RunLocationArray[0];

		// Define a distance to consider the waypoint "reached"
		// Using squared distance is more efficient as it avoids a square root calculation
		constexpr float ReachedDistanceSquared = 100.f * 100.f; // 1 meter

		// Check if the entity has reached the current waypoint
		if (FVector::DistSquared(CurrentLocation, CurrentWaypoint) < ReachedDistanceSquared)
		{
			// Waypoint reached, remove it from the path
			UnitBase->RunLocationArray.RemoveAt(0);

			// If there are more waypoints, update the move target to the next one
			if (UnitBase->RunLocationArray.Num() > 0)
			{
				if (FMassAIStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity))
				{
					StateFrag->StoredLocation = UnitBase->RunLocationArray[0];
				}
				
				UpdateMoveTarget(*MoveTargetFrag, UnitBase->RunLocationArray[0], StatsFrag->RunSpeed, GetWorld());
				SwitchState(UnitSignals::Run, Entity, EntityManager);
			}
		}
	}
}


void UUnitStateProcessor::UpdateUnitMovement(FMassEntityHandle& Entity, AUnitBase* UnitBase)
{
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	// Helper to route Mass signals to the Game Thread to avoid MTAccessDetector re-entrancy
	auto SendSignalSafe = [this](const FName InSignal, const FMassEntityHandle InEntity)
	{
		TWeakObjectPtr<UMassSignalSubsystem> WeakSignal = SignalSubsystem;
		AsyncTask(ENamedThreads::GameThread, [WeakSignal, InSignal, InEntity]()
		{
			if (UMassSignalSubsystem* Strong = WeakSignal.Get())
			{
				Strong->SignalEntity(InSignal, InEntity);
			}
		});
	};

	if (!EntityManager.IsEntityValid(Entity) || !UnitBase)
	{
		if (World && World->GetNetMode() == NM_Client && UnitBase)
		{
			//UE_LOG(LogTemp, Warning, TEXT("[RTS.Replication] UpdateUnitMovement: Invalid Mass entity or UnitBase for %s on client. Destroying local actor to clean up zombie."), *UnitBase->GetName());
			TWeakObjectPtr<AUnitBase> WeakUnit(UnitBase);
			AsyncTask(ENamedThreads::GameThread, [WeakUnit]()
			{
				if (AUnitBase* Strong = WeakUnit.Get())
				{
					Strong->Destroy();
				}
			});
		}
		return;
	}
	
	if (UnitBase->IsWorker)
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
		}
		else if (UnitBase->UnitState == UnitData::GoToBase)
		{
			StateFraggPtr->StoredLocation = WorkerStatsFrag->BasePosition;
			UpdateMoveTarget(MoveTarget, WorkerStatsFrag->BasePosition, StatsFrag.RunSpeed, World);
		}
		else if (UnitBase->UnitState == UnitData::GoToBuild)
		{
			StateFraggPtr->StoredLocation = WorkerStatsFrag->BuildAreaPosition;
			UpdateMoveTarget(MoveTarget, WorkerStatsFrag->BuildAreaPosition, StatsFrag.RunSpeed, World);
		}
		else if (UnitBase->UnitState == UnitData::GoToRepair)
		{
			// Move towards the friendly repair target if available; otherwise use last known friendly location
			if (const FMassAITargetFragment* TargetFrag = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Entity))
			{
				FVector Goal = TargetFrag->LastKnownFriendlyLocation;
				if (EntityManager.IsEntityValid(TargetFrag->FriendlyTargetEntity))
				{
					if (const FTransformFragment* FriendlyXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetFrag->FriendlyTargetEntity))
					{
						Goal = FriendlyXform->GetTransform().GetLocation();
					}
				}
				StateFraggPtr->StoredLocation = Goal;
				UpdateMoveTarget(MoveTarget, Goal, StatsFrag.RunSpeed, World);
			}
		}
		else if (UnitBase->UnitState == UnitData::Idle)
		{
			StopMovement(MoveTarget, World);
		}
	}
}


void UUnitStateProcessor::SyncRepairTime(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	if (!EntitySubsystem)
	{
		return;
	}

	TArray<FMassEntityHandle> EntitiesCopy = Entities;
	AsyncTask(ENamedThreads::GameThread, [this, EntitiesCopy]() mutable
	{
		if (!EntitySubsystem) { return; }
		FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

		for (const FMassEntityHandle& Entity : EntitiesCopy)
		{
			if (!EntityManager.IsEntityValid(Entity)) { continue; }
			FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
			FMassAIStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
			if (!ActorFragPtr || !StateFrag) { continue; }

			AUnitBase* UnitBase = Cast<AUnitBase>(ActorFragPtr->GetMutable());
			if (!UnitBase) { continue; }

			// Only workers can repair
			if (!UnitBase->IsWorker) { continue; }

			// Require a valid repair target and not building
			if (UnitBase->BuildArea || !UnitBase->FollowUnit || !UnitBase->CanRepair || !UnitBase->FollowUnit->CanBeRepaired)
			{
				continue;
			}

			AUnitBase* Target = UnitBase->FollowUnit;
			float MyRadius = 0.f, TargetRadius = 0.f;
			if (UCapsuleComponent* MyCapsule = UnitBase->GetCapsuleComponent()) MyRadius = MyCapsule->GetScaledCapsuleRadius();
			if (UCapsuleComponent* TRCapsule = Target->GetCapsuleComponent()) TargetRadius = TRCapsule->GetScaledCapsuleRadius();
			const AWorkingUnitBase* Worker = Cast<AWorkingUnitBase>(UnitBase);
			const float RepairReach = MyRadius + TargetRadius + (Worker ? Worker->RepairDistance : 50.f);
			const float Dist2D = FVector::Dist2D(UnitBase->GetMassActorLocation(), Target->GetMassActorLocation());

			// If out of range while repairing, go back to GoToRepair to re-approach (with hysteresis)
			const float ExitBuffer = 40.f;
			if (Dist2D > (RepairReach + ExitBuffer))
			{
				FMassEntityHandle MutableEntity = Entity;
				SwitchState(UnitSignals::GoToRepair, MutableEntity, EntityManager);
				continue;
			}

			// Compute CastTime and progress from target health
			const float RepairRate = FMath::Max((Worker ? Worker->RepairHealth : 10.f), 0.001f);
			if (Target->Attributes)
			{
				const float MaxHP = Target->Attributes->GetMaxHealth();
				if (MaxHP > 0.f)
				{
					UnitBase->CastTime = MaxHP / RepairRate;
					if (FMassCombatStatsFragment* StatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity))
					{
						StatsFrag->CastTime = UnitBase->CastTime;
					}
				}
				const float CurrHPNow = Target->Attributes->GetHealth();
				const float Elapsed = CurrHPNow / RepairRate;
				UnitBase->UnitControlTimer = Elapsed;
				StateFrag->StateTimer = Elapsed;
			}

			// Apply heal once per second
			static TMap<AUnitBase*, float> LastRepairTick;
			const float Now = World ? World->GetTimeSeconds() : 0.f;
			float& LastTick = LastRepairTick.FindOrAdd(UnitBase);
			if (World && (Now - LastTick) >= 1.0f)
			{
				LastTick = Now;
				if (Target->Attributes)
				{
					const float MaxHP = Target->Attributes->GetMaxHealth();
					const float CurrHP = Target->Attributes->GetHealth();
					const float HealAmt = Worker ? Worker->RepairHealth : 10.f;
					const float NewHP = FMath::Clamp(CurrHP + HealAmt, 0.f, MaxHP);
					Target->SetHealth(NewHP);
				}
			}

			// If target fully repaired: clear follow and return to Idle
			if (!Target->Attributes || Target->Attributes->GetHealth() >= Target->Attributes->GetMaxHealth())
			{
				UnitBase->FollowUnit = nullptr;
				FMassEntityHandle MutableEntity = Entity;
				SwitchState(UnitSignals::GoToBase, MutableEntity, EntityManager);
				continue;
			}
		}
	});
}
