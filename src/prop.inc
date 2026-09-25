; ----------------------------------------------------------------------------
;  prop.inc — двор объёмными моделями: заборы, фонари, лавочки, дрова, будки,
;  скворечники, кресты, снеговики, бельевая верёвка, колодец-журавль.
;  Модель — брусья, цилиндры и шарики (veg.inc: vx_seg, vx_tab), снимается той же
;  камерой в объёмную картинку (нормаль и точка в мире на пиксель): фонарь светит
;  на доски забора сбоку, луна кладёт тень лавки, снег лежит на всём, что сверху
;  открыто. На тайле — вариант и зеркало (x <-> y) в vmap, как у растений:
;  основание — середина сетки сдвигов (0.6, 0.6 тайла), у него же фигуры столкновения
; ----------------------------------------------------------------------------
PV_FENCE    equ 0                   ; штакетник: прясло (3), угол (2); столб; сплошной: прясло (2), угол
PV_FPOST    equ 5
PV_FBOARD   equ 6
PV_LAMP     equ 9
PV_BENCH    equ 11                  ; без спинки, спинка к +y, спинка к −y
PV_PILE     equ 14
PV_KENNEL   equ 17
PV_BIRDH    equ 19
PV_CROSS    equ 21
PV_SNOWMAN  equ 25
PV_LINE     equ 28
PV_WELL     equ 29
%if PV_WELL + 1 != NPROP
  %error "prop.inc: не NPROP вариантов"
%endif
%define PF_X0   -13.611806          ; кромки тайла от основания, U: 0 и 1
%define PF_X1   9.015611
%define PF_XP   8.515611            ; столб — чуть внутрь от угла
%define PF_C    -2.298097           ; середина тайла
%define PL_BULB 0.3363              ; лампа от столба, тайлы (7.6 U)

; таблицы фигур (vx_tab): первое слово — 0: x, y в тайлах от угла, 1: U от основания
%macro PBOX 10                      ; A (x, y, z), B (x, y, z), hu, hv, цвет, вид
    dd 1, %1, %2, %3, %4, %5, %6, %7, %8, 0.0, 0.0, 0.0, %9, %10
%endmacro
%macro PBOXH 13                     ; + своя «вверх» (x, y, z) — наклонная доска
    dd 1, %1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13
%endmacro
%macro PCYL 9                       ; A, B, радиус, цвет, вид
    dd 2, %1, %2, %3, %4, %5, %6, %7, %7, 0.0, 0.0, 0.0, %8, %9
%endmacro
%macro PBALL 6                      ; середина, радиус, цвет, вид
    dd 3, %1, %2, %3, 0.0, 0.0, 0.0, %4, %4, 0.0, 0.0, 0.0, %5, %6
%endmacro
%macro PEND 0
    dd 0
%endmacro
; dword [%1] = число
%macro VSET 2
    mov     dword [%1], __float32__(%2)
%endmacro

section .text

; ---------------------------------------------------------------- все модели
; после растений: картинки NVAR .. NVAR + NPROP − 1
prop_init:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40
    xor     r12d, r12d
.v:
    mov     rax, [vra_px]
    lea     rcx, [va_px + (VA_PX - VRW*VRH)*4]
    cmp     rax, rcx
    ja      .skip
    mov     rax, [vra_hm]
    lea     rcx, [va_hm + VA_HM - HSMAX*HSMAX]
    cmp     rax, rcx
    ja      .skip
    lea     eax, [r12d + 0x51]
    imul    eax, eax, 0x9E3779B1
    or      eax, 1
    mov     [vg_rng], eax
    mov     [vg_seed], eax
    mov     dword [vg_nb], 0
    mov     dword [vg_nx], 0
    call    vx_reset
    lea     rax, [pv_arg]
    movzx   eax, byte [rax + r12]
    mov     [pv_a], eax
    lea     rax, [pv_gen]
    call    [rax + r12*8]
    lea     eax, [r12d + NVAR]
    imul    eax, eax, RS_SZ
    lea     rsi, [vrs]
    add     rsi, rax
    call    vg_render
    mov     dword [rsi + RS_AMP], 0
.skip:
    inc     r12d
    cmp     r12d, NPROP
    jb      .v
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; перенос таблиц: без масштаба, сдвига и наклона
vx_reset:
    VSET    vx_scl, 1.0
    xor     eax, eax
    mov     [vx_off], eax
    mov     [vx_off + 4], eax
    mov     [vx_off + 8], eax
    mov     [vx_shx], eax
    mov     [vx_shy], eax
    ret

; rsi -> таблица (портит rax, rcx, rdx, r8, r9, xmm0-xmm7)
pv_tab:
    jmp     vx_tab

; xmm0..2 -> vx_a; xmm3..5 -> vx_b (без переноса таблиц)
%macro VAB 0
    movss   [vx_a], xmm0
    movss   [vx_a + 4], xmm1
    movss   [vx_a + 8], xmm2
    movss   [vx_b], xmm3
    movss   [vx_b + 4], xmm4
    movss   [vx_b + 8], xmm5
%endmacro

; ---------------------------------------------------------------- забор
; pv_a: биты 0-1 — прясло вдоль x по кромке +y / угол (и вдоль y по кромке +x) / столб;
; бит 2 — сплошной; биты 4-5 — ровный / без штакетины / сломанная / покосился.
; Столб — только в переднем углу (1, 1): начало прясла держит столб соседа
pf_fence:
    push    rbx
    sub     rsp, 32
    mov     eax, [pv_a]
    mov     dword [pf_cpk], 0x5E5242    ; штакетины — серые от ветра
    mov     dword [pf_crl], 0x4A3E32
    mov     dword [pf_cps], 0x3C3228
    test    eax, 4
    jz      .c1
    mov     dword [pf_cpk], 0x4C4034    ; сплошной — тёмный, в смоле
    mov     dword [pf_crl], 0x3E342A
.c1:
    and     eax, 3
    cmp     eax, 2
    je      .post
    VSET    pr_x0, PF_X0
    VSET    pr_y0, PF_X1
    VSET    pr_dx, 1.0
    VSET    pr_dy, 0.0
    VSET    pr_nx, 0.0
    VSET    pr_ny, 1.0
    call    pf_run
    mov     eax, [pv_a]
    and     eax, 3
    cmp     eax, 1
    jne     .post
    VSET    pr_x0, PF_X1
    VSET    pr_y0, PF_X0
    VSET    pr_dx, 0.0
    VSET    pr_dy, 1.0
    VSET    pr_nx, 1.0
    VSET    pr_ny, 0.0
    call    pf_run
.post:
    ; столб с шапкой снега
    mov     dword [vx_type], 1
    VSET    vx_a, PF_XP
    VSET    vx_a + 4, PF_XP
    VSET    vx_a + 8, -0.8
    VSET    vx_b, PF_XP
    VSET    vx_b + 4, PF_XP
    VSET    vx_b + 8, 18.2
    VSET    vx_hu, 0.58
    VSET    vx_hv, 0.58
    mov     eax, [pf_cps]
    mov     [vx_col], eax
    mov     dword [vx_kind], VK_WOOD
    call    vx_seg
    VSET    vx_a + 8, 18.5
    VSET    vx_hu, 0.85
    mov     dword [vx_col], VC_SNOW
    mov     dword [vx_kind], VK_SNOW
    call    vx_ball
    add     rsp, 32
    pop     rbx
    ret

