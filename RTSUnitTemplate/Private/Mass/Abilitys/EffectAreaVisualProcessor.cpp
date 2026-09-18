// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
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
#include "Core/ViewportUtils.h"
#include "ProfilingDebugging/CsvProfiler.h"

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

	// Siehe mass_scopes: macht diesen Prozessor als Spalte Exclusive/UMassEffectAreaVisualProcessor im CSV sichtbar.
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UMassEffectAreaVisualProcessor);

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
					FTransform VisualTransform = Visual.VisualRelativeTransform * BaseTransform;

					// Hier wurde die Sichtgroesse bisher BEDINGUNGSLOS auf den Wirkradius gezogen -
					// Optik und Wirkbereich waren damit zwangslaeufig dieselbe Zahl, und ein
					// groesserer Ausloeseradius liess den Mesh mitwachsen. Das Flag dafuer gab es
					// laengst (AEffectArea::ScaleMesh), es wurde nur nie ausgewertet.
					//
					// Steht es auf false, bleibt die Skalierung stehen, die aus
					// VisualRelativeTransform kommt - und das ist der Relativtransform der
					// ISM-Komponente, also genau der Maszstab, den man im Blueprint sieht.
					if (Impact.bScaleMesh)
					{
						const float LocalRadius = Visual.BaseMeshRadius;
						const float ScaleFactor = (LocalRadius > 0.f) ? (Impact.CurrentRadius / LocalRadius) : 1.f;

						// Der Maszstab der ISM-Vorlage bleibt als Faktor JE ACHSE erhalten, statt
						// vom einheitlichen Radiusfaktor ueberschrieben zu werden. Vorher war jede
						// Flaeche zwangslaeufig so hoch wie breit - eine Kugel wurde zur vollen
						// Kugel statt zur flachen Haube, und ein (1,1,0.25) im Blueprint war
						// wirkungslos. Bei (1,1,1), also allen bestehenden Flaechen, aendert sich
						// nichts. X und Y decken weiterhin exakt den Wirkradius ab.
						const FVector TemplateScale = Visual.VisualRelativeTransform.GetScale3D();
						VisualTransform.SetScale3D(FVector(ScaleFactor) * TemplateScale);
					}

					// [FlaecheGroesse] Einmal je Flaeche: sagt, welche Skalierung wirklich angewandt
					// wurde und woher sie kommt. Ohne das bleibt "sieht zu gross aus" eine Vermutung -
					// die Zahl unterscheidet sauber zwischen "Flag kommt nicht an" (mitskaliert
					// obwohl es aus sein soll), "Mesh im Blueprint zu gross" (Skalierung stimmt, Bild
					// nicht) und "alte Instanz aus einer frueheren Sitzung".
					if (!Impact.bGroesseGemeldet)
					{
						Impact.bGroesseGemeldet = true;
						UE_LOG(LogTemp, Warning,
							TEXT("[FlaecheGroesse] %s: mitskalieren=%d angewandt=%.2f MeshRadius=%.0f Radius=%.0f (%.0f->%.0f)"),
							*GetNameSafe(AreaActor), Impact.bScaleMesh ? 1 : 0,
							VisualTransform.GetScale3D().X, Visual.BaseMeshRadius,
							Impact.CurrentRadius, Impact.StartRadius, Impact.EndRadius);
					}
					
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
                if (World && World->GetNetMode() != NM_DedicatedServer
                    && RTSViewportUtils::IsLocationOnLocalViewport(World, BaseTransform.GetLocation(), Visibility.VisibilityOffset))
                {
                    UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, EffectArea->ImpactVFX, BaseTransform.GetLocation());
                }
                Impact.bImpactVFXTriggered = false;
            }

			// Handle Spawn VFX / Sound
			if (!Impact.bSpawnEffectsTriggered && bIsVisibleByFog && EffectArea)
			{
                UWorld* World = VisualContext.GetWorld();
                // Same viewport cull the ISM and Niagara_A above already get through bShouldShow, but
                // projected live instead of read from the fragment: these are ONE-SHOTS. bShouldShow's
                // Visibility.bIsOnViewport is seeded true at bind (MassActorBindingComponent) and first
                // corrected by UUnitVisibilityProcessor up to a 20 Hz tick later - which for a persistent
                // component only costs one frame of being wrong, but for a fire-once burst is the whole
                // decision. The trigger flags below latch, so this runs once per area, never per frame.
                if (World && World->GetNetMode() != NM_DedicatedServer
                    && RTSViewportUtils::IsLocationOnLocalViewport(World, BaseTransform.GetLocation(), Visibility.VisibilityOffset))
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
