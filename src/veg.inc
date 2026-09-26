; ----------------------------------------------------------------------------
;  veg.inc — лес: ели, сосны, берёзы, подрост, кусты и бурьян, пни.
;  Растение — модель из шариков в мире (хвоя, кора, берёста, снежные подушки,
;  веточки). При запуске она «снимается» камерой игры в объёмную картинку: у
;  каждого пикселя альбедо, нормаль (сфера N_S0) и своя точка в мире, так что
;  луна, фонари и фонарик ложатся на крону, как на избу: освещённый бок, тень
;  под лапой, снег сверху. На тайле — свой вариант породы (хеш места), может
;  быть зеркальным и сдвинутым по сетке 3x3; тень в карте высот — с той же модели.
;  Единицы модели U: тайл = 22.627 U по горизонтали, высота z в U, на экране z·0.866
; ----------------------------------------------------------------------------
VG_MAXB     equ 24000               ; шариков в модели
VB_SZ       equ 32                  ; шарик: x, y, z, r (U), цвет, вид, доп. e0, e1 (f32)
VK_NEEDLE   equ 0                   ; хвоя: снег на открытом сверху
VK_BARK     equ 1                   ; кора: борозды
VK_SNOW     equ 2                   ; снег: подушки на лапах, шапки
VK_TWIG     equ 3                   ; веточка, стебель
VK_BIRCH    equ 4                   ; берёста: чёрточки (порог·100 — биты 8-15, e0, e1 — куда)
VK_WOOD     equ 5                   ; доска: волокна вдоль, торец темнее, снег сверху
VK_LOG      equ 6                   ; бревно, полено: кора по боку, торец в кольцах (срез — биты 8-15)
VK_METAL    equ 7                   ; железо, бетон: ровно, снег сверху
VK_GLOW     equ 8                   ; светится (цвет как есть, с битами эмиссии)
VK_PLAIN    equ 9                   ; как есть: верёвка, уголь, морковка
; брусья и цилиндры (модели двора, prop.inc): середина, вид, цвет, оси, полуразмеры
VX_TYPE     equ 12                  ; 1 брус, 2 цилиндр (x, y, z середины — как у шарика)
VX_U        equ 32                  ; оси u, v, w (единичные; w — вдоль, от A к B)
VX_V        equ 44
VX_W        equ 56
VX_HU       equ 68                  ; полуразмеры по u, v, w (у цилиндра hu — радиус)
VX_HV       equ 72
VX_HW       equ 76
VX_SZ       equ 96
VX_MAX      equ 1600
VXID        equ 0x100000            ; в vr_b: шарик < VXID <= брус
VX_K        equ 200                 ; съёмка: луч к глазу — от глубины VX_K
VX_ZT       equ 400                 ; сверху: луч вниз — от высоты VX_ZT
VRW         equ 128                 ; кадр съёмки, px
VRH         equ 176
VTN         equ 256                 ; сетка «что сверху» для снега: 0.5 U, от −64 U
NVAR        equ 32
NPROP       equ 30                  ; модели двора (prop.inc) — картинки после растений
NVT         equ NVAR + NPROP
VA_PX       equ 320000              ; арены картинок, пикселей
VA_HM       equ 320000              ; арена карт высот, байт
HSMAX       equ 97                  ; сторона карты высот растения не больше (1/32 тайла)

VC_SPBARK   equ 0x3A2C22            ; кора ели
VC_PBARKL   equ 0x4C4038            ; сосна: низ ствола серо-бурый
VC_PBARKH   equ 0xA8643A            ; верх — рыжий
VC_PLIMB    equ 0x8A5634
VC_PNEEDLE  equ 0x34543A            ; хвоя сосны светлее и желтее еловой
VC_BWHITE   equ 0xD8D6D0            ; берёста
VC_BTOP     equ 0x6A5656            ; верхушка берёзы — бурая
VC_BLIMB    equ 0xB0AAA4
VC_BTIP     equ 0x5A4648
VC_TWIG     equ 0x544448            ; веточки берёзы — издали сизо-лиловые
VC_TWIGT    equ 0x6C5E60
VC_WILLOW   equ 0x5A3028            ; ивняк красноватый
VC_WILLOW2  equ 0x6A5C38            ; и жёлтый
VC_STRAW    equ 0x8C7A54            ; бурьян
VC_DRYB     equ 0x5A4632
VC_BURR     equ 0x3A2A1E            ; репьи
VC_TANSY    equ 0x70603A            ; пижма
VC_STUMP    equ 0x4C3C2E
VC_SNOW     equ 0xDCE4EE

section .text

; ---------------------------------------------------------------- сфера нормалей
; nq_lut[16³]: вектор (16 ступеней на ось, −1..1) -> ближайшая нормаль сферы;
; nq_mir[128]: нормаль у зеркальной картинки (x <-> y)
nq_init:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    xor     ebx, ebx
.v:
    lea     eax, [ebx + N_S0]
    imul    eax, eax, 6
    lea     rsi, [nraw]
    movsx   ecx, word [rsi + rax]
    cvtsi2ss xmm0, ecx
    movsx   ecx, word [rsi + rax + 2]
    cvtsi2ss xmm1, ecx
    movsx   ecx, word [rsi + rax + 4]
    cvtsi2ss xmm2, ecx
    movaps  xmm3, xmm0
    mulss   xmm3, xmm0
    movaps  xmm4, xmm1
    mulss   xmm4, xmm1
    addss   xmm3, xmm4
    movaps  xmm4, xmm2
    mulss   xmm4, xmm2
    addss   xmm3, xmm4
    sqrtss  xmm3, xmm3
    movss   xmm4, [f_1]
    divss   xmm4, xmm3
    mulss   xmm0, xmm4
    mulss   xmm1, xmm4
    mulss   xmm2, xmm4
    mov     eax, ebx
    shl     eax, 4
    lea     rdi, [nq_v]
    movss   [rdi + rax], xmm0
    movss   [rdi + rax + 4], xmm1
    movss   [rdi + rax + 8], xmm2
    inc     ebx
    cmp     ebx, NAZ*NEL + 1
    jb      .v
    ; таблица: ближайшая по скалярному произведению
    xor     r12d, r12d
.c:
    mov     eax, r12d
    and     eax, 15
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    mulss   xmm0, [f_0_125]
    subss   xmm0, [f_1]
    mov     eax, r12d
    shr     eax, 4
    and     eax, 15
    cvtsi2ss xmm1, eax
    addss   xmm1, [f_0_5]
    mulss   xmm1, [f_0_125]
    subss   xmm1, [f_1]
    mov     eax, r12d
    shr     eax, 8
    cvtsi2ss xmm2, eax
    addss   xmm2, [f_0_5]
    mulss   xmm2, [f_0_125]
    subss   xmm2, [f_1]
    movss   xmm5, [vk_mbig]
    xor     esi, esi
    xor     ebx, ebx
    lea     rdi, [nq_v]
.s:
    mov     eax, ebx
    shl     eax, 4
    movss   xmm3, [rdi + rax]
    mulss   xmm3, xmm0
    movss   xmm4, [rdi + rax + 4]
    mulss   xmm4, xmm1
    addss   xmm3, xmm4
    movss   xmm4, [rdi + rax + 8]
    mulss   xmm4, xmm2
    addss   xmm3, xmm4
    comiss  xmm3, xmm5
    jbe     .sn
    movss   xmm5, xmm3
    mov     esi, ebx
.sn:
    inc     ebx
    cmp     ebx, NAZ*NEL + 1
    jb      .s
    lea     eax, [esi + N_S0]
    lea     rdi, [nq_lut]
    mov     [rdi + r12], al
    inc     r12d
    cmp     r12d, 4096
    jb      .c
    ; зеркало: азимут 15° + 30°k -> 75° − 30°k, т. е. k -> 2 − k (mod 12)
    lea     rdi, [nq_mir]
    xor     ecx, ecx
.m:
    mov     [rdi + rcx], cl
    inc     ecx
    cmp     ecx, 128
    jb      .m
    xor     ebx, ebx
.me:
    xor     esi, esi
.ma:
    mov     eax, 14
    sub     eax, esi
    cmp     eax, 12
    jb      .m1
    sub     eax, 12
.m1:
    imul    ecx, ebx, NAZ
    lea     edx, [ecx + esi + N_S0]
    lea     eax, [eax + ecx + N_S0]
    mov     [rdi + rdx], al
    inc     esi
    cmp     esi, NAZ
    jb      .ma
    inc     ebx
    cmp     ebx, NEL
    jb      .me
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; xmm0, xmm1, xmm2 = нормаль -> eax = номер в сфере (портит ecx, edx, xmm0-2)
nq_id:
    addss   xmm0, [f_1]
    mulss   xmm0, [f_8]
    cvttss2si eax, xmm0
    xor     ecx, ecx
    cmp     eax, ecx
    cmovl   eax, ecx
    mov     ecx, 15
    cmp     eax, ecx
    cmovg   eax, ecx
    mov     edx, eax
    addss   xmm1, [f_1]
    mulss   xmm1, [f_8]
    cvttss2si eax, xmm1
    xor     ecx, ecx
    cmp     eax, ecx
    cmovl   eax, ecx
    mov     ecx, 15
    cmp     eax, ecx
    cmovg   eax, ecx
    shl     eax, 4
    add     edx, eax
    addss   xmm2, [f_1]
    mulss   xmm2, [f_8]
    cvttss2si eax, xmm2
    xor     ecx, ecx
    cmp     eax, ecx
    cmovl   eax, ecx
    mov     ecx, 15
    cmp     eax, ecx
    cmovg   eax, ecx
    shl     eax, 8
    add     edx, eax
    lea     rcx, [nq_lut]
    movzx   eax, byte [rcx + rdx]
    ret

; ---------------------------------------------------------------- свой ГСЧ, цвета
; -> eax (портит только eax)
vr_u:
    push    rcx
    mov     eax, [vg_rng]
    mov     ecx, eax
    shl     ecx, 13
    xor     eax, ecx
    mov     ecx, eax
    shr     ecx, 17
    xor     eax, ecx
    mov     ecx, eax
    shl     ecx, 5
    xor     eax, ecx
    mov     [vg_rng], eax
    pop     rcx
    ret
; -> xmm0 в [0, 1) (портит eax)
vr_f:
    call    vr_u
    shr     eax, 8
    cvtsi2ss xmm0, eax
    mulss   xmm0, [f_inv24]
    ret
; -> xmm0 в [−1, 1)
vr_s:
    call    vr_f
    addss   xmm0, xmm0
    subss   xmm0, [f_1]
    ret
; xmm0 = угол -> xmm0 = cos, xmm1 = sin (портит eax, xmm2)
vg_cs:
    movss   [vg_ang], xmm0
    call    fsin
    movss   [vg_sin], xmm0
    movss   xmm0, [vg_ang]
    addss   xmm0, [vk_hpi]
    call    fsin
    movss   xmm1, [vg_sin]
    ret

; один канал цвета ebx·r8d/256 -> r9d
%macro VJCH 1
    mov     eax, ebx
    shr     eax, %1
    and     eax, 255
    imul    eax, r8d
    shr     eax, 8
    mov     edx, 255
    cmp     eax, edx
    cmova   eax, edx
    shl     eax, %1
    or      r9d, eax
%endmacro
; ecx = цвет -> ecx: ярче/темнее ×0.88..1.12 (портит eax, edx, r8, r9)
vc_jit:
    call    vr_u
    mov     r8d, eax
    and     r8d, 63
    add     r8d, 225
; ecx = цвет, r8d = множитель /256 -> ecx
vc_mul:
    push    rbx
    mov     ebx, ecx
    xor     r9d, r9d
    VJCH    0
    VJCH    8
    VJCH    16
    mov     ecx, r9d
    pop     rbx
    ret
; ecx = цвет a, edx = цвет b, xmm0 = доля b (0..1) -> ecx (портит eax, edx, r8, r9, r10)
vc_lerp:
    push    rbx
    mulss   xmm0, [f_256]
    cvttss2si r8d, xmm0
    xor     eax, eax
    cmp     r8d, eax
    cmovl   r8d, eax
    mov     eax, 256
    cmp     r8d, eax
    cmovg   r8d, eax
    mov     ebx, ecx
    mov     r10d, edx
    xor     r9d, r9d
%assign sh 0
%rep 3
    mov     eax, ebx
    shr     eax, sh
    and     eax, 255
    mov     edx, r10d
    shr     edx, sh
    and     edx, 255
    sub     edx, eax
    imul    edx, r8d
    sar     edx, 8
    add     eax, edx
    and     eax, 255
    shl     eax, sh
    or      r9d, eax
%assign sh sh + 8
%endrep
    mov     ecx, r9d
    pop     rbx
    ret

; xmm0..3 = x, y, z, r; ecx = цвет; edx = вид; xmm4, xmm5 — доп. (портит rax, r8)
vb_add:
    mov     eax, [vg_nb]
    cmp     eax, VG_MAXB
    jae     .full
    shl     eax, 5
    lea     r8, [vg_blobs]
    add     r8, rax
    movss   [r8], xmm0
    movss   [r8 + 4], xmm1
    movss   [r8 + 8], xmm2
    movss   [r8 + 12], xmm3
    mov     [r8 + 16], ecx
    mov     [r8 + 20], edx
    movss   [r8 + 24], xmm4
    movss   [r8 + 28], xmm5
    inc     dword [vg_nb]
.full:
    ret

; ---------------------------------------------------------------- брусья и цилиндры
; vx_a, vx_b — концы оси (x, y, z, U); vx_hu, vx_hv — полутолщины поперёк: hu — по
; горизонтали (у стоячего — вдоль x), hv — вверх (у стоячего — вдоль y); vx_hint — своя
; «вверх» для наклонной доски (0 — нет); у цилиндра hu — радиус. vx_type: 1 брус,
; 2 цилиндр; vx_col, vx_kind -> фигура в vg_shp, цвет чуть вразброс
; (портит rax, rcx, rdx, r8, r9, xmm0-xmm7)
vx_seg:
    mov     eax, [vg_nx]
    cmp     eax, VX_MAX
    jae     .full
    mov     ecx, [vx_col]
    mov     eax, [vx_kind]
    cmp     al, VK_GLOW
    je      .nj
    call    vr_u
    mov     r8d, eax
    and     r8d, 31
    add     r8d, 240
    call    vc_mul
.nj:
    mov     [vx_colj], ecx
    imul    eax, [vg_nx], VX_SZ
    lea     r8, [vg_shp]
    add     r8, rax
    ; середина, ось
    xor     ecx, ecx
    lea     r9, [vx_a]
.c:
    movss   xmm0, [r9 + rcx*4]
    movss   xmm1, [r9 + rcx*4 + (vx_b - vx_a)]
    movaps  xmm2, xmm0
    addss   xmm2, xmm1
    mulss   xmm2, [f_0_5]
    movss   [r8 + rcx*4], xmm2
    subss   xmm1, xmm0
    movss   [r9 + rcx*4 + (vx_w3 - vx_a)], xmm1
    inc     ecx
    cmp     ecx, 3
    jb      .c
    call    .norm3
    maxss   xmm0, [vk_0_01]
    mulss   xmm0, [f_0_5]
    movss   [r8 + VX_HW], xmm0
    ; «вверх»: своя, иначе z (у стоячего — −y)
    movss   xmm0, [vx_hint]
    mulss   xmm0, xmm0
    movss   xmm1, [vx_hint + 4]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    movss   xmm1, [vx_hint + 8]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    comiss  xmm0, [f_0_5]
    ja      .hk
    mov     dword [vx_hint], 0
    mov     dword [vx_hint + 4], 0
    mov     dword [vx_hint + 8], __float32__(1.0)
    movss   xmm0, [vx_w3 + 8]
    andps   xmm0, [f_absmask]
    comiss  xmm0, [vk_0_99]
    jbe     .hk
    mov     dword [vx_hint + 4], __float32__(-1.0)
    mov     dword [vx_hint + 8], 0
.hk:
    ; u = w × h
    movss   xmm0, [vx_w3 + 4]
    mulss   xmm0, [vx_hint + 8]
    movss   xmm1, [vx_w3 + 8]
    mulss   xmm1, [vx_hint + 4]
    subss   xmm0, xmm1
    movss   xmm1, [vx_w3 + 8]
    mulss   xmm1, [vx_hint]
    movss   xmm2, [vx_w3]
    mulss   xmm2, [vx_hint + 8]
    subss   xmm1, xmm2
    movss   xmm2, [vx_w3]
    mulss   xmm2, [vx_hint + 4]
    movss   xmm3, [vx_w3 + 4]
    mulss   xmm3, [vx_hint]
    subss   xmm2, xmm3
    movss   [vx_u3], xmm0
    movss   [vx_u3 + 4], xmm1
    movss   [vx_u3 + 8], xmm2
    push    r8
    lea     r8, [vx_u3]
    call    .nrm
    pop     r8
    ; v = u × w
    movss   xmm0, [vx_u3 + 4]
    mulss   xmm0, [vx_w3 + 8]
    movss   xmm1, [vx_u3 + 8]
    mulss   xmm1, [vx_w3 + 4]
    subss   xmm0, xmm1
    movss   xmm1, [vx_u3 + 8]
    mulss   xmm1, [vx_w3]
    movss   xmm2, [vx_u3]
    mulss   xmm2, [vx_w3 + 8]
    subss   xmm1, xmm2
    movss   xmm2, [vx_u3]
    mulss   xmm2, [vx_w3 + 4]
    movss   xmm3, [vx_u3 + 4]
    mulss   xmm3, [vx_w3]
    subss   xmm2, xmm3
    movss   [r8 + VX_V], xmm0
    movss   [r8 + VX_V + 4], xmm1
    movss   [r8 + VX_V + 8], xmm2
    xor     ecx, ecx
    lea     r9, [vx_a]
.cp:
    mov     eax, [r9 + rcx*4 + (vx_u3 - vx_a)]
    mov     [r8 + VX_U + rcx*4], eax
    mov     eax, [r9 + rcx*4 + (vx_w3 - vx_a)]
    mov     [r8 + VX_W + rcx*4], eax
    inc     ecx
    cmp     ecx, 3
    jb      .cp
    mov     eax, [vx_hu]
    mov     [r8 + VX_HU], eax
    mov     eax, [vx_hv]
    cmp     dword [vx_type], 2
    jne     .hv
    mov     eax, [vx_hu]
.hv:
    mov     [r8 + VX_HV], eax
    mov     eax, [vx_type]
    mov     [r8 + VX_TYPE], eax
    mov     eax, [vx_colj]
    mov     [r8 + 16], eax
    mov     eax, [vx_kind]
    mov     [r8 + 20], eax
    mov     dword [r8 + 24], 0
    mov     dword [r8 + 28], 0
    mov     dword [vx_hint], 0
    mov     dword [vx_hint + 4], 0
    mov     dword [vx_hint + 8], 0
    inc     dword [vg_nx]
.full:
    ret
; vx_w3 -> единичный, xmm0 = была длина (портит xmm1, xmm2)
.norm3:
    push    r8
    lea     r8, [vx_w3]
    call    .nrm
    pop     r8
    ret
; r8 -> 3 числа: единичными, xmm0 = была длина (портит xmm1, xmm2)
.nrm:
    movss   xmm0, [r8]
    mulss   xmm0, xmm0
    movss   xmm1, [r8 + 4]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    movss   xmm1, [r8 + 8]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    sqrtss  xmm0, xmm0
    movss   xmm2, xmm0
    maxss   xmm2, [vk_1em4]
    movss   xmm1, [f_1]
    divss   xmm1, xmm2
    movss   xmm2, [r8]
    mulss   xmm2, xmm1
    movss   [r8], xmm2
    movss   xmm2, [r8 + 4]
    mulss   xmm2, xmm1
    movss   [r8 + 4], xmm2
    movss   xmm2, [r8 + 8]
    mulss   xmm2, xmm1
    movss   [r8 + 8], xmm2
    ret

; rsi -> таблица фигур (prop.inc: PBOX, PCYL, PBALL ... PEND); первое слово — 0: x, y
; в тайлах от угла тайла (основание модели — 0.6 тайла), 1: в U от основания; z и
; размеры — U. Перенос: ·vx_scl, + vx_off, x += z·vx_shx, y += z·vx_shy (наклон)
; (портит rax, rcx, rdx, r8, r9, xmm0-xmm7; rsi — за таблицей)
vx_tab:
    mov     eax, [rsi]
    mov     [vx_tmode], eax
    add     rsi, 4
.l:
    mov     eax, [rsi]
    test    eax, eax
    jz      .end
    mov     [vx_type], eax
    lea     rdx, [rsi + 4]
    lea     r9, [vx_a]
    call    .pt
    lea     rdx, [rsi + 16]
    lea     r9, [vx_b]
    call    .pt
    movss   xmm0, [rsi + 28]
    mulss   xmm0, [vx_scl]
    movss   [vx_hu], xmm0
    movss   xmm0, [rsi + 32]
    mulss   xmm0, [vx_scl]
    movss   [vx_hv], xmm0
    mov     eax, [rsi + 36]
    mov     [vx_hint], eax
    mov     eax, [rsi + 40]
    mov     [vx_hint + 4], eax
    mov     eax, [rsi + 44]
    mov     [vx_hint + 8], eax
    mov     eax, [rsi + 48]
    mov     [vx_col], eax
    mov     eax, [rsi + 52]
    mov     [vx_kind], eax
    cmp     dword [vx_type], 3
    je      .ball
    call    vx_seg
    jmp     .n
