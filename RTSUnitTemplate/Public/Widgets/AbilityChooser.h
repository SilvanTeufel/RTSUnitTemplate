#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Characters/Unit/AbilityUnit.h"
#include "AbilityChooser.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UAbilityChooser : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(Transient)
	AAbilityUnit* OwnerAbilityUnit;

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	UPROPERTY(meta = (BindWidget))
	UTextBlock* OffensiveAbilityText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* DefensiveAbilityText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* AttackAbilityText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ThrowAbilityText;
	
	UPROPERTY(meta = (BindWidget))
	TArray<class UButton*> OffensiveAbilityButtons;

	UPROPERTY(meta = (BindWidget))
	TArray<class UButton*> DefensiveAbilityButtons;

	UPROPERTY(meta = (BindWidget))
	TArray<class UButton*> AttackAbilityButtons;

	UPROPERTY(meta = (BindWidget))
	TArray<class UButton*> ThrowAbilityButtons;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* AvailableAbilityPointsText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* UsedAbilityPointsText;

	UPROPERTY(meta = (BindWidget))
	TArray<class UTextBlock*> UsedAbilityPointsTextArray;
	
	UPROPERTY(EditAnywhere, meta = (BindWidget))
	int ButtonInitCount = 4;

	UPROPERTY(EditAnywhere, meta = (BindWidget))
	TArray<FString> ButtonPreFixes = {"Offensive", "Defensive", "Attack", "Throw"};
	
	void SetOwnerActor(AAbilityUnit* NewOwner);
	void UpdateAbilityDisplay();
	void InitializeButtonArray(const FString& ButtonPrefix, TArray<UButton*>& ButtonArray);
	// Utility function to get enum value as string
	static FString GetEnumValueAsString(const FString& EnumName, int32 EnumValue);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	ALevelUnit* GetOwnerActor() {
		return OwnerAbilityUnit;
	}
	
};