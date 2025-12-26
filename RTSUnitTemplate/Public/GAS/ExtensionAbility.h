// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GAS/GameplayAbilityBase.h"
#include "ExtensionAbility.generated.h"

class USoundBase;
class AWorkArea;
class ABuildingBase;
class AUnitBase;

UCLASS()
class RTSUNITTEMPLATE_API UExtensionAbility : public UGameplayAbilityBase
{
	GENERATED_BODY()
public:
	UExtensionAbility();

	// Offset for placing the extension work area relative to the executing building's Mass location
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	FVector ExtensionOffset = FVector(100.f, 100.f, 0.f);

	// Sound to play locally on cancel
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	USoundBase* CancelSound = nullptr;

	// Class of the WorkArea to spawn for the extension build site
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<AWorkArea> WorkAreaClass;

 // Optional: a construction site unit to visualize building progress
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TSubclassOf<AUnitBase> ConstructionUnitClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool AllowAddingWorkers = false;
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// Ensure delegate cleanup when the ability ends for any reason
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// Helper: compute target placement location from offset or by tracing to ground
	bool ComputePlacementLocation(class AUnitBase* Unit, FVector& OutLocation) const;

	// Helper: validate area by checking overlap with other WorkAreas/Buildings (ignoring the creator)
	bool IsPlacementBlocked(AActor* IgnoredActor, const FVector& TestLocation, float TestRadius, TArray<AActor*>& OutBlockingActors) const;

	// Tracked weak pointers for cleanup and state rollback
	UPROPERTY(Transient)
	TWeakObjectPtr<AWorkArea> TrackedWorkArea;
	UPROPERTY(Transient)
	TWeakObjectPtr<AUnitBase> TrackedUnit;

	bool bEndedByWorkArea = false;
};
