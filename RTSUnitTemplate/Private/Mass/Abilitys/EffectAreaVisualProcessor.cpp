#include "Mass/Abilitys/EffectAreaVisualProcessor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassEntityView.h"
#include "MassActorSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Actors/EffectArea.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

UMassEffectAreaVisualProcessor::UMassEffectAreaVisualProcessor()
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UMassEffectAreaVisualProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	VisualQuery.Initialize(EntityManager);
	VisualQuery.AddRequirement<FEffectAreaVisualFragment>(EMassFragmentAccess::ReadWrite);
	VisualQuery.AddRequirement<FEffectAreaImpactFragment>(EMassFragmentAccess::ReadWrite);
	VisualQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	VisualQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
    VisualQuery.AddRequirement<FMassVisibilityFragment>(EMassFragmentAccess::ReadOnly);
	VisualQuery.AddTagRequirement<FMassEffectAreaActiveTag>(EMassFragmentPresence::All);
	VisualQuery.RegisterWithProcessor(*this);

	CleanupQuery.Initialize(EntityManager);
	CleanupQuery.AddRequirement<FEffectAreaVisualFragment>(EMassFragmentAccess::ReadWrite);
	CleanupQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	CleanupQuery.AddTagRequirement<FMassEffectAreaActiveTag>(EMassFragmentPresence::None);
	CleanupQuery.AddTagRequirement<FMassEffectAreaImpactTag>(EMassFragmentPresence::All);
	CleanupQuery.RegisterWithProcessor(*this);
}

void UMassEffectAreaVisualProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	// 1. Visual Updates
	VisualQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& VisualContext)
	{
		const int32 NumEntities = VisualContext.GetNumEntities();
		TArrayView<FEffectAreaVisualFragment> VisualList = VisualContext.GetMutableFragmentView<FEffectAreaVisualFragment>();
		TArrayView<FEffectAreaImpactFragment> ImpactList = VisualContext.GetMutableFragmentView<FEffectAreaImpactFragment>();
		TConstArrayView<FTransformFragment> TransformList = VisualContext.GetFragmentView<FTransformFragment>();
		TArrayView<FMassActorFragment> ActorList = VisualContext.GetMutableFragmentView<FMassActorFragment>();
		TConstArrayView<FMassVisibilityFragment> VisibilityList = VisualContext.GetFragmentView<FMassVisibilityFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			FEffectAreaVisualFragment& Visual = VisualList[i];
			FEffectAreaImpactFragment& Impact = ImpactList[i];
			const FTransform& EntityTransform = TransformList[i].GetTransform();
			const FMassVisibilityFragment& Visibility = VisibilityList[i];
            
			AActor* AreaActor = ActorList[i].GetMutable();
			AEffectArea* EffectArea = Cast<AEffectArea>(AreaActor);

            bool bIsVisible = true;
            if (EffectArea && EffectArea->bAffectedByFogOfWar)
            {
                // In a real scenario, we'd check against a fog subsystem or use FMassVisibilityFragment if updated by another processor
                bIsVisible = EffectArea->bIsVisibleByFog;
            }

			if (Visual.ISMComponent.IsValid() && Visual.InstanceIndex != INDEX_NONE)
			{
				if (bIsVisible)
				{
					float LocalRadius = Visual.BaseMeshRadius;
					float ScaleFactor = (LocalRadius > 0.f) ? (Impact.CurrentRadius / LocalRadius) : 1.f;
					FTransform VisualTransform = Visual.VisualRelativeTransform * EntityTransform;
					VisualTransform.SetScale3D(FVector(ScaleFactor));
					
					Visual.ISMComponent->UpdateInstanceTransform(Visual.InstanceIndex, VisualTransform, true, true, true);
				}
				else
				{
					// Hide instance by setting scale to zero
					Visual.ISMComponent->UpdateInstanceTransform(Visual.InstanceIndex, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector), true, true, true);
				}
			}

            // Handle Niagara position
            if (Visual.Niagara_A.IsValid())
            {
                Visual.Niagara_A->SetWorldTransform(Visual.Niagara_A_RelativeTransform * EntityTransform);
                Visual.Niagara_A->SetVisibility(bIsVisible);
            }

            // Handle Impact VFX
            bool bTriggerVFX = Impact.bImpactVFXTriggered || (EffectArea && EffectArea->bImpactVFXTriggered);

            if (bTriggerVFX && bIsVisible && EffectArea && EffectArea->ImpactVFX)
            {
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(VisualContext.GetWorld(), EffectArea->ImpactVFX, EntityTransform.GetLocation());
                Impact.bImpactVFXTriggered = false;
                if (VisualContext.GetWorld()->GetNetMode() != NM_Client && EffectArea)
                {
                    EffectArea->bImpactVFXTriggered = false;
                }
            }
		}
	});

	// 2. Cleanup
	CleanupQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& CleanupContext)
	{
		const int32 NumEntities = CleanupContext.GetNumEntities();
		TArrayView<FEffectAreaVisualFragment> VisualList = CleanupContext.GetMutableFragmentView<FEffectAreaVisualFragment>();
		TArrayView<FMassActorFragment> ActorList = CleanupContext.GetMutableFragmentView<FMassActorFragment>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			FEffectAreaVisualFragment& Visual = VisualList[i];
			FMassActorFragment& ActorFrag = ActorList[i];

			if (Visual.ISMComponent.IsValid() && Visual.InstanceIndex != INDEX_NONE)
			{
				Visual.ISMComponent->UpdateInstanceTransform(Visual.InstanceIndex, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector), true, true, true);
				Visual.InstanceIndex = INDEX_NONE;
			}

			if (AActor* Actor = ActorFrag.GetMutable())
			{
				Actor->Destroy();
			}

			CleanupContext.Defer().DestroyEntity(CleanupContext.GetEntity(i));
		}
	});
}
