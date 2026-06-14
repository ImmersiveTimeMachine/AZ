# Inventory System Details

## Already Migrated to CommonUI (C++)
1. UAZ_Inv_CommonUI_InventoryComponent (UActorComponent) - Core inventory logic + FastArray replication
2. UAZ_Inv_CommonUI_InventoryItem (UObject) - Replicated item with FInstancedStruct manifest
3. UAZ_Inv_CommonUI_ItemComponent (UActorComponent) - Pickup item component
4. FAZ_Inv_CommonUI_InventoryFastArray (FFastArraySerializer) - Net replication
5. UAZ_Inv_CommonUI_GameInventoryMenu (UCommonActivatableWidget) - Abstract menu base
6. UAZ_Inv_CommonUI_Button (UCommonButtonBase) - Base button
7. UAZ_Inv_CommonUI_GridSlot (UAZ_Inv_CommonUI_Button) - Grid cell
8. UAZ_Inv_CommonUI_SlottedItem (UAZ_Inv_CommonUI_Button) - Item in grid
9. UAZ_Inv_CommonUI_InventoryGrid (UUserWidget) - Grid container with UGridPanel
10. UAZ_Inv_CommonUI_HoverItem (UCommonUserWidget) - Drag preview
11. UAZ_Inv_CommonUI_ItemPopUp (UCommonUserWidget) - EMPTY STUB
12. FAZ_Inv_CommonUI_ItemManifest - Item data container
13. FAZ_Inv_CommonUI_ItemFragment - Base + Grid, Image, Text, Stackable fragments
14. Composite widgets (Base, Container, Leaf, Leaf_Image, Leaf_Text, Leaf_LabeledValue)

## NOT Yet Migrated (C++ only)
### Missing Fragments (in AZ_Inv_CommonUI_ItemFragment.h):
- LabeledNumberFragment (min/max/value stats)
- ConsumableFragment + ConsumeModifier base
- HealthPotionFragment, ManaPotionFragment
- EquipmentFragment + EquipModifier base
- StrengthModifier, ArmorModifier, DamageModifier

### Missing Widgets:
- ItemPopUp (full implementation with Split/Drop/Consume/Slider)
- SpatialInventoryMenu (full screen with tabs, equipped slots, descriptions)
- EquippedGridSlot (equipment-specific slot)
- EquippedSlottedItem (equipped item display)
- ItemDescription (extends Composite, shows item details)
- CharacterDisplay (3D character preview with rotation)
- InfoMessage (HUD text notification)
- InventoryHudWidget (HUD integration)

### Missing Components:
- EquipmentComponent (spawn/destroy equipped actors)

### Statics Updates Needed:
- UAZ_Inv_InventoryStatics: Add CommonUI versions of ItemHovered, ItemUnhovered, GetHoverItem, GetInventoryWidget

### Shared/Reusable (no migration needed):
- AAZ_Inv_EquipActor - Gameplay actor
- AAZ_Inv_ProxyMesh - Character preview mesh
- AAZ_PickupItem - Pickup actor
- UAZ_Inv_WidgetUtils - Pure utility functions
- AZ_Inv_GridTypes.h - Already has CommonUI struct variants

## Key File Paths
- Headers: Source/AZ/Public/Inventory/
- Implementations: Source/AZ/Private/Inventory/
- Grid types: Public/Inventory/Types/AZ_Inv_GridTypes.h
- Statics: Public/Inventory/Widgets/Utils/AZ_Inv_InventoryStatics.h
- Old fragments: Public/Inventory/Items/Fragments/AZ_Inv_ItemFragment.h
- CommonUI fragments: Public/Inventory/Items/Fragments/AZ_Inv_CommonUI_ItemFragment.h
