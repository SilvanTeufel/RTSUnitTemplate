// Copyright 2023 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include <Components/TextBlock.h>
#include "DamageIndicator.generated.h"

/**
 * 
 */
UCLASS()
class RTSUNITTEMPLATE_API UDamageIndicator : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetDamage(float NewDamage) {
		Damage = NewDamage;
	}

protected:
	void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY()
	float Damage;

	UPROPERTY()
	float Opacity = 1.0f;
	
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* Indicator;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Indicator")
	float MaxDamage = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Indicator")
	float MinDamage = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Indicator")
	float MaxTextSize = 24.f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Indicator")
	float MinTextSize = 8.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Damage Indicator")
	FLinearColor HighDamageColor = FLinearColor::Red;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Indicator")
	FLinearColor LowDamageColor = FLinearColor::White;
	
	float CalculateOpacity();
	float CalculateTextSize();
	FLinearColor CalculateTextColor();
};