; прясло от pr_x0, pr_y0 по pr_dx, pr_dy (длина — тайл), штакетины по линии, прожилины
; позади (−pr_n); стиль и поломки — pv_a
pf_run:
    push    rbx
    push    r12
    push    r13
    sub     rsp, 32
    ; прожилины
    mov     r13d, 2
    VSET    pr_z, 4.8
.rl:
    movss   xmm6, [pr_nx]
    mulss   xmm6, [pk_m0_8]
    addss   xmm6, [pr_x0]
    movss   xmm7, [pr_ny]
    mulss   xmm7, [pk_m0_8]
    addss   xmm7, [pr_y0]
    movss   [vx_a], xmm6
    movss   [vx_a + 4], xmm7
    movss   xmm0, [pr_z]
    movss   [vx_a + 8], xmm0
    movss   [vx_b + 8], xmm0
    movss   xmm0, [pr_dx]
    mulss   xmm0, [vk_t2u]
    addss   xmm0, xmm6
    movss   [vx_b], xmm0
    movss   xmm0, [pr_dy]
    mulss   xmm0, [vk_t2u]
    addss   xmm0, xmm7
    movss   [vx_b + 4], xmm0
    mov     dword [vx_type], 1
    VSET    vx_hu, 0.42
    VSET    vx_hv, 0.72
    mov     eax, [pf_crl]
    mov     [vx_col], eax
    mov     dword [vx_kind], VK_WOOD
    call    vx_seg
    VSET    pr_z, 12.6
    dec     r13d
    jnz     .rl
    ; штакетины (у сплошного — доски вплотную)
    mov     r12d, 4
    test    dword [pv_a], 4
    jz      .n1
    mov     r12d, 7
.n1:
    cvtsi2ss xmm0, r12d
    movss   xmm1, [vk_t2u]
    divss   xmm1, xmm0
    movss   [pr_step], xmm1
    xor     ebx, ebx
.p:
    ; поломки: 1 — нет второй, 2 — третья сломана, 3 — всё покосилось
    mov     eax, [pv_a]
    shr     eax, 4
    and     eax, 3
    cmp     eax, 1
    jne     .p1
    cmp     ebx, 1
    je      .pn
.p1:
    cvtsi2ss xmm0, ebx
    addss   xmm0, [f_0_5]
    mulss   xmm0, [pr_step]
    movss   [pr_t], xmm0
    call    vr_s
    mulss   xmm0, [pk_0_25]
    addss   xmm0, [pr_t]
    movss   [pr_t], xmm0
    ; высота
    call    vr_s
    mulss   xmm0, [pk_0_6]
    addss   xmm0, [pk_15_3]
    test    dword [pv_a], 4
    jz      .h1
    addss   xmm0, [pk_1_9]
.h1:
    mov     eax, [pv_a]
    shr     eax, 4
    and     eax, 3
    cmp     eax, 2
    jne     .h2
    cmp     ebx, 2
    jne     .h2
    call    vr_f
    addss   xmm0, xmm0
    addss   xmm0, [pk_7_5]
.h2:
    movss   [pr_h], xmm0
    ; наклон верха: вдоль и поперёк прясла
    call    vr_s
    mulss   xmm0, [pk_0_3]
    movss   [pr_ld], xmm0
    call    vr_s
    mulss   xmm0, [pk_0_25]
    movss   [pr_ln], xmm0
    mov     eax, [pv_a]
    shr     eax, 4
    and     eax, 3
    cmp     eax, 3
    jne     .l1
    movss   xmm0, [pr_ln]
    addss   xmm0, [pk_0_9]              ; покосился наружу
    movss   [pr_ln], xmm0
.l1:
    ; низ: по линии; у сплошного — через одну чуть наружу
    movss   xmm6, [pr_dx]
    mulss   xmm6, [pr_t]
    addss   xmm6, [pr_x0]
    movss   xmm7, [pr_dy]
    mulss   xmm7, [pr_t]
    addss   xmm7, [pr_y0]
    test    dword [pv_a], 4
    jz      .b1
    test    ebx, 1
    jz      .b1
    movss   xmm0, [pr_nx]
    mulss   xmm0, [pk_0_28]
    addss   xmm6, xmm0
    movss   xmm0, [pr_ny]
    mulss   xmm0, [pk_0_28]
    addss   xmm7, xmm0
.b1:
    movss   [vx_a], xmm6
    movss   [vx_a + 4], xmm7
    VSET    vx_a + 8, -0.6
    movss   xmm0, [pr_dx]
    mulss   xmm0, [pr_ld]
    addss   xmm6, xmm0
    movss   xmm0, [pr_nx]
    mulss   xmm0, [pr_ln]
    addss   xmm6, xmm0
    movss   xmm0, [pr_dy]
    mulss   xmm0, [pr_ld]
    addss   xmm7, xmm0
    movss   xmm0, [pr_ny]
    mulss   xmm0, [pr_ln]
    addss   xmm7, xmm0
    movss   [vx_b], xmm6
    movss   [vx_b + 4], xmm7
    movss   xmm0, [pr_h]
    movss   [vx_b + 8], xmm0
    ; поперёк — по пряслу: «вверх» (dy, −dx, 0)
    mov     eax, [pr_dy]
    mov     [vx_hint], eax
    movss   xmm0, [pr_dx]
    xorps   xmm0, [f_signmask]
    movss   [vx_hint + 4], xmm0
    mov     dword [vx_hint + 8], 0
    mov     dword [vx_type], 1
    VSET    vx_hu, 1.2
    test    dword [pv_a], 4
    jz      .w1
    VSET    vx_hu, 1.66
.w1:
    VSET    vx_hv, 0.32
    mov     eax, [pf_cpk]
    mov     [vx_col], eax
    mov     dword [vx_kind], VK_WOOD
    call    vx_seg
.pn:
    inc     ebx
    cmp     ebx, r12d
    jb      .p
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rbx
    ret

; ---------------------------------------------------------------- фонарь, лавочка
pf_lamp:
    lea     rsi, [pt_lamp_a]
    cmp     dword [pv_a], 0
    je      pv_tab
    lea     rsi, [pt_lamp_b]
    jmp     pv_tab

; pv_a: 0 без спинки, 1 — спинка к +y, 2 — к −y
pf_bench:
    sub     rsp, 40
    lea     rsi, [pt_bench]
    call    pv_tab
    mov     eax, [pv_a]
    test    eax, eax
    jz      .out
    lea     rsi, [pt_bench_bp]
    cmp     eax, 1
    je      .t
    lea     rsi, [pt_bench_bm]
.t:
    call    pv_tab
.out:
    add     rsp, 40
    ret

; ---------------------------------------------------------------- поленница
; поленья вдоль y (срезы — на грань +y), 6 рядов в два полена глубиной, по краям —
; колья, сверху снег; pv_a — порода: сосна, берёза, старые серые
pf_pile:
    push    rbx
    push    r12
    push    r13
    sub     rsp, 32
    mov     eax, [pv_a]
    lea     rcx, [pp_bark]
    mov     ecx, [rcx + rax*4]
    mov     [pp_col], ecx
    shl     eax, 8
    or      eax, VK_LOG
    mov     [pp_kind], eax
    xor     r12d, r12d                  ; ряд
