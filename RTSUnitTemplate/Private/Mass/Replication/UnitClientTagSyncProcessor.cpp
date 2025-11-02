#include "Mass/Replication/UnitClientTagSyncProcessor.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "GameFramework/Actor.h"
#include "Characters/Unit/AbilityUnit.h"
#include "Mass/UnitMassTag.h"
#include "Core/UnitData.h"
#include "Mass/Replication/ReplicationSettings.h"
#include "EngineUtils.h"
#include "Mass/MassActorBindingComponent.h"

UUnitClientTagSyncProcessor::UUnitClientTagSyncProcessor()
	: EntityQuery(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::Client; // client-only
	bRequiresGameThreadExecution = true; // we touch AActors
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UUnitClientTagSyncProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.RegisterWithProcessor(*this);
}

void UUnitClientTagSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// Respect global replication mode: only run in Mass mode
	if (RTSReplicationSettings::GetReplicationMode() != RTSReplicationSettings::Mass)
	{
		return;
	}

	// Iterate all AbilityUnit actors on the client and mirror their Mass tag-derived state
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// This processor is client-only. If not on a client, do nothing.
	if (!World->IsNetMode(NM_Client))
	{
		return;
	}
	
	if (bShowLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("UnitClientTagSyncProcessor: Execute (World=%s Time=%.2f)"), *World->GetName(), World->GetTimeSeconds());
	}
	for (TActorIterator<AAbilityUnit> It(World); It; ++It)
	{
		AAbilityUnit* Ability = *It;
		if (!Ability) { continue; }
		if (UMassActorBindingComponent* Bind = Ability->FindComponentByClass<UMassActorBindingComponent>())
		{
			const FMassEntityHandle Entity = Bind->GetEntityHandle();
			if (Entity.IsValid())
			{
				const TEnumAsByte<UnitData::EState> NewState = ComputeState(EntityManager, Entity);
				ApplyStateToActor(Ability, NewState);
			}
		}
	}
}

TEnumAsByte<UnitData::EState> UUnitClientTagSyncProcessor::ComputeState(const FMassEntityManager& EntityManager, const FMassEntityHandle& Entity) const
{
	using namespace UnitData;
	// Helper lambda for tag checks via composition (client-safe)
	auto HasTag = [&EntityManager, &Entity](const UScriptStruct* TagStruct) -> bool
	{
		return DoesEntityHaveTag(EntityManager, Entity, TagStruct);
	};
	// Highest priority first
	if (HasTag(FMassStateDeadTag::StaticStruct()))
	{
		return EState::Dead;
	}
	if (HasTag(FMassStateRootedTag::StaticStruct()))
	{
		return EState::Rooted;
	}
	if (HasTag(FMassStateCastingTag::StaticStruct()) || HasTag(FMassStateChargingTag::StaticStruct()))
	{
		return EState::Casting;
	}
	if (HasTag(FMassStateIsAttackedTag::StaticStruct()))
	{
		return EState::IsAttacked;
	}
	if (HasTag(FMassStateAttackTag::StaticStruct()))
	{
		return EState::Attack;
	}
	if (HasTag(FMassStateChaseTag::StaticStruct()))
	{
		return EState::Chase;
	}
	// Worker flows
	if (HasTag(FMassStateBuildTag::StaticStruct()))
	{
		return EState::Build;
	}
	if (HasTag(FMassStateResourceExtractionTag::StaticStruct()))
	{
		return EState::ResourceExtraction;
	}
	if (HasTag(FMassStateGoToResourceExtractionTag::StaticStruct()))
	{
		return EState::GoToResourceExtraction;
	}
	if (HasTag(FMassStateGoToBuildTag::StaticStruct()))
	{
		return EState::GoToBuild;
	}
	if (HasTag(FMassStateGoToBaseTag::StaticStruct()))
	{
		return EState::GoToBase;
	}
	// Patrol/Run
	if (HasTag(FMassStatePatrolIdleTag::StaticStruct()))
	{
		return EState::PatrolIdle;
	}
	if (HasTag(FMassStatePatrolRandomTag::StaticStruct()))
	{
		return EState::PatrolRandom;
	}
	if (HasTag(FMassStatePatrolTag::StaticStruct()))
	{
		return EState::Patrol;
	}
	if (HasTag(FMassStateRunTag::StaticStruct()))
	{
		return EState::Run;
	}
	// Pause and Evasion
	if (HasTag(FMassStatePauseTag::StaticStruct()))
	{
		return EState::Pause;
	}
	if (HasTag(FMassStateEvasionTag::StaticStruct()))
	{
		return EState::Evasion;
	}
	// Default fallback
	if (HasTag(FMassStateIdleTag::StaticStruct()))
	{
		return EState::Idle;
	}
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
		
		AbilityUnit->SetUnitState(NewState);
	}
}
