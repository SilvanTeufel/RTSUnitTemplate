// Copyright 2025 Silvan Teufel / Teufel-Engineering.com All Rights Reserved.
#include "Widgets/MinimapWidget.h" // Change this path to match your file structure
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"
#include "Actors/MinimapActor.h" // Change this path to match your file structure
#include "Characters/Camera/ExtendedCameraBase.h"
#include "Controller/PlayerController/CameraControllerBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Controller/PlayerController/CustomControllerBase.h" // Change this path to match your file structure
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Sound/SoundBase.h"

void UMinimapWidget::InitializeForTeam(int32 TeamId)
{
    PendingTeamId = TeamId;
    
    // --- 1. Find the correct MinimapActor for this player ---
    if (!GetWorld()) return;

    // A new team (e.g. switched at runtime) must not keep the previous team's actor.
    if (MinimapActorRef && MinimapActorRef->TeamId != TeamId)
    {
        MinimapActorRef = nullptr;
    }

    // Use the TeamId passed into this function
    const int32 PlayerTeamId = TeamId;

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMinimapActor::StaticClass(), FoundActors);

    for (AActor* Actor : FoundActors)
    {
        AMinimapActor* MinimapActor = Cast<AMinimapActor>(Actor);
        if (MinimapActor)
        {
			if (MinimapActor->TeamId == PlayerTeamId)
			{
				MinimapActorRef = MinimapActor;
				break; // Found our actor, no need to search further
			}
        }
    }
    if (MinimapActorRef && MinimapImage)
    {
        // 1. Erstellen Sie eine dynamische Instanz des Materials, das dem Image zugewiesen ist.
        UTexture2D* TopoTexture = MinimapActorRef->GetTopographyTexture();
        UTexture2D* DataTexture = MinimapActorRef->GetDynamicDataTexture();
        UMaterialInstanceDynamic* MID = MinimapImage->GetDynamicMaterial();

        if (MID && TopoTexture && DataTexture)
        {
            // 2. Setzen Sie den Textur-Parameter mit dem Namen "MinimapTexture" auf unsere Laufzeit-Textur.
            MID->SetTextureParameterValue(TEXT("TopographyTexture"), TopoTexture);
            MID->SetTextureParameterValue(TEXT("MinimapTexture"), DataTexture);
            
            // Pass Brightness and Contrast to the Material for potential runtime adjustments
            MID->SetScalarParameterValue(TEXT("MinimapBrightness"), MinimapActorRef->MinimapBrightness);
            MID->SetScalarParameterValue(TEXT("MinimapContrast"), MinimapActorRef->MinimapContrast);

            if (GetWorld())
            {
                GetWorld()->GetTimerManager().ClearTimer(RetryTimerHandle);
            }
        }
        else
        {
             if (GetWorld() && !RetryTimerHandle.IsValid())
             {
                 GetWorld()->GetTimerManager().SetTimer(RetryTimerHandle, this, &UMinimapWidget::RetryInitialize, 0.5f, true);
             }
        }
    }
    else
    {
        if (GetWorld() && !RetryTimerHandle.IsValid())
        {
            GetWorld()->GetTimerManager().SetTimer(RetryTimerHandle, this, &UMinimapWidget::RetryInitialize, 0.5f, true);
        }
    }
}

void UMinimapWidget::RetryInitialize()
{
    InitializeForTeam(PendingTeamId);
}

