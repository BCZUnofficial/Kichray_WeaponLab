# Параметры боеприпасов Kichray

Справочник проверен по **BCZUnofficial/kichray, master,
6d832035330a004a2ffabce8d124f17409dcc563**. Подписи редактора находятся в отдельной
функции `DescribeAmmunitionParameter`: сведения об оружии и настройки стенда
не подставляются в карточку боеприпаса. Редактор боеприпасов не запускает
симуляцию стрельбы. После выбора оружия его существующий стенд использует
значения выбранного совместимого патрона.

## Предмет, патрон и летящий снаряд

В этой ревизии `AMMO`, `A_VOG25`, `A_OG7B`, `A_M209` зарегистрированы как
`CWeaponAmmo`. Это предметы инвентаря, включая боеприпасы гранатомётов.
`G_FAKE` зарегистрирован как отдельный `CExplosiveRocket`, а ручные гранаты
относятся к другим классам. Название секции с префиксом `ammo_` само по себе
не определяет класс. Класс может наследоваться, например у
`ammo_12x76_zhekan_heli`.

Коэффициенты пуль ниже описывают цепочку `CCartridge` → `FireBullet` →
`CBulletManager`. Гранатомёт может запускать самостоятельный объект вместо
пули; наличие одинакового ключа в его боеприпасе не означает, что ключ
управляет взрывом этого объекта.

| Ключ | Значение и действие |
| --- | --- |
| `class` | Класс игрового предмета. Совместимость с оружием задаётся ссылками `ammo_class` / `grenade_class` у оружия. |
| `box_size` | Ёмкость пачки. В движке приводится к `u16`; для корректной массы требуется положительное значение. Не меняет размер магазина оружия. |
| `inv_weight` | Масса полной пачки в кг. Масса остатка = `inv_weight × m_boxCurr / box_size`. Это инвентарная масса, не физическая масса пули. |
| `cost` | Базовая стоимость предмета. Итоговая торговая цена может дополнительно изменяться правилами торговли. |
| `k_disp` | Множитель собственного углового разброса оружия. Вклад точности владельца добавляется отдельно. |
| `k_hit` | Множитель исходной силы попадания оружия. При столкновении участвуют скорость и свойства цели. |
| `k_impulse` | Множитель импульса попадания. Не является множителем начальной скорости. |
| `k_pierce` | Входит в `shoot_factor = материал.fShootFactor × k_pierce × speed / max_speed`, определяя прохождение материала и условие рикошета. |
| `k_ap` | AP пули, по умолчанию 0. Для пулевого урона по экипировке вклад брони = `броня кости × состояние экипировки × (1 − AP)`. Действует также нижний предел доли проходящего урона. Это отдельный параметр от `k_pierce`; не только MP. |
| `k_dist` | Множитель предельной длины пути `fire_distance`. Не меняет начальную скорость. |
| `impair` | Множитель износа оружия за один патрон. Списание состояния выполняется до расчёта разброса. |
| `buck_shot` | Число пуль/дробин на один патрон в `FireTrace`. Износ списывается один раз. У гранатомётных боеприпасов встречается 0, поскольку отдельный снаряд запускается другим кодом. |
| `tracer` | Разрешение видимого трассера, дополнительно ограниченное флагом трассеров оружия. |
| `tracer_color_ID` | Индекс `[tracers_color_table]`, по умолчанию 0; оружие может переопределить цвет. |
| `explosive` | Флаг частиц при попадании пули в статический материал. `PlayExplodePS` создаёт частицы; сам флаг не задаёт радиальный урон гранаты. |
| `wm_size` | Размер следа попадания, передаваемый рендеру. Требуется значение больше 0. Не является радиусом поражения. |
| `k_air_resistance` | В MP участвует в торможении с множителем `speed / max_speed`. При отсутствии загружается глобальный коэффициент. В SP используется непосредственно `[bullet_manager] air_resistance_k`, а не поле патрона. |
| `can_be_unlimited` | Разрешение участвовать в режиме неограниченных патронов, по умолчанию включено. Само включение не активирует такой режим. |
| `fake_grenade_name` | Ссылка на секцию отдельного снаряда для `SpawnRocket` у соответствующего гранатомёта. Модель и взрыв снаряда определяются этой секцией. |
| `k_speed` | Загрузчик патрона этой ревизии его не читает. Начальная скорость пули берётся у оружия. |

## Модель, текст, иконки и свойства инвентаря