.row:
    cvtsi2ss xmm0, r12d
    mulss   xmm0, [pk_2_3]
    addss   xmm0, [pk_1_3]
    movss   [pp_z], xmm0
    ; x: от края, у нечётного ряда — вполовину сдвинут
    VSET    pp_x, -12.298097
    test    r12d, 1
    jz      .x
    movss   xmm0, [pp_x]
    addss   xmm0, [pk_1_25]
    movss   [pp_x], xmm0
.x:
    movss   xmm0, [pp_x]
    comiss  xmm0, [pk_xmax]
    ja      .rn
    xor     r13d, r13d                  ; глубина: ближний, дальний
.d:
    ; верхний ряд прорежен
    cmp     r12d, 5
    jne     .d1
    call    vr_f
    comiss  xmm0, [pk_0_3]
    jb      .dn
.d1:
    call    vr_s
    mulss   xmm0, [pk_0_25]
    addss   xmm0, [pp_x]
    movss   [vx_a], xmm0
    movss   [vx_b], xmm0
    call    vr_s
    mulss   xmm0, [pk_0_2]
    addss   xmm0, [pp_z]
    movss   [vx_a + 8], xmm0
    movss   [vx_b + 8], xmm0
    ; y: [−7.6, −0.3] или [0.3, 7.6] от середины, концы вразнобой
    call    vr_s
    mulss   xmm0, [f_0_5]
    movss   xmm1, [pk_m7_6]
    test    r13d, r13d
    jz      .y0
    movss   xmm1, [pk_0_3]
.y0:
    addss   xmm0, xmm1
    addss   xmm0, [pk_c]
    movss   [vx_a + 4], xmm0
    call    vr_s
    mulss   xmm0, [f_0_5]
    movss   xmm1, [pk_m0_3]
    test    r13d, r13d
    jz      .y1
    movss   xmm1, [pk_7_6]
.y1:
    addss   xmm0, xmm1
    addss   xmm0, [pk_c]
    movss   [vx_b + 4], xmm0
    call    vr_f
    mulss   xmm0, [pk_0_22]
    addss   xmm0, [pk_1_12]
    movss   [vx_hu], xmm0
    mov     dword [vx_type], 2
    mov     eax, [pp_col]
    mov     [vx_col], eax
    mov     eax, [pp_kind]
    mov     [vx_kind], eax
    call    vx_seg
.dn:
    inc     r13d
    cmp     r13d, 2
    jb      .d
    movss   xmm0, [pp_x]
    addss   xmm0, [pk_2_5]
    movss   [pp_x], xmm0
    jmp     .x
.rn:
    inc     r12d
    cmp     r12d, 6
    jb      .row
    ; колья по краям и снег поверху
    lea     rsi, [pt_pile]
    call    pv_tab
    VSET    pp_x, -11.298097
.sn:
    call    vr_s
    mulss   xmm0, [pk_0_6]
    addss   xmm0, [pp_x]
    movss   [vx_a], xmm0
    call    vr_s
    mulss   xmm0, [pk_4_5]
    addss   xmm0, [pk_c]
    movss   [vx_a + 4], xmm0
    call    vr_f
    mulss   xmm0, [pk_0_6]
    addss   xmm0, [pk_14_2]
    movss   [vx_a + 8], xmm0
    call    vr_f
    mulss   xmm0, [pk_0_6]
    addss   xmm0, [pk_1_6]
    movss   [vx_hu], xmm0
    mov     dword [vx_col], VC_SNOW
    mov     dword [vx_kind], VK_SNOW
    call    vx_ball
    movss   xmm0, [pp_x]
    addss   xmm0, [pk_2_3]
    movss   [pp_x], xmm0
    comiss  xmm0, [pk_xmax]
    jb      .sn
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rbx
    ret

; ---------------------------------------------------------------- будка, скворечник
; будка: стенки — пять венцов досок (через один чуть утоплены — видно швы), фронтоны
; ступеньками под двускатной крышей, лаз на грани +x, цепь и миска
pf_kennel:
    push    rbx
    sub     rsp, 32
    mov     eax, [pv_a]
    lea     rcx, [pk_kcol]
    mov     ecx, [rcx + rax*4]
    mov     [pp_col], ecx
    xor     ebx, ebx
.r:
    cvtsi2ss xmm0, ebx
    mulss   xmm0, [pk_1_9]
    addss   xmm0, [pk_0_95]
    movss   [vx_a + 8], xmm0
    movss   [vx_b + 8], xmm0
    xorps   xmm1, xmm1
    test    ebx, 1
    jz      .r1
    movss   xmm1, [pk_m0_12]
.r1:
    mov     dword [vx_a], 0
    mov     dword [vx_b], 0
    movss   xmm0, [pk_m5_2]
    subss   xmm0, xmm1
    movss   [vx_a + 4], xmm0
    movss   xmm0, [pk_5_2]
    addss   xmm0, xmm1
    movss   [vx_b + 4], xmm0
    movss   xmm0, [pk_4_2]
    addss   xmm0, xmm1
    movss   [vx_hu], xmm0
    VSET    vx_hv, 0.95
    mov     dword [vx_type], 1
    mov     eax, [pp_col]
    mov     [vx_col], eax
    mov     dword [vx_kind], VK_WOOD
    call    vx_seg
    inc     ebx
    cmp     ebx, 5
    jb      .r
    lea     rsi, [pt_kennel]
    call    pv_tab
    add     rsp, 32
    pop     rbx
    ret

pf_birdh:
    lea     rsi, [pt_birdh_a]
    cmp     dword [pv_a], 0
    je      pv_tab
    lea     rsi, [pt_birdh_b]
    jmp     pv_tab

; ---------------------------------------------------------------- крест, снеговик
; pv_a: 0 простой, 1 пониже и покосился, 2 с кровелькой (голубец), 3 повыше, наклон вбок
pf_cross:
    sub     rsp, 40
    mov     eax, [pv_a]
    lea     rcx, [pk_xscl]
    mov     ecx, [rcx + rax*4]
    mov     [vx_scl], ecx
    lea     rcx, [pk_xshx]
    mov     ecx, [rcx + rax*4]
    mov     [vx_shx], ecx
    lea     rsi, [pt_cross]
    call    pv_tab
    cmp     dword [pv_a], 2
    jne     .out
    lea     rsi, [pt_cross_roof]
    call    pv_tab
.out:
    add     rsp, 40
    ret

; pv_a: 0 — в ведре, 1 — в шарфе, 2 — маленький, кривой, в ведре
pf_snowman:
    sub     rsp, 40
    cmp     dword [pv_a], 2
    jne     .s
    VSET    vx_scl, 0.82
    VSET    vx_shx, 0.05
    VSET    vx_shy, -0.03
.s:
    lea     rsi, [pt_snowman]
    call    pv_tab
    lea     rsi, [pt_sm_bucket]
    cmp     dword [pv_a], 1
    jne     .t
    lea     rsi, [pt_sm_scarf]
.t:
    call    pv_tab
    add     rsp, 40
    ret