.ball:
    call    vx_ball
.n:
    add     rsi, 56
    jmp     .l
.end:
    add     rsi, 4
    ret
; rdx -> точка таблицы -> r9: в U от основания, с переносом (портит xmm0-xmm2)
.pt:
    movss   xmm0, [rdx]
    movss   xmm1, [rdx + 4]
    cmp     dword [vx_tmode], 0
    jne     .p1
    mulss   xmm0, [vk_t2u]
    subss   xmm0, [vk_b0]
    mulss   xmm1, [vk_t2u]
    subss   xmm1, [vk_b0]
.p1:
    movss   xmm2, [rdx + 8]
    mulss   xmm0, [vx_scl]
    mulss   xmm1, [vx_scl]
    mulss   xmm2, [vx_scl]
    addss   xmm0, [vx_off]
    addss   xmm1, [vx_off + 4]
    addss   xmm2, [vx_off + 8]
    movss   [r9 + 8], xmm2
    mulss   xmm2, [vx_shx]
    addss   xmm0, xmm2
    movss   [r9], xmm0
    movss   xmm2, [r9 + 8]
    mulss   xmm2, [vx_shy]
    addss   xmm1, xmm2
    movss   [r9 + 4], xmm1
    ret

; шарик из vx_a (середина), vx_hu (радиус), vx_col, vx_kind (портит rax, rcx, rdx, r8)
vx_ball:
    movss   xmm0, [vx_a]
    movss   xmm1, [vx_a + 4]
    movss   xmm2, [vx_a + 8]
    movss   xmm3, [vx_hu]
    mov     ecx, [vx_col]
    mov     edx, [vx_kind]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    jmp     vb_add

; rbx = фигура -> vx_cn: 8 углов (x, y, z) (портит rax, rcx, rdx, r10, xmm0-xmm3)
vx_corners:
    xor     ecx, ecx
    lea     r10, [vx_cn]
.k:
    xor     eax, eax
.i:
    movss   xmm0, [rbx + VX_U + rax*4]
    mulss   xmm0, [rbx + VX_HU]
    test    ecx, 1
    jnz     .u1
    xorps   xmm0, [f_signmask]
.u1:
    movss   xmm1, [rbx + VX_V + rax*4]
    mulss   xmm1, [rbx + VX_HV]
    test    ecx, 2
    jnz     .v1
    xorps   xmm1, [f_signmask]
.v1:
    movss   xmm2, [rbx + VX_W + rax*4]
    mulss   xmm2, [rbx + VX_HW]
    test    ecx, 4
    jnz     .w1
    xorps   xmm2, [f_signmask]
.w1:
    addss   xmm0, xmm1
    addss   xmm0, xmm2
    addss   xmm0, [rbx + rax*4]
    lea     edx, [ecx + ecx*2]
    add     edx, eax
    movss   [r10 + rdx*4], xmm0
    inc     eax
    cmp     eax, 3
    jb      .i
    inc     ecx
    cmp     ecx, 8
    jb      .k
    ret

; rbx = фигура; vx_lo — начало луча, vx_ld — направление (в осях фигуры u, v, w),
; vx_inf — раздуть на столько (карта высот: тонкое не проскочит между ячейками)
; -> eax = 1, если попал; xmm0 = t входа; vx_ln — нормаль (в осях фигуры)
; (портит rcx, rdx, r10, xmm0-xmm7)
vx_hit:
    cmp     dword [rbx + VX_TYPE], 2
    je      .cyl
    ; брус: три пары плоскостей
    movss   xmm6, [vk_mbig]             ; вход
    movss   xmm7, [vk_big]              ; выход
    xor     ecx, ecx                    ; ось входа
    xor     edx, edx
    lea     r10, [vx_lo]
.b:
    movss   xmm0, [r10 + rdx*4]
    movss   xmm1, [r10 + rdx*4 + (vx_ld - vx_lo)]
    movss   xmm2, [rbx + VX_HU + rdx*4]
    addss   xmm2, [vx_inf]
    movaps  xmm3, xmm1
    andps   xmm3, [f_absmask]
    comiss  xmm3, [f_1em6]
    jae     .bd
    movaps  xmm3, xmm0
    andps   xmm3, [f_absmask]
    comiss  xmm3, xmm2
    ja      .miss
    jmp     .bn
.bd:
    movaps  xmm3, xmm2
    xorps   xmm3, [f_signmask]
    subss   xmm3, xmm0
    divss   xmm3, xmm1                  ; t1
    movaps  xmm4, xmm2
    subss   xmm4, xmm0
    divss   xmm4, xmm1                  ; t2
    movaps  xmm5, xmm3
    minss   xmm5, xmm4
    maxss   xmm3, xmm4
    comiss  xmm5, xmm6
    jbe     .b1
    movaps  xmm6, xmm5
    mov     ecx, edx
.b1:
    minss   xmm7, xmm3
.bn:
    inc     edx
    cmp     edx, 3
    jb      .b
    comiss  xmm6, xmm7
    ja      .miss
    ; грань входа — навстречу лучу
    xor     eax, eax
    mov     [vx_ln], eax
    mov     [vx_ln + 4], eax
    mov     [vx_ln + 8], eax
    mov     eax, __float32__(1.0)
    movss   xmm1, [r10 + rcx*4 + (vx_ld - vx_lo)]
    comiss  xmm1, [f_0]
    jb      .bs
    mov     eax, __float32__(-1.0)
.bs:
    mov     [r10 + rcx*4 + (vx_ln - vx_lo)], eax
    movaps  xmm0, xmm6
    mov     eax, 1
    ret
.cyl:
    ; цилиндр вдоль w: бок (a + t·da)² + (b + t·db)² = r², торцы |c + t·dc| = h
    movss   xmm2, [rbx + VX_HU]
    addss   xmm2, [vx_inf]
    movss   [vx_r], xmm2
    movss   xmm0, [vx_ld]
    mulss   xmm0, xmm0
    movss   xmm1, [vx_ld + 4]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1                  ; A
    movss   xmm6, [vk_mbig]
    movss   xmm7, [vk_big]
    comiss  xmm0, [f_1em6]
    jae     .cq
    ; вдоль оси: внутри круга — да
    movss   xmm0, [vx_lo]
    mulss   xmm0, xmm0
    movss   xmm1, [vx_lo + 4]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    mulss   xmm2, xmm2
    comiss  xmm0, xmm2
    ja      .miss
    jmp     .cs
.cq:
    movss   xmm1, [vx_lo]
    mulss   xmm1, [vx_ld]
    movss   xmm3, [vx_lo + 4]
    mulss   xmm3, [vx_ld + 4]
    addss   xmm1, xmm3                  ; B
    movss   xmm3, [vx_lo]
    mulss   xmm3, xmm3
    movss   xmm4, [vx_lo + 4]
    mulss   xmm4, xmm4
    addss   xmm3, xmm4
    mulss   xmm2, xmm2
    subss   xmm3, xmm2                  ; C
    movaps  xmm4, xmm1
    mulss   xmm4, xmm1
    mulss   xmm3, xmm0
    subss   xmm4, xmm3                  ; B² − AC
    comiss  xmm4, [f_0]
    jb      .miss
    sqrtss  xmm4, xmm4
    xorps   xmm1, [f_signmask]
    movaps  xmm6, xmm1
    subss   xmm6, xmm4
    divss   xmm6, xmm0                  ; вход в бок
    movaps  xmm7, xmm1
    addss   xmm7, xmm4
    divss   xmm7, xmm0                  ; выход
.cs:
    movss   [vx_tc], xmm6
    ; торцы
    movss   xmm2, [rbx + VX_HW]
    addss   xmm2, [vx_inf]
    movss   xmm0, [vx_lo + 8]
    movss   xmm1, [vx_ld + 8]
    movaps  xmm3, xmm1
    andps   xmm3, [f_absmask]
    comiss  xmm3, [f_1em6]
    jae     .cd
    movaps  xmm3, xmm0
    andps   xmm3, [f_absmask]
    comiss  xmm3, xmm2
    ja      .miss
    movss   xmm5, [vk_mbig]
    jmp     .cc
.cd:
    movaps  xmm3, xmm2
    xorps   xmm3, [f_signmask]
    subss   xmm3, xmm0
    divss   xmm3, xmm1
    movaps  xmm4, xmm2
    subss   xmm4, xmm0
    divss   xmm4, xmm1
    movaps  xmm5, xmm3
    minss   xmm5, xmm4                  ; вход в торец
    maxss   xmm3, xmm4
    minss   xmm7, xmm3
    maxss   xmm6, xmm5
.cc:
    comiss  xmm6, xmm7
    ja      .miss
    xor     eax, eax
    mov     [vx_ln + 8], eax
    comiss  xmm5, [vx_tc]
    ja      .ccap
    ; бок: (a + t·da, b + t·db) / r
    movss   xmm1, [f_1]
    divss   xmm1, [vx_r]
    movss   xmm0, [vx_ld]
    mulss   xmm0, xmm6
    addss   xmm0, [vx_lo]
    mulss   xmm0, xmm1
    movss   [vx_ln], xmm0
    movss   xmm0, [vx_ld + 4]
    mulss   xmm0, xmm6
    addss   xmm0, [vx_lo + 4]
    mulss   xmm0, xmm1
    movss   [vx_ln + 4], xmm0
    movaps  xmm0, xmm6
    mov     eax, 1
    ret
.ccap:
    mov     [vx_ln], eax
    mov     [vx_ln + 4], eax
    mov     eax, __float32__(1.0)
    movss   xmm1, [vx_ld + 8]
    comiss  xmm1, [f_0]
    jb      .cps
    mov     eax, __float32__(-1.0)
.cps:
    mov     [vx_ln + 8], eax
    movaps  xmm0, xmm6
    mov     eax, 1
    ret
.miss:
    xor     eax, eax
    ret

; rbx = фигура, vx_P — точка в мире, vx_D — направление -> vx_lo, vx_ld (в осях фигуры)
; (портит rax, rcx, r10, xmm0-xmm2)
vx_local:
    xor     eax, eax
    lea     r10, [vx_lo]
.a:
    imul    ecx, eax, 12
    movss   xmm0, [vx_P]
    subss   xmm0, [rbx]
    mulss   xmm0, [rbx + VX_U + rcx]
    movss   xmm1, [vx_P + 4]
    subss   xmm1, [rbx + 4]
    mulss   xmm1, [rbx + VX_U + rcx + 4]
    addss   xmm0, xmm1
    movss   xmm1, [vx_P + 8]
    subss   xmm1, [rbx + 8]
    mulss   xmm1, [rbx + VX_U + rcx + 8]
    addss   xmm0, xmm1
    movss   [r10 + rax*4], xmm0
    movss   xmm0, [vx_D]
    mulss   xmm0, [rbx + VX_U + rcx]
    movss   xmm1, [vx_D + 4]
    mulss   xmm1, [rbx + VX_U + rcx + 4]
    addss   xmm0, xmm1
    movss   xmm1, [vx_D + 8]
    mulss   xmm1, [rbx + VX_U + rcx + 8]
    addss   xmm0, xmm1
    movss   [r10 + rax*4 + (vx_ld - vx_lo)], xmm0
    inc     eax
    cmp     eax, 3
    jb      .a
    ret

; rbx = фигура: vx_ln (в осях) -> vx_n (в мире) (портит eax, r10, xmm0-xmm1)
vx_world_n:
    xor     eax, eax
    lea     r10, [vx_n]
.a:
    movss   xmm0, [vx_ln]
    mulss   xmm0, [rbx + VX_U + rax*4]
    movss   xmm1, [vx_ln + 4]
    mulss   xmm1, [rbx + VX_V + rax*4]
    addss   xmm0, xmm1
    movss   xmm1, [vx_ln + 8]
    mulss   xmm1, [rbx + VX_W + rax*4]
    addss   xmm0, xmm1
    movss   [r10 + rax*4], xmm0
    inc     eax
    cmp     eax, 3
    jb      .a
    ret

; rbx = фигура, xmm0 = x, xmm1 = y: луч сверху -> eax = 1, xmm0 = высота попадания
; (портит rcx, rdx, xmm0-xmm7)
vx_down:
    movss   [vx_P], xmm0
    movss   [vx_P + 4], xmm1
    mov     eax, [vk_zt]
    mov     [vx_P + 8], eax
    mov     dword [vx_D], 0
    mov     dword [vx_D + 4], 0
    mov     dword [vx_D + 8], __float32__(-1.0)
    call    vx_local
    call    vx_hit
    test    eax, eax
    jz      .out
    movss   xmm1, [vk_zt]
    subss   xmm1, xmm0
    movaps  xmm0, xmm1
.out:
    ret

; ---------------------------------------------------------------- ель
; vg_H — высота, U; vg_snow — снеговая нагрузка 0..1; vg_young — подрост (шире, гуще,
; весь в снегу); vg_ncol — хвоя. Ствол, мутовки сверху вниз: 5-7 лап, каждая — веер
; хвои с прогибом под снегом, края веера никнут, на лапах — снежные подушки
vg_spruce:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    ; радиус кроны у низа: 0.15..0.19 высоты (подрост — 0.30..0.36), не шире 24 U
    call    vr_f
    movss   xmm1, [vk_0_04]
    movss   xmm2, [vk_0_15]
    cmp     dword [vg_young], 0
    je      .r1
    movss   xmm1, [vk_0_06]
    movss   xmm2, [vk_0_3]
.r1:
    mulss   xmm0, xmm1
    addss   xmm0, xmm2
    mulss   xmm0, [vg_H]
    minss   xmm0, [vk_24]
    movss   [vg_R], xmm0
    ; где начинаются лапы (подрост — от самой земли)
    call    vr_f
    mulss   xmm0, [vk_0_07]
    addss   xmm0, [vk_0_03]
    mulss   xmm0, [vg_H]
    movss   xmm1, [vk_7_5]
    cmp     dword [vg_young], 0
    je      .r2
    movss   xmm0, [vk_m1]
    movss   xmm1, [vk_4_2]
.r2:
    movss   [vg_zc], xmm0
    movss   [vg_dz], xmm1
    ; ----- ствол: от −2 до 0.92 высоты, к макушке тоньше
    movss   xmm0, [vk_m2]
    movss   [vg_z], xmm0
.tr:
    movss   xmm4, [vg_z]
    divss   xmm4, [vg_H]
    mulss   xmm4, [vk_0_7]
    movss   xmm3, [f_1]
    subss   xmm3, xmm4
    movss   xmm4, [vg_H]
    mulss   xmm4, [vk_0_012]
    addss   xmm4, [vk_0_9]
    mulss   xmm3, xmm4
    mov     ecx, VC_SPBARK
    call    vc_jit
    xorps   xmm0, xmm0
    xorps   xmm1, xmm1
    movss   xmm2, [vg_z]
    mov     edx, VK_BARK
    call    vb_add
    movss   xmm0, [vg_z]
    addss   xmm0, [vk_0_6]
    movss   [vg_z], xmm0
    movss   xmm1, [vg_H]
    mulss   xmm1, [vk_0_92]
    comiss  xmm0, xmm1
    jb      .tr
    ; ----- мутовки сверху вниз
    movss   xmm0, [vg_H]
    subss   xmm0, [vk_3]
    movss   [vg_z], xmm0
.wh:
    movss   xmm0, [vg_z]
    comiss  xmm0, [vg_zc]
    jbe     .top
    ; t: 0 — макушка, 1 — низ кроны
    movss   xmm1, [vg_H]
    subss   xmm1, xmm0
    movss   xmm2, [vg_H]
    subss   xmm2, [vg_zc]
    divss   xmm1, xmm2
    minss   xmm1, [f_1]
    movss   [vg_t], xmm1
    ; радиус мутовки: R·t·(1.35 − 0.35 t) + 1.5 — конус чуть выпуклый
    movaps  xmm3, xmm1
    mulss   xmm3, [vk_0_35]
    movss   xmm2, [vk_1_35]
    subss   xmm2, xmm3
    mulss   xmm2, xmm1
    mulss   xmm2, [vg_R]
    addss   xmm2, [vk_1_5]
    movss   [vg_Rc], xmm2
    ; лап 5..7 (подрост 6..8), мутовка повёрнута как придётся
    call    vr_u
    and     eax, 0xFFFF
    imul    eax, eax, 3
    shr     eax, 16
    lea     r12d, [eax + 5]
    cmp     dword [vg_young], 0
    je      .nb
    inc     r12d
.nb:
    call    vr_f
    mulss   xmm0, [vk_2pi]
    movss   [vg_phi0], xmm0
    xor     r13d, r13d
.br:
    cmp     r13d, r12d
    jae     .whn
    ; просвет: 6 % лап нет (не у макушки)
    call    vr_f
    comiss  xmm0, [vk_0_06]
    jae     .bk
    movss   xmm0, [vg_t]
    comiss  xmm0, [vk_0_2]
    ja      .brn
.bk:
    cvtsi2ss xmm0, r13d
    cvtsi2ss xmm1, r12d
    divss   xmm0, xmm1
    mulss   xmm0, [vk_2pi]
    addss   xmm0, [vg_phi0]
    movss   [vg_phi], xmm0
    call    vr_s
    mulss   xmm0, [vk_0_25]
    addss   xmm0, [vg_phi]
    call    vg_cs
    movss   [vg_dx], xmm0
    movss   [vg_dy], xmm1
    ; длина 0.8..1.1 радиуса мутовки
    call    vr_f
    mulss   xmm0, [vk_0_3]
    addss   xmm0, [vk_0_8]
    mulss   xmm0, [vg_Rc]
    movss   [vg_L], xmm0
    ; прогиб (0.3 + 0.4 t)·(0.8..1.2) + 0.2·снег — ярусы юбками
    call    vr_f
    mulss   xmm0, [vk_0_4]
    addss   xmm0, [vk_0_8]
    movss   xmm1, [vg_t]
    mulss   xmm1, [vk_0_4]
    addss   xmm1, [vk_0_3]
    mulss   xmm1, xmm0
    movss   xmm0, [vg_snow]
    mulss   xmm0, [vk_0_2]
    addss   xmm1, xmm0
    movss   [vg_dr], xmm1
    ; у ствола лапа идёт вверх (0.3 − 0.4 t), нижние — чуть вниз
    movss   xmm0, [vg_t]
    mulss   xmm0, [vk_0_4]
    movss   xmm1, [vk_0_3]
    subss   xmm1, xmm0
    movss   [vg_s0], xmm1
    call    vg_branch
.brn:
    inc     r13d
    jmp     .br
.whn:
    call    vr_f
    mulss   xmm0, [vk_0_3]
    addss   xmm0, [vk_0_85]
    mulss   xmm0, [vg_dz]
    movss   xmm1, [vg_z]
    subss   xmm1, xmm0
    movss   [vg_z], xmm1
    jmp     .wh
.top:
    ; ----- макушка: побег с пучком, шапка снега
    movss   xmm0, [vg_H]
    subss   xmm0, [vk_5]
    movss   [vg_z], xmm0
.tp:
    movss   xmm3, [vk_0_8]
    mov     ecx, [vg_ncol]
    call    vc_jit
    xorps   xmm0, xmm0
    xorps   xmm1, xmm1
    movss   xmm2, [vg_z]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     edx, VK_NEEDLE
    call    vb_add
    movss   xmm0, [vg_z]
    addss   xmm0, [f_0_5]
    movss   [vg_z], xmm0
    comiss  xmm0, [vg_H]
    jb      .tp
    movss   xmm0, [vg_snow]
    comiss  xmm0, [f_0_5]
    jbe     .out
    xorps   xmm0, xmm0
    xorps   xmm1, xmm1
    movss   xmm2, [vg_H]
    subss   xmm2, [vk_3]
    movss   xmm3, [vk_1_3]
    mov     ecx, VC_SNOW
    mov     edx, VK_SNOW
    call    vb_add
.out:
    add     rsp, 40
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; лапа ели: из ствола (0, 0, vg_z) по (vg_dx, vg_dy) длиной vg_L; ось z(t) = z + L(s0·t −
; dr·t²), веер хвои шириной до 0.32 L, края никнут; у ствола хвоя темнее (в глубине
; кроны); через сегмент — снежная подушка (чаще к концу лапы)
vg_branch:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32
    movss   xmm0, [vg_L]
    mulss   xmm0, [vk_0_77]
    cvttss2si ebx, xmm0
    mov     eax, 3
    cmp     ebx, eax
    cmovl   ebx, eax
    mov     esi, 1
