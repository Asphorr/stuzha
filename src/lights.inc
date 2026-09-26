; ----------------------------------------------------------------------------
;  lights.inc — свет огней попиксельно
; ----------------------------------------------------------------------------
; У огня — точка в мире (x, y и высота над снегом), и каждый пиксель G-буфера
; считает его свет сам: затухание по расстоянию в 3D, N·L с мягкой обёрткой по
; нормали, тень — по карте горизонта огня (снята на новую игру с карты высот: с
; какой высоты над снегом в этом месте огонь уже виден; полутень шире вдали от
; заслона) и от фигур рядом (капсулы на отрезке пиксель — огонь). Фонарь светит
; из-под колпака — вниз и вбок, выше головы нет; лампочка в избе светит только
; своё, а наружу — сквозь окна: луч от неё через пиксель пересекает плоскость
; стены, и маска рамы (переплёт, шторы, кот или герань на подоконнике) кладёт на
; снег пятно окна, дальний край — размытее. Печь светит из топки вперёд и дрожит,
; телевизор мерцает; фонарик — прожектор из руки. Снег под огнями подсвечивает
; стены и фигуры снизу (сетка lgst).
; Строку считает dl_row по 4 пикселя (SSE2, её зовёт light_rows) или dl_row8 по 8
; (AVX2, lightx.inc, зовёт light_rows_x) — те же операции по дорожкам, кадр совпадает
; до байта — и кладёт в dlb словами B, G, R, 0. После освещения — тоновая кривая
; tm_lut: пересвет плавно уходит к 255, а не в белое.

AL_MAX      equ 64                  ; огней на кадр (бит в маске плитки)
AL_SZ       equ 864
AL_X        equ 0                   ; точка огня, мир 8.8 (f32); у окна — середина стекла снаружи
AL_Y        equ 4
AL_Z        equ 8                   ; высота, px
AL_IR2      equ 12                  ; 1 / R² отсечения (R в px-единицах: тайл = 19.6)
AL_COL      equ 16                  ; цвет B, G, R, 0 (f32, с множителем кадра)
AL_KIND     equ 32
AL_HID      equ 36                  ; дом + 1: огонь внутри светит только его (0 — всем)
AL_NF       equ 40                  ; фигур рядом
AL_IRG      equ 44                  ; окно: 1 / R² свечения стекла
AL_HM       equ 48                  ; карта горизонта (адрес) или 0
AL_HMX      equ 56                  ; её начало, сэмплы (4·AL_HSC на тайл)
AL_HMY      equ 60
AL_HMW      equ 64                  ; сторона
AL_WAX      equ 68                  ; окно: 0 — плоскость y = const, 1 — x = const
AL_WPC      equ 72                  ; плоскость стены снаружи, мир 8.8
AL_WSG      equ 76                  ; знак «наружу»
AL_WU0      equ 80                  ; вдоль стены: где wu = 0, мир 8.8
AL_WUD      equ 84                  ; 1 / шаг wu
AL_WMS      equ 88                  ; маска рамы (адрес): чёткая, +256 — размытая
AL_BX       equ 96                  ; окно: лампочка своей избы, мир 8.8, высота px
AL_BY       equ 100
AL_BZ       equ 104
AL_BD2      equ 108                 ; квадрат расстояния лампочка — окно
AL_PCOL     equ 112                 ; цвет сквозь раму B, G, R, 0
AL_IRB      equ 128                 ; 1 / R² пятна (от лампочки)
AL_WHID     equ 132                 ; окно: своя изба + 1 (её оно не светит)
AL_FIG      equ 136                 ; фигуры рядом: x, y, рамка тени x0, x1, y0, y1 (мир 8.8, f32)
AL_FSZ      equ 32
AL_FMAX     equ 6
; для dl_row и dl_row8 — то же, размноженное на 8 дорожек (dl_row берёт первые 4)
AL_XV       equ 352                 ; точка огня
AL_YV       equ 384
AL_ZV       equ 416
AL_IR2V     equ 448                 ; 1 / R² отсечения
AL_IRAV     equ 480                 ; 1 / R² затухания (у окна — свечения стекла)
AL_CB       equ 512                 ; цвет
AL_CG       equ 544
AL_CR       equ 576
AL_LDX      equ 608                 ; лепесток: clamp((ось · к огню)/d · k + b)
AL_LDY      equ 640
AL_LDZ      equ 672
AL_LK       equ 704
AL_LB       equ 736
AL_HIDV     equ 768                 ; изба
AL_R2       equ 800                 ; R² отсечения — плиткам
AL_R2G      equ 804                 ; окно: R² свечения стекла
AL_EX1      equ 808                 ; окно: клин пятна — от лампочки к краям стекла (8.8)
AL_EY1      equ 812
AL_EX2      equ 816
AL_EY2      equ 820
AL_ESG      equ 824                 ; знак: край 1 -> край 2 против часовой или по
AL_HT       equ 832                 ; пороги карты горизонта, 4 на тайл (адрес), если есть карта
AL_HSC      equ 840                 ; f32 сэмплов карты на четверть тайла
AL_HCX      equ 844                 ; пороги: начало, клетки
AL_HCY      equ 848
AL_HCW      equ 852                 ; сторона

DT_W        equ SCR_W/16            ; маски огней по плиткам 16x16 px
DT_H        equ (SCR_H + 15)/16
DL_FGMAX    equ 64                  ; фигур на кадр
HZ_POOL     equ 6*1024*1024         ; дворды карт горизонта
TM_N        equ 2048                ; тоновая кривая: вход 0..2047
TM_K        equ 200                 ; колено: ниже — как есть
WM_W        equ 11                  ; маска рамы: wu −1..9
WM_H        equ 21                  ;             wh −1..19

; 4 строки xmm4..7 (дорожки: a, b, c, d) -> столбцы: xmm2 = 0-е, xmm1 = 1-е,
; xmm3 = 2-е, xmm6 = 3-е (портит xmm0, xmm4)
%macro TR4 0
    movaps  xmm0, xmm4
    unpcklps xmm0, xmm5                 ; a0 b0 a1 b1
    movaps  xmm1, xmm6
    unpcklps xmm1, xmm7                 ; c0 d0 c1 d1
    unpckhps xmm4, xmm5                 ; a2 b2 a3 b3
    unpckhps xmm6, xmm7                 ; c2 d2 c3 d3
    movaps  xmm2, xmm0
    movlhps xmm2, xmm1                  ; a0 b0 c0 d0
    movhlps xmm1, xmm0                  ; a1 b1 c1 d1
    movaps  xmm3, xmm4
    movlhps xmm3, xmm6                  ; a2 b2 c2 d2
    movhlps xmm6, xmm4                  ; a3 b3 c3 d3
%endmacro

; угол 2x2 карты горизонта (%1: горизонт + 64 | 65536/полутень << 16 по дорожкам),
; xmm4 = (z + 64) << 16 -> %1 = 128 + (z − hz)·ip >> 8, не прижато (портит xmm0):
; (z − hz) << 16 на pmaddwd с самим углом — старшее слово на ip, младшее на 0
%macro HZT 1
    movdqa  xmm0, %1
    pslld   xmm0, 16
    movdqa  xmm7, xmm4
    psubd   xmm7, xmm0
    pmaddwd xmm7, %1
    psrad   xmm7, 8
    paddd   xmm7, [lt_d128]
    movdqa  %1, xmm7
%endmacro

; канал отсвета (байт со сдвигом %1) из углов lgst xmm2 = c00, xmm1 = c10, xmm3 = c01,
; xmm6 = c11 билинейно — по x (c0·(64 − fx) + c1·fx) >> 6, по y так же (как light_rows) —
; и · xmm8 -> xmm0. xmm9, xmm10 — веса x, y (64 − f | f << 16), xmm11 = 0xFF,
; xmm12 = 0xFF0000 (портит xmm4, xmm5)
%macro BNC1 1
    movdqa  xmm0, xmm2
%if %1
    psrld   xmm0, %1
%endif
    pand    xmm0, xmm11
    movdqa  xmm4, xmm1
%if %1 < 16
    pslld   xmm4, 16 - %1
%endif
    pand    xmm4, xmm12
    por     xmm0, xmm4
    pmaddwd xmm0, xmm9
    psrld   xmm0, 6                     ; ряд 0
    movdqa  xmm5, xmm3
%if %1
    psrld   xmm5, %1
%endif
    pand    xmm5, xmm11
    movdqa  xmm4, xmm6
%if %1 < 16
    pslld   xmm4, 16 - %1
%endif
    pand    xmm4, xmm12
    por     xmm5, xmm4
    pmaddwd xmm5, xmm9
    pslld   xmm5, 10
    pand    xmm5, xmm12                 ; (ряд 1 >> 6) << 16
    por     xmm0, xmm5
    pmaddwd xmm0, xmm10
    psrld   xmm0, 6
    cvtdq2ps xmm0, xmm0
    mulps   xmm0, xmm8
%endmacro

; %1 *= [%2] по дорожкам, младшие 32 бита (как imul; в SSE2 нет pmulld) — портит xmm6, xmm7
%macro MUL32 2
    movdqa  xmm6, %1
    psrlq   xmm6, 32
    movdqa  xmm7, [%2]
    pmuludq %1, xmm7
    pmuludq xmm6, xmm7
    pshufd  %1, %1, 0x08
    pshufd  xmm6, xmm6, 0x08
    punpckldq %1, xmm6
%endmacro

; вектор констант: 8 дорожек (dl_row берёт первые 4, dl_row8 — все)
%macro V4 2
%1: times 8 dd %2
%endmacro

section .rdata
align 32, db 0
V4 lt_pxu4, 0.0766
V4 lt_one4, 1.0
V4 lt_zero4, 0.0
V4 lt_1e4v, 0.0001
V4 lt_big4, 1.0e9
V4 lt_mbig4, -1.0e9
V4 lt_16v, 16.0
V4 lt_0_02v, 0.02
V4 lt_figh4, 27.0
V4 lt_m2v, -2.0
V4 lt_100v, 100.0
V4 lt_0_05v, 0.05
V4 lt_1_2v, 1.2
V4 lt_figr4, 3.9
V4 lt_0_25v, 0.25
V4 lt_0_5v, 0.5
V4 lt_figkv, 0.88
V4 lt_6v, 6.0
V4 lt_wmaxav, 9.999
V4 lt_wmaxbv, 19.999
V4 lt_handz4, 20.0
V4 lt_fwrap4, 0.45
V4 lt_fwk4, 0.6896552
V4 lt_0_86v, 0.86
V4 lt_14v, 14.0
V4 lt_0_965v, 0.965
V4 lt_40v, 40.0
V4 lt_0_35v, 0.35
V4 lt_0_915v, 0.915
V4 lt_50v, 50.0
V4 lt_0_12v, 0.12
V4 lt_3v, 3.0
V4 lt_0_07v, 0.07
V4 lt_i256v, 0.00390625
V4 lt_0_03v, 0.03
V4 lt_0_85v, 0.85
V4 lt_fk4, 5.73e-5
V4 lt_i196v, 0.0510204
V4 lt_i4096v, 9.5751e-7
V4 lt_flb, 0.82
V4 lt_flg, 0.94
V4 lt_flr, 1.0
V4 lt_i72v, 0.0138889
V4 lt_i1mv, 9.5367432e-7
V4 lt_vxy4, 0.6113
V4 lt_vz4, 0.4989
V4 lt_10v, 10.0
V4 lt_btan4, 0.5
V4 lt_bend4, 0.0127
V4 lt_dglim, GLIM
V4 lt_dgw, GW
V4 lt_d80, 0x80
V4 lt_d3f, 0x7F                         ; номер нормали без NOWN
V4 lt_d63, 63
V4 lt_d64, 64
V4 lt_d128, 128
V4 lt_dff, 0xFF
V4 lt_dff0000, 0xFF0000
V4 lt_dndy1, N_DY - 1
V4 lt_dnsm, N_SM
V4 lt_d199, 199
V4 lt_d5, 5
V4 lt_hx, 0x27d4eb2d                    ; множители HASH
V4 lt_hy, 0x165667b1
V4 lt_hm, 0x2c1b3c6d
V4 lt_2_1v, 2.1
V4 lt_absm, 0x7FFFFFFF
lt_w256:    times 16 dw 256
w_lmax:     dw 1023, 1023, 1023, 255, 1023, 1023, 1023, 255
w_2047:     times 8 dw 2047
col_fire:   dd 26.0, 96.0, 235.0, 0.0           ; топка: B, G, R
lt_projk:   dd 4.0, 4.0, 4.0, 0.0               ; сила пятна окна (у самой рамы)
lt_projtv:  dd 2.0, 2.0, 2.0, 0.0               ; и телевизора в нём
; обёртка N·L по нормали: грани резче, фигуры и спрайты мягче (картинка, а не плоскость)
lt_nwrap:
    dd 0.1                          ; 0 вверх
    dd 0.1, 0.1                     ; 1, 2 грани
    dd 0.2, 0.2, 0.2, 0.2           ; 3-6 брёвна
    dd 0.6, 0.6, 0.6                ; 7-9 фигуры, трубы
    times 12 dd 0.15                ; 10-21 скаты
    dd 0.1, 0.1                     ; 22, 23 сугробы у стен
    times 24 dd 0.1                 ; 24-47 рельеф снега
    dd 0.6, 0.6, 0.6                ; 48-50 мелкие спрайты
    dd 0.7, 0.7, 0.7                ; 51-53 крупные
    times NAZ*NEL + 1 dd 0.4        ; сфера: хвоя и одежда — не зеркало, свет затекает
%if ($ - lt_nwrap) != NN*4
  %error "lt_nwrap: не NN нормалей"
%endif
; отсвет снега снизу: сколько грань видит освещённого снега под собой
lt_nbnc:
    dd 0.0
    dd 0.34, 0.34
    dd 0.14, 0.42, 0.14, 0.42
    dd 0.34, 0.34, 0.34
    times 12 dd 0.0
    dd 0.06, 0.06
    times 24 dd 0.0
    dd 0.3, 0.3, 0.3
    dd 0.22, 0.22, 0.22
    times NAZ dd 0.5                ; сфера: смотрит вниз — видит снег под собой
    times NAZ dd 0.4
    times NAZ dd 0.26
    times NAZ dd 0.12
    times NAZ dd 0.03
    dd 0.0
%if ($ - lt_nbnc) != NN*4
  %error "lt_nbnc: не NN нормалей"
%endif
; по виду огня (комната, окно, фонарь, сломанный, ТВ, печь): высота, радиус (× L_R), сила
lt_kz:      dd 30.0, 16.0, 40.0, 40.0, 12.0, 6.0
lt_kr:      dd 1.3, 1.0, 1.45, 1.45, 0.8, 1.0
lt_ki:      dd 1.0, 0.7, 1.35, 1.35, 0.8, 1.0
lt_tmk:     dd 200.0
lt_tms:     dd 55.0
lt_tmis:    dd 0.0181818
lt_inv6:    dd 0.1666667
lt_inv24:   dd 0.0416667
lt_ipxu:    dd 13.0548                  ; px-единицы -> 8.8
lt_0884:    dd 0.0884                   ; √2/16
lt_0442:    dd 0.0442                   ; √2/32
lt_cx0:     dd 320.0
lt_cy0:     dd 196.0
lt_zmax:    dd 100.0
lt_capk:    dd 2.857                    ; колпак: 1/0.35
lt_capb:    dd 0.714                    ; 0.25/0.35
lt_m1_2:    dd -1.2
lt_figh:    dd 31.0                     ; рост с поднятыми руками
lt_0_02:    dd 0.02
lt_wrb:     dd 200.0                    ; пятно окна: R от лампочки, px-единицы
lt_wrc:     dd 6.5                      ; окно: радиус отсечения, тайлы
lt_ftilt:   dd -0.18                    ; фонарик — под ноги и вдаль: ось на 10° вниз
lt_handz:   dd 20.0
lt_cmin:    dd 3.0
lt_blur:    dd 0.15
lt_fl1:     dd 7.3
lt_fl2:     dd 13.1
lt_fl3:     dd 23.7
lt_fa1:     dd 0.14
lt_fa2:     dd 0.09
lt_fa3:     dd 0.05
lt_fa0:     dd 0.72
lt_hmsz:    dd 3.0, 10.0                ; полутень: размер огня (фонарь, окно), px

section .text

; ---------------------------------------------------------------- один раз
; тоновая кривая, 1 / (1 + обёртка) по нормалям, маски рам окон
dl_init:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40
    ; ----- тоновая кривая без поправки глаза (grade.inc)
    movss   xmm0, [f_1]
    call    tm_build
    ; ----- по нормали: обёртка, 1 / (1 + обёртка), отсвет снега, сдвиг выборки тени
    ; (msd·256: 8.8 на единичный вектор к огню)
    lea     rsi, [lt_nwrap]
    lea     r8, [dl_nw]
    lea     r9, [lt_nbnc]
    lea     r10, [msd]
    xor     ebx, ebx
.nw:
    movss   xmm1, [f_1]
    addss   xmm1, [rsi + rbx*4]
    movss   xmm0, [f_1]
    divss   xmm0, xmm1
    mov     eax, ebx
    shl     eax, 4
    movss   xmm1, [rsi + rbx*4]
    movss   [r8 + rax], xmm1
    movss   [r8 + rax + 4], xmm0
    movss   xmm1, [r9 + rbx*4]
    movss   [r8 + rax + 8], xmm1
    movss   xmm1, [r10 + rbx*4]
    mulss   xmm1, [f_256]
    movss   [r8 + rax + 12], xmm1
    inc     ebx
    cmp     ebx, NN
    jb      .nw
    ; ----- маски рам: пусто / кот / герань; чёткая, +256 — размытая 3x3.
    ; Как shade_side: рама и переплёт (wu 4, wh 9) — 0, шторы (wu 2, 6) — 100,
    ; стекло — 255, силуэт на подоконнике (wh 2..6, wu 2..6) — 0
    xor     r12d, r12d
.wm:
    imul    edi, r12d, 512
    lea     rax, [wm_buf]
    add     rdi, rax
    xor     ebx, ebx                    ; b = wh + 1
.wb:
    xor     esi, esi                    ; a = wu + 1
.wa:
    lea     eax, [esi - 1]              ; wu
    lea     ecx, [ebx - 1]              ; wh
    xor     edx, edx
    cmp     eax, 2
    jl      .wput
    cmp     eax, 6
    jg      .wput
    cmp     ecx, 2
    jl      .wput
    cmp     ecx, 16
    jg      .wput
    cmp     eax, 4
    je      .wput
    cmp     ecx, 9
    je      .wput
    mov     edx, 255
    cmp     eax, 2
    je      .curt
    cmp     eax, 6
    jne     .sil
.curt:
    mov     edx, 100
.sil:
    test    r12d, r12d
    jz      .wput
    lea     r8d, [ecx - 2]
    cmp     r8d, 4
    ja      .wput
    lea     r9d, [eax - 2]
    cmp     r12d, 1
    jne     .ger
    lea     r10, [cat_rows]
    movzx   r10d, byte [r10 + r8]
    bt      r10d, r9d
    jnc     .wput
    xor     edx, edx
    jmp     .wput
.ger:
    lea     r10, [ger_pot]
    movzx   r11d, byte [r10 + r8]
    lea     r10, [ger_leaf]
    movzx   r10d, byte [r10 + r8]
    or      r11d, r10d
    lea     r10, [ger_flow]
    movzx   r10d, byte [r10 + r8]
    or      r11d, r10d
    bt      r11d, r9d
    jnc     .wput
    xor     edx, edx
.wput:
    imul    eax, ebx, WM_W
    add     eax, esi
    mov     [rdi + rax], dl
    inc     esi
    cmp     esi, WM_W
    jb      .wa
    inc     ebx
    cmp     ebx, WM_H
    jb      .wb
    xor     ebx, ebx
.bb:
    xor     esi, esi
.ba:
    xor     r11d, r11d
    mov     r8d, -1
.by:
    lea     r10d, [ebx + r8d]
    cmp     r10d, WM_H
    jae     .byn
    mov     r9d, -1
.bx:
    lea     ecx, [esi + r9d]
    cmp     ecx, WM_W
    jae     .bxn
    imul    eax, r10d, WM_W
    add     eax, ecx
    movzx   eax, byte [rdi + rax]
    add     r11d, eax
.bxn:
    inc     r9d
    cmp     r9d, 1
    jle     .bx
