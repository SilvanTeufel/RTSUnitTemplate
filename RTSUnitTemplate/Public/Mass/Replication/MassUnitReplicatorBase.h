// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MassReplicationProcessor.h"
#include "MassUnitReplicatorBase.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UMassUnitReplicatorBase : public UMassReplicatorBase
{
	GENERATED_BODY()
public:
	// This function tells the replication system what fragments we need to read/write.
	virtual void AddRequirements(FMassEntityQuery& EntityQuery) override;

	// Called on server to add entity to replication list
	void AddEntity(FMassEntityHandle Entity, FMassReplicationContext& ReplicationContext);

	// Called on server to remove entity from replication list
	void RemoveEntity(FMassEntityHandle Entity, FMassReplicationContext& ReplicationContext);

	// Server-side: Serialize entity data for replication
	virtual void ProcessClientReplication(FMassExecutionContext& Context, FMassReplicationContext& ReplicationContext) override;
};