.s:
    cmp     esi, ebx
    ja      .out
    cvtsi2ss xmm0, esi
    cvtsi2ss xmm1, ebx
    divss   xmm0, xmm1
    movss   [vb_t], xmm0
    movaps  xmm1, xmm0
    mulss   xmm1, [vg_L]
    movaps  xmm2, xmm1
    mulss   xmm2, [vg_dx]
    movss   [vb_bx], xmm2
    movaps  xmm2, xmm1
    mulss   xmm2, [vg_dy]
    movss   [vb_by], xmm2
    movss   xmm2, [vg_s0]
    mulss   xmm2, xmm0
    movss   xmm3, [vg_dr]
    mulss   xmm3, xmm0
    mulss   xmm3, xmm0
    subss   xmm2, xmm3
    mulss   xmm2, [vg_L]
    addss   xmm2, [vg_z]
    movss   [vb_bz], xmm2
    ; ширина веера 0.4 L·√(4t(1−t)) + 0.6
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    mulss   xmm1, xmm0
    mulss   xmm1, [vk_4]
    sqrtss  xmm1, xmm1
    mulss   xmm1, [vg_L]
    mulss   xmm1, [vk_0_4]
    addss   xmm1, [vk_0_6]
    movss   [vb_w], xmm1
    ; тень в глубине: 0.42 + 0.58 t
    movss   xmm0, [vb_t]
    mulss   xmm0, [vk_0_58]
    addss   xmm0, [vk_0_42]
    mulss   xmm0, [f_256]
    cvttss2si eax, xmm0
    mov     [vb_ao], eax
    ; поперёк: 1 + w/1.4 шариков в каждую сторону
    mulss   xmm1, [vk_0_7]
    cvttss2si edi, xmm1
    inc     edi
    mov     [vb_n], edi
    neg     edi
.j:
    cmp     edi, [vb_n]
    jg      .sn
    cvtsi2ss xmm0, edi
    cvtsi2ss xmm1, dword [vb_n]
    divss   xmm0, xmm1
    mulss   xmm0, [vb_w]
    movss   [vb_l], xmm0
    call    vr_f
    mulss   xmm0, [vk_0_4]
    addss   xmm0, [vk_0_8]
    mulss   xmm0, [vb_l]
    movss   [vb_l], xmm0
    movss   xmm1, [vg_dy]
    mulss   xmm1, xmm0
    movss   xmm2, [vb_bx]
    subss   xmm2, xmm1
    movss   [vb_px], xmm2
    movss   xmm1, [vg_dx]
    mulss   xmm1, xmm0
    addss   xmm1, [vb_by]
    movss   [vb_py], xmm1
    mulss   xmm0, xmm0
    divss   xmm0, [vb_w]
    mulss   xmm0, [vk_0_3]
    movss   xmm2, [vb_bz]
    subss   xmm2, xmm0
    movss   [vb_pz], xmm2
    call    vr_s
    mulss   xmm0, [vk_0_35]
    addss   xmm0, [vb_pz]
    movss   [vb_pz], xmm0
    ; r = 0.9 + 1.1·(1 − t) + 0.4·rnd
    call    vr_f
    mulss   xmm0, [vk_0_4]
    movss   xmm3, [f_1]
    subss   xmm3, [vb_t]
    mulss   xmm3, [vk_1_1]
    addss   xmm3, xmm0
    addss   xmm3, [vk_0_9]
    movss   [vb_r], xmm3
    ; под лапой темнее: ×(1 + 0.12·(z − ось)), 0.62..1.1
    movss   xmm0, [vb_pz]
    subss   xmm0, [vb_bz]
    mulss   xmm0, [vk_0_12]
    addss   xmm0, [f_1]
    maxss   xmm0, [vk_0_62]
    minss   xmm0, [vk_1_1]
    cvtsi2ss xmm1, dword [vb_ao]
    mulss   xmm0, xmm1
    cvttss2si eax, xmm0
    mov     [vb_aob], eax
    mov     ecx, [vg_ncol]
    call    vc_jit
    mov     r8d, [vb_aob]
    call    vc_mul
    movss   xmm0, [vb_px]
    movss   xmm1, [vb_py]
    movss   xmm2, [vb_pz]
    movss   xmm3, [vb_r]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     edx, VK_NEEDLE
    call    vb_add
    inc     edi
    jmp     .j
.sn:
    ; снежная подушка сверху лапы — на каждом сегменте, чаще к концу: полоса снега
    ; по ярусу, а не крапинки
    movss   xmm0, [vb_t]
    comiss  xmm0, [vk_0_2]
    jbe     .nx
    mulss   xmm0, [vk_0_55]
    addss   xmm0, [vk_0_35]
    mulss   xmm0, [vg_snow]
    movss   [vb_l], xmm0
    call    vr_f
    comiss  xmm0, [vb_l]
    jae     .nx
    call    vr_s
    mulss   xmm0, [vb_w]
    mulss   xmm0, [vk_0_3]
    movss   [vb_l], xmm0                ; вбок от оси
    call    vr_f
    mulss   xmm0, [vg_snow]
    mulss   xmm0, [vk_1_4]
    addss   xmm0, [vk_0_9]
    movaps  xmm3, xmm0
    movss   xmm2, [vb_bz]
    addss   xmm2, [vk_0_9]
    movss   xmm0, [vg_dy]
    mulss   xmm0, [vb_l]
    movss   xmm1, [vb_bx]
    subss   xmm1, xmm0
    movaps  xmm0, xmm1
    movss   xmm1, [vg_dx]
    mulss   xmm1, [vb_l]
    addss   xmm1, [vb_by]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     ecx, VC_SNOW
    mov     edx, VK_SNOW
    call    vb_add
.nx:
    inc     esi
    jmp     .s
.out:
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- сосна
; ствол с лёгким наклоном: внизу серо-бурый в бороздах, выше рыжий; крона с 0.55..0.67
; высоты — сучья вверх-наружу, на концах плоские пучки хвои, на пучках — снег
vg_pine:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    ; наклон: у макушки на 0.02..0.06 высоты
    call    vr_f
    mulss   xmm0, [vk_2pi]
    call    vg_cs
    movss   [vg_dx], xmm0
    movss   [vg_dy], xmm1
    call    vr_f
    mulss   xmm0, [vk_0_05]
    addss   xmm0, [vk_0_03]
    mulss   xmm0, [vg_H]
    movss   xmm1, xmm0
    mulss   xmm0, [vg_dx]
    movss   [vg_lx], xmm0
    mulss   xmm1, [vg_dy]
    movss   [vg_ly], xmm1
    ; радиус ствола у земли 1.9..2.4, крона с 0.48..0.6 высоты
    call    vr_f
    mulss   xmm0, [f_0_5]
    addss   xmm0, [vk_1_9]
    movss   [vg_R], xmm0
    call    vr_f
    mulss   xmm0, [vk_0_12]
    addss   xmm0, [vk_0_48]
    mulss   xmm0, [vg_H]
    movss   [vg_zc], xmm0
    ; ----- ствол
    movss   xmm0, [vk_m2]
    movss   [vg_z], xmm0
.tr:
    movss   xmm0, [vg_z]
    call    vg_plean                    ; xmm0, xmm1 — ось на высоте z
    movss   [vb_px], xmm0
    movss   [vb_py], xmm1
    movss   xmm0, [vg_z]
    divss   xmm0, [vg_H]
    movss   [vb_t], xmm0
    mulss   xmm0, [vk_0_6]
    movss   xmm3, [f_1]
    subss   xmm3, xmm0
    mulss   xmm3, [vg_R]
    maxss   xmm3, [vk_0_55]
    movss   [vb_r], xmm3
    ; цвет: до 0.3 — серо-бурый (борозды темнее), к 0.45 — в рыжий
    movss   xmm0, [vb_t]
    subss   xmm0, [vk_0_3]
    mulss   xmm0, [vk_6_67]
    CLAMP01 xmm0
    mov     ecx, VC_PBARKL
    mov     edx, VC_PBARKH
    call    vc_lerp
    call    vc_jit
    movss   xmm0, [vb_t]
    comiss  xmm0, [vk_0_3]
    jae     .tc
    call    vr_u
    test    eax, 3
    jnz     .tc
    mov     r8d, 170
    call    vc_mul
.tc:
    movss   xmm0, [vb_px]
    movss   xmm1, [vb_py]
    movss   xmm2, [vg_z]
    movss   xmm3, [vb_r]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     edx, VK_BARK
    call    vb_add
    movss   xmm0, [vg_z]
    addss   xmm0, [vk_0_45]
    movss   [vg_z], xmm0
    movss   xmm1, [vg_H]
    subss   xmm1, [vk_2]
    comiss  xmm0, xmm1
    jb      .tr
    ; ----- сухие сучки-обломки на нижнем стволе
    mov     r12d, 6
.stub:
    call    vr_f
    mulss   xmm0, [vk_0_35]
    addss   xmm0, [vk_0_15]
    mulss   xmm0, [vg_H]
    movss   [vg_z], xmm0
    call    vg_plean
    movss   [vb_px], xmm0
    movss   [vb_py], xmm1
    call    vr_f
    mulss   xmm0, [vk_2pi]
    call    vg_cs
    movss   [vb_bx], xmm0
    movss   [vb_by], xmm1
    call    vr_f
    mulss   xmm0, [vk_1_5]
    addss   xmm0, [vk_2]
    movss   [vb_l], xmm0              ; длина обломка
    movss   xmm0, [vg_R]
    movss   [vb_t], xmm0              ; от оси — с коры
.st1:
    movss   xmm0, [vb_t]
    movss   xmm1, [vg_R]
    addss   xmm1, [vb_l]
    comiss  xmm0, xmm1
    jae     .st2
    movss   xmm0, [vb_bx]
    mulss   xmm0, [vb_t]
    addss   xmm0, [vb_px]
    movss   xmm1, [vb_by]
    mulss   xmm1, [vb_t]
    addss   xmm1, [vb_py]
    movss   xmm2, [vg_z]
    movss   xmm3, [vk_0_55]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     ecx, 0x5A544E
    mov     edx, VK_TWIG
    call    vb_add
    movss   xmm0, [vb_t]
    addss   xmm0, [vk_0_45]
    movss   [vb_t], xmm0
    jmp     .st1
.st2:
    dec     r12d
    jnz     .stub
    ; ----- сучья с пучками хвои
    call    vr_u
    and     eax, 0xFFFF
    imul    eax, eax, 5
    shr     eax, 16
    lea     r12d, [eax + 8]             ; 8..12
    xor     r13d, r13d
.lb:
    cmp     r13d, r12d
    jae     .ltop
    ; доля по высоте кроны
    call    vr_f
    mulss   xmm0, [vk_0_6]
    addss   xmm0, [vk_0_2]
    cvtsi2ss xmm1, r13d
    addss   xmm0, xmm1
    cvtsi2ss xmm1, r12d
    divss   xmm0, xmm1
    movss   [vg_t], xmm0
    movss   xmm1, [vg_H]
    subss   xmm1, [vg_zc]
    subss   xmm1, [vk_4]
    mulss   xmm1, xmm0
    addss   xmm1, [vg_zc]
    movss   [vg_z], xmm1
    ; азимут — золотой угол, вверх 20..50°
    cvtsi2ss xmm0, r13d
    mulss   xmm0, [vk_2_4]
    movss   [vg_phi], xmm0
    call    vr_s
    mulss   xmm0, [vk_0_4]
    addss   xmm0, [vg_phi]
    movss   [vg_phi], xmm0
    call    vr_f
    mulss   xmm0, [vk_0_2]
    movss   xmm1, [vg_t]
    mulss   xmm1, [vk_0_6]
    addss   xmm0, xmm1
    addss   xmm0, [vk_0_1]
    movss   [vg_el], xmm0
    ; длина (8..16)·(1 − 0.4 доли)
    call    vr_f
    mulss   xmm0, [vk_8]
    addss   xmm0, [vk_8]
    movss   xmm1, [vg_t]
    mulss   xmm1, [vk_0_4]
    movss   xmm2, [f_1]
    subss   xmm2, xmm1
    mulss   xmm0, xmm2
    movss   [vg_L], xmm0
    ; пучок: 3.8..6.4 в радиус (выше — меньше), 2.6..3.8 в высоту
    call    vr_f
    mulss   xmm0, [vk_2_6]
    addss   xmm0, [vk_3_8]
    movss   xmm1, [vg_t]
    mulss   xmm1, [vk_0_3]
    movss   xmm2, [f_1]
    subss   xmm2, xmm1
    mulss   xmm0, xmm2
    movss   [vg_Rc], xmm0
    call    vg_plimb
    ; три четверти сучьев — с отростком поменьше
    call    vr_f
    comiss  xmm0, [vk_0_75]
    jae     .lbn
    call    vr_s
    mulss   xmm0, [vk_0_7]
    addss   xmm0, [vg_phi]
    movss   [vg_phi], xmm0
    movss   xmm0, [vg_el]
    addss   xmm0, [vk_0_15]
    movss   [vg_el], xmm0
    movss   xmm0, [vg_L]
    mulss   xmm0, [vk_0_55]
    movss   [vg_L], xmm0
    movss   xmm0, [vg_Rc]
    mulss   xmm0, [vk_0_7]
    movss   [vg_Rc], xmm0
    movss   xmm0, [vg_z]
    addss   xmm0, [vk_2]
    movss   [vg_z], xmm0
    call    vg_plimb
.lbn:
    inc     r13d
    jmp     .lb
.ltop:
    ; верхний пучок на макушке
    movss   xmm0, [vg_H]
    subss   xmm0, [f_1]
    call    vg_plean
    movss   [vc_x], xmm0
    movss   [vc_y], xmm1
    movss   xmm0, [vg_H]
    subss   xmm0, [f_1]
    movss   [vc_z], xmm0
    call    vr_f
    addss   xmm0, [vk_3]
    movss   [vc_r], xmm0
    call    vr_f
    addss   xmm0, [vk_2]
    movss   [vc_h], xmm0
    call    vg_clump
    add     rsp, 40
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; xmm0 = z -> xmm0, xmm1 = ось сосны на этой высоте (наклон ∝ (z/H)²) (портит xmm2)
vg_plean:
    divss   xmm0, [vg_H]
    mulss   xmm0, xmm0
    movaps  xmm1, xmm0
    mulss   xmm0, [vg_lx]
    mulss   xmm1, [vg_ly]
    ret

; сук сосны: из оси на высоте vg_z по азимуту vg_phi, вверх vg_el (рад), длиной vg_L;
; на конце — пучок радиуса vg_Rc
vg_plimb:
    push    rbx
    sub     rsp, 32
    movss   xmm0, [vg_el]
    call    vg_cs
    movss   [vb_w], xmm0                ; cos el
    movss   [vb_l], xmm1                ; sin el
    movss   xmm0, [vg_phi]
    call    vg_cs
    mulss   xmm0, [vb_w]
    mulss   xmm1, [vb_w]
    movss   [vb_bx], xmm0
    movss   [vb_by], xmm1
    movss   xmm0, [vg_z]
    call    vg_plean
    movss   [vb_px], xmm0
    movss   [vb_py], xmm1
    xorps   xmm0, xmm0
    movss   [vb_t], xmm0                ; пройдено
.c:
    movss   xmm0, [vb_t]
    comiss  xmm0, [vg_L]
    jae     .end
    ; точка: начало + dir·s, к концу чуть провисает
    movss   xmm0, [vb_bx]
    mulss   xmm0, [vb_t]
    addss   xmm0, [vb_px]
    movss   [vc_x], xmm0
    movss   xmm1, [vb_by]
    mulss   xmm1, [vb_t]
    addss   xmm1, [vb_py]
    movss   [vc_y], xmm1
    movss   xmm2, [vb_l]
    mulss   xmm2, [vb_t]
    addss   xmm2, [vg_z]
    movss   xmm3, [vb_t]
    mulss   xmm3, xmm3
    mulss   xmm3, [vk_0_02]
    subss   xmm2, xmm3
    movss   [vc_z], xmm2
    ; r = 0.85 − 0.4·s/L
    movss   xmm3, [vb_t]
    divss   xmm3, [vg_L]
    mulss   xmm3, [vk_0_4]
    movss   xmm4, [vk_0_85]
    subss   xmm4, xmm3
    movss   [vb_r], xmm4
    mov     ecx, VC_PLIMB
    call    vc_jit
    movss   xmm0, [vc_x]
    movss   xmm1, [vc_y]
    movss   xmm2, [vc_z]
    movss   xmm3, [vb_r]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     edx, VK_BARK
    call    vb_add
    movss   xmm0, [vb_t]
    addss   xmm0, [f_0_5]
    movss   [vb_t], xmm0
    jmp     .c
.end:
    movss   xmm0, [vc_z]
    addss   xmm0, [f_0_5]
    movss   [vc_z], xmm0
    movss   xmm0, [vg_Rc]
    movss   [vc_r], xmm0
    call    vr_f
    mulss   xmm0, [vk_1_2]
    addss   xmm0, [vk_2_6]
    movss   [vc_h], xmm0
    call    vg_clump
    add     rsp, 32
    pop     rbx
    ret

; пучок хвои сосны: сплюснутый эллипсоид (vc_x, vc_y, vc_z; радиус vc_r, высота vc_h),
; снаружи и сверху светлее; сверху — снег по нагрузке
vg_clump:
    push    rbx
    push    rsi
    sub     rsp, 40
    movss   xmm0, [vc_r]
    mulss   xmm0, xmm0
    mulss   xmm0, [vk_2_4]
    cvttss2si ebx, xmm0
    add     ebx, 6
    mov     esi, ebx
.p:
    ; точка: u = √rnd·R по кругу, по высоте — ±h/2 и тоньше к краю
    call    vr_f
    sqrtss  xmm0, xmm0
    movss   [vb_t], xmm0                ; u/R
    call    vr_f
    mulss   xmm0, [vk_2pi]
    call    vg_cs
    movss   xmm2, [vb_t]
    mulss   xmm2, [vc_r]
    mulss   xmm0, xmm2
    mulss   xmm1, xmm2
    addss   xmm0, [vc_x]
    addss   xmm1, [vc_y]
    movss   [vb_px], xmm0
    movss   [vb_py], xmm1
    movss   xmm0, [vb_t]
    mulss   xmm0, xmm0
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    movss   [vb_w], xmm1                ; 1 − (u/R)²
    call    vr_s
    movss   [vb_l], xmm0                ; −1..1 по высоте
    mulss   xmm0, [vb_w]
    mulss   xmm0, [vc_h]
    mulss   xmm0, [f_0_5]
    addss   xmm0, [vc_z]
    movss   [vb_pz], xmm0
    ; тень: 0.5 + 0.35·u/R + 0.15·выше
    movss   xmm0, [vb_t]
    mulss   xmm0, [vk_0_35]
    movss   xmm1, [vb_l]
    mulss   xmm1, [vk_0_15]
    addss   xmm0, xmm1
    addss   xmm0, [vk_0_55]
    mulss   xmm0, [f_256]
    cvttss2si eax, xmm0
    mov     [vb_ao], eax
    call    vr_f
    mulss   xmm0, [vk_0_8]
    addss   xmm0, [f_1]
    movss   [vb_r], xmm0
    mov     ecx, VC_PNEEDLE
    call    vc_jit
    mov     r8d, [vb_ao]
    call    vc_mul
    movss   xmm0, [vb_px]
    movss   xmm1, [vb_py]
    movss   xmm2, [vb_pz]
    movss   xmm3, [vb_r]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     edx, VK_NEEDLE
    call    vb_add
    dec     esi
    jnz     .p
    ; снег сверху пучка: доля 0.2·нагрузка
    cvtsi2ss xmm0, ebx
    mulss   xmm0, [vk_0_2]
    mulss   xmm0, [vg_snow]
    cvttss2si esi, xmm0
    test    esi, esi
    jz      .out
.sn:
    call    vr_f
    sqrtss  xmm0, xmm0
    mulss   xmm0, [vk_0_85]
    movss   [vb_t], xmm0
    call    vr_f
    mulss   xmm0, [vk_2pi]
    call    vg_cs
    movss   xmm2, [vb_t]
    mulss   xmm2, [vc_r]
    mulss   xmm0, xmm2
    mulss   xmm1, xmm2
    addss   xmm0, [vc_x]
    addss   xmm1, [vc_y]
    movss   [vb_px], xmm0
    movss   [vb_py], xmm1
    movss   xmm0, [vb_t]
    mulss   xmm0, xmm0
    movss   xmm2, [f_1]
    subss   xmm2, xmm0
    mulss   xmm2, [vc_h]
    mulss   xmm2, [f_0_5]
    addss   xmm2, [vc_z]
    addss   xmm2, [vk_0_7]
    movss   [vb_pz], xmm2
    call    vr_f
    mulss   xmm0, [vk_0_9]
    addss   xmm0, [vk_0_9]
    movaps  xmm3, xmm0
    movss   xmm0, [vb_px]
    movss   xmm1, [vb_py]
    movss   xmm2, [vb_pz]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     ecx, VC_SNOW
    mov     edx, VK_SNOW
    call    vb_add
    dec     esi
    jnz     .sn
