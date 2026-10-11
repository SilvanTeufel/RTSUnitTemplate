// Copyright 2022 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GAS/GameplayAbilityBase.h"
#include "Core/WorkerData.h"
#include "ConversionSelectAbility.generated.h"

/**
 * Picks what a Converter building (BaseType = Converter) turns into what - one ability per
 * conversion, e.g. GA_Convert_PrimaryToSecondary. Instant: no cast, no cost.
 *
 * Switching hands the input still waiting to the output store (it is hauled away as what it is) and
 * sends supply that is on its way back to where it was loaded - see ABuildingBase::SetConversion.
 * The rates (amount in / out, interval) are set on the building, not here.
 */
UCLASS(Blueprintable)
class RTSUNITTEMPLATE_API UConversionSelectAbility : public UGameplayAbilityBase
{
	GENERATED_BODY()

public:
	/** The type the Converter is supplied with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics")
	EResourceType InputType = EResourceType::Primary;

	/** The type it produces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Logistics")
	EResourceType OutputType = EResourceType::Secondary;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
