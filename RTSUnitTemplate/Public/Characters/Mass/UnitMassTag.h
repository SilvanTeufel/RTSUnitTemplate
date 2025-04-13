// MyUnitFragments.h
#pragma once

#include "MassEntityTypes.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "UnitMassTag.generated.h"

USTRUCT()
struct FUnitMassTag : public FMassTag
{
	GENERATED_BODY()
};

// Position + movement are handled by built-in fragments like FMassMovementFragment, so no need to add here.
