// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Signals/UnitStateProcessor.h"

// Source: UnitStateProcessor.cpp
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
        UE_LOG(LogTemp, Log, TEXT("UUnitStateProcessor Initializing Signal Handlers..."));

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

            UE_LOG(LogTemp, Log, TEXT("Registered ChangeUnitState for signal: %s"), *SignalName.ToString());
        }

    	SetUnitToChaseSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::SetUnitToChase)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, SetUnitToChase));

    	MeleeAttackSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::MeleeAttack)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, UnitMeeleAttack));

    	RangedAttackSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::RangedAttack)
				.AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UUnitStateProcessor, UnitRangedAttack));

    }else
    {
        UE_LOG(LogTemp, Error, TEXT("UUnitStateProcessor failed to get SignalSubsystem!"));
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

void UUnitStateProcessor::ChangeUnitState(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    // **Keep initial checks outside AsyncTask if possible and thread-safe**
    if (!EntitySubsystem)
    {
        // Log error - This check itself is generally safe
        UE_LOG(LogTemp, Error, TEXT("ChangeUnitState called but EntitySubsystem is null!"));
        return;
    }

    // **Capture necessary data for the lambda**
    // Capture 'this' to access EntitySubsystem inside the lambda.
    // Capture SignalName by value.
    // Capture Entities by value (TArray copy) to ensure lifetime if the original goes out of scope.
    // Alternatively, capture by reference if you are certain the original TArray will live long enough,
    // but copying is safer for async tasks unless performance is critical.
    TArray<FMassEntityHandle> EntitiesCopy = Entities; 

    AsyncTask(ENamedThreads::GameThread, [this, SignalName, EntitiesCopy]() mutable // mutable if you modify captures (not needed here)
    {
        // **Code inside this lambda now runs on the Game Thread**

        // Re-check EntitySubsystem just in case? Usually fine if 'this' is valid.
        if (!EntitySubsystem) 
        {
             UE_LOG(LogTemp, Error, TEXT("ChangeUnitState (GameThread): EntitySubsystem became null!"));
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
                        
                        UE_LOG(LogTemp, Log, TEXT("Setting Unit %s (Entity %d:%d) state to %s via Signal."), 
                               *UnitBase->GetName(), Entity.Index, Entity.SerialNumber, *SignalName.ToString()); // Use SignalName in log

                        // --- Add new tag ---
                        if (SignalName == UnitSignals::Idle) { EntityManager.Defer().AddTag<FMassStateIdleTag>(Entity); }
                        else if (SignalName == UnitSignals::Chase)
                        {
                        	EntityManager.Defer().AddTag<FMassStateDetectTag>(Entity);
	                        EntityManager.Defer().AddTag<FMassStateChaseTag>(Entity);
                        }
                        else if (SignalName == UnitSignals::Attack)
                        {
                        	EntityManager.Defer().RemoveTag<FMassStateDetectTag>(Entity);
	                        EntityManager.Defer().AddTag<FMassStateAttackTag>(Entity);
                        }
                        else if (SignalName == UnitSignals::Dead) { EntityManager.Defer().AddTag<FMassStateDeadTag>(Entity); }
                        else if (SignalName == UnitSignals::PatrolIdle) { EntityManager.Defer().AddTag<FMassStatePatrolIdleTag>(Entity); }
                        else if (SignalName == UnitSignals::PatrolRandom) { EntityManager.Defer().AddTag<FMassStatePatrolRandomTag>(Entity); }
                        else if (SignalName == UnitSignals::Pause)
                        {
                        	EntityManager.Defer().RemoveTag<FMassStateDetectTag>(Entity);
                        	EntityManager.Defer().AddTag<FMassStatePauseTag>(Entity);
                        }
                        else if (SignalName == UnitSignals::Run) { EntityManager.Defer().AddTag<FMassStateRunTag>(Entity); }
                        else if (SignalName == UnitSignals::Casting) { EntityManager.Defer().AddTag<FMassStateCastingTag>(Entity); }
                        else if (SignalName == UnitSignals::IsAttacked) { EntityManager.Defer().AddTag<FMassStateIsAttackedTag>(Entity); }
                        
                        // --- Call Actor Function (Usually needs Game Thread too) ---
                        // Assuming UnitData enum maps directly to SignalName logic
                        if (SignalName == UnitSignals::Idle) { UnitBase->SetUnitState(UnitData::Idle); }
                        else if (SignalName == UnitSignals::Chase) { UnitBase->SetUnitState(UnitData::Chase); }
                        else if (SignalName == UnitSignals::Attack) { UnitBase->SetUnitState(UnitData::Attack); }
                        else if (SignalName == UnitSignals::Dead) { UnitBase->SetUnitState(UnitData::Dead); }
                        else if (SignalName == UnitSignals::PatrolIdle) { UnitBase->SetUnitState(UnitData::PatrolIdle); }
                        else if (SignalName == UnitSignals::PatrolRandom) { UnitBase->SetUnitState(UnitData::PatrolRandom); }
                        else if (SignalName == UnitSignals::Pause) { UnitBase->SetUnitState(UnitData::Pause); }
                        else if (SignalName == UnitSignals::Run) { UnitBase->SetUnitState(UnitData::Run); }
                        else if (SignalName == UnitSignals::Casting) { UnitBase->SetUnitState(UnitData::Casting); }
                        else if (SignalName == UnitSignals::IsAttacked) { UnitBase->SetUnitState(UnitData::IsAttacked); }
                    }
                    // ... rest of your else logs for invalid casts/actors ...
                }
                 else { UE_LOG(LogTemp, Warning, TEXT("ChangeUnitState (GameThread): Actor pointer in ActorFragment for Entity %d:%d is invalid."), Entity.Index, Entity.SerialNumber); }
            }
             else { UE_LOG(LogTemp, Warning, TEXT("ChangeUnitState (GameThread): Entity %d:%d has no ActorFragment."), Entity.Index, Entity.SerialNumber); }
        } // End For loop
    }); // End AsyncTask Lambda
}

