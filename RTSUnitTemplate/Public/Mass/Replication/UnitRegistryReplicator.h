#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UnitRegistryPayload.h"
// Forward declare to avoid including TimerManager in header
struct FTimerHandle;
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

	// Server-only: block a NetID from being reused for NetIDQuarantineTime seconds
	void QuarantineNetID(uint32 NetID);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTS|Replication")
	float NetIDQuarantineTime = 15.0f;
	
	// Client-side: track recent registry updates to debounce reconcile-unlink (plain members; not replicated)
	int32 ClientOnRepCounter = 0;
	double ClientLastOnRepTime = 0.0;
	
	// Check if all live units in the world are registered (useful for startup validation)
	// Returns true if all non-dead units have a corresponding registry entry
	UFUNCTION(BlueprintCallable, Category = "RTS|Replication")
	bool AreAllUnitsRegistered() const;
	
	// Get registration progress as a ratio (0.0 to 1.0)
	// Returns the number of registered units divided by total live units
	UFUNCTION(BlueprintCallable, Category = "RTS|Replication")
	float GetRegistrationProgress() const;
	
	// Get counts for diagnostics: OutRegistered = units in registry, OutTotal = live units in world
	UFUNCTION(BlueprintCallable, Category = "RTS|Replication")
	void GetRegistrationCounts(int32& OutRegistered, int32& OutTotal) const;
	
protected:
	virtual void BeginPlay() override;
	
	// Server-only periodic diagnostics to detect unregistered Units on the field
	void ServerDiagnosticsTick();

private:
	// Periodic diagnostics timer (server-only)
	FTimerHandle DiagnosticsTimerHandle;
	
	UPROPERTY(Transient)
	uint32 NextNetID = 1; // not replicated; authoritative on server only

	// IDs that cannot be reused yet (NetID -> ExpirationTime)
	TMap<uint32, double> QuarantinedNetIDs;
};