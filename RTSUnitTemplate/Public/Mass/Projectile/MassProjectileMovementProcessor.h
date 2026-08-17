// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "MassProjectileMovementProcessor.generated.h"

/**
 * Processor for projectile movement.
 */
UCLASS()
class RTSUNITTEMPLATE_API UMassProjectileMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassProjectileMovementProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;

	// Zaehler fuer [TrailDiag]. Bewusst KEIN static: der lebt so lange wie der Prozess und wuerde
	// nach acht Schuessen bis zum Editor-Neustart schweigen - derselbe Fehler, der die
	// Server-Rotation aussetzen liess (siehe MassRotateToMouseProcessor.h). Als Member gehoert er
	// zur Welt und beginnt mit jeder PIE-Sitzung neu.
	int32 TrailDiagFlugCount = 0;
};