.byn:
    inc     r8d
    cmp     r8d, 1
    jle     .by
    imul    eax, r11d, 7282             ; /9
    shr     eax, 16
    imul    ecx, ebx, WM_W
    add     ecx, esi
    mov     [rdi + rcx + 256], al
    inc     esi
    cmp     esi, WM_W
    jb      .ba
    inc     ebx
    cmp     ebx, WM_H
    jb      .bb
    inc     r12d
    cmp     r12d, 3
    jb      .wm
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- на новую игру
; после calc_static_light: точки огней, их избы, рамы окон, лампочки изб для
; окон, карты горизонта фонарей и окон
dl_bake:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    mov     dword [hz_used], 0
    ; ----- проход 1: точка, изба, рама
    lea     r12, [lights]
    mov     r13d, [nlights]
.p1:
    test    r13d, r13d
    jz      .p1d
    mov     eax, [r12 + L_KIND]
    movss   xmm0, [r12 + L_X]
    mulss   xmm0, [f_256]
    movss   [r12 + L_GX], xmm0
    movss   xmm0, [r12 + L_Y]
    mulss   xmm0, [f_256]
    movss   [r12 + L_GY], xmm0
    lea     rdx, [lt_kz]
    movss   xmm0, [rdx + rax*4]
    movss   [r12 + L_GZ], xmm0
    cmp     eax, LK_ROOM
    je      .room
    cmp     eax, LK_WIN
    je      .win
    cmp     eax, LK_TV
    je      .tile
    cmp     eax, LK_STOVE
    je      .tile
    jmp     .p1n
.room:
    cvttss2si ecx, [r12 + L_X]
    cvttss2si edx, [r12 + L_Y]
    call    .hid
    mov     [r12 + L_HID], eax
    jmp     .p1n
.tile:
    mov     ecx, [r12 + L_TX]
    mov     edx, [r12 + L_TY]
    call    .hid
    mov     [r12 + L_HID], eax
    jmp     .p1n
.win:
    mov     ecx, [r12 + L_TX]
    mov     edx, [r12 + L_TY]
    call    .hid
    mov     [r12 + L_HID], eax
    mov     eax, [r12 + L_TY]
    shl     eax, 7
    add     eax, [r12 + L_TX]
    lea     rdx, [hflags]
    movzx   eax, byte [rdx + rax]
    shr     eax, 1
    and     eax, 1
    mov     [r12 + L_TV], eax           ; в избе смотрят телевизор
    mov     eax, [r12 + L_DY]
    test    eax, eax
    jz      .wx
    ; стена вдоль x: плоскость y = const; u = x, wu = (x − (tx·256 + 64)) / 16
    mov     dword [r12 + L_WAX], 0
    mov     ecx, [r12 + L_TY]
    test    eax, eax
    js      .wy1
    inc     ecx
.wy1:
    shl     ecx, 8
    cvtsi2ss xmm0, ecx
    movss   [r12 + L_WPC], xmm0
    cvtsi2ss xmm1, eax
    movss   [r12 + L_WSG], xmm1
    mov     ecx, [r12 + L_TX]
    shl     ecx, 8
    lea     edx, [ecx + 64]
    cvtsi2ss xmm2, edx
    movss   [r12 + L_WU0], xmm2
    mov     dword [r12 + L_WUD], __float32__(0.0625)
    add     ecx, 128
    cvtsi2ss xmm2, ecx
    movss   [r12 + L_GX], xmm2          ; середина стекла, чуть снаружи
    mulss   xmm1, [f_8]
    addss   xmm1, xmm0
    movss   [r12 + L_GY], xmm1
    xor     r8d, r8d                    ; грань 0
    jmp     .wpat
.wx:
    ; стена вдоль y: плоскость x = const; u = y, wu = (ty·256 + 184 − y) / 16
    mov     eax, [r12 + L_DX]
    mov     dword [r12 + L_WAX], 1
    mov     ecx, [r12 + L_TX]
    test    eax, eax
    js      .wx1
    inc     ecx
.wx1:
    shl     ecx, 8
    cvtsi2ss xmm0, ecx
    movss   [r12 + L_WPC], xmm0
    cvtsi2ss xmm1, eax
    movss   [r12 + L_WSG], xmm1
    mov     ecx, [r12 + L_TY]
    shl     ecx, 8
    lea     edx, [ecx + 184]
    cvtsi2ss xmm2, edx
    movss   [r12 + L_WU0], xmm2
    mov     dword [r12 + L_WUD], __float32__(-0.0625)
    add     ecx, 120
    cvtsi2ss xmm2, ecx
    movss   [r12 + L_GY], xmm2
    mulss   xmm1, [f_8]
    addss   xmm1, xmm0
    movss   [r12 + L_GX], xmm1
    mov     r8d, 1                      ; грань 1
.wpat:
    ; силуэт на подоконнике — как в shade_side: (семя тайла + грань)·0x2C1B3C6D >> 28
    mov     eax, [r12 + L_TX]
    mov     ecx, [r12 + L_TY]
    HASH    eax, ecx
    add     eax, r8d
    imul    eax, eax, 0x2C1B3C6D
    shr     eax, 28
    xor     ecx, ecx
    cmp     eax, 1
    jbe     .wcat
    cmp     eax, 5
    ja      .wset
    mov     ecx, 2
    jmp     .wset
.wcat:
    mov     ecx, 1
.wset:
    mov     [r12 + L_WMS], ecx
.p1n:
    add     r12, L_SZ
    dec     r13d
    jmp     .p1
.p1d:
    ; ----- проход 2: окну — лампочка своей избы
    lea     r12, [lights]
    mov     r13d, [nlights]
.p2:
    test    r13d, r13d
    jz      .p2d
    cmp     dword [r12 + L_KIND], LK_WIN
    jne     .p2n
    ; запас: на тайл вглубь от середины окна, под потолком
    movss   xmm0, [r12 + L_WSG]
    mulss   xmm0, [lt_m384]
    movss   xmm1, [r12 + L_GX]
    movss   xmm2, [r12 + L_GY]
    cmp     dword [r12 + L_WAX], 0
    jne     .p2a
    addss   xmm2, xmm0
    jmp     .p2b
.p2a:
    addss   xmm1, xmm0
.p2b:
    movss   [r12 + L_BX], xmm1
    movss   [r12 + L_BY], xmm2
    mov     dword [r12 + L_BZ], __float32__(30.0)
    mov     eax, [r12 + L_HID]
    lea     rsi, [lights]
    mov     ecx, [nlights]
.p2f:
    cmp     dword [rsi + L_KIND], LK_ROOM
    jne     .p2fn
    cmp     [rsi + L_HID], eax
    jne     .p2fn
    movss   xmm0, [rsi + L_GX]
    movss   [r12 + L_BX], xmm0
    movss   xmm0, [rsi + L_GY]
    movss   [r12 + L_BY], xmm0
    movss   xmm0, [rsi + L_GZ]
    movss   [r12 + L_BZ], xmm0
    jmp     .p2n
.p2fn:
    add     rsi, L_SZ
    dec     ecx
    jnz     .p2f
.p2n:
    add     r12, L_SZ
    dec     r13d
    jmp     .p2
.p2d:
    ; ----- проход 3: карты горизонта фонарей (12 сэмплов на тайл) и окон (8) по hm16
    mov     dword [hzt_used], 0
    lea     rax, [hm16]
    mov     [pm_fld], rax
    lea     r12, [lights]
    mov     r13d, [nlights]
.p3:
    test    r13d, r13d
    jz      .done
    mov     dword [r12 + L_HM], 0
    mov     eax, [r12 + L_KIND]
    cmp     eax, LK_LAMP
    je      .p3l
    cmp     eax, LK_BROKEN
    je      .p3l
    cmp     eax, LK_WIN
    jne     .p3n
    mov     dword [pm_h], __float32__(16.0)
    mov     dword [pm_skip], 2
    movss   xmm0, [lt_hmsz + 4]
    movss   [pm_sz], xmm0
    mov     dword [pm_half], 1
    mov     dword [pm_f], 2
    movss   xmm0, [lt_wrc]
    jmp     .p3m
.p3l:
    mov     dword [pm_h], __float32__(40.0)
    mov     dword [pm_skip], 5              ; кронштейн и колпак; свой столб тенит
    movss   xmm0, [lt_hmsz]
    movss   [pm_sz], xmm0
    mov     dword [pm_half], 0
    mov     dword [pm_f], 3
    movss   xmm0, [r12 + L_R]
    mulss   xmm0, [lt_kr + LK_LAMP*4]
.p3m:
    call    hz_bake
.p3n:
    add     r12, L_SZ
    dec     r13d
    jmp     .p3
.done:
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
; ecx, edx = тайл -> eax = дом + 1 (rmap)
.hid:
    xor     eax, eax
    cmp     ecx, MAPW
    jae     .hid0
    cmp     edx, MAPW
    jae     .hid0
    shl     edx, 7
    add     edx, ecx
    lea     rax, [rmap]
    movzx   eax, byte [rax + rdx]
.hid0:
    ret

; ---------------------------------------------------------------- на кадр
; главный поток, до прохода света: фигуры, фонарик, огни кадра, маски плиток
dl_frame:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 72
    ; ----- фигуры: игрок и живые зомби (мир 8.8, номер записи тени: игрок — MAXE)
    lea     rdi, [dl_fg]
    movss   xmm0, [pl + E_X]
    mulss   xmm0, [f_256]
    movss   [rdi], xmm0
    movss   xmm0, [pl + E_Y]
    mulss   xmm0, [f_256]
    movss   [rdi + 4], xmm0
    mov     dword [rdi + 8], MAXE
    mov     ecx, 1
    lea     rbx, [ents]
    xor     edx, edx
.fe:
    mov     eax, [rbx + E_ST]
    cmp     eax, ST_WANDER
    jb      .fen
    cmp     eax, ST_CORPSE
    jae     .fen
    cmp     ecx, DL_FGMAX
    jae     .fen
    mov     eax, ecx
    shl     eax, 4
    movss   xmm0, [rbx + E_X]
    mulss   xmm0, [f_256]
    movss   [rdi + rax], xmm0
    movss   xmm0, [rbx + E_Y]
    mulss   xmm0, [f_256]
    movss   [rdi + rax + 4], xmm0
    mov     [rdi + rax + 8], edx
    inc     ecx
.fen:
    add     rbx, ESZ
    inc     edx
    cmp     edx, MAXE
    jb      .fe
    mov     [dl_nfg], ecx
    ; ----- фонарик: рука на четверть тайла впереди, 20 px над снегом, луч чуть вниз
    mov     dword [dl_fon], 0
    cmp     dword [pl_flash], 0
    je      .nofl
    cmp     dword [gstate], 1
    je      .nofl
    movss   xmm0, [flash_i]
    comiss  xmm0, [f_0]
    jbe     .nofl
    mov     dword [dl_fon], 1
    movss   xmm0, [pl + E_FX]
    mulss   xmm0, [f_64]
    cvtsi2ss xmm1, dword [plw_x]
    addss   xmm0, xmm1
    movss   [dl_sx], xmm0
    movss   xmm0, [pl + E_FY]
    mulss   xmm0, [f_64]
    cvtsi2ss xmm1, dword [plw_y]
    addss   xmm0, xmm1
    movss   [dl_sy], xmm0
    movss   xmm0, [pl + E_FX]
    movss   xmm1, [pl + E_FY]
    movss   xmm2, [lt_ftilt]
    movaps  xmm3, xmm0
    mulss   xmm3, xmm0
    movaps  xmm4, xmm1
    mulss   xmm4, xmm1
    addss   xmm3, xmm4
    movaps  xmm4, xmm2
    mulss   xmm4, xmm2
    addss   xmm3, xmm4
    rsqrtss xmm3, xmm3
    mulss   xmm0, xmm3
    mulss   xmm1, xmm3
    mulss   xmm2, xmm3
    movss   [dl_ax], xmm0
    movss   [dl_ay], xmm1
    movss   [dl_az], xmm2
    ; дальность луча, px-единицы²
    movss   xmm0, [fl_reach]
    mulss   xmm0, [f_19_6]
    movss   [dl_frc], xmm0
    mulss   xmm0, xmm0
    movss   [dl_fr2], xmm0
    ; луч в воздухе: b = V·a (V — к глазу), 1/(1 − b²), 1/sin угла между ними
    movss   xmm0, [dl_ax]
    addss   xmm0, [dl_ay]
    mulss   xmm0, [lt_vxy]
    movss   xmm1, [dl_az]
    mulss   xmm1, [lt_vz]
    addss   xmm0, xmm1
    movss   [dl_vb], xmm0
    mulss   xmm0, xmm0
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    maxss   xmm1, [lt_0_02]
    movss   xmm0, [f_1]
    divss   xmm0, xmm1
    movss   [dl_vden], xmm0
    sqrtss  xmm1, xmm1
    maxss   xmm1, [f_0_3]
    movss   xmm0, [f_1]
    divss   xmm0, xmm1
    movss   [dl_visn], xmm0
    ; сила: фонарик · снег в воздухе (в ясную ночь — иней, едва, в метель — стеной)
    movss   xmm0, [flash_i]
    mulss   xmm0, [lt_fmul]
    movss   xmm1, [cone_k]
    subss   xmm1, [f_0_7]
    maxss   xmm1, [f_0_1]
    mulss   xmm0, xmm1
    mulss   xmm0, [lt_bsig]
    movss   [dl_bk], xmm0               ; лучи тени от руки — фоном (fl_map в sky_job)
.nofl:
    ; ----- маски плиток — с нуля
    lea     rdi, [dl_tm]
    xor     eax, eax
    mov     ecx, DT_W*DT_H
    rep     stosq
    mov     [dl_hidm], rax
    xor     r15d, r15d                  ; огней кадра
    mov     dword [rsp + 60], 0         ; проход: 0 — фонари (важнее), 1 — остальное
.pass:
    lea     r12, [lights]
    mov     r13d, [nlights]
.L:
    test    r13d, r13d
    jz      .Lp
    cmp     r15d, AL_MAX
    jae     .Ld
    imul    eax, r15d, AL_SZ
    lea     r14, [al_buf]
    add     r14, rax
    mov     eax, [r12 + L_KIND]
    xor     ecx, ecx
    cmp     eax, LK_LAMP
    je      .p0
    cmp     eax, LK_BROKEN
    je      .p0
    inc     ecx
.p0:
    cmp     ecx, [rsp + 60]
    jne     .Ln
    ; огни в избе видны, только если крыша снята или в ней окно у игрока
    cmp     eax, LK_ROOM
    je      .in
    cmp     eax, LK_TV
    je      .in
    cmp     eax, LK_STOVE
    jne     .out1
.in:
    mov     ecx, [r12 + L_HID]
    test    ecx, ecx
    jz      .Ln
    dec     ecx
    imul    ecx, ecx, HS_SZ
    lea     rdx, [houses]
    add     rdx, rcx
    cmp     dword [rdx + HS_OCC], 0
    jne     .out1
    movss   xmm0, [rdx + HS_FADE]
    mulss   xmm0, [f_16]
    cvtss2si ecx, xmm0
    cmp     ecx, 16
    jge     .Ln
.out1:
    mov     [r14 + AL_KIND], eax
    ; цвет кадра: вид — своя сила, окна и комнаты — по времени, фонари — прогрев, ТВ — мерцание
    movups  xmm0, [r12 + L_COL]
    lea     rdx, [lt_ki]
    movss   xmm1, [rdx + rax*4]
    shufps  xmm1, xmm1, 0
    mulps   xmm0, xmm1
    cmp     eax, LK_LAMP
    je      .kl
    cmp     eax, LK_BROKEN
    je      .kb
    cmp     eax, LK_TV
    je      .kt
    cmp     eax, LK_STOVE
    je      .ks
    mulps   xmm0, [winf_vec]
    jmp     .kc
.kl:
    mulps   xmm0, [lampf_vec]
    jmp     .kc
.kb:
    mulps   xmm0, [lampb_vec]
    jmp     .kc
.kt:
    mulps   xmm0, [tv_vec]
    jmp     .kc
.ks:
    ; пламя: три синуса своей фазы
    movaps  [rsp + 32], xmm0
    mov     eax, [r12 + L_TX]
    mov     ecx, [r12 + L_TY]
    HASH    eax, ecx
    and     eax, 1023
    cvtsi2ss xmm0, eax
    mulss   xmm0, [f_inv163]
    movss   [rsp + 48], xmm0            ; фаза
    movss   xmm0, [elapsed]
    mulss   xmm0, [lt_fl1]
    addss   xmm0, [rsp + 48]
    call    fsin
    mulss   xmm0, [lt_fa1]
    addss   xmm0, [lt_fa0]
    movss   [rsp + 52], xmm0
    movss   xmm0, [elapsed]
    mulss   xmm0, [lt_fl2]
    movss   xmm1, [rsp + 48]
    addss   xmm1, xmm1
    addss   xmm0, xmm1
    call    fsin
    mulss   xmm0, [lt_fa2]
    addss   xmm0, [rsp + 52]
    movss   [rsp + 52], xmm0
    movss   xmm0, [elapsed]
    mulss   xmm0, [lt_fl3]
    movss   xmm1, [rsp + 48]
    mulss   xmm1, [f_3]
    addss   xmm0, xmm1
    call    fsin
    mulss   xmm0, [lt_fa3]
    addss   xmm0, [rsp + 52]
    shufps  xmm0, xmm0, 0
    mulps   xmm0, [rsp + 32]
.kc:
    movaps  [r14 + AL_COL], xmm0
    movaps  xmm1, xmm0
    shufps  xmm1, xmm1, 0x55
    addss   xmm1, xmm0
    movhlps xmm2, xmm0
    addss   xmm1, xmm2
    comiss  xmm1, [lt_cmin]
    jb      .Ln                         ; не горит
    movss   xmm0, [r12 + L_GX]
    movss   [r14 + AL_X], xmm0
    movss   xmm0, [r12 + L_GY]
    movss   [r14 + AL_Y], xmm0
    movss   xmm0, [r12 + L_GZ]
    movss   [r14 + AL_Z], xmm0
    ; радиус, px-единицы
    mov     eax, [r12 + L_KIND]
    lea     rdx, [lt_kr]
    movss   xmm3, [r12 + L_R]
    mulss   xmm3, [rdx + rax*4]
    mulss   xmm3, [f_19_6]
    cmp     eax, LK_WIN
    jne     .r1
    movaps  xmm4, xmm3                  ; свечение стекла — свой радиус
    mulss   xmm4, xmm3
    movss   xmm5, [f_1]
    divss   xmm5, xmm4
    movss   [r14 + AL_IRG], xmm5
    movss   xmm3, [lt_wrc]              ; а пятно сквозь раму — дальше
    mulss   xmm3, [f_19_6]
.r1:
    movss   [rsp + 56], xmm3
    movaps  xmm4, xmm3
    mulss   xmm4, xmm3
    movss   xmm5, [f_1]
    divss   xmm5, xmm4
    movss   [r14 + AL_IR2], xmm5
    ; огни в избе светят только её
    xor     ecx, ecx
    cmp     eax, LK_ROOM
    je      .h1
    cmp     eax, LK_TV
    je      .h1
    cmp     eax, LK_STOVE
    jne     .h2
.h1:
    mov     ecx, [r12 + L_HID]
.h2:
    mov     [r14 + AL_HID], ecx
    test    ecx, ecx
    jnz     .hm
    cmp     eax, LK_WIN
    jne     .hn
.hm:
    mov     rdx, [dl_hidm]              ; огню нужна изба под пикселем (dl_row8)
    bts     rdx, r15
    mov     [dl_hidm], rdx
.hn:
    ; карта горизонта
    mov     qword [r14 + AL_HM], 0
    mov     ecx, [r12 + L_HM]
    test    ecx, ecx
    jz      .nohm
    dec     ecx
    lea     rdx, [hz_pool]
    lea     rdx, [rdx + rcx*4]
    mov     [r14 + AL_HM], rdx
    mov     ecx, [r12 + L_HTO]
    lea     rdx, [hzt_pool]
    lea     rdx, [rdx + rcx*2]
    mov     [r14 + AL_HT], rdx
    mov     ecx, [r12 + L_HMX]
    mov     [r14 + AL_HMX], ecx
    mov     ecx, [r12 + L_HMY]
    mov     [r14 + AL_HMY], ecx
    mov     ecx, [r12 + L_HMW]
    mov     [r14 + AL_HMW], ecx
    cvtsi2ss xmm0, dword [r12 + L_HF]
    movss   [r14 + AL_HSC], xmm0
    mov     ecx, [r12 + L_HCX]
    mov     [r14 + AL_HCX], ecx
    mov     ecx, [r12 + L_HCY]
    mov     [r14 + AL_HCY], ecx
    mov     ecx, [r12 + L_HCW]
    mov     [r14 + AL_HCW], ecx
