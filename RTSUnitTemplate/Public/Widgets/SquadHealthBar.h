// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/UnitBaseHealthBar.h"
#include "SquadHealthBar.generated.h"

/**
 * A squad-level health bar that aggregates health across all units in the same squad.
 * It inherits from UUnitBaseHealthBar to reuse bindings and visibility behavior.
 */
UCLASS()
class RTSUNITTEMPLATE_API USquadHealthBar : public UUnitBaseHealthBar
{
	GENERATED_BODY()
public:
	// If true, the squad healthbar is always visible (subject to viewport/fog), ignoring OpenHealthWidget.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	bool bAlwaysShowSquadHealthbar = true;

	// Override to aggregate squad health instead of a single unit
	virtual void UpdateWidget() override;

private:
	void ComputeSquadHealth(float& OutCurrentHealth, float& OutMaxHealth, float& OutCurrentShield, float& OutMaxShield) const;
};
