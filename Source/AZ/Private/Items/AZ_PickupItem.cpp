#include "Items/AZ_PickupItem.h"

#include "GameFramework/Pawn.h"
#include "InventoryUI/AZ_Inv_CommonUI_ItemComponent.h"
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

	if (const APawn* Hero = Cast<APawn>(OtherActor))
	{
		if (Hero->IsLocallyControlled())
		{
			if (AAZ_PlayerController* PlayerCtrl = Cast<AAZ_PlayerController>(Hero->GetController());
				PlayerCtrl)
			{
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
	if (const APawn* Hero = Cast<APawn>(OtherActor))
	{
		if (Hero->IsLocallyControlled())
		{
			if (AAZ_PlayerController* PlayerCtrl = Cast<AAZ_PlayerController>(Hero->GetController());
				PlayerCtrl)
			{
				PlayerCtrl->RefreshPickupTarget();
			}
		}
	}
}
