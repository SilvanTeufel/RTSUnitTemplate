// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/ActorTransformSyncProcessor.h"

#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassCommonFragments.h"          // For FTransformFragment
#include "MassRepresentationFragments.h"  // For FMassRepresentationLODFragment
#include "MassRepresentationTypes.h"      // For FMassRepresentationLODParams, EMassLOD
#include "MassActorSubsystem.h"           // Potentially useful, good to know about
#include "Characters/Unit/UnitBase.h"
#include "GameFramework/Actor.h"
#include "Async/Async.h"
#include "NavigationSystem.h"

UActorTransformSyncProcessor::UActorTransformSyncProcessor()
    : RepresentationSubsystem(nullptr) // Initialize pointer here
{
    //ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
    //ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Editor);
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
    // Optional ExecutionOrder settings...
}

void UActorTransformSyncProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
    
    // Use FMassActorFragment to get the linked actor
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);

    // Still need LOD info to know if the actor should be visible/updated
    EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);

    EntityQuery.AddTagRequirement<FMassStateRunTag>(EMassFragmentPresence::Any);     // Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::Any);   // ...OR if this tag is present.
    EntityQuery.AddTagRequirement<FMassStatePatrolRandomTag>(EMassFragmentPresence::Any); 
    EntityQuery.AddTagRequirement<FMassStatePatrolTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateIdleTag>(EMassFragmentPresence::Any);
    
    EntityQuery.AddTagRequirement<FMassStateGoToBaseTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToResourceExtractionTag>(EMassFragmentPresence::Any);
    EntityQuery.AddTagRequirement<FMassStateGoToBuildTag>(EMassFragmentPresence::Any);

    //EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);
    //EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
    
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::None);     // Dont Execute if this tag is present...
    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateIsAttackedTag>(EMassFragmentPresence::None);

    EntityQuery.RegisterWithProcessor(*this); // Important!

}


void UActorTransformSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    // --- Dynamic Tick Rate Calculation ---
    HighFPSThreshold = FMath::Max(HighFPSThreshold, LowFPSThreshold + 1.0f);
    MinTickInterval = FMath::Max(0.001f, MinTickInterval);
    MaxTickInterval = FMath::Max(MinTickInterval, MaxTickInterval);

    const float ActualDeltaTime = Context.GetDeltaTimeSeconds();
    if (ActualDeltaTime <= 0.0f) return;

    const float LowDeltaTimeThreshold = 1.0f / HighFPSThreshold;
    const float HighDeltaTimeThreshold = 1.0f / LowFPSThreshold;

    const FVector2D InputDeltaTimeRange(LowDeltaTimeThreshold, HighDeltaTimeThreshold);
    const FVector2D OutputIntervalRange(MinTickInterval, MaxTickInterval);
    const float CurrentDynamicTickInterval = FMath::GetMappedRangeValueClamped(InputDeltaTimeRange, OutputIntervalRange, ActualDeltaTime);

    AccumulatedTimeA += ActualDeltaTime;
    if (AccumulatedTimeA < CurrentDynamicTickInterval) return;
    AccumulatedTimeA = 0.0f;

    // This array will hold all the necessary updates to be performed on the game thread.
    TArray<FActorTransformUpdatePayload> PendingActorUpdates;

    EntityQuery.ForEachEntityChunk(Context,
        [this, ActualDeltaTime, &PendingActorUpdates](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        TArrayView<FTransformFragment> TransformFragments = ChunkContext.GetMutableFragmentView<FTransformFragment>();
        TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
        const TConstArrayView<FMassVelocityFragment> VelocityList = ChunkContext.GetFragmentView<FMassVelocityFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            AActor* Actor = ActorFragments[i].GetMutable();
            AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            if (!IsValid(UnitBase)) continue;

            // The Mass simulation continues even for off-screen entities. 
            // We trust the simulation and just sync the result, regardless of visibility.
            // The IsNetVisible() check and timer logic have been removed as they were causing the issue.
            
            FTransform& MassTransform = TransformFragments[i].GetMutableTransform();
            FVector FinalLocation = MassTransform.GetLocation();
            float HeightOffset;

            // Determine height offset based on unit type
            if (UnitBase->bUseSkeletalMovement)
            {
                MassTransform.SetScale3D(UnitBase->GetActorScale3D());
                HeightOffset = UnitBase->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            }
            else
            {
                const FTransform& ISMTransform = UnitBase->ISMComponent->GetComponentTransform();
                MassTransform.SetScale3D(ISMTransform.GetScale3D());
                HeightOffset = ISMTransform.GetScale3D().Z / 2.0f;
            }

            // --- Ground/Height Adjustment Logic ---
            FHitResult Hit;
            FCollisionQueryParams Params;
            Params.AddIgnoredActor(UnitBase);
            FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);

            // Use a wider search area for the trace start to be more robust
            FVector TraceStart = FVector(FinalLocation.X, FinalLocation.Y, FinalLocation.Z + 1000.0f);
            FVector TraceEnd = FVector(FinalLocation.X, FinalLocation.Y, FinalLocation.Z - 2000.0f);


       
            FVector CurrentActorLocation = UnitBase->GetActorLocation();

            if (!UnitBase->bUseSkeletalMovement)
            {
                FTransform InstanceTransform;
                UnitBase->ISMComponent->GetInstanceTransform(UnitBase->InstanceIndex, InstanceTransform, true);
                CurrentActorLocation = InstanceTransform.GetLocation();
            }
            
             if (GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectParams, Params))
             {
                 AActor* HitActor = Hit.GetActor();
                    
                 float DeltaZ = Hit.ImpactPoint.Z - CurrentActorLocation.Z;
                 if (IsValid(HitActor) && !HitActor->IsA(AUnitBase::StaticClass()) && DeltaZ <= HeightOffset && !CharList[i].bIsFlying)
                 {
                     CharList[i].LastGroundLocation = Hit.ImpactPoint.Z;

                     FinalLocation.Z = Hit.ImpactPoint.Z + HeightOffset;
                 }else if (!CharList[i].bIsFlying)
                 {
                     FinalLocation.Z = CharList[i].LastGroundLocation + HeightOffset;
                 }else if (IsValid(HitActor) && CharList[i].bIsFlying)
                 {
                     //if (!UnitBase->bUseSkeletalMovement)
                     //{
                     float CurrentZ = CurrentActorLocation.Z; //CharList[i].LastGroundLocation + CharList[i].FlyHeight;
                     float TargetZ = Hit.ImpactPoint.Z + CharList[i].FlyHeight;
                     FinalLocation.Z = FMath::FInterpConstantTo(CurrentZ, TargetZ, ActualDeltaTime, VerticalInterpSpeed*100.f);
         
                     CharList[i].LastGroundLocation = Hit.ImpactPoint.Z;
                 }
               }else
               {
                   if (CharList[i].bIsFlying)
                        FinalLocation.Z = CharList[i].LastGroundLocation + CharList[i].FlyHeight;
                   else
                       FinalLocation.Z = CharList[i].LastGroundLocation + HeightOffset;
               }

            // --- Rotation Logic ---
            const FVector& CurrentVelocity = VelocityList[i].Value;
            FQuat DesiredQuat = MassTransform.GetRotation(); // Default to current rotation

            if (CharList[i].RotatesToMovement && !CurrentVelocity.IsNearlyZero(0.1f))
            {
                FVector LookAtDir = CurrentVelocity;
                LookAtDir.Z = 0.f; // Flatten to XY plane for ground units
                if (LookAtDir.Normalize())
                {
                    DesiredQuat = LookAtDir.ToOrientationQuat();
                }
            }
            
            // --- Interpolation and Transform Update ---
            const float RotationSpeedRad = FMath::DegreesToRadians(StatsList[i].RotationSpeed * CharList[i].RotationSpeed);
            FQuat NewQuat = MassTransform.GetRotation();

            if (CharList[i].RotatesToMovement)
            {
                if (RotationSpeedRad > KINDA_SMALL_NUMBER)
                {
                    NewQuat = FMath::QInterpConstantTo(MassTransform.GetRotation(), DesiredQuat, ActualDeltaTime, RotationSpeedRad);
                }
                else
                {
                    NewQuat = DesiredQuat; // Instant rotation
                }
            }
            
            MassTransform.SetRotation(NewQuat);
            MassTransform.SetLocation(FinalLocation);
            
            // The FinalActorTransform is now the same as the MassTransform we just calculated
            const FTransform& FinalActorTransform = MassTransform;

            // Only schedule an update if the transform has actually changed to avoid unnecessary work.
            if (!Actor->GetActorTransform().Equals(FinalActorTransform, 0.025f))
            {
                PendingActorUpdates.Emplace(Actor, FinalActorTransform, UnitBase->bUseSkeletalMovement, UnitBase->InstanceIndex);
            }
        }
    });

    // --- Perform all updates on the Game Thread ---
    if (!PendingActorUpdates.IsEmpty())
    {
        // Use MoveTemp to efficiently transfer ownership of the array to the async task
        AsyncTask(ENamedThreads::GameThread, [Updates = MoveTemp(PendingActorUpdates)]()
        {
            for (const FActorTransformUpdatePayload& Update : Updates)
            {
                if (AActor* Actor = Update.ActorPtr.Get())
                {
                    if (Update.bUseSkeletal)
                    {
                        Actor->SetActorTransform(Update.NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
                    }
                    else if (AUnitBase* Unit = Cast<AUnitBase>(Actor))
                    {
                        // For ISM units, we call the multicast function to update the specific instance
                        Unit->Multicast_UpdateISMInstanceTransform(Update.InstanceIndex, Update.NewTransform);
                    }
                }
            }
        });
    }
}

