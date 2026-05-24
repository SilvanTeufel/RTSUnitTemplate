#include "Mass/Abilitys/EffectAreaVisualProcessor.h"
#include "Mass/Abilitys/EffectAreaVisualManager.h"
#include "MassReplicationFragments.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassEntityView.h"
#include "MassActorSubsystem.h"
#include "Mass/UnitMassTag.h"
#include "Actors/EffectArea.h"
#include "Kismet/GameplayStatics.h"
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
	VisualQuery.AddRequirement<FMassNetworkIDFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
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

		TConstArrayView<FMassNetworkIDFragment> NetIDList = VisualContext.GetFragmentView<FMassNetworkIDFragment>();
		bool bIsClient = VisualContext.GetWorld()->GetNetMode() == NM_Client;

		for (int32 i = 0; i < NumEntities; ++i)
		{
			AActor* AreaActor = ActorList[i].GetMutable();

			// 1. FILTER: On client, ignore entities without actors (naked bubble entities)
			// and ignore entities with NetID 0 (local placeholders)
			if (bIsClient)
			{
				if (!AreaActor)
				{
					continue;
				}
			}

			FEffectAreaVisualFragment& Visual = VisualList[i];
			FEffectAreaImpactFragment& Impact = ImpactList[i];
			const FMassVisibilityFragment& Visibility = VisibilityList[i];

			// 2. REFRESH OFFSETS: If replicated rotation offset changed, update relative transforms
			if (!Impact.VisualRotationOffset.Equals(Visual.LastAppliedRotationOffset))
			{
				FTransform VisualOffsetTransform(Impact.VisualRotationOffset);
				Visual.VisualRelativeTransform = Visual.BaseRelativeTransform * VisualOffsetTransform;
				Visual.Niagara_A_RelativeTransform = Visual.Niagara_A_BaseRelativeTransform * VisualOffsetTransform;
				Visual.LastAppliedRotationOffset = Impact.VisualRotationOffset;

			}
            
			// 3. TRANSFORM: Use smooth Actor transform on client if available
			FTransform BaseTransform;
			if (bIsClient && AreaActor)
			{
				BaseTransform = AreaActor->GetTransform();
				
			}
			else
			{
				BaseTransform = TransformList[i].GetTransform();
			}

			AEffectArea* EffectArea = Cast<AEffectArea>(AreaActor);

			bool bIsVisibleByFog = !Visibility.bAffectedByFogOfWar || Visibility.bIsMyTeam || Visibility.bIsVisibleEnemy;

			// If pending destruction and hide delay elapsed, hide once
			bool bIsHiddenByDestruction = Impact.bPendingDestruction && Impact.PostImpactTimer >= Impact.HideActorTime;
			if (bIsHiddenByDestruction && !Impact.bHasHiddenVisual)
			{
				Impact.bHasHiddenVisual = true;
			}

			bool bShouldShow = bIsVisibleByFog && Visibility.bIsOnViewport && !bIsHiddenByDestruction && !Impact.bHasHiddenVisual && (!AreaActor || !AreaActor->IsHidden());

			if (Visual.ISMComponent.IsValid() && Visual.InstanceIndex != INDEX_NONE)
			{
				if (bShouldShow)
				{
					float LocalRadius = Visual.BaseMeshRadius;
					float ScaleFactor = (LocalRadius > 0.f) ? (Impact.CurrentRadius / LocalRadius) : 1.f;

					if (bIsClient && (Impact.ElapsedTime < 5.0f || (Impact.ElapsedTime > 10.0f && Impact.ElapsedTime < 25.0f)))
					{
						bool bHasLoadingTag = VisualContext.DoesArchetypeHaveTag<FMassEffectAreaLoadingTag>();
						UE_LOG(LogTemp, Log, TEXT("[MassScaling] VisualProc: Entity %d, Radius=%.2f, Base=%.2f, Scale=%.2f, Visible=%d, HasLoadingTag=%d, ScalingActive=%d"), 
							i, Impact.CurrentRadius, LocalRadius, ScaleFactor, bShouldShow, bHasLoadingTag, Impact.bIsScalingAfterImpact);
					}


					FTransform VisualTransform = Visual.VisualRelativeTransform * BaseTransform;
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
                Visual.Niagara_A->SetWorldTransform(Visual.Niagara_A_RelativeTransform * BaseTransform);
                Visual.Niagara_A->SetVisibility(bShouldShow);
            }

            // Handle Impact VFX
            bool bTriggerVFX = Impact.bImpactVFXTriggered;

            if (bTriggerVFX && bIsVisibleByFog && EffectArea && EffectArea->ImpactVFX)
            {
                UWorld* World = VisualContext.GetWorld();
                if (World && World->GetNetMode() != NM_DedicatedServer)
                {
                    UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, EffectArea->ImpactVFX, BaseTransform.GetLocation());
                }
                Impact.bImpactVFXTriggered = false;
            }

			// Handle Spawn VFX / Sound
			if (!Impact.bSpawnEffectsTriggered && bIsVisibleByFog && EffectArea)
			{
                UWorld* World = VisualContext.GetWorld();
                if (World && World->GetNetMode() != NM_DedicatedServer)
                {
				    if (EffectArea->SpawnVFX)
				    {
					    UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, EffectArea->SpawnVFX, BaseTransform.GetLocation());
				    }
				    if (EffectArea->SpawnSound)
				    {
					    UGameplayStatics::PlaySoundAtLocation(World, EffectArea->SpawnSound, BaseTransform.GetLocation());
				    }
                }
				Impact.bSpawnEffectsTriggered = true;
			}
		}
	});

	// 2. Cleanup
	CleanupQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& CleanupContext)
	{
		const int32 NumEntities = CleanupContext.GetNumEntities();
		TArrayView<FEffectAreaVisualFragment> VisualList = CleanupContext.GetMutableFragmentView<FEffectAreaVisualFragment>();
		TArrayView<FMassActorFragment> ActorList = CleanupContext.GetMutableFragmentView<FMassActorFragment>();
		UEffectAreaVisualManager* VisualManager = CleanupContext.GetWorld()->GetSubsystem<UEffectAreaVisualManager>();

		for (int32 i = 0; i < NumEntities; ++i)
		{
			FEffectAreaVisualFragment& Visual = VisualList[i];
			FMassActorFragment& ActorFrag = ActorList[i];

			if (VisualManager && Visual.ISMComponent.IsValid() && Visual.InstanceIndex != INDEX_NONE)
			{
				VisualManager->RemoveVisualInstance(CleanupContext.GetEntity(i));
			}

			if (AActor* Actor = ActorFrag.GetMutable())
			{
				Actor->Destroy();
			}

			CleanupContext.Defer().DestroyEntity(CleanupContext.GetEntity(i));
		}
	});
}
