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
    SignalSubsystem = GetWorld()->GetSubsystem<UMassSignalSubsystem>();
    EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();

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
                if (UnitBase)
                {
                    UE_LOG(LogTemp, Log, TEXT("Setting Unit %s (Entity %d:%d) state to Idle."), *UnitBase->GetName(), Entity.Index, Entity.SerialNumber);
                    // ** Finally call the function **
                    if (SignalName == UnitSignals::Idle)
                        {
                        UnitBase->SetUnitState(UnitData::Idle); // Make sure UnitData::Idle is accessible
                    }
                    else if (SignalName == UnitSignals::Chase)
                    {
                        UnitBase->SetUnitState(UnitData::Chase); // Assuming UnitData::Chase exists
                    }
                    else if (SignalName == UnitSignals::Attack)
                    {
                        UnitBase->SetUnitState(UnitData::Attack); // Assuming UnitData::Attack exists
                    }
                    else if (SignalName == UnitSignals::Dead)
                    {
                        UnitBase->SetUnitState(UnitData::Dead); // Assuming UnitData::Death exists
                    }
                    else if (SignalName == UnitSignals::PatrolIdle)
                    {
                        UnitBase->SetUnitState(UnitData::PatrolIdle);
                    }
                    else if (SignalName == UnitSignals::PatrolRandom)
                    {
                        UnitBase->SetUnitState(UnitData::PatrolRandom);
                    }
                    else if (SignalName == UnitSignals::Pause)
                    {
                        UnitBase->SetUnitState(UnitData::Pause);
                    }
                    else if (SignalName == UnitSignals::Run)
                    {
                        UnitBase->SetUnitState(UnitData::Run);
                    }
                    else if (SignalName == UnitSignals::Casting)
                    {
                        UnitBase->SetUnitState(UnitData::Casting); // Assuming UnitData::Cast exists
                    }
                }
                else
                {
                     UE_LOG(LogTemp, Warning, TEXT("Actor for Entity %d:%d is not an AUnitBase."), Entity.Index, Entity.SerialNumber);
                }
            }
             else
             {
                  UE_LOG(LogTemp, Warning, TEXT("Actor pointer in ActorFragment for Entity %d:%d is invalid."), Entity.Index, Entity.SerialNumber);
             }
        }
        else
        {
             // This could happen if the processor handling the signal doesn't ensure entities have FMassActorFragment.
              UE_LOG(LogTemp, Warning, TEXT("Entity %d:%d received ReachedDestination signal but has no ActorFragment."), Entity.Index, Entity.SerialNumber);
        }
    }
}

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
					// Attack without Projectile
						float NewDamage = UnitBase->Attributes->GetAttackDamage() - UnitBase->UnitToChase->Attributes->GetArmor();
						
						if(UnitBase->IsDoingMagicDamage)
							NewDamage = UnitBase->Attributes->GetAttackDamage() - UnitBase->UnitToChase->Attributes->GetMagicResistance();
							
						if(UnitBase->UnitToChase->Attributes->GetShield() <= 0)
							UnitBase->UnitToChase->SetHealth_Implementation(UnitBase->UnitToChase->Attributes->GetHealth()-NewDamage);
						else
							UnitBase->UnitToChase->SetShield_Implementation(UnitBase->UnitToChase->Attributes->GetShield()-UnitBase->Attributes->GetAttackDamage());

						UnitBase->LevelData.Experience++;

						if(UnitBase->HealthWidgetComp)
							if (UUnitBaseHealthBar* HealthBarWidget = Cast<UUnitBaseHealthBar>(UnitBase->HealthWidgetComp->GetUserWidgetObject()))
							{
								HealthBarWidget->UpdateWidget();
							}
								
						UnitBase->ServerMeeleImpactEvent();
						
						UnitBase->UnitToChase->ActivateAbilityByInputID(UnitBase->UnitToChase->DefensiveAbilityID, UnitBase->UnitToChase->DefensiveAbilities);
						UnitBase->UnitToChase->FireEffects(UnitBase->MeleeImpactVFX, UnitBase->MeleeImpactSound, UnitBase->ScaleImpactVFX, UnitBase->ScaleImpactSound, UnitBase->MeeleImpactVFXDelay, UnitBase->MeleeImpactSoundDelay);
								
						if (!UnitBase->UnitToChase->UnitsToChase.Contains(UnitBase))
						{
							// If not, add UnitBase to the array
							UnitBase->UnitToChase->UnitsToChase.Emplace(UnitBase);
							UnitBase->UnitToChase->SetNextUnitToChase();
						}
							
						if(UnitBase->UnitToChase->GetUnitState() != UnitData::Run &&
							UnitBase->UnitToChase->GetUnitState() != UnitData::Attack &&
							UnitBase->UnitToChase->GetUnitState() != UnitData::Casting &&
							UnitBase->UnitToChase->GetUnitState() != UnitData::Rooted &&
							UnitBase->UnitToChase->GetUnitState() != UnitData::Pause)
						{
							UnitBase->UnitToChase->UnitControlTimer = 0.f;
							UnitBase->UnitToChase->SetUnitState( UnitData::IsAttacked );
						}else if(UnitBase->UnitToChase->GetUnitState() == UnitData::Casting)
						{
							UnitBase->UnitToChase->UnitControlTimer -= UnitBase->UnitToChase->ReduceCastTime;
						}else if(UnitBase->UnitToChase->GetUnitState() == UnitData::Rooted)
						{
							UnitBase->UnitToChase->UnitControlTimer -= UnitBase->UnitToChase->ReduceRootedTime;
						}
				}
			}
		}
	}
}

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

/*
void UUnitStateProcessor::SetUnitToChase(FName SignalName, TArray<FMassEntityHandle>& Entities)
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
				if(UnitBase)
				{
					UnitBase->UnitToChase = 
				}
			}
		}
	}
}
*/
