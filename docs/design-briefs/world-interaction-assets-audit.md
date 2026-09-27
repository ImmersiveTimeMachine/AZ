# Первый набор взаимодействий: ассеты и площадка

Проверено и размещено 26 сентября 2026. Это подбор моделей для проектирования, не готовая игровая система взаимодействий.

## Размещение

- Карта: `/Game/AZ/Maps/L_001`, сохранена.
- Ориентир пользователя: `(-2857, 3582, 11)`, существующая первая граната.
- Образцы стоят двумя рядами рядом: X примерно -3307 и -2507, Y 3582–4632; высота выровнена по поверхности.
- Папка Outliner: `AZ_Interaction_Showcase`, метка `AZ_InteractionShowcase_v1`.
- 19 StaticMeshActor, уникальные подписи `AZ INTERACTION …`. Это 12 групп/видов образцов, включая отдельные детали.
- Старт игрока, гранаты и прочие существовавшие акторы не перемещались. Исходные меши/материалы не редактировались.
- Основное взаимодействие на клавиатуре — **E**, согласно указанию пользователя. Будущие действия должны использовать существующий input action и общие подсказки геймпада.
- На образцы ещё не подключены подбор, открывание, осмотр или логика сохранений. Навигационная релевантность демонстрационных компонентов выключена; перед игровой интеграцией мебель потребует своей настройки коллизии/навигации.

## Выбранные модели

| Группа | Источник |
|---|---|
| Деревянная дверь | `/Game/ClassicMansion/Meshes/SM_DoorA_L` |
| Металлическая дверь с рамой и двумя створками | `/Game/Safe_House/meshes/SM_entrance_door_frame`, `SM_entrance_door_left`, `SM_entrance_door_right`; масштаб 0.6 |
| Комод с отдельными ящиками | `/Game/ClassicMansion/Meshes/SM_DrawersA`, `SM_DrawerA`, `SM_DrawerB` |
| Шкафчик с двумя отдельными дверцами | `/Game/Safe_House/meshes/SM_locker_locker_main`, две копии `SM_locker_locker_door` |
| Сундук с отдельной крышкой | `/Game/InfinityBladeFireLands/Environments/Forge/Env_Forge/StaticMesh/SM_Forge_Chest_Bottom`, `SM_Forge_Chest_Top`; внешний вид пока демонстрационный |
| Стеллаж | `/Game/PostDistrict/Models/Structure/Furnitures/SM_Shelf_4X1`; масштаб 0.8 |
| Стол для осмотра | `/Game/ClassicMansion/Meshes/SM_DeskA` |
| Ключ | `/Game/InventorySystemPro/ExampleContent/Common/Art/Key/SM_HotelKey`; масштаб 0.075, исходный меш сильно увеличен |
| Книга | `/Game/Safe_House/meshes/SM_book_01` |
| Письмо | `/Game/Post_ap_city/Meshes/Post-apocalypse_vol2-square/props_shop/postal_1/SM_postal_letter_1` |
| Картинка/рамка | `/Game/Post_ap_city/Meshes/Post-apocalypse_vol2-square/props_shop/pharmacy_pops_1/SM_picture_1`; масштаб 0.2 |
| Радио для осмотра | `/Game/Safe_House/meshes/SM_rest_area_radio` |

Дополнительно найдены демонстрационные Blueprint-ассеты InventorySystemPro: `BP_InventoryDoor`, `BP_InventoryTriggerDoor`, `BP_ItemPickupChest`, `BP_InspectionViewerBase`, `BP_InspectionInteractionActor`, `BP_InspectionInteractionChestActor`. Они не размещались и не подключались: сначала нужен аудит их зависимости от системы набора и адаптация к нашему взаимодействию.

## Анимации

Наличие и скелеты перечисленных клипов проверены. Игровой прогон, ретаргет и подгонка рук к конкретной мебели не выполнялись.

| Действие | Кандидат | Проверено |
|---|---|---|
| Открытие двери с винтовкой | `/Game/RifleAnimsetPro/Animations/RootMotion/Rifle_OpenDoor` | 1.6 с, исходный UE4 Mannequin; нужна проверка/адаптация |
| Проход через дверь правой рукой | `/Game/MovementAnimsetPro/Animations/RootMotion/RM_WalkThroughDoor_RH` | 2.87 с, исходный UE4 Mannequin |
| Подбор правой рукой | `/Game/MovementAnimsetPro/Animations/RootMotion/RM_PickUp_RH` | 2 с, исходный UE4 Mannequin; есть и варианты LH |
| Подбор на средней высоте стоя | `/Game/AZ/Assets/Master/RifleMega/Rifle_Styly03_St/Rifle03_OtherAnims/AZ_MST_Rifle03_St_PickUp_Mid` | 2 с, SK_AZ_Master |
| Подбор с низкой высоты в приседе | `/Game/AZ/Assets/Master/RifleMega/Rifle_Cr/Rifle_Cr_OtherAnims/AZ_MST_Rifle_Cr_PickUp_Low` | 1.5 с, SK_AZ_Master; прежняя реплика о полном отсутствии приседа неверна |
| Оружейный осмотр | `/Game/FPS_Controller/Animations/FPP_Anims/Pistol/Inspect/A_FPP_X24_Inspect_01` | 4 с, скелет набора FPS_Controller; не подтверждает готовность осмотра произвольного предмета нашим героем |

Отдельный полноценный клип вытягивания мебельного ящика и универсальное удержание/осмотр фотографии пока не подтверждены. Способ осмотра (в руках в мире или отдельный viewer) выбирается на этапе дизайна. Наличие InspectionViewer не доказывает готовность нужной анимации рук.

Звуки подбора, движения/закрытия/запертой двери найдены в InventorySystemPro и ClassicMansion. Их уже можно рассматривать как исходные кандидаты; слух врагов повторно не создаётся.

## Проверка и восстановление

Сцену проверили двумя снимками редакторского viewport; материалы отобразились после загрузки шейдеров. Play не запускался.

- Расстановка и резервная копия карты: `Saved/InteractionAudit/placement-20260926-190849-569768/`.
- Скрипт размещения: `Tools/place_interaction_showcase.py`, повторный запуск использует собственные подписи/метки без дубликатов.
- Проверенные анимации: `Saved/InteractionAudit/animation_verified.json`.
- Общие результаты поиска: `asset_candidates.json` и `animation_candidates.json` в той же папке. Это широкий поиск по именам с ложными совпадениями, не перечень утверждённых ассетов.

Следующий шаг — пользователь выбирает визуально подходящие модели, затем проектируется поведение двери, контейнера и осмотра. Покупать ассеты для первого технического набора сейчас не требуется.

## Набор принят пользователем
Пользователь подтвердил: все размещённые образцы подходят для разработки механик; финальный внешний вид сейчас не является блокером. Повторно согласовывать модели не нужно. Следующий этап — конкретный дизайн взаимодействий на E для двери, контейнера и осмотра, согласно согласованному порядку дизайн → реализация. Само принятие моделей не означает, что эти действия уже подключены.

