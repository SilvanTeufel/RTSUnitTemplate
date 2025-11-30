// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Characters/Unit/ConstructionUnit.h"
#include "Net/UnrealNetwork.h"
#include "Actors/WorkArea.h"

AConstructionUnit::AConstructionUnit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Name = TEXT("ConstructionSite");
	// Likely stationary
	CanMove = false;
}

void AConstructionUnit::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AConstructionUnit, Worker);
	DOREPLIFETIME(AConstructionUnit, WorkArea);
}
