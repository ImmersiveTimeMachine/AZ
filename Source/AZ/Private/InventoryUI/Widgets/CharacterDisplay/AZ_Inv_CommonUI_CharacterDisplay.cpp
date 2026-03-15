// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryUI/Widgets/CharacterDisplay/AZ_Inv_CommonUI_CharacterDisplay.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Equipment/ProxyMesh/AZ_Inv_ProxyMesh.h"
#include "Kismet/GameplayStatics.h"


FReply UAZ_Inv_CommonUI_CharacterDisplay::NativeOnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	CurrentPosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());
	LastPosition = CurrentPosition;

	bIsDragging = true;
	return FReply::Handled();
}

FReply UAZ_Inv_CommonUI_CharacterDisplay::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	bIsDragging = false;
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UAZ_Inv_CommonUI_CharacterDisplay::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bIsDragging = false;
}

void UAZ_Inv_CommonUI_CharacterDisplay::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(this, AAZ_Inv_ProxyMesh::StaticClass(), Actors);

	if (!Actors.IsValidIndex(0)) return;

	AAZ_Inv_ProxyMesh* ProxyMesh = Cast<AAZ_Inv_ProxyMesh>(Actors[0]);
	if (!IsValid(ProxyMesh)) return;

	Mesh = ProxyMesh->GetMesh();
}

void UAZ_Inv_CommonUI_CharacterDisplay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bIsDragging) return;

	LastPosition = CurrentPosition;
	CurrentPosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());

	const float HorizontalDelta = LastPosition.X - CurrentPosition.X;

	if (!Mesh.IsValid()) return;
	Mesh->AddRelativeRotation(FRotator(0.f, HorizontalDelta, 0.f));
}
