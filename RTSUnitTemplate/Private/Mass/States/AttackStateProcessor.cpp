#include "Mass/States/AttackStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "MassSignalSubsystem.h" // Für Schadens-Signal

// Fragmente und Tags
#include "MassActorSubsystem.h"   
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassCommonFragments.h" // Für Transform
#include "MassActorSubsystem.h"  
// Für Actor-Cast und Projektil-Spawn (Beispiel)
#include "MassArchetypeTypes.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/Signals/MySignals.h"


UAttackStateProcessor::UAttackStateProcessor()
{
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
}

void UAttackStateProcessor::Initialize(UObject& Owner)
{
    Super::Initialize(Owner);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
}


void UAttackStateProcessor::ConfigureQueries()
{
    EntityQuery.AddTagRequirement<FMassStateAttackTag>(EMassFragmentPresence::All);

    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);

    EntityQuery.AddTagRequirement<FMassStatePauseTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateChaseTag>(EMassFragmentPresence::None); // Exclude Chase too
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None); // Already excluded by other logic, but explicit

    EntityQuery.RegisterWithProcessor(*this);
}

void UAttackStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    //UE_LOG(LogTemp, Log, TEXT("UAttackStateProcessor::Execute!"));
    UWorld* World = Context.GetWorld(); // World für MoveTarget holen

    // Stelle sicher, dass das Signal Subsystem gültig ist
    if (!SignalSubsystem)
    {
        //UE_LOG(LogTemp, Error, TEXT("UAttackStateProcessor: SignalSubsystem is invalid!"));
        return;
    }

    // Set leeren, das speichert, wer in diesem Tick schon angegriffen hat
   //EntitiesThatAttackedThisTick.Reset();

    EntityQuery.ForEachEntityChunk(EntityManager, Context,
        [&](FMassExecutionContext& ChunkContext)
    {

        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto TargetList = ChunkContext.GetFragmentView<FMassAITargetFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
 
        const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();
        //UE_LOG(LogTemp, Log, TEXT("Attack EntityCount:! %d"), NumEntities);
        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassAITargetFragment& TargetFrag = TargetList[i];
            const FMassCombatStatsFragment& Stats = StatsList[i];
            const FTransform& Transform = TransformList[i].GetTransform();
            FMassVelocityFragment& Velocity = VelocityList[i];
        

            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            
            //UE::Mass::Debug::LogEntityTags(Entity, EntityManager, this);

            // 1. Sicherstellen, dass Einheit steht
            //Velocity.Value = FVector::ZeroVector;
            // Rotation zum Ziel: Sollte idealerweise ein separater LookAtProcessor machen
            // 2. Ziel verloren oder ungültig? -> Zurück zu Idle/Chase
            if (!TargetFrag.bHasValidTarget || !TargetFrag.TargetEntity.IsSet())
            {
                //UE_LOG(LogTemp, Error, TEXT("TARGET NOT VALID ANYMORE!!!!!!!"));
                UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                if (!SignalSubsystem) continue;
                    
                SignalSubsystem->SignalEntity(UnitSignals::Run, Entity);
                continue;
            }

            // 3. Timer für Angriffszyklus prüfen
            StateFrag.StateTimer += DeltaTime;

            // Gesamtdauer des Angriffs (ohne Pause)
            // 4. Prüfen, ob der "Impact"-Zeitpunkt erreicht ist und wir noch nicht angegriffen haben
            if (StateFrag.StateTimer <= Stats.AttackDuration) //&& !EntitiesThatAttackedThisTick.Contains(Entity)
            {
                // Prüfen, ob Ziel noch in Reichweite ist
                const float EffectiveAttackRange = Stats.AttackRange; // + Stats.AgentRadius; // Vereinfacht
                const float DistSq = FVector::DistSquared(Transform.GetLocation(), TargetFrag.LastKnownLocation);
                const float AttackRangeSq = FMath::Square(EffectiveAttackRange);

                if (DistSq <= AttackRangeSq)
                {
                    if (!Stats.bUseProjectile && !StateFrag.HasAttacked)// Nahkampf
                    {
                        UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                        if (!SignalSubsystem) continue;
                        
                        SignalSubsystem->SignalEntity(UnitSignals::MeleeAttack, Entity);
                        StateFrag.HasAttacked = true;
                    }
                }
                else
                {
                    //UE_LOG(LogTemp, Log, TEXT("Attack TO CHASE!!!!!!!"));
                    UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                      if (!SignalSubsystem)
                      {
                           continue; // Handle missing subsystem
                      }
                      SignalSubsystem->SignalEntity(
                      UnitSignals::Chase,
                      Entity);
                    continue;
                }
            }else
            {
                UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
                if (!SignalSubsystem)
                {
                   continue; // Handle missing subsystem
                }
                SignalSubsystem->SignalEntity(
                UnitSignals::Pause,
                Entity);
                // Angriff beendet -> Wechsle zu Pause
                StateFrag.HasAttacked = false;
                continue;
            }

            // 6. Im Attack-State bleiben
        }
    });
}



void UAttackStateProcessor::SpawnProjectileFromActor(
    FMassEntityManager& EntityManager, // <--- EntityManager als Parameter
    FMassExecutionContext& Context,
    FMassEntityHandle AttackerEntity,
    FMassEntityHandle TargetEntity,
    AActor* AttackerActor) // AttackerActor kann non-const sein, wenn UnitBase->SpawnProjectile ihn nicht ändert
{
    AUnitBase* UnitBase = Cast<AUnitBase>(AttackerActor);
    if (UnitBase && TargetEntity.IsSet())
    {
        // Hole Ziel-Actor Fragment über den übergebenen EntityManager
        const FMassActorFragment* TargetActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(TargetEntity);

        if (TargetActorFrag && TargetActorFrag->IsValid()) // Prüfe auch IsValid()
        {
            // === KORREKTUR HIER: TargetActor als const deklarieren ===
            const AActor* TargetActor = TargetActorFrag->Get(); // Get() von const Fragment* liefert const AActor*

            // Cast zu AUnitBase* ist technisch ein const_cast, aber oft notwendig/akzeptiert in UE,
            // WENN SpawnProjectile den TargetUnitBase NICHT modifiziert.
            // Wenn SpawnProjectile einen const AUnitBase* akzeptiert, wäre das sicherer.
            AUnitBase* TargetUnitBase = const_cast<AUnitBase*>(Cast<AUnitBase>(TargetActor)); // const explizit entfernen!
            if(TargetUnitBase)
            {
                UnitBase->SpawnProjectile(TargetUnitBase, UnitBase); // Funktioniert jetzt, aber umgeht const-Sicherheit
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Entity [%d] target actor for projectile [%s] was not AUnitBase!"), AttackerEntity.Index, *GetNameSafe(TargetActor));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Entity [%d] target entity [%d] for projectile has no valid ActorFragment!"), AttackerEntity.Index, TargetEntity.Index);
            // Fallback: Position verwenden?
            // const FTransformFragment* TargetTransformFrag = EntityManager.GetFragmentDataPtr<FTransformFragment>(TargetEntity);
            // if(TargetTransformFrag) { UnitBase->SpawnProjectileAtLocation(TargetTransformFrag->GetTransform().GetLocation(), UnitBase); }
        }
    }
    else if (!UnitBase)
    {
        UE_LOG(LogTemp, Warning, TEXT("Entity [%d] tried to spawn projectile but AttackerActor [%s] was not AUnitBase!"), AttackerEntity.Index, *GetNameSafe(AttackerActor));
    }
}