.nohm:
    cmp     eax, LK_WIN
    jne     .nowin
    ; окно: рама, лампочка избы, цвет сквозь раму (+ телевизор)
    mov     ecx, [r12 + L_WAX]
    mov     [r14 + AL_WAX], ecx
    mov     ecx, [r12 + L_WPC]
    mov     [r14 + AL_WPC], ecx
    mov     ecx, [r12 + L_WSG]
    mov     [r14 + AL_WSG], ecx
    mov     ecx, [r12 + L_WU0]
    mov     [r14 + AL_WU0], ecx
    mov     ecx, [r12 + L_WUD]
    mov     [r14 + AL_WUD], ecx
    mov     ecx, [r12 + L_WMS]
    shl     ecx, 9
    lea     rdx, [wm_buf]
    add     rdx, rcx
    mov     [r14 + AL_WMS], rdx
    mov     ecx, [r12 + L_HID]
    mov     [r14 + AL_WHID], ecx
    movss   xmm0, [r12 + L_BX]
    movss   [r14 + AL_BX], xmm0
    subss   xmm0, [r12 + L_GX]
    mulss   xmm0, [f_pxu]
    movss   xmm1, [r12 + L_BY]
    movss   [r14 + AL_BY], xmm1
    subss   xmm1, [r12 + L_GY]
    mulss   xmm1, [f_pxu]
    movss   xmm2, [r12 + L_BZ]
    movss   [r14 + AL_BZ], xmm2
    subss   xmm2, [r12 + L_GZ]
    mulss   xmm0, xmm0
    mulss   xmm1, xmm1
    mulss   xmm2, xmm2
    addss   xmm0, xmm1
    addss   xmm0, xmm2
    movss   [r14 + AL_BD2], xmm0
    movss   xmm0, [lt_wrb]
    mulss   xmm0, xmm0
    movss   xmm1, [f_1]
    divss   xmm1, xmm0
    movss   [r14 + AL_IRB], xmm1
    ; плиткам: R² свечения; клин пятна — лучи от лампочки к краям стекла (wu 1.5, 7.5)
    movss   xmm0, [r12 + L_R]
    mulss   xmm0, [f_19_6]
    mulss   xmm0, xmm0
    movss   [r14 + AL_R2G], xmm0
    movss   xmm0, [r12 + L_WU0]
    movss   xmm1, [r12 + L_WUD]
    movss   xmm2, [lt_1_5]
    divss   xmm2, xmm1
    addss   xmm2, xmm0                  ; u края 1
    movss   xmm3, [lt_7_5]
    divss   xmm3, xmm1
    addss   xmm3, xmm0                  ; u края 2
    movss   xmm4, [r12 + L_WPC]
    cmp     dword [r12 + L_WAX], 0
    jne     .ewx
    subss   xmm2, [r12 + L_BX]          ; стена вдоль x: край = (u, плоскость)
    subss   xmm3, [r12 + L_BX]
    subss   xmm4, [r12 + L_BY]
    movss   [r14 + AL_EX1], xmm2
    movss   [r14 + AL_EY1], xmm4
    movss   [r14 + AL_EX2], xmm3
    movss   [r14 + AL_EY2], xmm4
    jmp     .esg
.ewx:
    subss   xmm2, [r12 + L_BY]          ; стена вдоль y: край = (плоскость, u)
    subss   xmm3, [r12 + L_BY]
    subss   xmm4, [r12 + L_BX]
    movss   [r14 + AL_EX1], xmm4
    movss   [r14 + AL_EY1], xmm2
    movss   [r14 + AL_EX2], xmm4
    movss   [r14 + AL_EY2], xmm3
.esg:
    ; знак cross(r1, r2): внутри клина cross(r1, P − B) того же знака, cross(r2, P − B) — другого
    movss   xmm0, [r14 + AL_EX1]
    mulss   xmm0, [r14 + AL_EY2]
    movss   xmm1, [r14 + AL_EY1]
    mulss   xmm1, [r14 + AL_EX2]
    subss   xmm0, xmm1
    movss   xmm1, [f_1]
    comiss  xmm0, [f_0]
    ja      .esp
    movss   xmm1, [f_m1]
.esp:
    movss   [r14 + AL_ESG], xmm1
    movups  xmm0, [col_room]
    mulps   xmm0, [lt_projk]
    mulps   xmm0, [winf_vec]
    cmp     dword [r12 + L_TV], 0
    je      .pc
    movups  xmm1, [col_tv]
    mulps   xmm1, [tv_vec]
    mulps   xmm1, [lt_projtv]
    addps   xmm0, xmm1
.pc:
    movaps  [r14 + AL_PCOL], xmm0
.nowin:
    ; ----- для dl_row и dl_row8: размножить на 8 дорожек
    movss   xmm0, [r14 + AL_X]
    shufps  xmm0, xmm0, 0
    movaps  [r14 + AL_XV], xmm0
    movaps  [r14 + AL_XV + 16], xmm0
    movss   xmm0, [r14 + AL_Y]
    shufps  xmm0, xmm0, 0
    movaps  [r14 + AL_YV], xmm0
    movaps  [r14 + AL_YV + 16], xmm0
    movss   xmm0, [r14 + AL_Z]
    shufps  xmm0, xmm0, 0
    movaps  [r14 + AL_ZV], xmm0
    movaps  [r14 + AL_ZV + 16], xmm0
    movss   xmm0, [r14 + AL_IR2]
    shufps  xmm0, xmm0, 0
    movaps  [r14 + AL_IR2V], xmm0
    movaps  [r14 + AL_IR2V + 16], xmm0
    cmp     dword [r14 + AL_KIND], LK_WIN
    jne     .ira
    movss   xmm0, [r14 + AL_IRG]
    shufps  xmm0, xmm0, 0
.ira:
    movaps  [r14 + AL_IRAV], xmm0
    movaps  [r14 + AL_IRAV + 16], xmm0
    movaps  xmm0, [r14 + AL_COL]
    movaps  xmm1, xmm0
    shufps  xmm1, xmm1, 0x00
    movaps  [r14 + AL_CB], xmm1
    movaps  [r14 + AL_CB + 16], xmm1
    movaps  xmm1, xmm0
    shufps  xmm1, xmm1, 0x55
    movaps  [r14 + AL_CG], xmm1
    movaps  [r14 + AL_CG + 16], xmm1
    shufps  xmm0, xmm0, 0xAA
    movaps  [r14 + AL_CR], xmm0
    movaps  [r14 + AL_CR + 16], xmm0
    movd    xmm0, [r14 + AL_HID]
    pshufd  xmm0, xmm0, 0
    movdqa  [r14 + AL_HIDV], xmm0
    movdqa  [r14 + AL_HIDV + 16], xmm0
    movss   xmm0, [rsp + 56]
    mulss   xmm0, xmm0
    movss   [r14 + AL_R2], xmm0
    ; лепесток огня: clamp((ось · к огню)/d · k + b): фонарь — из-под колпака (ось
    ; вверх: пиксель ниже головы), топка — вперёд по +y, окно — наружу, прочее — всюду
    xorps   xmm0, xmm0                  ; ось
    xorps   xmm1, xmm1
    xorps   xmm2, xmm2
    xorps   xmm3, xmm3                  ; k
    movss   xmm4, [f_1]                 ; b
    mov     eax, [r14 + AL_KIND]
    cmp     eax, LK_LAMP
    je      .lcap
    cmp     eax, LK_BROKEN
    je      .lcap
    cmp     eax, LK_STOVE
    je      .lst
    cmp     eax, LK_WIN
    jne     .lset
    ; окно: к огню — внутрь, значит ось — наружу со знаком минус
    movss   xmm5, [r14 + AL_WSG]
    xorps   xmm5, [f_signmask]
    cmp     dword [r14 + AL_WAX], 0
    jne     .lwx
    movaps  xmm1, xmm5
    jmp     .lwk
.lwx:
    movaps  xmm0, xmm5
.lwk:
    movss   xmm3, [f_1]
    xorps   xmm4, xmm4
    jmp     .lset
.lcap:
    movss   xmm2, [f_1]
    movss   xmm3, [lt_capk]
    movss   xmm4, [lt_capb]
    jmp     .lset
.lst:
    movss   xmm1, [f_1]
    movss   xmm3, [lt_m1_2]
    movss   xmm4, [f_0_1]
.lset:
    shufps  xmm0, xmm0, 0
    movaps  [r14 + AL_LDX], xmm0
    movaps  [r14 + AL_LDX + 16], xmm0
    shufps  xmm1, xmm1, 0
    movaps  [r14 + AL_LDY], xmm1
    movaps  [r14 + AL_LDY + 16], xmm1
    shufps  xmm2, xmm2, 0
    movaps  [r14 + AL_LDZ], xmm2
    movaps  [r14 + AL_LDZ + 16], xmm2
    shufps  xmm3, xmm3, 0
    movaps  [r14 + AL_LK], xmm3
    movaps  [r14 + AL_LK + 16], xmm3
    shufps  xmm4, xmm4, 0
    movaps  [r14 + AL_LB], xmm4
    movaps  [r14 + AL_LB + 16], xmm4
    ; прямоугольник на экране -> маски плиток
    movss   xmm0, [r14 + AL_X]
    movss   xmm1, [r14 + AL_Y]
    movss   xmm2, [r14 + AL_Z]
    movss   xmm3, [rsp + 56]
    cmp     dword [r14 + AL_KIND], LK_WIN
    jne     .rc
    ; окно светит на одну сторону: круг на 2.5 тайла наружу, радиус 4 тайла
    movss   xmm4, [r14 + AL_WSG]
    mulss   xmm4, [lt_w640]
    cmp     dword [r14 + AL_WAX], 0
    jne     .rcx
    addss   xmm1, xmm4
    jmp     .rcy
.rcx:
    addss   xmm0, xmm4
.rcy:
    movss   xmm3, [lt_wrr]
.rc:
    call    al_rect
    test    eax, eax
    jz      .Ln
    call    al_figs
    inc     r15d
.Ln:
    add     r12, L_SZ
    dec     r13d
    jmp     .L
.Lp:
    inc     dword [rsp + 60]
    cmp     dword [rsp + 60], 2
    jb      .pass
.Ld:
    mov     [n_al], r15d
    add     rsp, 72
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; xmm0, xmm1 = точка огня (мир 8.8), xmm2 = высота, xmm3 = радиус (px-единицы),
; r15d = номер огня -> бит во всех плитках экрана, куда он достаёт; eax = 0 — мимо
; экрана. Поверхности — от снега до 100 px
al_rect:
    movaps  xmm4, xmm3
    mulss   xmm4, [lt_ipxu]             ; радиус, 8.8
    cvtsi2ss xmm5, dword [camx]
    subss   xmm0, xmm5
    cvtsi2ss xmm5, dword [camy]
    subss   xmm1, xmm5
    movaps  xmm5, xmm0
    subss   xmm5, xmm1
    mulss   xmm5, [f_inv16]
    addss   xmm5, [lt_cx0]              ; x на экране
    movaps  xmm6, xmm0
    addss   xmm6, xmm1
    mulss   xmm6, [f_inv32]
    addss   xmm6, [lt_cy0]              ; y точки на снегу
    movaps  xmm0, xmm4
    mulss   xmm0, [lt_0884]
    addss   xmm0, [f_2]                 ; полуширина
    mulss   xmm4, [lt_0442]
    addss   xmm4, [f_2]                 ; полувысота
    movaps  xmm1, xmm2
    subss   xmm1, xmm3
    maxss   xmm1, [f_0]                 ; zmin
    addss   xmm2, xmm3
    minss   xmm2, [lt_zmax]             ; zmax
    movaps  xmm7, xmm5
    subss   xmm7, xmm0
    cvttss2si r8d, xmm7                 ; x0
    addss   xmm5, xmm0
    cvttss2si r9d, xmm5                 ; x1
    movaps  xmm7, xmm6
    subss   xmm7, xmm4
    subss   xmm7, xmm2
    cvttss2si r10d, xmm7                ; y0
    addss   xmm6, xmm4
    subss   xmm6, xmm1
    cvttss2si r11d, xmm6                ; y1
    xor     eax, eax
    test    r9d, r9d
    js      .out
    cmp     r8d, SCR_W - 1
    jg      .out
    test    r11d, r11d
    js      .out
    cmp     r10d, SCR_H - 1
    jg      .out
    xor     ecx, ecx
    cmp     r8d, ecx
    cmovl   r8d, ecx
    cmp     r10d, ecx
    cmovl   r10d, ecx
    mov     ecx, SCR_W - 1
    cmp     r9d, ecx
    cmovg   r9d, ecx
    mov     ecx, SCR_H - 1
    cmp     r11d, ecx
    cmovg   r11d, ecx
    shr     r8d, 4
    shr     r9d, 4
    shr     r10d, 4
    shr     r11d, 4
    mov     ecx, r15d
    mov     edx, 1
    shl     rdx, cl                     ; бит огня
    sub     r9d, r8d
    inc     r9d                         ; плиток в строке
.ty:
    imul    eax, r10d, DT_W
    add     eax, r8d
    lea     rcx, [dl_tm]
    lea     rax, [rcx + rax*8]
    lea     rcx, [rax + r9*8]
.tx:
    or      [rax], rdx
    add     rax, 8
    cmp     rax, rcx
    jb      .tx
    inc     r10d
    cmp     r10d, r11d
    jbe     .ty
    mov     eax, 1
.out:
    ret

; r14 -> огонь кадра: фигуры в его радиусе (у огня в избе — в той же избе), кроме
; стоящих вплотную к огню
al_figs:
    xor     ecx, ecx
    movss   xmm4, [rsp + 8 + 56]        ; радиус огня, px-единицы (из кадра dl_frame)
    mulss   xmm4, [lt_ipxu]
    mulss   xmm4, xmm4                  ; R², 8.8
    lea     rdx, [dl_fg]
    mov     r8d, [dl_nfg]
.f:
    test    r8d, r8d
    jz      .d
    movss   xmm0, [rdx]
    subss   xmm0, [r14 + AL_X]
    movss   xmm1, [rdx + 4]
    subss   xmm1, [r14 + AL_Y]
    mulss   xmm0, xmm0
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    comiss  xmm0, xmm4
    jae     .n
    comiss  xmm0, [lt_fclose]
    jb      .n
    mov     eax, [r14 + AL_HID]
    test    eax, eax
    jz      .add
    cvttss2si r9d, [rdx]
    cvttss2si r10d, [rdx + 4]
    shr     r9d, 8
    shr     r10d, 8
    cmp     r9d, MAPW
    jae     .n
    cmp     r10d, MAPW
    jae     .n
    shl     r10d, 7
    add     r10d, r9d
    lea     r9, [rmap]
    movzx   r9d, byte [r9 + r10]
    cmp     r9d, eax
    jne     .n
.add:
    ; рамка тени: от фигуры прочь от огня на длину тени (фонарь выше роста — конечна)
    imul    eax, ecx, AL_FSZ
    lea     r9, [r14 + AL_FIG]
    add     r9, rax
    movss   xmm0, [rdx]
    movss   [r9], xmm0
    movss   xmm1, [rdx + 4]
    movss   [r9 + 4], xmm1
    subss   xmm0, [r14 + AL_X]
    subss   xmm1, [r14 + AL_Y]
    movaps  xmm2, xmm0
    mulss   xmm2, xmm0
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    addss   xmm2, xmm3
    sqrtss  xmm2, xmm2                  ; до огня, 8.8
    maxss   xmm2, [f_1]
    divss   xmm0, xmm2
    divss   xmm1, xmm2                  ; прочь от огня
    movaps  xmm3, xmm4
    sqrtss  xmm3, xmm3                  ; R, 8.8
    movss   xmm5, [r14 + AL_Z]
    subss   xmm5, [lt_figh]
    comiss  xmm5, [f_1]
    jbe     .fl1
    mulss   xmm2, [lt_figh]
    divss   xmm2, xmm5                  ; тень за фигурой: d·h / (H − h)
    minss   xmm3, xmm2
.fl1:
    mulss   xmm0, xmm3
    mulss   xmm1, xmm3
    addss   xmm0, [r9]                  ; кончик тени
    addss   xmm1, [r9 + 4]
    mulss   xmm3, [lt_fpm]
    addss   xmm3, [lt_fm0]              ; поле: полширины + полутень
    movss   xmm5, [r9]
    movaps  xmm6, xmm5
    minss   xmm5, xmm0
    maxss   xmm6, xmm0
    subss   xmm5, xmm3
    addss   xmm6, xmm3
    movss   [r9 + 8], xmm5
    movss   [r9 + 12], xmm6
    movss   xmm5, [r9 + 4]
    movaps  xmm6, xmm5
    minss   xmm5, xmm1
    maxss   xmm6, xmm1
    subss   xmm5, xmm3
    addss   xmm6, xmm3
    movss   [r9 + 16], xmm5
    movss   [r9 + 20], xmm6
    mov     eax, [rdx + 8]
    mov     [r9 + 24], eax              ; номер записи тени (sc_buf)
    inc     ecx
    cmp     ecx, AL_FMAX
    jae     .d
.n:
    add     rdx, 16
    dec     r8d
    jmp     .f
.d:
    mov     [r14 + AL_NF], ecx
    ret

; ---------------------------------------------------------------- плитки
; после G-буфера, до света (par_for по строкам плиток ecx .. edx−1): границы плитки
; 16x16 в мире по её пикселям (x, y — 8.8, z — px); огонь, чья сфера до них не
; достаёт, снимается с маски; окно — ещё и если вся плитка по ту сторону стены;
; флаг плитки — достаёт ли фонарик
DTL_END     equ 64
dl_tiles:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 72
    mov     [rsp + DTL_END], edx
    mov     r12d, ecx                   ; строка плиток
    lea     rsi, [albedo]
    mov     eax, [camx]
    add     eax, [camy]
    movd    xmm15, eax
    pshufd  xmm15, xmm15, 0             ; camx + camy
.ty:
    xor     r13d, r13d                  ; столбец плиток
.tx:
    movaps  xmm0, [lt_big4]             ; min x, max x, min y, max y, min z, max z
    movaps  xmm1, [lt_mbig4]
    movaps  xmm2, xmm0
    movaps  xmm3, xmm1
    movaps  xmm4, xmm0
    movaps  xmm5, xmm1
    mov     ebx, r12d
    shl     ebx, 4                      ; строка пикселей
    lea     r14d, [ebx + 16]
    mov     eax, SCR_H
    cmp     r14d, eax
    cmova   r14d, eax
.py:
    imul    ebp, ebx, SCR_W
    mov     eax, r13d
    shl     eax, 4
    add     ebp, eax                    ; первый пиксель
    mov     eax, CY0
    sub     eax, ebx
    movd    xmm14, eax
    pshufd  xmm14, xmm14, 0             ; CY0 − строка
    mov     ecx, 4
.pg:
    movdqu  xmm6, [rsi + rbp*4]
    pxor    xmm7, xmm7
    pcmpgtd xmm6, xmm7                  ; есть свет (не пустота, не эмиссия)
    movdqu  xmm7, [rsi + rbp*4 + (gposb - albedo)]
    movdqa  xmm8, xmm7
    pslld   xmm8, 16
    psrld   xmm8, 16                    ; x
    psrld   xmm7, 16                    ; y
    movdqa  xmm9, xmm8
    paddd   xmm9, xmm7
    psubd   xmm9, xmm15
    psrad   xmm9, 5
    paddd   xmm9, xmm14                 ; z
    cvtdq2ps xmm8, xmm8
    cvtdq2ps xmm7, xmm7
    cvtdq2ps xmm9, xmm9
    ; пустые — вне рамки: min берёт +big, max — −big
    movaps  xmm10, xmm6
    andnps  xmm10, [lt_big4]
    movaps  xmm11, xmm6
    andnps  xmm11, [lt_mbig4]
    movaps  xmm12, xmm8
    andps   xmm12, xmm6
    orps    xmm12, xmm10
    minps   xmm0, xmm12
    movaps  xmm12, xmm8
    andps   xmm12, xmm6
    orps    xmm12, xmm11
    maxps   xmm1, xmm12
    movaps  xmm12, xmm7
    andps   xmm12, xmm6
    orps    xmm12, xmm10
    minps   xmm2, xmm12
    movaps  xmm12, xmm7
    andps   xmm12, xmm6
    orps    xmm12, xmm11
    maxps   xmm3, xmm12
    movaps  xmm12, xmm9
    andps   xmm12, xmm6
    orps    xmm12, xmm10
    minps   xmm4, xmm12
    andps   xmm9, xmm6
    orps    xmm9, xmm11
    maxps   xmm5, xmm9
    add     ebp, 4
    dec     ecx
    jnz     .pg
    inc     ebx
    cmp     ebx, r14d
    jb      .py
    ; свести 4 дорожки
    movhlps xmm6, xmm0
    minps   xmm0, xmm6
    pshufd  xmm6, xmm0, 1
    minss   xmm0, xmm6                  ; min x
    movhlps xmm6, xmm1
    maxps   xmm1, xmm6
    pshufd  xmm6, xmm1, 1
    maxss   xmm1, xmm6                  ; max x
    movhlps xmm6, xmm2
    minps   xmm2, xmm6
    pshufd  xmm6, xmm2, 1
    minss   xmm2, xmm6
    movhlps xmm6, xmm3
    maxps   xmm3, xmm6
    pshufd  xmm6, xmm3, 1
    maxss   xmm3, xmm6
    movhlps xmm6, xmm4
    minps   xmm4, xmm6
    pshufd  xmm6, xmm4, 1
    minss   xmm4, xmm6
    movhlps xmm6, xmm5
    maxps   xmm5, xmm6
    pshufd  xmm6, xmm5, 1
    maxss   xmm5, xmm6
    imul    eax, r12d, DT_W
    add     eax, r13d
    mov     r15d, eax                   ; плитка
    lea     rdx, [dl_tm]
    mov     rbx, [rdx + r15*8]          ; маска
    xor     edi, edi                    ; флаг фонарика
    xor     r10d, r10d                  ; окна, чьё пятно может лечь в плитку
    xor     r11d, r11d                  ; огни, чью тень в плитке надо считать
    comiss  xmm0, xmm1
    ja      .empty                      ; в плитке света нет
    ; ----- огни: сфера против рамки
    mov     r8, rbx
