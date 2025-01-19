// Fill out your copyright notice in the Description page of Project Settings.


#include "Widgets/TaggedUnitSelector.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/Image.h"

UTaggedUnitSelector::UTaggedUnitSelector(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Optionally set all pointers to nullptr
    TagButtonCtrl1 = nullptr;
    IconCtrl1 = nullptr;
    // ... etc. for other pointers as needed
}

void UTaggedUnitSelector::NativeConstruct()
{
    Super::NativeConstruct();

    ControllerBase = Cast<AExtendedControllerBase>(GetWorld()->GetFirstPlayerController());

    // 2. Bind all buttons to their respective click handlers
    if (TagButtonCtrl1)
    {
        TagButtonCtrl1->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrl1Clicked);
    }

    if (TagButtonCtrl2)
    {
        TagButtonCtrl2->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrl2Clicked);
    }

    if (TagButtonCtrl3)
    {
        TagButtonCtrl3->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrl3Clicked);
    }

    if (TagButtonCtrl4)
    {
        TagButtonCtrl4->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrl4Clicked);
    }

    if (TagButtonCtrl5)
    {
        TagButtonCtrl5->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrl5Clicked);
    }

    if (TagButtonCtrl6)
    {
        TagButtonCtrl6->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrl6Clicked);
    }

    if (TagButtonCtrlQ)
    {
        TagButtonCtrlQ->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrlQClicked);
    }

    if (TagButtonCtrlW)
    {
        TagButtonCtrlW->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrlWClicked);
    }

    if (TagButtonCtrlE)
    {
        TagButtonCtrlE->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrlEClicked);
    }

    if (TagButtonCtrlR)
    {
        TagButtonCtrlR->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrlRClicked);
    }

    if (TagButtonAlt1)
    {
        TagButtonAlt1->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonAlt1Clicked);
    }

    if (TagButtonAlt2)
    {
        TagButtonAlt2->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonAlt2Clicked);
    }

    if (TagButtonAlt3)
    {
        TagButtonAlt3->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonAlt3Clicked);
    }

    if (TagButtonAlt4)
    {
        TagButtonAlt4->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonAlt4Clicked);
    }

    if (TagButtonAlt5)
    {
        TagButtonAlt5->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonAlt5Clicked);
    }

    if (TagButtonAlt6)
    {
        TagButtonAlt6->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonAlt6Clicked);
    }
}
/*
TSharedRef<SWidget> UTaggedUnitSelector::RebuildWidget()
{
    // 1) If there's no existing root widget, create one.
    //    Older engine versions do not have SetRootWidget(), so we'll do:
    if (WidgetTree->RootWidget == nullptr)
    {
        // We'll use a vertical box as the root container
        UVerticalBox* NewRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RootVerticalBox"));
        WidgetTree->RootWidget = NewRoot; // Assign directly
    }

    // 2) Cast the root widget to UPanelWidget so we can add child panels to it
    UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget);
    if (!RootPanel)
    {
        // Safety check, though normally this should succeed if we created a VerticalBox
        return Super::RebuildWidget();
    }

    // Clear out existing children (in case RebuildWidget is called again)
    RootPanel->ClearChildren();

    // 3) Create two horizontal boxes for the two rows
    UHorizontalBox* Row1 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row1"));
    UHorizontalBox* Row2 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row2"));

    // Add them to the root vertical box
    if (UVerticalBox* VerticalBox = Cast<UVerticalBox>(RootPanel))
    {
        VerticalBox->AddChildToVerticalBox(Row1);
        VerticalBox->AddChildToVerticalBox(Row2);
    }

    //---------------------------------------------------
    // 4) Helper lambda to create a button+icon
    //---------------------------------------------------
    auto CreateButtonWithIcon = [&](const FString& ButtonName, const FString& IconName, UButton*& OutButton, UImage*& OutIcon)
    {
        // Construct a Button
        OutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *ButtonName);

        // Construct an Image (icon)
        OutIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *IconName);

        // Example: If you have a texture asset to set on the icon,
        // you can do something like:
        //
        // UTexture2D* MyTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Textures/MyIcon.MyIcon"));
        // if (MyTexture)
        // {
        //     OutIcon->SetBrushFromTexture(MyTexture, true);
        // }

        // Add the Image as a child of the Button
        OutButton->AddChild(OutIcon);
    };

//---------------------------------------------------
    // Create & Bind: Ctrl1..6
    //---------------------------------------------------
    CreateButtonWithIcon(TEXT("TagButtonCtrl1"), TEXT("IconCtrl1"), TagButtonCtrl1, IconCtrl1);
    if (TagButtonCtrl1)
    {
        TagButtonCtrl1->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrl1Clicked);
    }

    CreateButtonWithIcon(TEXT("TagButtonCtrl2"), TEXT("IconCtrl2"), TagButtonCtrl2, IconCtrl2);
    if (TagButtonCtrl2)
    {
        TagButtonCtrl2->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrl2Clicked);
    }

    CreateButtonWithIcon(TEXT("TagButtonCtrl3"), TEXT("IconCtrl3"), TagButtonCtrl3, IconCtrl3);
    if (TagButtonCtrl3)
    {
        TagButtonCtrl3->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrl3Clicked);
    }

    CreateButtonWithIcon(TEXT("TagButtonCtrl4"), TEXT("IconCtrl4"), TagButtonCtrl4, IconCtrl4);
    if (TagButtonCtrl4)
    {
        TagButtonCtrl4->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrl4Clicked);
    }

    CreateButtonWithIcon(TEXT("TagButtonCtrl5"), TEXT("IconCtrl5"), TagButtonCtrl5, IconCtrl5);
    if (TagButtonCtrl5)
    {
        TagButtonCtrl5->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrl5Clicked);
    }

    CreateButtonWithIcon(TEXT("TagButtonCtrl6"), TEXT("IconCtrl6"), TagButtonCtrl6, IconCtrl6);
    if (TagButtonCtrl6)
    {
        TagButtonCtrl6->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrl6Clicked);
    }

    //---------------------------------------------------
    // Create & Bind: CtrlQ, CtrlW, CtrlE, CtrlR
    //---------------------------------------------------
    CreateButtonWithIcon(TEXT("TagButtonCtrlQ"), TEXT("IconCtrlQ"), TagButtonCtrlQ, IconCtrlQ);
    if (TagButtonCtrlQ)
    {
        TagButtonCtrlQ->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrlQClicked);
    }

    CreateButtonWithIcon(TEXT("TagButtonCtrlW"), TEXT("IconCtrlW"), TagButtonCtrlW, IconCtrlW);
    if (TagButtonCtrlW)
    {
        TagButtonCtrlW->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrlWClicked);
    }

    CreateButtonWithIcon(TEXT("TagButtonCtrlE"), TEXT("IconCtrlE"), TagButtonCtrlE, IconCtrlE);
    if (TagButtonCtrlE)
    {
        TagButtonCtrlE->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrlEClicked);
    }

    CreateButtonWithIcon(TEXT("TagButtonCtrlR"), TEXT("IconCtrlR"), TagButtonCtrlR, IconCtrlR);
    if (TagButtonCtrlR)
    {
        TagButtonCtrlR->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrlRClicked);
    }

    //---------------------------------------------------
    // Create & Bind: Alt1..6
    //---------------------------------------------------
    CreateButtonWithIcon(TEXT("TagButtonAlt1"), TEXT("IconAlt1"), TagButtonAlt1, IconAlt1);
    if (TagButtonAlt1)
    {
        TagButtonAlt1->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonAlt1Clicked);
    }

    CreateButtonWithIcon(TEXT("TagButtonAlt2"), TEXT("IconAlt2"), TagButtonAlt2, IconAlt2);
    if (TagButtonAlt2)
    {
        TagButtonAlt2->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonAlt2Clicked);
    }

    CreateButtonWithIcon(TEXT("TagButtonAlt3"), TEXT("IconAlt3"), TagButtonAlt3, IconAlt3);
    if (TagButtonAlt3)
    {
        TagButtonAlt3->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonAlt3Clicked);
    }

    CreateButtonWithIcon(TEXT("TagButtonAlt4"), TEXT("IconAlt4"), TagButtonAlt4, IconAlt4);
    if (TagButtonAlt4)
    {
        TagButtonAlt4->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonAlt4Clicked);
    }

    CreateButtonWithIcon(TEXT("TagButtonAlt5"), TEXT("IconAlt5"), TagButtonAlt5, IconAlt5);
    if (TagButtonAlt5)
    {
        TagButtonAlt5->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonAlt5Clicked);
    }

    CreateButtonWithIcon(TEXT("TagButtonAlt6"), TEXT("IconAlt6"), TagButtonAlt6, IconAlt6);
    if (TagButtonAlt6)
    {
        TagButtonAlt6->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonAlt6Clicked);
    }


    
    // 5) Create each of the 16 buttons in code

    // Row #1: (Ctrl1..6, CtrlQ, CtrlW)
    CreateButtonWithIcon(TEXT("TagButtonCtrl1"), TEXT("IconCtrl1"), TagButtonCtrl1, IconCtrl1);
    CreateButtonWithIcon(TEXT("TagButtonCtrl2"), TEXT("IconCtrl2"), TagButtonCtrl2, IconCtrl2);
    CreateButtonWithIcon(TEXT("TagButtonCtrl3"), TEXT("IconCtrl3"), TagButtonCtrl3, IconCtrl3);
    CreateButtonWithIcon(TEXT("TagButtonCtrl4"), TEXT("IconCtrl4"), TagButtonCtrl4, IconCtrl4);
    CreateButtonWithIcon(TEXT("TagButtonCtrl5"), TEXT("IconCtrl5"), TagButtonCtrl5, IconCtrl5);
    CreateButtonWithIcon(TEXT("TagButtonCtrl6"), TEXT("IconCtrl6"), TagButtonCtrl6, IconCtrl6);
    CreateButtonWithIcon(TEXT("TagButtonCtrlQ"), TEXT("IconCtrlQ"), TagButtonCtrlQ, IconCtrlQ);
    CreateButtonWithIcon(TEXT("TagButtonCtrlW"), TEXT("IconCtrlW"), TagButtonCtrlW, IconCtrlW);

    // Row #2: (Alt1..6, CtrlE, CtrlR)
    CreateButtonWithIcon(TEXT("TagButtonAlt1"), TEXT("IconAlt1"), TagButtonAlt1, IconAlt1);
    CreateButtonWithIcon(TEXT("TagButtonAlt2"), TEXT("IconAlt2"), TagButtonAlt2, IconAlt2);
    CreateButtonWithIcon(TEXT("TagButtonAlt3"), TEXT("IconAlt3"), TagButtonAlt3, IconAlt3);
    CreateButtonWithIcon(TEXT("TagButtonAlt4"), TEXT("IconAlt4"), TagButtonAlt4, IconAlt4);
    CreateButtonWithIcon(TEXT("TagButtonAlt5"), TEXT("IconAlt5"), TagButtonAlt5, IconAlt5);
    CreateButtonWithIcon(TEXT("TagButtonAlt6"), TEXT("IconAlt6"), TagButtonAlt6, IconAlt6);
    CreateButtonWithIcon(TEXT("TagButtonCtrlE"), TEXT("IconCtrlE"), TagButtonCtrlE, IconCtrlE);
    CreateButtonWithIcon(TEXT("TagButtonCtrlR"), TEXT("IconCtrlR"), TagButtonCtrlR, IconCtrlR);

    // 6) Add row1's 8 buttons
    Row1->AddChildToHorizontalBox(TagButtonCtrl1);
    Row1->AddChildToHorizontalBox(TagButtonCtrl2);
    Row1->AddChildToHorizontalBox(TagButtonCtrl3);
    Row1->AddChildToHorizontalBox(TagButtonCtrl4);
    Row1->AddChildToHorizontalBox(TagButtonCtrl5);
    Row1->AddChildToHorizontalBox(TagButtonCtrl6);
    Row1->AddChildToHorizontalBox(TagButtonCtrlQ);
    Row1->AddChildToHorizontalBox(TagButtonCtrlW);

    // 7) Add row2's 8 buttons
    Row2->AddChildToHorizontalBox(TagButtonAlt1);
    Row2->AddChildToHorizontalBox(TagButtonAlt2);
    Row2->AddChildToHorizontalBox(TagButtonAlt3);
    Row2->AddChildToHorizontalBox(TagButtonAlt4);
    Row2->AddChildToHorizontalBox(TagButtonAlt5);
    Row2->AddChildToHorizontalBox(TagButtonAlt6);
    Row2->AddChildToHorizontalBox(TagButtonCtrlE);
    Row2->AddChildToHorizontalBox(TagButtonCtrlR);

    // 8) Bind at least one button to a click event (example: Ctrl1)
    if (TagButtonCtrl1)
    {
        TagButtonCtrl1->OnClicked.AddDynamic(this, &UTaggedUnitSelector::OnTagButtonCtrl1Clicked);
    }

    // Finally, call the parent’s RebuildWidget
    return Super::RebuildWidget();
}*/


