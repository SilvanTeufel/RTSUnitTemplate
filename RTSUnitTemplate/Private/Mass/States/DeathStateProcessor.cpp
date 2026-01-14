// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Mass/States/DeathStateProcessor.h" // Passe Pfad an
#include "MassExecutionContext.h"
#include "MassEntityManager.h"

// Fragmente und Tags
#include "MassMovementFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassActorSubsystem.h"  
#include "MassCommonFragments.h" // Für FMassRepresentationFragment (optional für Sichtbarkeit)

// Für Actor Cast und Destroy
#include "MassSignalSubsystem.h"
#include "MassEntitySubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "Controller/PlayerController/ControllerBase.h"
#include "Mass/Signals/MySignals.h"
#include "Async/Async.h"
#include "Hud/HUDBase.h"


UDeathStateProcessor::UDeathStateProcessor(): EntityQuery()
{
    ExecutionFlags = (int32)EProcessorExecutionFlags::All;
    ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
}

void UDeathStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.Initialize(EntityManager);
    EntityQuery.AddTagRequirement<FMassStateDeadTag>(EMassFragmentPresence::All); // Nur tote Entitäten
    
    EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite); // Timer
    EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite); // Anhalten
    EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadOnly);

    EntityQuery.RegisterWithProcessor(*this);
}

void UDeathStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
    Super::InitializeInternal(Owner, EntityManager);
    SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
    EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(Owner.GetWorld());

    if (SignalSubsystem)
    {
        RemoveDeadUnitSignalDelegateHandle = SignalSubsystem->GetSignalDelegateByName(UnitSignals::RemoveDeadUnit)
            .AddUFunction(this, GET_FUNCTION_NAME_CHECKED(UDeathStateProcessor, HandleRemoveDeadUnit));
    }
}

void UDeathStateProcessor::BeginDestroy()
{
    if (SignalSubsystem)
    {
        if (RemoveDeadUnitSignalDelegateHandle.IsValid())
        {
            auto& Delegate = SignalSubsystem->GetSignalDelegateByName(UnitSignals::RemoveDeadUnit);
            Delegate.Remove(RemoveDeadUnitSignalDelegateHandle);
            RemoveDeadUnitSignalDelegateHandle.Reset();
        }
    }
    Super::BeginDestroy();
}

void UDeathStateProcessor::HandleRemoveDeadUnit(FName SignalName, TArray<FMassEntityHandle>& Entities)
{
    if (!EntitySubsystem) return;

    TArray<FMassEntityHandle> EntitiesCopy = Entities; 

    AsyncTask(ENamedThreads::GameThread, [this, EntitiesCopy]()
    {
        if (!EntitySubsystem || !GetWorld()) return;

        FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
        for (const FMassEntityHandle& Entity : EntitiesCopy)
        {
            if (!EntityManager.IsEntityValid(Entity)) continue;

            FMassActorFragment* ActorFragPtr = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
            if (ActorFragPtr && IsValid(ActorFragPtr->GetMutable()))
            {
                AUnitBase* UnitBase = Cast<AUnitBase>(ActorFragPtr->GetMutable());
                if (UnitBase)
                {
                    UnitBase->SetDeselected();
                    UnitBase->CanBeSelected = false;
                    if (GetWorld()->IsNetMode(NM_Client))
                    {
                        AControllerBase* PC = Cast<AControllerBase>(GetWorld()->GetFirstPlayerController());
                        if (PC && PC->HUDBase)
                        {
                            int32 UnitIndex = PC->HUDBase->SelectedUnits.Find(UnitBase);
                            PC->HUDBase->SelectedUnits.Remove(UnitBase);
                            PC->SelectedUnits = PC->HUDBase->SelectedUnits;
                            if (UnitIndex != INDEX_NONE)
                            {
                                if (UnitIndex < PC->CurrentUnitWidgetIndex)
                                {
                                    PC->CurrentUnitWidgetIndex--;
                                }
                                else if (UnitIndex == PC->CurrentUnitWidgetIndex)
                                {
                                    if (PC->HUDBase->SelectedUnits.Num() > 0)
                                    {
                                        PC->CurrentUnitWidgetIndex = FMath::Clamp(PC->CurrentUnitWidgetIndex, 0, PC->HUDBase->SelectedUnits.Num() - 1);
                                    }
                                    else
                                    {
                                        PC->CurrentUnitWidgetIndex = 0;
                                    }
                                }
                            }
                        }
                    }
                    /*
                    else
                    {
                        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
                        {
                            AControllerBase* PC = Cast<AControllerBase>(It->Get());
                            if (PC && PC->HUDBase)
                            {
                                int32 UnitIndex = PC->HUDBase->SelectedUnits.Find(UnitBase);
                                PC->HUDBase->SelectedUnits.Remove(UnitBase);
                                PC->SelectedUnits = PC->HUDBase->SelectedUnits;
                                if (UnitIndex != INDEX_NONE)
                                {
                                    if (UnitIndex < PC->CurrentUnitWidgetIndex)
                                    {
                                        PC->CurrentUnitWidgetIndex--;
                                    }
                                    else if (UnitIndex == PC->CurrentUnitWidgetIndex)
                                    {
                                        if (PC->HUDBase->SelectedUnits.Num() > 0)
                                        {
                                            PC->CurrentUnitWidgetIndex = FMath::Clamp(PC->CurrentUnitWidgetIndex, 0, PC->HUDBase->SelectedUnits.Num() - 1);
                                        }
                                        else
                                        {
                                            PC->CurrentUnitWidgetIndex = 0;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    */
                }
            }
        }
    });
}

void UDeathStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastRun += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastRun < ExecutionInterval)
    {
        return; 
    }
    TimeSinceLastRun -= ExecutionInterval;

    if (GetWorld() && GetWorld()->IsNetMode(NM_Client))
    {
        ExecuteClient(EntityManager, Context);
    }
    else
    {
        ExecuteServer(EntityManager, Context);
    }
}

void UDeathStateProcessor::ExecuteClient(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    if (!SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(Context,
        [this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

            const float PrevTimer = StateFrag.StateTimer;
            StateFrag.StateTimer += ExecutionInterval;
            
            if (PrevTimer <= KINDA_SMALL_NUMBER)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::RemoveDeadUnit, Entity);
            }
        }
    });
}

void UDeathStateProcessor::ExecuteServer(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    if (!SignalSubsystem) return;

    EntityQuery.ForEachEntityChunk(Context,
        [this](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        auto StateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
        auto VelocityList = ChunkContext.GetMutableFragmentView<FMassVelocityFragment>();
        const auto AgentFragList = ChunkContext.GetFragmentView<FMassAgentCharacteristicsFragment>();

        for (int32 i = 0; i < NumEntities; ++i)
        {
            FMassAIStateFragment& StateFrag = StateList[i];
            FMassVelocityFragment& Velocity = VelocityList[i];
            const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
            const FMassAgentCharacteristicsFragment CharacteristicsFragment = AgentFragList[i];

            Velocity.Value = FVector::ZeroVector;

            const float PrevTimer = StateFrag.StateTimer;
            StateFrag.StateTimer += ExecutionInterval;

            if (PrevTimer <= KINDA_SMALL_NUMBER)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::StartDead, Entity);
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::RemoveDeadUnit, Entity);
            }
            
            if (StateFrag.StateTimer >= CharacteristicsFragment.DespawnTime+1.f)
            {
                SignalSubsystem->SignalEntityDeferred(ChunkContext, UnitSignals::EndDead, Entity);
            }
        }
    });
}
