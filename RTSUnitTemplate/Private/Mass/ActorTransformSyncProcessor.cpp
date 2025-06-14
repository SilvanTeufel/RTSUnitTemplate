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
    EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);

    // Use FMassActorFragment to get the linked actor
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);

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

    EntityQuery.RegisterWithProcessor(*this); // Important!

}
/*
struct FActorTransformUpdatePayload
{
    TWeakObjectPtr<AActor> ActorPtr;
    FTransform NewTransform;
    int32 InstanceIndex = INDEX_NONE;
    bool bUseSkeletal = true;

    FActorTransformUpdatePayload(AActor* InActor, const FTransform& InTransform, bool bInUseSkeletal, int32 InInstanceIndex = INDEX_NONE)
        : ActorPtr(InActor), NewTransform(InTransform), InstanceIndex(InInstanceIndex), bUseSkeletal(bInUseSkeletal)
    {}
};
*/
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
            const TConstArrayView<FMassAgentCharacteristicsFragment> CharList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();
            
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
            
            FVector FinalLocation = MassTransform.GetLocation();

        
            FCollisionQueryParams Params;
            Params.AddIgnoredActor(UnitBase);

            FCollisionObjectQueryParams ObjectParams;
            ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

            FVector TraceStart = FVector(FinalLocation.X, FinalLocation.Y, CurrentActorLocation.Z + 500.0f);
            FVector TraceEnd = TraceStart - FVector(0, 0, 1000.0f);

            FHitResult Hit;
            if (GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectParams, Params))
            {
                AActor* HitActor = Hit.GetActor();
                float DeltaZ = Hit.ImpactPoint.Z - CurrentActorLocation.Z;
                if (IsValid(HitActor) && !HitActor->IsA(AUnitBase::StaticClass()) && DeltaZ <= HeightOffset)
                {
                    if (!CharList[i].bIsFlying)
                        FinalLocation.Z = Hit.ImpactPoint.Z + HeightOffset;
                    else
                        FinalLocation.Z = Hit.ImpactPoint.Z + CharList[i].FlyHeight;
                }
            }
            
            FVector Dir =  (MoveFrag.Center - FinalLocation)*1000.f;
            Dir.Z = 0.f;  // LookAt in XY plane
            if (!Dir.Normalize())
            {
                continue; // Avoid issues with zero direction
            }
            
            const FQuat DesiredQuat = Dir.ToOrientationQuat();

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
            
            /*
            DrawDebugDirectionalArrow(
            GetWorld(),
            FinalLocation,
            FinalLocation + DesiredQuat.GetForwardVector() * 100.0f,
            20.0f, FColor::Red, false, 1.0f);

            DrawDebugDirectionalArrow(
            GetWorld(),
            FinalLocation,
            FinalLocation + DesiredQuat.GetForwardVector() * 100.0f,
            20.0f, FColor::Green, false, 1.0f);
            */
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