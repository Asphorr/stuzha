<div align="center">

# Стужа

*Stuzha* (“bitter cold”): an isometric winter-night survival game written
entirely in x86-64 assembly.

![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20x64-555)
![Language](https://img.shields.io/badge/language-NASM-6e4a7e)

![Night in the village](docs/night.png)

</div>

---

## English

A Siberian village on a January night. The dead wander the streets; you have a
shovel, a flashlight and a warm izba somewhere nearby. Survive from 17:20 until
dawn at 08:00 (about nine and a half real minutes).

There is no engine, no libraries beyond the Windows system DLLs, and no asset
files. The village, sprites, light, weather and sound are all produced by
~24.5k lines of hand-written NASM. The game is a single 217 KB executable.

**What's inside**

- **Software renderer.** 640×360, scaled to the window. It uses a G-buffer
  (albedo, normals, world position) with deferred lighting on SSE2: moonlight
  with contact-hardening soft shadows from a height map, sodium street lamps
  and lit windows baked with ray-cast shadows, and a flashlight. It also has a
  view cone that greys out what you cannot see, bloom and a frost overlay.
- **The world.** Gabled snow roofs are rasterized in world planes with
  per-pixel depth. There are snow drifts with normals, and footprints and blood
  that stay in the snow. Walls are cut away around the player, and collisions
  follow the sprite shapes.
- **Wind as a system.** Each night has its own weather script: calm, then
  ground drift, a blizzard and a morning breeze. The gust field is sheltered
  behind houses, fences and trees, and it moves snowfall, drifting snow, chimney
  smoke, trees and wires. Zombies hear you farther downwind and smell your
  scent trail. The wind slows you when you walk into it and chills you until
  you get to a stove.
- **Synthesized sound.** A waveOut thread mixes it all in real time from
  filters and oscillators, with no samples. It includes wind that differs
  between your left and right ear, howling at corners and fences, singing
  wires, and footsteps on snow, packed road and floorboards. You also hear
  the shovel, zombie voices, dogs and stove crackle. A TV mumbles through a
  wall. Sounds carry with the wind and are muffled by walls.

**Controls**

| Key | Action |
|---|---|
| WASD | walk |
| Shift | run |
| LMB / Space | swing the shovel |
| F | flashlight |
| M | sound on/off |
| F11 / Alt+Enter | fullscreen |
| F3 | show hitboxes |
| R / Esc | restart / quit (after the night ends) |

**Building**

You need [NASM](https://nasm.us) 2.15+ and mingw-w64 binutils
(`x86_64-w64-mingw32-ld`), either on `PATH` or inside WSL:

```sh
sh build.sh        # -> build/stuzha.exe
```

**Checking without playing**

The game can render fixed-seed scenes to files, which is how it was developed:

| Flag | Output |
|---|---|
| `--shot1` … `--shot9`, `--shot0` | `build/shotN.bmp`: night, dusk, inside an izba, walk, fight, death, yard, flashlight, dawn, behind a house |
| `--shotw`, `--shotb`, `--shotz` | wind at 21:40, a blizzard, the scent test (`zlog.bin`) |
| `--wav` (with a shot) | also records the scene's sound to `shotN.wav` |
| `--sndtest` | every sound event and ambience in a row → `sndtest.wav` |
| `--hb`, `--lm`, `--wind`, `--soak` | hitboxes, baked lamp light, wind field from above, 3000-frame soak |
| `--fs`, `--alog` | start fullscreen; log audio buffer underruns to `alog.bin` |

`build/topng.ps1 -n "1,b"` converts shots to PNG, and `build/bench.ps1 -n 1`
gives per-pass render timings.

## Русский

Сибирское село январской ночью. По улицам бродят мёртвые, у тебя лопата,
фонарь и где-то рядом тёплая изба. Нужно дожить с 17:20 до рассвета в 08:00,
это примерно девять с половиной минут.

Ни движка, ни библиотек, кроме системных DLL Windows, ни файлов с ресурсами.
Село, спрайты, свет, погода и звук целиком получаются из ~24.5 тыс. строк NASM,
написанных руками. Вся игра — один exe на 217 КБ.

**Что внутри**

- **Софтверный рендер.** 640×360 с масштабом под окно. Это G-буфер (цвет,
  нормали, мировые координаты) и отложенное освещение на SSE2: луна с мягкими
  тенями по карте высот, натриевые фонари и окна с запечёнными тенями, фонарик.
  Ещё там конус обзора (невидимое — серой памятью), блум и иней по краям экрана.
- **Мир.** Двускатные крыши в снегу растеризуются в мировых плоскостях с
  попиксельной глубиной. Есть сугробы с нормалями, следы и кровь, которые
  остаются на снегу. Стены срезаются вокруг игрока, а столкновения повторяют
  форму спрайтов.
- **Ветер как система.** У каждой ночи свой сценарий погоды: тишина, потом
  позёмка, метель и утренний ветерок. Поле порывов с затишьем за избами,
  заборами и деревьями двигает снег, позёмку, дым, деревья и провода. Зомби
  слышат дальше по ветру и чуют запах. Против ветра идти тяжелее, а без печки
  замерзаешь.
- **Синтезированный звук.** Поток на waveOut в реальном времени смешивает всё
  из фильтров и генераторов, без сэмплов. Ветер в левом и правом ухе разный,
  воет на углах и заборах, гудят провода. Шаги звучат по-своему на снегу,
  укатанной дороге и половицах. Ещё слышно лопату, голоса зомби, собак, треск
  печки и телевизор за стеной. Звуки несёт ветром и глушат стены.

**Управление:** WASD — идти, Shift — бежать, ЛКМ/Пробел — лопата, F — фонарь,
M — звук, F11 или Alt+Enter — полный экран, F3 — хитбоксы; после конца ночи
R — заново, Esc — выход.

**Сборка:** нужны NASM 2.15+ и mingw-w64 binutils (`x86_64-w64-mingw32-ld`) —
в `PATH` или в WSL. Команда `sh build.sh` собирает `build/stuzha.exe`.

**Проверка без игры:** флаги `--shotN`, `--wav`, `--sndtest` и остальные из
таблицы выше пишут снимки сцен и их звук в файлы. Так игра и разрабатывалась.

## Screenshots

| | |
|---|---|
| ![Blizzard](docs/blizzard.png) | ![Inside an izba](docs/izba.png) |
| ![Yard at 21:00](docs/yard.png) | ![Fight](docs/fight.png) |

## License

MIT, see [LICENSE](LICENSE).
