#include "Mass/Replication/UnitClientTagSyncProcessor.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "GameFramework/Actor.h"
#include "Characters/Unit/AbilityUnit.h"
#include "Characters/Unit/UnitBase.h"
#include "Mass/UnitMassTag.h"
#include "Core/UnitData.h"
#include "Mass/Replication/ReplicationSettings.h"
#include "GameStates/ResourceGameState.h"
#include "EngineUtils.h"
#include "Mass/MassActorBindingComponent.h"
#include "MassSignalSubsystem.h"
#include "Mass/Signals/MySignals.h"

UUnitClientTagSyncProcessor::UUnitClientTagSyncProcessor()
	: EntityQuery(*this)
	, InitialKickCleanupQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All; // Allow Client and Server execution
	bRequiresGameThreadExecution = true; // we touch AActors
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UUnitClientTagSyncProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);
	if (UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld()))
	{
		SubscribeToSignal(*SignalSubsystem, UnitSignals::UnitSpawned);
	}
}

void UUnitClientTagSyncProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassAITargetFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FMassIsEffectAreaTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateFrozenTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassStateNeedsInitialKickTag>(EMassFragmentPresence::None);
	
	EntityQuery.RegisterWithProcessor(*this);

	InitialKickCleanupQuery.Initialize(EntityManager);
	InitialKickCleanupQuery.AddTagRequirement<FMassStateNeedsInitialKickTag>(EMassFragmentPresence::All);
	InitialKickCleanupQuery.RegisterWithProcessor(*this);

	LoadingTagCleanupQuery.Initialize(EntityManager);
	LoadingTagCleanupQuery.AddTagRequirement<FMassEffectAreaLoadingTag>(EMassFragmentPresence::All);
	LoadingTagCleanupQuery.RegisterWithProcessor(*this);
}

void UUnitClientTagSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (RTSReplicationSettings::GetReplicationMode() != RTSReplicationSettings::Mass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	// Client-side cleanup for the Initial Kick tag
	if (World->GetNetMode() == NM_Client)
	{
		InitialKickCleanupQuery.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& KickCtx)
		{
			const int32 Num = KickCtx.GetNumEntities();
			for (int32 i = 0; i < Num; ++i)
			{
				const FMassEntityHandle Entity = KickCtx.GetEntity(i);
				KickCtx.Defer().RemoveTag<FMassStateNeedsInitialKickTag>(Entity);
				
				// Also remove StopSeparation tag if it was added during spawn 
				// to prevent units from being stuck in place.
				KickCtx.Defer().RemoveTag<FMassStateStopSeparationTag>(Entity);
			}
		});
	}

	// Client-side cleanup for the Loading tag
	if (World->GetNetMode() == NM_Client)
	{
		if (AResourceGameState* GS = World->GetGameState<AResourceGameState>())
		{
			if (GS->MatchStartTime > 0 && GS->GetServerWorldTimeSeconds() >= GS->MatchStartTime)
			{
				LoadingTagCleanupQuery.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& LoadingCtx)
				{
					const int32 Num = LoadingCtx.GetNumEntities();
					for (int32 i = 0; i < Num; ++i)
					{
						const FMassEntityHandle Entity = LoadingCtx.GetEntity(i);
						LoadingCtx.Defer().RemoveTag<FMassEffectAreaLoadingTag>(Entity);
					}
				});
			}
		}
	}

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, &EntityManager, SignalSubsystem, &Context, World](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		auto ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
		auto CombatStatsList = ChunkContext.GetFragmentView<FMassCombatStatsFragment>();
		auto AIStateList = ChunkContext.GetFragmentView<FMassAIStateFragment>();
		auto AITargetList = ChunkContext.GetMutableFragmentView<FMassAITargetFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
		
			const FMassAIStateFragment* StateFragPtr = !AIStateList.IsEmpty() ? &AIStateList[i] : nullptr;
			FMassAITargetFragment* TargetFragPtr = !AITargetList.IsEmpty() ? &AITargetList[i] : nullptr;
			
			if (StateFragPtr)
			{
				const float Age = World->GetTimeSeconds() - StateFragPtr->BirthTime;
				if (bShowLogs)
				{
					UE_LOG(LogTemp, Log, TEXT("UnitClientTagSyncProcessor: Entity Age: %.2f (BirthTime: %.2f, WorldTime: %.2f)"), 
						Age, StateFragPtr->BirthTime, World->GetTimeSeconds());
				}
				if (Age < 1.f)
				{
					continue;
				}
			}

			if (AUnitBase* UnitBase = Cast<AUnitBase>(ActorList[i].GetMutable()))
			{
				const FMassEntityHandle Entity = ChunkContext.GetEntity(i);

				const TEnumAsByte<UnitData::EState> NewState = ComputeState(EntityManager, Entity);

				// Spezialfall Casting: Wenn Timer abgelaufen, Signal senden
				if (NewState == UnitData::Casting && SignalSubsystem && StateFragPtr)
				{
					const FMassCombatStatsFragment& StatsFrag = CombatStatsList[i];
					if (StateFragPtr->StateTimer >= StatsFrag.CastTime)
					{
						SignalSubsystem->SignalEntityDeferred(Context, UnitSignals::ClientSetToPlaceholder, Entity);
					}
				}

				if (TargetFragPtr)
				{
					if (UMassActorBindingComponent* BindingComp = UnitBase->FindComponentByClass<UMassActorBindingComponent>())
					{
						TargetFragPtr->FollowRadius = BindingComp->FollowRadius;
						TargetFragPtr->FollowOffset = BindingComp->FollowOffset;
					}
				}

				ApplyStateToActor(UnitBase, NewState);
			}
		}
	});

	Super::Execute(EntityManager, Context);
}

void UUnitClientTagSyncProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context,
	FMassSignalNameLookup& EntitySignals)
{
	EntityQuery.ForEachEntityChunk(Context, [this, &EntitySignals, &EntityManager](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		for (int32 i = 0; i < NumEntities; ++i)
		{
			const FMassEntityHandle Entity = ChunkContext.GetEntity(i);
			TArray<FName> Signals;
			EntitySignals.GetSignalsForEntity(Entity, Signals);
			if (Signals.Contains(UnitSignals::UnitSpawned))
			{
				HandleUnitSpawned(Entity, EntityManager);
			}
		}
	});
}

void UUnitClientTagSyncProcessor::HandleUnitSpawned(FMassEntityHandle Entity, FMassEntityManager& EntityManager)
{
	FMassAIStateFragment* StateFrag = EntityManager.GetFragmentDataPtr<FMassAIStateFragment>(Entity);
	FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
	FMassAITargetFragment* TargetFrag = EntityManager.GetFragmentDataPtr<FMassAITargetFragment>(Entity);

	if (StateFrag && ActorFrag)
	{
		AUnitBase* Unit = Cast<AUnitBase>(ActorFrag->GetMutable());
		if (Unit)
		{
			// Initialize the local actor state from the replicated StoredUnitState on the client
			if (Unit->GetNetMode() == NM_Client)
			{
				Unit->SetUnitState(Unit->StoredUnitState);
			}

			StateFrag->CanMove = Unit->CanMove;
			StateFrag->CanAttack = Unit->CanAttack;
			StateFrag->IsInitialized = Unit->IsInitialized;
			StateFrag->StoredLocation = Unit->GetActorLocation();

			if (TargetFrag)
			{
				if (UMassActorBindingComponent* BindingComp = Unit->FindComponentByClass<UMassActorBindingComponent>())
				{
					TargetFrag->FollowRadius = BindingComp->FollowRadius;
					TargetFrag->FollowOffset = BindingComp->FollowOffset;
				}
			}

			Unit->SwitchEntityTagByState(Unit->StoredUnitState, Unit->UnitStatePlaceholder);
		}
	}
}

