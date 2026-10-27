# Сборка Kichray WeaponLab 0.2.0

## Windows / Visual Studio 2022

1. Установить Visual Studio 2022 или Build Tools 2022 с рабочей нагрузкой
   «Разработка классических приложений на C++», MSVC v143 и Windows 10/11 SDK.
2. Распаковать весь архив в отдельную папку. Зависимость Dear ImGui уже входит
   в `third_party/imgui`; отдельный скачиваемый SDK DirectX не требуется.
3. Открыть `Kichray_WeaponLab.sln`.
4. Выбрать `Release` и `x64`, затем «Сборка → Собрать решение».
5. Запустить `bin/Release/x64/WeaponLab.exe`. Рядом должна оставаться папка
   `assets` с `Preview.hlsl`; проект копирует шейдер после сборки автоматически.

Командная строка: открыть **x64 Native Tools Command Prompt for VS 2022**,
перейти в папку проекта и запустить `build_windows.bat`.

Иконка и заставка встроены в EXE через ресурсный файл. Исходные изображения и
шейдер рендера перенесены из Material Lab 0.13.0 без изменения.

## CMake

Для Visual Studio 2022 используйте CMake 3.21+ (для переносимых проверок достаточно 3.16+):

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Приложение: `build/Release/WeaponLab.exe`; шейдер будет скопирован в
`build/Release/assets/Preview.hlsl`.

## Переносимые проверки без Windows

GCC/Clang с поддержкой C++17 и Python 3:

```sh
python3 build_checks.py
```

С ASan/UBSan:

```sh
ASAN_OPTIONS=detect_leaks=0 python3 build_checks.py --sanitize --build-dir build-sanitize
```

Через CMake:

```sh
cmake -S . -B build -DWEAPONLAB_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Восемь переносимых наборов проверяют загрузчики, конфиги, текстовую таблицу,
атлас, математику камеры, лабораторию стрельбы и интерфейс ImGui. Они не запускают
Windows/D3D11 и не заменяют проверку приложения на видеокарте с собственными
игровыми файлами.

## Что входит в исходники

- `src/WeaponData.*` — конфиги LTX, секции, наследование и XML-тексты.
- `src/WeaponSimulation.*` — лабораторные формулы стрельбы из `kichray/master`.
- `src/IconAtlas.*` — клеточная сетка, альфа-канал, подготовка и запись иконок.
- `src/WeaponUI.*`, `src/WinApp.cpp` — интерфейс и Windows-приложение.
- `src/Renderer.*`, `src/OGF.cpp`, `src/DDS.cpp` — просмотр моделей и текстур.
- `assets` — исходные значок, заставка и шейдер.
- `tests` — тесты новой программы и используемых загрузчиков.

Редактор материалов, кисти, пресеты материалов и сессии Material Lab в сборку
WeaponLab не входят.

## Проверка D3D11 после сборки Windows

В командной строке из папки готового приложения:

```bat
WeaponLab.exe --smoke-test
```

Проверка использует программное устройство D3D11/WARP и тестовый куб, проверяет
шесть направлений камеры, исключение скрытого меша из прозрачного снимка,
восстановление видимости и сохранение PNG через Windows WIC.
Файл `weaponlab-smoke.txt` создаётся рядом с EXE. Код завершения `0` означает,
что все проверки прошли. Эта проверка не загружает и не изменяет gamedata.

## Источники формата ТЧ

Правила сетки и связи с конфигом проверены по историческим исходникам X-Ray
для Shadow of Chernobyl; это архив разработки, а не гарантия побайтового
соответствия конкретному игровому EXE 1.0006:

- [UIInventoryUtilities.h](https://github.com/OpenXRay/xray-soc-history/blob/58aaa5185f7a682b8cf5f5f376a2e5b6ca16fed4/xr_3da/xrGame/ui/UIInventoryUtilities.h): клетка инвентарной текстуры — 50×50 пикселей.
- [inventory_item.cpp](https://github.com/OpenXRay/xray-soc-history/blob/58aaa5185f7a682b8cf5f5f376a2e5b6ca16fed4/xr_3da/xrGame/inventory_item.cpp): `inv_grid_x`, `inv_grid_y`, `inv_grid_width`, `inv_grid_height` читаются как целые значения; `inv_name` и `description` проходят через таблицу строк.

Размер клетки не зависит от размера DDS. В атласе 1024×1024 помещается
20×20 полных клеток; в 2048×2048 — 40×40. Оставшиеся пиксели по краям сохраняются.
Большие атласы доступны прокруткой, координаты больше 19 допустимы.

## Автоматическая сборка GitHub Actions

В `.github/workflows/ci.yml` включены два задания:

- Windows Server 2022: сборка MSVC x64, все переносимые тесты и нативная проверка
  D3D11/WARP и PNG; при успехе выдаётся артефакт `WeaponLab-Windows-x64`.
- Ubuntu: переносимые тесты с AddressSanitizer и UndefinedBehaviorSanitizer.

Workflow запускается при push в master, pull request или вручную из Actions.
Он не работает с пользовательской gamedata. Наличие workflow в исходниках
само по себе не означает, что конкретная ревизия уже прошла эти проверки.
Текущий подтверждённый результат указан в `verification/BUILD_STATUS_RU.txt`.
