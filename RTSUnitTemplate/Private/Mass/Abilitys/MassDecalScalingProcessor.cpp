// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/Abilitys/MassDecalScalingProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassEntityView.h"
#include "MassActorSubsystem.h" 
#include "Mass/UnitMassTag.h"
#include "Actors/AreaDecalComponent.h"
#include "Characters/Unit/UnitBase.h"
#include "Characters/Unit/BuildingBase.h"
#include "Mass/Abilitys/DecalScalingFragments.h"

UMassDecalScalingProcessor::UMassDecalScalingProcessor()
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true; // Needed to call into Actors
}

void UMassDecalScalingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassGameplayEffectFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FMassDecalScalingTag>(EMassFragmentPresence::All);
	EntityQuery.RegisterWithProcessor(*this);
}

void UMassDecalScalingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TimeSinceLastRun += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastRun < ExecutionInterval)
	{
		return;
	}
	
	const float AccumulatedDeltaTime = TimeSinceLastRun;
	TimeSinceLastRun = 0.0f; // Reset timer

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, AccumulatedDeltaTime](FMassExecutionContext& ChunkContext)
	{
		const int32 NumEntities = ChunkContext.GetNumEntities();
		TArrayView<FMassActorFragment> ActorList = ChunkContext.GetMutableFragmentView<FMassActorFragment>();
		TArrayView<FMassGameplayEffectFragment> EffectList = ChunkContext.GetMutableFragmentView<FMassGameplayEffectFragment>();
		const bool bHasEffectFragment = EffectList.Num() > 0;

		for (int32 i = 0; i < NumEntities; ++i)
		{
			FMassActorFragment& ActorFrag = ActorList[i];
			
			bool bDone = false;
			float NewRadius = 0.f;

			if (AActor* Actor = ActorFrag.GetMutable())
			{
				if (UAreaDecalComponent* DecalComp = Actor->FindComponentByClass<UAreaDecalComponent>())
				{
					if (DecalComp->AdvanceMassScaling(AccumulatedDeltaTime, NewRadius, bDone))
					{
						if (bHasEffectFragment)
						{
							EffectList[i].EffectRadius = NewRadius;
						}
						
						DecalComp->SetCurrentDecalRadiusFromMass(NewRadius);
						if (DecalComp->IsBeaconScaling())
						{
							if (ABuildingBase* Building = Cast<ABuildingBase>(Actor))
							{
								Building->SetBeaconRange(NewRadius);
							}
						}
					}
				}
			}

			if (bDone)
			{
				ChunkContext.Defer().RemoveTag<FMassDecalScalingTag>(ChunkContext.GetEntity(i));
			}
		}
	});
}