.lt:
    test    r8, r8
    jz      .ltd
    bsf     rcx, r8
    lea     rax, [r8 - 1]
    and     r8, rax
    imul    eax, ecx, AL_SZ
    lea     r9, [al_buf]
    add     r9, rax
    movss   xmm6, [r9 + AL_X]
    movaps  xmm7, xmm6
    maxss   xmm7, xmm0
    minss   xmm7, xmm1
    subss   xmm6, xmm7
    mulss   xmm6, [f_pxu]
    mulss   xmm6, xmm6
    movss   xmm8, [r9 + AL_Y]
    movaps  xmm7, xmm8
    maxss   xmm7, xmm2
    minss   xmm7, xmm3
    subss   xmm8, xmm7
    mulss   xmm8, [f_pxu]
    mulss   xmm8, xmm8
    addss   xmm6, xmm8
    movss   xmm8, [r9 + AL_Z]
    movaps  xmm7, xmm8
    maxss   xmm7, xmm4
    minss   xmm7, xmm5
    subss   xmm8, xmm7
    mulss   xmm8, xmm8
    addss   xmm6, xmm8
    comiss  xmm6, [r9 + AL_R2]
    jae     .off
    ; ----- тень: вся плитка не ниже порогов карты горизонта под собой — не считать.
    ; Выборка .hz сдвинута к огню не дальше 0.7 тайла (179 в 8.8), PCF берёт и +1
    cmp     qword [r9 + AL_HM], 0
    je      .nh
    cvttss2si r14d, xmm4                ; низ плитки, px
    mov     edx, [r9 + AL_HCW]
    dec     edx                         ; W − 1
    movss   xmm7, [r9 + AL_X]
    cvttss2si eax, xmm0
    comiss  xmm7, xmm1
    jae     .q0                         ; огонь правее всей плитки — сдвиг только вправо
    sub     eax, 192
.q0:
    sar     eax, 6
    sub     eax, [r9 + AL_HCX]
    test    eax, eax
    jns     .q1
    xor     eax, eax
.q1:
    mov     [rsp], eax                  ; i0
    cvttss2si eax, xmm1
    comiss  xmm7, xmm0
    jbe     .q2
    add     eax, 192
.q2:
    sar     eax, 6
    inc     eax
    sub     eax, [r9 + AL_HCX]
    cmp     eax, edx
    jle     .q3
    mov     eax, edx
.q3:
    sub     eax, [rsp]
    jl      .nh                         ; мимо карты — тени нет
    inc     eax
    mov     [rsp + 4], eax              ; сэмплов в ряду
    movss   xmm7, [r9 + AL_Y]
    cvttss2si eax, xmm2
    comiss  xmm7, xmm3
    jae     .q4
    sub     eax, 192
.q4:
    sar     eax, 6
    sub     eax, [r9 + AL_HCY]
    test    eax, eax
    jns     .q5
    xor     eax, eax
.q5:
    mov     [rsp + 8], eax              ; j0
    cvttss2si eax, xmm3
    comiss  xmm7, xmm2
    jbe     .q6
    add     eax, 192
.q6:
    sar     eax, 6
    inc     eax
    sub     eax, [r9 + AL_HCY]
    cmp     eax, edx
    jle     .q7
    mov     eax, edx
.q7:
    sub     eax, [rsp + 8]
    jl      .nh
    inc     eax
    mov     edi, eax                    ; рядов
    mov     eax, [r9 + AL_HCW]
    lea     rbp, [rax + rax]
    mov     [rsp + 16], rbp             ; шаг ряда, байты
    imul    eax, [rsp + 8]
    add     eax, [rsp]
    mov     rbp, [r9 + AL_HT]
    lea     rbp, [rbp + rax*2]          ; первый сэмпл
.qy:
    xor     eax, eax
.qx:
    cmp     [rbp + rax*2], r14w
    jg      .hs                         ; порог выше низа плитки — тенить
    inc     eax
    cmp     eax, [rsp + 4]
    jb      .qx
    add     rbp, [rsp + 16]
    dec     edi
    jnz     .qy
    jmp     .nh
.hs:
    bts     r11, rcx
.nh:
    cmp     dword [r9 + AL_KIND], LK_WIN
    jne     .lt
    movaps  xmm9, xmm6                  ; d² до стекла
    ; окно: вся плитка по ту сторону стены — не светит
    movaps  xmm6, xmm2                  ; n = y
    movaps  xmm7, xmm3
    cmp     dword [r9 + AL_WAX], 0
    je      .w0
    movaps  xmm6, xmm0                  ; n = x
    movaps  xmm7, xmm1
.w0:
    subss   xmm6, [r9 + AL_WPC]
    mulss   xmm6, [r9 + AL_WSG]
    subss   xmm7, [r9 + AL_WPC]
    mulss   xmm7, [r9 + AL_WSG]
    maxss   xmm6, xmm7
    comiss  xmm6, [f_0]
    jbe     .off
    ; пятно сквозь раму: ниже верха рамы и в клине от лампочки через стекло
    comiss  xmm4, [lt_pz26]
    jae     .wg
    movaps  xmm10, xmm0
    unpcklps xmm10, xmm1
    movlhps xmm10, xmm10                ; x0 x1 x0 x1
    movaps  xmm11, xmm2
    unpcklps xmm11, xmm2
    movaps  xmm12, xmm3
    unpcklps xmm12, xmm3
    movlhps xmm11, xmm12                ; y0 y0 y1 y1
    movss   xmm12, [r9 + AL_BX]
    shufps  xmm12, xmm12, 0
    subps   xmm10, xmm12
    movss   xmm12, [r9 + AL_BY]
    shufps  xmm12, xmm12, 0
    subps   xmm11, xmm12
    movss   xmm13, [r9 + AL_ESG]
    shufps  xmm13, xmm13, 0
    ; cross(r1, угол): у всех углов со знаком против клина — мимо
    movss   xmm6, [r9 + AL_EX1]
    shufps  xmm6, xmm6, 0
    mulps   xmm6, xmm11
    movss   xmm7, [r9 + AL_EY1]
    shufps  xmm7, xmm7, 0
    mulps   xmm7, xmm10
    subps   xmm6, xmm7
    mulps   xmm6, xmm13
    xorps   xmm7, xmm7
    cmpltps xmm6, xmm7
    movmskps eax, xmm6
    cmp     eax, 15
    je      .wg
    movss   xmm6, [r9 + AL_EX2]
    shufps  xmm6, xmm6, 0
    mulps   xmm6, xmm11
    movss   xmm7, [r9 + AL_EY2]
    shufps  xmm7, xmm7, 0
    mulps   xmm7, xmm10
    subps   xmm6, xmm7
    mulps   xmm6, xmm13
    xorps   xmm7, xmm7
    cmpltps xmm7, xmm6
    movmskps eax, xmm7
    cmp     eax, 15
    je      .wg
    bts     r10, rcx                    ; пятно может лечь
    jmp     .lt
.wg:
    ; пятна нет — остаётся свечение стекла, если достаёт
    comiss  xmm9, [r9 + AL_R2G]
    jb      .lt
.off:
    btr     rbx, rcx
    jmp     .lt
.ltd:
    ; ----- фонарик: от руки до рамки не дальше луча
    lea     rdx, [dl_tfg]
    mov     word [rdx + r15*2], 0
    xor     edi, edi
    cmp     dword [dl_fon], 0
    je      .mfg
    movss   xmm6, [dl_sx]
    movaps  xmm7, xmm6
    maxss   xmm7, xmm0
    minss   xmm7, xmm1
    subss   xmm6, xmm7
    mulss   xmm6, [f_pxu]
    mulss   xmm6, xmm6
    movss   xmm8, [dl_sy]
    movaps  xmm7, xmm8
    maxss   xmm7, xmm2
    minss   xmm7, xmm3
    subss   xmm8, xmm7
    mulss   xmm8, [f_pxu]
    mulss   xmm8, xmm8
    addss   xmm6, xmm8
    movss   xmm8, [lt_handz]
    movaps  xmm7, xmm8
    maxss   xmm7, xmm4
    minss   xmm7, xmm5
    subss   xmm8, xmm7
    mulss   xmm8, xmm8
    addss   xmm6, xmm8
    comiss  xmm6, [dl_fr2]
    jae     .mfg
    mov     edi, 1
    call    .fsh
.mfg:
    ; ----- луна: фигуры, чья полоса тени (от ступней прочь от луны, с полем) задевает
    ; рамку плитки — по осям рамки, затем вдоль и поперёк полосы (edi |= 4)
    mov     edx, [mn_nf]
    test    edx, edx
    jz      .st
    xor     r8d, r8d                    ; маска
    xor     ecx, ecx
    lea     r9, [mn_fgl]
    movss   xmm8, [mn_ux]
    shufps  xmm8, xmm8, 0
    movss   xmm9, [mn_uy]
    shufps  xmm9, xmm9, 0
.mg1:
    comiss  xmm1, [r9 + 8]
    jb      .mg2
    comiss  xmm0, [r9 + 12]
    ja      .mg2
    comiss  xmm3, [r9 + 16]
    jb      .mg2
    comiss  xmm2, [r9 + 20]
    ja      .mg2
    movaps  xmm4, xmm0
    unpcklps xmm4, xmm1
    movlhps xmm4, xmm4                  ; x0 x1 x0 x1
    movaps  xmm5, xmm2
    unpcklps xmm5, xmm2
    movaps  xmm6, xmm3
    unpcklps xmm6, xmm3
    movlhps xmm5, xmm6                  ; y0 y0 y1 y1
    movss   xmm6, [r9]
    shufps  xmm6, xmm6, 0
    subps   xmm4, xmm6                  ; углы от ступней
    movss   xmm6, [r9 + 4]
    shufps  xmm6, xmm6, 0
    subps   xmm5, xmm6
    movaps  xmm6, xmm4
    mulps   xmm6, xmm8
    movaps  xmm7, xmm5
    mulps   xmm7, xmm9
    addps   xmm6, xmm7                  ; вдоль
    mulps   xmm5, xmm8
    mulps   xmm4, xmm9
    subps   xmm5, xmm4                  ; поперёк
    movss   xmm10, [r9 + 32]            ; поле
    movhlps xmm7, xmm6
    movaps  xmm11, xmm6
    minps   xmm6, xmm7
    maxps   xmm11, xmm7
    pshufd  xmm7, xmm6, 1
    minss   xmm6, xmm7
    pshufd  xmm7, xmm11, 1
    maxss   xmm11, xmm7
    addss   xmm11, xmm10
    comiss  xmm11, [f_0]
    jb      .mg2                        ; вся плитка позади ступней
    subss   xmm6, xmm10
    comiss  xmm6, [r9 + 28]
    ja      .mg2                        ; за кончиком
    movhlps xmm7, xmm5
    movaps  xmm11, xmm5
    minps   xmm5, xmm7
    maxps   xmm11, xmm7
    pshufd  xmm7, xmm5, 1
    minss   xmm5, xmm7
    pshufd  xmm7, xmm11, 1
    maxss   xmm11, xmm7
    addss   xmm11, xmm10
    comiss  xmm11, [f_0]
    jb      .mg2                        ; сбоку
    subss   xmm5, xmm10
    comiss  xmm5, [f_0]
    ja      .mg2
    bts     r8d, ecx
.mg2:
    add     r9, MN_FSZ
    inc     ecx
    cmp     ecx, edx
    jb      .mg1
    lea     rdx, [dl_tmf]
    mov     [rdx + r15*2], r8w
    test    r8d, r8d
    jz      .st
    or      edi, 4
    jmp     .st
.empty:
    xor     ebx, ebx
    xor     r10d, r10d
.st:
    lea     rdx, [dl_tm]
    mov     [rdx + r15*8], rbx
    lea     rdx, [dl_tp]
    mov     [rdx + r15*8], r10
    lea     rdx, [dl_th]
    mov     [rdx + r15*8], r11
    lea     rdx, [dl_tf]
    mov     [rdx + r15], dil
    inc     r13d
    cmp     r13d, DT_W
    jb      .tx
    inc     r12d
    cmp     r12d, [rsp + DTL_END]
    jb      .ty
    add     rsp, 72
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
; фонарику в плитке нужна тень (edi |= 2), если хоть один луч веера, что её задевает,
; встречает заслонитель ближе дальнего её угла (+ слот билинейки); лучи — по ромбовому
; углу углов рамки xmm0..3 от руки, рука в рамке — все (портит xmm4..9, eax, ecx, edx,
; r8, r9)
.fsh:
    movss   xmm8, [dl_sx]
    movss   xmm9, [dl_sy]
    movaps  xmm4, xmm0
    unpcklps xmm4, xmm1
    movlhps xmm4, xmm4                  ; x0 x1 x0 x1
    movaps  xmm5, xmm2
    unpcklps xmm5, xmm2
    movaps  xmm6, xmm3
    unpcklps xmm6, xmm3
    movlhps xmm5, xmm6                  ; y0 y0 y1 y1
    shufps  xmm8, xmm8, 0
    subps   xmm4, xmm8                  ; от руки
    shufps  xmm9, xmm9, 0
    subps   xmm5, xmm9
    ; дальний угол -> последний слот, что возьмёт билинейка
    movaps  xmm6, xmm4
    mulps   xmm6, xmm4
    movaps  xmm7, xmm5
    mulps   xmm7, xmm5
    addps   xmm6, xmm7
    movhlps xmm7, xmm6
    maxps   xmm6, xmm7
    pshufd  xmm7, xmm6, 1
    maxss   xmm6, xmm7
    sqrtss  xmm6, xmm6
    mulss   xmm6, [fl_idsv]
    cvttss2si r8d, xmm6
    add     r8d, 2
    ; лучи
    xor     ecx, ecx                    ; первый
    mov     edx, FL_NR - 1              ; последний
    comiss  xmm8, xmm0
    jb      .fs1
    comiss  xmm8, xmm1
    ja      .fs1
    comiss  xmm9, xmm2
    jb      .fs1
    comiss  xmm9, xmm3
    jbe     .fs2                        ; рука в рамке — весь веер
.fs1:
    movss   xmm8, [fl_ax]
    shufps  xmm8, xmm8, 0
    movss   xmm9, [fl_ay]
    shufps  xmm9, xmm9, 0
    movaps  xmm6, xmm4
    mulps   xmm6, xmm8
    movaps  xmm7, xmm5
    mulps   xmm7, xmm9
    addps   xmm6, xmm7                  ; вдоль
    mulps   xmm5, xmm8
    mulps   xmm4, xmm9
    subps   xmm5, xmm4                  ; поперёк
    maxps   xmm6, [lt_zero4]
    movaps  xmm7, xmm5
    andps   xmm7, [lt_absm]
    addps   xmm6, xmm7
    addps   xmm6, [lt_1e4v]
    divps   xmm5, xmm6
    mulps   xmm5, [fl_kuv]
    addps   xmm5, [fl_u0v]              ; лучи углов, дробные
    movhlps xmm6, xmm5
    movaps  xmm7, xmm5
    minps   xmm7, xmm6
    maxps   xmm5, xmm6
    pshufd  xmm6, xmm7, 1
    minss   xmm7, xmm6
    pshufd  xmm6, xmm5, 1
    maxss   xmm5, xmm6
    maxss   xmm7, [lt_zero4]
    minss   xmm7, [fl_umaxv]
    maxss   xmm5, [lt_zero4]
    minss   xmm5, [fl_umaxv]
    cvttss2si ecx, xmm7
    dec     ecx
    xor     eax, eax
    cmp     ecx, eax
    cmovl   ecx, eax
    cvttss2si edx, xmm5
    add     edx, 2
    mov     eax, FL_NR - 1
    cmp     edx, eax
    cmovg   edx, eax
.fs2:
    ; тени зомби, что могут лечь в плитку: их лучи — среди её лучей, начало — не дальше
    ; её дальнего угла
    mov     eax, [fl_nf]
    test    eax, eax
    jz      .fs5
    push    rbx
    push    r10
    push    r11
    xor     r10d, r10d                  ; маска
    xor     r11d, r11d                  ; фигура
.fs6:
    lea     r9, [fl_frl]
    movzx   ebx, word [r9 + r11*4]
    cmp     ebx, edx
    ja      .fs7
    movzx   ebx, word [r9 + r11*4 + 2]
    cmp     ebx, ecx
    jb      .fs7
    lea     r9, [fl_fr0]
    cmp     r8d, [r9 + r11*4]
    jl      .fs7
    bts     r10d, r11d
.fs7:
    inc     r11d
    cmp     r11d, eax
    jb      .fs6
    lea     r9, [dl_tfg]
    mov     [r9 + r15*2], r10w
    pop     r11
    pop     r10
    pop     rbx
.fs5:
    lea     r9, [fl_first]
.fs3:
    cmp     [r9 + rcx*4], r8d
    jle     .fs4                        ; заслонитель ближе — тень нужна
    inc     ecx
    cmp     ecx, edx
    jbe     .fs3
    ret
.fs4:
    or      edi, 2
    ret