.out:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- берёза
; 1-3 ствола от одного корня (веером), чуть изогнуты; берёста белая с чёрточками,
; у земли чёрная в трещинах, к макушке бурая; сучья круто вверх, с них свисают
; тонкие веточки (издали крона лиловая)
vg_birch:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    mov     r12d, 1
    call    vr_f
    comiss  xmm0, [vk_0_35]
    jae     .n1
    inc     r12d
.n1:
    call    vr_f
    comiss  xmm0, [vk_0_12]
    jae     .n2
    inc     r12d
.n2:
    call    vr_f
    mulss   xmm0, [vk_2pi]
    movss   [vg_phi0], xmm0
    ; тёмный низ — до 0.06..0.12 высоты
    call    vr_f
    mulss   xmm0, [vk_0_06]
    addss   xmm0, [vk_0_06]
    mulss   xmm0, [vg_H]
    movss   [vg_bdz], xmm0
    xor     r13d, r13d                  ; ствол
.stem:
    cmp     r13d, r12d
    jae     .out
    ; куда клонится
    cvtsi2ss xmm0, r13d
    cvtsi2ss xmm1, r12d
    divss   xmm0, xmm1
    mulss   xmm0, [vk_2pi]
    addss   xmm0, [vg_phi0]
    call    vg_cs
    movss   [vg_dx], xmm0
    movss   [vg_dy], xmm1
    call    vr_f
    mulss   xmm0, [vk_0_05]
    addss   xmm0, [vk_0_03]
    cmp     r12d, 1
    je      .k1
    call    vr_f
    mulss   xmm0, [vk_0_08]
    addss   xmm0, [vk_0_1]
.k1:
    movss   [vg_s0], xmm0               ; наклон
    ; основание: у нескольких — врозь на 0.8 U
    xorps   xmm0, xmm0
    xorps   xmm1, xmm1
    cmp     r12d, 1
    je      .b1
    movss   xmm0, [vg_dx]
    mulss   xmm0, [vk_0_8]
    movss   xmm1, [vg_dy]
    mulss   xmm1, [vk_0_8]
.b1:
    movss   [vg_lx], xmm0
    movss   [vg_ly], xmm1
    call    vr_s
    mulss   xmm0, [vk_0_06]
    movss   [vg_dr], xmm0               ; изгиб
    ; высота ствола: первый — H, прочие 0.85..1
    movss   xmm0, [vg_H]
    test    r13d, r13d
    jz      .h1
    call    vr_f
    mulss   xmm0, [vk_0_15]
    addss   xmm0, [vk_0_85]
    mulss   xmm0, [vg_H]
.h1:
    movss   [vg_Rc], xmm0               ; Hs
    call    vr_f
    mulss   xmm0, [vk_0_4]
    addss   xmm0, [vk_1_8]
    cmp     r12d, 1
    je      .rb
    mulss   xmm0, [vk_0_77]
.rb:
    movss   [vg_R], xmm0                ; радиус у земли
    ; ----- ствол
    movss   xmm0, [vk_m2]
    movss   [vg_z], xmm0
.tr:
    movss   xmm0, [vg_z]
    call    vg_baxis
    movss   [vb_px], xmm0
    movss   [vb_py], xmm1
    movss   xmm0, [vg_z]
    divss   xmm0, [vg_Rc]
    movss   [vb_t], xmm0
    mulss   xmm0, [vk_0_72]
    movss   xmm3, [f_1]
    subss   xmm3, xmm0
    mulss   xmm3, [vg_R]
    maxss   xmm3, [vk_0_4]
    movss   [vb_r], xmm3
    ; цвет: белый, выше 0.72 — к бурому
    movss   xmm0, [vb_t]
    subss   xmm0, [vk_0_72]
    mulss   xmm0, [vk_3_6]
    CLAMP01 xmm0
    mov     ecx, VC_BWHITE
    mov     edx, VC_BTOP
    call    vc_lerp
    call    vr_u
    and     eax, 15
    add     eax, 244
    mov     r8d, eax
    call    vc_mul
    mov     [vb_ao], ecx
    ; чёрточка: 30 % колец, своя ширина и сторона
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     edx, VK_BIRCH
    call    vr_f
    comiss  xmm0, [vk_0_3]
    jae     .nd
    call    vr_f
    mulss   xmm0, [vk_35]
    addss   xmm0, [vk_55]
    cvttss2si edx, xmm0
    shl     edx, 8
    or      edx, VK_BIRCH
    mov     [vb_n], edx
    call    vr_f
    mulss   xmm0, [vk_2pi]
    call    vg_cs
    movaps  xmm4, xmm0
    movaps  xmm5, xmm1
    mov     edx, [vb_n]
.nd:
    mov     ecx, [vb_ao]
    movss   xmm0, [vb_px]
    movss   xmm1, [vb_py]
    movss   xmm2, [vg_z]
    movss   xmm3, [vb_r]
    call    vb_add
    movss   xmm0, [vg_z]
    addss   xmm0, [vk_0_42]
    movss   [vg_z], xmm0
    comiss  xmm0, [vg_Rc]
    jb      .tr
    ; ----- сучья: 9..14, от 0.32 до 0.92 высоты ствола (у многоствольной — меньше)
    call    vr_u
    and     eax, 0xFFFF
    imul    eax, eax, 6
    shr     eax, 16
    lea     r14d, [eax + 9]
    cmp     r12d, 1
    je      .nl
    sub     r14d, 3
.nl:
    xor     r15d, r15d
.lb:
    cmp     r15d, r14d
    jae     .stn
    call    vr_f
    mulss   xmm0, [f_0_5]
    cvtsi2ss xmm1, r15d
    addss   xmm0, xmm1
    cvtsi2ss xmm1, r14d
    divss   xmm0, xmm1
    movss   [vg_t], xmm0
    mulss   xmm0, [vk_0_6]
    addss   xmm0, [vk_0_32]
    mulss   xmm0, [vg_Rc]
    movss   [vg_zl], xmm0
    ; азимут — от наклона ствола наружу охотнее
    call    vr_f
    mulss   xmm0, [vk_2pi]
    movss   [vg_phi], xmm0
    ; вверх 37..63°
    call    vr_f
    mulss   xmm0, [vk_0_45]
    addss   xmm0, [vk_0_65]
    movss   [vg_el], xmm0
    ; длина (10..20)·(1 − 0.4 доли)
    call    vr_f
    mulss   xmm0, [vk_10]
    addss   xmm0, [vk_10]
    movss   xmm1, [vg_t]
    mulss   xmm1, [vk_0_4]
    movss   xmm2, [f_1]
    subss   xmm2, xmm1
    mulss   xmm0, xmm2
    movss   [vg_L], xmm0
    call    vg_blimb
    inc     r15d
    jmp     .lb
.stn:
    ; на макушке — пучок веточек
    movss   xmm0, [vg_Rc]
    movss   [vg_zl], xmm0
    mov     r15d, 5
.tt:
    call    vr_f
    mulss   xmm0, [vk_2pi]
    movss   [vg_phi], xmm0
    movss   xmm0, [vg_zl]
    subss   xmm0, [f_1]
    call    vg_baxis
    movss   [vc_x], xmm0
    movss   [vc_y], xmm1
    movss   xmm0, [vg_zl]
    subss   xmm0, [f_1]
    movss   [vc_z], xmm0
    call    vg_twig
    dec     r15d
    jnz     .tt
    inc     r13d
    jmp     .stem
.out:
    add     rsp, 40
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; xmm0 = z -> xmm0, xmm1: ось ствола берёзы (основание vg_lx/ly, наклон vg_s0 по
; vg_dx/dy, изгиб vg_dr·z²/Hs поперёк) (портит xmm2, xmm3)
vg_baxis:
    movaps  xmm2, xmm0
    mulss   xmm2, [vg_s0]               ; вдоль наклона
    movaps  xmm3, xmm0
    mulss   xmm3, xmm0
    divss   xmm3, [vg_Rc]
    mulss   xmm3, [vg_dr]               ; поперёк
    movss   xmm0, [vg_dx]
    mulss   xmm0, xmm2
    movss   xmm1, [vg_dy]
    mulss   xmm1, xmm3
    subss   xmm0, xmm1
    addss   xmm0, [vg_lx]
    movss   xmm1, [vg_dy]
    mulss   xmm1, xmm2
    mulss   xmm3, [vg_dx]
    addss   xmm1, xmm3
    addss   xmm1, [vg_ly]
    ret

; сук берёзы: со ствола на высоте vg_zl, азимут vg_phi, вверх vg_el, длина vg_L;
; к концу изгибается наружу, тоньше и темнее; с него свисают веточки
vg_blimb:
    push    rbx
    sub     rsp, 32
    movss   xmm0, [vg_el]
    call    vg_cs
    movss   [vb_w], xmm0
    movss   [vb_l], xmm1
    movss   xmm0, [vg_phi]
    call    vg_cs
    movss   [vb_bx], xmm0
    movss   [vb_by], xmm1
    movss   xmm0, [vg_zl]
    call    vg_baxis
    movss   [vb_px], xmm0
    movss   [vb_py], xmm1
    xorps   xmm0, xmm0
    movss   [vb_t], xmm0
    xor     ebx, ebx                    ; шагов (через 3 — веточка)
.c:
    movss   xmm0, [vb_t]
    comiss  xmm0, [vg_L]
    jae     .out
    ; наружу: cos el·s + 0.03 s², вверх: sin el·s
    movss   xmm1, [vb_w]
    mulss   xmm1, xmm0
    movaps  xmm2, xmm0
    mulss   xmm2, xmm0
    mulss   xmm2, [vk_0_03]
    addss   xmm1, xmm2
    movss   xmm2, [vb_bx]
    mulss   xmm2, xmm1
    addss   xmm2, [vb_px]
    movss   [vc_x], xmm2
    movss   xmm2, [vb_by]
    mulss   xmm2, xmm1
    addss   xmm2, [vb_py]
    movss   [vc_y], xmm2
    movss   xmm2, [vb_l]
    mulss   xmm2, xmm0
    addss   xmm2, [vg_zl]
    movss   [vc_z], xmm2
    ; r = 0.75 − 0.42·s/L, цвет — от светлой берёсты к бурому
    divss   xmm0, [vg_L]
    movss   [vb_ao], xmm0
    mulss   xmm0, [vk_0_42]
    movss   xmm3, [vk_0_75]
    subss   xmm3, xmm0
    maxss   xmm3, [vk_0_3]
    movss   [vb_r], xmm3
    movss   xmm0, [vb_ao]
    mov     ecx, VC_BLIMB
    mov     edx, VC_BTIP
    call    vc_lerp
    movss   xmm0, [vc_x]
    movss   xmm1, [vc_y]
    movss   xmm2, [vc_z]
    movss   xmm3, [vb_r]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     edx, VK_BIRCH
    call    vb_add
    ; веточки — с пятой части сука, на каждом шаге по две
    inc     ebx
    movss   xmm0, [vb_ao]
    comiss  xmm0, [vk_0_2]
    jb      .cn
    cmp     ebx, 1
    jb      .cn
    xor     ebx, ebx
    movss   xmm0, [vg_phi]
    movss   [vg_ang2], xmm0
    call    vr_s
    mulss   xmm0, [vk_1_2]
    addss   xmm0, [vg_ang2]
    movss   [vg_phi], xmm0
    call    vg_twig
    call    vr_s
    mulss   xmm0, [vk_1_2]
    addss   xmm0, [vg_ang2]
    movss   [vg_phi], xmm0
    call    vg_twig
.c1:
    movss   xmm0, [vg_ang2]
    movss   [vg_phi], xmm0
.cn:
    movss   xmm0, [vb_t]
    addss   xmm0, [vk_0_45]
    movss   [vb_t], xmm0
    jmp     .c
.out:
    add     rsp, 32
    pop     rbx
    ret

; веточка берёзы из (vc_x, vc_y, vc_z) по азимуту vg_phi: чуть вверх и наружу, потом
; свисает (плакучая), 4..9 U; к концу светлее. Портит vb_* своими — сохраняет их
vg_twig:
    push    rbx
    sub     rsp, 32
    ; свои: vt_*
    movss   xmm0, [vg_phi]
    call    vg_cs
    movss   [vt_dx], xmm0
    movss   [vt_dy], xmm1
    call    vr_f
    mulss   xmm0, [vk_6]
    addss   xmm0, [vk_5]
    movss   [vt_len], xmm0
    xorps   xmm0, xmm0
    movss   [vt_s], xmm0
.p:
    movss   xmm0, [vt_s]
    comiss  xmm0, [vt_len]
    jae     .out
    ; наружу 0.35 s, вверх 0.25 s − 0.07 s² — плакучая: чуть вверх и вниз прядью
    movaps  xmm1, xmm0
    mulss   xmm1, [vk_0_35]
    movss   xmm2, [vt_dx]
    mulss   xmm2, xmm1
    addss   xmm2, [vc_x]
    movss   [vt_x], xmm2
    movss   xmm2, [vt_dy]
    mulss   xmm2, xmm1
    addss   xmm2, [vc_y]
    movss   [vt_y], xmm2
    movaps  xmm2, xmm0
    mulss   xmm2, [vk_0_25]
    movaps  xmm3, xmm0
    mulss   xmm3, xmm0
    mulss   xmm3, [vk_0_07]
    subss   xmm2, xmm3
    addss   xmm2, [vc_z]
    movss   [vt_z], xmm2
    divss   xmm0, [vt_len]
    mov     ecx, VC_TWIG
    mov     edx, VC_TWIGT
    call    vc_lerp
    call    vc_jit
    movss   xmm0, [vt_x]
    movss   xmm1, [vt_y]
    movss   xmm2, [vt_z]
    movss   xmm3, [vk_0_55]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     edx, VK_TWIG
    call    vb_add
    movss   xmm0, [vt_s]
    addss   xmm0, [vk_0_45]
    movss   [vt_s], xmm0
    jmp     .p
.out:
    add     rsp, 32
    pop     rbx
    ret

; ---------------------------------------------------------------- куст, бурьян, пень
; стебель: из (vt_x, vt_y, vt_z) по азимуту vg_phi с наклоном vg_el (от вертикали),
; к концу клонится сильнее (vg_dr — добавка), длина vg_L; цвет vg_ncol -> vg_ncol2 к концу;
; vg_s0 — где переломлен (доля длины, ≥ 1 — цел)
vg_stalk:
    push    rbx
    sub     rsp, 32
    movss   xmm0, [vg_phi]
    call    vg_cs
    movss   [vt_dx], xmm0
    movss   [vt_dy], xmm1
    xorps   xmm0, xmm0
    movss   [vt_s], xmm0
.p:
    movss   xmm0, [vt_s]
    comiss  xmm0, [vg_L]
    jae     .out
    ; наклон сейчас
    divss   xmm0, [vg_L]
    movss   [vt_len], xmm0
    movaps  xmm1, xmm0
    mulss   xmm1, [vg_dr]
    addss   xmm1, [vg_el]
    comiss  xmm0, [vg_s0]
    jb      .nb
    movss   xmm1, [vk_1_4]              ; переломлен — никнет
.nb:
    movaps  xmm0, xmm1
    call    vg_cs                       ; cos — вверх, sin — вбок
    mulss   xmm0, [vk_0_45]
    mulss   xmm1, [vk_0_45]
    addss   xmm0, [vt_z]
    movss   [vt_z], xmm0
    movss   xmm2, [vt_dx]
    mulss   xmm2, xmm1
    addss   xmm2, [vt_x]
    movss   [vt_x], xmm2
    mulss   xmm1, [vt_dy]
    addss   xmm1, [vt_y]
    movss   [vt_y], xmm1
    movss   xmm0, [vt_len]
    mov     ecx, [vg_ncol]
    mov     edx, [vg_ncol2]
    call    vc_lerp
    movss   xmm0, [vt_x]
    movss   xmm1, [vt_y]
    movss   xmm2, [vt_z]
    movss   xmm3, [vk_0_6]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     edx, VK_TWIG
    call    vb_add
    movss   xmm0, [vt_s]
    addss   xmm0, [vk_0_45]
    movss   [vt_s], xmm0
    jmp     .p
.out:
    add     rsp, 32
    pop     rbx
    ret

; ивняк (vg_sub 0) или бурьян (1): vg_H — высота, U
vg_bush:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40
    cmp     dword [vg_sub], 0
    jne     .weeds
    ; ----- ивняк: 10..18 прутьев веером из одного места, у корня — сугроб
    call    vr_u
    and     eax, 7
    lea     r12d, [eax + 10]
.w:
    call    vr_f
    sqrtss  xmm0, xmm0
    mulss   xmm0, [vk_1_5]
    movss   [vb_t], xmm0
    call    vr_f
    mulss   xmm0, [vk_2pi]
    movss   [vg_phi], xmm0
    call    vg_cs
    mulss   xmm0, [vb_t]
    mulss   xmm1, [vb_t]
    movss   [vt_x], xmm0
    movss   [vt_y], xmm1
    mov     dword [vt_z], __float32__(-0.5)
    call    vr_f
    mulss   xmm0, [vk_0_45]
    addss   xmm0, [vk_0_12]
    movss   [vg_el], xmm0
    mulss   xmm0, [f_0_5]
    movss   [vg_dr], xmm0
    call    vr_f
    mulss   xmm0, [vk_0_45]
    addss   xmm0, [vk_0_55]
    mulss   xmm0, [vg_H]
    movss   [vg_L], xmm0
    mov     dword [vg_s0], __float32__(2.0)
    mov     dword [vg_ncol], VC_WILLOW
    mov     dword [vg_ncol2], 0x7A4A3A
    call    vr_f
    comiss  xmm0, [vk_0_3]
    jae     .wc
    mov     dword [vg_ncol], VC_WILLOW2
    mov     dword [vg_ncol2], 0x8A7C52
.wc:
    call    vg_stalk
    dec     r12d
    jnz     .w
    ; сугроб у корня
    mov     r12d, 5
.wm:
    call    vr_s
    mulss   xmm0, [vk_1_6]
    movss   [vb_px], xmm0
    call    vr_s
    mulss   xmm0, [vk_1_6]
    movss   [vb_py], xmm0
    call    vr_f
    mulss   xmm0, [vk_0_6]
    subss   xmm0, [vk_0_8]
    movss   [vb_pz], xmm0
    call    vr_f
    addss   xmm0, [vk_2_2]
    movaps  xmm3, xmm0
    movss   xmm0, [vb_px]
    movss   xmm1, [vb_py]
    movss   xmm2, [vb_pz]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     ecx, VC_SNOW
    mov     edx, VK_SNOW
    call    vb_add
    dec     r12d
    jnz     .wm
    jmp     .out
.weeds:
    ; ----- бурьян: 5..10 стеблей, репьи, пижма, иные переломлены
    call    vr_u
    and     eax, 0xFFFF
    imul    eax, eax, 6
    shr     eax, 16
    lea     r12d, [eax + 5]
.b:
    call    vr_f
    sqrtss  xmm0, xmm0
    mulss   xmm0, [vk_1_2]
    movss   [vb_t], xmm0
    call    vr_f
    mulss   xmm0, [vk_2pi]
    movss   [vg_phi], xmm0
    call    vg_cs
    mulss   xmm0, [vb_t]
    mulss   xmm1, [vb_t]
    movss   [vt_x], xmm0
    movss   [vt_y], xmm1
    mov     dword [vt_z], __float32__(-0.5)
    call    vr_f
    mulss   xmm0, [vk_0_3]
    addss   xmm0, [vk_0_05]
    movss   [vg_el], xmm0
    mov     dword [vg_dr], __float32__(0.1)
    call    vr_f
    mulss   xmm0, [f_0_5]
    addss   xmm0, [f_0_5]
    mulss   xmm0, [vg_H]
    movss   [vg_L], xmm0
    mov     dword [vg_s0], __float32__(2.0)
    call    vr_f
    comiss  xmm0, [vk_0_15]
    jae     .bb
    mov     dword [vg_s0], __float32__(0.6)
.bb:
    mov     dword [vg_ncol], VC_STRAW
    mov     dword [vg_ncol2], 0xA08E62
    call    vr_f
    comiss  xmm0, [vk_0_4]
    jae     .bc
    mov     dword [vg_ncol], VC_DRYB
    mov     dword [vg_ncol2], 0x6E5A40
.bc:
    call    vg_stalk
    ; головка у конца стебля (vt_x, vt_y, vt_z)
    call    vr_f
    comiss  xmm0, [vk_0_45]
    jb      .burr
    comiss  xmm0, [vk_0_75]
    jb      .tansy
    jmp     .bn
.burr:
    mov     dword [vg_ncol], VC_BURR
    mov     dword [vb_r], __float32__(0.95)
    mov     esi, 4
    jmp     .head
.tansy:
    mov     dword [vg_ncol], VC_TANSY
    mov     dword [vb_r], __float32__(0.75)
    mov     esi, 5
