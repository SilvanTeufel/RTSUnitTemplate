// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/Replication/UnitActorToFragmentSyncProcessor.h"
#include "MassCommonFragments.h"
#include "MassActorSubsystem.h"
#include "Characters/Unit/UnitBase.h"
#include "MassExecutionContext.h"
#include "MassReplicationFragments.h"
#include "Mass/UnitMassTag.h"
#include "MassEntitySubsystem.h"


UUnitActorToFragmentSyncProcessor::UUnitActorToFragmentSyncProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All; // Allow Client and Server execution
	bRequiresGameThreadExecution = true; // we touch AActors
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
}

void UUnitActorToFragmentSyncProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassCombatStatsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAgentCharacteristicsFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RegisterWithProcessor(*this);
}

void UUnitActorToFragmentSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [&, this](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FMassActorFragment> ActorList = ChunkContext.GetFragmentView<FMassActorFragment>();
		const TArrayView<FMassCombatStatsFragment> CombatStatsList = ChunkContext.GetMutableFragmentView<FMassCombatStatsFragment>();
		const TArrayView<FMassAgentCharacteristicsFragment> CharacteristicsList = ChunkContext.GetMutableFragmentView<FMassAgentCharacteristicsFragment>();
		const TArrayView<FMassAIStateFragment> AIStateList = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();

		for (int32 EntityIndex = 0; EntityIndex < ChunkContext.GetNumEntities(); ++EntityIndex)
		{
			if (const AUnitBase* Unit = Cast<AUnitBase>(ActorList[EntityIndex].Get()))
			{
				// Sync Combat Stats
				if (Unit->Attributes)
				{
					CombatStatsList[EntityIndex].Health = Unit->Attributes->GetHealth();
					CombatStatsList[EntityIndex].Shield = Unit->Attributes->GetShield();
					CombatStatsList[EntityIndex].MaxHealth = Unit->Attributes->GetMaxHealth();
					CombatStatsList[EntityIndex].MaxShield = Unit->Attributes->GetMaxShield();
					CombatStatsList[EntityIndex].TeamId = Unit->TeamId;
				}

				// Sync Characteristics
				CharacteristicsList[EntityIndex].FlyHeight = Unit->FlyHeight;
				CharacteristicsList[EntityIndex].bIsFlying = Unit->IsFlying;

				// Sync AI State
				//AIStateList[EntityIndex].StateTimer = Unit->UnitControlTimer; 
			}
		}
	});
}