| Ключ | Значение и действие |
| --- | --- |
| `visual` | Модель предмета в мире, относительно `gamedata/meshes`. Модель летящего снаряда может быть указана в другой секции. |
| `cform` | Наличие ключа вызывает создание скелетной формы столкновений предмета по модели. Не определяет конус стрельбы. |
| `inv_name`, `inv_name_short`, `description` | Ключи строковой таблицы; движок получает текст через `CStringTable`. Краткое имя также хранится у патрона в магазине оружия. |
| `inv_grid_x`, `inv_grid_y` | Левый верхний угол иконки в клетках, начиная с 0. Клетка **50×50 пикселей** независимо от размера DDS. |
| `inv_grid_width`, `inv_grid_height` | Размер области иконки и предмета в сетке инвентаря, в тех же клетках. |
| `kill_msg_x`, `kill_msg_y`, `kill_msg_width`, `kill_msg_height` | Прямоугольник иконки сообщения об убийстве в MP. Это **пиксели**, не клетки `inv_grid`; отсутствующие значения равны 0. |
| `immunities_sect` | Секция коэффициентов стойкости самого предмета; не коэффициенты урона его пули. |
| `belt` | Флаг допуска размещения на поясе; default `false`. |
| `default_to_ruck` | Флаг помещения в рюкзак по умолчанию; default `true`. |
| `can_take` | Флаг возможности подбора; default `true`. |
| `can_trade` | Флаг торговли; default `true`. Квестовый статус проверяется дополнительно. |
| `quest_item` | Квестовый статус; default `false`. В частности, запрещает обычную торговлю. |
| `slot` | Заявленный слот инвентарного предмета; default `NO_ACTIVE_SLOT`. Это не индекс типа патронов в оружии. |
| `item_remove_time` | Временной порог проверки удаления независимого предмета, в миллисекундах. Остальные условия удаления проверяются отдельно. |
| `$spawn` | Имя в каталоге объектов редактора уровней. Не название для игрока. |

Для `jump_height`, `$prefetch`, `GroupControlSection`, `discovery_dependency`
в проверенных потребителях не подтверждено игровое действие. Редактор прямо
показывает отсутствие проверенного описания, как и для неизвестных ключей
модов. В частности, справочник не приписывает `jump_height` изменение высоты
подскока, а `$prefetch` — изменение поведения пули. Отсутствие описания
не означает доказанного отсутствия использования во всех скриптах мода.

## Исходники проверенной ревизии

- [WeaponAmmo.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/WeaponAmmo.cpp#L29-L62): поля патрона и значения по умолчанию; строки 74–100 — пачка, 232–239 — пропорциональная масса.
- [WeaponDispersion.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/WeaponDispersion.cpp#L17-L45) и [WeaponFire.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/WeaponFire.cpp#L57-L95): разброс, порядок износа, дробины и ограничение трассера оружием.
- [Level_Bullet_Manager.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/Level_Bullet_Manager.cpp#L31-L66): поля пули; строки 123–133 — частицы, 275–284 — различие сопротивления SP/MP.
- [Level_bullet_manager_firetrace.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/Level_bullet_manager_firetrace.cpp#L214-L299): следы и эффект `explosive`; строки 449–536 — сила, импульс, материалы и рикошет.
- [EntityCondition.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/EntityCondition.cpp#L277-L297) и [CustomOutfit.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/CustomOutfit.cpp#L98-L104): применение AP к экипировке.
- [inventory_item.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/inventory_item.cpp#L118-L153): текст, масса, цена и флаги; строки 1084–1109 — удаление/торговля, 1111–1146 — координаты иконок.
- [UICellCustomItems.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/ui/UICellCustomItems.cpp#L6-L19) и [UIInventoryUtilities.h](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/ui/UIInventoryUtilities.h#L9-L10): размер клетки; [game_cl_mp.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/game_cl_mp.cpp#L773-L777) — пиксельный прямоугольник `kill_msg`.
- [xr_object.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xr_object.cpp#L150-L185): `visual` и `cform`.
- [Weapon.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/Weapon.cpp#L1527-L1533): ограничения неограниченных патронов.
- [WeaponMagazinedWGrenade.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/WeaponMagazinedWGrenade.cpp#L125-L149) и [WeaponRG6.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/WeaponRG6.cpp#L23-L34): создание отдельного снаряда.
- [object_factory_register.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/object_factory_register.cpp#L290-L293): классы боеприпасов; строка 326 — `G_FAKE`.
- [ESceneSpawnTools.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/editors/LevelEditor/Edit/ESceneSpawnTools.cpp#L46-L48): `$spawn` в редакторе уровней.