void UTaggedUnitSelector::OnTagButtonCtrl1Clicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonCtrl1 clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagCtrl1, ControllerBase->SelectableTeamId);
    }
}

void UTaggedUnitSelector::OnTagButtonCtrl2Clicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonCtrl2 clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagCtrl2, ControllerBase->SelectableTeamId);
    }
}

void UTaggedUnitSelector::OnTagButtonCtrl3Clicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonCtrl3 clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagCtrl3, ControllerBase->SelectableTeamId);
    }
}

void UTaggedUnitSelector::OnTagButtonCtrl4Clicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonCtrl4 clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagCtrl4, ControllerBase->SelectableTeamId);
    }
}

void UTaggedUnitSelector::OnTagButtonCtrl5Clicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonCtrl5 clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagCtrl5, ControllerBase->SelectableTeamId);
    }
}

void UTaggedUnitSelector::OnTagButtonCtrl6Clicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonCtrl6 clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagCtrl6, ControllerBase->SelectableTeamId);
    }
}

// --- Ctrl Q, W, E, R ---
void UTaggedUnitSelector::OnTagButtonCtrlQClicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonCtrlQ clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagCtrlQ, ControllerBase->SelectableTeamId);
    }
}

void UTaggedUnitSelector::OnTagButtonCtrlWClicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonCtrlW clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagCtrlW, ControllerBase->SelectableTeamId);
    }
}

