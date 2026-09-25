; ----------------------------------------------------------------------------
;  const.inc — константы, макросы, импорт WinAPI
; ----------------------------------------------------------------------------

SCR_W       equ 640             ; внутренний кадр
SCR_H       equ 360
WIN_W       equ 1280            ; окно (x2, nearest)
WIN_H       equ 720
CX0         equ 320             ; экранная точка камеры
CY0         equ 196
MAPW        equ 128             ; карта 128x128 тайлов
GW          equ 184             ; сетка света: 4 сэмпла на тайл, окно 46x46 тайлов
SLW         equ 512             ; статический свет: 4 сэмпла на тайл, вся карта
DECW        equ 1024            ; следы/кровь: 8 ячеек на тайл
MAXE        equ 256             ; слоты сущностей (зомби + трупы)
ESZ         equ 80
NFLK        equ 2400            ; снежинки (летит часть — по снегопаду)
FL_SZ       equ 32              ; снежинка: u, v, z, падение, vu, vv, фаза, отклик
MAXL        equ 256             ; источники статического света
L_SZ        equ 128
GLIM        equ (GW-2)*64+63    ; предел координаты для билинейки
LOSN        equ 512             ; шагов луча видимости не больше (32767 >> 6)

; тайлы
T_SNOW      equ 0
T_ROAD      equ 1
T_WOOD      equ 2
T_WALL      equ 3
T_WIN       equ 4
T_WINL      equ 5
T_FENCE     equ 6
T_SPRUCE    equ 7
T_BIRCH     equ 8
T_LAMP      equ 9
T_STOVE     equ 10
T_TABLE     equ 11
T_PILE      equ 12
T_DOOR      equ 13
T_STOVEL    equ 14
T_CROSS     equ 15
T_SNOWMAN   equ 16
T_BENCH     equ 17
T_LINE      equ 18
T_KENNEL    equ 19
T_BIRDH     equ 20
T_WELL      equ 21
T_BED       equ 22                  ; мебель избы (interior.inc): кровать — два тайла
T_CUPB      equ 23                  ; шкаф или буфет
T_CHEST     equ 24                  ; сундук
T_LAVKA     equ 25                  ; лавка у стены
T_PINE      equ 26                  ; лес (veg.inc): сосна, подрост ели, куст/бурьян, пень
T_YOUNG     equ 27
T_BUSH      equ 28                  ; не твёрдый
T_STUMP     equ 29
NTILE       equ 32

; виды отрисовки
D_NONE      equ 0
D_WALL      equ 1
D_WIN       equ 2
D_WINL      equ 3
D_FENCE     equ 4
D_STOVE     equ 5
D_TABLE     equ 6
D_PILE      equ 7
D_SPRUCE    equ 8
D_BIRCH     equ 9
D_LAMP      equ 10
D_STOVEL    equ 11
D_CROSS     equ 12
D_SNOWMAN   equ 13
D_BENCH     equ 14
D_LINE      equ 15
D_KENNEL    equ 16
D_BIRDH     equ 17
D_WELL      equ 18
D_BED       equ 19                  ; мебель — коробками (furn.inc)
D_CUPB      equ 20
D_CHEST     equ 21
D_LAVKA     equ 22
D_PINE      equ 23                  ; растения — объёмные картинки (veg.inc, vegdraw.inc)
D_YOUNG     equ 24
D_BUSH      equ 25
D_STUMP     equ 26

; растения: породы и объёмные картинки (veg.inc)
SP_SPRUCE   equ 0
SP_PINE     equ 1
SP_BIRCH    equ 2
SP_YOUNG    equ 3
SP_BUSH     equ 4
SP_STUMP    equ 5
NSP         equ 6
RS_W        equ 0                   ; картинка: ширина, высота
RS_H        equ 4
RS_AX       equ 8                   ; столбец и строка точки основания
RS_AY       equ 12
RS_PX       equ 16                  ; -> альбедо (дворды, 0 — пусто)
RS_PN       equ 24                  ; -> нормали (байты)
RS_PO       equ 32                  ; -> точка в мире от основания: x | y << 16 (слова, 1/256 тайла)
RS_HM       equ 40                  ; -> высоты над основанием (байты px), сетка 1/32 тайла
RS_HS       equ 48                  ; её сторона (нечётная), основание в середине
RS_R        equ 52                  ; полуширина на экране, px
RS_AMP      equ 56                  ; качание на ветру у макушки, px·16
RS_SZ       equ 64
VJ_OFF      equ 36                  ; сдвиг основания по сетке 3x3: −36, 0, 36 (1/256 тайла)

