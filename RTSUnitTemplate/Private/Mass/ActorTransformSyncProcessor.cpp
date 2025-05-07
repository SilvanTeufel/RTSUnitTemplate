// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/ActorTransformSyncProcessor.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"          // For FTransformFragment
#include "MassRepresentationFragments.h"  // For FMassRepresentationLODFragment
#include "MassRepresentationTypes.h"      // For FMassRepresentationLODParams, EMassLOD
#include "MassActorSubsystem.h"           // Potentially useful, good to know about
#include "Characters/Unit/UnitBase.h"
#include "GameFramework/Actor.h"

UActorTransformSyncProcessor::UActorTransformSyncProcessor()
    : RepresentationSubsystem(nullptr) // Initialize pointer here
{
    //ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
    //ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Editor);
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
    // Optional ExecutionOrder settings...
}
/*
void UActorTransformSyncProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    // You don't necessarily need the RepresentationSubsystem pointer directly
    // if you use FMassActorFragment for the actor link.
}
*/
void UActorTransformSyncProcessor::ConfigureQueries()
{
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
    // Use FMassActorFragment to get the linked actor
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
    // Still need LOD info to know if the actor should be visible/updated
    EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);

    EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);     // Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);   // ...OR if this tag is present.
    EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
    EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);

    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);
    
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);
    //EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly); // <-- HINZUGEFÜGT: Geschwindigkeit lesen für Rotation
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

struct FActorTransformUpdatePayload
{
    // Use TWeakObjectPtr for safety, as the Actor could be destroyed
    // between the worker thread collecting it and the game thread processing it.
    TWeakObjectPtr<AActor> ActorPtr;
    FTransform NewTransform;

    FActorTransformUpdatePayload(AActor* InActor, const FTransform& InTransform)
        : ActorPtr(InActor), NewTransform(InTransform)
    {}
};

void UActorTransformSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // --- Throttling calculations remain the same ---
    HighFPSThreshold = FMath::Max(HighFPSThreshold, LowFPSThreshold + 1.0f);
    MinTickInterval = FMath::Max(0.001f, MinTickInterval);
    MaxTickInterval = FMath::Max(MinTickInterval, MaxTickInterval);
    const float ActualDeltaTime = Context.GetDeltaTimeSeconds();
    if (ActualDeltaTime <= 0.0f)
    {
        return;
    }
    const float LowDeltaTimeThreshold = 1.0f / HighFPSThreshold;
    const float HighDeltaTimeThreshold = 1.0f / LowFPSThreshold;
    const FVector2D InputDeltaTimeRange(LowDeltaTimeThreshold, HighDeltaTimeThreshold);
    const FVector2D OutputIntervalRange(MinTickInterval, MaxTickInterval);
    const float CurrentDynamicTickInterval = FMath::GetMappedRangeValueClamped(InputDeltaTimeRange, OutputIntervalRange, ActualDeltaTime);

    AccumulatedTimeA += ActualDeltaTime;
    if (AccumulatedTimeA < CurrentDynamicTickInterval)
    {
        return;
    }
    AccumulatedTimeA = 0.0f;

    AccumulatedTimeB += ActualDeltaTime;
    bool bResetVisibilityTimer = false;

    // --- Create a list to store updates needed for the Game Thread ---
    TArray<FActorTransformUpdatePayload> PendingActorUpdates;
    // Reserve some memory based on expected number of updates per tick if known
    // PendingActorUpdates.Reserve(ExpectedUpdates);
    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture PendingActorUpdates by reference to fill it
        [this, ActualDeltaTime, &bResetVisibilityTimer, &PendingActorUpdates](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        const TConstArrayView<FTransformFragment> TransformFragments = ChunkContext.GetFragmentView<FTransformFragment>();
        // We only need ReadOnly access now within the lambda as we don't modify the fragment directly here
        TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        const float MinMovementDistanceSq = FMath::Square(MinMovementDistanceForRotation);

        //UE_LOG(LogTemp, Log, TEXT("UActorTransformSyncProcessor::Execute started NumEntities: %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {
            AActor* Actor = ActorFragments[i].GetMutable();
            AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            // Get the actor pointer, but don't modify it directly here
            //AActor* Actor = ActorFragments[i].Get(); // Use Get() for const view
            //AUnitBase* UnitBase = Cast<AUnitBase>(Actor);

            // It's still important to check IsValid here before accessing UnitBase members
            if (!IsValid(UnitBase)) continue;

            // --- Visibility Check ---
            //bool UnitIsVisible = UnitBase->IsOnViewport && (!UnitBase->EnableFog || UnitBase->IsVisibleEnemy || UnitBase->IsMyTeam);
            if (!UnitBase->IsNetVisible() && AccumulatedTimeB <= 0.5f)
            {
                continue; // Skip update calculation for this non-visible, recently updated actor
            }
            if (UnitBase->IsNetVisible() || AccumulatedTimeB > 0.5f)
            {
                 bResetVisibilityTimer = true; // Mark that we potentially need to reset the timer
            }

            // --- Calculate Target Transform ---
            const FTransform& MassTransform = TransformFragments[i].GetTransform();
            // Get current state directly needed for calculation (might still need Game Thread access protection
            // if these getters aren't thread-safe, but often reads are safer than writes).
            // For maximum safety, consider caching these on the game thread previously or using Mass state.
            const FVector CurrentActorLocation = Actor->GetActorLocation(); // Potential thread-safety issue depending on engine version/implementation
            const FQuat CurrentActorRotation = Actor->GetActorQuat();       // Potential thread-safety issue

            FVector FinalLocation = MassTransform.GetLocation();

            // Z LOCATION ADAPTATION
            float CapsuleHalfHeight = UnitBase->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            
            FCollisionQueryParams Params;
            Params.AddIgnoredActor(UnitBase); // always ignore self

            FCollisionObjectQueryParams ObjectParams;
            ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic); // ECC_GameTraceChannel1 // ECC_WorldStatic

            // 2) In your sync processor, trace:
            FVector TraceStart = FVector(FinalLocation.X, FinalLocation.Y, CurrentActorLocation.Z + 500.0f);
            FVector TraceEnd   = TraceStart - FVector(0,0,1000.0f);

            FHitResult Hit;
            if (GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectParams, Params))
            {

                AActor* HitActor = Hit.GetActor();
                float DeltaZ = Hit.ImpactPoint.Z - CurrentActorLocation.Z;
                if (IsValid(HitActor) && !HitActor->IsA(AUnitBase::StaticClass()) && DeltaZ <= CapsuleHalfHeight) // && DeltaZ <= CapsuleHalfHeight
                {
                    FinalLocation.Z = Hit.ImpactPoint.Z + CapsuleHalfHeight;
                }else
                {
                    //FinalLocation.Z = CurrentActorLocation.Z;
                }
            }
            else
            {
                //FinalLocation.Z = CurrentActorLocation.Z;
            }

            
            //FinalLocation.Z = CurrentActorLocation.Z; // Keep original Z

            FQuat TargetRotation = CurrentActorRotation;
            const FVector MoveDirection = FinalLocation - CurrentActorLocation;

            if (MoveDirection.SizeSquared() > MinMovementDistanceSq)
            {
                FVector MoveDirection2D = MoveDirection;
                MoveDirection2D.Z = 0.0f;
                if (MoveDirection2D.Normalize())
                {
                    TargetRotation = MoveDirection2D.ToOrientationQuat();
                }
            }

            const float RotationInterpolationSpeed = 5.0f;
            const FQuat SmoothedRotation = FQuat::Slerp(CurrentActorRotation, TargetRotation, FMath::Clamp(ActualDeltaTime * RotationInterpolationSpeed, 0.0f, 1.0f));

            FTransform FinalActorTransform(SmoothedRotation, FinalLocation, MassTransform.GetScale3D());

            // --- Check if update is needed and add to pending list ---
            // GetActorTransform() might also not be thread-safe. Compare against MassTransform or previous state if possible.
            // For simplicity here, we assume it's okay or accept minor inaccuracy.
            if (!Actor->GetActorTransform().Equals(FinalActorTransform, 0.1f))
            {
                // Instead of calling SetActorTransform, add data to the list
                PendingActorUpdates.Emplace(Actor, FinalActorTransform);
            }
        }
    }); // End ForEachEntityChunk


    // --- Reset Visibility Timer ---
    if (bResetVisibilityTimer)
    {
        AccumulatedTimeB = 0.0f;
    }

    // --- Schedule Game Thread Task ---
    if (!PendingActorUpdates.IsEmpty())
    {
        // Capture the list of updates by value (moving it) into the lambda
        AsyncTask(ENamedThreads::GameThread, [Updates = MoveTemp(PendingActorUpdates)]()
        {
            for (const FActorTransformUpdatePayload& Update : Updates)
            {
                // Check if the actor is still valid before applying the transform
                if (AActor* Actor = Update.ActorPtr.Get()) // Get the raw pointer from TWeakObjectPtr
                {
                    // This part now runs safely on the Game Thread
                    Actor->SetActorTransform(Update.NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
                }
            }
        });
    }
}