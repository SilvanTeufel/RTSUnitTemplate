#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtr.h"
#include "RTSWorldCacheSubsystem.generated.h"

class AUnitRegistryReplicator;
class AUnitClientBubbleInfo;
class UMassActorBindingComponent;

UCLASS()
class RTSUNITTEMPLATE_API URTSWorldCacheSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	URTSWorldCacheSubsystem();

	virtual void Deinitialize() override;

	// Returns cached registry replicator; may spawn on server when allowed.
	AUnitRegistryReplicator* GetRegistry(bool bAllowSpawnOnServer = true);

	// Returns cached bubble info; may spawn on server when allowed.
	AUnitClientBubbleInfo* GetBubble(bool bAllowSpawnOnServer = true);

	// Rebuild OwnerName->BindingComponent cache if needed (throttled by interval seconds)
	void RebuildBindingCacheIfNeeded(float IntervalSeconds = 1.0f);

	// Find a binding component by stable owner name using the cache (call RebuildBindingCacheIfNeeded before heavy loops)
	UMassActorBindingComponent* FindBindingByOwnerName(FName OwnerName);

	// Clear caches explicitly
	void ClearAll();

private:
	TWeakObjectPtr<AUnitRegistryReplicator> CachedRegistry;
	TWeakObjectPtr<AUnitClientBubbleInfo> CachedBubble;
	TMap<FName, TWeakObjectPtr<UMassActorBindingComponent>> BindingByOwnerName;
	double LastBindingRebuildTime = -1000.0;
};