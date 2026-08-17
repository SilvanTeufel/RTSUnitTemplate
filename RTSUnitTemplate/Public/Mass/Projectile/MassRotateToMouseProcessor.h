// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "MassRotateToMouseProcessor.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UMassRotateToMouseProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	UMassRotateToMouseProcessor();
protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
private:
	void HandleMouseUpdateSignal(FName SignalName, TConstArrayView<FMassEntityHandle> Entities);
	FMassEntityQuery EntityQuery;

	// ================================================================================================
	// LUX-ANPASSUNG (17.08.2026) - war ein "static" in Execute() und damit die Ursache dafuer, dass
	// der Server die Client-Rotation manchmal gar nicht mehr uebernahm (Abhilfe war ein
	// Editor-Neustart). Ein static lebt so lange wie der PROZESS, die Weltzeit dagegen beginnt bei
	// jedem PIE-Start wieder bei 0. Nach einer laengeren Sitzung stand hier also eine Zeit in der
	// Zukunft, die Drosselung "CurrentTime - LastServerTickTime > 0.05" wurde nie wieder wahr, und
	// weil der Wert NUR in diesem Zweig aktualisiert wird, konnte er sich auch nicht mehr erholen.
	// Zusaetzlich teilten sich Server- und Clientwelt im PIE denselben static.
	// Als Member gehoert er jetzt zur jeweiligen Welt und startet mit ihr neu.
	// ================================================================================================
	float LastServerTickTime = 0.f;

	// DIAGNOSE [RotDiag] - Drosselung, ebenfalls Member (siehe oben).
	float LetzteRotDiagZeit = 0.f;
};