; таблица спрайтов тайлов (индекс = вид - D_SPRUCE)
SPT_PTR     equ 0
SPT_WH      equ 8
SPT_AX      equ 12
SPT_AMP     equ 16                  ; размах качания, px*16
SPT_MODE    equ 20                  ; 0 нет, 1 дерево (низ неподвижен), 2 висит (верх неподвижен)
SPT_DITH    equ 24                  ; можно просвечивать перед игроком
SPT_NB      equ 28                  ; нормали: N_BL / N_SM / N_SB (насколько сдвигать выборку луны)
SPT_SZ      equ 32

; частицы
MAXP        equ 1536
P_SZ        equ 32
P_X         equ 0
P_Y         equ 4
P_Z         equ 8
P_VX        equ 12
P_VY        equ 16
P_VZ        equ 20
P_LIFE      equ 24
P_KIND      equ 28
P_SEED      equ 29
P_WS        equ 30                  ; дым: сила ветра при выпуске ·100
PK_BREATH   equ 1
PK_SMOKE    equ 2
PK_PUFF     equ 3
PK_KICK     equ 4
PK_SPRAY    equ 5                   ; снег, сорванный ветром с конька

; ветер: карта заслонителей и затишья — 2 ячейки на тайл, вся карта
WSW         equ 256

; блум: четверть разрешения
BLW         equ SCR_W/4
BLH         equ SCR_H/4

; тени луны: карта высот и развёртка — 8 ячеек на тайл
HPAD        equ 256                 ; нулевые поля карты высот, ячейки
HW8         equ 1024 + 2*HPAD       ; строка карты высот
MG8         equ 64                  ; поля развёртки вокруг окна сетки света (8 тайлов)
MW8         equ 2*GW                ; окно сетки света в ячейках
SW8         equ MW8 + 2*MG8         ; сторона развёртки
MLIM        equ (SW8-1)*32 - 1      ; предел координаты для выборки 2x2
SWD         equ (SW8 + 8)*4         ; дорожка развёртки с рамкой 4 ячейки, байты
MFLR        equ 24                  ; луч над пустым снегом не опускается ниже −MFLR px
MAXSTAMP    equ 1400                ; ячейки фигур, вписанные в карту на кадр

; нормали G-буфера (nbuf): индекс в таблицах луны/неба; бит 7 — свой свет в lbuf
NN          equ 115
NOWN        equ 0x80
N_UP        equ 0                   ; вверх: земля, верхушки
N_FY        equ 1                   ; грань +y (левая на экране)
N_FX        equ 2                   ; грань +x (правая)
N_LYU       equ 3                   ; бревно +y: верх, низ; +x: верх, низ
N_LXU       equ 5
N_BL        equ 7                   ; спрайт: левая, средняя, правая треть
N_BC        equ 8
N_RF        equ 10                  ; скаты: к +y, к -y, к +x, к -x
N_RV        equ 14                  ; варианты скатов: (круче, положе) x 4
N_DY        equ 22                  ; сугроб у грани +y, у +x
N_DX        equ 23
N_G0        equ 24                  ; рельеф снега 5x5 без центра
N_SM        equ 48                  ; мелкие спрайты (фонарь, крест, снеговик, лавка...) — как N_BL
N_SB        equ 51                  ; крупные (ель, берёза, колодец)
N_S0        equ 54                  ; сфера нормалей (деревья, фигуры): 5 высот x 12 азимутов,
NAZ         equ 12                  ; id = N_S0 + высота·NAZ + азимут; азимут k — 15° + 30°·k
NEL         equ 5                   ; от +x к +y, высоты −55°, −25°, 5°, 33°, 60°
N_SUP       equ N_S0 + NAZ*NEL      ; и строго вверх (своя, со сдвигом тени, не как земля)