.head:
    call    vr_s
    mulss   xmm0, [vk_0_9]
    addss   xmm0, [vt_x]
    movss   [vb_px], xmm0
    call    vr_s
    mulss   xmm0, [vk_0_9]
    addss   xmm0, [vt_y]
    movss   [vb_py], xmm0
    call    vr_s
    mulss   xmm0, [f_0_5]
    addss   xmm0, [vt_z]
    movss   [vb_pz], xmm0
    mov     ecx, [vg_ncol]
    call    vc_jit
    movss   xmm0, [vb_px]
    movss   xmm1, [vb_py]
    movss   xmm2, [vb_pz]
    movss   xmm3, [vb_r]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     edx, VK_TWIG
    call    vb_add
    dec     esi
    jnz     .head
    ; снежок на головке
    call    vr_f
    comiss  xmm0, [vk_0_3]
    jae     .bn
    movss   xmm0, [vt_x]
    movss   xmm1, [vt_y]
    movss   xmm2, [vt_z]
    addss   xmm2, [vk_0_8]
    movss   xmm3, [vk_0_8]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     ecx, VC_SNOW
    mov     edx, VK_SNOW
    call    vb_add
.bn:
    dec     r12d
    jnz     .b
.out:
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; пень: столбик коры радиуса 2.3..3.3, сверху снежная шапка
vg_stump:
    push    rbx
    sub     rsp, 32
    call    vr_f
    addss   xmm0, [vk_2_3]
    movss   [vg_R], xmm0
    movss   xmm0, [vk_m1]
    movss   [vg_z], xmm0
.c:
    mov     ecx, VC_STUMP
    call    vc_jit
    xorps   xmm0, xmm0
    xorps   xmm1, xmm1
    movss   xmm2, [vg_z]
    movss   xmm3, [vg_R]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     edx, VK_BARK
    call    vb_add
    movss   xmm0, [vg_z]
    addss   xmm0, [f_0_5]
    movss   [vg_z], xmm0
    comiss  xmm0, [vg_H]
    jb      .c
    ; шапка
    xorps   xmm0, xmm0
    xorps   xmm1, xmm1
    movss   xmm2, [vg_H]
    addss   xmm2, [vk_0_6]
    movss   xmm3, [vg_R]
    mulss   xmm3, [vk_0_85]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     ecx, VC_SNOW
    mov     edx, VK_SNOW
    call    vb_add
    mov     ebx, 6
.s:
    call    vr_s
    mulss   xmm0, [vg_R]
    mulss   xmm0, [vk_0_6]
    movss   [vb_px], xmm0
    call    vr_s
    mulss   xmm0, [vg_R]
    mulss   xmm0, [vk_0_6]
    movss   [vb_py], xmm0
    call    vr_f
    mulss   xmm0, [vk_0_6]
    addss   xmm0, [vk_1_2]
    movaps  xmm3, xmm0
    movss   xmm0, [vb_px]
    movss   xmm1, [vb_py]
    movss   xmm2, [vg_H]
    addss   xmm2, [vk_0_45]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    mov     ecx, VC_SNOW
    mov     edx, VK_SNOW
    call    vb_add
    dec     ebx
    jnz     .s
    add     rsp, 32
    pop     rbx
    ret

; ---------------------------------------------------------------- съёмка модели
; до float -> целое вниз
%macro VFLOOR 3
    cvttss2si %1, %2
    cvtsi2ss %3, %1
    comiss  %3, %2
    jbe     %%ok
    dec     %1
%%ok:
%endmacro

; rbx = шарик -> xmm0 = sx, xmm1 = sy, xmm2 = глубина (к глазу больше) (портит xmm3)
vb_proj:
    movss   xmm0, [rbx]
    subss   xmm0, [rbx + 4]
    mulss   xmm0, [vk_s2]
    movss   xmm1, [rbx]
    addss   xmm1, [rbx + 4]
    movaps  xmm2, xmm1
    mulss   xmm1, [vk_s8]
    movss   xmm3, [rbx + 8]
    mulss   xmm3, [vk_c30]
    subss   xmm1, xmm3
    mulss   xmm2, [vk_c6]
    movss   xmm3, [rbx + 8]
    mulss   xmm3, [f_0_5]
    addss   xmm2, xmm3
    ret

; vg_blobs[vg_nb] -> запись RS (rsi): картинка (альбедо, нормали, точки в мире) и карта
; высот — в аренах vra_*. Шарик на экране — круг; пиксель берёт ближайший к глазу,
; нормаль — сфера шарика в этой точке
vg_render:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    mov     rbp, rsi
    ; ----- 1) рамка
    movss   xmm8, [vk_big]
    movss   xmm9, [vk_mbig]
    movss   xmm10, [vk_big]
    movss   xmm11, [vk_mbig]
    lea     rbx, [vg_blobs]
    mov     r12d, [vg_nb]
.b1:
    test    r12d, r12d
    jz      .b2
    call    vb_proj
    movss   xmm3, [rbx + 12]
    maxss   xmm3, [vk_0_6]
    movaps  xmm2, xmm0
    subss   xmm2, xmm3
    minss   xmm8, xmm2
    movaps  xmm2, xmm0
    addss   xmm2, xmm3
    maxss   xmm9, xmm2
    movaps  xmm2, xmm1
    subss   xmm2, xmm3
    minss   xmm10, xmm2
    movaps  xmm2, xmm1
    addss   xmm2, xmm3
    maxss   xmm11, xmm2
    add     rbx, VB_SZ
    dec     r12d
    jmp     .b1
.b2:
    ; брусья — по углам
    xor     r12d, r12d
.bx:
    cmp     r12d, [vg_nx]
    jae     .b3
    imul    eax, r12d, VX_SZ
    lea     rbx, [vg_shp]
    add     rbx, rax
    call    vx_corners
    xor     r13d, r13d
.bc:
    lea     eax, [r13d + r13d*2]
    movss   xmm0, [r10 + rax*4]
    movss   xmm1, [r10 + rax*4 + 4]
    movss   xmm2, [r10 + rax*4 + 8]
    movaps  xmm3, xmm0
    subss   xmm3, xmm1
    mulss   xmm3, [vk_s2]
    addss   xmm0, xmm1
    mulss   xmm0, [vk_s8]
    mulss   xmm2, [vk_c30]
    subss   xmm0, xmm2
    minss   xmm8, xmm3
    maxss   xmm9, xmm3
    minss   xmm10, xmm0
    maxss   xmm11, xmm0
    inc     r13d
    cmp     r13d, 8
    jb      .bc
    inc     r12d
    jmp     .bx
.b3:
    VFLOOR  eax, xmm8, xmm0
    mov     [vr_x0], eax
    cvtsi2ss xmm0, eax
    movss   [vr_x0f], xmm0
    VFLOOR  ecx, xmm9, xmm0
    sub     ecx, eax
    inc     ecx
    mov     eax, VRW
    cmp     ecx, eax
    cmova   ecx, eax
    mov     [vr_w], ecx
    VFLOOR  eax, xmm10, xmm0
    mov     [vr_y0], eax
    cvtsi2ss xmm0, eax
    movss   [vr_y0f], xmm0
    VFLOOR  ecx, xmm11, xmm0
    sub     ecx, eax
    inc     ecx
    mov     eax, VRH
    cmp     ecx, eax
    cmova   ecx, eax
    mov     [vr_h], ecx
    ; ----- 2) буферы: глубина −∞, шарика нет
    mov     ecx, [vr_w]
    imul    ecx, [vr_h]
    mov     [vr_n_px], ecx
    lea     rdi, [vr_d]
    mov     eax, __float32__(-1.0e30)
    rep     stosd
    mov     ecx, [vr_n_px]
    lea     rdi, [vr_b]
    mov     eax, -1
    rep     stosd
    ; ----- 3) шарики
    lea     rbx, [vg_blobs]
    xor     r12d, r12d
.sp:
    cmp     r12d, [vg_nb]
    jae     .spd
    call    vb_proj
    subss   xmm0, [vr_x0f]
    subss   xmm1, [vr_y0f]
    movss   [vs_cx], xmm0
    movss   [vs_cy], xmm1
    movss   [vs_d0], xmm2
    movss   xmm3, [rbx + 12]
    movss   [vs_r], xmm3
    maxss   xmm3, [vk_0_6]
    movss   xmm4, [f_1]
    divss   xmm4, xmm3
    movss   [vs_ir], xmm4
    movaps  xmm5, xmm0
    subss   xmm5, xmm3
    VFLOOR  r13d, xmm5, xmm6
    xor     eax, eax
    cmp     r13d, eax
    cmovl   r13d, eax
    movaps  xmm5, xmm0
    addss   xmm5, xmm3
    VFLOOR  r14d, xmm5, xmm6
    mov     eax, [vr_w]
    dec     eax
    cmp     r14d, eax
    cmovg   r14d, eax
    movaps  xmm5, xmm1
    subss   xmm5, xmm3
    VFLOOR  r15d, xmm5, xmm6
    xor     eax, eax
    cmp     r15d, eax
    cmovl   r15d, eax
    movaps  xmm5, xmm1
    addss   xmm5, xmm3
    VFLOOR  edi, xmm5, xmm6
    mov     eax, [vr_h]
    dec     eax
    cmp     edi, eax
    cmovg   edi, eax
.sj:
    cmp     r15d, edi
    jg      .spn
    cvtsi2ss xmm7, r15d
    addss   xmm7, [f_0_5]
    subss   xmm7, [vs_cy]
    mulss   xmm7, [vs_ir]
    xorps   xmm7, [f_signmask]          ; b — вверх
    mov     esi, r13d
.si:
    cmp     esi, r14d
    jg      .sjn
    cvtsi2ss xmm6, esi
    addss   xmm6, [f_0_5]
    subss   xmm6, [vs_cx]
    mulss   xmm6, [vs_ir]               ; a — вправо
    movaps  xmm0, xmm6
    mulss   xmm0, xmm6
    movaps  xmm1, xmm7
    mulss   xmm1, xmm7
    addss   xmm0, xmm1
    comiss  xmm0, [f_1]
    jae     .sin
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    sqrtss  xmm1, xmm1                  ; c — к глазу
    ; под снегом не видно: точка ниже земли — мимо
    movaps  xmm2, xmm7
    mulss   xmm2, [vk_c30]
    movaps  xmm3, xmm1
    mulss   xmm3, [f_0_5]
    addss   xmm2, xmm3
    mulss   xmm2, [vs_r]
    addss   xmm2, [rbx + 8]
    comiss  xmm2, [vk_m0_2]
    jb      .sin
    movaps  xmm2, xmm1
    mulss   xmm2, [vs_r]
    addss   xmm2, [vs_d0]
    mov     eax, r15d
    imul    eax, [vr_w]
    add     eax, esi
    lea     r8, [vr_d]
    comiss  xmm2, [r8 + rax*4]
    jbe     .sin
    movss   [r8 + rax*4], xmm2
    lea     r8, [vr_b]
    mov     [r8 + rax*4], r12d
    lea     r8, [vr_n]
    lea     rax, [rax + rax*2]
    ; nx = 0.70711a − 0.35355b + 0.61237c; ny = −0.70711a − 0.35355b + 0.61237c;
    ; nz = 0.86603b + 0.5c
    movaps  xmm3, xmm6
    mulss   xmm3, [vk_s2]
    movaps  xmm4, xmm7
    mulss   xmm4, [vk_s8]
    movaps  xmm5, xmm1
    mulss   xmm5, [vk_c6]
    subss   xmm5, xmm4                  ; −0.35355b + 0.61237c
    movaps  xmm0, xmm5
    addss   xmm0, xmm3
    movss   [r8 + rax*4], xmm0
    subss   xmm5, xmm3
    movss   [r8 + rax*4 + 4], xmm5
    movaps  xmm0, xmm7
    mulss   xmm0, [vk_c30]
    mulss   xmm1, [f_0_5]
    addss   xmm0, xmm1
    movss   [r8 + rax*4 + 8], xmm0
.sin:
    inc     esi
    jmp     .si
.sjn:
    inc     r15d
    jmp     .sj
.spn:
    add     rbx, VB_SZ
    inc     r12d
    jmp     .sp
.spd:
    ; ----- 3б) брусья и цилиндры: из каждого пикселя их рамки — луч от глаза
    mov     dword [vx_inf], 0
    xor     r12d, r12d
.xs:
    cmp     r12d, [vg_nx]
    jae     .xsd
    imul    eax, r12d, VX_SZ
    lea     rbx, [vg_shp]
    add     rbx, rax
    call    vx_corners
    movss   xmm4, [vk_big]
    movss   xmm5, [vk_mbig]
    movss   xmm6, [vk_big]
    movss   xmm7, [vk_mbig]
    xor     r13d, r13d
.xc:
    lea     eax, [r13d + r13d*2]
    movss   xmm0, [r10 + rax*4]
    movss   xmm1, [r10 + rax*4 + 4]
    movss   xmm2, [r10 + rax*4 + 8]
    movaps  xmm3, xmm0
    subss   xmm3, xmm1
    mulss   xmm3, [vk_s2]
    subss   xmm3, [vr_x0f]
    addss   xmm0, xmm1
    mulss   xmm0, [vk_s8]
    mulss   xmm2, [vk_c30]
    subss   xmm0, xmm2
    subss   xmm0, [vr_y0f]
    minss   xmm4, xmm3
    maxss   xmm5, xmm3
    minss   xmm6, xmm0
    maxss   xmm7, xmm0
    inc     r13d
    cmp     r13d, 8
    jb      .xc
    VFLOOR  r13d, xmm4, xmm0
    xor     eax, eax
    cmp     r13d, eax
    cmovl   r13d, eax
    VFLOOR  r14d, xmm5, xmm0
    mov     eax, [vr_w]
    dec     eax
    cmp     r14d, eax
    cmovg   r14d, eax
    VFLOOR  r15d, xmm6, xmm0
    xor     eax, eax
    cmp     r15d, eax
    cmovl   r15d, eax
    VFLOOR  edi, xmm7, xmm0
    mov     eax, [vr_h]
    dec     eax
    cmp     edi, eax
    cmovg   edi, eax
    ; луч в осях фигуры: o = pa·i + pb·j + pk, направление — от глаза (−T)
    xor     ecx, ecx
    lea     r11, [vx_lo]
.xk:
    imul    eax, ecx, 12
    movss   xmm0, [rbx + VX_U + rax]
    movss   xmm1, [rbx + VX_U + rax + 4]
    movss   xmm2, [rbx + VX_U + rax + 8]
    movaps  xmm3, xmm0
    subss   xmm3, xmm1
    mulss   xmm3, [vk_s2]               ; pa = R·a
    movss   [r11 + rcx*4 + (vx_pa - vx_lo)], xmm3
    addss   xmm0, xmm1
    movaps  xmm1, xmm0
    mulss   xmm0, [vk_s8]
    movaps  xmm4, xmm2
    mulss   xmm4, [vk_c30]
    subss   xmm0, xmm4                  ; pb = −Up·a
    movss   [r11 + rcx*4 + (vx_pb - vx_lo)], xmm0
    mulss   xmm1, [vk_c6]
    mulss   xmm2, [f_0_5]
    addss   xmm1, xmm2                  ; T·a
    movaps  xmm2, xmm1
    xorps   xmm2, [f_signmask]
    movss   [r11 + rcx*4 + (vx_ld - vx_lo)], xmm2
    mulss   xmm1, [vk_vk]
    movss   xmm2, [rbx]
    mulss   xmm2, [rbx + VX_U + rax]
    subss   xmm1, xmm2
    movss   xmm2, [rbx + 4]
    mulss   xmm2, [rbx + VX_U + rax + 4]
    subss   xmm1, xmm2
    movss   xmm2, [rbx + 8]
    mulss   xmm2, [rbx + VX_U + rax + 8]
    subss   xmm1, xmm2                  ; K·(T·a) − c·a
    movss   xmm2, [vr_x0f]
    addss   xmm2, [f_0_5]
    mulss   xmm2, xmm3
    addss   xmm1, xmm2
    movss   xmm2, [vr_y0f]
    addss   xmm2, [f_0_5]
    mulss   xmm2, xmm0
    addss   xmm1, xmm2
    movss   [r11 + rcx*4 + (vx_pk - vx_lo)], xmm1
    inc     ecx
    cmp     ecx, 3
    jb      .xk
.xj:
    cmp     r15d, edi
    jg      .xn
    mov     esi, r13d
.xi:
    cmp     esi, r14d
    jg      .xjn
    cvtsi2ss xmm4, esi
    cvtsi2ss xmm5, r15d
    xor     ecx, ecx
    lea     r11, [vx_lo]
.xo:
    movss   xmm0, [r11 + rcx*4 + (vx_pa - vx_lo)]
    mulss   xmm0, xmm4
    movss   xmm1, [r11 + rcx*4 + (vx_pb - vx_lo)]
    mulss   xmm1, xmm5
    addss   xmm0, xmm1
    addss   xmm0, [r11 + rcx*4 + (vx_pk - vx_lo)]
    movss   [r11 + rcx*4], xmm0
    inc     ecx
    cmp     ecx, 3
    jb      .xo
    call    vx_hit
    test    eax, eax
    jz      .xin
    movss   xmm1, [vk_vk]
    subss   xmm1, xmm0                  ; глубина
    ; под снегом не видно
    cvtsi2ss xmm3, r15d
    addss   xmm3, [f_0_5]
    addss   xmm3, [vr_y0f]
    mulss   xmm3, [vk_c30]
    movaps  xmm2, xmm1
    mulss   xmm2, [f_0_5]
    subss   xmm2, xmm3
    comiss  xmm2, [vk_m0_2]
    jb      .xin
    mov     eax, r15d
    imul    eax, [vr_w]
    add     eax, esi
    lea     r8, [vr_d]
    comiss  xmm1, [r8 + rax*4]
    jbe     .xin
    movss   [r8 + rax*4], xmm1
    lea     r8, [vr_b]
    lea     ecx, [r12d + VXID]
    mov     [r8 + rax*4], ecx
    lea     r9, [rax + rax*2]
    ; точка: X·R − Y·Up + глубина·T
    cvtsi2ss xmm2, esi
    addss   xmm2, [f_0_5]
    addss   xmm2, [vr_x0f]
    mulss   xmm2, [vk_s2]               ; X·0.707
    cvtsi2ss xmm3, r15d
    addss   xmm3, [f_0_5]
    addss   xmm3, [vr_y0f]              ; Y
    movaps  xmm4, xmm3
    mulss   xmm4, [vk_s8]
    movaps  xmm5, xmm1
    mulss   xmm5, [vk_c6]
    addss   xmm4, xmm5                  ; Y·0.354 + d·0.612
    lea     r8, [vr_sp]
    movaps  xmm0, xmm4
    addss   xmm0, xmm2
    movss   [r8 + r9*4], xmm0
    subss   xmm4, xmm2
    movss   [r8 + r9*4 + 4], xmm4
    mulss   xmm3, [vk_c30]
    mulss   xmm1, [f_0_5]
    subss   xmm1, xmm3
    movss   [r8 + r9*4 + 8], xmm1
    call    vx_world_n
    lea     r8, [vr_n]
    movss   xmm0, [vx_n]
    movss   [r8 + r9*4], xmm0
    movss   xmm0, [vx_n + 4]
    movss   [r8 + r9*4 + 4], xmm0
    movss   xmm0, [vx_n + 8]
    movss   [r8 + r9*4 + 8], xmm0
.xin:
    inc     esi
    jmp     .xi
.xjn:
    inc     r15d
    jmp     .xj
.xn:
    inc     r12d
    jmp     .xs
.xsd:
    ; ----- 4) «что сверху» (снег лежит только там, где над точкой ничего): макушки шариков
    lea     rdi, [vt_top]
    mov     ecx, VTN*VTN
    mov     eax, __float32__(-1.0e30)
    rep     stosd
    lea     rbx, [vg_blobs]
    mov     r12d, [vg_nb]
.t1:
    test    r12d, r12d
    jz      .t9
    mov     eax, [rbx + 20]
    cmp     al, VK_TWIG
    je      .tn
    movss   xmm3, [rbx + 12]
    movss   xmm0, [rbx]
    addss   xmm0, [vk_64]
    movaps  xmm1, xmm0
    subss   xmm0, xmm3
    addss   xmm0, xmm0
    addss   xmm1, xmm3
    addss   xmm1, xmm1
    VFLOOR  r13d, xmm0, xmm4
    VFLOOR  r14d, xmm1, xmm4
    movss   xmm0, [rbx + 4]
    addss   xmm0, [vk_64]
    movaps  xmm1, xmm0
    subss   xmm0, xmm3
    addss   xmm0, xmm0
    addss   xmm1, xmm3
    addss   xmm1, xmm1
    VFLOOR  r15d, xmm0, xmm4
    VFLOOR  edi, xmm1, xmm4
    xor     eax, eax
    cmp     r13d, eax
    cmovl   r13d, eax
    cmp     r15d, eax
    cmovl   r15d, eax
    mov     eax, VTN - 1
    cmp     r14d, eax
    cmovg   r14d, eax
    cmp     edi, eax
    cmovg   edi, eax
    mulss   xmm3, xmm3                  ; r²