/*
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
*/
FReply UMinimapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    const FKey Button = InMouseEvent.GetEffectingButton();

    // Mouse position on the map image itself (it may sit inside a frame), computed once.
    FVector2D LocalMousePosition;
    FVector2D WidgetSize;
    GetMapLocalPosition(InGeometry, InMouseEvent.GetScreenSpacePosition(), LocalMousePosition, WidgetSize);

    // Helper to compute world XY from minimap click with rotation correction
    auto ComputeWorldXY = [&](FVector2D& OutWorldXY)
    {
        if (!MinimapActorRef) return false;
        if (WidgetSize.X <= 0 || WidgetSize.Y <= 0) return false;

        // Rotation compensation identical to MoveCameraToMinimapLocation
        const FVector2D Center(WidgetSize.X * 0.5f, WidgetSize.Y * 0.5f);
        const float dx = LocalMousePosition.X - Center.X;
        const float dy = LocalMousePosition.Y - Center.Y;
        const float AngleRad = FMath::DegreesToRadians(-CurrentMapAngle);
        const float CosAngle = FMath::Cos(AngleRad);
        const float SinAngle = FMath::Sin(AngleRad);
        const float RotatedX_relative = dx * CosAngle - dy * SinAngle;
        const float RotatedY_relative = dx * SinAngle + dy * CosAngle;
        const FVector2D CorrectedLocalPosition(RotatedX_relative + Center.X, RotatedY_relative + Center.Y);

        const float U = FMath::Clamp(CorrectedLocalPosition.X / WidgetSize.X, 0.f, 1.f);
        const float V = FMath::Clamp(CorrectedLocalPosition.Y / WidgetSize.Y, 0.f, 1.f);
        const FVector2D& Min = MinimapActorRef->MinimapMinBounds;
        const FVector2D& Max = MinimapActorRef->MinimapMaxBounds;
        const float WorldExtentX = Max.X - Min.X;
        const float WorldExtentY = Max.Y - Min.Y;
        OutWorldXY.X = Min.X + (U * WorldExtentX);
        OutWorldXY.Y = Min.Y + (V * WorldExtentY);
        return true;
    };

    auto LineTraceGround = [&](const FVector2D& WorldXY, FVector& OutGround)
    {
        UWorld* World = GetWorld();
        if (!World) return false;
        APawn* PlayerPawn = GetOwningPlayerPawn();
        const FVector TraceStart(WorldXY.X, WorldXY.Y, 10000.0f);
        const FVector TraceEnd(WorldXY.X, WorldXY.Y, -10000.0f);
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(MinimapTrace), false);
        if (PlayerPawn) Params.AddIgnoredActor(PlayerPawn);
        if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params) && Hit.bBlockingHit)
        {
            OutGround = Hit.ImpactPoint;
            return true;
        }
        OutGround = FVector(WorldXY.X, WorldXY.Y, 0.f);
        return false;
    };

    // Handle Right Mouse Button: issue move commands for selected units via controller
    if (Button == EKeys::RightMouseButton)
    {
        FVector2D WorldXY;
        if (!ComputeWorldXY(WorldXY))
        {
            return FReply::Unhandled();
        }
        FVector Ground;
        LineTraceGround(WorldXY, Ground);

        // Call controller function
        APlayerController* PC = GetOwningPlayer();
        ACustomControllerBase* Ctrl = PC ? Cast<ACustomControllerBase>(PC) : nullptr;
        if (!Ctrl)
        {
            APawn* Pawn = GetOwningPlayerPawn();
            Ctrl = Pawn ? Cast<ACustomControllerBase>(Pawn->GetController()) : nullptr;
        }
        if (Ctrl)
        {
            Ctrl->RightClickPressedMassMinimap(Ground);
            return FReply::Handled();
        }
        return FReply::Unhandled();
    }

    // Handle Left Mouse Button
    if (Button == EKeys::LeftMouseButton)
    {
        // If AttackToggled and units are selected, do attack-move to minimap position
        APlayerController* PC = GetOwningPlayer();
        ACustomControllerBase* Ctrl = PC ? Cast<ACustomControllerBase>(PC) : nullptr;
        if (!Ctrl)
        {
            APawn* Pawn = GetOwningPlayerPawn();
            Ctrl = Pawn ? Cast<ACustomControllerBase>(Pawn->GetController()) : nullptr;
        }

        if (Ctrl && Ctrl->AttackToggled && Ctrl->SelectedUnits.Num() > 0)
        {
            FVector2D WorldXY;
            if (!ComputeWorldXY(WorldXY))
            {
                return FReply::Unhandled();
            }
            FVector Ground;
            LineTraceGround(WorldXY, Ground);
            Ctrl->LeftClickPressedMassMinimapAttack(Ground);
            return FReply::Handled();
        }

        // Default behavior: move camera to minimap location
        MoveCameraToMinimapLocation(LocalMousePosition, WidgetSize);
        return FReply::Handled();
    }

    // Other buttons: unhandled
    return FReply::Unhandled();
}

void UMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    // 1. Den Kamera-Pawn holen.
    ACameraBase* CameraPawn = Cast<ACameraBase>(GetOwningPlayerPawn());
    if (CameraPawn && MinimapImage)
    {
        // 2. Den Yaw der Kamera in WELTKOORDINATEN abfragen.
        //
        // Hier stand `CameraPawn->SpringArmRotator.Yaw`, und das ist der Yaw RELATIV zum Pawn.
        // Der Pawn selbst wird nie gedreht (CameraBase dreht ausschliesslich den SpringArm), er
        // behaelt also die Drehung seines PlayerStarts fuer die ganze Partie. Steht dieser
        // PlayerStart nicht auf Yaw 0 - was bei der zweiten Fraktion der Normalfall ist, weil sie
        // von der Gegenseite ins Feld schaut - fehlt der Minimap genau dieser feste Betrag.
        // Das erklaert alle drei gemeldeten Symptome auf einmal, denn CurrentMapAngle treibt sie
        // alle: verdrehtes Bild, falschherum stehende Kamerakennzeichnung und den Klickversatz
        // (siehe ComputeWorldXY und MoveCameraToMinimapLocation).
        // Die Weltdrehung des SpringArms enthaelt beides, Pawn- und Relativdrehung. Bei einem
        // PlayerStart mit Yaw 0 kommt derselbe Wert heraus wie vorher - fuer die Fraktion, bei
        // der es bisher stimmte, aendert sich also nichts.
        const float CameraYaw = CameraPawn->SpringArm
            ? CameraPawn->SpringArm->GetComponentRotation().Yaw
            : CameraPawn->GetActorRotation().Yaw + CameraPawn->SpringArmRotator.Yaw;

        // 3. Den finalen Winkel für das Bild berechnen.
        // Wir starten mit unserer Basis-Rotation von -90 Grad.
        // Wenn die Kamera sich dreht, muss die Minimap sich "gegenläufig" drehen,
        // damit die Ausrichtung wieder stimmt. Daher ziehen wir den Kamera-Yaw ab.
        CurrentMapAngle = -90.0f - CameraYaw;

        // 4. Den Rotationswinkel auf das Bild anwenden.
        MinimapImage->SetRenderTransformAngle(CurrentMapAngle);
    }

    // Angriffsalarm: Ton einmal je neuem Alarm, und solange einer laeuft jedes Bild neu zeichnen.
    if (const ACustomControllerBase* PC = Cast<ACustomControllerBase>(GetOwningPlayer()))
    {
        const TArray<FMinimapAttackAlert>& Alerts = PC->GetMinimapAttackAlerts();
        const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
        bool bAnyRunning = false;
        for (const FMinimapAttackAlert& Alert : Alerts)
        {
            bAnyRunning |= Now - Alert.StartTime <= PC->UnderAttackAlertDuration;
            if (Alert.Id <= LastAlertIdHandled)
            {
                continue;
            }
            LastAlertIdHandled = Alert.Id;
            if (UnderAttackSound && !(bSilentWhenOnScreen && Alert.bOnScreen) && Now - LastAlertSoundTime >= UnderAttackSoundCooldown)
            {
                LastAlertSoundTime = Now;
                UGameplayStatics::PlaySound2D(this, UnderAttackSound, PC->GetSoundMultiplier());
            }
        }
        if (bAnyRunning)
        {
            Invalidate(EInvalidateWidgetReason::Paint);
        }
    }
}