; ---------------------------------------------------------------- верёвка, колодец
; столбы, перекладины; верёвка — 12 отрезков по провису (бельё — картинкой поверх)
pf_line:
    push    rbx
    sub     rsp, 32
    lea     rsi, [pt_line]
    call    pv_tab
    xor     ebx, ebx
.s:
    cvtsi2ss xmm0, ebx
    mulss   xmm0, [pk_1_12th]           ; t
    call    .pt
    movss   [vx_a], xmm0
    movss   [vx_a + 4], xmm1
    movss   [vx_a + 8], xmm2
    lea     eax, [ebx + 1]
    cvtsi2ss xmm0, eax
    mulss   xmm0, [pk_1_12th]
    call    .pt
    movss   [vx_b], xmm0
    movss   [vx_b + 4], xmm1
    movss   [vx_b + 8], xmm2
    mov     dword [vx_type], 2
    VSET    vx_hu, 0.28
    mov     dword [vx_col], 0x6A6E74
    mov     dword [vx_kind], VK_PLAIN
    call    vx_seg
    inc     ebx
    cmp     ebx, 12
    jb      .s
    add     rsp, 32
    pop     rbx
    ret
; xmm0 = t -> точка верёвки xmm0, xmm1, xmm2 (U)
.pt:
    movss   xmm3, [f_1]
    subss   xmm3, xmm0
    mulss   xmm3, xmm0
    mulss   xmm3, [pk_m12_8]            ; провис 3.2·4t(1 − t) вниз
    addss   xmm3, [pk_26_9]
    movaps  xmm2, xmm3
    movaps  xmm1, xmm0
    mulss   xmm0, [pk_ldx]
    addss   xmm0, [pk_lx0]
    mulss   xmm1, [pk_ldy]
    addss   xmm1, [pk_ly0]
    ret

; колодец-журавль: сруб в пять венцов (концы брёвен навыпуск), полкрышки, тёмный
; зев, стояк с развилкой, очеп с противовесом, шест с ведром над зевом
pf_well:
    push    rbx
    push    r12
    sub     rsp, 40
    xor     ebx, ebx                    ; венец
.k:
    cvtsi2ss xmm0, ebx
    mulss   xmm0, [pk_2_1]
    addss   xmm0, [pk_1_15]
    movss   [pp_z], xmm0
    xor     r12d, r12d                  ; ближнее, дальнее бревно
.lg:
    call    vr_s
    mulss   xmm0, [pk_0_3]
    addss   xmm0, [pk_m5_5]
    movss   [pr_t], xmm0                ; начало вдоль
    call    vr_s
    mulss   xmm0, [pk_0_3]
    addss   xmm0, [pk_5_5]
    movss   [pr_h], xmm0                ; конец
    movss   xmm1, [pk_m3_45]
    test    r12d, r12d
    jz      .o
    movss   xmm1, [pk_3_45]
.o:
    movss   [pr_ln], xmm1               ; поперёк
    test    ebx, 1
    jnz     .ly
    ; вдоль x
    movss   xmm0, [pr_t]
    addss   xmm0, [pk_wcx]
    movss   [vx_a], xmm0
    movss   xmm0, [pr_h]
    addss   xmm0, [pk_wcx]
    movss   [vx_b], xmm0
    movss   xmm0, [pr_ln]
    addss   xmm0, [pk_wcy]
    movss   [vx_a + 4], xmm0
    movss   [vx_b + 4], xmm0
    jmp     .lz
.ly:
    movss   xmm0, [pr_t]
    addss   xmm0, [pk_wcy]
    movss   [vx_a + 4], xmm0
    movss   xmm0, [pr_h]
    addss   xmm0, [pk_wcy]
    movss   [vx_b + 4], xmm0
    movss   xmm0, [pr_ln]
    addss   xmm0, [pk_wcx]
    movss   [vx_a], xmm0
    movss   [vx_b], xmm0
.lz:
    movss   xmm0, [pp_z]
    movss   [vx_a + 8], xmm0
    movss   [vx_b + 8], xmm0
    call    vr_f
    mulss   xmm0, [pk_0_2]
    addss   xmm0, [pk_1_05]
    movss   [vx_hu], xmm0
    mov     dword [vx_type], 2
    mov     dword [vx_col], 0x564C42
    mov     dword [vx_kind], VK_LOG | (2 << 8)
    call    vx_seg
    inc     r12d
    cmp     r12d, 2
    jb      .lg
    inc     ebx
    cmp     ebx, 5
    jb      .k
    lea     rsi, [pt_well]
    call    pv_tab
    add     rsp, 40
    pop     r12
    pop     rbx
    ret

; ---------------------------------------------------------------- на карте
; после veg_assign: вариант и зеркало каждой мелочи двора; фонарю — огонь у лампы
prop_assign:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32
    xor     r12d, r12d
.t:
    lea     rdx, [tmap]
    movzx   eax, byte [rdx + r12]
    mov     ebx, r12d
    and     ebx, MAPW - 1               ; x
    mov     esi, r12d
    shr     esi, 7                      ; y
    lea     ecx, [ebx*8 + 3]
    lea     edx, [esi*4 + 17]
    HASH    ecx, edx
    mov     r13d, ecx                   ; хеш места
    xor     edi, edi                    ; зеркало
    cmp     eax, T_FENCE
    je      .fence
    cmp     eax, T_LAMP
    je      .lamp
    cmp     eax, T_BENCH
    je      .bench
    cmp     eax, T_PILE
    je      .pile
    cmp     eax, T_KENNEL
    je      .kennel
    cmp     eax, T_BIRDH
    je      .birdh
    cmp     eax, T_CROSS
    je      .cross
    cmp     eax, T_SNOWMAN
    je      .snowman
    cmp     eax, T_LINE
    je      .line
    cmp     eax, T_WELL
    je      .well
    jmp     .n
.fence:
    ; сплошной или штакетник — пятнами по деревне (участок обычно целиком)
    mov     ecx, ebx
    mov     edx, esi
    mov     r8d, 13
    mov     r9d, 0x77
    call    vg_vnoise
    xor     r8d, r8d
    comiss  xmm0, [pk_0_62]
    jbe     .f0
    inc     r8d
.f0:
    mov     ecx, ebx
    mov     edx, esi
    call    fence_edges
    test    eax, eax
    jnz     .f1
    mov     eax, PV_FPOST
    jmp     .put
.f1:
    cmp     eax, 3
    je      .fl
    cmp     eax, 2
    jne     .fe
    mov     edi, 1                      ; вдоль y — зеркало прясла вдоль x
.fe:
    mov     eax, r13d
    shr     eax, 8
    and     eax, 0xFFFF
    test    r8d, r8d
    jnz     .fbe
    imul    eax, eax, 3
    shr     eax, 16
    add     eax, PV_FENCE
    jmp     .put
.fbe:
    shr     eax, 15
    add     eax, PV_FBOARD
    jmp     .put
.fl:
    mov     eax, PV_FBOARD + 2
    test    r8d, r8d
    jnz     .put
    mov     eax, r13d
    shr     eax, 9
    and     eax, 1
    add     eax, PV_FENCE + 3
    jmp     .put