.tj:
    cmp     r15d, edi
    jg      .tn
    cvtsi2ss xmm1, r15d
    addss   xmm1, [f_0_5]
    mulss   xmm1, [f_0_5]
    subss   xmm1, [vk_64]
    subss   xmm1, [rbx + 4]
    mulss   xmm1, xmm1
    mov     esi, r13d
.ti:
    cmp     esi, r14d
    jg      .tjn
    cvtsi2ss xmm0, esi
    addss   xmm0, [f_0_5]
    mulss   xmm0, [f_0_5]
    subss   xmm0, [vk_64]
    subss   xmm0, [rbx]
    mulss   xmm0, xmm0
    addss   xmm0, xmm1
    comiss  xmm0, xmm3
    jae     .tin
    movaps  xmm2, xmm3
    subss   xmm2, xmm0
    sqrtss  xmm2, xmm2
    addss   xmm2, [rbx + 8]
    mov     eax, r15d
    shl     eax, 8
    add     eax, esi
    lea     r8, [vt_top]
    maxss   xmm2, [r8 + rax*4]
    movss   [r8 + rax*4], xmm2
.tin:
    inc     esi
    jmp     .ti
.tjn:
    inc     r15d
    jmp     .tj
.tn:
    add     rbx, VB_SZ
    dec     r12d
    jmp     .t1
.t9:
    ; брусья: луч сверху в каждую клетку под ними
    mov     dword [vx_inf], 0
    xor     r12d, r12d
.u1:
    cmp     r12d, [vg_nx]
    jae     .u9
    imul    eax, r12d, VX_SZ
    lea     rbx, [vg_shp]
    add     rbx, rax
    call    .xybox
    movaps  xmm0, xmm4
    addss   xmm0, [vk_64]
    addss   xmm0, xmm0
    VFLOOR  r13d, xmm0, xmm1
    movaps  xmm0, xmm5
    addss   xmm0, [vk_64]
    addss   xmm0, xmm0
    VFLOOR  r14d, xmm0, xmm1
    movaps  xmm0, xmm6
    addss   xmm0, [vk_64]
    addss   xmm0, xmm0
    VFLOOR  r15d, xmm0, xmm1
    movaps  xmm0, xmm7
    addss   xmm0, [vk_64]
    addss   xmm0, xmm0
    VFLOOR  edi, xmm0, xmm1
    xor     eax, eax
    cmp     r13d, eax
    cmovl   r13d, eax
    cmp     r15d, eax
    cmovl   r15d, eax
    mov     eax, VTN - 1
    cmp     r14d, eax
    cmovg   r14d, eax
    cmp     edi, eax
    cmovg   edi, eax
.uj:
    cmp     r15d, edi
    jg      .un
    mov     esi, r13d
.ui:
    cmp     esi, r14d
    jg      .ujn
    cvtsi2ss xmm0, esi
    addss   xmm0, [f_0_5]
    mulss   xmm0, [f_0_5]
    subss   xmm0, [vk_64]
    cvtsi2ss xmm1, r15d
    addss   xmm1, [f_0_5]
    mulss   xmm1, [f_0_5]
    subss   xmm1, [vk_64]
    call    vx_down
    test    eax, eax
    jz      .uin
    mov     eax, r15d
    shl     eax, 8
    add     eax, esi
    lea     r8, [vt_top]
    maxss   xmm0, [r8 + rax*4]
    movss   [r8 + rax*4], xmm0
.uin:
    inc     esi
    jmp     .ui
.ujn:
    inc     r15d
    jmp     .uj
.un:
    inc     r12d
    jmp     .u1
.u9:
    ; ----- 5) пиксели -> картинка
    mov     rax, [vra_px]
    mov     [rbp + RS_PX], rax
    mov     rax, [vra_pn]
    mov     [rbp + RS_PN], rax
    mov     rax, [vra_po]
    mov     [rbp + RS_PO], rax
    mov     eax, [vr_w]
    mov     [rbp + RS_W], eax
    mov     eax, [vr_h]
    mov     [rbp + RS_H], eax
    mov     eax, [vr_x0]
    neg     eax
    mov     [rbp + RS_AX], eax
    mov     ecx, [vr_w]
    sub     ecx, eax
    cmp     eax, ecx
    cmovl   eax, ecx
    mov     [rbp + RS_R], eax
    mov     eax, [vr_y0]
    neg     eax
    mov     [rbp + RS_AY], eax
    xor     r12d, r12d                  ; пиксель
.px:
    cmp     r12d, [vr_n_px]
    jae     .pxd
    lea     r8, [vr_b]
    movsxd  rax, dword [r8 + r12*4]
    test    eax, eax
    js      .pempty
    cmp     eax, VXID
    jae     .pshape
    mov     [vs_bi], eax
    mov     dword [vs_flat], 0
    shl     rax, 5
    lea     rbx, [vg_blobs]
    add     rbx, rax
    ; нормаль, точка поверхности S = C + r·n
    lea     r8, [vr_n]
    lea     rax, [r12 + r12*2]
    movss   xmm0, [r8 + rax*4]
    movss   xmm1, [r8 + rax*4 + 4]
    movss   xmm2, [r8 + rax*4 + 8]
    movss   [vs_nx], xmm0
    movss   [vs_ny], xmm1
    movss   [vs_nz], xmm2
    movss   xmm3, [rbx + 12]
    mulss   xmm0, xmm3
    addss   xmm0, [rbx]
    movss   [vs_sx], xmm0
    mulss   xmm1, xmm3
    addss   xmm1, [rbx + 4]
    movss   [vs_sy], xmm1
    mulss   xmm2, xmm3
    addss   xmm2, [rbx + 8]
    movss   [vs_sz], xmm2
.hsh:
    ; хеш пикселя: шум и пороги
    mov     eax, r12d
    mov     ecx, [vg_seed]
    HASH    eax, ecx
    mov     [vs_h], eax
    ; открыто ли сверху
    movss   xmm0, [vs_sx]
    addss   xmm0, [vk_64]
    addss   xmm0, xmm0
    cvttss2si eax, xmm0
    movss   xmm1, [vs_sy]
    addss   xmm1, [vk_64]
    addss   xmm1, xmm1
    cvttss2si ecx, xmm1
    xor     edx, edx
    cmp     eax, edx
    cmovl   eax, edx
    cmp     ecx, edx
    cmovl   ecx, edx
    mov     edx, VTN - 1
    cmp     eax, edx
    cmovg   eax, edx
    cmp     ecx, edx
    cmovg   ecx, edx
    shl     ecx, 8
    add     eax, ecx
    lea     r8, [vt_top]
    movss   xmm0, [r8 + rax*4]
    movss   xmm1, [vs_sz]
    addss   xmm1, [vk_0_9]
    xor     r13d, r13d                  ; 1 — открыто
    comiss  xmm0, xmm1
    ja      .cov
    inc     r13d
.cov:
    mov     ecx, [rbx + 16]             ; цвет шарика
    movzx   eax, byte [rbx + 20]
    cmp     eax, VK_NEEDLE
    je      .kn
    cmp     eax, VK_BARK
    je      .kb
    cmp     eax, VK_BIRCH
    je      .kbi
    cmp     eax, VK_SNOW
    je      .ksn
    cmp     eax, VK_WOOD
    je      .kw
    cmp     eax, VK_LOG
    je      .kl
    cmp     eax, VK_METAL
    je      .km
    cmp     eax, VK_GLOW
    je      .col
    jmp     .noise                      ; веточка, верёвка — как есть
.kw:
    ; доска: полосы волокон поперёк (по грани — та ось, что не нормаль и не длина),
    ; изредка трещина; торец темнее; снег сверху
    call    .loc
    call    .faces
    comiss  xmm2, xmm0
    jb      .kw0
    comiss  xmm2, xmm1
    jb      .kw0
    mov     r8d, 196
    call    vc_mul
    jmp     .kwsn
.kw0:
    movss   xmm3, [vs_lv]
    comiss  xmm0, xmm1
    jae     .kw1
    movss   xmm3, [vs_lu]
.kw1:
    mulss   xmm3, [vk_1_2]
    addss   xmm3, [vk_64]
    cvttss2si eax, xmm3
    imul    edx, [vs_bi], 7
    add     edx, [vg_seed]
    HASH    eax, edx
    mov     r8d, eax
    and     r8d, 47
    add     r8d, 212
    test    eax, 0x3F00
    jnz     .kw2
    mov     r8d, 164
.kw2:
    call    vc_mul
.kwsn:
    test    r13d, r13d
    jz      .noise
    movss   xmm1, [vs_nz]
    comiss  xmm1, [vk_0_6]
    ja      .snowc
    jmp     .noise
.kl:
    ; бревно: бок — кора бороздами, торец — срез в кольцах, по краю кора
    call    .loc
    call    .faces
    comiss  xmm2, [vk_0_8]
    ja      .klc
    movss   xmm0, [vs_lw]
    mulss   xmm0, [vk_0_8]
    addss   xmm0, [vk_64]
    cvttss2si eax, xmm0
    movss   xmm0, [vs_nu]
    addss   xmm0, [f_1]
    mulss   xmm0, [vk_2_4]
    cvttss2si edx, xmm0
    movss   xmm0, [vs_nv]
    addss   xmm0, [f_1]
    mulss   xmm0, [vk_2_4]
    cvttss2si r8d, xmm0
    shl     r8d, 3
    add     edx, r8d
    mov     r8d, [vs_bi]
    shl     r8d, 6
    add     edx, r8d
    HASH    eax, edx
    test    eax, 3
    jnz     .kl1
    mov     r8d, 176
    call    vc_mul
.kl1:
    test    r13d, r13d
    jz      .noise
    movss   xmm1, [vs_nz]
    comiss  xmm1, [vk_0_55]
    ja      .snowc
    jmp     .noise
.klc:
    movss   xmm0, [vs_lu]
    mulss   xmm0, xmm0
    movss   xmm1, [vs_lv]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    sqrtss  xmm0, xmm0
    divss   xmm0, [rbx + VX_HU]         ; ρ: 0 в середине, 1 — край
    comiss  xmm0, [vk_0_85]
    jbe     .klr
    mov     r8d, 210                    ; кора по краю среза
    call    vc_mul
    jmp     .kl1
.klr:
    movss   [vs_lu], xmm0
    movzx   eax, byte [rbx + 21]
    and     eax, 3
    lea     rcx, [vk_endc]
    mov     ecx, [rcx + rax*4]
    call    vr_u
    mov     r8d, eax
    and     r8d, 31
    add     r8d, 232
    call    vc_mul
    ; кольца: сдвиг — свой у полена
    mov     eax, [vs_bi]
    mov     edx, [vg_seed]
    HASH    eax, edx
    and     eax, 7
    cvtsi2ss xmm1, eax
    mulss   xmm1, [vk_0_12]
    movss   xmm0, [vs_lu]
    mulss   xmm0, [vk_4_2]
    addss   xmm0, xmm1
    cvttss2si eax, xmm0
    mov     r8d, 250
    test    eax, 1
    jz      .klq
    mov     r8d, 222
.klq:
    movss   xmm0, [vs_lu]
    comiss  xmm0, [vk_0_15]
    jae     .klk
    mov     r8d, 196                    ; сердцевина
.klk:
    call    vc_mul
    jmp     .kl1
.km:
    ; железо, бетон: ровно, снег сверху
    test    r13d, r13d
    jz      .kmn
    movss   xmm1, [vs_nz]
    comiss  xmm1, [vk_0_6]
    ja      .snowc
.kmn:
    mov     eax, [vs_h]
    and     eax, 15
    add     eax, 248
    mov     r8d, eax
    call    vc_mul
    jmp     .col
.kn:
    ; хвоя: снег, если открыто и смотрит вверх (порог — по нагрузке, разброс — у шарика
    ; целиком: снег комьями, а не крапом)
    test    r13d, r13d
    jz      .noise
    mov     eax, [vs_bi]
    mov     edx, [vg_seed]
    HASH    eax, edx
    shr     eax, 8
    and     eax, 63
    cvtsi2ss xmm0, eax
    mulss   xmm0, [vk_i256]
    addss   xmm0, [vg_sthr]
    subss   xmm0, [vk_0_12]
    movss   xmm1, [vs_nz]
    comiss  xmm1, xmm0
    jbe     .noise
    jmp     .snowc
.kb:
    ; кора: борозды (по высоте и столбцу), снег на верхних боках
    mov     eax, r12d
    xor     edx, edx
    div     dword [vr_w]
    movss   xmm0, [vs_sz]
    mulss   xmm0, [vk_1_4]
    cvttss2si eax, xmm0
    HASH    edx, eax
    test    edx, 3
    jnz     .kb1
    mov     r8d, 184
    call    vc_mul
.kb1:
    test    r13d, r13d
    jz      .noise
    movss   xmm1, [vs_nz]
    comiss  xmm1, [vk_0_55]
    ja      .snowc
    jmp     .noise
.kbi:
    ; берёста: низ чёрный в трещинах, чёрточки, снег на верхних боках
    movss   xmm0, [vs_sz]
    comiss  xmm0, [vg_bdz]
    jae     .bi1
    mov     eax, [vs_h]
    test    eax, 7
    jz      .bi1
    mov     ecx, 0x322C2A
    jmp     .noise
.bi1:
    mov     eax, [rbx + 20]
    shr     eax, 8
    and     eax, 255
    jz      .bi2
    cvtsi2ss xmm3, eax
    mulss   xmm3, [vk_0_01]             ; порог
    movss   xmm0, [vs_nx]
    mulss   xmm0, [rbx + 24]
    movss   xmm1, [vs_ny]
    mulss   xmm1, [rbx + 28]
    addss   xmm0, xmm1
    movss   xmm1, [vs_nx]
    mulss   xmm1, xmm1
    movss   xmm2, [vs_ny]
    mulss   xmm2, xmm2
    addss   xmm1, xmm2
    addss   xmm1, [vk_1em4]
    sqrtss  xmm1, xmm1
    divss   xmm0, xmm1
    comiss  xmm0, xmm3
    jbe     .bi2
    mov     ecx, 0x2C2826
    jmp     .noise
.bi2:
    test    r13d, r13d
    jz      .noise
    movss   xmm1, [vs_nz]
    comiss  xmm1, [vk_0_6]
    ja      .snowc
    jmp     .noise
.ksn:
    ; снег: снизу подушки — голубее
    mov     ecx, VC_SNOW
    movss   xmm1, [vs_nz]
    comiss  xmm1, [vk_m0_2]
    jae     .snn
    mov     ecx, 0xC4CEDC
.snn:
    mov     eax, [vs_h]
    and     eax, 15
    add     eax, 244
    mov     r8d, eax
    call    vc_mul
    jmp     .col
.snowc:
    mov     ecx, 0xD8E0EA
    mov     eax, [vs_h]
    and     eax, 15
    add     eax, 244
    mov     r8d, eax
    call    vc_mul
    jmp     .col
.noise:
    mov     eax, [vs_h]
    and     eax, 31
    add     eax, 240
    mov     r8d, eax
    call    vc_mul
.col:
    or      ecx, 0x01000000
    mov     rax, [rbp + RS_PX]
    mov     [rax + r12*4], ecx
    ; нормаль в сферу, с дрожью (переходы полутонами, без полос); грань бруса — ровно
    cmp     dword [vs_flat], 0
    je      .njit
    movss   xmm0, [vs_nx]
    movss   xmm1, [vs_ny]
    movss   xmm2, [vs_nz]
    jmp     .nq
.njit:
    mov     eax, [vs_h]
    shr     eax, 14
    and     eax, 31
    cvtsi2ss xmm0, eax
    mulss   xmm0, [vk_0_006]
    subss   xmm0, [vk_0_09]
    addss   xmm0, [vs_nx]
    mov     eax, [vs_h]
    shr     eax, 19
    and     eax, 31
    cvtsi2ss xmm1, eax
    mulss   xmm1, [vk_0_006]
    subss   xmm1, [vk_0_09]
    addss   xmm1, [vs_ny]
    mov     eax, [vs_h]
    shr     eax, 24
    and     eax, 31
    cvtsi2ss xmm2, eax
    mulss   xmm2, [vk_0_006]
    subss   xmm2, [vk_0_09]
    addss   xmm2, [vs_nz]
.nq:
    call    nq_id
    ; почти вверх — своя «вверх» сферы
    movss   xmm0, [vs_nz]
    comiss  xmm0, [vk_0_96]
    jbe     .nid
    mov     eax, N_SUP
.nid:
    mov     rcx, [rbp + RS_PN]
    mov     [rcx + r12], al
    ; точка в мире: U -> 1/256 тайла
    movss   xmm0, [vs_sx]
    mulss   xmm0, [vk_u2t]
    cvtss2si eax, xmm0
    movss   xmm1, [vs_sy]
    mulss   xmm1, [vk_u2t]
    cvtss2si ecx, xmm1
    and     eax, 0xFFFF
    shl     ecx, 16
    or      eax, ecx
    mov     rcx, [rbp + RS_PO]
    mov     [rcx + r12*4], eax
    jmp     .pxn
.pempty:
    mov     rax, [rbp + RS_PX]
    mov     dword [rax + r12*4], 0
    mov     rax, [rbp + RS_PN]
    mov     byte [rax + r12], 0
    mov     rax, [rbp + RS_PO]
    mov     dword [rax + r12*4], 0
    jmp     .pxn
.pshape:
    ; брус, цилиндр: нормаль и точка — со съёмки
    sub     eax, VXID
    mov     [vs_bi], eax
    imul    eax, eax, VX_SZ
    lea     rbx, [vg_shp]
    add     rbx, rax
    lea     rax, [r12 + r12*2]
    lea     r8, [vr_n]
    movss   xmm0, [r8 + rax*4]
    movss   [vs_nx], xmm0
    movss   xmm0, [r8 + rax*4 + 4]
    movss   [vs_ny], xmm0
    movss   xmm0, [r8 + rax*4 + 8]
    movss   [vs_nz], xmm0
    lea     r8, [vr_sp]
    movss   xmm0, [r8 + rax*4]
    movss   [vs_sx], xmm0
    movss   xmm0, [r8 + rax*4 + 4]
    movss   [vs_sy], xmm0
    movss   xmm0, [r8 + rax*4 + 8]
    movss   [vs_sz], xmm0
    ; грань — ровно; бок цилиндра — с дрожью, как шарик
    mov     dword [vs_flat], 1
    cmp     dword [rbx + VX_TYPE], 2
    jne     .hsh
    call    .faces
    comiss  xmm2, [vk_0_8]
    ja      .hsh
    mov     dword [vs_flat], 0
    jmp     .hsh
.pxn:
    inc     r12d
    jmp     .px
.pxd:
    mov     eax, [vr_n_px]
    shl     rax, 2
    add     [vra_px], rax
    add     [vra_po], rax
    shr     rax, 2
    add     [vra_pn], rax
    ; ----- 6) карта высот: сетка 1/32 тайла (0.70711 U), основание в середине
    xorps   xmm8, xmm8                  ; наибольшее |x|, |y| + r
    lea     rbx, [vg_blobs]
    mov     r12d, [vg_nb]
.h1:
    test    r12d, r12d
    jz      .h2
    movss   xmm0, [rbx]
    andps   xmm0, [f_absmask]
    movss   xmm1, [rbx + 4]
    andps   xmm1, [f_absmask]
    maxss   xmm0, xmm1
    addss   xmm0, [rbx + 12]
    maxss   xmm8, xmm0
    add     rbx, VB_SZ
    dec     r12d
    jmp     .h1
.h2:
    ; брусья — по углам
    xor     r12d, r12d
.hx:
    cmp     r12d, [vg_nx]
    jae     .h2b
    imul    eax, r12d, VX_SZ
    lea     rbx, [vg_shp]
    add     rbx, rax
    call    .xybox
    andps   xmm4, [f_absmask]
    andps   xmm5, [f_absmask]
    andps   xmm6, [f_absmask]
    andps   xmm7, [f_absmask]
    maxss   xmm4, xmm5
    maxss   xmm6, xmm7
    maxss   xmm4, xmm6
    addss   xmm4, [f_0_5]
    maxss   xmm8, xmm4
    inc     r12d
    jmp     .hx
.h2b:
    mulss   xmm8, [vk_s2i]              ; / 0.70711
    cvttss2si eax, xmm8
    inc     eax
    mov     ecx, (HSMAX - 1) / 2
    cmp     eax, ecx
    cmova   eax, ecx
    mov     [vs_hc], eax                ; середина
    lea     eax, [eax*2 + 1]
    mov     [rbp + RS_HS], eax
    imul    eax, eax
    mov     [vs_hn], eax
    mov     rdi, [vra_hm]
    mov     [rbp + RS_HM], rdi
    mov     ecx, eax
    xor     eax, eax
    rep     stosb
    mov     eax, [vs_hn]
    add     [vra_hm], rax
    cvtsi2ss xmm9, dword [vs_hc]
    lea     rbx, [vg_blobs]
    mov     r12d, [vg_nb]