int32 UMinimapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    const ACustomControllerBase* PC = Cast<ACustomControllerBase>(GetOwningPlayer());
    const UWorld* World = GetWorld();
    if (!PC || !World || !MinimapImage || !MinimapActorRef || PC->GetMinimapAttackAlerts().Num() == 0)
    {
        return MaxLayer;
    }

    // Im Raum des Bildes zeichnen: seine Paint-Geometrie traegt die Drehung (SetRenderTransformAngle) schon in
    // sich, die Klammern sitzen damit dort, wo die Einheit auf der gedrehten Karte zu sehen ist.
    const FGeometry& MapGeometry = MinimapImage->GetPaintSpaceGeometry();
    const FVector2D Size = MapGeometry.GetLocalSize();
    const FVector2D& Min = MinimapActorRef->MinimapMinBounds;
    const FVector2D& Max = MinimapActorRef->MinimapMaxBounds;
    if (Size.X <= 0.f || Size.Y <= 0.f || Max.X <= Min.X || Max.Y <= Min.Y)
    {
        return MaxLayer;
    }

    const double Now = World->GetTimeSeconds();
    const int32 AlertLayer = MaxLayer + 1;
    for (const FMinimapAttackAlert& Alert : PC->GetMinimapAttackAlerts())
    {
        const double Age = Now - Alert.StartTime;
        if (Age < 0.0 || Age > PC->UnderAttackAlertDuration)
        {
            continue;
        }
        const FVector2D Center((Alert.Location.X - Min.X) / (Max.X - Min.X) * Size.X, (Alert.Location.Y - Min.Y) / (Max.Y - Min.Y) * Size.Y);

        // Zwei eckige Klammern [ ], die sich auf den Punkt zusammenziehen - immer wieder, solange der Alarm laeuft.
        // Die Versaetze werden gegen die Kartendrehung gedreht: so stehen die Klammern auf dem Bildschirm
        // immer links und rechts, wie auch immer die Kamera schaut.
        const float Pulse = FMath::Fmod((float)Age, AttackAlertPulseTime) / AttackAlertPulseTime;
        const float Ease = 1.f - FMath::Square(1.f - Pulse);
        const float Half = FMath::Lerp(AttackAlertStartSize, AttackAlertEndSize, Ease) * FMath::Min(Size.X, Size.Y);
        const float Arm = FMath::Max(Half * 0.45f, 2.f);
        FLinearColor Color = AttackAlertColor;
        Color.A *= FMath::Lerp(0.55f, 1.f, Ease);

        const FQuat2D Unrotate(FMath::DegreesToRadians(-CurrentMapAngle));
        for (const float Side : { -1.f, 1.f })
        {
            const float X = Side * Half;
            TArray<FVector2D> Points;
            for (const FVector2D& Offset : { FVector2D(X - Side * Arm, -Half), FVector2D(X, -Half), FVector2D(X, Half), FVector2D(X - Side * Arm, Half) })
            {
                Points.Add(Center + Unrotate.TransformPoint(Offset));
            }
            FSlateDrawElement::MakeLines(OutDrawElements, AlertLayer, MapGeometry.ToPaintGeometry(), Points,
                ESlateDrawEffect::None, Color, true, AttackAlertThickness);
        }
    }
    return AlertLayer;
}

void UMinimapWidget::GetMapLocalPosition(const FGeometry& InGeometry, const FVector2D& ScreenPosition, FVector2D& OutLocal, FVector2D& OutSize) const
{
    OutLocal = InGeometry.AbsoluteToLocal(ScreenPosition);
    OutSize = InGeometry.GetLocalSize();
    if (!MinimapImage)
    {
        return;
    }
    // The image is rotated around its centre (render transform), so its centre stays put whatever its
    // geometry includes. From there with the layout scale only: the position before the rotation, which
    // the callers undo themselves (CurrentMapAngle).
    const FGeometry& ImageGeometry = MinimapImage->GetCachedGeometry();
    const FVector2D ImageSize = ImageGeometry.GetLocalSize();
    const float Scale = ImageGeometry.GetAccumulatedLayoutTransform().GetScale();
    if (ImageSize.X <= 0.f || ImageSize.Y <= 0.f || Scale <= KINDA_SMALL_NUMBER)
    {
        return;
    }
    const FVector2D CentreAbsolute = ImageGeometry.LocalToAbsolute(ImageSize * 0.5f);
    OutLocal = (ScreenPosition - CentreAbsolute) / Scale + ImageSize * 0.5f;
    OutSize = ImageSize;
}

FCursorReply UMinimapWidget::NativeOnCursorQuery(const FGeometry& InGeometry, const FPointerEvent& InCursorEvent)
{
    // The UMG default would be the system arrow; the game's cursor (a software cursor widget registered for
    // this cursor type, see AControllerBase::ApplyCustomMouseCursor) belongs to the controller.
    if (const APlayerController* PC = GetOwningPlayer())
    {
        return FCursorReply::Cursor(PC->CurrentMouseCursor);
    }
    return Super::NativeOnCursorQuery(InGeometry, InCursorEvent);
}