; избы и крыши
MAXH        equ 48
HS_SZ       equ 128
HS_X0       equ 0                   ; тайлы
HS_Y0       equ 4
HS_W        equ 8
HS_H        equ 12
HS_AX       equ 16                  ; 0 — конёк вдоль x, 1 — вдоль y
HS_P        equ 20                  ; уклон, px высоты на тайл
HS_FL       equ 24
HS_SX       equ 28                  ; печь (труба), тайл
HS_SY       equ 32
HS_SEED     equ 36
HS_GAB      equ 40                  ; краска фронтона, 0xRRGGBB
HS_MAT      equ 44                  ; кровля под снегом: 0 тёс, 1 шифер, 2 зелёное железо, 3 рыжее
HS_OCC      equ 48                  ; прошлый кадр: крыша закрывала фигуру игрока
HS_FADE     equ 52                  ; f32: 1 — крыша есть, 0 — снята (игрок внутри)
HS_ZT       equ 56                  ; верх трубы, px
HS_ZR       equ 60                  ; снег на коньке, px
; убранство (interior.inc) — от хеша места, как облик крыши
HS_IVIS     equ 64                  ; кадр: нутро видно (крыша снята, тает или прорежена)
HS_WALL     equ 68                  ; стены изнутри: 0 брёвна, 1 побелка с панелью, 2 обои, 3 вагонка
HS_WCOL     equ 72                  ; цвет панели / обоев / вагонки, 0xRRGGBB
HS_WMOT     equ 76                  ; узор обоев
HS_CURT     equ 80                  ; шторы
HS_CARP     equ 84                  ; ковёр на стене: палитра 0..3
HS_CLOTH    equ 88                  ; стол: 0 голый, 1 белая скатерть, 2 клеёнка в клетку, 3 зелёная
HS_BLNK     equ 92                  ; одеяло: палитра 0..3
HS_WOOD     equ 96                  ; дерево мебели, 0xRRGGBB
HS_ONTAB    equ 100                 ; на столе (без телевизора): 0 ничего, 1 самовар, 2 кринка с геранью
HS_CHPAL    equ 104                 ; краска сундука, 0xRRGGBB
; мебель в hflags (биты 0-1 — свет и телевизор всего дома)
FM_ROT      equ 0x0C                ; к какой стене спиной: 0 −y, 1 −x, 2 +y, 3 +x (<< 2)
FM_HEAD     equ 0x10                ; кровать: изголовье на этом тайле
FM_ALT      equ 0x20                ; кровать: пара — к +u; шкаф: буфет
FM_OPEN     equ 0x40                ; сундук, шкаф: обысканы
FM_IRON     equ 0x80                ; кровать железная
; лут (interior.inc)
LO_SZ       equ 16
LO_X        equ 0                   ; f32, тайлы
LO_Y        equ 4
LO_Z        equ 8                   ; байт: над полом, px
LO_T        equ 9                   ; вид: 1 батарейки, 2 бинт, 3 термос, 4 тушёнка
LO_S        equ 10                  ; 0 нет, 1 лежит на виду, 2 в сундуке или шкафу
LO_H        equ 11                  ; дом + 1
LO_C        equ 12                  ; тайник: y·128 + x
MAXLOOT     equ 160
NLT         equ 4
HF_LIT      equ 1
HF_TV       equ 2
HF_SLIDE    equ 4                   ; снег съехал с края ската
HF_CROW     equ 8                   ; вороньи следы
HF_WASH     equ 16                  ; труба побелена
HF_SUN      equ 32                  ; «солнце» на фронтоне
HF_BARGE    equ 64                  ; причелины в цвет фронтона
RZW         equ 36                  ; высота стен
RTS         equ 3                   ; снег на кровле, px
RTB         equ 2                   ; тёс, px
ROV         equ 72                  ; свес над стеной у ската (8.8)
ROG         equ 48                  ; свес над фронтоном
RCH         equ 40                  ; полуширина трубы
RCHH        equ 12                  ; труба над снегом, px
RCUT_I2     equ 22*22               ; окно в крыше вокруг игрока, px²
RCUT_O2     equ 32*32

; столкновения: наборы фигур (csets), 48 байт на набор
CS_TALL     equ 1                   ; полный тайл, держит удар
CS_LOW      equ 2                   ; полный тайл, низкий
CS_FY       equ 3                   ; забор: кромка +y, +x, обе
CS_FX       equ 4
CS_FXY      equ 5
CS_SPRUCE   equ 6
CS_BIRCH    equ 7
CS_LAMP     equ 8
CS_CROSS    equ 9
CS_SNOWMAN  equ 10
CS_BENCH    equ 11
CS_LINE     equ 12
CS_KENNEL   equ 13
CS_BIRDH    equ 14
CS_WELL     equ 15
CS_V0       equ 16                  ; растения: CS_V0 + порода·9 + сдвиг основания (3x3)
CS_SZ       equ 48
SW_N        equ 40                  ; след лопаты: лучей на всю дугу
SW_TAIL     equ 14                  ; длина хвоста, лучей

MAXOCC      equ 32                  ; заслонители луча фонарика
OC_SZ       equ 32
MAXWIRE     equ 64
MAXLAMP     equ 64
MAXSMK      equ 48

; сущность
E_X         equ 0
E_Y         equ 4
E_VX        equ 8
E_VY        equ 12
E_FX        equ 16
E_FY        equ 20
E_TX        equ 24
E_TY        equ 28
E_HP        equ 32
E_ST        equ 36
E_TM        equ 40
E_CD        equ 44
E_AN        equ 48
E_SA        equ 52
E_COAT      equ 56
E_PANTS     equ 60
E_STUN      equ 64
E_SPD       equ 68
E_MEM       equ 72
E_FOOT      equ 76

