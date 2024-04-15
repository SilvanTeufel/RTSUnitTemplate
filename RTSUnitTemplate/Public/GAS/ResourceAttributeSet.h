// ResourceAttributeSet.h

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ResourceAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS(BlueprintType)
class UResourceAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UResourceAttributeSet();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Gameplay Attributes for resources. One attribute for each resource type.
    UPROPERTY(BlueprintReadOnly, Category = "Resources", ReplicatedUsing = OnRep_PrimaryResource)
    FGameplayAttributeData PrimaryResource;
    ATTRIBUTE_ACCESSORS(UResourceAttributeSet, PrimaryResource)

    UPROPERTY(BlueprintReadOnly, Category = "Resources", ReplicatedUsing = OnRep_SecondaryResource)
    FGameplayAttributeData SecondaryResource;
    ATTRIBUTE_ACCESSORS(UResourceAttributeSet, SecondaryResource)

    UPROPERTY(BlueprintReadOnly, Category = "Resources", ReplicatedUsing = OnRep_TertiaryResource)
    FGameplayAttributeData TertiaryResource;
    ATTRIBUTE_ACCESSORS(UResourceAttributeSet, TertiaryResource)

    UPROPERTY(BlueprintReadOnly, Category = "Resources", ReplicatedUsing = OnRep_RareResource)
    FGameplayAttributeData RareResource;
    ATTRIBUTE_ACCESSORS(UResourceAttributeSet, RareResource)

    UPROPERTY(BlueprintReadOnly, Category = "Resources", ReplicatedUsing = OnRep_EpicResource)
    FGameplayAttributeData EpicResource;
    ATTRIBUTE_ACCESSORS(UResourceAttributeSet, EpicResource)

    UPROPERTY(BlueprintReadOnly, Category = "Resources", ReplicatedUsing = OnRep_LegendaryResource)
    FGameplayAttributeData LegendaryResource;
    ATTRIBUTE_ACCESSORS(UResourceAttributeSet, LegendaryResource)

public:
    // Worker counts for each resource type
    UPROPERTY(BlueprintReadOnly, Category = "Workers", ReplicatedUsing = OnRep_PrimaryWorkers)
    FGameplayAttributeData PrimaryWorkers;
    ATTRIBUTE_ACCESSORS(UResourceAttributeSet, PrimaryWorkers)

    UPROPERTY(BlueprintReadOnly, Category = "Workers", ReplicatedUsing = OnRep_SecondaryWorkers)
    FGameplayAttributeData SecondaryWorkers;
    ATTRIBUTE_ACCESSORS(UResourceAttributeSet, SecondaryWorkers)

    UPROPERTY(BlueprintReadOnly, Category = "Workers", ReplicatedUsing = OnRep_TertiaryWorkers)
    FGameplayAttributeData TertiaryWorkers;
    ATTRIBUTE_ACCESSORS(UResourceAttributeSet, TertiaryWorkers)

    UPROPERTY(BlueprintReadOnly, Category = "Workers", ReplicatedUsing = OnRep_RareWorkers)
    FGameplayAttributeData RareWorkers;
    ATTRIBUTE_ACCESSORS(UResourceAttributeSet, RareWorkers)

    UPROPERTY(BlueprintReadOnly, Category = "Workers", ReplicatedUsing = OnRep_EpicWorkers)
    FGameplayAttributeData EpicWorkers;
    ATTRIBUTE_ACCESSORS(UResourceAttributeSet, EpicWorkers)

    UPROPERTY(BlueprintReadOnly, Category = "Workers", ReplicatedUsing = OnRep_LegendaryWorkers)
    FGameplayAttributeData LegendaryWorkers;
    ATTRIBUTE_ACCESSORS(UResourceAttributeSet, LegendaryWorkers)

    
protected:
    // Replication functions for each attribute
    UFUNCTION()
    virtual void OnRep_PrimaryResource(const FGameplayAttributeData& OldPrimaryResource);
    UFUNCTION()
    virtual void OnRep_SecondaryResource(const FGameplayAttributeData& OldSecondaryResource);
    UFUNCTION()
    virtual void OnRep_TertiaryResource(const FGameplayAttributeData& OldTertiaryResource);
    UFUNCTION()
    virtual void OnRep_RareResource(const FGameplayAttributeData& OldRareResource);
    UFUNCTION()
    virtual void OnRep_EpicResource(const FGameplayAttributeData& OldEpicResource);
    UFUNCTION()
    virtual void OnRep_LegendaryResource(const FGameplayAttributeData& OldLegendaryResource);

protected:
    // Replication functions for worker attributes
    UFUNCTION()
    virtual void OnRep_PrimaryWorkers(const FGameplayAttributeData& OldPrimaryWorkers);
    UFUNCTION()
    virtual void OnRep_SecondaryWorkers(const FGameplayAttributeData& OldSecondaryWorkers);
    UFUNCTION()
    virtual void OnRep_TertiaryWorkers(const FGameplayAttributeData& OldTertiaryWorkers);
    UFUNCTION()
    virtual void OnRep_RareWorkers(const FGameplayAttributeData& OldRareWorkers);
    UFUNCTION()
    virtual void OnRep_EpicWorkers(const FGameplayAttributeData& OldEpicWorkers);
    UFUNCTION()
    virtual void OnRep_LegendaryWorkers(const FGameplayAttributeData& OldLegendaryWorkers);


    
};