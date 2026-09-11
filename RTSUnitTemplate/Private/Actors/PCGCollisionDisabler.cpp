// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#include "Actors/PCGCollisionDisabler.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

APCGCollisionDisabler::APCGCollisionDisabler()
{
	PrimaryActorTick.bCanEverTick = false;
}

void APCGCollisionDisabler::BeginPlay()
{
	Super::BeginPlay();
	KollisionJetztAbschalten();
}

int32 APCGCollisionDisabler::KollisionJetztAbschalten()
{
	UWorld* Welt = GetWorld();
	if (!Welt)
	{
		return 0;
	}

	int32 Komponenten = 0;
	int32 Besitzer = 0;
	for (TActorIterator<AActor> It(Welt); It; ++It)
	{
		AActor* A = *It;
		if (!A || !A->GetClass()->GetName().Contains(BesitzerFilter))
		{
			continue;
		}

		TArray<UInstancedStaticMeshComponent*> Instanzen;
		A->GetComponents(Instanzen);
		if (Instanzen.Num() == 0)
		{
			continue;
		}
		++Besitzer;
		for (UInstancedStaticMeshComponent* C : Instanzen)
		{
			if (!C || C->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
			{
				continue;
			}
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			++Komponenten;
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("[PCG-Kollision] %d Instanzkomponenten auf %d Traegern abgeschaltet (Filter '%s')."),
		Komponenten, Besitzer, *BesitzerFilter);
	return Komponenten;
}