; ---------------------------------------------------------------- строка
; ecx = строка: свет огней на пиксель -> dlb (слова B, G, R, 0), по 4 пикселя —
; дорожка на пиксель (все 4 — в одной плитке). Кадр дорожек — от rbp.
; xmm8 = 1/(1 + обёртка), xmm9 = обёртка, xmm10–12 = нормаль, xmm13–15 = свет B, G, R.
; Сохраняет rbx, rsi, rdi, rbp, r12–r15; xmm портит все
DS_GX       equ 0                       ; x, y (8.8), z (px) — f32
DS_GY       equ 16
DS_GZ       equ 32
DS_ZI       equ 48                      ; z, целые
DS_VAL      equ 64                      ; дорожка со светом
DS_OWN      equ 80                      ; крыша (свой свет)
DS_HID      equ 96                      ; изба под пикселем + 1
DS_MSD      equ 112                     ; сдвиг выборки тени к огню: msd·256
DS_BNC      equ 128                     ; отсвет снега
DS_DX       equ 144                     ; до огня, px-единицы
DS_DY       equ 160
DS_DZ       equ 176
DS_ID       equ 192                     ; 1/d
DS_F        equ 208                     ; сила огня до тени
DS_FP       equ 224                     ; окно: сила пятна до тени
DS_M        equ 240                     ; дорожки, куда огонь достаёт
DS_SIDX     equ 256                     ; сэмпл сетки
DS_WX       equ 272                     ; веса билинейки по нему словами: 64 − f | f << 16
DS_WY       equ 288
DS_NID      equ 304                     ; нормаль
DS_T        equ 320                     ; временное (окно: t; тень: индексы)
DS_A        equ 336
DS_B        equ 352
DS_TMP      equ 368
DS_BBX0     equ 432                     ; рамка группы (скаляры)
DS_BBX1     equ 436
DS_BBY0     equ 440
DS_BBY1     equ 444
DS_BBOK     equ 448
DS_ROWZ     equ 452
DS_CAM      equ 456
DS_GOX      equ 460
DS_GOY      equ 464
DS_TM       equ 472                     ; маски плиток строки (адрес)
DS_TF       equ 480                     ; флаги плиток строки (адрес)
DS_TP       equ 488                     ; пятна окон по плиткам строки (адрес)
DS_TH       equ 496                     ; тени огней по плиткам строки (адрес)
DS_TG       equ 504                     ; тени зомби от фонарика по плиткам строки (адрес)
DS_F0       equ 512                     ; тень фонарика: r (слоты), доли, углы
DS_F1       equ 528
DS_F2       equ 544
DS_F3       equ 560
DS_F4       equ 576
DS_F5       equ 592
DS_F6       equ 608
DS_PX       equ 624                     ; пиксель, px-единицы (x, y)
DS_PY       equ 640
DS_AA       equ 656                     ; тени фигур: |D|², 1/|D|², размер огня
DS_IA       equ 672
DS_SZL      equ 688
DS_R0X      equ 704                     ; пиксель − A капсулы
DS_R0Y      equ 720
DS_R0Z      equ 736
DS_CF       equ 752                     ; D·r0
DS_CB       equ 768                     ; D·E
DS_NO       equ 784                     ; не своя капсула
DS_OM       equ 800                     ; наибольшая доля тени
DS_CM       equ 816                     ; дорожки огня
DS_CM2      equ 832                     ; дорожки фигуры
DS_TMF      equ 848                     ; фигуры под луной по плиткам строки (адрес)
DS_WM       equ 856                     ; отсев капсул: запас на полутень огня
DS_SZ       equ 872                     ; ≡ 8 (mod 16): кадр выровнен после 8 push
dl_row:
    push    rbx
    push    rsi
    push    rdi
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, DS_SZ
    mov     rbp, rsp
    lea     rsi, [albedo]
    mov     eax, CY0
    sub     eax, ecx
    mov     [rbp + DS_ROWZ], eax
    mov     eax, ecx
    shr     eax, 4
    imul    edx, eax, DT_W
    lea     r8, [dl_tf]
    add     r8, rdx
    mov     [rbp + DS_TF], r8
    lea     r8, [dl_tm]
    lea     r8, [r8 + rdx*8]
    mov     [rbp + DS_TM], r8
    lea     r8, [dl_tp]
    lea     r8, [r8 + rdx*8]
    mov     [rbp + DS_TP], r8
    lea     r8, [dl_th]
    lea     r8, [r8 + rdx*8]
    mov     [rbp + DS_TH], r8
    lea     r8, [dl_tfg]
    lea     r8, [r8 + rdx*2]
    mov     [rbp + DS_TG], r8
    lea     r8, [dl_tmf]
    lea     r8, [r8 + rdx*2]
    mov     [rbp + DS_TMF], r8
    mov     eax, [sc_wmax]
    mov     [rbp + DS_WM], eax
    mov     eax, [camx]
    add     eax, [camy]
    mov     [rbp + DS_CAM], eax
    mov     eax, [gox]
    shl     eax, 6
    mov     [rbp + DS_GOX], eax
    mov     eax, [goy]
    shl     eax, 6
    mov     [rbp + DS_GOY], eax
    imul    r12d, ecx, SCR_W            ; пиксель
    xor     r13d, r13d                  ; x
.grp:
    ; плитка без огней и без фонарика — темно (и отсвету взяться неоткуда)
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DS_TM]
    cmp     qword [rdx + rax*8], 0
    jne     .g0
    mov     rdx, [rbp + DS_TF]
    cmp     byte [rdx + rax], 0
    je      .gzero
.g0:
    movdqu  xmm0, [rsi + r12*4]
    pxor    xmm1, xmm1
    pcmpgtd xmm0, xmm1                  ; есть свет: не пустота, не эмиссия
    movmskps eax, xmm0
    test    eax, eax
    jz      .gzero
    movdqa  [rbp + DS_VAL], xmm0
    ; ----- мир: x, y (8.8), z (px)
    movdqu  xmm1, [rsi + r12*4 + (gposb - albedo)]
    movdqa  xmm2, xmm1
    pslld   xmm2, 16
    psrld   xmm2, 16                    ; x
    psrld   xmm1, 16                    ; y
    movdqa  xmm3, xmm2
    paddd   xmm3, xmm1
    movd    xmm4, [rbp + DS_CAM]
    pshufd  xmm4, xmm4, 0
    psubd   xmm3, xmm4
    psrad   xmm3, 5
    movd    xmm4, [rbp + DS_ROWZ]
    pshufd  xmm4, xmm4, 0
    paddd   xmm3, xmm4                  ; z
    movdqa  [rbp + DS_ZI], xmm3
    cvtdq2ps xmm4, xmm2
    movaps  [rbp + DS_GX], xmm4
    mulps   xmm4, [lt_pxu4]
    movaps  [rbp + DS_PX], xmm4
    cvtdq2ps xmm4, xmm1
    movaps  [rbp + DS_GY], xmm4
    mulps   xmm4, [lt_pxu4]
    movaps  [rbp + DS_PY], xmm4
    cvtdq2ps xmm4, xmm3
    movaps  [rbp + DS_GZ], xmm4
    mov     dword [rbp + DS_BBOK], 0
    ; плитка только с тенями фигур под луной — их и всё (огней нет: dlb — 0)
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DS_TF]
    movzx   ecx, byte [rdx + rax]
    test    ecx, 1
    jnz     .g1
    mov     rdx, [rbp + DS_TM]
    cmp     qword [rdx + rax*8], 0
    jne     .g1
    call    .mf
    jmp     .gz1
.g1:
    ; изба под пикселем: rmap[(y >> 8)·128 + (x >> 8)]
    movdqa  xmm4, xmm1
    psrld   xmm4, 8
    pslld   xmm4, 7
    movdqa  xmm5, xmm2
    psrld   xmm5, 8
    paddd   xmm4, xmm5
    movdqa  [rbp + DS_TMP], xmm4
    mov     eax, [rbp + DS_TMP]
    movzx   eax, byte [rsi + rax + (rmap - albedo)]
    mov     [rbp + DS_HID], eax
    mov     eax, [rbp + DS_TMP + 4]
    movzx   eax, byte [rsi + rax + (rmap - albedo)]
    mov     [rbp + DS_HID + 4], eax
    mov     eax, [rbp + DS_TMP + 8]
    movzx   eax, byte [rsi + rax + (rmap - albedo)]
    mov     [rbp + DS_HID + 8], eax
    mov     eax, [rbp + DS_TMP + 12]
    movzx   eax, byte [rsi + rax + (rmap - albedo)]
    mov     [rbp + DS_HID + 12], eax
    ; сэмпл сетки света — как в light_rows: от окна сетки, прижато к [0, GLIM]
    movd    xmm4, [rbp + DS_GOX]
    pshufd  xmm4, xmm4, 0
    psubd   xmm2, xmm4
    movd    xmm4, [rbp + DS_GOY]
    pshufd  xmm4, xmm4, 0
    psubd   xmm1, xmm4
    movdqa  xmm4, xmm2
    psrad   xmm4, 31
    pandn   xmm4, xmm2                  ; x ≥ 0
    movdqa  xmm5, xmm4
    pcmpgtd xmm5, [lt_dglim]
    movdqa  xmm6, xmm5
    pandn   xmm6, xmm4
    pand    xmm5, [lt_dglim]
    por     xmm5, xmm6                  ; x ≤ GLIM
    movdqa  xmm4, xmm1
    psrad   xmm4, 31
    pandn   xmm4, xmm1
    movdqa  xmm6, xmm4
    pcmpgtd xmm6, [lt_dglim]
    movdqa  xmm7, xmm6
    pandn   xmm7, xmm4
    pand    xmm6, [lt_dglim]
    por     xmm6, xmm7                  ; y
    movdqa  xmm4, xmm5
    pand    xmm4, [lt_d63]              ; fx
    movdqa  xmm7, xmm4
    pslld   xmm7, 16
    psubd   xmm7, xmm4
    paddd   xmm7, [lt_d64]
    movdqa  [rbp + DS_WX], xmm7
    movdqa  xmm4, xmm6
    pand    xmm4, [lt_d63]              ; fy
    movdqa  xmm7, xmm4
    pslld   xmm7, 16
    psubd   xmm7, xmm4
    paddd   xmm7, [lt_d64]
    movdqa  [rbp + DS_WY], xmm7
    psrld   xmm5, 6
    psrld   xmm6, 6
    pmaddwd xmm6, [lt_dgw]
    paddd   xmm5, xmm6
    movdqa  [rbp + DS_SIDX], xmm5
    ; ----- нормаль, её обёртка, отсвет, сдвиг выборки тени — по индексу нормали
    movd    xmm4, [rsi + r12 + (nbuf - albedo)]
    pxor    xmm5, xmm5
    punpcklbw xmm4, xmm5
    punpcklwd xmm4, xmm5
    movdqa  xmm5, xmm4
    pand    xmm5, [lt_d80]
    pcmpeqd xmm5, [lt_d80]
    movdqa  [rbp + DS_OWN], xmm5
    pand    xmm4, [lt_d3f]
    movdqa  [rbp + DS_NID], xmm4
    mov     eax, [rbp + DS_NID]
    shl     eax, 4
    movups  xmm4, [rsi + rax + (nvec - albedo)]     ; nvec не выровнен на 16
    mov     eax, [rbp + DS_NID + 4]
    shl     eax, 4
    movups  xmm5, [rsi + rax + (nvec - albedo)]
    mov     eax, [rbp + DS_NID + 8]
    shl     eax, 4
    movups  xmm6, [rsi + rax + (nvec - albedo)]
    mov     eax, [rbp + DS_NID + 12]
    shl     eax, 4
    movups  xmm7, [rsi + rax + (nvec - albedo)]
    TR4
    movaps  xmm10, xmm2                 ; x
    movaps  xmm11, xmm1                 ; y
    movaps  xmm12, xmm3                 ; z
    mov     eax, [rbp + DS_NID]
    shl     eax, 4
    movaps  xmm4, [rsi + rax + (dl_nw - albedo)]
    mov     eax, [rbp + DS_NID + 4]
    shl     eax, 4
    movaps  xmm5, [rsi + rax + (dl_nw - albedo)]
    mov     eax, [rbp + DS_NID + 8]
    shl     eax, 4
    movaps  xmm6, [rsi + rax + (dl_nw - albedo)]
    mov     eax, [rbp + DS_NID + 12]
    shl     eax, 4
    movaps  xmm7, [rsi + rax + (dl_nw - albedo)]
    TR4
    movaps  xmm9, xmm2                  ; обёртка
    movaps  xmm8, xmm1                  ; 1 / (1 + обёртка)
    movaps  [rbp + DS_BNC], xmm3
    movaps  [rbp + DS_MSD], xmm6
    ; ----- тени фигур под луной (xmm8–12 — нормаль — не трогает)
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DS_TF]
    test    byte [rdx + rax], 4
    jz      .g2
    call    .mf
    jmp     .g3
.g2:
    mov     dword [rsi + r12 + (mfsh - albedo)], -1
.g3:
    xorps   xmm13, xmm13
    xorps   xmm14, xmm14
    xorps   xmm15, xmm15
    ; ----- огни плитки
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DS_TM]
    mov     r15, [rdx + rax*8]
.al:
    test    r15, r15
    jz      .fl
    bsf     rax, r15
    lea     rcx, [r15 - 1]
    and     r15, rcx
    mov     r14d, eax                   ; номер огня (пятна окон по плиткам)
    imul    eax, eax, AL_SZ
    lea     rdi, [al_buf]
    add     rdi, rax
    movaps  xmm0, [rdi + AL_XV]
    subps   xmm0, [rbp + DS_GX]
    mulps   xmm0, [lt_pxu4]             ; до огня, px-единицы
    movaps  xmm1, [rdi + AL_YV]
    subps   xmm1, [rbp + DS_GY]
    mulps   xmm1, [lt_pxu4]
    movaps  xmm2, [rdi + AL_ZV]
    subps   xmm2, [rbp + DS_GZ]
    movaps  xmm3, xmm0
    mulps   xmm3, xmm0
    movaps  xmm4, xmm1
    mulps   xmm4, xmm1
    addps   xmm3, xmm4
    movaps  xmm4, xmm2
    mulps   xmm4, xmm2
    addps   xmm3, xmm4                  ; d²
    movaps  xmm5, xmm3
    mulps   xmm5, [rdi + AL_IR2V]
    cmpltps xmm5, [lt_one4]             ; достаёт
    andps   xmm5, [rbp + DS_VAL]
    cmp     dword [rdi + AL_HID], 0
    je      .a1
    movdqa  xmm4, [rbp + DS_HID]
    pcmpeqd xmm4, [rdi + AL_HIDV]
    andps   xmm5, xmm4                  ; огонь в избе — только её
.a1:
    movmskps eax, xmm5
    test    eax, eax
    jz      .al
    movaps  [rbp + DS_M], xmm5
    movaps  [rbp + DS_DX], xmm0
    movaps  [rbp + DS_DY], xmm1
    movaps  [rbp + DS_DZ], xmm2
    ; затухание (1 − d²/R²)²
    movaps  xmm4, xmm3
    mulps   xmm4, [rdi + AL_IRAV]
    movaps  xmm6, [lt_one4]
    subps   xmm6, xmm4
    maxps   xmm6, [lt_zero4]
    mulps   xmm6, xmm6
    andps   xmm6, xmm5
    maxps   xmm3, [lt_one4]
    rsqrtps xmm7, xmm3
    movaps  [rbp + DS_ID], xmm7         ; 1/d
    ; N·L с обёрткой
    movaps  xmm4, xmm0
    mulps   xmm4, xmm10
    movaps  xmm3, xmm1
    mulps   xmm3, xmm11
    addps   xmm4, xmm3
    movaps  xmm3, xmm2
    mulps   xmm3, xmm12
    addps   xmm4, xmm3
    mulps   xmm4, xmm7
    addps   xmm4, xmm9
    mulps   xmm4, xmm8
    maxps   xmm4, [lt_zero4]
    mulps   xmm6, xmm4
    ; лепесток огня
    mulps   xmm0, [rdi + AL_LDX]
    mulps   xmm1, [rdi + AL_LDY]
    addps   xmm0, xmm1
    mulps   xmm2, [rdi + AL_LDZ]
    addps   xmm0, xmm2
    mulps   xmm0, xmm7
    mulps   xmm0, [rdi + AL_LK]
    addps   xmm0, [rdi + AL_LB]
    maxps   xmm0, [lt_zero4]
    minps   xmm0, [lt_one4]
    mulps   xmm6, xmm0
    movaps  [rbp + DS_F], xmm6
    xorps   xmm0, xmm0
    movaps  [rbp + DS_FP], xmm0
    cmp     dword [rdi + AL_KIND], LK_WIN
    jne     .a2
    call    .win                        ; свечение — только снаружи; пятно сквозь раму
.a2:
    movaps  xmm0, [rbp + DS_F]
    orps    xmm0, [rbp + DS_FP]
    pxor    xmm1, xmm1
    pcmpeqd xmm0, xmm1
    movmskps eax, xmm0
    cmp     eax, 15
    je      .al                         ; ничего не дошло
    movaps  xmm5, [lt_one4]             ; видимость огня
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DS_TH]
    mov     rdx, [rdx + rax*8]
    bt      rdx, r14
    jnc     .a3                         ; в плитке огню ничто не заслоняет (или карты нет)
    call    .hz
.a3:
    cmp     dword [rdi + AL_NF], 0
    je      .a4
    call    .figs
.a4:
    movaps  xmm4, [rbp + DS_F]
    mulps   xmm4, xmm5
    movaps  xmm0, xmm4
    mulps   xmm0, [rdi + AL_CB]
    addps   xmm13, xmm0
    movaps  xmm0, xmm4
    mulps   xmm0, [rdi + AL_CG]
    addps   xmm14, xmm0
    mulps   xmm4, [rdi + AL_CR]
    addps   xmm15, xmm4
    cmp     dword [rdi + AL_KIND], LK_WIN
    jne     .al
    movaps  xmm4, [rbp + DS_FP]
    mulps   xmm4, xmm5
    movss   xmm0, [rdi + AL_PCOL]
    shufps  xmm0, xmm0, 0
    mulps   xmm0, xmm4
    addps   xmm13, xmm0
    movss   xmm0, [rdi + AL_PCOL + 4]
    shufps  xmm0, xmm0, 0
    mulps   xmm0, xmm4
    addps   xmm14, xmm0
    movss   xmm0, [rdi + AL_PCOL + 8]
    shufps  xmm0, xmm0, 0
    mulps   xmm0, xmm4
    addps   xmm15, xmm0
    jmp     .al

    ; ----- фонарик: прожектор из руки
.fl:
    cmp     dword [dl_fon], 0
    je      .bn
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DS_TF]
    test    byte [rdx + rax], 1
    jz      .bn                         ; до плитки луч не достаёт
    movss   xmm0, [dl_sx]
    shufps  xmm0, xmm0, 0
    movaps  xmm1, [rbp + DS_GX]
    subps   xmm1, xmm0
    mulps   xmm1, [lt_pxu4]             ; от руки
    movss   xmm0, [dl_sy]
    shufps  xmm0, xmm0, 0
    movaps  xmm2, [rbp + DS_GY]
    subps   xmm2, xmm0
    mulps   xmm2, [lt_pxu4]
    movaps  xmm3, [rbp + DS_GZ]
    subps   xmm3, [lt_handz4]
    movaps  xmm4, xmm1
    mulps   xmm4, xmm1
    movaps  xmm5, xmm2
    mulps   xmm5, xmm2
    addps   xmm4, xmm5
    movaps  xmm5, xmm3
    mulps   xmm5, xmm3
    addps   xmm4, xmm5                  ; d²
    movss   xmm5, [dl_fr2]
    shufps  xmm5, xmm5, 0
    cmpltps xmm5, xmm4
    andnps  xmm5, [rbp + DS_VAL]        ; в пределах луча
    maxps   xmm4, [lt_one4]
    movaps  [rbp + DS_A], xmm4          ; d²
    rsqrtps xmm6, xmm4                  ; 1/d
    movaps  [rbp + DS_ID], xmm6
    movss   xmm0, [dl_ax]
    shufps  xmm0, xmm0, 0
    mulps   xmm0, xmm1
    movss   xmm7, [dl_ay]
    shufps  xmm7, xmm7, 0
    mulps   xmm7, xmm2
    addps   xmm0, xmm7
    movss   xmm7, [dl_az]
    shufps  xmm7, xmm7, 0
    mulps   xmm7, xmm3
    addps   xmm0, xmm7
    mulps   xmm0, xmm6                  ; c — косинус от оси луча
    movaps  xmm7, [lt_0_5v]
    cmpltps xmm7, xmm0
    andps   xmm5, xmm7
    movmskps eax, xmm5
    test    eax, eax
    jz      .bn
    movaps  [rbp + DS_M], xmm5
    ; N·L: свет от руки; вскользь по снегу — обёртка шире
    mulps   xmm1, xmm10
    mulps   xmm2, xmm11
    addps   xmm1, xmm2
    mulps   xmm3, xmm12
    addps   xmm1, xmm3
    mulps   xmm1, xmm6
    movaps  xmm2, [lt_fwrap4]
    subps   xmm2, xmm1
    mulps   xmm2, [lt_fwk4]
    maxps   xmm2, [lt_zero4]
    andps   xmm2, xmm5
    movaps  [rbp + DS_B], xmm2          ; N·L
    ; профиль луча: пятно, горячая середина, тёмное кольцо рефлектора, ореол
    movaps  xmm1, xmm0
    subps   xmm1, [lt_0_86v]
    mulps   xmm1, [lt_14v]
    maxps   xmm1, [lt_zero4]
    minps   xmm1, [lt_one4]
    movaps  xmm2, xmm0
    subps   xmm2, [lt_0_965v]
    mulps   xmm2, [lt_40v]
    maxps   xmm2, [lt_zero4]
    minps   xmm2, [lt_one4]
    mulps   xmm2, [lt_0_35v]
    addps   xmm1, xmm2
    movaps  xmm2, xmm0
    subps   xmm2, [lt_0_915v]
    andps   xmm2, [f_absmask]
    mulps   xmm2, [lt_50v]
    movaps  xmm3, [lt_one4]
    subps   xmm3, xmm2
    maxps   xmm3, [lt_zero4]
    mulps   xmm3, [lt_0_12v]
    subps   xmm1, xmm3
    maxps   xmm1, [lt_zero4]
    subps   xmm0, [lt_0_5v]
    mulps   xmm0, [lt_3v]
    maxps   xmm0, [lt_zero4]
    minps   xmm0, [lt_one4]
    mulps   xmm0, [lt_0_07v]
    addps   xmm1, xmm0                  ; профиль
    ; затухание: 1/(1 + 0.022·d²) (тайлы), к краю дальности — на нет
    movaps  xmm0, [rbp + DS_A]
    movaps  xmm2, xmm0
    mulps   xmm0, [lt_fk4]
    addps   xmm0, [lt_one4]
    movss   xmm3, [flash_i]
    mulss   xmm3, [lt_fmul]
    shufps  xmm3, xmm3, 0
    divps   xmm3, xmm0
    mulps   xmm2, [rbp + DS_ID]         ; d
    mulps   xmm2, [lt_i196v]
    movss   xmm0, [fl_reach]
    shufps  xmm0, xmm0, 0
    subps   xmm0, xmm2
    mulps   xmm0, [lt_0_25v]
    maxps   xmm0, [lt_zero4]
    minps   xmm0, [lt_one4]
    mulps   xmm3, xmm0
    mulps   xmm1, xmm3
    mulps   xmm1, [rbp + DS_B]          ; · N·L (0 вне луча)
    movaps  [rbp + DS_F], xmm1
    ; тень: лучи от руки по карте высот (fl_buf), если в плитке есть что заслонять;
    ; крыше — её видимость (lbuf)
    movaps  xmm2, [lt_one4]
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DS_TF]
    test    byte [rdx + rax], 2
    jz      .fl1
    call    .fls
