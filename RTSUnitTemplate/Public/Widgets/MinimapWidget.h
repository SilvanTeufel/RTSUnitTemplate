// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MinimapWidget.generated.h"

// Forward declarations to avoid including heavy headers.
class UImage;
class AMinimapActor;

/**
 * C++-driven UMG widget for the minimap.
 * Handles finding the correct MinimapActor for the player, binding its texture,
 * and processing clicks to move the camera pawn.
 */
UCLASS()
class RTSUNITTEMPLATE_API UMinimapWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Minimap")
	void InitializeForTeam(int32 TeamId);
protected:
	/**
	 * This is the C++ equivalent of the OnInitialized event in Blueprints.
	 * We'll use this to find our MinimapActor and bind its texture.
	 */
	// virtual void NativeOnInitialized() override;
	/**
	 * Captures mouse down events on this widget. We'll use this to detect clicks.
	 * @param InGeometry The geometry of the widget.
	 * @param InMouseEvent The mouse event that occurred.
	 * @return A reply indicating whether the event was handled.
	 */
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** The Image component from the UMG Designer that will display the minimap.
	 * The name "MinimapImage" MUST match the name of the Image widget in your UMG Blueprint.
	 */
	UPROPERTY(meta = (BindWidget))
	UImage* MinimapImage;

private:
	/**
	 * Converts a click position on the widget to a world location and moves the camera.
	 * @param LocalMousePosition The mouse position in the widget's local coordinate space.
	 * @param WidgetGeometry The geometry of the widget at the time of the click.
	 */
	void MoveCameraToMinimapLocation(const FVector2D& LocalMousePosition, const FGeometry& WidgetGeometry);

	UPROPERTY()
	float CurrentMapAngle = -90.f;
	/** A cached pointer to the MinimapActor this widget is displaying. */
	UPROPERTY()
	AMinimapActor* MinimapActorRef;
};