.lamp:
    mov     edi, r13d
    shr     edi, 5
    and     edi, 1
    mov     eax, r13d
    shr     eax, 11
    and     eax, 1
    add     eax, PV_LAMP
    call    .bulb
    jmp     .put
.bench:
    ; спинкой к забору: забор за ней по y — вдоль x; по x — вдоль y (зеркало)
    mov     eax, PV_BENCH + 1
    lea     ecx, [ebx]
    lea     edx, [esi + 1]
    call    .isf
    jz      .put
    lea     edx, [esi + 2]
    call    .isf
    jz      .put
    mov     eax, PV_BENCH + 2
    lea     edx, [esi - 1]
    call    .isf
    jz      .put
    mov     edi, 1
    mov     eax, PV_BENCH + 1
    lea     ecx, [ebx + 1]
    mov     edx, esi
    call    .isf
    jz      .put
    lea     ecx, [ebx + 2]
    call    .isf
    jz      .put
    mov     eax, PV_BENCH + 2
    lea     ecx, [ebx - 1]
    call    .isf
    jz      .put
    mov     edi, r13d
    shr     edi, 7
    and     edi, 1
    mov     eax, PV_BENCH
    jmp     .put
.pile:
    mov     eax, r13d
    shr     eax, 8
    and     eax, 0xFFFF
    imul    eax, eax, 3
    shr     eax, 16
    add     eax, PV_PILE
    jmp     .rmir
.kennel:
    mov     eax, r13d
    shr     eax, 9
    and     eax, 1
    add     eax, PV_KENNEL
    jmp     .rmir
.birdh:
    mov     eax, r13d
    shr     eax, 9
    and     eax, 1
    add     eax, PV_BIRDH
    jmp     .rmir
.cross:
    mov     eax, r13d
    shr     eax, 9
    and     eax, 3
    add     eax, PV_CROSS
    jmp     .rmir
.snowman:
    mov     eax, r13d
    shr     eax, 8
    and     eax, 0xFFFF
    imul    eax, eax, 3
    shr     eax, 16
    add     eax, PV_SNOWMAN
    jmp     .rmir
.line:
    mov     eax, PV_LINE
    jmp     .put
.well:
    mov     eax, PV_WELL
    jmp     .put
.rmir:
    mov     edi, r13d
    shr     edi, 27
    and     edi, 1
.put:
    ; вариант + 1 | (сдвиг основания 4 — середина | зеркало << 7) << 8
    lea     eax, [eax + NVAR + 1]
    shl     edi, 15
    or      eax, edi
    or      eax, 4 << 8
    lea     rdx, [vmap]
    mov     [rdx + r12*2], ax
.n:
    inc     r12d
    cmp     r12d, MAPW*MAPW
    jb      .t
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
; ecx, edx = тайл -> ZF = 1, если забор (сохраняет eax)
.isf:
    push    rax
    call    get_tile
    cmp     eax, T_FENCE
    pop     rax
    ret
; огонь фонаря ebx, esi — к лампе: от столба на PL_BULB тайла по x (у зеркала — по y)
; (сохраняет eax)
.bulb:
    push    rax
    lea     r8, [lights]
    mov     r9d, [nlights]
.b1:
    test    r9d, r9d
    jz      .b9
    mov     eax, [r8 + L_KIND]
    cmp     eax, LK_LAMP
    je      .b2
    cmp     eax, LK_BROKEN
    jne     .bn
.b2:
    cvttss2si eax, [r8 + L_X]
    cmp     eax, ebx
    jne     .bn
    cvttss2si eax, [r8 + L_Y]
    cmp     eax, esi
    jne     .bn
    cvtsi2ss xmm0, ebx
    addss   xmm0, [pk_base]
    cvtsi2ss xmm1, esi
    addss   xmm1, [pk_base]
    test    edi, edi
    jnz     .b3
    addss   xmm0, [pk_bulb]
    jmp     .b4
.b3:
    addss   xmm1, [pk_bulb]
.b4:
    movss   [r8 + L_X], xmm0
    movss   [r8 + L_Y], xmm1
    jmp     .b9
.bn:
    add     r8, L_SZ
    dec     r9d
    jmp     .b1
.b9:
    pop     rax
    ret

; ============================================================================
section .rdata
align 8, db 0
pv_gen:
    times 9 dq pf_fence
    times 2 dq pf_lamp
    times 3 dq pf_bench
    times 3 dq pf_pile
    times 2 dq pf_kennel
    times 2 dq pf_birdh
    times 4 dq pf_cross
    times 3 dq pf_snowman
    dq pf_line
    dq pf_well
%if ($ - pv_gen) != NPROP*8
  %error "pv_gen: не NPROP"
%endif
pv_arg:
    db 0x00, 0x10, 0x20, 0x01, 0x31, 0x02, 0x04, 0x14, 0x05    ; забор
    db 0, 1                                                     ; фонарь
    db 0, 1, 2                                                  ; лавочка
    db 0, 1, 2                                                  ; дрова
    db 0, 1                                                     ; будка
    db 0, 1                                                     ; скворечник
    db 0, 1, 2, 3                                               ; крест
    db 0, 1, 2                                                  ; снеговик
    db 0, 0                                                     ; верёвка, колодец
%if ($ - pv_arg) != NPROP
  %error "pv_arg: не NPROP"
%endif
align 4, db 0
pk_m0_8:    dd -0.8
pk_0_2:     dd 0.2
pk_0_22:    dd 0.22
pk_0_25:    dd 0.25
pk_0_28:    dd 0.28
pk_0_3:     dd 0.3
pk_m0_3:    dd -0.3
pk_0_6:     dd 0.6
pk_0_62:    dd 0.62
pk_0_9:     dd 0.9
pk_0_95:    dd 0.95
pk_m0_12:   dd -0.12
pk_1_05:    dd 1.05
pk_1_12:    dd 1.12
pk_1_15:    dd 1.15
pk_1_25:    dd 1.25
pk_1_3:     dd 1.3
pk_1_6:     dd 1.6
pk_1_9:     dd 1.9
pk_2_1:     dd 2.1
pk_2_3:     dd 2.3
pk_2_5:     dd 2.5
pk_4_2:     dd 4.2
pk_4_5:     dd 4.5
pk_5_2:     dd 5.2
pk_m5_2:    dd -5.2
pk_5_5:     dd 5.5
pk_m5_5:    dd -5.5
pk_3_45:    dd 3.45
pk_m3_45:   dd -3.45
pk_7_5:     dd 7.5
pk_7_6:     dd 7.6
pk_m7_6:    dd -7.6
pk_14_2:    dd 14.2
pk_15_3:    dd 15.3
pk_xmax:    dd 7.901903                 ; середина тайла + 10.2
pk_c:       dd PF_C
pk_wcx:     dd -6.824                   ; сруб колодца: середина (0.3, 0.9 тайла)
pk_wcy:     dd 6.753
pk_base:    dd 0.6015625                ; основание модели, тайлы
pk_bulb:    dd PL_BULB
pk_1_12th:  dd 0.083333333
pk_m12_8:   dd -12.8
pk_26_9:    dd 26.9
pk_lx0:     dd -10.647                  ; верёвка: от столба (0.131, 1.069) к (1.038, 0.163)
pk_ly0:     dd 10.577
pk_ldx:     dd 20.523
pk_ldy:     dd -20.500
pp_bark:    dd 0x5E3E2A, 0xC4BEB2, 0x4E4640 ; кора дров: сосна, берёза, старые
pk_kcol:    dd 0x5E4630, 0x585450          ; будка: бурая, серая
pk_xscl:    dd 1.0, 0.86, 1.0, 1.1          ; кресты
pk_xshx:    dd 0.0, 0.07, 0.0, -0.04

