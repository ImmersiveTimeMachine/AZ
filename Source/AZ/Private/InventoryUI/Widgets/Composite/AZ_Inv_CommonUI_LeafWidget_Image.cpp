// Fill out your copyright notice in the Description page of Project Settings.

#include "InventoryUI/Widgets/Composite/AZ_Inv_CommonUI_LeafWidget_Image.h"

#include "Components/Image.h"
#include "Components/SizeBox.h"

void UAZ_Inv_CommonUI_LeafWidget_Image::SetImage(UTexture2D* Texture) const
{
	Image_Icon->SetBrushFromTexture(Texture);
}

void UAZ_Inv_CommonUI_LeafWidget_Image::SetBoxSize(const FVector2D& Size) const
{
	SizeBox_Icon->SetWidthOverride(Size.X);
	SizeBox_Icon->SetHeightOverride(Size.Y);
}

void UAZ_Inv_CommonUI_LeafWidget_Image::SetImageSize(const FVector2D& Size) const
{
	Image_Icon->SetDesiredSizeOverride(Size);
}

FVector2D UAZ_Inv_CommonUI_LeafWidget_Image::GetImageSize() const
{
	return Image_Icon->GetDesiredSize();
}
