// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "MassEntityTypes.h"
#include "UObject/WeakObjectPtr.h"
#include "MassUnitHoverProcessor.generated.h"

class AMassUnitBase;
class USkeletalMeshComponent;
class UMassSignalSubsystem;

/**
 * Processor that handles hover detection for Mass units and buildings.
 * Runs on the local client and triggers CustomOverlapStart/End on the unit actors via signals.
 */
UCLASS()
class RTSUNITTEMPLATE_API UMassUnitHoverProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassUnitHoverProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void BeginDestroy() override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	UPROPERTY()
	FMassEntityHandle LastHoveredEntity;

	UPROPERTY()
	UMassSignalSubsystem* SignalSubsystem = nullptr;

	FMassEntityQuery EntityQuery;
	float AccumulatedTime = 0.f;

	/**
	 * Abstand zwischen zwei Hover-Pruefungen in Sekunden.
	 *
	 * Lag bis zum 21.09.2026 fest bei 0,1 s (10 Hz). Das sind bis zu 100 ms zwischen
	 * Mausbewegung und Erkennung - als Traegheit spuerbar, und genau das war die Beschwerde.
	 * 0,05 s halbiert die Wartezeit; zusammen mit dem Ausschluss toter Entitaeten (siehe
	 * ConfigureQueries) kostet der Prozessor trotzdem nicht mehr als vorher.
	 *
	 * Zur Laufzeit ueber rts.hover.interval ueberschreibbar, damit sich der Preis messen laesst,
	 * ohne neu zu bauen.
	 */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate|Performance")
	float HoverUpdateInterval = 0.05f;

	/** Zeitgeber der Diagnose aus rts.hover.diag. */
	float HoverDiagTime = 0.f;

	/** Letzte Messung: gepruefte Entitaeten und Dauer. Siehe rts.hover.diag. */
	int32 LastCheckedEntities = 0;
	float LastCheckMilliseconds = 0.f;

	UFUNCTION()
	void HandleCustomOverlapStart(FName SignalName, TArray<FMassEntityHandle>& Entities);

	UFUNCTION()
	void HandleCustomOverlapEnd(FName SignalName, TArray<FMassEntityHandle>& Entities);

	FDelegateHandle CustomOverlapStartDelegateHandle;
	FDelegateHandle CustomOverlapEndDelegateHandle;

	UPROPERTY(EditAnywhere, Category = "RTS|Hover")
	bool bSetCustomDataValue = true;
};
