#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UnitRegistryPayload.h"
#include "UnitRegistryReplicator.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API AUnitRegistryReplicator : public AActor
{
	GENERATED_BODY()
public:
	AUnitRegistryReplicator();

	UPROPERTY(ReplicatedUsing=OnRep_Registry)
	FUnitRegistryArray Registry;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_Registry();

	// Returns the per-world registry actor, spawning if needed (server only)
	static AUnitRegistryReplicator* GetOrSpawn(UWorld& World);
	
	// Server-only: reset NextNetID to 1 at game start
	void ResetNetIDCounter();

	// Server-only: get next NetID, starting at 1 for each game session
	uint32 GetNextNetID();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	uint32 NextNetID = 1; // not replicated; authoritative on server only
};