void UMinimapWidget::MoveCameraToMinimapLocation(const FVector2D& LocalMousePosition, const FVector2D& MapSize)
{
    if (!MinimapActorRef) return;
    APawn* PlayerPawn = GetOwningPlayerPawn();
    if (!PlayerPawn) return;

    ACameraControllerBase* CameraPC = Cast<ACameraControllerBase>(PlayerPawn->GetController());

    if (!CameraPC) return;
    
    const FVector2D WidgetSize = MapSize;
    if (WidgetSize.X <= 0 || WidgetSize.Y <= 0) return;

    // --- DYNAMISCHE ROTATIONS-LOGIK ---

    // 1. Den Mittelpunkt des Widgets berechnen.
    const FVector2D Center(WidgetSize.X * 0.5f, WidgetSize.Y * 0.5f);

    // 2. Die Klick-Position relativ zum Mittelpunkt berechnen.
    const float dx = LocalMousePosition.X - Center.X;
    const float dy = LocalMousePosition.Y - Center.Y;

    // 3. Die "Gegen-Rotation" berechnen, um den Klick zu korrigieren.
    // Wir nehmen den negativen Winkel, den wir im Tick berechnet haben.
    const float AngleRad = FMath::DegreesToRadians(-CurrentMapAngle);
    const float CosAngle = FMath::Cos(AngleRad);
    const float SinAngle = FMath::Sin(AngleRad);

    // 4. Die volle 2D-Rotationsformel anwenden, um die korrigierte Position zu finden.
    const float RotatedX_relative = dx * CosAngle - dy * SinAngle;
    const float RotatedY_relative = dx * SinAngle + dy * CosAngle;

    // 5. Die rotierten Koordinaten zurück in den lokalen Widget-Raum umrechnen.
    const FVector2D CorrectedLocalPosition(RotatedX_relative + Center.X, RotatedY_relative + Center.Y);
    
    // --- ENDE DER DYNAMISCHEN LOGIK ---

    // 6. Die finalen UVs berechnen (dieser Teil bleibt gleich).
    const float U = FMath::Clamp(CorrectedLocalPosition.X / WidgetSize.X, 0.f, 1.f);
    const float V = FMath::Clamp(CorrectedLocalPosition.Y / WidgetSize.Y, 0.f, 1.f);

    // Der Rest der Funktion bleibt unverändert.
    const FVector2D& Min = MinimapActorRef->MinimapMinBounds;
    const FVector2D& Max = MinimapActorRef->MinimapMaxBounds;
    const float WorldExtentX = Max.X - Min.X;
    const float WorldExtentY = Max.Y - Min.Y;

    const float TargetWorldX = Min.X + (U * WorldExtentX);
    const float TargetWorldY = Min.Y + (V * WorldExtentY);

    const FVector CurrentLocation = PlayerPawn->GetActorLocation();

    
    // --- MODIFIED: LineTrace to find Ground Z-Coordinate only if not flying ---
    
    float TargetWorldZ = CurrentLocation.Z; // Default to the pawn's current Z-height.
    
    // Attempt to get the character's movement component.
    ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerPawn);
    UCharacterMovementComponent* MovementComponent = PlayerCharacter ? PlayerCharacter->GetCharacterMovement() : nullptr;

    // Only perform the ground trace if we have a valid movement component AND the character is not flying.
    if (MovementComponent && MovementComponent->MovementMode != EMovementMode::MOVE_Flying)
    {
        UWorld* World = GetWorld();
        if (World)
        {
            const FVector TraceStart(TargetWorldX, TargetWorldY, 10000.0f);
            const FVector TraceEnd(TargetWorldX, TargetWorldY, -10000.0f);

            FHitResult HitResult;
            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(PlayerPawn);

            if (World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
            {
                // If we hit the ground, use that Z-coordinate.
                TargetWorldZ = HitResult.ImpactPoint.Z + PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            }
        }
    }
    // If the character is flying, TargetWorldZ remains at the current actor Z, so they don't snap to the ground.
    
    // --- END of LineTrace Logic ---

    // Construct the new location.
    const FVector NewLocation(TargetWorldX, TargetWorldY, TargetWorldZ);
    
    PlayerPawn->SetActorLocation(NewLocation);
    CameraPC->Server_SetCameraLocation(NewLocation);
}