ST_FREE     equ 0
ST_WANDER   equ 1
ST_CHASE    equ 2
ST_INVEST   equ 3
ST_CORPSE   equ 4

; источник света: x, y, r (f32), вид (i32), цвет B,G,R,0 (f32 x4); дальше — для
; света попиксельно (lights.inc)
L_X         equ 0
L_Y         equ 4
L_R         equ 8
L_KIND      equ 12
L_COL       equ 16
L_HM        equ 32                  ; карта горизонта: смещение в hz_pool (дворды) + 1, 0 — нет
L_HMX       equ 36                  ; её начало, сэмплы (4 на тайл)
L_HMY       equ 40
L_HMW       equ 44                  ; сторона, сэмплы
L_HID       equ 48                  ; дом + 1 (rmap) — огонь в избе светит только её
L_TX        equ 52                  ; тайл окна, печи, телевизора
L_TY        equ 56
L_DX        equ 60                  ; окно: наружу (−1, 0, 1)
L_DY        equ 64
L_WAX       equ 68                  ; окно: 0 — плоскость y = const, 1 — x = const
L_WPC       equ 72                  ; f32 плоскость стены снаружи, мир 8.8
L_WSG       equ 76                  ; f32 знак «наружу» по нормали плоскости
L_WU0       equ 80                  ; f32 вдоль стены: где столбец рамы wu = 0, мир 8.8
L_WUD       equ 84                  ; f32 1 / шаг wu (±1/16)
L_WMS       equ 88                  ; маска рамы: 0 — пусто, 1 — кот, 2 — герань
L_BX        equ 92                  ; f32 лампочка избы, мир 8.8
L_BY        equ 96
L_BZ        equ 100                 ; px
L_GX        equ 104                 ; f32 точка огня для света попиксельно, мир 8.8
L_GY        equ 108
L_GZ        equ 112                 ; px
L_TV        equ 116                 ; в избе окна — телевизор
LK_ROOM     equ 0
LK_WIN      equ 1
LK_LAMP     equ 2
LK_BROKEN   equ 3
LK_TV       equ 4
LK_STOVE    equ 5                   ; топка печи: только свет попиксельно

; ---------------------------------------------------------------- макросы

; целочисленный хеш двух координат: %1 = x (вход/выход), %2 = y (портится)
%macro HASH 2
    imul    %1, %1, 0x27d4eb2d
    imul    %2, %2, 0x165667b1
    xor     %1, %2
    mov     %2, %1
    shr     %2, 15
    xor     %1, %2
    imul    %1, %1, 0x2c1b3c6d
    mov     %2, %1
    shr     %2, 13
    xor     %1, %2
%endmacro

%macro CLAMP01 1
    maxss   %1, [f_0]
    minss   %1, [f_1]
%endmacro

; импорт: call [Имя] -> call [__imp_Имя]
%macro IMPORT 1-*
  %rep %0
    extern __imp_%1
    %define %1 __imp_%1
    %rotate 1
  %endrep
%endmacro

IMPORT GetModuleHandleW, ExitProcess, QueryPerformanceCounter, QueryPerformanceFrequency
IMPORT GetCommandLineW, CreateFileW, WriteFile, CloseHandle
IMPORT SetProcessDPIAware, RegisterClassExW, CreateWindowExW, PeekMessageW, TranslateMessage
IMPORT DispatchMessageW, DefWindowProcW, PostQuitMessage, LoadCursorW, GetDC, AdjustWindowRect
IMPORT GetAsyncKeyState, GetCursorPos, ScreenToClient, GetForegroundWindow
IMPORT CreateCompatibleDC, CreateDIBSection, SelectObject, SetBkMode, SetTextColor, TextOutW
IMPORT StretchBlt, SetStretchBltMode, CreateFontW, GdiFlush, DeleteObject
IMPORT GetTextExtentPoint32W, GetCharWidth32W
IMPORT DwmFlush
IMPORT SetWindowLongPtrW, SetWindowPos, GetWindowPlacement, SetWindowPlacement
IMPORT MonitorFromWindow, GetMonitorInfoW, PatBlt
IMPORT CreateThread, CreateEventW, WaitForSingleObject, SetEvent, SetThreadPriority, GetCurrentThread
IMPORT waveOutOpen, waveOutPrepareHeader, waveOutUnprepareHeader, waveOutWrite, waveOutReset
IMPORT waveOutClose
IMPORT GetActiveProcessorCount, WaitOnAddress, WakeByAddressAll