TEnumAsByte<UnitData::EState> UUnitClientTagSyncProcessor::ComputeState(const FMassEntityManager& EntityManager, const FMassEntityHandle& Entity) const
{
	using namespace UnitData;

	auto HasTag = [&EntityManager, &Entity](const UScriptStruct* TagStruct) -> bool
	{
		return DoesEntityHaveTag(EntityManager, Entity, TagStruct);
	};

	// 1. Dead (Höchste Priorität)
	if (HasTag(FMassStateDeadTag::StaticStruct())) return EState::Dead;

	if (HasTag(FMassStateContinuousAttackTag::StaticStruct())) return EState::ContinousAttack;

	// 1.1 FMassRotateToMouseTag (ZweitHöchste Priorität)
	if (HasTag(FMassRotateToMouseTag::StaticStruct())) return EState::Aim;
	
	// 2. IsAttacked
	if (HasTag(FMassStateIsAttackedTag::StaticStruct())) return EState::IsAttacked;

	// 3. Casting / Charging
	if (HasTag(FMassStateCastingTag::StaticStruct()) || HasTag(FMassStateChargingTag::StaticStruct())) return EState::Casting;

	// 4. Attack
	if (HasTag(FMassStateAttackTag::StaticStruct())) return EState::Attack;

	// 5. Worker: Build / Repair / Extraction
	if (HasTag(FMassStateBuildTag::StaticStruct())) return EState::Build;
	if (HasTag(FMassStateRepairTag::StaticStruct())) return EState::Repair;
	if (HasTag(FMassStateResourceExtractionTag::StaticStruct())) return EState::ResourceExtraction;

	// 6. Pause (WICHTIG: Muss vor Chase/Run stehen!)
	if (HasTag(FMassStatePauseTag::StaticStruct())) return EState::Pause;

	// 7. Worker Movement
	if (HasTag(FMassStateGoToBaseTag::StaticStruct())) return EState::GoToBase;
	if (HasTag(FMassStateGoToBuildTag::StaticStruct())) return EState::GoToBuild;
	if (HasTag(FMassStateGoToRepairTag::StaticStruct())) return EState::GoToRepair;
	if (HasTag(FMassStateGoToResourceExtractionTag::StaticStruct())) return EState::GoToResourceExtraction;

	// 8. General Movement
	if (HasTag(FMassStateChaseTag::StaticStruct())) return EState::Chase;
	if (HasTag(FMassStateRunTag::StaticStruct())) return EState::Run;

	// 9. Patrol
	if (HasTag(FMassStatePatrolRandomTag::StaticStruct())) return EState::PatrolRandom;
	if (HasTag(FMassStatePatrolIdleTag::StaticStruct())) return EState::PatrolIdle;
	if (HasTag(FMassStatePatrolTag::StaticStruct())) return EState::Patrol;

	// 10. Utility / Other
	if (HasTag(FMassStateEvasionTag::StaticStruct())) return EState::Evasion;
	if (HasTag(FMassStateRootedTag::StaticStruct())) return EState::Rooted;

	if (HasTag(FRunAnimationTag::StaticStruct()))
	{
		if (const FRunAnimationFragment* RunAnimFrag = EntityManager.GetFragmentDataPtr<FRunAnimationFragment>(Entity))
		{
			return RunAnimFrag->AnimationState;
		}
		return EState::Attack;
	}

	// 11. Idle (Niedrigste Priorität)
	if (HasTag(FMassStateIdleTag::StaticStruct())) return EState::Idle;

	return EState::None; 
}

void UUnitClientTagSyncProcessor::ApplyStateToActor(AAbilityUnit* AbilityUnit, TEnumAsByte<UnitData::EState> NewState) const
{
	if (!AbilityUnit)
	{
		return;
	}
	const TEnumAsByte<UnitData::EState> OldState = AbilityUnit->GetUnitState();
	if (OldState != NewState)
	{
		if (bShowLogs)
		{
			const UEnum* EnumPtr = StaticEnum<UnitData::EState>();
			const FString OldStr = EnumPtr ? EnumPtr->GetNameStringByValue(OldState.GetValue()) : FString::FromInt(OldState.GetValue());
			const FString NewStr = EnumPtr ? EnumPtr->GetNameStringByValue(NewState.GetValue()) : FString::FromInt(NewState.GetValue());
			UE_LOG(LogTemp, Log, TEXT("UnitClientTagSyncProcessor: %s state %s -> %s"),
				*AbilityUnit->GetName(), *OldStr, *NewStr);
		}
		
			if (NewState != UnitData::None && AbilityUnit->GetUnitState() != UnitData::Dead)
			{
				AbilityUnit->SetUnitState(NewState);
			}
	}
}