.fl1:
    ; зомби в луче, чьи тени задевают плитку, — капсулами от руки
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DS_TG]
    movzx   r11d, word [rdx + rax*2]
    test    r11d, r11d
    jz      .fl2
    ; дорожки, где тень что-то решает: луч дошёл, не крыша, рельеф не затенил
    movaps  xmm0, [rbp + DS_F]
    pxor    xmm1, xmm1
    pcmpeqd xmm0, xmm1
    xorps   xmm1, xmm1
    cmpltps xmm1, xmm2
    andnps  xmm0, xmm1
    andps   xmm0, [rbp + DS_M]
    movaps  xmm1, [rbp + DS_OWN]
    andnps  xmm1, xmm0
    movaps  [rbp + DS_CM], xmm1
    movmskps eax, xmm1
    test    eax, eax
    jz      .fl2
    movaps  xmm5, xmm2
    call    .gbb
    movss   xmm0, [dl_sx]
    shufps  xmm0, xmm0, 0
    subps   xmm0, [rbp + DS_GX]
    mulps   xmm0, [lt_pxu4]
    movaps  [rbp + DS_DX], xmm0         ; D = рука − пиксель, px-единицы
    movss   xmm1, [dl_sy]
    shufps  xmm1, xmm1, 0
    subps   xmm1, [rbp + DS_GY]
    mulps   xmm1, [lt_pxu4]
    movaps  [rbp + DS_DY], xmm1
    movaps  xmm2, [lt_handz4]
    subps   xmm2, [rbp + DS_GZ]
    movaps  [rbp + DS_DZ], xmm2
    mulps   xmm0, xmm0
    mulps   xmm1, xmm1
    addps   xmm0, xmm1
    mulps   xmm2, xmm2
    addps   xmm0, xmm2
    maxps   xmm0, [lt_one4]
    movaps  [rbp + DS_AA], xmm0
    movaps  xmm1, [lt_one4]
    divps   xmm1, xmm0
    movaps  [rbp + DS_IA], xmm1
    movaps  xmm0, [fl_szlv]
    movaps  [rbp + DS_SZL], xmm0
.fl3:
    bsf     eax, r11d
    btr     r11d, eax
    imul    eax, eax, AL_FSZ
    lea     r9, [fl_fgl]
    add     r9, rax
    mov     r10d, 1
    call    .figl
    test    r11d, r11d
    jnz     .fl3
    movaps  xmm2, xmm5
.fl2:
    movdqu  xmm0, [rsi + r12*4 + (lbuf - albedo)]
    psrld   xmm0, 24
    cvtdq2ps xmm0, xmm0
    mulps   xmm0, [lt_i255v]
    movaps  xmm1, [rbp + DS_OWN]
    andps   xmm0, xmm1
    andnps  xmm1, xmm2
    orps    xmm0, xmm1
    andps   xmm0, [rbp + DS_M]
    mulps   xmm0, [rbp + DS_F]
    movaps  xmm1, xmm0
    mulps   xmm1, [lt_flb]
    addps   xmm13, xmm1
    movaps  xmm1, xmm0
    mulps   xmm1, [lt_flg]
    addps   xmm14, xmm1
    mulps   xmm0, [lt_flr]
    addps   xmm15, xmm0

    ; ----- отсвет снега снизу: свет огней на снегу вокруг (lgst), по грани и высоте
.bn:
    movaps  xmm0, [rbp + DS_GZ]
    mulps   xmm0, [lt_i72v]
    movaps  xmm1, [lt_one4]
    subps   xmm1, xmm0
    maxps   xmm1, [lt_zero4]
    mulps   xmm1, [rbp + DS_BNC]
    andps   xmm1, [rbp + DS_VAL]
    movaps  [rbp + DS_A], xmm1
    xorps   xmm0, xmm0
    cmpltps xmm0, xmm1
    movmskps eax, xmm0
    test    eax, eax
    jz      .gl
    movaps  xmm8, xmm1                  ; доля отсвета (0 — дорожка не берёт)
    ; углы lgst по 4 дорожкам разом
    mov     eax, [rbp + DS_SIDX]
    movq    xmm4, [rsi + rax*4 + (lgst - albedo)]
    movq    xmm0, [rsi + rax*4 + (lgst - albedo) + GW*4]
    punpcklqdq xmm4, xmm0
    mov     eax, [rbp + DS_SIDX + 4]
    movq    xmm5, [rsi + rax*4 + (lgst - albedo)]
    movq    xmm0, [rsi + rax*4 + (lgst - albedo) + GW*4]
    punpcklqdq xmm5, xmm0
    mov     eax, [rbp + DS_SIDX + 8]
    movq    xmm6, [rsi + rax*4 + (lgst - albedo)]
    movq    xmm0, [rsi + rax*4 + (lgst - albedo) + GW*4]
    punpcklqdq xmm6, xmm0
    mov     eax, [rbp + DS_SIDX + 12]
    movq    xmm7, [rsi + rax*4 + (lgst - albedo)]
    movq    xmm0, [rsi + rax*4 + (lgst - albedo) + GW*4]
    punpcklqdq xmm7, xmm0
    TR4                                 ; xmm2 = c00, xmm1 = c10, xmm3 = c01, xmm6 = c11
    movdqa  xmm9, [rbp + DS_WX]
    movdqa  xmm10, [rbp + DS_WY]
    movdqa  xmm11, [lt_dff]
    movdqa  xmm12, [lt_dff0000]
    BNC1    0
    addps   xmm13, xmm0
    BNC1    8
    addps   xmm14, xmm0
    BNC1    16
    addps   xmm15, xmm0

    ; ----- искры: в свете огней и фонарика снег блестит гуще — кристаллы ловят луч
.gl:
    movaps  xmm0, xmm13
    addps   xmm0, xmm15
    xorps   xmm1, xmm1
    cmpltps xmm1, xmm0
    movmskps eax, xmm1                  ; дорожки со светом огней
    test    eax, eax
    jz      .st
    ; снег: нормаль 0 или N_DY..N_SM − 1, и светлый (R ≥ 200)
    movdqa  xmm2, [rbp + DS_NID]
    pxor    xmm3, xmm3
    pcmpeqd xmm3, xmm2
    movdqa  xmm4, xmm2
    pcmpgtd xmm4, [lt_dndy1]
    movdqa  xmm5, [lt_dnsm]
    pcmpgtd xmm5, xmm2
    pand    xmm4, xmm5
    por     xmm3, xmm4
    pand    xmm1, xmm3
    movdqu  xmm2, [rsi + r12*4]
    psrld   xmm2, 16
    pand    xmm2, [lt_dff]
    pcmpgtd xmm2, [lt_d199]
    pand    xmm1, xmm2
    movmskps eax, xmm1
    test    eax, eax
    jz      .st
    ; кристалл: HASH((x >> 3) ^ glint, y >> 3) & 255 < 5 — как HASH, по 4 дорожкам
    cvttps2dq xmm2, [rbp + DS_GX]
    psrld   xmm2, 3
    movd    xmm0, [glint]
    pshufd  xmm0, xmm0, 0
    pxor    xmm2, xmm0
    cvttps2dq xmm3, [rbp + DS_GY]
    psrld   xmm3, 3
    MUL32   xmm2, lt_hx
    MUL32   xmm3, lt_hy
    pxor    xmm2, xmm3
    movdqa  xmm3, xmm2
    psrld   xmm3, 15
    pxor    xmm2, xmm3
    MUL32   xmm2, lt_hm
    movdqa  xmm3, xmm2
    psrld   xmm3, 13
    pxor    xmm2, xmm3
    pand    xmm2, [lt_dff]
    movdqa  xmm3, [lt_d5]
    pcmpgtd xmm3, xmm2
    pand    xmm1, xmm3
    movmskps eax, xmm1
    test    eax, eax
    jz      .st
    ; искра — свет ×2.1
    movaps  xmm0, xmm1
    andps   xmm0, [lt_2_1v]
    andnps  xmm1, [lt_one4]
    orps    xmm0, xmm1
    mulps   xmm13, xmm0
    mulps   xmm14, xmm0
    mulps   xmm15, xmm0
.st:
    ; B, G, R по дорожкам -> 4 пикселя по 4 слова
    cvtps2dq xmm13, xmm13
    cvtps2dq xmm14, xmm14
    cvtps2dq xmm15, xmm15
    packssdw xmm13, xmm13
    packssdw xmm14, xmm14
    packssdw xmm15, xmm15
    punpcklwd xmm13, xmm14              ; B0 G0 B1 G1 B2 G2 B3 G3
    pxor    xmm0, xmm0
    punpcklwd xmm15, xmm0               ; R0 0 R1 0 ...
    movdqa  xmm1, xmm13
    punpckldq xmm13, xmm15
    punpckhdq xmm1, xmm15
    movdqu  [rsi + r12*8 + (dlb - albedo)], xmm13
    movdqu  [rsi + r12*8 + (dlb - albedo) + 16], xmm1
    jmp     .next
.gzero:
    mov     dword [rsi + r12 + (mfsh - albedo)], -1
.gz1:
    xorps   xmm0, xmm0
    movdqu  [rsi + r12*8 + (dlb - albedo)], xmm0
    movdqu  [rsi + r12*8 + (dlb - albedo) + 16], xmm0
.next:
    add     r12d, 4
    add     r13d, 4
    cmp     r13d, SCR_W
    jb      .grp
    add     rsp, DS_SZ
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret

    ; ----- окно (rdi): свечение стекла — только снаружи стены; пятно сквозь раму —
    ; луч лампочка -> пиксель пересекает плоскость стены в W, маска рамы там
.win:
    movaps  xmm0, [rbp + DS_GY]         ; поперёк стены
    movaps  xmm1, [rbp + DS_GX]         ; вдоль
    movss   xmm2, [rdi + AL_BY]
    movss   xmm3, [rdi + AL_BX]
    cmp     dword [rdi + AL_WAX], 0
    je      .w1
    movaps  xmm0, [rbp + DS_GX]
    movaps  xmm1, [rbp + DS_GY]
    movss   xmm2, [rdi + AL_BX]
    movss   xmm3, [rdi + AL_BY]
.w1:
    shufps  xmm2, xmm2, 0               ; лампочка: поперёк
    shufps  xmm3, xmm3, 0               ;           вдоль
    movss   xmm4, [rdi + AL_WPC]
    shufps  xmm4, xmm4, 0
    movaps  xmm5, xmm0
    subps   xmm5, xmm4
    movss   xmm6, [rdi + AL_WSG]
    shufps  xmm6, xmm6, 0
    mulps   xmm5, xmm6
    xorps   xmm6, xmm6
    cmpltps xmm6, xmm5                  ; снаружи
    movd    xmm7, [rdi + AL_WHID]
    pshufd  xmm7, xmm7, 0
    pcmpeqd xmm7, [rbp + DS_HID]
    pandn   xmm7, xmm6                  ; и не в своей избе
    andps   xmm7, [rbp + DS_M]
    movaps  xmm6, [rbp + DS_F]
    andps   xmm6, xmm7
    movaps  [rbp + DS_F], xmm6
    movmskps eax, xmm7
    test    eax, eax
    jz      .wr
    ; пятно в эту плитку не ложится (клин от лампочки сквозь стекло — мимо)
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DS_TP]
    mov     rdx, [rdx + rax*8]
    bt      rdx, r14
    jnc     .wr
    ; t = (WPC − Bn) / (n − Bn); W = B + t·(P − B)
    movaps  xmm5, xmm4
    subps   xmm5, xmm2
    movaps  xmm6, xmm0
    subps   xmm6, xmm2
    divps   xmm5, xmm6                  ; t
    andps   xmm5, xmm7                  ; вне — 0 (не NaN)
    movaps  [rbp + DS_T], xmm5
    movaps  xmm6, xmm1
    subps   xmm6, xmm3
    mulps   xmm6, xmm5
    addps   xmm6, xmm3
    movss   xmm0, [rdi + AL_WU0]
    shufps  xmm0, xmm0, 0
    subps   xmm6, xmm0
    movss   xmm0, [rdi + AL_WUD]
    shufps  xmm0, xmm0, 0
    mulps   xmm6, xmm0
    addps   xmm6, [lt_one4]             ; a = wu + 1
    movss   xmm0, [rdi + AL_BZ]
    shufps  xmm0, xmm0, 0
    movaps  xmm1, [rbp + DS_GZ]
    subps   xmm1, xmm0
    mulps   xmm1, xmm5
    addps   xmm1, xmm0
    subps   xmm1, [lt_6v]               ; b = wh + 1
    xorps   xmm0, xmm0
    cmpltps xmm0, xmm6
    andps   xmm7, xmm0
    movaps  xmm0, xmm6
    cmpltps xmm0, [lt_wmaxav]
    andps   xmm7, xmm0
    xorps   xmm0, xmm0
    cmpltps xmm0, xmm1
    andps   xmm7, xmm0
    movaps  xmm0, xmm1
    cmpltps xmm0, [lt_wmaxbv]
    andps   xmm7, xmm0                  ; в раме
    movmskps eax, xmm7
    test    eax, eax
    jz      .wr
    andps   xmm6, xmm7
    andps   xmm1, xmm7
    movaps  [rbp + DS_A], xmm6
    movaps  [rbp + DS_B], xmm1
    ; до лампочки: затухание 1/d², N·L
    movss   xmm0, [rdi + AL_BX]
    shufps  xmm0, xmm0, 0
    subps   xmm0, [rbp + DS_GX]
    mulps   xmm0, [lt_pxu4]
    movss   xmm1, [rdi + AL_BY]
    shufps  xmm1, xmm1, 0
    subps   xmm1, [rbp + DS_GY]
    mulps   xmm1, [lt_pxu4]
    movss   xmm2, [rdi + AL_BZ]
    shufps  xmm2, xmm2, 0
    subps   xmm2, [rbp + DS_GZ]
    movaps  xmm3, xmm0
    mulps   xmm3, xmm10
    movaps  xmm4, xmm1
    mulps   xmm4, xmm11
    addps   xmm3, xmm4
    movaps  xmm4, xmm2
    mulps   xmm4, xmm12
    addps   xmm3, xmm4                  ; N·(B − P)
    mulps   xmm0, xmm0
    mulps   xmm1, xmm1
    mulps   xmm2, xmm2
    addps   xmm0, xmm1
    addps   xmm0, xmm2                  ; dB²
    maxps   xmm0, [lt_one4]
    rsqrtps xmm1, xmm0
    mulps   xmm3, xmm1
    addps   xmm3, xmm9
    mulps   xmm3, xmm8
    maxps   xmm3, [lt_zero4]
    movss   xmm1, [rdi + AL_IRB]
    shufps  xmm1, xmm1, 0
    mulps   xmm1, xmm0
    movaps  xmm2, [lt_one4]
    subps   xmm2, xmm1
    maxps   xmm2, [lt_zero4]
    mulps   xmm2, xmm2
    mulps   xmm3, xmm2
    movss   xmm1, [rdi + AL_BD2]
    shufps  xmm1, xmm1, 0
    divps   xmm1, xmm0
    minps   xmm1, [lt_one4]
    mulps   xmm3, xmm1
    andps   xmm3, xmm7
    movaps  [rbp + DS_FP], xmm3
    ; маска рамы по дорожкам: чёткая и размытая, билинейно; дальше от рамы — размытее
    movmskps ebx, xmm7                  ; (r14 — номер огня, нужен после)
    xor     ecx, ecx
.wl:
    bt      ebx, ecx
    jnc     .wln
    movss   xmm1, [rbp + DS_A + rcx*4]
    movss   xmm2, [rbp + DS_B + rcx*4]
    cvttss2si eax, xmm1
    cvtsi2ss xmm3, eax
    subss   xmm1, xmm3                  ; fa
    cvttss2si edx, xmm2
    cvtsi2ss xmm3, edx
    subss   xmm2, xmm3                  ; fb
    imul    edx, edx, WM_W
    add     edx, eax
    mov     rax, [rdi + AL_WMS]
    add     rdx, rax
    call    .mask
    movaps  xmm6, xmm3                  ; чёткая
    add     rdx, 256
    call    .mask                       ; размытая
    movss   xmm0, [f_1]
    movss   xmm4, [rbp + DS_T + rcx*4]
    maxss   xmm4, [f_0_05]
    divss   xmm0, xmm4
    subss   xmm0, [f_1]
    mulss   xmm0, [lt_blur]
    minss   xmm0, [f_1]
    subss   xmm3, xmm6
    mulss   xmm3, xmm0
    addss   xmm3, xmm6
    mulss   xmm3, [f_inv255]
    mulss   xmm3, [rbp + DS_FP + rcx*4]
    movss   [rbp + DS_FP + rcx*4], xmm3
.wln:
    inc     ecx
    cmp     ecx, 4
    jb      .wl
.wr:
    ret
; rdx -> маска (левый верхний из 2x2), xmm1 = fa, xmm2 = fb -> xmm3 (0..255); портит xmm4, xmm5, eax
.mask:
    movzx   eax, byte [rdx]
    cvtsi2ss xmm3, eax
    movzx   eax, byte [rdx + 1]
    cvtsi2ss xmm4, eax
    subss   xmm4, xmm3
    mulss   xmm4, xmm1
    addss   xmm3, xmm4
    movzx   eax, byte [rdx + WM_W]
    cvtsi2ss xmm4, eax
    movzx   eax, byte [rdx + WM_W + 1]
    cvtsi2ss xmm5, eax
    subss   xmm5, xmm4
    mulss   xmm5, xmm1
    addss   xmm4, xmm5
    subss   xmm4, xmm3
    mulss   xmm4, xmm2
    addss   xmm3, xmm4
    ret

    ; ----- тень по карте горизонта огня (rdi) -> xmm5: 2x2 сэмпла у точки, сдвинутой к
    ; огню на msd тайлов (спрайт не тенит сам себя); крыша, вне карты — 1