void UUnitStateProcessor::UnitMeeleAttack(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    if (!EntitySubsystem)
    {
       UE_LOG(LogTemp, Error, TEXT("UnitMeeleAttack called but EntitySubsystem is null!")); // Changed log message context
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
            const FMassAITargetFragment* TargetFrag = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Entity);
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
                    UE_LOG(LogTemp, Warning, TEXT("UnitMeeleAttack: Target entity from TargetFragment is invalid for attacker %s."), *UnitBase->GetName());
                    continue; // Skip this attack
                }

                // Capture necessary variables for the AsyncTask
                TWeakObjectPtr<AUnitBase> WeakAttacker(UnitBase);
                TWeakObjectPtr<AUnitBase> WeakTarget(UnitBase->UnitToChase);
                bool bIsMagicDamage = UnitBase->IsDoingMagicDamage; // Capture simple types by value

                // *** FIX: Capture 'this', 'AttackerEntity', and 'TargetEntity' in the lambda ***
                AsyncTask(ENamedThreads::GameThread, [this, AttackerEntity, TargetEntity, WeakAttacker, WeakTarget, bIsMagicDamage, MoveTargetFragmentPtr, TargetStateFrag, TargetStatsFragmentPtr]() mutable
                {
                    // Get Strong Pointers for Actor operations
                    AUnitBase* StrongAttacker = WeakAttacker.Get();
                    AUnitBase* StrongTarget = WeakTarget.Get();

                    // *** FIX: 'this' is now captured, so EntitySubsystem is accessible ***
                    if (!EntitySubsystem)
                    {
                         UE_LOG(LogTemp, Error, TEXT("UnitMeeleAttack (GameThread): EntitySubsystem became null!"));
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
                            UE_LOG(LogTemp, Warning, TEXT("UnitMeeleAttack (GameThread): Failed to get CombatStatsFragment for Attacker (%d) or Target (%d)."), AttackerStats != nullptr, TargetStats != nullptr);
                            return;
                        }

                    	/*
                    	UE_LOG(LogTemp, Log, TEXT("111HEALTH!!!!!: %f"), StrongTarget->Attributes->GetHealth());
						UE_LOG(LogTemp, Log, TEXT("2222HEALTH!!!!!: %f"), TargetStats->Health);
                    	UE_LOG(LogTemp, Log, TEXT("111SHIELD!!!!!: %f"), StrongTarget->Attributes->GetShield());
						UE_LOG(LogTemp, Log, TEXT("2222SHIELD!!!!!: %f"), TargetStats->Shield);
                    	*/
                        // --- Apply Damage using Fragment Stats (Example from previous answer) ---
                        float BaseDamage = AttackerStats->AttackDamage;
                        float Defense = bIsMagicDamage ? TargetStats->MagicResistance : TargetStats->Armor;
                        float DamageAfterDefense = FMath::Max(0.0f, BaseDamage - Defense);

                        UE_LOG(LogTemp, Log, TEXT("Unit %s attacking Unit %s. BaseDamage: %.2f, Defense: %.2f, FinalDamageApplied: %.2f, MagicDamage: %s"),
                               *StrongAttacker->GetName(), *StrongTarget->GetName(), BaseDamage, Defense, DamageAfterDefense, bIsMagicDamage ? TEXT("True") : TEXT("False"));

                        float RemainingDamage = DamageAfterDefense;
                        if (TargetStats->Shield > 0)
                        {
                        	float ShieldDamage = FMath::Min(TargetStats->Shield, RemainingDamage);
                        	TargetStats->Shield -= ShieldDamage;
                        	StrongTarget->SetShield_Implementation(StrongTarget->Attributes->GetShield() - ShieldDamage);
                        	RemainingDamage -= ShieldDamage;
                        	UE_LOG(LogTemp, Log, TEXT("Unit %s Shield Damage: %.2f. Shield Remaining: %.2f"), *StrongTarget->GetName(), ShieldDamage, TargetStats->Shield);
                        }
                        if (RemainingDamage > 0)
                        {
                        	float HealthDamage = FMath::Min(TargetStats->Health, RemainingDamage);
                        	TargetStats->Health -= HealthDamage;
                        	StrongTarget->SetHealth_Implementation(StrongTarget->Attributes->GetHealth() - HealthDamage);
                        	UE_LOG(LogTemp, Log, TEXT("Unit %s Health Damage: %.2f. Health Remaining: %.2f"), *StrongTarget->GetName(), HealthDamage, TargetStats->Health);
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
                    	/*
                    	UE_LOG(LogTemp, Log, TEXT("???111HEALTH!!!!!: %f"), StrongTarget->Attributes->GetHealth());
						UE_LOG(LogTemp, Log, TEXT("???2222HEALTH!!!!!: %f"), TargetStats->Health);
						UE_LOG(LogTemp, Log, TEXT("???111SHIELD!!!!!: %f"), StrongTarget->Attributes->GetShield());
						UE_LOG(LogTemp, Log, TEXT("???2222SHIELD!!!!!: %f"), TargetStats->Shield);
                    	*/			
                        StrongAttacker->ServerMeeleImpactEvent();
                        StrongTarget->ActivateAbilityByInputID(StrongTarget->DefensiveAbilityID, StrongTarget->DefensiveAbilities);
                        StrongTarget->FireEffects(StrongAttacker->MeleeImpactVFX, StrongAttacker->MeleeImpactSound, StrongAttacker->ScaleImpactVFX, StrongAttacker->ScaleImpactSound, StrongAttacker->MeeleImpactVFXDelay, StrongAttacker->MeleeImpactSoundDelay);

                    	/*
                        if (!StrongTarget->UnitsToChase.Contains(StrongAttacker))
                        {
                            StrongTarget->UnitsToChase.Emplace(StrongAttacker);
                            StrongTarget->SetNextUnitToChase();
                        }*/

                    	if (TargetStats->Health <= 0.f)
                    	{

                    		UE_LOG(LogTemp, Warning, TEXT("!!!!!!!!!!!!!!!!!!!SET BACK TO RUN!!!!!!!!!!!!!"));
                    		UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
							if (SignalSubsystem) SignalSubsystem->SignalEntity(UnitSignals::Run, AttackerEntity);
                    		
                    		if (MoveTargetFragmentPtr && TargetStateFrag && TargetStatsFragmentPtr)
                    		{
								 UpdateMoveTarget(*MoveTargetFragmentPtr, TargetStateFrag->StoredLocation, TargetStatsFragmentPtr->RunSpeed, World);
							}
						}
                        // Check for death / Set state
                        if (TargetStats->Health <= 0)
                        {
                        	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
							if (SignalSubsystem) SignalSubsystem->SignalEntity(UnitSignals::Dead, TargetEntity);
                        	
                             UE_LOG(LogTemp, Log, TEXT("Unit %s died from attack by %s."), *StrongTarget->GetName(), *StrongAttacker->GetName());
                        }
                        else if( StrongTarget->GetUnitState() != UnitData::Run && StrongTarget->GetUnitState() != UnitData::Pause && StrongTarget->GetUnitState() != UnitData::Attack)
                        {
                        	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
							if (SignalSubsystem) SignalSubsystem->SignalEntity(UnitSignals::IsAttacked, TargetEntity);
                        	
                        	TargetAIStateFragment->StateTimer = 0.f;
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
                    else
                    {
                         UE_LOG(LogTemp, Warning, TEXT("UnitMeeleAttack (GameThread): Attacker or Target Actor/Entity became invalid before task could execute."));
                    }
                }); // End AsyncTask Lambda
             } // End check for valid UnitBase, Target, etc.
          } // End IsValid(Actor)
       } // End ActorFragPtr check
    } // End loop through Entities
}