.h3:
    test    r12d, r12d
    jz      .h9
    cmp     byte [rbx + 20], VK_PLAIN
    je      .hn                         ; верёвка, уголь, изолятор — тени тоньше пикселя
    movss   xmm3, [rbx + 12]
    ; клетки k: (k − c)·0.70711 в [x − r, x + r]
    movss   xmm0, [rbx]
    movaps  xmm1, xmm0
    subss   xmm0, xmm3
    addss   xmm1, xmm3
    mulss   xmm0, [vk_s2i]
    mulss   xmm1, [vk_s2i]
    addss   xmm0, xmm9
    addss   xmm1, xmm9
    VFLOOR  r13d, xmm0, xmm4
    inc     r13d
    VFLOOR  r14d, xmm1, xmm4
    movss   xmm0, [rbx + 4]
    movaps  xmm1, xmm0
    subss   xmm0, xmm3
    addss   xmm1, xmm3
    mulss   xmm0, [vk_s2i]
    mulss   xmm1, [vk_s2i]
    addss   xmm0, xmm9
    addss   xmm1, xmm9
    VFLOOR  r15d, xmm0, xmm4
    inc     r15d
    VFLOOR  edi, xmm1, xmm4
    xor     eax, eax
    cmp     r13d, eax
    cmovl   r13d, eax
    cmp     r15d, eax
    cmovl   r15d, eax
    mov     eax, [rbp + RS_HS]
    dec     eax
    cmp     r14d, eax
    cmovg   r14d, eax
    cmp     edi, eax
    cmovg   edi, eax
    mulss   xmm3, xmm3
.hj:
    cmp     r15d, edi
    jg      .hn
    cvtsi2ss xmm1, r15d
    subss   xmm1, xmm9
    mulss   xmm1, [vk_s2]
    subss   xmm1, [rbx + 4]
    mulss   xmm1, xmm1
    mov     esi, r13d
.hi:
    cmp     esi, r14d
    jg      .hjn
    cvtsi2ss xmm0, esi
    subss   xmm0, xmm9
    mulss   xmm0, [vk_s2]
    subss   xmm0, [rbx]
    mulss   xmm0, xmm0
    addss   xmm0, xmm1
    comiss  xmm0, xmm3
    jae     .hin
    movaps  xmm2, xmm3
    subss   xmm2, xmm0
    sqrtss  xmm2, xmm2
    addss   xmm2, [rbx + 8]
    mulss   xmm2, [vk_c30]              ; px
    cvtss2si eax, xmm2
    test    eax, eax
    jle     .hin
    mov     ecx, 127
    cmp     eax, ecx
    cmova   eax, ecx
    mov     ecx, r15d
    imul    ecx, [rbp + RS_HS]
    add     ecx, esi
    mov     r8, [rbp + RS_HM]
    cmp     al, [r8 + rcx]
    jbe     .hin
    mov     [r8 + rcx], al
.hin:
    inc     esi
    jmp     .hi
.hjn:
    inc     r15d
    jmp     .hj
.hn:
    add     rbx, VB_SZ
    dec     r12d
    jmp     .h3
.h9:
    ; брусья: луч сверху в каждую клетку (фигура чуть раздута — тонкая доска не
    ; проскочит между клетками)
    mov     dword [vx_inf], __float32__(0.36)
    xor     r12d, r12d
.g1:
    cmp     r12d, [vg_nx]
    jae     .g9
    imul    eax, r12d, VX_SZ
    lea     rbx, [vg_shp]
    add     rbx, rax
    cmp     byte [rbx + 20], VK_PLAIN
    je      .gn                         ; верёвка: иначе в карте высот — стенка на столбах
    call    .xybox
    subss   xmm4, [vk_0_4]
    addss   xmm5, [vk_0_4]
    subss   xmm6, [vk_0_4]
    addss   xmm7, [vk_0_4]
    mulss   xmm4, [vk_s2i]
    addss   xmm4, xmm9
    VFLOOR  r13d, xmm4, xmm0
    mulss   xmm5, [vk_s2i]
    addss   xmm5, xmm9
    VFLOOR  r14d, xmm5, xmm0
    inc     r14d
    mulss   xmm6, [vk_s2i]
    addss   xmm6, xmm9
    VFLOOR  r15d, xmm6, xmm0
    mulss   xmm7, [vk_s2i]
    addss   xmm7, xmm9
    VFLOOR  edi, xmm7, xmm0
    inc     edi
    xor     eax, eax
    cmp     r13d, eax
    cmovl   r13d, eax
    cmp     r15d, eax
    cmovl   r15d, eax
    mov     eax, [rbp + RS_HS]
    dec     eax
    cmp     r14d, eax
    cmovg   r14d, eax
    cmp     edi, eax
    cmovg   edi, eax
.gj:
    cmp     r15d, edi
    jg      .gn
    mov     esi, r13d
.gi:
    cmp     esi, r14d
    jg      .gjn
    cvtsi2ss xmm0, esi
    subss   xmm0, xmm9
    mulss   xmm0, [vk_s2]
    cvtsi2ss xmm1, r15d
    subss   xmm1, xmm9
    mulss   xmm1, [vk_s2]
    call    vx_down
    test    eax, eax
    jz      .gin
    mulss   xmm0, [vk_c30]              ; px
    cvtss2si eax, xmm0
    test    eax, eax
    jle     .gin
    mov     ecx, 127
    cmp     eax, ecx
    cmova   eax, ecx
    mov     ecx, r15d
    imul    ecx, [rbp + RS_HS]
    add     ecx, esi
    mov     r8, [rbp + RS_HM]
    cmp     al, [r8 + rcx]
    jbe     .gin
    mov     [r8 + rcx], al
.gin:
    inc     esi
    jmp     .gi
.gjn:
    inc     r15d
    jmp     .gj
.gn:
    inc     r12d
    jmp     .g1
.g9:
    mov     dword [vx_inf], 0
    add     rsp, 40
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
; rbx = фигура -> xmm4, xmm5 = x от..до, xmm6, xmm7 = y от..до (портит rax, rcx, xmm0-xmm3)
.xybox:
    call    vx_corners
    movss   xmm4, [vk_big]
    movss   xmm5, [vk_mbig]
    movss   xmm6, [vk_big]
    movss   xmm7, [vk_mbig]
    xor     eax, eax
.xy1:
    lea     ecx, [eax + eax*2]
    movss   xmm0, [r10 + rcx*4]
    movss   xmm1, [r10 + rcx*4 + 4]
    minss   xmm4, xmm0
    maxss   xmm5, xmm0
    minss   xmm6, xmm1
    maxss   xmm7, xmm1
    inc     eax
    cmp     eax, 8
    jb      .xy1
    ret
; rbx = фигура, vs_s* -> vs_lu, vs_lv, vs_lw — точка в осях фигуры (портит xmm0-xmm2)
.loc:
    movss   xmm0, [vs_sx]
    subss   xmm0, [rbx]
    movss   xmm1, [vs_sy]
    subss   xmm1, [rbx + 4]
    movss   xmm2, [vs_sz]
    subss   xmm2, [rbx + 8]
    movss   [vs_lu], xmm0
    movss   [vs_lv], xmm1
    movss   [vs_lw], xmm2
    push    rax
    lea     rax, [rbx + VX_U]
    call    .dotl
    movss   [vs_tu], xmm0
    lea     rax, [rbx + VX_V]
    call    .dotl
    movss   [vs_tv], xmm0
    lea     rax, [rbx + VX_W]
    call    .dotl
    movss   [vs_lw], xmm0
    movss   xmm0, [vs_tu]
    movss   [vs_lu], xmm0
    movss   xmm0, [vs_tv]
    movss   [vs_lv], xmm0
    pop     rax
    ret
.dotl:
    movss   xmm0, [vs_lu]
    mulss   xmm0, [rax]
    movss   xmm1, [vs_lv]
    mulss   xmm1, [rax + 4]
    addss   xmm0, xmm1
    movss   xmm1, [vs_lw]
    mulss   xmm1, [rax + 8]
    addss   xmm0, xmm1
    ret
; rbx = фигура, vs_n* -> vs_nu, vs_nv (n·u, n·v); xmm0, xmm1, xmm2 = |n·u|, |n·v|, |n·w|
.faces:
    push    rax
    lea     rax, [rbx + VX_U]
    call    .dotn
    movss   [vs_nu], xmm0
    lea     rax, [rbx + VX_V]
    call    .dotn
    movss   [vs_nv], xmm0
    lea     rax, [rbx + VX_W]
    call    .dotn
    movaps  xmm2, xmm0
    andps   xmm2, [f_absmask]
    movss   xmm0, [vs_nu]
    andps   xmm0, [f_absmask]
    movss   xmm1, [vs_nv]
    andps   xmm1, [f_absmask]
    pop     rax
    ret
.dotn:
    movss   xmm0, [vs_nx]
    mulss   xmm0, [rax]
    movss   xmm1, [vs_ny]
    mulss   xmm1, [rax + 4]
    addss   xmm0, xmm1
    movss   xmm1, [vs_nz]
    mulss   xmm1, [rax + 8]
    addss   xmm0, xmm1
    ret

; ---------------------------------------------------------------- все варианты
veg_init:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40
    call    nq_init
    lea     rax, [va_px]
    mov     [vra_px], rax
    lea     rax, [va_pn]
    mov     [vra_pn], rax
    lea     rax, [va_po]
    mov     [vra_po], rax
    lea     rax, [va_hm]
    mov     [vra_hm], rax
    xor     r12d, r12d
.v:
    imul    eax, r12d, 16
    lea     rbx, [vv_def]
    add     rbx, rax
    ; место в аренах: худший случай — кадр целиком
    mov     rax, [vra_px]
    lea     rcx, [va_px + (VA_PX - VRW*VRH)*4]
    cmp     rax, rcx
    ja      .skip
    mov     rax, [vra_hm]
    lea     rcx, [va_hm + VA_HM - HSMAX*HSMAX]
    cmp     rax, rcx
    ja      .skip
    mov     eax, [rbx + 12]
    imul    eax, eax, 0x9E3779B1
    or      eax, 1
    mov     [vg_rng], eax
    mov     [vg_seed], eax
    mov     dword [vg_nb], 0
    mov     dword [vg_nx], 0
    mov     dword [vg_young], 0
    cvtsi2ss xmm0, dword [rbx + 4]
    mulss   xmm0, [vk_ic30]
    movss   [vg_H], xmm0
    movzx   eax, word [rbx + 8]
    cvtsi2ss xmm0, eax
    mulss   xmm0, [vk_0_01]
    movss   [vg_snow], xmm0
    ; порог снега на хвое по нормали: 0.72 − 0.62·нагрузка
    mulss   xmm0, [vk_0_62]
    movss   xmm1, [vk_0_72]
    subss   xmm1, xmm0
    movss   [vg_sthr], xmm1
    movzx   eax, word [rbx + 10]
    mov     [vg_sub], eax
    ; хвоя ели — одна из четырёх
    mov     eax, r12d
    and     eax, 3
    lea     rcx, [vg_npal]
    mov     eax, [rcx + rax*4]
    mov     [vg_ncol], eax
    mov     eax, [rbx]
    cmp     eax, SP_SPRUCE
    je      .sp
    cmp     eax, SP_YOUNG
    je      .yo
    cmp     eax, SP_PINE
    je      .pi
    cmp     eax, SP_BIRCH
    je      .bi
    cmp     eax, SP_BUSH
    je      .bu
    call    vg_stump
    jmp     .gen
.yo:
    mov     dword [vg_young], 1
.sp:
    call    vg_spruce
    jmp     .gen
.pi:
    call    vg_pine
    jmp     .gen
.bi:
    call    vg_birch
    jmp     .gen
.bu:
    call    vg_bush
.gen:
    imul    eax, r12d, RS_SZ
    lea     rsi, [vrs]
    add     rsi, rax
    call    vg_render
    ; качание у макушки: высота·k породы / 100
    mov     eax, [rbx]
    lea     rcx, [vg_amp]
    movzx   eax, byte [rcx + rax]
    imul    eax, [rbx + 4]
    xor     edx, edx
    mov     ecx, 100
    div     ecx
    mov     [rsi + RS_AMP], eax
.skip:
    inc     r12d
    cmp     r12d, NVAR
    jb      .v
    call    prop_init                   ; двор — туда же, после растений
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- по карте
; eax = ix, ecx = iy, edi = соль -> xmm0 = значение узла 0..1 (портит eax, ecx)
vn_h:
    xor     eax, edi
    HASH    eax, ecx
    shr     eax, 8
    and     eax, 0xFFFF
    cvtsi2ss xmm0, eax
    mulss   xmm0, [vk_i65536]
    ret

; ecx = x, edx = y, r8d = клетка (тайлы), r9d = соль -> xmm0: плавный шум 0..1
; (портит eax, ecx, edx, r8-r11, xmm1-xmm5)
vg_vnoise:
    push    rbx
    push    rdi
    mov     ebx, r8d
    mov     edi, r9d
    mov     r11d, edx
    mov     eax, ecx
    xor     edx, edx
    div     ebx
    mov     r10d, eax                   ; ix
    cvtsi2ss xmm4, edx
    mov     eax, r11d
    xor     edx, edx
    div     ebx
    mov     r11d, eax                   ; iy
    cvtsi2ss xmm5, edx
    cvtsi2ss xmm1, ebx
    divss   xmm4, xmm1
    divss   xmm5, xmm1
    ; сглаживание t²(3 − 2t)
    movss   xmm1, [f_3]
    subss   xmm1, xmm4
    subss   xmm1, xmm4
    mulss   xmm1, xmm4
    mulss   xmm4, xmm1
    movss   xmm1, [f_3]
    subss   xmm1, xmm5
    subss   xmm1, xmm5
    mulss   xmm1, xmm5
    mulss   xmm5, xmm1
    mov     eax, r10d
    mov     ecx, r11d
    call    vn_h
    movss   xmm2, xmm0                  ; v00
    lea     eax, [r10d + 1]
    mov     ecx, r11d
    call    vn_h
    subss   xmm0, xmm2
    mulss   xmm0, xmm4
    addss   xmm2, xmm0                  ; верхний ряд
    mov     eax, r10d
    lea     ecx, [r11d + 1]
    call    vn_h
    movss   xmm3, xmm0                  ; v01
    lea     eax, [r10d + 1]
    lea     ecx, [r11d + 1]
    call    vn_h
    subss   xmm0, xmm3
    mulss   xmm0, xmm4
    addss   xmm3, xmm0                  ; нижний
    subss   xmm3, xmm2
    mulss   xmm3, xmm5
    addss   xmm2, xmm3
    movaps  xmm0, xmm2
    pop     rdi
    pop     rbx
    ret

; place_trees: eax = вид дерева по старому броску (ель, берёза), ebx = x, esi = y ->
; eax — рощи сосен там, где шум выше 0.6 (главный ГСЧ не трогает)
veg_species:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40
    mov     r12d, eax
    mov     ecx, ebx
    mov     edx, esi
    mov     r8d, 9
    mov     r9d, 0x51
    call    vg_vnoise
    comiss  xmm0, [vk_0_6]
    jbe     .keep
    mov     r12d, T_PINE
.keep:
    mov     eax, r12d
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; после place_trees: подрост у опушки, ивняк, бурьян у заборов, пни — хешем места
; (главный ГСЧ не трогает)
veg_extra:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 32
    xor     esi, esi
.y:
    xor     ebx, ebx
.x:
    mov     ecx, ebx
    mov     edx, esi
    call    get_tile
    test    eax, eax
    jnz     .n
    ; не у старта
    lea     eax, [ebx - 64 + 5]
    cmp     eax, 10
    jae     .far
    lea     eax, [esi - 61 + 5]
    cmp     eax, 10
    jb      .n
.far:
    ; до края карты
    mov     edi, ebx
    cmp     esi, edi
    cmovl   edi, esi
    mov     eax, 127
    sub     eax, ebx
    cmp     eax, edi
    cmovl   edi, eax
    mov     eax, 127
    sub     eax, esi
    cmp     eax, edi
    cmovl   edi, eax
    ; деревья в 5x5, забор или изба рядом
    xor     r12d, r12d
    xor     r13d, r13d
    mov     r14d, -2
.ny:
    mov     r15d, -2
.nx:
    lea     ecx, [ebx + r15d]
    lea     edx, [esi + r14d]
    call    get_tile
    cmp     eax, T_SPRUCE
    je      .tr
    cmp     eax, T_BIRCH
    je      .tr
    cmp     eax, T_PINE
    jne     .nt
.tr:
    inc     r12d
.nt:
    mov     ecx, r15d
    add     ecx, 1
    cmp     ecx, 2
    ja      .nn
    mov     ecx, r14d
    add     ecx, 1
    cmp     ecx, 2
    ja      .nn
    cmp     eax, T_FENCE
    je      .fe
    cmp     eax, T_WALL
    je      .fe
    cmp     eax, T_WIN
    je      .fe
    cmp     eax, T_WINL
    jne     .nn
.fe:
    inc     r13d
.nn:
    inc     r15d
    cmp     r15d, 2
    jle     .nx
    inc     r14d
    cmp     r14d, 2
    jle     .ny
    ; бросок места
    lea     eax, [ebx + 911]
    lea     ecx, [esi + 353]
    HASH    eax, ecx
    and     eax, 0xFFFF
    mov     r15d, eax                   ; 0..65535
    ; подрост: у опушки, где рядом хотя бы два дерева — 11 %
    cmp     edi, 2
    jb      .b
    cmp     edi, 12
    ja      .b
    cmp     r12d, 2
    jb      .b
    cmp     r15d, 7200
    jae     .b
    mov     ecx, ebx
    mov     edx, esi
    call    snow_cross4
    test    eax, eax
    jz      .b
    mov     eax, T_YOUNG
    jmp     .set
.b:
    ; ивняк у опушки
    cmp     edi, 4
    jb      .w
    cmp     edi, 13
    ja      .w
    cmp     r15d, 7200
    jb      .w
    cmp     r15d, 9800
    jae     .w
    mov     eax, T_BUSH
    jmp     .set
.w:
    ; бурьян у заборов и изб
    test    r13d, r13d
    jz      .s
    cmp     r15d, 9800
    jb      .s
    cmp     r15d, 13400
    jae     .s
    mov     eax, T_BUSH
    jmp     .set
.s:
    ; пни у опушки (дрова рубят с краю)
    cmp     edi, 3
    jb      .n
    cmp     edi, 12
    ja      .n
    cmp     r15d, 13400
    jb      .n
    cmp     r15d, 14500
    jae     .n
    mov     ecx, ebx
    mov     edx, esi
    call    snow_cross4
    test    eax, eax
    jz      .n
    mov     eax, T_STUMP
.set:
    mov     ecx, ebx
    mov     edx, esi
    call    set_tile
.n:
    inc     ebx
    cmp     ebx, MAPW
    jb      .x
    inc     esi
    cmp     esi, MAPW
    jb      .y
    add     rsp, 32
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; vmap: у каждого растения — вариант породы, сдвиг основания (3x3), зеркало — хешем
; места. Ивняк или бурьян: у забора и избы — бурьян
veg_assign:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40
    xor     r12d, r12d
.t:
    lea     rdx, [tmap]
    movzx   eax, byte [rdx + r12]
    lea     rdx, [tsp]
    movzx   ebx, byte [rdx + rax]       ; порода
    xor     edi, edi
    cmp     ebx, 0xFF
    je      .put
    mov     eax, r12d
    and     eax, MAPW - 1
    imul    eax, eax, 7
    add     eax, 13
    mov     ecx, r12d
    shr     ecx, 7
    imul    ecx, ecx, 11
    add     ecx, 5
    HASH    eax, ecx
    mov     esi, eax                    ; хеш
    lea     rdx, [vsp_n]
    movzx   ecx, byte [rdx + rbx]
    mov     eax, esi
    shr     eax, 4
    and     eax, 0xFFFF
    cmp     ebx, SP_BUSH
    jne     .k
    ; куст: половина вариантов — ивняк, половина — бурьян
    shr     ecx, 1
    imul    eax, ecx
    shr     eax, 16
    call    .nearfence
    jz      .kk
    add     eax, ecx
    jmp     .kk
.k:
    imul    eax, ecx
    shr     eax, 16
.kk:
    lea     rdx, [vsp_first]
    movzx   ecx, byte [rdx + rbx]
    lea     edi, [eax + ecx + 1]
    ; сдвиг основания 0..8 и зеркало
    mov     eax, esi
    shr     eax, 20
    and     eax, 0xFF
    imul    eax, eax, 9
    shr     eax, 8
    mov     ecx, esi
    shr     ecx, 28
    and     ecx, 1
    shl     ecx, 7
    or      eax, ecx
    shl     eax, 8
    or      edi, eax
.put:
    lea     rdx, [vmap]
    mov     [rdx + r12*2], di
    inc     r12d
    cmp     r12d, MAPW*MAPW
    jb      .t
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
; r12d = тайл -> ZF = 0, если в 3x3 забор или сруб (портит r8-r11, сохраняет eax, ecx)
.nearfence:
    push    rax
    push    rcx
    mov     r8d, r12d
    and     r8d, MAPW - 1
    mov     r9d, r12d
    shr     r9d, 7
    xor     r11d, r11d
    mov     r10d, -1
