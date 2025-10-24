// Fill out your copyright notice in the Description page of Project Settings.


#include "Mass/Traits/UnitAITrait.h"

#include "MassActorSubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "MassRepresentationFragments.h"
#include "Mass/UnitMassTag.h"
#include "Mass/UnitNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"
#include "Mass/Traits/UnitReplicationFragments.h"

void UUnitAITrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	//Super::BuildTemplate(BuildContext, World);
	// --- Fragments already added by 'Movement' Trait ---
	BuildContext.AddFragment<FTransformFragment>();
	BuildContext.AddFragment<FMassVelocityFragment>();
	BuildContext.AddFragment<FMassForceFragment>();
	BuildContext.AddFragment<FMassMoveTargetFragment>();
    
	// --- Fragments already added by 'Avoidance' Trait ---
	BuildContext.AddFragment<FMassSteeringFragment>();
	BuildContext.AddFragment<FMassAvoidanceColliderFragment>();
    
	// --- Fragments already added by 'Mass Actor Link' Trait ---
	BuildContext.AddFragment<FMassActorFragment>();
    
	// --- Fragments already added by 'Crowd Visualization' Trait ---
	BuildContext.AddFragment<FMassRepresentationFragment>();
	BuildContext.AddFragment<FMassRepresentationLODFragment>();

	// --- Fragments already added by 'Unit Mass Tag'/'Unit Nav Path' Traits ---
	BuildContext.AddTag<FUnitMassTag>();
	BuildContext.AddFragment<FUnitNavigationPathFragment>();
	
	// Core/Nav
	BuildContext.AddFragment<FAgentRadiusFragment>();
	BuildContext.AddFragment<FMassGhostLocationFragment>();

	// Your Custom Logic
	BuildContext.AddFragment<FMassPatrolFragment>(); 
	BuildContext.AddFragment<FMassAIStateFragment>();
	BuildContext.AddFragment<FMassSightFragment>();
	BuildContext.AddFragment<FMassAITargetFragment>(); 
	BuildContext.AddFragment<FMassCombatStatsFragment>(); 
	BuildContext.AddFragment<FMassAgentCharacteristicsFragment>();
	BuildContext.AddFragment<FMassChargeTimerFragment>();
	BuildContext.AddFragment<FMassWorkerStatsFragment>();

	// Ensure entities with AI also carry the client-side replicated transform fragment
	// This is safe even if UnitReplicationTrait also adds it; AddFragment is idempotent.
	BuildContext.AddFragment<FUnitReplicatedTransformFragment>();
}
