#include "UI/AZ_HUDReticleWidget.h"

#include "UI/AZ_HUDReticleDefinition.h"

void UAZ_HUDReticleWidget::SetReticleView(const FAZ_PlayerReticleView& View)
{
	CurrentReticleView = View;
	if (IsValid(View.Definition))
	{
		SetColorAndOpacity(View.Definition->Tint);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	OnReticleViewChanged(View);
}
