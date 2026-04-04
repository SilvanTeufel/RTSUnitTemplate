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
	EntityQuery.AddRequirement<FRunAnimationFragment>(EMassFragmentAccess::ReadOnly);
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

		for (int32 i = 0; i < ChunkContext.GetNumEntities(); ++i)
		{
			StateFrags[i].StateTimer += DeltaTime;
			if (StateFrags[i].StateTimer >= RunAnimFrags[i].Duration)
			{
				// Timer finished: remove the tag and fragment
				StateFrags[i].StateTimer = 0.f;
				Context.Defer().RemoveTag<FRunAnimationTag>(ChunkContext.GetEntity(i));
				Context.Defer().RemoveFragment<FRunAnimationFragment>(ChunkContext.GetEntity(i));
			}
		}
	}));
}
