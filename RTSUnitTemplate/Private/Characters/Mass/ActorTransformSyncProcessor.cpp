// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/Mass/ActorTransformSyncProcessor.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"          // For FTransformFragment
#include "MassRepresentationFragments.h"  // For FMassRepresentationLODFragment
#include "MassRepresentationTypes.h"      // For FMassRepresentationLODParams, EMassLOD
#include "MassActorSubsystem.h"           // Potentially useful, good to know about
#include "GameFramework/Actor.h"

UActorTransformSyncProcessor::UActorTransformSyncProcessor()
    : RepresentationSubsystem(nullptr) // Initialize pointer here
{
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    // Optional ExecutionOrder settings...
}

void UActorTransformSyncProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    // You don't necessarily need the RepresentationSubsystem pointer directly
    // if you use FMassActorFragment for the actor link.
}

void UActorTransformSyncProcessor::ConfigureQueries()
{
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    // Use FMassActorFragment to get the linked actor
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
    // Still need LOD info to know if the actor should be visible/updated
    EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.RegisterWithProcessor(*this); // Important!
    // Shared fragment defining LOD distances, etc. Might not be strictly needed
    // in Execute if just checking LOD level, but good practice to include.
    //EntityQuery.AddConstSharedRequirement<FMassRepresentationLODParams>(EMassFragmentPresence::All);

    // Optional but recommended: Add a tag requirement if you only want to sync
    // entities specifically marked as having an Actor representation.
    // The MassActor subsystem usually adds this tag automatically.
     //EntityQuery.AddTagRequirement<FMassHasActorRepresentationTag>(EMassFragmentPresence::All);

    // Process entities that are visible (LOD > Off)
    //EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::All); // Ensure LOD fragment exists
}

void UActorTransformSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    //UE_LOG(LogTemp, Log, TEXT("UActorTransformSyncProcessor::Execute11111"));
    EntityQuery.ForEachEntityChunk(EntityManager, Context, [this](FMassExecutionContext& Context)
    {
        const int32 NumEntities = Context.GetNumEntities();

        // Use GetFragmentView (ReadOnly) for fragments you only read from.
        const TConstArrayView<FTransformFragment> TransformFragments = Context.GetFragmentView<FTransformFragment>();
        const TConstArrayView<FMassRepresentationLODFragment> RepresentationLODFragments = Context.GetFragmentView<FMassRepresentationLODFragment>();

        // Use GetMutableFragmentView (ReadWrite) for FMassActorFragment as needed.
        // Assign to a non-const TArrayView.
        TArrayView<FMassActorFragment> ActorFragments = Context.GetMutableFragmentView<FMassActorFragment>();

        // Optionally get shared LOD Params if needed for more complex logic
        // const FMassRepresentationLODParams& LODParams = Context.GetConstSharedFragment<FMassRepresentationLODParams>();
        //UE_LOG(LogTemp, Log, TEXT("UActorTransformSyncProcessor::Execute22222"));
        //UE_LOG(LogTemp, Log, TEXT("NumEntities: %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {
            // Check if the current LOD is not "Off" (reading from const view is fine)
            //if (RepresentationLODFragments[i].LOD > EMassLOD::Off)
            {
                //UE_LOG(LogTemp, Log, TEXT("FIRST IF"));
                // Get the AActor* from the MUTABLE view. This should resolve the const error.
                AActor* Actor = ActorFragments[i].GetMutable();

                if (IsValid(Actor)) // Good practice to check if the actor is valid
                {
                    //UE_LOG(LogTemp, Log, TEXT("SECOND IF"));
                    // Get the transform (reading from const view is fine)
                    const FTransform& MassTransform = TransformFragments[i].GetTransform();
                    //UE_LOG(LogTemp, Log, TEXT("MassTransform!!!! %s"), *MassTransform.ToString());
                    // Update the actor's transform using the calculated Mass transform
                    Actor->SetActorTransform(MassTransform, false, nullptr, ETeleportType::TeleportPhysics);
                }
            }
        }
    });
}