; ---------------------------------------------------------------- таблицы
; фонарь: деревянный столб на бетонном пасынке, изоляторы, кронштейн, корпус лампы,
; снизу — светящееся стекло (огонь — у него, prop_assign)
pt_lamp_a:
    dd 1
    PBOX  0.3, -1.9, -0.8,  0.3, -1.9, 16.5,  0.95, 0.8, 0x8A8C8C, VK_METAL
    PBOX  0.3, -1.9, 5.4,  0.3, -1.9, 6.3,  1.12, 0.97, 0x2E3032, VK_METAL
    PBOX  0.3, -1.9, 12.9,  0.3, -1.9, 13.8,  1.12, 0.97, 0x2E3032, VK_METAL
    PCYL  0.0, 0.0, 5.0,  0.15, 0.1, 54.0,  1.2, 0x4C4238, VK_LOG | (2 << 8)
    PBALL 1.45, 0.1, 47.4, 0.6, 0xB8B6AC, VK_PLAIN
    PBALL 0.1, 1.45, 47.4, 0.6, 0xB8B6AC, VK_PLAIN
    PBALL -1.0, -0.9, 49.2, 0.6, 0xB8B6AC, VK_PLAIN
    PCYL  1.0, 0.1, 48.2,  6.4, 0.1, 51.4,  0.45, 0x3A3C40, VK_METAL
    PCYL  1.0, 0.1, 44.6,  3.6, 0.1, 49.6,  0.32, 0x3A3C40, VK_METAL
    PBOX  4.8, 0.1, 51.3,  10.6, 0.1, 50.2,  1.95, 1.0, 0x5A5E64, VK_METAL
    PBOX  5.5, 0.1, 49.0,  10.1, 0.1, 48.3,  1.85, 1.15, 0xC1FFE0A0, VK_GLOW
    PEND
; старый: столб покосился, лампа в тарелке на гнутой трубе
pt_lamp_b:
    dd 1
    PBOX  0.3, -1.9, -0.8,  0.3, -1.9, 15.5,  0.95, 0.8, 0x828484, VK_METAL
    PBOX  0.3, -1.9, 4.8,  0.3, -1.9, 5.7,  1.12, 0.97, 0x2E3032, VK_METAL
    PBOX  0.3, -1.9, 12.2,  0.3, -1.9, 13.1,  1.12, 0.97, 0x2E3032, VK_METAL
    PCYL  0.0, 0.0, 4.4,  -0.5, 0.3, 53.0,  1.3, 0x3E362E, VK_LOG | (3 << 8)
    PBALL 0.9, 0.3, 47.2, 0.6, 0xB0AEA4, VK_PLAIN
    PBALL -0.3, 1.5, 47.8, 0.6, 0xB0AEA4, VK_PLAIN
    PCYL  0.6, 0.2, 49.6,  4.2, 0.2, 52.4,  0.4, 0x2E3034, VK_METAL
    PCYL  4.2, 0.2, 52.4,  7.5, 0.2, 52.0,  0.4, 0x2E3034, VK_METAL
    PCYL  7.6, 0.2, 52.1,  7.6, 0.2, 50.4,  0.3, 0x2E3034, VK_METAL
    PCYL  7.6, 0.2, 50.8,  7.6, 0.2, 50.1,  2.4, 0x4A5058, VK_METAL
    PCYL  7.6, 0.2, 50.2,  7.6, 0.2, 49.5,  1.7, 0x444A52, VK_METAL
    PBALL 7.6, 0.2, 48.0, 1.55, 0xC1FFE0A0, VK_GLOW
    PEND
; лавочка вдоль x: чурбаки, сиденье в две доски, подушка снега
pt_bench:
    dd 1
    PCYL  -6.8, 0.0, -0.8,  -6.8, 0.0, 7.1,  1.75, 0x4E3E2E, VK_LOG | (2 << 8)
    PCYL  6.8, 0.0, -0.8,  6.8, 0.0, 7.1,  1.75, 0x4E3E2E, VK_LOG | (2 << 8)
    PBOX  -9.6, -1.3, 7.65,  9.6, -1.3, 7.65,  1.25, 0.55, 0x6A5038, VK_WOOD
    PBOX  -9.4, 1.3, 7.7,  9.4, 1.3, 7.7,  1.25, 0.55, 0x644C36, VK_WOOD
    PBOX  -8.6, 0.1, 8.7,  8.2, 0.1, 8.7,  2.05, 0.5, VC_SNOW, VK_SNOW
    PBALL -8.3, 0.5, 8.8, 1.2, VC_SNOW, VK_SNOW
    PBALL 7.8, -0.4, 8.9, 1.1, VC_SNOW, VK_SNOW
    PBALL -1.5, 1.3, 9.1, 0.9, VC_SNOW, VK_SNOW
    PBALL 3.5, -0.9, 9.0, 1.0, VC_SNOW, VK_SNOW
    PEND
pt_bench_bp:
    dd 1
    PBOX  -7.4, 3.0, -0.8,  -7.4, 3.0, 15.6,  0.65, 0.5, 0x5A4430, VK_WOOD
    PBOX  7.4, 3.0, -0.8,  7.4, 3.0, 15.6,  0.65, 0.5, 0x5A4430, VK_WOOD
    PBOX  -9.6, 3.4, 14.0,  9.6, 3.4, 14.0,  0.4, 1.3, 0x6A5038, VK_WOOD
    PBOX  -9.6, 3.4, 11.1,  9.6, 3.4, 11.1,  0.4, 0.95, 0x644C36, VK_WOOD
    PEND
pt_bench_bm:
    dd 1
    PBOX  -7.4, -3.0, -0.8,  -7.4, -3.0, 15.6,  0.65, 0.5, 0x5A4430, VK_WOOD
    PBOX  7.4, -3.0, -0.8,  7.4, -3.0, 15.6,  0.65, 0.5, 0x5A4430, VK_WOOD
    PBOX  -9.6, -3.4, 14.0,  9.6, -3.4, 14.0,  0.4, 1.3, 0x6A5038, VK_WOOD
    PBOX  -9.6, -3.4, 11.1,  9.6, -3.4, 11.1,  0.4, 0.95, 0x644C36, VK_WOOD
    PEND
; колья поленницы
pt_pile:
    dd 1
    PBOX  -13.6, -8.4, -0.8,  -13.6, -8.4, 15.4,  0.55, 0.55, 0x3A3028, VK_WOOD
    PBOX  -13.6, 3.8, -0.8,  -13.6, 3.8, 15.4,  0.55, 0.55, 0x3A3028, VK_WOOD
    PBOX  9.0, -8.4, -0.8,  9.0, -8.4, 15.4,  0.55, 0.55, 0x3A3028, VK_WOOD
    PBOX  9.0, 3.8, -0.8,  9.0, 3.8, 15.4,  0.55, 0.55, 0x3A3028, VK_WOOD
    PEND
