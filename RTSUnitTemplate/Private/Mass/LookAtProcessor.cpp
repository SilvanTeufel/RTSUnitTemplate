// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass//LookAtProcessor.h"

#include "MassActorSubsystem.h"
#include "MassExecutionContext.h"
#include "MassRepresentationFragments.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/UnitMassTag.h"
#include "Steering/MassSteeringFragments.h"
#include "Async/Async.h"

ULookAtProcessor::ULookAtProcessor()
{
	// Sollte laufen, nachdem Ziele gesetzt wurden, aber bevor/während Bewegung/Angriff relevant wird
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Movement; // Oder ::Behavior, je nach Präferenz
	ProcessingPhase = EMassProcessingPhase::PrePhysics; // Rotation vor der Physik berechnen? Oder PostPhysics? Testen!
	bAutoRegisterWithProcessingPhases = true;
	// Normalerweise nicht auf GameThread beschränkt, es sei denn, es gibt spezifische Gründe
	// bRequiresGameThreadExecution = false;
}

void ULookAtProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly);

	EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);


    // ← HERE: steering + avoidance forces as inputs
    //EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadOnly);
    //EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadOnly);

    
	EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::Any);
	EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::Any);
	// Schließe tote oder pausierte Einheiten aus (optional, je nach gewünschtem Verhalten)
	EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
	// EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);

	EntityQuery.RegisterWithProcessor(*this);
}


void ULookAtProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
 
    const float DeltaTime = Context.GetDeltaTimeSeconds();

    // --- Throttling ---
    AccumulatedTimeA += DeltaTime;
    if (constexpr float TickInterval = 0.1f; AccumulatedTimeA < TickInterval)
    {
       return;
    }
    AccumulatedTimeA = 0.0f;

    // --- List for Game Thread Updates ---
    TArray<FActorTransformUpdatePayload> PendingLookAtUpdates;
    // PendingLookAtUpdates.Reserve(ExpectedUpdates); // Optional optimization

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        // Capture PendingLookAtUpdates by reference
        [this, DeltaTime, &EntityManager, &PendingLookAtUpdates](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        // We likely only need read access to fragments within the lambda now
            
        const TConstArrayView<FMassAITargetFragment> TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const TConstArrayView<FMassCombatStatsFragment> StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const TArrayView<FMassActorFragment> ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>(); // ReadOnly access is sufficient
        TArrayView<FTransformFragment> TransformList = ChunkContext.GetMutableFragmentView<FTransformFragment>();

            
        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            if (!TargetFrag.TargetEntity.IsSet())
            {
                continue;
            }

            // --- Determine Target Location ---
            FVector TargetLocation = TargetFrag.LastKnownLocation;
            const FMassEntityHandle TargetEntity = TargetFrag.TargetEntity;
            // EntityManager access is generally thread-safe within Mass
            if (EntityManager.IsEntityValid(TargetEntity))
            {
                if (const FTransformFragment* TargetXform = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetEntity))
                {
                    TargetLocation = TargetXform->GetTransform().GetLocation();
                }
            }

            // --- Get Actor (ReadOnly pointer) ---
            AActor* Actor = ActorList[i].GetMutable(); // Use Get() for const view
            if (!IsValid(Actor)) // Still need IsValid check
            {
                continue;
            }

            AUnitBase* UnitBase = Cast<AUnitBase>(Actor);

             if (!UnitBase) // Still need IsValid check
            {
                continue;
            }
      
            // --- LookAt Calculation ---
            // Reading actor state might have thread-safety implications, but often acceptable.
            FTransform& MassTransform = TransformList[i].GetMutableTransform();
        
            FVector ActorLocation = MassTransform.GetLocation(); // Actor->GetActorLocation();


            float HeightOffset;
            
            if (UnitBase->bUseSkeletalMovement)
            {
                MassTransform.SetScale3D(UnitBase->GetActorScale3D());

                HeightOffset = UnitBase->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            }else
            {
                const FTransform& ActorTransform = UnitBase->ISMComponent->GetComponentTransform();; //UnitBase->ISMComponent->GetComponentTransform();
                MassTransform.SetScale3D(ActorTransform.GetScale3D());
                
                FVector InstanceScale = ActorTransform.GetScale3D();
                HeightOffset = InstanceScale.Z/2;
            }

            
          
             FCollisionQueryParams Params;
             Params.AddIgnoredActor(UnitBase);

             FCollisionObjectQueryParams ObjectParams;
             ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

             FVector TraceStart = FVector(ActorLocation.X, ActorLocation.Y, ActorLocation.Z + 500.0f);
             FVector TraceEnd = TraceStart - FVector(0, 0, 1000.0f);

             FHitResult Hit;
             if (GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectParams, Params))
             {
                 AActor* HitActor = Hit.GetActor();
                 float DeltaZ = Hit.ImpactPoint.Z - ActorLocation.Z;
                 if (IsValid(HitActor) && !HitActor->IsA(AUnitBase::StaticClass()) && DeltaZ <= HeightOffset)
                 {
                     ActorLocation.Z = Hit.ImpactPoint.Z + HeightOffset;
                 }
             }
            

         
            FVector Dir = TargetLocation - ActorLocation;

            Dir.Z = 0.f;  // LookAt in XY plane
            if (!Dir.Normalize())
            {
                continue; // Avoid issues with zero direction
            }
            const FQuat DesiredQuat = Dir.ToOrientationQuat();

            // --- Interpolation ---
            const float RotationSpeedDeg = StatsList[i].RotationSpeed * 15.f; // Consider renaming Stat or clarifying multiplier

            //const FQuat CurrentQuat = Actor->GetActorQuat();
            const FQuat CurrentQuat = MassTransform.GetRotation();
            
            FQuat NewQuat;
            if (RotationSpeedDeg > KINDA_SMALL_NUMBER*10.f)
            {
                NewQuat = FMath::QInterpConstantTo(CurrentQuat, DesiredQuat, DeltaTime, FMath::DegreesToRadians(RotationSpeedDeg));
            }
            else // Instant rotation if speed is zero/negligible
            {
                NewQuat = DesiredQuat;
            }
 

            FTransform FinalActorTransform = FTransform (NewQuat, ActorLocation,  MassTransform.GetScale3D()); // MassTransform.GetScale3D()
            MassTransform.SetRotation(FinalActorTransform.GetRotation());
            MassTransform.SetLocation(FinalActorTransform.GetLocation());
            
            //MassTransform.SetLocation(FinalActorTransform.GetLocation());
            //FTransform FinalTransform(SmoothedRotation, ActorLocation, MassTransform.GetScale3D()); // TransformFrag.GetScale3D()
            // --- Check if update is needed and add to list ---
            if (UnitBase) // && !Actor->GetActorTransform().GetRotation().Equals(SmoothedRotation, 0.01f)
            {
                const bool bUseSkeletal = UnitBase->bUseSkeletalMovement;
                const int32 InstIdx     = UnitBase->InstanceIndex;
                PendingLookAtUpdates.Emplace(Actor, FinalActorTransform, bUseSkeletal, InstIdx);
            }
        }
    }); // End ForEachEntityChunk

    // --- Schedule Game Thread Task ---
    if (!PendingLookAtUpdates.IsEmpty())
    {
        // Capture the list of updates by value (moving it)
        AsyncTask(ENamedThreads::GameThread, [Updates = MoveTemp(PendingLookAtUpdates)]()
        {
            for (const FActorTransformUpdatePayload& Update : Updates)
            {
                // Check if the actor is still valid on the Game Thread
                AActor* Actor = Update.ActorPtr.Get();
                
                if (Update.bUseSkeletal)
               {
                   Actor->SetActorTransform(Update.NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
               }
               else if (AUnitBase* Unit = Cast<AUnitBase>(Actor))
               {
                   Unit->Multicast_UpdateISMInstanceTransform(Update.InstanceIndex, Update.NewTransform);
               }
            }
        });
    }


    
}