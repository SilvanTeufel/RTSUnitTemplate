// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PCGCollisionDisabler.generated.h"

/**
 * Schaltet die Kollision der von PCG erzeugten Instanzen ab - kartenweise.
 *
 * Hintergrund: PCG legt seine Meshes in InstancedStaticMesh-Komponenten ab, deren
 * Kollision weder am PCGComponent noch im Graphen einstellbar ist
 * (PCGSoftISMComponentDescriptor legt kein Kollisionsfeld offen, und die Graphen sind
 * zwischen den Karten geteilt). Wo die Bepflanzung nur schmuecken und nicht blockieren
 * soll, raeumt dieser Aktor sie nach dem Start still ab.
 *
 * Erkannt wird PCG am Klassennamen des Besitzers - so bleibt das Plugin frei von einer
 * Modulabhaengigkeit auf PCG.
 */
UCLASS()
class RTSUNITTEMPLATE_API APCGCollisionDisabler : public AActor
{
	GENERATED_BODY()

public:
	APCGCollisionDisabler();

	virtual void BeginPlay() override;

	/** Sofort anwenden - im Editor ueber den Knopf in den Details pruefbar. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "RTSUnitTemplate|PCG")
	int32 KollisionJetztAbschalten();

	/** Zusaetzlich beim Laden im Editor anwenden, damit man es sofort sieht. */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate|PCG")
	bool bAuchImEditor = true;

	/** Nur Besitzer, deren Klassenname diesen Teil enthaelt. */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate|PCG")
	FString BesitzerFilter = TEXT("PCG");
};
