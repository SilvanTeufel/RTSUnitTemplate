#include "Mass/Replication/ReplicationSettings.h"
#include "HAL/IConsoleManager.h"

// Master CVAR to select the replication mode
// 0 = Standard Actor replication (AUnitBase SetReplicateMovement(true))
// 1 = Custom Mass replication pipeline

static TAutoConsoleVariable<int32> CVarRTS_ReplicationMode(
	TEXT("net.RTS.ReplicationMode"),
	0,
	TEXT("Select replication mode: 0=Standard Actor replication, 1=Custom Mass replication."),
	ECVF_Default);

namespace RTSReplicationSettings
{
	int32 GetReplicationMode()
	{
		return 1;
		// Read the CVAR value on the game thread. Default to Standard (0) if not available.
		//IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("net.RTS.ReplicationMode"));
		//return Var ? Var->GetInt() : 0;
	}
}