.hz:
    movaps  xmm0, [rbp + DS_DX]
    movaps  xmm1, [rbp + DS_DY]
    movaps  xmm2, xmm0
    mulps   xmm2, xmm0
    movaps  xmm3, xmm1
    mulps   xmm3, xmm1
    addps   xmm2, xmm3
    maxps   xmm2, [lt_1e4v]
    rsqrtps xmm2, xmm2
    mulps   xmm2, [rbp + DS_MSD]
    mulps   xmm0, xmm2
    mulps   xmm1, xmm2
    addps   xmm0, [rbp + DS_GX]
    addps   xmm1, [rbp + DS_GY]
    movss   xmm2, [rdi + AL_HSC]
    shufps  xmm2, xmm2, 0
    mulps   xmm0, xmm2
    mulps   xmm1, xmm2
    cvttps2dq xmm0, xmm0                ; точка выборки, 64 на сэмпл карты
    cvttps2dq xmm1, xmm1
    movdqa  xmm2, xmm0
    pand    xmm2, [lt_d63]
    movdqa  [rbp + DS_A], xmm2          ; доли
    movdqa  xmm3, xmm1
    pand    xmm3, [lt_d63]
    movdqa  [rbp + DS_B], xmm3
    psrad   xmm0, 6
    psrad   xmm1, 6
    movd    xmm4, [rdi + AL_HMX]
    pshufd  xmm4, xmm4, 0
    psubd   xmm0, xmm4                  ; сэмпл карты
    movd    xmm4, [rdi + AL_HMY]
    pshufd  xmm4, xmm4, 0
    psubd   xmm1, xmm4
    mov     eax, [rdi + AL_HMW]
    dec     eax
    movd    xmm4, eax
    pshufd  xmm4, xmm4, 0               ; W − 1
    movdqa  xmm6, xmm4
    pcmpgtd xmm6, xmm0
    movdqa  xmm7, xmm4
    pcmpgtd xmm7, xmm1
    pand    xmm6, xmm7                  ; i, j < W − 1
    pxor    xmm7, xmm7
    pcmpgtd xmm7, xmm0
    pandn   xmm7, xmm6                  ; i ≥ 0
    pxor    xmm6, xmm6
    pcmpgtd xmm6, xmm1
    pandn   xmm6, xmm7                  ; j ≥ 0
    pand    xmm6, [rbp + DS_M]
    movdqa  xmm7, [rbp + DS_OWN]
    pandn   xmm7, xmm6                  ; тенить: в карте, огонь достаёт, не крыша
    movaps  xmm5, [lt_one4]
    movmskps eax, xmm7
    test    eax, eax
    jz      .hzr
    movdqa  [rbp + DS_TMP], xmm7
    pand    xmm0, xmm7
    pand    xmm1, xmm7
    movd    xmm4, [rdi + AL_HMW]
    pshufd  xmm4, xmm4, 0
    pmaddwd xmm1, xmm4
    paddd   xmm1, xmm0
    movdqa  [rbp + DS_T], xmm1          ; индекс j·W + i
    mov     rdx, [rdi + AL_HM]
    mov     r8d, [rdi + AL_HMW]
    mov     eax, [rbp + DS_T]
    movq    xmm4, [rdx + rax*4]
    add     eax, r8d
    movq    xmm0, [rdx + rax*4]
    punpcklqdq xmm4, xmm0               ; дорожка 0: c00 c10 c01 c11
    mov     eax, [rbp + DS_T + 4]
    movq    xmm5, [rdx + rax*4]
    add     eax, r8d
    movq    xmm0, [rdx + rax*4]
    punpcklqdq xmm5, xmm0
    mov     eax, [rbp + DS_T + 8]
    movq    xmm6, [rdx + rax*4]
    add     eax, r8d
    movq    xmm0, [rdx + rax*4]
    punpcklqdq xmm6, xmm0
    mov     eax, [rbp + DS_T + 12]
    movq    xmm7, [rdx + rax*4]
    add     eax, r8d
    movq    xmm0, [rdx + rax*4]
    punpcklqdq xmm7, xmm0
    TR4                                 ; -> xmm2 = c00, xmm1 = c10, xmm3 = c01, xmm6 = c11
    movdqa  xmm4, [rbp + DS_ZI]
    paddd   xmm4, [lt_d64]
    pslld   xmm4, 16                    ; (z + 64) << 16
    HZT     xmm2
    HZT     xmm1
    HZT     xmm3
    HZT     xmm6
    ; углы словами, прижаты к 0..256: [t00 | t10], [t01 | t11]
    packssdw xmm2, xmm1
    packssdw xmm3, xmm6
    pxor    xmm0, xmm0
    pmaxsw  xmm2, xmm0
    pmaxsw  xmm3, xmm0
    pminsw  xmm2, [lt_w256]
    pminsw  xmm3, [lt_w256]
    ; билинейно (сумма весов 4096): по x — ряды, по y — их
    movdqa  xmm0, [rbp + DS_A]          ; fx
    movdqa  xmm5, xmm0
    pslld   xmm5, 16
    psubd   xmm5, xmm0
    paddd   xmm5, [lt_d64]              ; 64 − fx | fx << 16
    pshufd  xmm0, xmm2, 0xEE
    punpcklwd xmm2, xmm0                ; t00 t10 по дорожкам
    pmaddwd xmm2, xmm5
    pshufd  xmm0, xmm3, 0xEE
    punpcklwd xmm3, xmm0
    pmaddwd xmm3, xmm5
    packssdw xmm2, xmm3                 ; ряды 0 | 1 (≤ 16384)
    movdqa  xmm0, [rbp + DS_B]          ; fy
    movdqa  xmm5, xmm0
    pslld   xmm5, 16
    psubd   xmm5, xmm0
    paddd   xmm5, [lt_d64]
    pshufd  xmm0, xmm2, 0xEE
    punpcklwd xmm2, xmm0
    pmaddwd xmm2, xmm5
    cvtdq2ps xmm2, xmm2
    mulps   xmm2, [lt_i1mv]             ; 0..1
    movaps  xmm0, [rbp + DS_TMP]
    andps   xmm2, xmm0
    andnps  xmm0, [lt_one4]
    orps    xmm2, xmm0                  ; где не тенили — 1
    movaps  xmm5, xmm2
.hzr:
    ret

    ; ----- тень фонарика -> xmm2 (0..1 по дорожкам): точка выборки ближе к руке на msd,
    ; её луч в веере (ромбовый угол от оси) и слот (r по 1/16 тайла); 2 луча x 2 слота,
    ; каждый — z против горизонта H + m·r через полутень, результат — билинейно
.fls:
    movss   xmm0, [dl_sx]
    shufps  xmm0, xmm0, 0
    movaps  xmm1, [rbp + DS_GX]
    subps   xmm1, xmm0                  ; от руки, 8.8
    movss   xmm0, [dl_sy]
    shufps  xmm0, xmm0, 0
    movaps  xmm2, [rbp + DS_GY]
    subps   xmm2, xmm0
    movaps  xmm3, xmm1
    mulps   xmm3, xmm1
    movaps  xmm4, xmm2
    mulps   xmm4, xmm2
    addps   xmm3, xmm4
    maxps   xmm3, [lt_1e4v]
    rsqrtps xmm4, xmm3
    mulps   xmm3, xmm4                  ; r
    subps   xmm3, [rbp + DS_MSD]
    maxps   xmm3, [lt_zero4]
    movaps  xmm5, xmm3
    mulps   xmm5, xmm4
    mulps   xmm1, xmm5
    mulps   xmm2, xmm5                  ; точка выборки от руки
    mulps   xmm3, [fl_idsv]
    maxps   xmm3, [lt_0_5v]             ; у самой руки — не в нулевом слоте
    minps   xmm3, [fl_rmaxv]
    movaps  [rbp + DS_F0], xmm3         ; r, слоты
    movss   xmm5, [fl_ax]
    shufps  xmm5, xmm5, 0
    movss   xmm6, [fl_ay]
    shufps  xmm6, xmm6, 0
    movaps  xmm0, xmm1
    mulps   xmm0, xmm5
    movaps  xmm7, xmm2
    mulps   xmm7, xmm6
    addps   xmm0, xmm7                  ; вдоль оси
    mulps   xmm2, xmm5
    mulps   xmm1, xmm6
    subps   xmm2, xmm1                  ; поперёк
    maxps   xmm0, [lt_zero4]
    movaps  xmm1, xmm2
    andps   xmm1, [lt_absm]
    addps   xmm0, xmm1
    addps   xmm0, [lt_1e4v]
    divps   xmm2, xmm0                  ; ромбовый угол
    mulps   xmm2, [fl_kuv]
    addps   xmm2, [fl_u0v]              ; луч, дробный
    maxps   xmm2, [lt_zero4]
    minps   xmm2, [fl_umaxv]
    cvttps2dq xmm0, xmm2
    cvtdq2ps xmm1, xmm0
    subps   xmm2, xmm1
    movaps  [rbp + DS_F1], xmm2         ; доля луча
    cvttps2dq xmm1, xmm3
    cvtdq2ps xmm4, xmm1
    subps   xmm3, xmm4
    movaps  [rbp + DS_F2], xmm3         ; доля слота
    pmaddwd xmm0, [fl_nw]
    paddd   xmm0, xmm1
    movdqa  [rbp + DS_T], xmm0          ; (луч, слот) угла 00
    xor     edx, edx
    call    .flr                        ; луч i
    movaps  [rbp + DS_F6], xmm2
    mov     edx, FL_N*8
    call    .flr                        ; луч i + 1
    subps   xmm2, [rbp + DS_F6]
    mulps   xmm2, [rbp + DS_F1]
    addps   xmm2, [rbp + DS_F6]
    ret
; луч (смещение edx к лучу угла 00) -> xmm2: два слота, между ними — по доле слота
.flr:
    lea     rax, [fl_buf]
    add     rdx, rax
    mov     eax, [rbp + DS_T]
    movdqu  xmm4, [rdx + rax*8]
    mov     eax, [rbp + DS_T + 4]
    movdqu  xmm5, [rdx + rax*8]
    mov     eax, [rbp + DS_T + 8]
    movdqu  xmm6, [rdx + rax*8]
    mov     eax, [rbp + DS_T + 12]
    movdqu  xmm7, [rdx + rax*8]
    TR4                                 ; xmm2 = m, xmm1 = 1/k* (слот j); xmm3, xmm6 — j + 1
    movaps  [rbp + DS_F3], xmm3
    movaps  [rbp + DS_F4], xmm6
    call    .flt
    movaps  [rbp + DS_F5], xmm2
    movaps  xmm2, [rbp + DS_F3]
    movaps  xmm1, [rbp + DS_F4]
    call    .flt
    subps   xmm2, [rbp + DS_F5]
    mulps   xmm2, [rbp + DS_F2]
    addps   xmm2, [rbp + DS_F5]
    ret
; xmm2 = m, xmm1 = 1/k* -> xmm2 = доля света 0..1 (портит xmm0, xmm1, xmm4)
.flt:
    movaps  xmm0, [rbp + DS_F0]
    mulps   xmm2, xmm0
    addps   xmm2, [fl_hv]               ; горизонт, px
    mulps   xmm1, xmm0
    mulps   xmm1, [fl_szv]
    subps   xmm1, [fl_szv]              ; полутень: размер·(r/k* − 1)
    maxps   xmm1, [fl_pen0v]
    movaps  xmm4, [rbp + DS_GZ]
    subps   xmm4, xmm2
    divps   xmm4, xmm1
    addps   xmm4, [lt_0_5v]
    maxps   xmm4, [lt_zero4]
    minps   xmm4, [lt_one4]
    movaps  xmm2, xmm4
    ret

    ; ----- фигуры у огня (rdi): их капсулы (figsh.inc) на отрезке пиксель — огонь;
    ; xmm5 *= тень. Дорожки — где тень что-то решает: свет дошёл, рельеф не затенил
.figs:
    movaps  xmm0, [rbp + DS_F]
    orps    xmm0, [rbp + DS_FP]
    pxor    xmm1, xmm1
    pcmpeqd xmm0, xmm1
    xorps   xmm1, xmm1
    cmpltps xmm1, xmm5
    andnps  xmm0, xmm1
    andps   xmm0, [rbp + DS_M]
    movaps  [rbp + DS_CM], xmm0
    movmskps eax, xmm0
    test    eax, eax
    jz      .fb1
    call    .gbb
    movaps  xmm0, [rbp + DS_DX]
    mulps   xmm0, xmm0
    movaps  xmm1, [rbp + DS_DY]
    mulps   xmm1, xmm1
    addps   xmm0, xmm1
    movaps  xmm1, [rbp + DS_DZ]
    mulps   xmm1, xmm1
    addps   xmm0, xmm1
    maxps   xmm0, [lt_one4]
    movaps  [rbp + DS_AA], xmm0         ; |D|²
    movaps  xmm1, [lt_one4]
    divps   xmm1, xmm0
    movaps  [rbp + DS_IA], xmm1
    movss   xmm0, [lt_hmsz]             ; размер огня: фонарь, окно
    cmp     dword [rdi + AL_KIND], LK_WIN
    jne     .fs0
    movss   xmm0, [lt_hmsz + 4]
.fs0:
    shufps  xmm0, xmm0, 0
    movaps  [rbp + DS_SZL], xmm0
    mov     r10d, [rdi + AL_NF]
    lea     r9, [rdi + AL_FIG]
; фигуры r9 (как AL_FIG: рамка тени, номер записи), r10d > 0 — сколько
.figl:
.fg:
    ; рамка тени фигуры мимо группы — дальше
    movss   xmm0, [rbp + DS_BBX1]
    comiss  xmm0, [r9 + 8]
    jb      .fgn
    movss   xmm0, [rbp + DS_BBX0]
    comiss  xmm0, [r9 + 12]
    ja      .fgn
    movss   xmm0, [rbp + DS_BBY1]
    comiss  xmm0, [r9 + 16]
    jb      .fgn
    movss   xmm0, [rbp + DS_BBY0]
    comiss  xmm0, [r9 + 20]
    ja      .fgn
    mov     eax, [r9 + 24]
    imul    eax, eax, SC_SZ
    lea     rdx, [sc_buf]
    add     rdx, rax
    cmp     dword [rdx + SC_N], 0
    je      .fgn
    call    .cap
.fgn:
    add     r9, AL_FSZ
    dec     r10d
    jnz     .fg
    ret

    ; ----- тень фигуры (rdx -> запись sc_buf) от огня: D = огонь − пиксель (DS_DX..DZ,
    ; px-единицы), |D|², 1/|D|² — DS_AA, DS_IA, размер огня — DS_SZL, дорожки — DS_CM.
    ; Рамкой: ближайшая к оси фигуры точка отрезка по горизонтали — между пикселем и
    ; огнём, луч там ниже верха, сбоку ближе радиуса и полутени; тогда — все капсулы,
    ; тень — наибольшая их доля: xmm5 *= 1 − она (портит xmm0–4, xmm6, xmm7, eax, ecx, r8)
.cap:
    movss   xmm0, [rdx + SC_FX]
    shufps  xmm0, xmm0, 0
    subps   xmm0, [rbp + DS_PX]         ; к фигуре
    movss   xmm1, [rdx + SC_FY]
    shufps  xmm1, xmm1, 0
    subps   xmm1, [rbp + DS_PY]
    movaps  xmm2, [rbp + DS_DX]
    mulps   xmm2, xmm0
    movaps  xmm3, [rbp + DS_DY]
    mulps   xmm3, xmm1
    addps   xmm2, xmm3
    movaps  xmm3, [rbp + DS_DX]
    mulps   xmm3, xmm3
    movaps  xmm4, [rbp + DS_DY]
    mulps   xmm4, xmm4
    addps   xmm3, xmm4
    maxps   xmm3, [lt_1e4v]
    divps   xmm2, xmm3                  ; t ближайшей точки
    movaps  xmm6, [lt_zero4]
    cmpltps xmm6, xmm2
    movaps  xmm7, xmm2
    cmpleps xmm7, [lt_one4]
    andps   xmm6, xmm7
    andps   xmm6, [rbp + DS_CM]
    movaps  xmm7, [rbp + DS_DZ]
    mulps   xmm7, xmm2
    addps   xmm7, [rbp + DS_GZ]         ; луч там
    movss   xmm4, [rdx + SC_TOP]
    addss   xmm4, [rdx + SC_RB]
    shufps  xmm4, xmm4, 0
    movaps  xmm3, [rbp + DS_SZL]
    mulps   xmm3, xmm2
    addps   xmm4, xmm3                  ; + полутень
    cmpltps xmm7, xmm4
    andps   xmm6, xmm7
    movaps  xmm7, [rbp + DS_DX]
    mulps   xmm7, xmm2
    subps   xmm0, xmm7
    mulps   xmm0, xmm0
    movaps  xmm7, [rbp + DS_DY]
    mulps   xmm7, xmm2
    subps   xmm1, xmm7
    mulps   xmm1, xmm1
    addps   xmm0, xmm1                  ; сбоку²
    movaps  xmm1, [rbp + DS_SZL]
    mulps   xmm1, xmm2
    movss   xmm4, [rdx + SC_RB]
    addss   xmm4, [f_1]
    shufps  xmm4, xmm4, 0
    addps   xmm1, xmm4
    mulps   xmm1, xmm1
    cmpltps xmm0, xmm1
    andps   xmm6, xmm0
    movmskps eax, xmm6
    test    eax, eax
    jz      .cpr
    movaps  [rbp + DS_CM2], xmm6
    xorps   xmm0, xmm0
    movaps  [rbp + DS_OM], xmm0
    mov     ecx, [rdx + SC_N]
    lea     r8, [rdx + SC_HSZ]
.cp1:
    ; отсев: прямая пиксель — огонь дальше шара капсулы у всех дорожек — доля 0
    movss   xmm0, [r8 + SC_CX]
    shufps  xmm0, xmm0, 0
    subps   xmm0, [rbp + DS_PX]
    movss   xmm1, [r8 + SC_CY]
    shufps  xmm1, xmm1, 0
    subps   xmm1, [rbp + DS_PY]
    movss   xmm2, [r8 + SC_CZ]
    shufps  xmm2, xmm2, 0
    subps   xmm2, [rbp + DS_GZ]
    movaps  xmm3, xmm0
    mulps   xmm3, [rbp + DS_DX]
    movaps  xmm4, xmm1
    mulps   xmm4, [rbp + DS_DY]
    addps   xmm3, xmm4
    movaps  xmm4, xmm2
    mulps   xmm4, [rbp + DS_DZ]
    addps   xmm3, xmm4
    mulps   xmm0, xmm0
    mulps   xmm1, xmm1
    addps   xmm0, xmm1
    mulps   xmm2, xmm2
    addps   xmm0, xmm2
    mulps   xmm3, xmm3
    mulps   xmm3, [rbp + DS_IA]
    subps   xmm0, xmm3
    movss   xmm1, [r8 + SC_CR]
    addss   xmm1, [rbp + DS_WM]
    mulss   xmm1, xmm1
    shufps  xmm1, xmm1, 0
    cmpltps xmm0, xmm1
    andps   xmm0, [rbp + DS_CM2]
    movmskps eax, xmm0
    test    eax, eax
    jz      .cp2
    call    .cap1
    maxps   xmm0, [rbp + DS_OM]
    movaps  [rbp + DS_OM], xmm0
.cp2:
    add     r8, SC_CSZ
    dec     ecx
    jnz     .cp1
    movaps  xmm0, [rbp + DS_OM]
    andps   xmm0, [rbp + DS_CM2]
    movaps  xmm1, [lt_one4]
    subps   xmm1, xmm0
    mulps   xmm5, xmm1
.cpr:
    ret
