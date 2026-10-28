# HUD и статичные эффекты — WeaponLab 0.4.1

Опорный движок: `BCZUnofficial/kichray`, master
`6d832035330a004a2ffabce8d124f17409dcc563`. Проверены исходники игрового HUD,
рендеров R1/R2 и библиотеки `xrParticles`. Соответствие другим форкам не заявляется.

## Преобразования и камера

- `CHudItem::Load` читает имя HUD-секции из оружейного `hud`.
- `weapon_hud_value::load`: `position` в метрах; `orientation` переводится
  из градусов в радианы и передаётся в `setHPB(x,y,z)` — heading вокруг Y,
  pitch вокруг X, bank вокруг Z. Это не обычный порядок вращений XYZ.
- В режиме HUD устранена нормализация меша, используемая world-просмотрщиком.
  Камера неподвижна в начале координат, смотрит вдоль +Z. Вращение всего HUD
  не затрагивает отдельные части, вершины и OGF на диске.
- `CWeaponHUD::UpdatePosition` составляет матрицу камеры и HUD offset.
  В покое матрица камеры единичная. Прицеливание добавляет rotateX/rotateY
  и zoom_offset; `zoom_rotate_x/y` уже выражены в радианах.
- HUD-проекция использует вертикальный FOV `cameraFov * hud_fov`, ближнюю
  плоскость 0.2 м и диапазон глубины 0..0.02, как `r_dsgraph_render_hud`/`rmNear`.
  В движке обычные частицы `CParticlesObject::renderable_Render` используют FOV
  камеры. В редакторе 0.4.1 метки и статичные эффекты намеренно используют
  ту же HUD-проекцию и глубину, что и модель: это стенд расстановки точек,
  и при повороте/изменении FOV они должны оставаться на поверхности оружия.
- Дальность отсечения стенда 100 м. Уровень, скорость персонажа, инерция,
  отдача и смешивание анимаций отсутствуют. Выбирается один неподвижный кадр
  OGF/OMF или bind pose. Автоматически выбирается `anim_idle`, если он загружен.
- FOV не читается автоматически из пользовательского `user.ltx`; 67.5 и 0.45 —
  начальные настройки редактора, доступные для ручного изменения. Зум FOV
  конкретного оружия также задаётся вручную; кнопка прицеливания применяет
  только положение/поворот. Кадр можно ограничить 16:9, 4:3 или размерами окна.

Для HUD `fire_point`, `fire_point2` и `shell_point` сначала преобразуются
матрицей `fire_bone` выбранной позы, затем матрицей всего HUD. Направление
выстрела — локальная +Z всего HUD; поворот fire_bone не заменяет направление
эмиттера. `Weapon.cpp` восстанавливает базис эффекта через мировую вертикаль.
В стенде 0.4.1 полный базис HUD, включая bank, сохраняется для всех эффектов,
чтобы они поворачивались вместе с оружием по всем трём осям. Локальные
координаты точек в LTX при этом не переписываются и не накапливают поворот.
Для World эти точки находятся непосредственно в координатах OGF.
`fire_point2` при отсутствии наследует `fire_point`.

## Эффекты и запись конфигов

Подключения `flame_particles`, `smoke_particles`, `shell_particles`,
`flame_particles_2`, `silencer_flame_particles`, `silencer_smoke_particles`
записываются в выбранную оружейную секцию. Позиции HUD пишутся в HUD-секцию.
Строка со значением, которое отсутствует в библиотеке, не подключается кнопкой
выбора. Отсутствующий DDS явно сообщается, текстура другого эффекта не подставляется.

«Показывать в сцене» не меняет LTX. «В игре» меняет рабочую копию параметра;
файл записывается общей кнопкой сохранения, с прежней системой резервных копий.
При выключении создаётся пустое значение (без кавычек). В `CInifile` оно
становится NULL, поэтому `CShootingObject` не создаёт эффект. Это также
перекрывает наследование. Последнее подключение запоминается в текущем сеансе;
после нового запуска для выключенного эффекта потребуется выбрать имя снова.

При подключении гильз проверяется существование числового `shell_point` и у
оружия, и в HUD: движок читает эти значения при наличии строки shell_particles.
Недостающие HUD shell_point/fire_point2 показаны как редактируемые начальные
значения; строка создаётся при фактической правке. Отключение гильз не удаляет
точки. Обычный дым и пламя используют одну fire_point: отдельного smoke_point
в проверенной реализации нет. `shot_particles` остаётся обычным параметром:
он использует текущую позицию стрельбы `m_vCurrentShootPos`, а не гарантированно
fire_point, поэтому не подменён дополнительным редакторским эмиттером.

## Чтение particles.xr и пределы предпросмотра

Поддержан chunked `particles.xr` v1: контейнер PED 0x0003, PGD 0x0004,
PED v1, PGD v3, оба варианта списка элементов PGD. Читаются имена, текстура,
sprite shader, флаги, кадры, лимит времени, размеры, скорость и список PAPI.
Предпросмотр использует DDS из textures; библиотека и DDS эффектов не меняются.
Неиспользуемые поля pDomain и SFrame.reserved пропускаются: конструкторы
движка не обязаны их инициализировать, хотя Save пишет всю структуру.
Поля, участвующие в расчёте, по-прежнему проверяются на NaN/Inf. Перед стартом
источника сбрасывается runtime-флаг flSilent, как в PlayEffect движка.
Сжатые LZH-блоки отклоняются с пояснением; старые first-generation definitions
не интерпретируются. Отсутствие библиотеки не мешает редактировать LTX и модели.

