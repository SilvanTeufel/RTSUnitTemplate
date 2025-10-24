#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtr.h"
class FSubsystemCollectionBase;
class AUnitRegistryReplicator;
class AUnitClientBubbleInfo;
class UMassActorBindingComponent;

#include "RTSWorldCacheSubsystem.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API URTSWorldCacheSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	URTSWorldCacheSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Returns cached registry replicator; may spawn on server when allowed.
	AUnitRegistryReplicator* GetRegistry(bool bAllowSpawnOnServer = true);

	// Returns cached bubble info; may spawn on server when allowed.
	AUnitClientBubbleInfo* GetBubble(bool bAllowSpawnOnServer = true);

	// Rebuild OwnerName->BindingComponent cache if needed (throttled by interval seconds)
	void RebuildBindingCacheIfNeeded(float IntervalSeconds = 1.0f);

 // Find a binding component by stable owner name using the cache (call RebuildBindingCacheIfNeeded before heavy loops)
	UMassActorBindingComponent* FindBindingByOwnerName(FName OwnerName);
	// Find a binding by UnitIndex (preferred unique key)
	UMassActorBindingComponent* FindBindingByUnitIndex(int32 UnitIndex);

	// Clear caches explicitly
	void ClearAll();

private:
	TWeakObjectPtr<AUnitRegistryReplicator> CachedRegistry;
	TWeakObjectPtr<AUnitClientBubbleInfo> CachedBubble;
	TMap<FName, TWeakObjectPtr<UMassActorBindingComponent>> BindingByOwnerName;
	TMap<int32, TWeakObjectPtr<UMassActorBindingComponent>> BindingByUnitIndex;
	double LastBindingRebuildTime = -1000.0;
};