// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Signals/UnitStateProcessor.h"

// Source: UnitStateProcessor.cpp
#include "MassSignalSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassEntityManager.h"
#include "MassActorSubsystem.h"
#include "Characters/Unit/UnitBase.h" // Include your AUnitBase header
#include "Mass/Signals/MySignals.h" // Include your signal definition header

// Define the signal name again or include where it's defined
namespace UnitSignals
{
    extern const FName ReachedDestination; // Assuming definition is elsewhere now
    // const FName ReachedDestination = FName("UnitReachedDestination"); // Or define here if local
}

UUnitStateProcessor::UUnitStateProcessor()
{
    // This processor is event-driven, so phase/group isn't critical unless
    // it needs specific ordering relative to other signal handlers.
    // ExecutionFlags = (int32)EProcessorExecutionFlags::All; // Run everywhere
    ProcessingPhase = EMassProcessingPhase::PostPhysics; // Run fairly late
    bAutoRegisterWithProcessingPhases = false; // Don't need ticking execute
}

void UUnitStateProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);

    // Get subsystems
    SignalSubsystem = GetWorld()->GetSubsystem<UMassSignalSubsystem>();
    EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();

    if (SignalSubsystem)
    {
        // Register the handler function to listen for the specific signal name
        // Using AddUObject requires the handler function to be a UFUNCTION,
        // OR use AddRaw/AddSP/AddLambda if the handler is not a UFUNCTION.
        // Let's use AddRaw for simplicity here, assuming OnUnitReachedDestination is just a standard C++ member func.
        ReachedDestinationSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::ReachedDestination)
            .AddUObject(this, &UUnitStateProcessor::OnUnitReachedDestination);
            // .AddRaw(this, &UUnitStateProcessor::OnUnitReachedDestination); // Alternative if not UFUNCTION

         UE_LOG(LogTemp, Log, TEXT("UUnitStateProcessor Initialized and Registered Signal Handler for %s"), *UnitSignals::ReachedDestination.ToString());
    }
     else
     {
          UE_LOG(LogTemp, Error, TEXT("UUnitStateProcessor failed to get SignalSubsystem!"));
     }
}


void UUnitStateProcessor::BeginDestroy()
{
    UE_LOG(LogTemp, Log, TEXT("UUnitStateProcessor BeginDestroy called."));
    
        // --- Perform your cleanup here ---
        // Unregister the signal handler
        if (SignalSubsystem && ReachedDestinationSignalDelegateHandle.IsValid()) // Check if subsystem and handle are valid
        {
            SignalSubsystem->GetSignalDelegateByName(UnitSignals::ReachedDestination).Remove(ReachedDestinationSignalDelegateHandle);
            UE_LOG(LogTemp, Log, TEXT("Unregistered Signal Handler in BeginDestroy."));
        }
    SignalSubsystem = nullptr; // Null out pointers if needed
    EntitySubsystem = nullptr;
    // --- End Cleanup ---

    Super::BeginDestroy();
}

void UUnitStateProcessor::ConfigureQueries()
{
}

void UUnitStateProcessor::OnUnitReachedDestination(FName SignalName, TConstArrayView<FMassEntityHandle> Entities)
{
    if (!EntitySubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("OnUnitReachedDestination called but EntitySubsystem is null!"));
        return;
    }

    FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
    
            
    UE_LOG(LogTemp, Log, TEXT("OnUnitReachedDestination signal received for %d entities."), Entities.Num());

    for (const FMassEntityHandle& Entity : Entities)
    {
        if (!EntityManager.IsEntityValid(Entity)) // Double check entity validity
        {
            continue;
        }

        // Get the ActorFragment to find the corresponding AUnitBase
        // Note: Accessing Actors directly here can be slow if many entities arrive at once.
        // Consider alternative approaches (like setting a Mass state tag) if performance becomes an issue.
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
                    UnitBase->SetUnitState(UnitData::Idle); // Make sure UnitData::Idle is accessible
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