void UTaggedUnitSelector::OnTagButtonCtrlEClicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonCtrlE clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagCtrlE, ControllerBase->SelectableTeamId);
    }
}

void UTaggedUnitSelector::OnTagButtonCtrlRClicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonCtrlR clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagCtrlR, ControllerBase->SelectableTeamId);
    }
}

// --- Alt1..6 ---
void UTaggedUnitSelector::OnTagButtonAlt1Clicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonAlt1 clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagAlt1, ControllerBase->SelectableTeamId);
    }
}

void UTaggedUnitSelector::OnTagButtonAlt2Clicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonAlt2 clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagAlt2, ControllerBase->SelectableTeamId);
    }
}

void UTaggedUnitSelector::OnTagButtonAlt3Clicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonAlt3 clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagAlt3, ControllerBase->SelectableTeamId);
    }
}

void UTaggedUnitSelector::OnTagButtonAlt4Clicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonAlt4 clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagAlt4, ControllerBase->SelectableTeamId);
    }
}

void UTaggedUnitSelector::OnTagButtonAlt5Clicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonAlt5 clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagAlt5, ControllerBase->SelectableTeamId);
    }
}

void UTaggedUnitSelector::OnTagButtonAlt6Clicked()
{
    UE_LOG(LogTemp, Log, TEXT("TagButtonAlt6 clicked"));

    if (ControllerBase)
    {
        ControllerBase->SelectUnitsWithTag(ControllerBase->KeyTagAlt6, ControllerBase->SelectableTeamId);
    }
}