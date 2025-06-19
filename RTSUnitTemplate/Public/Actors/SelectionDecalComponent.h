// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/DecalComponent.h"
#include "SelectionDecalComponent.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTSUNITTEMPLATE_API USelectionDecalComponent : public UDecalComponent
{
	GENERATED_BODY()

public:
	USelectionDecalComponent();

protected:
	virtual void BeginPlay() override;

public:
	// Das Decal-Material, das im Blueprint zugewiesen wird. 
	// Dies sollte ein Material sein, dessen Domain auf "Deferred Decal" eingestellt ist.
	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection Decal")
	//UMaterialInterface* DecalMaterial;

	// Funktion, um das Decal anzuzeigen (z.B. mit einer bestimmten Farbe).
	UPROPERTY(EditAnywhere, Category = "Selection Decal")
	FLinearColor SelectionColor = FLinearColor::Green;
	
	UFUNCTION(BlueprintCallable, Category = "Selection Decal")
	void ShowSelection();

	// Funktion, um das Decal auszublenden.
	UFUNCTION(BlueprintCallable, Category = "Selection Decal")
	void HideSelection();

private:
	// Die eigentliche Decal-Komponente, die den Selektionskreis projiziert.
	//UPROPERTY(VisibleAnywhere, Category = "Selection Decal")
	//UDecalComponent* SelectionDecal;

	// Eine dynamische Instanz des Materials, um zur Laufzeit Parameter wie die Farbe zu ändern.
	UPROPERTY()
	UMaterialInstanceDynamic* DynamicDecalMaterial;
};