; капсула r8 -> xmm0 = доля тени по дорожкам: ближайшие точки отрезка пиксель — огонь
; (t) и оси капсулы (s), расстояние d; край — (ρ − d)/(w0 + размер·t) + 0.5; пиксель
; на самой капсуле — 0
.cap1:
    movss   xmm0, [r8 + SC_AX]
    shufps  xmm0, xmm0, 0
    movaps  xmm1, [rbp + DS_PX]
    subps   xmm1, xmm0
    movss   xmm0, [r8 + SC_AY]
    shufps  xmm0, xmm0, 0
    movaps  xmm2, [rbp + DS_PY]
    subps   xmm2, xmm0
    movss   xmm0, [r8 + SC_AZ]
    shufps  xmm0, xmm0, 0
    movaps  xmm3, [rbp + DS_GZ]
    subps   xmm3, xmm0                  ; r0 = пиксель − A
    ; b = D·E
    movss   xmm0, [r8 + SC_EX]
    shufps  xmm0, xmm0, 0
    mulps   xmm0, [rbp + DS_DX]
    movss   xmm4, [r8 + SC_EY]
    shufps  xmm4, xmm4, 0
    mulps   xmm4, [rbp + DS_DY]
    addps   xmm0, xmm4
    movss   xmm4, [r8 + SC_EZ]
    shufps  xmm4, xmm4, 0
    mulps   xmm4, [rbp + DS_DZ]
    addps   xmm0, xmm4
    movaps  [rbp + DS_CB], xmm0
    ; c = D·r0
    movaps  xmm0, [rbp + DS_DX]
    mulps   xmm0, xmm1
    movaps  xmm4, [rbp + DS_DY]
    mulps   xmm4, xmm2
    addps   xmm0, xmm4
    movaps  xmm4, [rbp + DS_DZ]
    mulps   xmm4, xmm3
    addps   xmm0, xmm4
    movaps  [rbp + DS_CF], xmm0
    ; f = E·r0
    movss   xmm0, [r8 + SC_EX]
    shufps  xmm0, xmm0, 0
    mulps   xmm0, xmm1
    movss   xmm4, [r8 + SC_EY]
    shufps  xmm4, xmm4, 0
    mulps   xmm4, xmm2
    addps   xmm0, xmm4
    movss   xmm4, [r8 + SC_EZ]
    shufps  xmm4, xmm4, 0
    mulps   xmm4, xmm3
    addps   xmm0, xmm4
    ; своя: точка оси у пикселя ближе ρ + 0.6
    movss   xmm4, [r8 + SC_IE]
    shufps  xmm4, xmm4, 0
    mulps   xmm4, xmm0
    maxps   xmm4, [lt_zero4]
    minps   xmm4, [lt_one4]             ; s0
    movss   xmm6, [r8 + SC_EX]
    shufps  xmm6, xmm6, 0
    mulps   xmm6, xmm4
    subps   xmm6, xmm1
    mulps   xmm6, xmm6
    movss   xmm7, [r8 + SC_EY]
    shufps  xmm7, xmm7, 0
    mulps   xmm7, xmm4
    subps   xmm7, xmm2
    mulps   xmm7, xmm7
    addps   xmm6, xmm7
    movss   xmm7, [r8 + SC_EZ]
    shufps  xmm7, xmm7, 0
    mulps   xmm7, xmm4
    subps   xmm7, xmm3
    mulps   xmm7, xmm7
    addps   xmm6, xmm7
    movss   xmm7, [r8 + SC_RS2]
    shufps  xmm7, xmm7, 0
    cmpnltps xmm6, xmm7
    movaps  [rbp + DS_NO], xmm6
    ; t = (b·f − c·e) / (a·e − b²), s = (b·t + f)/e, t = (b·s − c)/a — всё в [0, 1]
    movaps  xmm4, [rbp + DS_CB]
    mulps   xmm4, xmm0
    movss   xmm6, [r8 + SC_EE]
    shufps  xmm6, xmm6, 0
    movaps  xmm7, [rbp + DS_CF]
    mulps   xmm7, xmm6
    subps   xmm4, xmm7
    mulps   xmm6, [rbp + DS_AA]
    movaps  xmm7, [rbp + DS_CB]
    mulps   xmm7, xmm7
    subps   xmm6, xmm7
    maxps   xmm6, [lt_1e4v]
    divps   xmm4, xmm6
    maxps   xmm4, [lt_zero4]
    minps   xmm4, [lt_one4]
    movaps  xmm6, [rbp + DS_CB]
    mulps   xmm6, xmm4
    addps   xmm6, xmm0
    movss   xmm7, [r8 + SC_IE]
    shufps  xmm7, xmm7, 0
    mulps   xmm6, xmm7
    maxps   xmm6, [lt_zero4]
    minps   xmm6, [lt_one4]             ; s
    movaps  xmm4, [rbp + DS_CB]
    mulps   xmm4, xmm6
    subps   xmm4, [rbp + DS_CF]
    mulps   xmm4, [rbp + DS_IA]
    maxps   xmm4, [lt_zero4]
    minps   xmm4, [lt_one4]             ; t
    ; d² = |r0 + D·t − E·s|²
    movaps  xmm0, [rbp + DS_DX]
    mulps   xmm0, xmm4
    addps   xmm0, xmm1
    movss   xmm7, [r8 + SC_EX]
    shufps  xmm7, xmm7, 0
    mulps   xmm7, xmm6
    subps   xmm0, xmm7
    mulps   xmm0, xmm0
    movaps  xmm1, [rbp + DS_DY]
    mulps   xmm1, xmm4
    addps   xmm1, xmm2
    movss   xmm7, [r8 + SC_EY]
    shufps  xmm7, xmm7, 0
    mulps   xmm7, xmm6
    subps   xmm1, xmm7
    mulps   xmm1, xmm1
    addps   xmm0, xmm1
    movaps  xmm1, [rbp + DS_DZ]
    mulps   xmm1, xmm4
    addps   xmm1, xmm3
    movss   xmm7, [r8 + SC_EZ]
    shufps  xmm7, xmm7, 0
    mulps   xmm7, xmm6
    subps   xmm1, xmm7
    mulps   xmm1, xmm1
    addps   xmm0, xmm1
    sqrtps  xmm0, xmm0
    movss   xmm1, [r8 + SC_RHO]
    shufps  xmm1, xmm1, 0
    subps   xmm1, xmm0                  ; ρ − d
    movaps  xmm2, [rbp + DS_SZL]
    mulps   xmm2, xmm4
    addps   xmm2, [sc_w0v]              ; полутень у капсулы
    divps   xmm1, xmm2
    addps   xmm1, [lt_0_5v]
    maxps   xmm1, [lt_zero4]
    minps   xmm1, [lt_one4]
    andps   xmm1, [rbp + DS_NO]
    movaps  xmm0, xmm1
    ret

    ; ----- луна: тени фигур mn_fgl, чья полоса задевает плитку, — капсулами на луче к
    ; луне (D у всех дорожек один) -> mfsh, 0..255 (портит xmm0–7, eax, ecx, edx, r8–r11)
.mf:
    movaps  xmm5, [lt_one4]
    call    .gbb
    movaps  xmm0, [mn_v]
    movaps  [rbp + DS_DX], xmm0
    movaps  xmm0, [mn_v + 32]
    movaps  [rbp + DS_DY], xmm0
    movaps  xmm0, [mn_v + 64]
    movaps  [rbp + DS_DZ], xmm0
    movaps  xmm0, [mn_v + 96]
    movaps  [rbp + DS_AA], xmm0
    movaps  xmm0, [mn_v + 128]
    movaps  [rbp + DS_IA], xmm0
    movaps  xmm0, [mn_v + 160]
    movaps  [rbp + DS_SZL], xmm0
    movaps  xmm0, [rbp + DS_VAL]
    movaps  [rbp + DS_CM], xmm0
    mov     eax, [mn_wm]
    mov     [rbp + DS_WM], eax
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DS_TMF]
    movzx   r11d, word [rdx + rax*2]
.mf1:
    bsf     eax, r11d
    btr     r11d, eax
    imul    eax, eax, MN_FSZ
    lea     r9, [mn_fgl]
    add     r9, rax
    mov     r10d, 1
    call    .figl
    test    r11d, r11d
    jnz     .mf1
    mov     eax, [sc_wmax]
    mov     [rbp + DS_WM], eax
    mulps   xmm5, [lt_255v]
    cvtps2dq xmm5, xmm5
    packssdw xmm5, xmm5
    packuswb xmm5, xmm5
    movd    [rsi + r12 + (mfsh - albedo)], xmm5
    ret

; рамка группы по дорожкам со светом (DS_BBX0..Y1, один раз на группу)
.gbb:
    cmp     dword [rbp + DS_BBOK], 0
    jne     .fb1
    movaps  xmm0, [rbp + DS_VAL]
    movaps  xmm1, xmm0
    andnps  xmm1, [lt_big4]
    movaps  xmm2, xmm0
    andnps  xmm2, [lt_mbig4]
    movaps  xmm3, [rbp + DS_GX]
    andps   xmm3, xmm0
    movaps  xmm4, xmm3
    orps    xmm3, xmm1
    orps    xmm4, xmm2
    movhlps xmm6, xmm3
    minps   xmm3, xmm6
    pshufd  xmm6, xmm3, 1
    minss   xmm3, xmm6
    movss   [rbp + DS_BBX0], xmm3
    movhlps xmm6, xmm4
    maxps   xmm4, xmm6
    pshufd  xmm6, xmm4, 1
    maxss   xmm4, xmm6
    movss   [rbp + DS_BBX1], xmm4
    movaps  xmm3, [rbp + DS_GY]
    andps   xmm3, xmm0
    movaps  xmm4, xmm3
    orps    xmm3, xmm1
    orps    xmm4, xmm2
    movhlps xmm6, xmm3
    minps   xmm3, xmm6
    pshufd  xmm6, xmm3, 1
    minss   xmm3, xmm6
    movss   [rbp + DS_BBY0], xmm3
    movhlps xmm6, xmm4
    maxps   xmm4, xmm6
    pshufd  xmm6, xmm4, 1
    maxss   xmm4, xmm6
    movss   [rbp + DS_BBY1], xmm4
    mov     dword [rbp + DS_BBOK], 1
.fb1:
    ret

; ---------------------------------------------------------------- луч в воздухе
; снег и иней рассеивают фонарик по пути к глазу: на пиксель — точка оси луча,
; ближайшая к лучу зрения (две прямые), хорда сквозь конус там (видимая часть — до
; поверхности пикселя), сила луча в этой точке, закрыт ли он стеной (сетка видимости).
; Без выборок вдоль луча — без шума
beam_pass:
    sub     rsp, 40
    cmp     dword [dl_fon], 0
    je      .out
    lea     rcx, [beam_rows]
    mov     edx, SCR_H
    mov     r8d, 8
    call    par_for
.out:
    add     rsp, 40
    ret

; строки ecx .. edx−1 (с любого потока пула), по 4 пикселя
BM_S        equ 0                       ; s по дорожкам
BM_I        equ 16                      ; сила
BM_END      equ 32
BM_CAM      equ 36
beam_rows:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    rbp
    sub     rsp, 48
    mov     [rsp + BM_END], edx
    mov     eax, [camx]
    add     eax, [camy]
    mov     [rsp + BM_CAM], eax
    mov     r12d, ecx                   ; строка
    lea     rsi, [albedo]
    mov     rdi, [fbbits]
    movss   xmm8, [dl_sx]
    shufps  xmm8, xmm8, 0               ; рука, 8.8
    movss   xmm9, [dl_sy]
    shufps  xmm9, xmm9, 0
    movss   xmm10, [dl_ax]
    shufps  xmm10, xmm10, 0             ; ось луча
    movss   xmm11, [dl_ay]
    shufps  xmm11, xmm11, 0
    movss   xmm12, [dl_az]
    shufps  xmm12, xmm12, 0
    movss   xmm13, [dl_vb]
    shufps  xmm13, xmm13, 0             ; V·a
    movss   xmm14, [dl_vden]
    shufps  xmm14, xmm14, 0             ; 1/(1 − b²)
    movss   xmm15, [dl_frc]
    shufps  xmm15, xmm15, 0             ; дальность
.row:
    imul    ebx, r12d, SCR_W
    mov     r13d, CY0
    sub     r13d, r12d                  ; CY0 − строка
    lea     r14d, [ebx + SCR_W]
.grp:
    movdqu  xmm0, [rsi + rbx*4]
    pxor    xmm1, xmm1
    pcmpeqd xmm0, xmm1                  ; пустота
    movmskps eax, xmm0
    cmp     eax, 15
    je      .next
    movdqa  xmm7, xmm0                  ; маска пустоты
    movdqu  xmm1, [rsi + rbx*4 + (gposb - albedo)]
    movdqa  xmm2, xmm1
    pslld   xmm2, 16
    psrld   xmm2, 16                    ; x
    psrld   xmm1, 16                    ; y
    movdqa  xmm3, xmm2
    paddd   xmm3, xmm1
    movd    xmm4, [rsp + BM_CAM]
    pshufd  xmm4, xmm4, 0
    psubd   xmm3, xmm4
    psrad   xmm3, 5
    movd    xmm4, r13d
    pshufd  xmm4, xmm4, 0
    paddd   xmm3, xmm4                  ; z
    cvtdq2ps xmm0, xmm2
    subps   xmm0, xmm8
    mulps   xmm0, [lt_pxu4]             ; w0 = P − рука, px-единицы
    cvtdq2ps xmm1, xmm1
    subps   xmm1, xmm9
    mulps   xmm1, [lt_pxu4]
    cvtdq2ps xmm2, xmm3
    subps   xmm2, [lt_handz4]
    ; d = V·w0, e = a·w0
    movaps  xmm3, xmm0
    addps   xmm3, xmm1
    mulps   xmm3, [lt_vxy4]
    movaps  xmm4, xmm2
    mulps   xmm4, [lt_vz4]
    addps   xmm3, xmm4                  ; d
    movaps  xmm4, xmm0
    mulps   xmm4, xmm10
    movaps  xmm5, xmm1
    mulps   xmm5, xmm11
    addps   xmm4, xmm5
    movaps  xmm5, xmm2
    mulps   xmm5, xmm12
    addps   xmm4, xmm5                  ; e
    ; s — вдоль оси луча, t — вдоль луча зрения
    movaps  xmm5, xmm3
    mulps   xmm5, xmm13
    movaps  xmm6, xmm4
    subps   xmm6, xmm5
    mulps   xmm6, xmm14                 ; s
    movaps  xmm5, [lt_10v]
    cmpltps xmm5, xmm6
    andnps  xmm7, xmm5                  ; не пустота и s > 10
    movaps  xmm5, xmm6
    cmpltps xmm5, xmm15
    andps   xmm7, xmm5                  ; и в пределах луча
    movmskps eax, xmm7
    test    eax, eax
    jz      .next
    movaps  [rsp + BM_S], xmm6
    mulps   xmm4, xmm13
    subps   xmm4, xmm3
    mulps   xmm4, xmm14                 ; t
    ; расстояние между прямыми в этих точках
    movaps  xmm3, xmm4
    mulps   xmm3, [lt_vxy4]
    movaps  xmm5, xmm6
    mulps   xmm5, xmm10
    addps   xmm0, xmm3
    subps   xmm0, xmm5
    movaps  xmm5, xmm6
    mulps   xmm5, xmm11
    addps   xmm1, xmm3
    subps   xmm1, xmm5
    movaps  xmm3, xmm4
    mulps   xmm3, [lt_vz4]
    movaps  xmm5, xmm6
    mulps   xmm5, xmm12
    addps   xmm2, xmm3
    subps   xmm2, xmm5
    mulps   xmm0, xmm0
    mulps   xmm1, xmm1
    mulps   xmm2, xmm2
    addps   xmm0, xmm1
    addps   xmm0, xmm2                  ; h²
    movaps  xmm1, xmm6
    mulps   xmm1, [lt_btan4]
    mulps   xmm1, xmm1                  ; r²
    movaps  xmm2, xmm0
    cmpltps xmm2, xmm1
    andps   xmm7, xmm2                  ; внутри конуса
    movaps  xmm2, xmm1
    subps   xmm2, xmm0
    maxps   xmm2, [lt_zero4]
    divps   xmm0, xmm1                  ; (h/r)²
    sqrtps  xmm2, xmm2
    movss   xmm3, [dl_visn]
    shufps  xmm3, xmm3, 0
    mulps   xmm2, xmm3                  ; полхорды
    ; видимая часть хорды — от поверхности пикселя к глазу
    movaps  xmm3, xmm4
    addps   xmm3, xmm2
    xorps   xmm5, xmm5
    cmpltps xmm5, xmm3
    andps   xmm7, xmm5
    addps   xmm2, xmm2
    minps   xmm3, xmm2
    movaps  xmm1, [lt_one4]
    subps   xmm1, xmm0
    maxps   xmm1, [lt_zero4]
    mulps   xmm1, xmm1                  ; к оси ярче
    mulps   xmm3, xmm1
    ; затухание вдоль луча, к краю дальности — на нет
    movaps  xmm1, xmm6
    mulps   xmm1, xmm6
    mulps   xmm1, [lt_fk4]
    addps   xmm1, [lt_one4]
    divps   xmm3, xmm1
    movaps  xmm1, xmm15
    subps   xmm1, xmm6
    mulps   xmm1, [lt_bend4]
    maxps   xmm1, [lt_zero4]
    minps   xmm1, [lt_one4]
    mulps   xmm3, xmm1
    movss   xmm1, [dl_bk]
    shufps  xmm1, xmm1, 0
    mulps   xmm3, xmm1
    andps   xmm3, xmm7
    movaps  [rsp + BM_I], xmm3
    movmskps eax, xmm7
    test    eax, eax
    jz      .next
    ; луч не прошёл сквозь стену: видимость точки оси по сетке — по дорожкам
    mov     ebp, eax
    xor     ecx, ecx
.lv:
    bt      ebp, ecx
    jnc     .lvn
    movss   xmm1, [rsp + BM_S + rcx*4]
    movaps  xmm2, xmm1
    mulss   xmm1, [dl_ax]
    mulss   xmm1, [lt_ipxu]
    addss   xmm1, [dl_sx]
    cvttss2si eax, xmm1
    sar     eax, 6
    sub     eax, [gox]
    cmp     eax, GW
    jae     .lv0
    mulss   xmm2, [dl_ay]
    mulss   xmm2, [lt_ipxu]
    addss   xmm2, [dl_sy]
    cvttss2si edx, xmm2
    sar     edx, 6
    sub     edx, [goy]
    cmp     edx, GW
    jae     .lv0
    imul    edx, edx, GW
    add     eax, edx
    cmp     byte [rsi + rax + (losg - albedo)], 0
    jne     .lvn
.lv0:
    mov     dword [rsp + BM_I + rcx*4], 0
.lvn:
    inc     ecx
    cmp     ecx, 4
    jb      .lv
    ; цвет: сила · цвет фонарика, к кадру с насыщением
    movaps  xmm3, [rsp + BM_I]
    movaps  xmm0, xmm3
    mulps   xmm0, [lt_flb]
    movaps  xmm1, xmm3
    mulps   xmm1, [lt_flg]
    mulps   xmm3, [lt_flr]
    cvtps2dq xmm0, xmm0
    cvtps2dq xmm1, xmm1
    cvtps2dq xmm3, xmm3
    packssdw xmm0, xmm0
    packssdw xmm1, xmm1
    packssdw xmm3, xmm3
    punpcklwd xmm0, xmm1                ; B G по пикселям
    pxor    xmm2, xmm2
    punpcklwd xmm3, xmm2                ; R 0
    movdqa  xmm1, xmm0
    punpckldq xmm0, xmm3
    punpckhdq xmm1, xmm3
    packuswb xmm0, xmm1                 ; 4 пикселя BGRA
    movdqu  xmm1, [rdi + rbx*4]
    paddusb xmm1, xmm0
    movdqu  [rdi + rbx*4], xmm1
.next:
    add     ebx, 4
    cmp     ebx, r14d
    jb      .grp
    inc     r12d
    cmp     r12d, [rsp + BM_END]
    jb      .row
    add     rsp, 48
    pop     rbp
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

section .rdata
align 4, db 0
lt_vxy:     dd 0.6113                   ; к глазу: (1, 1, 0.816), нормировано (px-единицы)
lt_vz:      dd 0.4989
lt_btan:    dd 0.5                      ; конус луча в воздухе: tg полуугла
lt_bsig:    dd 0.0022                   ; рассеяние снегом (· (cone_k − 0.7))
lt_bend:    dd 0.0127                   ; к краю дальности: 1 / 4 тайла
lt_m384:    dd -384.0                   ; окно без лампочки: полтора тайла вглубь
lt_w640:    dd 640.0                    ; 2.5 тайла, 8.8
lt_1_5:     dd 1.5
lt_7_5:     dd 7.5
lt_pz26:    dd 26.0                     ; выше верха рамы пятна нет (лампочка выше окна)
lt_wrr:     dd 78.4                     ; 4 тайла, px-единицы
lt_fmul:    dd 1.6                      ; фонарик ярче: на снег он светит вскользь
lt_fclose:  dd 1444.0                   ; (0.15 тайла)², 8.8
lt_fpm:     dd 0.05                     ; рамка тени: полутень растёт с длиной
lt_fm0:     dd 90.0                     ;             и полширины с запасом, 8.8
f_65536:    dd 65536.0

section .data
align 4, db 0
n_al:       dd 0                        ; огней кадра
dl_nfg:     dd 0                        ; фигур кадра
dl_fon:     dd 0                        ; фонарик светит
dl_sx:      dd 0.0                      ; рука, мир 8.8
dl_sy:      dd 0.0
dl_ax:      dd 0.0                      ; ось луча
dl_ay:      dd 0.0
dl_az:      dd 0.0
dl_fr2:     dd 0.0                      ; дальность², px-единицы
dl_frc:     dd 0.0                      ; дальность
dl_vb:      dd 0.0                      ; луч в воздухе: V·a
dl_vden:    dd 0.0                      ; 1 / (1 − b²)
dl_visn:    dd 0.0                      ; 1 / sin угла между лучом и взглядом
dl_bk:      dd 0.0                      ; сила рассеяния
hz_used:    dd 0                        ; занято в hz_pool, дворды

section .bss
alignb 64
dl_tm:      resq DT_W*DT_H              ; маски огней по плиткам экрана
dl_hidm:    resq 1                      ; огни, которым нужна изба под пикселем
alignb 32
al_buf:     resb AL_MAX*AL_SZ           ; огни кадра
dl_fg:      resb DL_FGMAX*16            ; фигуры кадра: x, y (мир 8.8), номер записи тени
wm_buf:     resb 3*512                  ; маски рам
hz_pool:    resd HZ_POOL                ; карты горизонта огней
; адресуются от albedo (light_rows, dl_row) — та же секция
alignb 16
dlb:        resq SCR_W*SCR_H            ; свет огней на пиксель: B, G, R, 0 словами
lgs:        resd GW*GW                  ; сетка света поверхностей: небо·AO + аура, видимость
lgst:       resd GW*GW                  ; свет огней и фонарика на снегу — для отсвета
tm_lut:     resd TM_N                   ; тоновая кривая
alignb 16
dl_nw:      resd NN*4                   ; по нормали: обёртка, 1/(1 + обёртка), отсвет, msd·256
dl_tf:      resb DT_W*DT_H              ; плитка: достаёт ли фонарик
alignb 8
dl_tp:      resq DT_W*DT_H              ; плитка: окна, чьё пятно может в неё лечь
dl_th:      resq DT_W*DT_H              ; плитка: огни, чью тень в ней надо считать
hzt_pool:   resw HZT_POOL               ; пороги карт горизонта (4 на тайл): светло целиком не ниже

section .text
