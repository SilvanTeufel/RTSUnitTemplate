// Copyright 2026 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Characters/Unit/AbilityUnit.h"
#include "Controller/PlayerController/CustomControllerBase.h"
#include "AbilityChooser.generated.h"

UCLASS()
class RTSUNITTEMPLATE_API UAbilityChooser : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(Transient)
	AAbilityUnit* OwnerAbilityUnit;

private:
	
	const float UpdateInterval = 1.0f;

	FTimerHandle UpdateTimerHandle;

public:
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StartUpdateTimer();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void StopTimer();
	
	virtual void NativeConstruct() override;

	void InitWidget(ACustomControllerBase* InController);
	//virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// Interval in seconds for how often to update the resource display
	
	UPROPERTY(meta = (BindWidget))
	UTextBlock* OffensiveAbilityText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* DefensiveAbilityText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* AttackAbilityText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ThrowAbilityText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	TArray<class UImage*> SelectAbilityIcons;
	
	UPROPERTY(meta = (BindWidget))
	TArray<class UButton*> OffensiveAbilityButtons;

	UPROPERTY(meta = (BindWidget))
	TArray<class UButton*> DefensiveAbilityButtons;

	UPROPERTY(meta = (BindWidget))
	TArray<class UButton*> AttackAbilityButtons;

	UPROPERTY(meta = (BindWidget))
	TArray<class UButton*> ThrowAbilityButtons;

	UPROPERTY(meta = (BindWidget))
	TArray<class UAbilityButton*> SelectAbilityButtons;
	
	UPROPERTY(meta = (BindWidget))
	UTextBlock* AvailableAbilityPointsText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* UsedAbilityPointsText;

	UPROPERTY(meta = (BindWidget))
	TArray<class UTextBlock*> UsedAbilityPointsTextArray;
	
	UPROPERTY(EditAnywhere, meta = (BindWidget), Category=RTSUnitTemplate)
	int ButtonInitCount = 4;

	UPROPERTY(EditAnywhere, meta = (BindWidget), Category=RTSUnitTemplate)
	int SelectableAbilityCount = 35;
	
	UPROPERTY(EditAnywhere, meta = (BindWidget), Category=RTSUnitTemplate)
	TArray<FString> ButtonPreFixes = {"Offensive", "Defensive", "Attack", "Throw"};
	
	void SetOwnerActor(AAbilityUnit* NewOwner);
	
	void UpdateAbilityDisplay();
	
	void InitializeButtonArray(const FString& ButtonPrefix, TArray<UButton*>& ButtonArray);
	
	void InitializeAbilityIconArray(const FString& Prefix, TArray<UImage*>& IconArray, TArray<UAbilityButton*>& ButtonArray);
	// Utility function to get enum value as string
	static FString GetEnumValueAsString(const FString& EnumName, int32 EnumValue);

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	AAbilityUnit* GetOwnerActor() {
		return OwnerAbilityUnit;
	}

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetAbilityIcons();

	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void SetVisibleAbilityButtonCount();
	
	UFUNCTION(BlueprintCallable, Category = RTSUnitTemplate)
	void ClearAbilityArray();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = RTSUnitTemplate)
	ACustomControllerBase* ControllerBase;
	
	// --- Vergabe je Tierklasse (02.09.2026) ------------------------------------------------
	// Bisher wirkte der Chooser nur auf die eine ausgewaehlte Einheit. Mit den Tierklassen-Tags
	// Units.Tier.1-4 an den Einheiten laesst sich stattdessen eine ganze Klasse auf einmal
	// ausstatten. Standard ist T1.

	/** 1-4. Entspricht Units.Tier.N. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Tier")
	int32 SelectedTier = 1;

	/** Text unter den Knoepfen. Die Auswahl ist eine Vorlage: sie gilt fuer die ganze Klasse
	 *  und wird auf spaeter gebaute Einheiten automatisch nachgetragen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Tier")
	FText TierHintText = NSLOCTEXT("RTSUnitTemplate", "TierHint",
		"Applies to every unit of this class - new units get it automatically.");

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Tier")
	void SetSelectedTier(int32 Tier);

	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Tier")
	int32 GetSelectedTier() const { return SelectedTier; }

	/** Der Tag zur aktuellen Auswahl, z.B. Units.Tier.1. */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Tier")
	FGameplayTag GetSelectedTierTag() const;

	/** Fuer das Ausgrauen der Knoepfe: hat ueberhaupt eine Einheit dieser Klasse Punkte? */
	UFUNCTION(BlueprintPure, Category = "RTSUnitTemplate|Tier")
	bool IsTierSpendable(int32 Tier) const;

	/** Vergibt die Faehigkeit an die gesamte gewaehlte Tierklasse. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Tier")
	void SpendAbilityForSelectedTier(EGASAbilityInputID AbilityID, int32 AbilityIndex);

	/** Graut Knoepfe ohne bedienbare Einheiten aus und setzt den Hinweistext. */
	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Tier")
	void RefreshTierButtons();

	// --- Slot-Knoepfe: Icon statt Zahl, Tooltip, sichtbare Auswahl (02.09.2026) -----------
	// Die Knoepfe Offensive_0..3 / Defensive_0..3 / Attack_0..3 / Throw_0..3 trugen nur die
	// Ziffern 1-4. Jeder bekommt jetzt das Icon der Faehigkeit, die er belegt, einen Tooltip
	// und - wenn er der gewaehlte Index ist - eine sichtbare Hervorhebung.

	/** Akzentfarbe der Fraktion; wird im Widget-Blueprint gesetzt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Style")
	FLinearColor AccentColor = FLinearColor(0.114f, 0.541f, 0.918f, 1.f);

	/** Farbe eines Slots, der keine Faehigkeit traegt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSUnitTemplate|Style")
	FLinearColor EmptySlotColor = FLinearColor(0.10f, 0.12f, 0.15f, 1.f);

	/** Fuellung der Knoepfe. Dunkel und in BEIDEN Zustaenden gleich - hervorgehoben wird ueber den Rahmen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityChooser|Auswahl")
	FLinearColor SlotFillColor = FLinearColor(0.05f, 0.06f, 0.08f, 0.92f);

	/** Rahmenstaerke des gewaehlten Knopfes in Pixeln. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityChooser|Auswahl", meta = (ClampMin = "0.0"))
	float SelectionOutlineWidth = 3.f;

	/** Eckenradius der Knoepfe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AbilityChooser|Auswahl", meta = (ClampMin = "0.0"))
	float SlotCornerRadius = 6.f;

	/**
	 * Gibt einem Knopf einen dunklen Grund und - wenn gewaehlt - einen Rahmen in der Akzentfarbe.
	 *
	 * Vorher wurde der gewaehlte Knopf VOLLFLAECHIG in der Akzentfarbe eingefaerbt. Das Icon lag
	 * darauf und ging darin unter; gemeldet am 10.09.2026 als "kaum zu erkennen was ausgewaehlt
	 * wurde". Ein Rahmen laesst die Mitte frei, also bleibt das Icon die Hauptsache.
	 */
	void WendeAuswahlrahmenAn(class UButton* Knopf, bool bGewaehlt);

	UFUNCTION(BlueprintCallable, Category = "RTSUnitTemplate|Style")
	void RefreshSlotButtons();

};