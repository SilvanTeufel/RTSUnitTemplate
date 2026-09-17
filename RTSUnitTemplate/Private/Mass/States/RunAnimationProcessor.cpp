// Copyright 2024 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Mass/States/RunAnimationProcessor.h"
#include "MassExecutionContext.h"
#include "MassEntityManager.h"
#include "Mass/UnitMassTag.h"
#include "MassCommonFragments.h"

URunAnimationProcessor::URunAnimationProcessor()
{
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = false;
}

void URunAnimationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddTagRequirement<FRunAnimationTag>(EMassFragmentPresence::All);
	// Optional, nicht Pflicht: eine Entitaet, die den Tag OHNE Fragment traegt, wuerde sonst gar
	// nicht erst erfasst - und weil dieser Prozessor der einzige ist, der den Tag wieder entfernt,
	// bliebe sie fuer immer darin haengen. Genau so ist sie auf dem Client eingefroren. Fehlt das
	// Fragment, greift unten die Vorgabedauer und der Tag faellt trotzdem weg.
	EntityQuery.AddRequirement<FRunAnimationFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FMassAIStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RegisterWithProcessor(*this);
}

void URunAnimationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, ([&](FMassExecutionContext& ChunkContext)
	{
		const float DeltaTime = ChunkContext.GetDeltaTimeSeconds();
		auto StateFrags = ChunkContext.GetMutableFragmentView<FMassAIStateFragment>();
		auto RunAnimFrags = ChunkContext.GetFragmentView<FRunAnimationFragment>();
		const bool bHatFragmente = RunAnimFrags.Num() > 0;
		constexpr float VorgabeDauer = 1.0f;

		for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
		{
			StateFrags[i].StateTimer += DeltaTime;
			const float Dauer = bHatFragmente ? RunAnimFrags[i].Duration : VorgabeDauer;
			if (StateFrags[i].StateTimer >= Dauer)
			{
				// Timer finished: remove the tag and fragment
				StateFrags[i].StateTimer = 0.f;
				Context.Defer().RemoveTag<FRunAnimationTag>(ChunkContext.GetEntity(i));
				if (bHatFragmente)
				{
					Context.Defer().RemoveFragment<FRunAnimationFragment>(ChunkContext.GetEntity(i));
				}
			}
		}
	}));
}