/*
void UUnitStateProcessor::UnitMeeleAttack(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	if (!EntitySubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("OnUnitReachedDestination called but EntitySubsystem is null!"));
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	for (const FMassEntityHandle& Entity : Entities)
	{
		if (!EntityManager.IsEntityValid(Entity)) // Double check entity validity
		{
			continue;
		}
        
		FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
		if (ActorFragPtr)
		{
			AActor* Actor = ActorFragPtr->GetMutable(); // Use Get() for const access if possible, GetMutable() otherwise
			if (IsValid(Actor)) // Check Actor validity
			{
				AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
				if(UnitBase && UnitBase->UnitToChase && !UnitBase->UseProjectile)
				{
					// Capture necessary data for the Game Thread Task
					// Capture Entity Handles for fragment access on Game Thread
				FMassEntityHandle AttackerEntity = Entity;
					
				const FMassAITargetFragment* TargetFrag = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Entity);
				if (!TargetFrag || !TargetFrag->bHasValidTarget || !TargetFrag->TargetEntity.IsSet()) return;
					
				const FMassEntityHandle TargetEntity = TargetFrag->TargetEntity;
		
					// Check if target entity is valid before dispatching
				if (!TargetEntity.IsSet() || !EntityManager.IsEntityValid(TargetEntity))
				{
					UE_LOG(LogTemp, Warning, TEXT("UnitMeeleAttack: Target Actor %s does not have a valid Mass Entity."), *UnitBase->UnitToChase->GetName());
					continue; // Skip this attack if target isn't a valid Mass entity
				}

                TWeakObjectPtr<AUnitBase> WeakAttacker(UnitBase);
                TWeakObjectPtr<AUnitBase> WeakTarget(UnitBase->UnitToChase); // Capture target as weak ptr too

                AsyncTask(ENamedThreads::GameThread, [WeakAttacker, WeakTarget]()
                {
                    AUnitBase* StrongAttacker = WeakAttacker.Get();
                    AUnitBase* StrongTarget = WeakTarget.Get();
                	
                			// Re-acquire EntityManager on the Game Thread if needed, or ensure 'this' capture is valid
				   if (!EntitySubsystem) return; // Need subsystem on game thread too
					FMassEntityManager& GTEntityManager = EntitySubsystem->GetMutableEntityManager(); // GT = Game Thread

                    // Ensure both attacker and target are still valid when the task runs
                    if (StrongAttacker && StrongTarget)
                    {
            // --- Access Fragments on Game Thread ---
                        // Use non-const pointers as we will modify the target's stats
                        const FMassCombatStatsFragment* AttackerStats = GTEntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(AttackerEntity);
                        FMassCombatStatsFragment* TargetStats = GTEntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(TargetEntity); // Mutable for health/shield updates

                        // Check if fragments were successfully retrieved
                        if (!AttackerStats || !TargetStats)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("UnitMeeleAttack (GameThread): Failed to get CombatStatsFragment for Attacker (%d) or Target (%d)."), AttackerStats != nullptr, TargetStats != nullptr);
                            return; // Cannot proceed without stats
                        }
                        // Apply Damage
                        float NewDamage = StrongAttacker->Attributes->GetAttackDamage(); // Start with base damage
                        if(StrongAttacker->IsDoingMagicDamage)
                        {
                            NewDamage -= StrongTarget->Attributes->GetMagicResistance();
                        }
                        else
                        {
                            NewDamage -= StrongTarget->Attributes->GetArmor();
                        }
                        NewDamage = FMath::Max(0.0f, NewDamage); // Ensure damage is not negative

                        if(StrongTarget->Attributes->GetShield() <= 0)
                        {
                            StrongTarget->SetHealth_Implementation(StrongTarget->Attributes->GetHealth() - NewDamage);
                        }
                        else
                        {
                            // Assuming shield absorbs full base damage before resistances/armor
                            StrongTarget->SetShield_Implementation(StrongTarget->Attributes->GetShield() - StrongAttacker->Attributes->GetAttackDamage());
                        }

                        StrongAttacker->LevelData.Experience++;

                        // Update Health Bar (UI must be on Game Thread)
                        if(StrongAttacker->HealthWidgetComp)
                        {
                            if (UUnitBaseHealthBar* HealthBarWidget = Cast<UUnitBaseHealthBar>(StrongAttacker->HealthWidgetComp->GetUserWidgetObject()))
                            {
                                HealthBarWidget->UpdateWidget(); // Make sure UpdateWidget is safe to call
                            }
                        }

                        // Trigger RPC (Must be on Game Thread)
                        StrongAttacker->ServerMeeleImpactEvent(); // This will call the Multicast

                        // Activate Target's Ability (Must be on Game Thread)
                        StrongTarget->ActivateAbilityByInputID(StrongTarget->DefensiveAbilityID, StrongTarget->DefensiveAbilities);

                        // Spawn Effects (Often involves spawning actors/components, needs Game Thread)
                        StrongTarget->FireEffects(StrongAttacker->MeleeImpactVFX, StrongAttacker->MeleeImpactSound, StrongAttacker->ScaleImpactVFX, StrongAttacker->ScaleImpactSound, StrongAttacker->MeeleImpactVFXDelay, StrongAttacker->MeleeImpactSoundDelay);

                        // Modify Target's Chase List (Modifying Actor Arrays needs Game Thread)
                        if (!StrongTarget->UnitsToChase.Contains(StrongAttacker))
                        {
                            StrongTarget->UnitsToChase.Emplace(StrongAttacker); // Use Emplace for potential efficiency
                            StrongTarget->SetNextUnitToChase(); // Assuming this modifies state safely
                        }

                        // Change Target's State (Modifying Actor state needs Game Thread)
                        if( StrongTarget->GetUnitState() != UnitData::Run &&
                            StrongTarget->GetUnitState() != UnitData::Attack &&
                            StrongTarget->GetUnitState() != UnitData::Casting &&
                            StrongTarget->GetUnitState() != UnitData::Rooted &&
                            StrongTarget->GetUnitState() != UnitData::Pause)
                        {
                            StrongTarget->UnitControlTimer = 0.f;
                            StrongTarget->SetUnitState( UnitData::IsAttacked );
                        }
                        else if(StrongTarget->GetUnitState() == UnitData::Casting)
                        {
                            StrongTarget->UnitControlTimer -= StrongTarget->ReduceCastTime; // Ensure thread safety if timers are updated elsewhere
                        }
                        else if(StrongTarget->GetUnitState() == UnitData::Rooted)
                        {
                            StrongTarget->UnitControlTimer -= StrongTarget->ReduceRootedTime; // Ensure thread safety
                        }
                        // --- End of Game Thread Logic ---
                    }
                });
				}
			}
		}
	}
}
*/

