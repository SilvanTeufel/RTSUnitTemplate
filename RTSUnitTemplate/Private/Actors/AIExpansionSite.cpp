// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/AIExpansionSite.h"
#include "Components/BillboardComponent.h"

const FName AAIExpansionSite::ExpansionSiteTag(TEXT("AIExpansionSite"));

AAIExpansionSite::AAIExpansionSite()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	Billboard->SetupAttachment(SceneRoot);
	Billboard->SetHiddenInGame(true);
#endif

	// Die Expansionslogik sucht ueber den Tag, nicht ueber die Klasse - so bleiben von Hand
	// getaggte Actors aus aelteren Leveln weiterhin gueltig.
	Tags.AddUnique(ExpansionSiteTag);
}

bool AAIExpansionSite::IsTeamAllowed(int32 TeamId) const
{
	// Leere Liste heisst bewusst "offen fuer alle": ein frisch gesetzter Marker soll wirken,
	// ohne dass man erst Teams eintragen muss.
	return AllowedTeamIds.IsEmpty() || AllowedTeamIds.Contains(TeamId);
}

bool AAIExpansionSite::IsTeamAllowedForActor(const AActor* Marker, int32 TeamId)
{
	if (!IsValid(Marker))
	{
		return false;
	}

	if (const AAIExpansionSite* Platz = Cast<AAIExpansionSite>(Marker))
	{
		return Platz->IsTeamAllowed(TeamId);
	}

	// Nur getaggt, keine eigene Klasse: unveraendertes Verhalten, jedes Team darf.
	return true;
}
