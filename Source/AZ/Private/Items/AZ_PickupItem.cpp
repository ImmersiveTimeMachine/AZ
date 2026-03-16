#include "Items/AZ_PickupItem.h"

#include "Character/AZ_HeroCharacter.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
#include "InventoryOld/Widgets/HUD/AZ_InventoryHudWidget.h"
#include "Player/AZ_PlayerController.h"


AAZ_PickupItem::AAZ_PickupItem()
{
	// All component creation handled by AAZ_Item
}

void AAZ_PickupItem::BeginPlay()
{
	Super::BeginPlay();

	const auto* ItemComponent = FindComponentByClass<UAZ_Inv_CommonUI_ItemComponent>();
	ensure(IsValid(ItemComponent));
}

void AAZ_PickupItem::HandleBeginOverlap(UPrimitiveComponent* OverlappedComp,
                                        AActor* OtherActor,
                                        UPrimitiveComponent* OtherComp,
                                        int32 OtherBodyIndex,
                                        bool bFromSweep,
                                        const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;

	if (const auto Hero = Cast<AAZ_HeroCharacter>(OtherActor))
	{
		if (Hero->IsLocallyControlled())
		{
			if (AAZ_PlayerController* PlayerCtrl = Cast<AAZ_PlayerController>(Hero->GetController());
				PlayerCtrl && PlayerCtrl->HUDWidget)
			{
				PlayerCtrl->HUDWidget->ShowPickupMessage(FString("Press 'E' to add item."));
				PlayerCtrl->SetActivePickUpActor(this);
			}
		}
	}
}

void AAZ_PickupItem::HandleEndOverlap(UPrimitiveComponent* OverlappedComp,
                                      AActor* OtherActor,
                                      UPrimitiveComponent* OtherComp,
                                      int32 OtherBodyIndex)
{
	if (const auto Hero = Cast<AAZ_HeroCharacter>(OtherActor))
	{
		if (Hero->IsLocallyControlled())
		{
			if (AAZ_PlayerController* PlayerCtrl = Cast<AAZ_PlayerController>(Hero->GetController());
				PlayerCtrl && PlayerCtrl->HUDWidget)
			{
				PlayerCtrl->HUDWidget->HidePickupMessage();
				PlayerCtrl->SetActivePickUpActor(nullptr);
			}
		}
	}
}