.nf1:
    mov     eax, -1
.nf2:
    lea     ecx, [r8d + eax]
    cmp     ecx, MAPW
    jae     .nf3
    lea     edx, [r9d + r10d]
    cmp     edx, MAPW
    jae     .nf3
    shl     edx, 7
    add     edx, ecx
    lea     rcx, [tmap]
    movzx   edx, byte [rcx + rdx]
    cmp     edx, T_FENCE
    je      .nfy
    cmp     edx, T_WALL
    je      .nfy
    cmp     edx, T_WIN
    je      .nfy
    cmp     edx, T_WINL
    je      .nfy
.nf3:
    inc     eax
    cmp     eax, 1
    jle     .nf2
    inc     r10d
    cmp     r10d, 1
    jle     .nf1
    jmp     .nf9
.nfy:
    inc     r11d
.nf9:
    test    r11d, r11d
    pop     rcx
    pop     rax
    ret

; ecx = tx, edx = ty -> eax = основание растения, мир 1/256 тайла: x (младшее слово),
; y (старшее) (портит r8, r9)
veg_basei:
    mov     eax, edx
    shl     eax, 7
    add     eax, ecx
    lea     r8, [vmap]
    movzx   eax, word [r8 + rax*2]
    shr     eax, 8
    and     eax, 15
    mov     r9d, eax
    imul    r9d, r9d, 11
    shr     r9d, 5                      ; jy = j / 3
    lea     r8d, [r9d + r9d*2]
    sub     eax, r8d                    ; jx
    imul    eax, eax, VJ_OFF
    add     eax, 154 - VJ_OFF
    mov     r8d, ecx
    shl     r8d, 8
    add     eax, r8d
    imul    r9d, r9d, VJ_OFF
    add     r9d, 154 - VJ_OFF
    mov     r8d, edx
    shl     r8d, 8
    add     r9d, r8d
    shl     r9d, 16
    or      eax, r9d
    ret
; то же -> xmm0, xmm1 (тайлы) (портит eax, r8, r9)
veg_base:
    call    veg_basei
    movzx   r8d, ax
    cvtsi2ss xmm0, r8d
    mulss   xmm0, [vk_i256]
    shr     eax, 16
    cvtsi2ss xmm1, eax
    mulss   xmm1, [vk_i256]
    ret

; конец calc_hmap: растения — своими картами высот (с модели), наибольшее
; из того, что уже в ячейке (крона свешивается на соседние тайлы)
veg_hmap:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    xor     r12d, r12d                  ; тайл
.t:
    lea     rdx, [vmap]
    movzx   eax, word [rdx + r12*2]
    test    al, al
    jz      .tn
    mov     [vh_fl], eax
    movzx   eax, al
    dec     eax
    imul    eax, eax, RS_SZ
    lea     rbx, [vrs]
    add     rbx, rax
    mov     ecx, r12d
    and     ecx, MAPW - 1
    mov     edx, r12d
    shr     edx, 7
    call    veg_basei
    movzx   r13d, ax                    ; bx
    shr     eax, 16
    mov     r14d, eax                   ; by
    mov     ebp, [rbx + RS_HS]
    mov     r15d, ebp
    shr     r15d, 1                     ; c
    ; ячейки 1/8 тайла в пределах c/4 + 1 от основания
    mov     eax, r15d
    shr     eax, 2
    inc     eax
    mov     [vh_r], eax
    mov     esi, r14d
    shr     esi, 5
    sub     esi, eax                    ; cy
    mov     eax, r14d
    shr     eax, 5
    add     eax, [vh_r]
    mov     [vh_cy1], eax
.cy:
    cmp     esi, [vh_cy1]
    jg      .tn
    cmp     esi, DECW
    jae     .cyn
    mov     edi, r13d
    shr     edi, 5
    sub     edi, [vh_r]                 ; cx
    mov     eax, r13d
    shr     eax, 5
    add     eax, [vh_r]
    mov     [vh_cx1], eax
.cx:
    cmp     edi, [vh_cx1]
    jg      .cyn
    cmp     edi, DECW
    jae     .cxn
    ; клетка карты растения: (центр ячейки − основание) / 8 + c
    mov     eax, edi
    shl     eax, 5
    add     eax, 16
    sub     eax, r13d
    add     eax, 4
    sar     eax, 3
    add     eax, r15d                   ; kx
    mov     ecx, esi
    shl     ecx, 5
    add     ecx, 16
    sub     ecx, r14d
    add     ecx, 4
    sar     ecx, 3
    add     ecx, r15d                   ; ky
    test    dword [vh_fl], 0x8000
    jz      .nm
    xchg    eax, ecx
.nm:
    cmp     eax, ebp
    jae     .cxn
    cmp     ecx, ebp
    jae     .cxn
    imul    ecx, ebp
    add     ecx, eax
    mov     r8, [rbx + RS_HM]
    movzx   eax, byte [r8 + rcx]
    test    eax, eax
    jz      .cxn
    lea     ecx, [esi + HPAD]
    imul    ecx, ecx, HW8
    lea     ecx, [ecx + edi + HPAD]
    lea     r8, [hmap8]
    movzx   edx, byte [r8 + rcx]
    mov     r9d, edx
    and     r9d, 0x7F
    cmp     r9d, eax
    jae     .cxn
    and     edx, 0x80
    or      edx, eax
    mov     [r8 + rcx], dl
.cxn:
    inc     edi
    jmp     .cx
.cyn:
    inc     esi
    jmp     .cy
.tn:
    inc     r12d
    cmp     r12d, MAPW*MAPW
    jb      .t
    add     rsp, 40
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; --veg: в кадр — все варианты рядами, альбедо под простым светом слева-сверху
veg_gallery:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    mov     rdi, [fbbits]
    mov     ecx, SCR_W*SCR_H
    mov     eax, 0x00404850
    rep     stosd
    mov     r13d, 4                     ; x
    mov     r14d, 4                     ; y ряда
    xor     r15d, r15d                  ; высота ряда
    xor     r12d, r12d
.v:
    imul    eax, r12d, RS_SZ
    lea     rbx, [vrs]
    add     rbx, rax
    mov     eax, [rbx + RS_W]
    test    eax, eax
    jz      .vn
    lea     ecx, [r13d + eax]
    cmp     ecx, SCR_W - 4
    jbe     .fit
    mov     r13d, 4
    add     r14d, r15d
    add     r14d, 4
    xor     r15d, r15d
.fit:
    mov     eax, [rbx + RS_H]
    cmp     r15d, eax
    cmovl   r15d, eax
    xor     esi, esi                    ; строка
.r:
    cmp     esi, [rbx + RS_H]
    jae     .vd
    lea     eax, [r14d + esi]
    cmp     eax, SCR_H
    jae     .vd
    imul    ebp, eax, SCR_W
    add     ebp, r13d
    xor     edi, edi
.c:
    cmp     edi, [rbx + RS_W]
    jae     .rn
    mov     eax, esi
    imul    eax, [rbx + RS_W]
    add     eax, edi
    mov     rcx, [rbx + RS_PX]
    mov     edx, [rcx + rax*4]
    test    edx, edx
    jz      .cn
    mov     rcx, [rbx + RS_PN]
    movzx   ecx, byte [rcx + rax]
    ; N·L, свет (0.2, 0.6, 0.77) — слева-сверху
    imul    ecx, ecx, 6
    lea     r8, [nraw]
    movsx   eax, word [r8 + rcx]
    imul    eax, eax, 20
    movsx   r9d, word [r8 + rcx + 2]
    imul    r9d, r9d, 60
    add     eax, r9d
    movsx   r9d, word [r8 + rcx + 4]
    imul    r9d, r9d, 77
    add     eax, r9d                    ; ·10000
    xor     r9d, r9d
    cmp     eax, r9d
    cmovl   eax, r9d
    imul    eax, eax, 180
    mov     r9d, 10000
    xor     r10d, r10d
    push    rdx
    cdq
    idiv    r9d
    pop     rdx
    add     eax, 76                     ; 0.3 + 0.7·N·L (×256)
    mov     r8d, eax
    mov     ecx, edx
    and     ecx, 0xFFFFFF
    push    rbx
    call    vc_mul
    pop     rbx
    mov     rax, [fbbits]
    lea     r8d, [ebp + edi]
    mov     [rax + r8*4], ecx
.cn:
    inc     edi
    jmp     .c
.rn:
    inc     esi
    jmp     .r
.vd:
    add     r13d, [rbx + RS_W]
    add     r13d, 4
.vn:
    inc     r12d
    cmp     r12d, NVT
    jb      .v
    add     rsp, 40
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ============================================================================
section .rdata
align 4, db 0
vk_big:     dd 1.0e30
vk_mbig:    dd -1.0e30
vk_hpi:     dd 1.5707963
vk_2pi:     dd 6.2831853
vk_s2:      dd 0.70710678              ; вправо по экрану: (x − y)·√½
vk_s8:      dd 0.35355339              ; вниз: (x + y)·√⅛ − z·cos 30°
vk_c30:     dd 0.86602540
vk_c6:      dd 0.61237244              ; к глазу: (x + y)·0.612 + z·0.5
vk_s2i:     dd 1.41421356
vk_ic30:    dd 1.15470054              ; px экрана -> U высоты
vk_u2t:     dd 11.3137085              ; U -> 1/256 тайла
vk_i256:    dd 0.00390625
vk_i65536:  dd 0.0000152587890625
vk_64:      dd 64.0
vk_1em4:    dd 0.0001
vk_0_006:   dd 0.006
vk_0_01:    dd 0.01
vk_0_02:    dd 0.02
vk_0_03:    dd 0.03
vk_0_04:    dd 0.04
vk_0_05:    dd 0.05
vk_0_06:    dd 0.06
vk_0_07:    dd 0.07
vk_0_08:    dd 0.08
vk_0_09:    dd 0.09
vk_0_1:     dd 0.1
vk_0_11:    dd 0.11
vk_0_012:   dd 0.012
vk_0_12:    dd 0.12
vk_0_15:    dd 0.15
vk_0_2:     dd 0.2
vk_0_25:    dd 0.25
vk_0_3:     dd 0.3
vk_0_32:    dd 0.32
vk_0_35:    dd 0.35
vk_0_4:     dd 0.4
vk_0_42:    dd 0.42
vk_0_45:    dd 0.45
vk_0_55:    dd 0.55
vk_0_58:    dd 0.58
vk_0_6:     dd 0.6
vk_0_62:    dd 0.62
vk_0_7:     dd 0.7
vk_0_72:    dd 0.72
vk_0_75:    dd 0.75
vk_0_77:    dd 0.77
vk_0_8:     dd 0.8
vk_0_85:    dd 0.85
vk_0_9:     dd 0.9
vk_0_92:    dd 0.92
vk_0_96:    dd 0.96
vk_m0_2:    dd -0.2
vk_m1:      dd -1.0
vk_m2:      dd -2.0
vk_1_1:     dd 1.1
vk_1_2:     dd 1.2
vk_1_3:     dd 1.3
vk_1_35:    dd 1.35
vk_1_4:     dd 1.4
vk_1_5:     dd 1.5
vk_1_6:     dd 1.6
vk_1_8:     dd 1.8
vk_2:       dd 2.0
vk_2_2:     dd 2.2
vk_2_3:     dd 2.3
vk_2_4:     dd 2.4
vk_3:       dd 3.0
vk_3_6:     dd 3.6
vk_4:       dd 4.0
vk_5:       dd 5.0
vk_6:       dd 6.0
vk_6_67:    dd 6.67
vk_7:       dd 7.0
vk_7_5:     dd 7.5
vk_4_2:     dd 4.2
vk_1_9:     dd 1.9
vk_0_48:    dd 0.48
vk_0_65:    dd 0.65
vk_2_6:     dd 2.6
vk_3_8:     dd 3.8
vk_10:      dd 10.0
vk_8:       dd 8.0
vk_9:       dd 9.0
vk_24:      dd 24.0
vk_35:      dd 35.0
vk_55:      dd 55.0
vk_0_99:    dd 0.99
vk_vk:      dd 200.0                    ; VX_K: откуда луч к глазу
vk_zt:      dd 400.0                    ; VX_ZT: откуда луч сверху
vk_t2u:     dd 22.627417                ; тайл -> U
vk_b0:      dd 13.611806                ; основание модели: 154/256 тайла, U
; срезы поленьев и брёвен: свежий, берёзовый, серый старый, тёмный
vk_endc:    dd 0xB08A5C, 0xC0A474, 0x7A7064, 0x5A4A3A
; хвоя ели — четыре оттенка
vg_npal:    dd 0x1E3828, 0x223E2C, 0x264230, 0x1C3426
; варианты: порода, высота px, снег·100 | подвид << 16, зерно
vv_def:
    dd SP_SPRUCE, 104
    dw 80, 0
    dd 11
    dd SP_SPRUCE, 96
    dw 65, 0
    dd 12
    dd SP_SPRUCE, 90
    dw 90, 0
    dd 13
    dd SP_SPRUCE, 84
    dw 55, 0
    dd 14
    dd SP_SPRUCE, 78
    dw 85, 0
    dd 15
    dd SP_SPRUCE, 72
    dw 70, 0
    dd 16
    dd SP_SPRUCE, 66
    dw 95, 0
    dd 17
    dd SP_SPRUCE, 60
    dw 60, 0
    dd 18
    dd SP_PINE, 112
    dw 70, 0
    dd 21
    dd SP_PINE, 106
    dw 85, 0
    dd 22
    dd SP_PINE, 100
    dw 60, 0
    dd 23
    dd SP_PINE, 94
    dw 90, 0
    dd 24
    dd SP_PINE, 88
    dw 75, 0
    dd 25
    dd SP_BIRCH, 100
    dw 50, 0
    dd 31
    dd SP_BIRCH, 94
    dw 50, 0
    dd 32
    dd SP_BIRCH, 88
    dw 50, 0
    dd 33
    dd SP_BIRCH, 82
    dw 50, 0
    dd 34
    dd SP_BIRCH, 76
    dw 50, 0
    dd 35
    dd SP_BIRCH, 70
    dw 50, 0
    dd 36
    dd SP_YOUNG, 44
    dw 95, 0
    dd 41
    dd SP_YOUNG, 38
    dw 100, 0
    dd 42
    dd SP_YOUNG, 32
    dw 90, 0
    dd 43
    dd SP_YOUNG, 26
    dw 100, 0
    dd 44
    dd SP_BUSH, 28
    dw 0, 0
    dd 51
    dd SP_BUSH, 24
    dw 0, 0
    dd 52
    dd SP_BUSH, 20
    dw 0, 0
    dd 53
    dd SP_BUSH, 18
    dw 0, 1
    dd 54
    dd SP_BUSH, 15
    dw 0, 1
    dd 55
    dd SP_BUSH, 12
    dw 0, 1
    dd 56
    dd SP_STUMP, 10
    dw 0, 0
    dd 61
    dd SP_STUMP, 8
    dw 0, 0
    dd 62
    dd SP_STUMP, 6
    dw 0, 0
    dd 63
%if ($ - vv_def) != NVAR*16
  %error "vv_def: не NVAR вариантов"
%endif
vsp_first:  db 0, 8, 13, 19, 23, 29     ; первый вариант породы
vsp_n:      db 8, 5, 6, 4, 6, 3
vg_amp:     db 55, 35, 95, 40, 130, 0   ; качание у макушки: px·16 на 100 px высоты

section .data
align 8, db 0
vra_px:     dq 0                        ; арены: следующее свободное место
vra_pn:     dq 0
vra_po:     dq 0
vra_hm:     dq 0
vg_rng:     dd 1
vg_seed:    dd 0
vg_nb:      dd 0
vg_young:   dd 0
vg_sub:     dd 0
vg_ncol:    dd 0
vg_ncol2:   dd 0
vg_H:       dd 0.0
vg_R:       dd 0.0
vg_zc:      dd 0.0
vg_dz:      dd 0.0
vg_z:       dd 0.0
vg_zl:      dd 0.0
vg_t:       dd 0.0
vg_Rc:      dd 0.0
vg_phi0:    dd 0.0
vg_phi:     dd 0.0
vg_el:      dd 0.0
vg_dx:      dd 0.0
vg_dy:      dd 0.0
vg_lx:      dd 0.0
vg_ly:      dd 0.0
vg_L:       dd 0.0
vg_dr:      dd 0.0
vg_s0:      dd 0.0
vg_snow:    dd 0.0
vg_sthr:    dd 0.0
vg_bdz:     dd 0.0
vg_ang:     dd 0.0
vg_ang2:    dd 0.0
vg_sin:     dd 0.0
vb_t:       dd 0.0
vb_bx:      dd 0.0
vb_by:      dd 0.0
vb_bz:      dd 0.0
vb_w:       dd 0.0
vb_n:       dd 0
vb_l:       dd 0.0
vb_px:      dd 0.0
vb_py:      dd 0.0
vb_pz:      dd 0.0
vb_r:       dd 0.0
vb_ao:      dd 0
vb_aob:     dd 0
vc_x:       dd 0.0
vc_y:       dd 0.0
vc_z:       dd 0.0
vc_r:       dd 0.0
vc_h:       dd 0.0
vt_dx:      dd 0.0
vt_dy:      dd 0.0
vt_len:     dd 0.0
vt_s:       dd 0.0
vt_x:       dd 0.0
vt_y:       dd 0.0
vt_z:       dd 0.0
vr_x0:      dd 0
vr_y0:      dd 0
vr_w:       dd 0
vr_h:       dd 0
vr_n_px:    dd 0
vr_x0f:     dd 0.0
vr_y0f:     dd 0.0
vs_cx:      dd 0.0
vs_cy:      dd 0.0
vs_d0:      dd 0.0
vs_r:       dd 0.0
vs_ir:      dd 0.0
vs_nx:      dd 0.0
vs_ny:      dd 0.0
vs_nz:      dd 0.0
vs_sx:      dd 0.0
vs_sy:      dd 0.0
vs_sz:      dd 0.0
vs_h:       dd 0
vs_bi:      dd 0
vs_hc:      dd 0
vs_hn:      dd 0
vh_fl:      dd 0
vh_r:       dd 0
vh_cx1:     dd 0
vh_cy1:     dd 0
dbg_veg:    dd 0                        ; --veg: в снимке — все растения
; брусья и цилиндры
vg_nx:      dd 0
vx_tmode:   dd 0
vx_scl:     dd 1.0
vx_shx:     dd 0.0
vx_shy:     dd 0.0
vx_off:     dd 0.0, 0.0, 0.0
vx_type:    dd 0
vx_col:     dd 0
vx_colj:    dd 0
vx_kind:    dd 0
vx_hu:      dd 0.0
vx_hv:      dd 0.0
vx_inf:     dd 0.0
vx_r:       dd 0.0
vx_tc:      dd 0.0
vs_flat:    dd 0
vs_lu:      dd 0.0
vs_lv:      dd 0.0
vs_lw:      dd 0.0
vs_tu:      dd 0.0
vs_tv:      dd 0.0
vs_nu:      dd 0.0
vs_nv:      dd 0.0
vx_a:       dd 0.0, 0.0, 0.0
vx_b:       dd 0.0, 0.0, 0.0
vx_hint:    dd 0.0, 0.0, 0.0
vx_w3:      dd 0.0, 0.0, 0.0
vx_u3:      dd 0.0, 0.0, 0.0
vx_lo:      dd 0.0, 0.0, 0.0
vx_ld:      dd 0.0, 0.0, 0.0
vx_ln:      dd 0.0, 0.0, 0.0
vx_n:       dd 0.0, 0.0, 0.0
vx_P:       dd 0.0, 0.0, 0.0
vx_D:       dd 0.0, 0.0, 0.0
vx_pa:      dd 0.0, 0.0, 0.0
vx_pb:      dd 0.0, 0.0, 0.0
vx_pk:      dd 0.0, 0.0, 0.0
vx_cn:      times 24 dd 0.0

section .bss
alignb 16
vg_blobs:   resb VG_MAXB*VB_SZ
vr_d:       resd VRW*VRH                ; съёмка: глубина, шарик, нормаль
vr_b:       resd VRW*VRH
vr_n:       resd VRW*VRH*3
vr_sp:      resd VRW*VRH*3              ; точка поверхности (брусья)
vg_shp:     resb VX_MAX*VX_SZ
vt_top:     resd VTN*VTN
va_px:      resd VA_PX                  ; картинки растений
va_po:      resd VA_PX
va_pn:      resb VA_PX
va_hm:      resb VA_HM
alignb 16
vrs:        resb NVT*RS_SZ
vmap:       resw MAPW*MAPW              ; вариант + 1 | (сдвиг основания | зеркало << 7) << 8
nq_v:       resd (NAZ*NEL + 1)*4
nq_lut:     resb 4096
nq_mir:     resb 128
section .text
