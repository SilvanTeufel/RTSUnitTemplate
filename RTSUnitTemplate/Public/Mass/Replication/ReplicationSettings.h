#pragma once
#include "CoreMinimal.h"

namespace RTSReplicationSettings
{
	// Replication mode selector
	// 0 = Standard UE Actor replication (SetReplicateMovement on AUnitBase)
	// 1 = Custom Mass-based replication pipeline (UnitRegistry/Bubble/Processors)
	enum EMode : int32
	{
		Standard = 0,
		Mass = 1
	};

	// Returns current replication mode. Defaults to Standard (0) if CVAR not found.
	RTSUNITTEMPLATE_API int32 GetReplicationMode();
}
