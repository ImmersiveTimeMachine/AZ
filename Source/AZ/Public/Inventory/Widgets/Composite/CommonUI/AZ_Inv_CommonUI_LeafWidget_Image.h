// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AZ_Inv_CommonUI_LeafWidget.h"
#include "AZ_Inv_CommonUI_LeafWidget_Image.generated.h"

class USizeBox;
class UImage;

/**
 * CommonUI version of image leaf widget.
 * Provides functionality for displaying and manipulating images.
 */
UCLASS()
class AZ_API UAZ_Inv_CommonUI_LeafWidget_Image : public UAZ_Inv_CommonUI_LeafWidget
{
	GENERATED_BODY()
	
public:
	void SetImage(UTexture2D* Texture) const;
	void SetBoxSize(const FVector2D& Size) const;
	void SetImageSize(const FVector2D& Size) const;
	FVector2D GetImageSize() const;
	
private:

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> SizeBox_Icon;
};