Статичный кадр рассчитан с шагом 0.033 с, временем 0..4 с и фиксированным seed.
«Автокадр для коротких эффектов» по умолчанию включён: если выбранный момент
пустой, находится первый видимый кадр в пределах 4 секунд. Фактическое время
показывается в диагностике. Отключение автокадра возвращает точное выбранное
время без подмены. Время можно менять и в окне выбора. Кадры кешируются,
поэтому вращение HUD не запускает поиск заново.
Реализованы Source (все 11 доменов), CopyVertexB (не влияет на рисунок стенда),
Damping, Gravity, KillOld, Move, RandomAccel/Displace/Velocity, SpeedLimit,
TargetColor/Size/Rotate/Velocity и варианты D. Sink/SinkVelocity исполняются
для point/plane/box/sphere. Результат детерминирован в редакторе; последовательность
RNG и квантование цвета не обещают побитового совпадения с игрой.

Прочие действия (Avoid/Bounce, Explosion, Follow/Gravitate/MatchVelocity, Jet,
Orbit, Restore, Vortex, Turbulence, Scatter), оставшиеся домены Sink и
RandomPlayback сопровождаются явными сообщениями об упрощении. Не реализованы
привязанные к частицам дочерние OnPlay/OnBirth/OnDeath эффекты и столкновения
с геометрией уровня. Основные элементы групп учитывают start/end, enabled и
immediate/deferred stop. Время вблизи окончания PED может отличаться на один шаг
от игры из-за остановки источника; этот стенд предназначен для расстановки,
не для оценки жизненного цикла эффекта.

Лимиты предпросмотра: 4096 частиц на PED, 64 слоя на PGD, 256 МиБ на библиотеку.
Sprite-проход различает BLEND (SRC_ALPHA/INV_SRC_ALPHA), ADD (ONE/ONE) и
ALPHA-ADD (SRC_ALPHA/ONE), как Blender_Particle. Выбор по стандартным именам:
`aadd`, `alpha_add`, `alpha-add`, `alphaadd` — ALPHA-ADD; остальные имена
с `add` — ADD; остальные — BLEND. ADD не гасится нулевым alpha DDS.
Само содержимое shaders.xr не исполняется: произвольные имена, multiply,
нестандартные шейдеры, soft particles, distortion и освещение могут выглядеть
иначе. Частицы располагаются в 3D и проверяются по глубине; миниатюра в окне
выбора является фронтальным sprite-превью с автоматическим масштабом.
Сортировка прозрачности выполняется внутри слоя. Поза/привязка настраиваются
по этим кадрам и меткам; окончательный вид сложных эффектов проверяется в игре.

## Проверенные первичные исходники

Все ссылки закреплены на одном коммите:

- [WeaponHUD.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/WeaponHUD.cpp)
- [HudItem.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/HudItem.cpp)
- [Weapon.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/Weapon.cpp)
- [_matrix.h](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xrCore/_matrix.h)
- [ShootingObject.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrGame/ShootingObject.cpp)
- [Xr_ini.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xrCore/Xr_ini.cpp)
- [r__dsgraph_render.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrRender/r__dsgraph_render.cpp), [device.h](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/device.h), [R2 rmNear](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrRender_R2/r2.cpp)
- [PSLibrary.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrRender/PSLibrary.cpp), [ParticleEffectDef.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrRender/ParticleEffectDef.cpp), [ParticleGroup.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrRender/ParticleGroup.cpp)
- [particle_actions_collection_io.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xrParticles/particle_actions_collection_io.cpp), [particle_actions_collection.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xrParticles/particle_actions_collection.cpp), [particle_core.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xrParticles/particle_core.cpp)

## Регрессии 0.4.1

Проверены старый и новый код на одинаковых бинарных fixtures:

- NaN в SFrame.reserved: раньше библиотека не загружалась, теперь загружается.
- NaN в неиспользуемых полях точечного pDomain: раньше получался пустой кадр,
  теперь источник создаёт частицы.
- Сохранённый flSilent: раньше 0 частиц, теперь источник запускается как PlayEffect.
- Вспышка, закончившаяся к 165 мс: автокадр выбирает живой кадр 33 мс;
  ручной режим сохраняет пустой кадр 165 мс.
- ADD с нулевой альфой видим; ALPHA-ADD с нулевой альфой остаётся прозрачным.
- Три точки после fire_bone совпадают с преобразованием всей модели при
  heading/pitch/bank, смешанном повороте и прицеливании.

Нативный smoke-test дополнен проверкой совпадения пикселя эффекта с
проекцией его точки после поворотов HUD и проверкой ADD с нулевой альфой.
Он требует Windows; переносимые тесты не подменяют его запуск.

Дополнительно сверены [particle_manager.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xrParticles/particle_manager.cpp),
[ParticleEffectActionsImpl.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/editors/ParticleEditor/ParticleEffectActionsImpl.cpp)
и [Blender_Particle.cpp](https://github.com/BCZUnofficial/kichray/blob/6d832035330a004a2ffabce8d124f17409dcc563/xr_3da/xrRender/Blender_Particle.cpp).
