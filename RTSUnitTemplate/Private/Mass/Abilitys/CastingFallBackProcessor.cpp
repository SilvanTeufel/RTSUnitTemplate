// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/Abilitys/CastingFallBackProcessor.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Characters/Unit/MassUnitBase.h"
#include "Characters/Unit/UnitBase.h"
#include "MassEntityManager.h"
#include "Async/Async.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "ProfilingDebugging/CsvProfiler.h"

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
    EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::None);
    // Sicherstellen, dass die Query registriert wird
    EntityQuery.RegisterWithProcessor(*this); 
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

	// Siehe mass_scopes: macht diesen Prozessor als Spalte Exclusive/UCastingFallBackProcessor im CSV sichtbar.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UCastingFallBackProcessor);

    const UWorld* World = EntityManager.GetWorld();
    if (!EntityQuery.IsInitialized() || !World || World->GetNetMode() == NM_Client)
    {
        return;
    }

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
                if (UnitBase->GetUnitState() != UnitData::Casting)
                {
                    // The unit has moved on to something else - a worker that placed a building spends the
                    // construction in the Build state, not Casting. Signalling here forced it back into
                    // Casting AND the handler zeroes StateTimer, so the build timer was reset every
                    // interval and construction could never finish. That is only visible on the Xeno,
                    // because only their abilities set bUseCastingFallbackProcessor and therefore only
                    // they ever carry this tag. Give up instead: the tag has done its job or never applied.
                    ChunkContext.Defer().RemoveTag<FMassCastingFallbackTag>(Entity);
                }
                else if (!DoesEntityHaveTag(EntityManager, Entity, FMassStateCastingTag::StaticStruct()))
                {
                    // The real rescue case: still casting, but the Mass tag was stripped (the server's
                    // initial kick does that), so the cast would silently stop progressing.
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
                if (AUnitBase* UnitBase = Cast<AUnitBase>(ActorFrag->GetMutable()))
                {
                    // 1. Tag-Wechsel anstoßen (passiert deferred)
                    if (AMassUnitBase* MassUnitBase = Cast<AMassUnitBase>(UnitBase))
                    {
                        MassUnitBase->SwitchEntityTag(FMassStateCastingTag::StaticStruct());
                    }

                    // 2. Daten SOFORT synchronisieren (damit sie im nächsten Frame bereit sind)
                    if (FMassCombatStatsFragment* StatsFrag = EntityManager.GetFragmentDataPtr<FMassCombatStatsFragment>(Entity))
                    {
                        StatsFrag->CastTime = UnitBase->CastTime;
                    }
                    
                    if (FMassAIStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity))
                    {
                        StateFrag->StateTimer = 0.f;
                    }
                }
            }
        }
    });
}
