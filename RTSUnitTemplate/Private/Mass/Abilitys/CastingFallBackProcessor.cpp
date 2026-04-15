#include "Mass/Abilitys/CastingFallBackProcessor.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Characters/Unit/MassUnitBase.h"
#include "MassEntityManager.h"
#include "Async/Async.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"

UCastingFallBackProcessor::UCastingFallBackProcessor()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::Server | (int32)EProcessorExecutionFlags::Standalone;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    bRequiresGameThreadExecution = false;
}

void UCastingFallBackProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddTagRequirement<FMassCastingFallbackTag>(EMassFragmentPresence::All);
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
}

void UCastingFallBackProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
    if (SignalSubsystem)
    {
        SignalSubsystem->GetSignalDelegateByName(TEXT("CastingFallbackSignal"))
            .AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UCastingFallBackProcessor, HandleCastingFallback));
    }
}

void UCastingFallBackProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval) return;
    TimeSinceLastRun -= ExecutionInterval;

    EntityQuery.ForEachEntityChunk(Context, [this, &EntityManager](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        const auto StatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
        const auto ActorList = ChunkContext.GetFragmentView<FMassActorFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const AMassUnitBase* UnitBase = Cast<AMassUnitBase>(ActorList[i].Get());
            
            if (UnitBase)
            {
                if (UnitBase->GetUnitState() != UnitData::Casting || !DoesEntityHaveTag(EntityManager, Entity, FMassStateCastingTag::StaticStruct()))
                {
                    if (SignalSubsystem)
                    {
                        SignalSubsystem->SignalEntityDeferred(ChunkContext, TEXT("CastingFallbackSignal"), Entity);
                    }
                }
            }

            // Remove tag shortly before CastTime is over
            if (StateList[i].StateTimer >= StatsList[i].CastTime - ExecutionInterval)
            {
                ChunkContext.Defer().RemoveTag<FMassCastingFallbackTag>(Entity);
            }
        }
    });
}

void UCastingFallBackProcessor::HandleCastingFallback(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    UWorld* World = GetWorld();
    if (!World) return;

    TArray<FMassEntityHandle> EntitiesCopy = Entities;
    AsyncTask(ENamedThreads::GameThread, [World, EntitiesCopy]()
    {
        UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
        if (!EntitySubsystem) return;

        FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
        for (const FMassEntityHandle& Entity : EntitiesCopy)
        {
            if (FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity))
            {
                if (AMassUnitBase* UnitBase = Cast<AMassUnitBase>(ActorFrag->GetMutable()))
                {
                    UnitBase->SwitchEntityTag(FMassStateCastingTag::StaticStruct());
                }
            }
        }
    });
}
