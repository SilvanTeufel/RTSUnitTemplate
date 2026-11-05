// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MinimapWidget.generated.h"

// Forward declarations to avoid including heavy headers.
class UImage;
class AMinimapActor;
class USoundBase;

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

	/** Keeps the game's cursor (e.g. the crosshair) over the minimap instead of the system arrow. */
	virtual FCursorReply NativeOnCursorQuery(const FGeometry& InGeometry, const FPointerEvent& InCursorEvent) override;

	/** The Image component from the UMG Designer that will display the minimap.
	 * The name "MinimapImage" MUST match the name of the Image widget in your UMG Blueprint.
	 */
	UPROPERTY(meta = (BindWidget))
	UImage* MinimapImage;

	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

public:
	/** Alarm for a new attack alert of the owning controller (ACustomControllerBase::ReportUnderAttack). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|AttackAlert")
	USoundBase* UnderAttackSound = nullptr;

	/** No alarm sound while the attacked spot is on screen - the player sees the fight already. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|AttackAlert")
	bool bSilentWhenOnScreen = true;

	/** Least time between two alarm sounds (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|AttackAlert", meta = (ClampMin = "0"))
	float UnderAttackSoundCooldown = 6.f;

	/** Colour of the two brackets that close in on the attacked spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|AttackAlert")
	FLinearColor AttackAlertColor = FLinearColor(1.f, 0.12f, 0.08f, 1.f);

	/** Half the brackets' height at the start of a pulse, as a fraction of the map image size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|AttackAlert", meta = (ClampMin = "0.01"))
	float AttackAlertStartSize = 0.16f;

	/** Half the brackets' height at the end of a pulse, as a fraction of the map image size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|AttackAlert", meta = (ClampMin = "0"))
	float AttackAlertEndSize = 0.02f;

	/** One closing of the brackets (seconds); repeats for the alert's duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|AttackAlert", meta = (ClampMin = "0.1"))
	float AttackAlertPulseTime = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minimap|AttackAlert", meta = (ClampMin = "0.5"))
	float AttackAlertThickness = 2.5f;

protected:

private:
	/**
	 * Converts a click position on the map image to a world location and moves the camera.
	 * @param LocalMousePosition The mouse position on the map image, unrotated (see GetMapLocalPosition).
	 * @param MapSize The map image's size in the same space.
	 */
	void MoveCameraToMinimapLocation(const FVector2D& LocalMousePosition, const FVector2D& MapSize);

	/**
	 * The mouse position on MinimapImage itself, before its rotation, and the image's size. The image need not
	 * fill the widget (a frame around it, e.g. BP_MinimapWidget_Xeno_AH: a SizeBox in a GridPanel). Without
	 * MinimapImage: the whole widget.
	 */
	void GetMapLocalPosition(const FGeometry& InGeometry, const FVector2D& ScreenPosition, FVector2D& OutLocal, FVector2D& OutSize) const;

	void RetryInitialize();
	
	UPROPERTY()
	int32 PendingTeamId = -1;

	FTimerHandle RetryTimerHandle;

	UPROPERTY()
	float CurrentMapAngle = -90.f;

	/** Newest alert id the alarm has been played (or skipped) for. */
	int32 LastAlertIdHandled = 0;
	double LastAlertSoundTime = -1000.0;
	/** A cached pointer to the MinimapActor this widget is displaying. */
	UPROPERTY()
	AMinimapActor* MinimapActorRef;
};