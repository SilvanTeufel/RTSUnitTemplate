// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Traits/UnitReplicationTrait.h"

#include "MassCommonFragments.h"
#include "MassReplicationFragments.h"
#include "MassEntityTraitBase.h"
#include "MassReplicationFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassClientBubbleInfoBase.h"       // Our new replicator
#include "MassClientBubbleHandler.h"         // Our new bubble
#include "MassReplicationSubsystem.h"
#include "MassEntityTemplate.h"
#include "Mass/Replication/ClientReplicationProcessor.h"
#include "Mass/Replication/MassUnitReplicatorBase.h"
#include "Mass/Traits/UnitReplicationFragments.h"
#include "Mass/Replication/UnitClientBubbleInfo.h"
#include "MassReplicationFragments.h"

void UUnitReplicationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
// 1. Add guard clause from the original
    if (World.IsNetMode(NM_Standalone) && !BuildContext.IsInspectingData())
    {
        return;
    }

    // 2. Add all the base replication fragments from the original
    FReplicationTemplateIDFragment& TemplateIDFragment = BuildContext.AddFragment_GetRef<FReplicationTemplateIDFragment>();
    TemplateIDFragment.ID = BuildContext.GetTemplateID();
    
    BuildContext.AddFragment<FMassNetworkIDFragment>();
    BuildContext.AddFragment<FMassReplicatedAgentFragment>();
    BuildContext.AddFragment<FMassReplicationViewerInfoFragment>();
    BuildContext.AddFragment<FMassReplicationLODFragment>();
    BuildContext.AddFragment<FMassReplicationGridCellLocationFragment>();

    // 3. Add your custom fragments
    BuildContext.AddFragment<FTransformFragment>();

    // Ensure the entity participates in the replication grid so the MassReplicationProcessor will process it
    BuildContext.AddTag<FMassInReplicationGridTag>();

    // Add replicated transform fragment (client-side storage for replicated transform)
    BuildContext.AddFragment<FUnitReplicatedTransformFragment>();
    // Replication shared fragment is configured at runtime in MassActorBindingComponent when building archetypes.
}