/*
void UActorTransformSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    HighFPSThreshold = FMath::Max(HighFPSThreshold, LowFPSThreshold + 1.0f);
    MinTickInterval = FMath::Max(0.001f, MinTickInterval);
    MaxTickInterval = FMath::Max(MinTickInterval, MaxTickInterval);

    const float ActualDeltaTime = Context.GetDeltaTimeSeconds();
    if (ActualDeltaTime <= 0.0f) return;

    const float LowDeltaTimeThreshold = 1.0f / HighFPSThreshold;
    const float HighDeltaTimeThreshold = 1.0f / LowFPSThreshold;

    const FVector2D InputDeltaTimeRange(LowDeltaTimeThreshold, HighDeltaTimeThreshold);
    const FVector2D OutputIntervalRange(MinTickInterval, MaxTickInterval);
    const float CurrentDynamicTickInterval = FMath::GetMappedRangeValueClamped(InputDeltaTimeRange, OutputIntervalRange, ActualDeltaTime);

    AccumulatedTimeA += ActualDeltaTime;
    if (AccumulatedTimeA < CurrentDynamicTickInterval) return;
    AccumulatedTimeA = 0.0f;

    AccumulatedTimeB += ActualDeltaTime;
    bool bResetVisibilityTimer = false;

    TArray<FActorTransformUpdatePayload> PendingActorUpdates;

    EntityQuery.ForEachEntityChunk(Context,
        [this, ActualDeltaTime, &bResetVisibilityTimer, &PendingActorUpdates](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        TArrayView<FTransformFragment> TransformFragments = ChunkContext.GetMutableFragmentView<FTransformFragment>();
        TArrayView<FMassActorFragment> ActorFragments = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
            const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
             const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = ChunkContext.GetFragmentView<FMassMoveTargetFragment>();
            TArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();

            // --- Get the Velocity fragment view ---
            const TConstArrayView<FMassVelocityFragment> VelocityList = ChunkContext.GetFragmentView<FMassVelocityFragment>();

            const float MinMovementDistanceSq = FMath::Square(MinMovementDistanceForRotation);

            //UE_LOG(LogTemp, Log, TEXT("UActorTransformSyncProcessor NumEntities: %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {

            const FMassMoveTargetFragment MoveFrag = MoveTargetList[i];
            AActor* Actor = ActorFragments[i].GetMutable();
            AUnitBase* UnitBase = Cast<AUnitBase>(Actor);
            if (!IsValid(UnitBase)) continue;

            if (!UnitBase->IsNetVisible() && AccumulatedTimeB <= 0.5f) continue;
            if (UnitBase->IsNetVisible() || AccumulatedTimeB > 0.5f) bResetVisibilityTimer = true;
            
            FTransform& MassTransform = TransformFragments[i].GetMutableTransform();
            
            
            FVector CurrentActorLocation = Actor->GetActorLocation();
            
            float HeightOffset;

            if (UnitBase->bUseSkeletalMovement)
            {
                MassTransform.SetScale3D(UnitBase->GetActorScale3D());

                HeightOffset = UnitBase->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            }else
            {
               
                const FTransform& ActorTransform = UnitBase->ISMComponent->GetComponentTransform(); //UnitBase->ISMComponent->GetComponentTransform();
                MassTransform.SetScale3D(ActorTransform.GetScale3D());
                
                FVector InstanceScale = ActorTransform.GetScale3D();
                HeightOffset = InstanceScale.Z/2;
            }

           // FVector FinalLocation = SimulatedLocation; 
            FVector FinalLocation = MassTransform.GetLocation();

        
            FCollisionQueryParams Params;
            Params.AddIgnoredActor(UnitBase);

            FCollisionObjectQueryParams ObjectParams;
            ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

            FVector TraceStart = FVector(FinalLocation.X, FinalLocation.Y, CurrentActorLocation.Z + 500.0f);
            FVector TraceEnd = TraceStart - FVector(0, 0, 2000.0f);


            FHitResult Hit;
            
             if (GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectParams, Params))
             {
                 AActor* HitActor = Hit.GetActor();
                    
                 float DeltaZ = Hit.ImpactPoint.Z - CurrentActorLocation.Z;
                 if (IsValid(HitActor) && !HitActor->IsA(AUnitBase::StaticClass()) && DeltaZ <= HeightOffset && !CharList[i].bIsFlying)
                 {
                     CharList[i].LastGroundLocation = Hit.ImpactPoint.Z;

                     FinalLocation.Z = Hit.ImpactPoint.Z + HeightOffset;
                 }else if (!CharList[i].bIsFlying)
                 {
                     FinalLocation.Z = CharList[i].LastGroundLocation + HeightOffset;
                 }else if (IsValid(HitActor) && CharList[i].bIsFlying)
                 {
                     //if (!UnitBase->bUseSkeletalMovement)
                     //{
                     float CurrentZ = CharList[i].LastGroundLocation + CharList[i].FlyHeight;
                     float TargetZ = Hit.ImpactPoint.Z + CharList[i].FlyHeight;
                     FinalLocation.Z = FMath::FInterpConstantTo(CurrentZ, TargetZ, ActualDeltaTime, VerticalInterpSpeed);
         
                     CharList[i].LastGroundLocation = Hit.ImpactPoint.Z;
                 }
               }else
               {
                   UE_LOG(LogTemp, Warning, TEXT("TRUE!"));
                   if (CharList[i].bIsFlying)
                        FinalLocation.Z = CharList[i].LastGroundLocation + CharList[i].FlyHeight;
                   else
                       FinalLocation.Z = CharList[i].LastGroundLocation + HeightOffset;
               }
            
            // --- VVVV ROTATION LOGIC CHANGE VVVV ---

           // Get the current velocity for this entity.
           const FVector& CurrentVelocity = VelocityList[i].Value;
           FQuat DesiredQuat = MassTransform.GetRotation(); // Default to current rotation if not moving

           // Only update rotation if the character is actually moving.
           if (!CurrentVelocity.IsNearlyZero(0.1f))
           {
               FVector LookAtDir = CurrentVelocity;
               LookAtDir.Z = 0.f; // LookAt in XY plane
                    
               // Check if the direction is valid before creating a quaternion from it
               if(LookAtDir.Normalize())
               {
                   DesiredQuat = LookAtDir.ToOrientationQuat();
               }
           }
            
            // --- Interpolation ---
            const float RotationSpeedDeg = StatsList[i].RotationSpeed * CharList[i].RotationSpeed; //15.f; // Consider renaming Stat or clarifying multiplier
            FTransform FinalActorTransform;
            
            if (CharList[i].RotatesToMovement)
            {
                const FQuat CurrentQuat = MassTransform.GetRotation();
            
                FQuat NewQuat;
                if (RotationSpeedDeg > KINDA_SMALL_NUMBER*10.f)
                {
                    NewQuat = FMath::QInterpConstantTo(CurrentQuat, DesiredQuat, ActualDeltaTime, FMath::DegreesToRadians(RotationSpeedDeg));
                }
                else // Instant rotation if speed is zero/negligible
                {
                    NewQuat = DesiredQuat;
                }

                FinalActorTransform  = FTransform (NewQuat, FinalLocation,  MassTransform.GetScale3D()); // MassTransform.GetScale3D()
            
                  MassTransform.SetRotation(FinalActorTransform.GetRotation());
                  MassTransform.SetLocation(FinalLocation);
            }else
            {
                FinalActorTransform  = FTransform (FQuat(FRotator::ZeroRotator), FinalLocation,  MassTransform.GetScale3D()); // MassTransform.GetScale3D()
                 MassTransform.SetRotation(FQuat(FRotator::ZeroRotator));
                 MassTransform.SetLocation(FinalLocation);
            }
            
            if (!Actor->GetActorTransform().Equals(FinalActorTransform, 0.1f))
            {
                PendingActorUpdates.Emplace(Actor, FinalActorTransform, UnitBase->bUseSkeletalMovement, UnitBase->InstanceIndex);
            }
        }
    });

    if (bResetVisibilityTimer)
    {
        AccumulatedTimeB = 0.0f;
    }

    if (!PendingActorUpdates.IsEmpty())
    {
        AsyncTask(ENamedThreads::GameThread, [Updates = MoveTemp(PendingActorUpdates)]()
        {
            for (const FActorTransformUpdatePayload& Update : Updates)
            {
                if (AActor* Actor = Update.ActorPtr.Get())
                {
                    if (Update.bUseSkeletal)
                    {
                        Actor->SetActorTransform(Update.NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
                    }
                    else
                    {
                        if (AUnitBase* Unit = Cast<AUnitBase>(Actor))
                        {
                            Unit->Multicast_UpdateISMInstanceTransform(Update.InstanceIndex, Update.NewTransform);
                        }
                    }
                }
            }
        });
    }
}
*/