; будка поверх стенок: фронтоны, скаты, конёк, лаз, миска, цепь, снег на коньке
pt_kennel:
    dd 1
    PBOX  0.0, -5.2, 10.1,  0.0, 5.2, 10.1,  3.3, 0.6, 0x5A4430, VK_WOOD
    PBOX  0.0, -5.2, 11.3,  0.0, 5.2, 11.3,  2.0, 0.6, 0x5A4430, VK_WOOD
    PBOX  0.0, -5.2, 12.4,  0.0, 5.2, 12.4,  0.9, 0.55, 0x5A4430, VK_WOOD
    PBOXH -2.75, -6.4, 11.35,  -2.75, 6.4, 11.35,  3.9, 0.38,  -0.68, 0.0, 0.733,  0x3E3026, VK_WOOD
    PBOXH 2.75, -6.4, 11.35,  2.75, 6.4, 11.35,  3.9, 0.38,  0.68, 0.0, 0.733,  0x3E3026, VK_WOOD
    PBOX  0.0, -6.5, 13.9,  0.0, 6.5, 13.9,  0.7, 0.4, 0x34281E, VK_WOOD
    PBOX  4.35, 0.0, 0.2,  4.35, 0.0, 6.2,  0.15, 1.8, 0x0C0A08, VK_PLAIN
    PCYL  4.25, 0.0, 6.2,  4.45, 0.0, 6.2,  1.8, 0x0C0A08, VK_PLAIN
    PCYL  8.2, -3.8, -0.4,  8.2, -3.8, 0.9,  1.5, 0x4A5A70, VK_METAL
    PBALL 4.9, 1.9, 0.5, 0.36, 0x6A6E72, VK_PLAIN
    PBALL 5.6, 2.3, 0.35, 0.33, 0x6A6E72, VK_PLAIN
    PBALL 6.3, 2.6, 0.3, 0.33, 0x6A6E72, VK_PLAIN
    PBALL 7.0, 2.8, 0.3, 0.33, 0x6A6E72, VK_PLAIN
    PBALL 7.7, 3.0, 0.3, 0.33, 0x6A6E72, VK_PLAIN
    PBALL 8.4, 3.3, 0.3, 0.33, 0x6A6E72, VK_PLAIN
    PBALL 0.0, -4.0, 14.2, 0.9, VC_SNOW, VK_SNOW
    PBALL 0.0, 1.5, 14.3, 0.8, VC_SNOW, VK_SNOW
    PBALL -3.2, 3.0, 12.3, 0.9, VC_SNOW, VK_SNOW
    PEND
; скворечник на берёзовом шесте: домик, скат, леток, жёрдочка, снег на скате
pt_birdh_a:
    dd 1
    PCYL  0.0, 0.0, -0.8,  0.7, 0.4, 44.0,  0.75, 0xA8A298, VK_LOG | (1 << 8)
    PBOX  0.7, 0.4, 43.4,  0.7, 0.4, 48.6,  1.75, 1.75, 0x6C5236, VK_WOOD
    PBOXH 0.9, -1.9, 49.3,  0.9, 2.7, 49.3,  2.4, 0.28,  0.34, 0.0, 0.94,  0x4A3828, VK_WOOD
    PCYL  2.35, 0.4, 46.6,  2.6, 0.4, 46.6,  0.6, 0x0E0C0A, VK_PLAIN
    PCYL  2.45, 0.4, 45.3,  3.5, 0.4, 45.3,  0.18, 0x5A4A38, VK_PLAIN
    PBALL 1.2, 0.5, 50.1, 1.0, VC_SNOW, VK_SNOW
    PBALL 0.2, -1.0, 50.4, 0.8, VC_SNOW, VK_SNOW
    PEND
pt_birdh_b:
    dd 1
    PCYL  0.0, 0.0, -0.8,  -0.6, 0.6, 41.0,  0.8, 0x6A6258, VK_LOG | (2 << 8)
    PBOX  -0.6, 0.6, 40.4,  -0.6, 0.6, 45.8,  1.6, 1.9, 0x5A4632, VK_WOOD
    PBOXH -0.4, -1.8, 46.5,  -0.4, 3.0, 46.5,  2.3, 0.28,  0.34, 0.0, 0.94,  0x3C3026, VK_WOOD
    PCYL  0.9, 0.6, 43.8,  1.15, 0.6, 43.8,  0.55, 0x0E0C0A, VK_PLAIN
    PBALL -0.2, 0.7, 47.3, 1.0, VC_SNOW, VK_SNOW
    PEND
; крест: столб, верхняя перекладина, главная, косая; снежный холмик
pt_cross:
    dd 1
    PBALL 0.0, 0.0, -3.4, 5.0, VC_SNOW, VK_SNOW
    PBALL 1.6, -1.2, -3.8, 4.2, VC_SNOW, VK_SNOW
    PBOX  0.0, 0.0, -1.0,  0.0, 0.0, 21.0,  0.72, 0.62, 0x3C3026, VK_WOOD
    PBOX  -1.9, 0.0, 18.7,  1.9, 0.0, 18.7,  0.55, 0.5, 0x3C3026, VK_WOOD
    PBOX  -4.6, 0.0, 15.2,  4.6, 0.0, 15.2,  0.58, 0.62, 0x3C3026, VK_WOOD
    PBOX  -2.7, 0.0, 5.1,  2.7, 0.0, 7.7,  0.55, 0.45, 0x3C3026, VK_WOOD
    PEND
pt_cross_roof:
    dd 1
    PBOXH -2.4, -0.8, 21.6,  2.4, -0.8, 21.6,  1.05, 0.25,  0.0, -0.6, 0.8,  0x2E241C, VK_WOOD
    PBOXH -2.4, 0.8, 21.6,  2.4, 0.8, 21.6,  1.05, 0.25,  0.0, 0.6, 0.8,  0x2E241C, VK_WOOD
    PEND
