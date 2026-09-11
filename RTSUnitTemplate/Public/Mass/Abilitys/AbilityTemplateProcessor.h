// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "AbilityTemplateProcessor.generated.h"

/**
 * Traegt die im AbilityChooser gewaehlten Faehigkeiten auf Einheiten nach, die erst spaeter
 * genug Punkte haben.
 *
 * Laeuft bewusst langsam: die Vorlage aendert sich selten, und geprueft werden muss nur, ob
 * eine Einheit inzwischen Punkte hat. Alle paar Sekunden reicht dafuer voellig.
 *
 * bRequiresGameThreadExecution ist an, weil hier GAS-Daten fremder Actor angefasst werden -
 * das ist derselbe Grund, aus dem der CurrentArchetype-Assert frueher zugeschlagen hat.
 */
UCLASS()
class RTSUNITTEMPLATE_API UAbilityTemplateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UAbilityTemplateProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	/** Abstand zwischen zwei Durchlaeufen in Sekunden. */
	UPROPERTY(EditAnywhere, Category = "RTSUnitTemplate")
	float ExecutionInterval = 5.f;

private:
	FMassEntityQuery EntityQuery;
	float TimeSinceLastRun = 0.f;
};
