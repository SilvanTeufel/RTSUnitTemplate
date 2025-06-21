// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Widgets/MinimapWidget.h" // Change this path to match your file structure
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Actors/MinimapActor.h" // Change this path to match your file structure
#include "Controller/PlayerController/CustomControllerBase.h" // Change this path to match your file structure
/*
void UMinimapWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // --- 1. Find the correct MinimapActor for this player ---
    if (!GetWorld()) return;

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    ACustomControllerBase* CustomPC = Cast<ACustomControllerBase>(PC);
    if (!CustomPC)
    {
        UE_LOG(LogTemp, Warning, TEXT("MinimapWidget: Could not get CustomControllerBase."));
        return;
    }

    const int32 PlayerTeamId = CustomPC->SelectableTeamId;

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMinimapActor::StaticClass(), FoundActors);

    for (AActor* Actor : FoundActors)
    {
        AMinimapActor* MinimapActor = Cast<AMinimapActor>(Actor);
        if (MinimapActor && MinimapActor->TeamId == PlayerTeamId)
        {
            UE_LOG(LogTemp, Log, TEXT("Found MinimapActor!"));
            MinimapActorRef = MinimapActor;
            break;
        }
    }

    // --- 2. Bind the texture to our Image widget ---
    if (MinimapActorRef && MinimapImage)
    {
        MinimapImage->SetBrushFromTexture(MinimapActorRef->MinimapTexture);
        UE_LOG(LogTemp, Log, TEXT("MinimapWidget successfully bound texture!"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("MinimapWidget failed to find its MinimapActor or MinimapImage is not bound!"));
    }
}*/

void UMinimapWidget::InitializeForTeam(int32 TeamId)
{
    // --- 1. Find the correct MinimapActor for this player ---
    if (!GetWorld()) return;

    // Use the TeamId passed into this function
    const int32 PlayerTeamId = TeamId;

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMinimapActor::StaticClass(), FoundActors);

    for (AActor* Actor : FoundActors)
    {
        AMinimapActor* MinimapActor = Cast<AMinimapActor>(Actor);
        if (MinimapActor && MinimapActor->TeamId == PlayerTeamId)
        {
            MinimapActorRef = MinimapActor;
            break; // Found our actor, no need to search further
        }
    }

    // --- 2. Bind the texture to our Image widget ---
    if (MinimapActorRef && MinimapImage)
    {
        MinimapImage->SetBrushFromTexture(MinimapActorRef->MinimapTexture);
        UE_LOG(LogTemp, Log, TEXT("MinimapWidget successfully bound texture for Team ID: %d"), PlayerTeamId);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("MinimapWidget failed to find MinimapActor for Team ID %d or MinimapImage is not bound!"), PlayerTeamId);
    }
}

FReply UMinimapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // We only care about the left mouse button for moving the camera.
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // Get the position of the click in the widget's local space.
        FVector2D LocalMousePosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

        MoveCameraToMinimapLocation(LocalMousePosition, InGeometry);

        // Return FReply::Handled() to signify that we have processed this input event.
        return FReply::Handled();
    }

    // If it's any other button, let the event bubble up.
    return FReply::Unhandled();
}

void UMinimapWidget::MoveCameraToMinimapLocation(const FVector2D& LocalMousePosition, const FGeometry& WidgetGeometry)
{
    if (!MinimapActorRef) return;

    APawn* PlayerPawn = GetOwningPlayerPawn();
    if (!PlayerPawn) return;

    // --- Coordinate Transformation ---

    // Step 1: Get the size of the widget on screen.
    const FVector2D WidgetSize = WidgetGeometry.GetLocalSize();
    if (WidgetSize.X <= 0 || WidgetSize.Y <= 0) return;

    // Step 2: Calculate the UV coordinates of the click (0.0 to 1.0 range).
    const float U = FMath::Clamp(LocalMousePosition.X / WidgetSize.X, 0.f, 1.f);
    const float V = FMath::Clamp(LocalMousePosition.Y / WidgetSize.Y, 0.f, 1.f);

    // Step 3: Convert the UV coordinates to world space using the MinimapActor's bounds.
    const FVector2D& Min = MinimapActorRef->MinimapMinBounds;
    const FVector2D& Max = MinimapActorRef->MinimapMaxBounds;
    const float WorldExtentX = Max.X - Min.X;
    const float WorldExtentY = Max.Y - Min.Y;

    const float TargetWorldX = Min.X + (U * WorldExtentX);
    const float TargetWorldY = Min.Y + (V * WorldExtentY);

    // Step 4: Move the player's pawn to the new location.
    // We preserve the pawn's current Z height. A more advanced system might do a line trace
    // from above to find the ground height at the target location.
    const FVector CurrentLocation = PlayerPawn->GetActorLocation();
    const FVector NewLocation(TargetWorldX, TargetWorldY, CurrentLocation.Z);

    PlayerPawn->SetActorLocation(NewLocation);
}