void UUnitStateProcessor::UnitRangedAttack(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
	if (!EntitySubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("OnUnitReachedDestination called but EntitySubsystem is null!"));
		return;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	for (const FMassEntityHandle& Entity : Entities)
	{
		if (!EntityManager.IsEntityValid(Entity)) // Double check entity validity
		{
			continue;
		}
        
		FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
		if (ActorFragPtr)
		{
			AActor* Actor = ActorFragPtr->GetMutable(); // Use Get() for const access if possible, GetMutable() otherwise
			if (IsValid(Actor)) // Check Actor validity
			{
				AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
				if(UnitBase && UnitBase->UnitToChase && UnitBase->UseProjectile)
				{
						UnitBase->ServerStartAttackEvent_Implementation();
						UnitBase->SetUnitState(UnitData::Attack);
						UnitBase->ActivateAbilityByInputID(UnitBase->AttackAbilityID, UnitBase->AttackAbilities);
						UnitBase->ActivateAbilityByInputID(UnitBase->ThrowAbilityID, UnitBase->ThrowAbilities);
						if(UnitBase->Attributes->GetRange() >= 600.f) UnitBase->ActivateAbilityByInputID(UnitBase->OffensiveAbilityID, UnitBase->OffensiveAbilities);
			
						UnitBase->SpawnProjectile(UnitBase->UnitToChase, UnitBase);
				}
			}
		}
	}
}

void UUnitStateProcessor::SetUnitToChase(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    if (!EntitySubsystem) return; // Early exit if subsystem is invalid

    // Use GetMutableEntityManager to allow getting mutable fragment data/actor pointers
    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

    for (const FMassEntityHandle& DetectorEntity : Entities)
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
            // UE_LOG(LogTemp, Log, TEXT("Unit %s target updated."), *DetectorUnitBase->GetName());
        }
    }
}