; снеговик: три кома, угольки, морковка, руки-прутья
pt_snowman:
    dd 1
    PBALL 0.0, 0.0, 4.3, 5.3, VC_SNOW, VK_SNOW
    PBALL 0.15, 0.15, 11.2, 3.95, VC_SNOW, VK_SNOW
    PBALL 0.3, 0.3, 16.9, 2.85, VC_SNOW, VK_SNOW
    PBALL 2.55, 1.05, 17.7, 0.45, 0x141414, VK_PLAIN
    PBALL 1.05, 2.55, 17.7, 0.45, 0x141414, VK_PLAIN
    PBALL 2.9, 2.9, 12.3, 0.45, 0x181818, VK_PLAIN
    PBALL 2.95, 2.95, 10.4, 0.45, 0x181818, VK_PLAIN
    PBALL 2.3, 2.3, 16.8, 0.62, 0xE07830, VK_PLAIN
    PBALL 2.85, 2.85, 16.7, 0.48, 0xD06828, VK_PLAIN
    PBALL 3.35, 3.35, 16.6, 0.34, 0xB85820, VK_PLAIN
    PBALL 2.41, -2.11, 12.4, 0.34, 0x402C1C, VK_TWIG
    PBALL 2.90, -2.60, 12.8, 0.32, 0x402C1C, VK_TWIG
    PBALL 3.38, -3.08, 13.2, 0.32, 0x402C1C, VK_TWIG
    PBALL 3.87, -3.57, 13.6, 0.31, 0x402C1C, VK_TWIG
    PBALL 4.35, -4.05, 14.0, 0.3, 0x402C1C, VK_TWIG
    PBALL 4.84, -4.54, 14.4, 0.29, 0x402C1C, VK_TWIG
    PBALL 5.32, -5.02, 14.8, 0.28, 0x402C1C, VK_TWIG
    PBALL 5.81, -5.51, 15.2, 0.27, 0x402C1C, VK_TWIG
    PBALL 6.3, -5.6, 16.0, 0.24, 0x402C1C, VK_TWIG
    PBALL 6.5, -6.2, 15.3, 0.24, 0x402C1C, VK_TWIG
    PBALL -2.11, 2.41, 12.4, 0.34, 0x402C1C, VK_TWIG
    PBALL -2.60, 2.90, 12.71, 0.32, 0x402C1C, VK_TWIG
    PBALL -3.08, 3.38, 13.03, 0.32, 0x402C1C, VK_TWIG
    PBALL -3.57, 3.87, 13.34, 0.31, 0x402C1C, VK_TWIG
    PBALL -4.05, 4.35, 13.66, 0.3, 0x402C1C, VK_TWIG
    PBALL -4.54, 4.84, 13.97, 0.29, 0x402C1C, VK_TWIG
    PBALL -5.02, 5.32, 14.29, 0.28, 0x402C1C, VK_TWIG
    PBALL -5.51, 5.81, 14.6, 0.27, 0x402C1C, VK_TWIG
    PBALL -6.2, 6.0, 15.3, 0.24, 0x402C1C, VK_TWIG
    PBALL -5.8, 6.5, 14.4, 0.24, 0x402C1C, VK_TWIG
    PEND
pt_sm_bucket:
    dd 1
    PCYL  0.25, 0.2, 19.0,  0.55, 0.4, 22.5,  2.05, 0x587090, VK_METAL
    PEND
pt_sm_scarf:
    dd 1
    PBALL 2.70, 0.20, 14.5, 0.85, 0xA02820, VK_PLAIN
    PBALL 2.22, 1.67, 14.5, 0.85, 0xA02820, VK_PLAIN
    PBALL 0.97, 2.58, 14.5, 0.85, 0xA02820, VK_PLAIN
    PBALL -0.57, 2.58, 14.5, 0.85, 0xA02820, VK_PLAIN
    PBALL -1.82, 1.67, 14.5, 0.85, 0xA02820, VK_PLAIN
    PBALL -2.30, 0.20, 14.5, 0.85, 0xA02820, VK_PLAIN
    PBALL -1.82, -1.27, 14.5, 0.85, 0xA02820, VK_PLAIN
    PBALL -0.57, -2.18, 14.5, 0.85, 0xA02820, VK_PLAIN
    PBALL 0.97, -2.18, 14.5, 0.85, 0xA02820, VK_PLAIN
    PBALL 2.22, -1.27, 14.5, 0.85, 0xA02820, VK_PLAIN
    PBALL 2.5, 1.2, 13.1, 0.72, 0x8C2018, VK_PLAIN
    PBALL 2.7, 1.3, 12.0, 0.62, 0x8C2018, VK_PLAIN
    PEND
; бельевая верёвка: столбы и перекладины (в тайлах: как фигуры столкновения)
pt_line:
    dd 0
    PCYL  0.131, 1.069, -0.8,  0.131, 1.069, 28.4,  0.95, 0x4E4840, VK_LOG | (2 << 8)
    PCYL  1.038, 0.163, -0.8,  1.038, 0.163, 28.4,  0.95, 0x4E4840, VK_LOG | (2 << 8)
    PBOX  0.069, 1.007, 27.6,  0.193, 1.131, 27.6,  0.4, 0.45, 0x4A4238, VK_WOOD
    PBOX  0.976, 0.101, 27.6,  1.100, 0.225, 27.6,  0.4, 0.45, 0x4A4238, VK_WOOD
    PBALL 0.131, 1.069, 28.8, 0.9, VC_SNOW, VK_SNOW
    PBALL 1.038, 0.163, 28.8, 0.9, VC_SNOW, VK_SNOW
    PEND
; колодец поверх сруба (в тайлах): полкрышки, зев, стояк с развилкой и осью, очеп,
; противовес, шест, ведро; снег на крышке и на стояке
pt_well:
    dd 0
    PBOX  0.132, 0.748, 10.95,  0.468, 0.748, 10.95,  0.62, 0.3, 0x5A4C3C, VK_WOOD
    PBOX  0.132, 0.803, 11.0,  0.468, 0.803, 11.0,  0.62, 0.3, 0x544636, VK_WOOD
    PBOX  0.132, 0.858, 10.95,  0.468, 0.858, 10.95,  0.62, 0.3, 0x5A4C3C, VK_WOOD
    PBOX  0.140, 0.806, 11.7,  0.450, 0.806, 11.7,  1.7, 0.45, VC_SNOW, VK_SNOW
    PBOX  0.300, 0.975, 7.5,  0.300, 0.975, 9.4,  2.3, 1.55, 0x080A0C, VK_PLAIN
    PCYL  0.834, 0.366, -0.8,  0.834, 0.366, 35.4,  1.35, 0x4A4036, VK_LOG | (2 << 8)
    PCYL  0.834, 0.366, 34.6,  0.806, 0.338, 39.4,  0.55, 0x4A4036, VK_LOG | (2 << 8)
    PCYL  0.834, 0.366, 34.6,  0.862, 0.394, 39.4,  0.55, 0x4A4036, VK_LOG | (2 << 8)
    PCYL  0.797, 0.329, 37.6,  0.871, 0.403, 37.6,  0.3, 0x2A2C30, VK_METAL
    PCYL  1.074, 0.094, 28.42,  0.278, 0.990, 58.8,  0.7, 0x5A5046, VK_LOG | (2 << 8)
    PCYL  1.052, 0.119, 29.0,  1.097, 0.068, 27.8,  1.8, 0x3E3630, VK_LOG | (3 << 8)
    PCYL  0.300, 0.966, 57.6,  0.300, 0.966, 21.3,  0.33, 0x4A3E32, VK_WOOD
    PCYL  0.300, 0.966, 17.6,  0.300, 0.966, 21.3,  1.75, 0x586270, VK_METAL
    PBALL 0.834, 0.366, 35.6, 1.1, VC_SNOW, VK_SNOW
    PEND

section .data
align 4, db 0
pv_a:       dd 0
pf_cpk:     dd 0
pf_crl:     dd 0
pf_cps:     dd 0
pr_x0:      dd 0.0
pr_y0:      dd 0.0
pr_dx:      dd 0.0
pr_dy:      dd 0.0
pr_nx:      dd 0.0
pr_ny:      dd 0.0
pr_z:       dd 0.0
pr_t:       dd 0.0
pr_h:       dd 0.0
pr_ld:      dd 0.0
pr_ln:      dd 0.0
pr_step:    dd 0.0
pp_col:     dd 0
pp_kind:    dd 0
pp_x:       dd 0.0
pp_z:       dd 0.0
section .text
