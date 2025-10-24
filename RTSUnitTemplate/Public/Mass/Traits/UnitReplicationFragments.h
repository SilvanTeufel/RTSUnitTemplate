#pragma once
#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "UnitReplicationFragments.generated.h"

// This is the fragment that will actually be sent over the network.
// It's stored on the client and updated by the replication processor.
USTRUCT()
struct RTSUNITTEMPLATE_API FUnitReplicatedTransformFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	FTransform Transform;
};