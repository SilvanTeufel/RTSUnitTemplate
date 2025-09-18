#include "Widgets/SaveSlotListItemWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Widgets/SaveGameWidget.h"

void USaveSlotListItemWidget::InitializeItem(const FString& InSlotName, const FString& InDisplayText, USaveGameWidget* InOwner)
{
    SlotName = InSlotName;
    OwnerWidget = InOwner;

    if (LabelText)
    {
        LabelText->SetText(FText::FromString(InDisplayText));
    }
}

void USaveSlotListItemWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SelectButton)
    {
        SelectButton->OnClicked.AddDynamic(this, &USaveSlotListItemWidget::OnSelectClicked);
    }
}

void USaveSlotListItemWidget::OnSelectClicked()
{
    if (OwnerWidget)
    {
        OwnerWidget->HandleSlotItemClicked(SlotName);
    }
}
