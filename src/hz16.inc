; ----------------------------------------------------------------------------
;  hz16.inc — карта высот 16 ячеек на тайл и карты горизонта огней по ней
; ----------------------------------------------------------------------------
; hm16 — то же, что hmap8 (биты 0-6 — высота, px; бит 7 — под крышей), вдвое мельче:
; скаты изб — по центрам ячеек 1/16, растения и модели двора — по своим картам высот
; 1/32 (у штакетника щели остаются щелями, столб — в ячейку). hmf16 — она же для
; фонарика: в избе вместо крыши — стены и мебель.
; Карты горизонта фонарей и окон печём лучами: из огня PM_NR лучей по «ромбовому» углу
; (четверть + dy/(|dx| + |dy|), без atan), шаг 1/16 тайла; в слоте j луча — наклон
; m = max (h − H)/k по заслонителям на шагах k < j и шаг k* того, что дал наибольший.
; Сэмпл декартовой карты (фонарь — 12 на тайл, окно — 8) берёт ближайший луч и слот
; под собой: горизонт H + m·r, полутень — размер·(r − k*)/k*. Лучи, строки сэмплов и
; пороги плиток (4 на тайл, наибольшее под клеткой) — в пуле, на огонь по очереди.
HM16W       equ MAPW*16                 ; сторона hm16
FL_NR       equ 640                     ; лучей фонарика (веер ±67° от оси)
FL_N        equ 288                     ; слотов луча фонарика: по 1/16 тайла, до 17 тайлов
FL_DS       equ 16                      ; его шаг, мир 8.8
FL_FMAX     equ 16                      ; зомби с тенью в луче
PM_NR       equ 2048                    ; лучей на круг
PM_N        equ 192                     ; слотов на луч (с нулевым)
PM_DS       equ 16                      ; шаг луча, мир 8.8
PM_TMAX     equ 320*320                 ; сэмплов карты огня, не больше
PM_INVN     equ 1024                    ; 1/j до
HZT_POOL    equ 1024*1024               ; слова порогов (4 на тайл)

section .text

; ---------------------------------------------------------------- hm16, hmf16
; из calc_hmap: hmap8 уже с избами, заборами и прочим, но ещё без растений
calc_hm16:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    ; 1/j — лучам
    lea     rdi, [pm_inv]
    mov     dword [rdi], 0
    mov     ecx, 1
.iv:
    cvtsi2ss xmm1, ecx
    movss   xmm0, [f_1]
    divss   xmm0, xmm1
    movss   [rdi + rcx*4], xmm0
    inc     ecx
    cmp     ecx, PM_INVN
    jb      .iv
    ; ----- hmap8 вдвое мельче
    xor     r12d, r12d                  ; строка
.uy:
    mov     eax, r12d
    shr     eax, 1
    add     eax, HPAD
    imul    eax, eax, HW8
    add     eax, HPAD
    lea     rsi, [hmap8]
    add     rsi, rax
    imul    edi, r12d, HM16W
    lea     rax, [hm16]
    add     rdi, rax
    mov     ecx, HM16W/16
.ux:
    movq    xmm0, [rsi]
    punpcklbw xmm0, xmm0
    movdqu  [rdi], xmm0
    add     rsi, 8
    add     rdi, 16
    dec     ecx
    jnz     .ux
    inc     r12d
    cmp     r12d, HM16W
    jb      .uy
    ; ----- избы: скаты и труба по центрам ячеек 1/16
    xor     r13d, r13d                  ; ty
.hy:
    xor     r12d, r12d                  ; tx
.hx:
    mov     r14d, r13d
    shl     r14d, 7
    add     r14d, r12d                  ; тайл
    lea     rdx, [rmap]
    movzx   eax, byte [rdx + r14]
    test    eax, eax
    jz      .hn
    dec     eax
    imul    eax, eax, HS_SZ
    lea     r10, [houses]
    add     r10, rax
    lea     rdx, [tmap]
    movzx   ebx, byte [rdx + r14]
    lea     rdx, [th_m]
    movzx   ebx, byte [rdx + rbx]       ; 2 — под крышей
    xor     r15d, r15d                  ; ячейка тайла
.hc:
    mov     esi, r15d
    and     esi, 15
    shl     esi, 4
    mov     eax, r12d
    shl     eax, 8
    lea     esi, [esi + eax + 8]        ; wx
    mov     ebp, r15d
    shr     ebp, 4
    shl     ebp, 4
    mov     eax, r13d
    shl     eax, 8
    lea     ebp, [ebp + eax + 8]        ; wy
    mov     eax, [r10 + HS_SX]
    shl     eax, 8
    add     eax, 128
    sub     eax, esi
    cdq
    xor     eax, edx
    sub     eax, edx
    cmp     eax, RCH
    jg      .hr
    mov     eax, [r10 + HS_SY]
    shl     eax, 8
    add     eax, 128
    sub     eax, ebp
    cdq
    xor     eax, edx
    sub     eax, edx
    cmp     eax, RCH
    jg      .hr
    mov     eax, [r10 + HS_ZT]
    jmp     .hk
.hr:
    mov     ecx, esi
    mov     edx, ebp
    call    roof_zs
.hk:
    xor     ecx, ecx
    cmp     eax, ecx
    cmovl   eax, ecx
    mov     ecx, 127
    cmp     eax, ecx
    cmovg   eax, ecx
    cmp     ebx, 2
    jne     .hk2
    or      eax, 0x80
.hk2:
    mov     edx, r13d
    shl     edx, 4
    mov     ecx, r15d
    shr     ecx, 4
    add     edx, ecx
    shl     edx, 11
    mov     ecx, r12d
    shl     ecx, 4
    add     edx, ecx
    mov     ecx, r15d
    and     ecx, 15
    add     edx, ecx
    lea     rcx, [hm16]
    mov     [rcx + rdx], al
    inc     r15d
    cmp     r15d, 256
    jb      .hc
.hn:
    inc     r12d
    cmp     r12d, MAPW
    jb      .hx
    inc     r13d
    cmp     r13d, MAPW
    jb      .hy
    call    veg_hm16
    ; ----- фонарику: в избе — стены и мебель вместо крыши
    lea     rsi, [hm16]
    lea     rdi, [hmf16]
    mov     ecx, HM16W*HM16W/8
    rep     movsq
    xor     r13d, r13d                  ; ty
.fy:
    xor     r12d, r12d                  ; tx
.fx:
    mov     r14d, r13d
    shl     r14d, 7
    add     r14d, r12d
    lea     rdx, [rmap]
    cmp     byte [rdx + r14], 0
    je      .fn
    lea     rdx, [tmap]
    movzx   eax, byte [rdx + r14]
    cmp     eax, T_STUMP + 1
    jb      .ft
    xor     eax, eax
.ft:
    lea     rdx, [hf_h]
    movzx   ebx, byte [rdx + rax]       ; высота
    lea     rdx, [hf_in]
    movzx   esi, byte [rdx + rax]       ; отступ от краёв тайла, ячейки
    mov     edi, r13d
    shl     edi, 4
    shl     edi, 11
    mov     eax, r12d
    shl     eax, 4
    add     edi, eax
    lea     rax, [hmf16]
    add     rdi, rax                    ; ячейка (0, 0) тайла
    xor     ecx, ecx                    ; v
.fv:
    xor     edx, edx                    ; u
.fu:
    xor     eax, eax
    cmp     ecx, esi
    jb      .fp
    cmp     edx, esi
    jb      .fp
    mov     r8d, 15
    sub     r8d, esi
    cmp     ecx, r8d
    ja      .fp
    cmp     edx, r8d
    ja      .fp
    mov     eax, ebx
.fp:
    mov     r8d, ecx
    shl     r8d, 11
    add     r8d, edx
    mov     [rdi + r8], al
    inc     edx
    cmp     edx, 16
    jb      .fu
    inc     ecx
    cmp     ecx, 16
    jb      .fv
.fn:
    inc     r12d
    cmp     r12d, MAPW
    jb      .fx
    inc     r13d
    cmp     r13d, MAPW
    jb      .fy
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

; растения и модели двора — в hm16: ячейке 1/16 — клетка 1/32 модели у её середины
; (середина клетки k — 8·(k − c) от основания)
veg_hm16:
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
    mov     [h16_fl], eax
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
    ; ячейки: от (b − 8c − 4) >> 4 до (b + 8(HS − c) − 4) >> 4
    mov     eax, r15d
    shl     eax, 3
    add     eax, 4
    mov     esi, r14d
    sub     esi, eax
    sar     esi, 4                      ; cy
    mov     eax, ebp
    sub     eax, r15d
    shl     eax, 3
    sub     eax, 4
    add     eax, r14d
    sar     eax, 4
    mov     [h16_cy1], eax
.cy:
    cmp     esi, [h16_cy1]
    jg      .tn
    cmp     esi, HM16W
    jae     .cyn
    mov     eax, r15d
    shl     eax, 3
    add     eax, 4
    mov     edi, r13d
    sub     edi, eax
    sar     edi, 4                      ; cx
    mov     eax, ebp
    sub     eax, r15d
    shl     eax, 3
    sub     eax, 4
    add     eax, r13d
    sar     eax, 4
    mov     [h16_cx1], eax
.cx:
    cmp     edi, [h16_cx1]
    jg      .cyn
    cmp     edi, HM16W
    jae     .cxn
    xor     r9d, r9d                    ; высота
    ; клетка модели у середины ячейки (как в hmap8, вдвое чаще): крона и куст —
    ; редкие, столб — в одну ячейку, между штакетинами — щели
    mov     eax, edi
    shl     eax, 4
    sub     eax, r13d
    add     eax, 12
    sar     eax, 3
    lea     r10d, [eax + r15d]
    mov     ecx, esi
    shl     ecx, 4
    sub     ecx, r14d
    add     ecx, 12
    sar     ecx, 3
    add     ecx, r15d
    call    .one
    test    r9d, r9d
    jz      .cxn
    mov     edx, esi
    shl     edx, 11
    add     edx, edi
    lea     r8, [hm16]
    movzx   r10d, byte [r8 + rdx]
    mov     r11d, r10d
    and     r11d, 0x7F
    cmp     r11d, r9d
    jae     .cxn
    and     r10d, 0x80
    or      r10d, r9d
    mov     [r8 + rdx], r10b
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
; r10d = kx, ecx = ky: r9d = max(r9d, клетка модели) (портит edx, r8, r11)
.one:
    cmp     r10d, ebp
    jae     .o
    cmp     ecx, ebp
    jae     .o
    mov     r11d, ecx
    mov     edx, r10d
    test    dword [h16_fl], 0x8000
    jz      .nm
    xchg    r11d, edx                   ; зеркало: оси модели местами
.nm:
    imul    r11d, ebp
    add     r11d, edx
    mov     r8, [rbx + RS_HM]
    movzx   edx, byte [r8 + r11]
    cmp     r9d, edx
    cmovb   r9d, edx
.o:
    ret

; ---------------------------------------------------------------- карта горизонта огня
; r12 -> огонь, xmm0 = радиус карты (тайлы); pm_h, pm_skip, pm_sz, pm_half, pm_f —
; параметры, pm_fld — карта высот. Декартова карта — pm_f сэмплов на четверть тайла,
; начало — на клетке порогов (4 на тайл); пороги — наибольшее под клеткой (с соседом)
hz_bake:
    push    rbx
    push    rsi
    push    rdi
    push    r14
    push    r15
    sub     rsp, 32
    movss   xmm1, [r12 + L_GX]
    movss   [pm_lxf], xmm1
    mulss   xmm1, [f_256]
    cvtss2si eax, xmm1
    mov     [pm_lx], eax
    movss   xmm1, [r12 + L_GY]
    movss   [pm_lyf], xmm1
    mulss   xmm1, [f_256]
    cvtss2si eax, xmm1
    mov     [pm_ly], eax
    mulss   xmm0, [f_4]
    cvttss2si ebx, xmm0
    inc     ebx                         ; радиус, клетки порогов
    lea     eax, [ebx*4 + 2]            ; слотов луча: радиус по 1/16 тайла
    mov     ecx, PM_N - 1
    cmp     eax, ecx
    cmova   eax, ecx
    mov     [pm_n], eax
    cvtss2si eax, [r12 + L_GX]
    sar     eax, 6
    sub     eax, ebx
    mov     [r12 + L_HCX], eax
    cvtss2si eax, [r12 + L_GY]
    sar     eax, 6
    sub     eax, ebx
    mov     [r12 + L_HCY], eax
    lea     r14d, [ebx*2 + 2]           ; сторона, клетки
    mov     [r12 + L_HCW], r14d
    mov     [pm_cw], r14d
    mov     ecx, [pm_f]
    mov     [r12 + L_HF], ecx
    mov     eax, [r12 + L_HCX]
    imul    eax, ecx
    mov     [r12 + L_HMX], eax
    mov     [pm_x0], eax
    mov     eax, [r12 + L_HCY]
    imul    eax, ecx
    mov     [r12 + L_HMY], eax
    mov     [pm_y0], eax
    mov     eax, r14d
    imul    eax, ecx
    inc     eax                         ; сэмплы: клетки · f и сосед последней
    mov     [r12 + L_HMW], eax
    mov     [pm_w], eax
    mov     r15d, eax
    imul    r15d, r15d
    cmp     r15d, PM_TMAX
    ja      .out
    mov     ecx, [hz_used]
    lea     edx, [rcx + r15]
    cmp     edx, HZ_POOL
    ja      .out                        ; места нет — огонь без теней
    mov     r8d, r14d
    imul    r8d, r8d
    mov     r9d, [hzt_used]
    lea     r10d, [r9 + r8]
    cmp     r10d, HZT_POOL
    ja      .out
    mov     [hz_used], edx
    mov     [hzt_used], r10d
    lea     eax, [rcx + 1]
    mov     [r12 + L_HM], eax
    mov     [r12 + L_HTO], r9d
    lea     rax, [hz_pool]
    lea     rax, [rax + rcx*4]
    mov     [pm_out], rax
    lea     rax, [hzt_pool]
    lea     rax, [rax + r9*2]
    mov     [pm_tout], rax
    mov     eax, [r12 + L_WAX]
    mov     [pm_wax], eax
    mov     eax, [r12 + L_WPC]
    mov     [pm_wpc], eax
    mov     eax, [r12 + L_WSG]
    mov     [pm_wsg], eax
    cvtsi2ss xmm0, dword [pm_f]
    movss   xmm1, [f_64]
    divss   xmm1, xmm0
    movss   [pm_fs], xmm1               ; шаг сэмпла, 8.8
    call    pm_circle
    lea     rcx, [pm_rays]
    mov     edx, PM_NR
    mov     r8d, 64
    call    par_for
    lea     rcx, [hz_fill]
    mov     edx, [pm_w]
    mov     r8d, 4
    call    par_for
    lea     rcx, [hz_thr]
    mov     edx, [pm_cw]
    mov     r8d, 4
    call    par_for
.out:
    add     rsp, 32
    pop     r15
    pop     r14
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; лучи по кругу: n -> четверть q = n / (NR/4), t = доля в ней; (1 − t, t), повёрнутое
; на q·90°, нормировано; в pm_dir — шаг луча (мир 8.8 · 256)
pm_circle:
    xor     ecx, ecx
    lea     r8, [pm_dir]
.n:
    mov     eax, ecx
    and     eax, PM_NR/4 - 1
    cvtsi2ss xmm1, eax
    mulss   xmm1, [pm_4nr]              ; t
    movss   xmm0, [f_1]
    subss   xmm0, xmm1                  ; 1 − t
    mov     eax, ecx
    shr     eax, 9                      ; четверть (PM_NR/4 = 512)
    cmp     eax, 1
    jb      .d
    je      .q1
    cmp     eax, 2
    je      .q2
    ; (t, t − 1)
    movaps  xmm2, xmm1
    movaps  xmm1, xmm0
    xorps   xmm1, [f_signmask]
    movaps  xmm0, xmm2
    jmp     .d
.q1:
    ; (−t, 1 − t)
    movaps  xmm2, xmm1
    xorps   xmm2, [f_signmask]
    movaps  xmm1, xmm0
    movaps  xmm0, xmm2
    jmp     .d
.q2:
    ; (t − 1, −t)
    xorps   xmm0, [f_signmask]
    xorps   xmm1, [f_signmask]
.d:
    movaps  xmm2, xmm0
    mulss   xmm2, xmm0
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    addss   xmm2, xmm3
    sqrtss  xmm2, xmm2
    movss   xmm3, [pm_4096]
    divss   xmm3, xmm2
    mulss   xmm0, xmm3
    mulss   xmm1, xmm3
    cvtss2si eax, xmm0
    mov     [r8 + rcx*8], eax
    cvtss2si eax, xmm1
    mov     [r8 + rcx*8 + 4], eax
    inc     ecx
    cmp     ecx, PM_NR
    jb      .n
    ret

; лучи ecx .. edx−1 (пул): слот j = (m, k*) по заслонителям на шагах 1..j−1; луч,
; ушедший с карты, дальше не меняется
pm_rays:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    mov     r14d, ecx
    mov     r15d, edx
    mov     rsi, [pm_fld]
    movss   xmm2, [pm_h]
    mov     r13d, [pm_n]
    mov     ebp, [pm_skip]
    lea     r12, [pm_inv]
.ray:
    cmp     r14d, r15d
    jae     .done
    lea     rax, [pm_dir]
    mov     r10d, [rax + r14*8]
    mov     r11d, [rax + r14*8 + 4]
    mov     r8d, [pm_lx]
    mov     r9d, [pm_ly]
    imul    rdi, r14, PM_N*8
    lea     rax, [pm_buf]
    add     rdi, rax
    mov     rax, [pm_none]
    movss   xmm1, [pm_mnone]
    mov     [rdi], rax
    mov     ecx, 1
.st:
    mov     [rdi + rcx*8], rax          ; слот j: заслонители до шага j
    cmp     ecx, r13d
    jae     .rn
    add     r8d, r10d
    add     r9d, r11d
    cmp     ecx, ebp
    jb      .sn                         ; сам огонь
    mov     edx, r8d
    or      edx, r9d
    cmp     edx, MAPW*256*256
    jae     .fill                       ; ушёл с карты
    mov     edx, r9d
    shr     edx, 12
    shl     edx, 11
    mov     ebx, r8d
    shr     ebx, 12
    add     edx, ebx
    movzx   edx, byte [rsi + rdx]
    and     edx, 0x7F
    jz      .sn
    cvtsi2ss xmm0, edx
    subss   xmm0, xmm2
    mulss   xmm0, [r12 + rcx*4]         ; (h − H)/k
    comiss  xmm0, xmm1
    jbe     .sn
    movss   xmm1, xmm0
    movd    eax, xmm0
    mov     edx, ecx
    shl     rdx, 32
    or      rax, rdx
.sn:
    inc     ecx
    jmp     .st
.fill:
    inc     ecx
.f1:
    cmp     ecx, r13d
    ja      .rn
    mov     [rdi + rcx*8], rax
    inc     ecx
    jmp     .f1
.rn:
    inc     r14d
    jmp     .ray
.done:
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; строки сэмплов ecx .. edx−1 (пул): горизонт + 64 | 65536/полутень << 16 и порог
; (слово: светло целиком не ниже hz + ⌈32768/ip⌉) в pm_thr
hz_fill:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    mov     r14d, ecx
    mov     r15d, edx
    lea     r12, [pm_buf]
.row:
    cmp     r14d, r15d
    jae     .done
    mov     eax, [pm_y0]
    add     eax, r14d
    cvtsi2ss xmm7, eax
    mulss   xmm7, [pm_fs]               ; Y, 8.8
    mov     eax, r14d
    imul    eax, [pm_w]
    mov     rdi, [pm_out]
    lea     rdi, [rdi + rax*4]
    lea     rsi, [pm_thr]
    lea     rsi, [rsi + rax*2]
    xor     ebx, ebx
.px:
    cmp     ebx, [pm_w]
    jae     .rowd
    mov     eax, [pm_x0]
    add     eax, ebx
    cvtsi2ss xmm6, eax
    mulss   xmm6, [pm_fs]               ; X, 8.8
    ; окно светит только наружу
    cmp     dword [pm_half], 0
    je      .nh
    movaps  xmm0, xmm6
    cmp     dword [pm_wax], 0
    jne     .hx
    movaps  xmm0, xmm7
.hx:
    subss   xmm0, [pm_wpc]
    mulss   xmm0, [pm_wsg]
    comiss  xmm0, [f_m40]
    jb      .open
.nh:
    movaps  xmm0, xmm6
    subss   xmm0, [pm_lxf]              ; dx
    movaps  xmm1, xmm7
    subss   xmm1, [pm_lyf]              ; dy
    movaps  xmm2, xmm0
    mulss   xmm2, xmm0
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    addss   xmm2, xmm3
    sqrtss  xmm2, xmm2
    mulss   xmm2, [pm_ids]              ; r, шаги луча
    movaps  xmm3, xmm0
    andps   xmm3, [f_absmask]
    movaps  xmm4, xmm1
    andps   xmm4, [f_absmask]
    addss   xmm3, xmm4                  ; |dx| + |dy|
    comiss  xmm3, [f_0_01]
    jb      .open                       ; сам огонь
    ; ромбовый угол: четверть по знакам, доля — dy/s или dx/s
    xorps   xmm5, xmm5
    comiss  xmm1, xmm5
    jb      .dn
    comiss  xmm0, xmm5
    jb      .a1
    movaps  xmm4, xmm1
    divss   xmm4, xmm3                  ; dy/s
    jmp     .ak
.a1:
    movaps  xmm4, xmm0
    divss   xmm4, xmm3
    movss   xmm5, [f_1]
    subss   xmm5, xmm4                  ; 1 − dx/s
    movaps  xmm4, xmm5
    jmp     .ak
.dn:
    comiss  xmm0, xmm5
    jae     .a3
    movaps  xmm4, xmm1
    divss   xmm4, xmm3
    movss   xmm5, [f_2]
    subss   xmm5, xmm4                  ; 2 − dy/s
    movaps  xmm4, xmm5
    jmp     .ak
.a3:
    movaps  xmm4, xmm0
    divss   xmm4, xmm3
    addss   xmm4, [f_3]                 ; 3 + dx/s
.ak:
    mulss   xmm4, [pm_nr4]
    cvtss2si eax, xmm4
    and     eax, PM_NR - 1              ; луч
    cvttss2si ecx, xmm2
    cmp     ecx, [pm_n]
    cmovg   ecx, [pm_n]                 ; слот
    imul    eax, eax, PM_N
    add     eax, ecx
    mov     rax, [r12 + rax*8]
    mov     rdx, rax
    shr     rdx, 32                     ; k*
    test    edx, edx
    jz      .open
    movd    xmm3, eax                   ; m
    mulss   xmm3, xmm2
    addss   xmm3, [pm_h]                ; горизонт, px
    cvtss2si eax, xmm3
    add     eax, 64
    xor     ecx, ecx
    cmp     eax, ecx
    cmovl   eax, ecx
    mov     ecx, 255
    cmp     eax, ecx
    cmovg   eax, ecx
    mov     r8d, eax                    ; горизонт + 64
    cvtsi2ss xmm4, edx
    movaps  xmm5, xmm2
    subss   xmm5, xmm4
    divss   xmm5, xmm4
    mulss   xmm5, [pm_sz]
    maxss   xmm5, [pm_pen0]             ; полутень, px
    movss   xmm4, [f_65536]
    divss   xmm4, xmm5
    cvttss2si ecx, xmm4
    mov     eax, 32767
    cmp     ecx, eax
    cmova   ecx, eax                    ; ip
    mov     eax, ecx
    shl     eax, 16
    or      eax, r8d
    mov     [rdi + rbx*4], eax
    ; порог: hz + ⌈32768/ip⌉
    mov     eax, 32767
    add     eax, ecx
    xor     edx, edx
    div     ecx
    lea     eax, [rax + r8 - 64]
    mov     ecx, 32767
    cmp     eax, ecx
    cmovg   eax, ecx
    mov     [rsi + rbx*2], ax
    inc     ebx
    jmp     .px
.open:
    mov     dword [rdi + rbx*4], 0x7FFF0000 ; горизонт −64: виден весь столб
    mov     word [rsi + rbx*2], -62
    inc     ebx
    jmp     .px
.rowd:
    inc     r14d
    jmp     .row
.done:
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; строки клеток порогов ecx .. edx−1 (пул): наибольший порог сэмплов под клеткой
; (f + 1 по каждой оси: и сосед, которого возьмёт билинейка)
hz_thr:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    mov     r14d, ecx
    mov     r15d, edx
    mov     r12d, [pm_f]
    mov     r13d, [pm_w]
.row:
    cmp     r14d, r15d
    jae     .done
    xor     ebx, ebx                    ; клетка в ряду
.c:
    cmp     ebx, [pm_cw]
    jae     .rowd
    mov     eax, r14d
    imul    eax, r12d                   ; первый ряд сэмплов
    imul    eax, r13d
    mov     ecx, ebx
    imul    ecx, r12d
    add     eax, ecx
    lea     rsi, [pm_thr]
    lea     rsi, [rsi + rax*2]
    mov     r8d, -32768
    xor     r9d, r9d                    ; ряд
.b:
    xor     r10d, r10d
.a:
    movsx   eax, word [rsi + r10*2]
    cmp     r8d, eax
    cmovl   r8d, eax
    inc     r10d
    cmp     r10d, r12d
    jbe     .a
    lea     rsi, [rsi + r13*2]
    inc     r9d
    cmp     r9d, r12d
    jbe     .b
    mov     eax, r14d
    imul    eax, [pm_cw]
    add     eax, ebx
    mov     rdi, [pm_tout]
    mov     [rdi + rax*2], r8w
    inc     ebx
    jmp     .c
.rowd:
    inc     r14d
    jmp     .row
.done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- тень фонарика
; на кадр, фоном после развёртки луны (sky_job; фонарик светит — dl_frame уже решил):
; веер FL_NR лучей из руки по «ромбовому» углу
; от оси, p = поперёк / (вдоль + |поперёк|) от −FL_P до FL_P, шаг 1/16 тайла; в слоте j
; — наклон m = max (h − H)/k по заслонителям на шагах до j и 1/k* того, что дал
; наибольший (dl_row: горизонт H + m·r, полутень — размер·(r/k* − 1)). Рука в избе —
; лучи по hmf16 (стены и мебель), снаружи — по hm16
fl_map:
    push    rbx
    sub     rsp, 32
    movss   xmm0, [pl + E_FX]
    movss   xmm1, [pl + E_FY]
    movaps  xmm2, xmm0
    mulss   xmm2, xmm0
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    addss   xmm2, xmm3
    maxss   xmm2, [f_0_01]
    sqrtss  xmm2, xmm2
    divss   xmm0, xmm2
    divss   xmm1, xmm2
    movss   [fl_ax], xmm0               ; ось по горизонтали
    movss   [fl_ay], xmm1
    movss   xmm0, [dl_sx]
    mulss   xmm0, [f_256]
    cvtss2si eax, xmm0
    mov     [fl_lx], eax                ; рука, мир 8.8 · 256
    movss   xmm0, [dl_sy]
    mulss   xmm0, [f_256]
    cvtss2si eax, xmm0
    mov     [fl_ly], eax
    ; слотов: дальность · 16 + 3
    movss   xmm0, [fl_reach]
    mulss   xmm0, [fl_spt]
    cvttss2si eax, xmm0
    add     eax, 3
    mov     ecx, FL_N - 1
    cmp     eax, ecx
    cmova   eax, ecx
    mov     [fl_n], eax
    cvtsi2ss xmm0, eax
    subss   xmm0, [fl_eps]
    shufps  xmm0, xmm0, 0
    movaps  [fl_rmaxv], xmm0            ; слот выборки не дальше последнего
    movaps  [fl_rmaxv + 16], xmm0
    ; рука в избе — стены и мебель вместо крыши
    lea     rax, [hm16]
    cvttss2si ecx, [dl_sx]
    shr     ecx, 8
    cvttss2si edx, [dl_sy]
    shr     edx, 8
    cmp     ecx, MAPW
    jae     .fld
    cmp     edx, MAPW
    jae     .fld
    shl     edx, 7
    add     edx, ecx
    lea     r8, [rmap]
    cmp     byte [r8 + rdx], 0
    je      .fld
    lea     rax, [hmf16]
.fld:
    mov     [fl_fld], rax
    ; лучи: p = −P + (n + 0.5)·2P/NR; (1 − |p|, p) вдоль и поперёк оси, нормировано
    xor     ecx, ecx
    lea     r8, [fl_dir]
.d:
    cvtsi2ss xmm0, ecx
    addss   xmm0, [f_0_5]
    mulss   xmm0, [fl_dp]
    subss   xmm0, [fl_P]                ; p
    movaps  xmm1, xmm0
    andps   xmm1, [f_absmask]
    movss   xmm2, [f_1]
    subss   xmm2, xmm1                  ; вдоль
    movaps  xmm3, xmm2
    mulss   xmm3, xmm2
    movaps  xmm4, xmm0
    mulss   xmm4, xmm0
    addss   xmm3, xmm4
    sqrtss  xmm3, xmm3
    movss   xmm4, [fl_step]
    divss   xmm4, xmm3
    mulss   xmm2, xmm4
    mulss   xmm0, xmm4
    movss   xmm5, [fl_ax]
    movss   xmm6, [fl_ay]
    movaps  xmm3, xmm2
    mulss   xmm3, xmm5
    movaps  xmm4, xmm0
    mulss   xmm4, xmm6
    subss   xmm3, xmm4                  ; x = вдоль·ax − поперёк·ay
    mulss   xmm2, xmm6
    mulss   xmm0, xmm5
    addss   xmm2, xmm0                  ; y = вдоль·ay + поперёк·ax
    cvtss2si eax, xmm3
    mov     [r8 + rcx*8], eax
    cvtss2si eax, xmm2
    mov     [r8 + rcx*8 + 4], eax
    inc     ecx
    cmp     ecx, FL_NR
    jb      .d
    xor     ecx, ecx                    ; фоном (sky_job): пул занят сеткой света
    mov     edx, FL_NR
    call    fl_rays
    ; зомби в веере: тень — от фигуры прочь от руки до конца дальности, шире вдали
    ; (рамка — как AL_FIG у огней; dl_row проверяет их капсулы)
    movss   xmm6, [fl_reach]
    mulss   xmm6, [f_256]               ; дальность, 8.8
    xor     r9d, r9d
    lea     rdx, [dl_fg + 16]           ; без игрока
    mov     ecx, [dl_nfg]
    dec     ecx
    jle     .fgd
    lea     r8, [fl_fgl]
.fg:
    movss   xmm0, [rdx]
    subss   xmm0, [dl_sx]
    movss   xmm1, [rdx + 4]
    subss   xmm1, [dl_sy]
    movaps  xmm2, xmm0
    mulss   xmm2, xmm0
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    addss   xmm2, xmm3
    sqrtss  xmm2, xmm2                  ; от руки, 8.8
    comiss  xmm2, xmm6
    jae     .fgn
    comiss  xmm2, [fl_fmin]
    jb      .fgn
    divss   xmm0, xmm2
    divss   xmm1, xmm2                  ; прочь от руки
    movaps  xmm3, xmm0
    mulss   xmm3, [fl_ax]
    movaps  xmm4, xmm1
    mulss   xmm4, [fl_ay]
    addss   xmm3, xmm4
    comiss  xmm3, [fl_fcos]
    jb      .fgn                        ; вне веера
    ; лучи веера, что задевают фигуру: касательные ±радиус от середины (ромбовый угол),
    ; и слот, с которого её тень может начаться, — плиткам (dl_tiles)
    movaps  xmm8, xmm3                  ; вдоль (доля)
    movaps  xmm9, xmm1
    mulss   xmm9, [fl_ax]
    movaps  xmm4, xmm0
    mulss   xmm4, [fl_ay]
    subss   xmm9, xmm4                  ; поперёк (доля)
    movaps  xmm10, xmm8
    mulss   xmm10, xmm2                 ; a — середина вдоль, 8.8
    movaps  xmm11, xmm9
    mulss   xmm11, xmm2                 ; c — поперёк
    movss   xmm12, [fl_frb]
    movaps  xmm4, xmm9
    mulss   xmm4, xmm12                 ; поперёк·R
    movaps  xmm5, xmm8
    mulss   xmm5, xmm12                 ; вдоль·R
    movaps  xmm12, xmm10
    subss   xmm12, xmm4
    movaps  xmm13, xmm11
    addss   xmm13, xmm5
    call    .fp                         ; касательная 1 -> xmm12
    movaps  xmm7, xmm12
    movaps  xmm12, xmm10
    addss   xmm12, xmm4
    movaps  xmm13, xmm11
    subss   xmm13, xmm5
    call    .fp                         ; касательная 2
    movaps  xmm4, xmm7
    minss   xmm4, xmm12
    maxss   xmm7, xmm12
    cvttss2si eax, xmm4
    dec     eax
    xor     r10d, r10d
    cmp     eax, r10d
    cmovl   eax, r10d
    cvttss2si r10d, xmm7
    add     r10d, 2
    mov     r11d, FL_NR - 1
    cmp     r10d, r11d
    cmovg   r10d, r11d
    shl     r10d, 16
    or      eax, r10d
    lea     r11, [fl_frl]
    mov     [r11 + r9*4], eax           ; лучи: первый | последний << 16
    movaps  xmm4, xmm2
    subss   xmm4, [fl_frb]
    mulss   xmm4, [fl_idsv]
    cvttss2si eax, xmm4
    xor     r10d, r10d
    cmp     eax, r10d
    cmovl   eax, r10d
    lea     r11, [fl_fr0]
    mov     [r11 + r9*4], eax           ; слот начала тени
    movaps  xmm3, xmm6
    subss   xmm3, xmm2                  ; тень до конца дальности
    mulss   xmm0, xmm3
    mulss   xmm1, xmm3
    addss   xmm0, [rdx]                 ; её конец
    addss   xmm1, [rdx + 4]
    movaps  xmm4, xmm6
    divss   xmm4, xmm2
    mulss   xmm4, [fl_frb]
    addss   xmm4, [fl_fpad]             ; поле: радиус фигуры, раздутый к концу
    movss   xmm3, [rdx]
    movaps  xmm5, xmm3
    minss   xmm3, xmm0
    maxss   xmm5, xmm0
    subss   xmm3, xmm4
    addss   xmm5, xmm4
    movss   [r8 + 8], xmm3
    movss   [r8 + 12], xmm5
    movss   xmm3, [rdx + 4]
    movaps  xmm5, xmm3
    minss   xmm3, xmm1
    maxss   xmm5, xmm1
    subss   xmm3, xmm4
    addss   xmm5, xmm4
    movss   [r8 + 16], xmm3
    movss   [r8 + 20], xmm5
    mov     eax, [rdx + 8]
    mov     [r8 + 24], eax              ; номер записи тени
    add     r8, AL_FSZ
    inc     r9d
    cmp     r9d, FL_FMAX
    jae     .fgd
.fgn:
    add     rdx, 16
    dec     ecx
    jnz     .fg
.fgd:
    mov     [fl_nf], r9d
    add     rsp, 32
    pop     rbx
    ret
; точка веера xmm12 = вдоль, xmm13 = поперёк (8.8) -> xmm12 = луч, дробный (как у dl_row)
.fp:
    maxss   xmm12, [lt_zero4]
    movaps  xmm14, xmm13
    andps   xmm14, [f_absmask]
    addss   xmm12, xmm14
    addss   xmm12, [lt_1e4v]
    divss   xmm13, xmm12
    mulss   xmm13, [fl_kuv]
    addss   xmm13, [fl_u0v]
    maxss   xmm13, [lt_zero4]
    minss   xmm13, [fl_umaxv]
    movaps  xmm12, xmm13
    ret

; лучи фонарика ecx .. edx−1 (пул): слот j = (m, 1/k*) по заслонителям до шага j
fl_rays:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    mov     r14d, ecx
    mov     r15d, edx
    mov     rsi, [fl_fld]
    movss   xmm2, [lt_handz]            ; H
    mov     r13d, [fl_n]
    lea     r12, [pm_inv]
.ray:
    cmp     r14d, r15d
    jae     .done
    lea     rax, [fl_dir]
    mov     r10d, [rax + r14*8]
    mov     r11d, [rax + r14*8 + 4]
    mov     r8d, [fl_lx]
    mov     r9d, [fl_ly]
    imul    rdi, r14, FL_N*8
    lea     rax, [fl_buf]
    add     rdi, rax
    mov     rax, [fl_none]
    movss   xmm1, [pm_mnone]
    mov     [rdi], rax
    mov     ebp, FL_N + 1               ; первый слот с заслонителем
    mov     ecx, 1
.st:
    mov     [rdi + rcx*8], rax          ; слот j: заслонители до шага j
    cmp     ecx, r13d
    jae     .rn
    add     r8d, r10d
    add     r9d, r11d
    mov     edx, r8d
    or      edx, r9d
    cmp     edx, MAPW*256*256
    jae     .fill                       ; ушёл с карты
    mov     edx, r9d
    shr     edx, 12
    shl     edx, 11
    mov     ebx, r8d
    shr     ebx, 12
    add     edx, ebx
    movzx   edx, byte [rsi + rdx]
    and     edx, 0x7F
    jz      .sn
    cvtsi2ss xmm0, edx
    subss   xmm0, xmm2
    mulss   xmm0, [r12 + rcx*4]         ; (h − H)/k
    comiss  xmm0, xmm1
    jbe     .sn
    movss   xmm1, xmm0
    movd    eax, xmm0
    mov     edx, [r12 + rcx*4]
    shl     rdx, 32
    or      rax, rdx
    lea     edx, [rcx + 1]
    cmp     ebp, edx
    cmova   ebp, edx
.sn:
    inc     ecx
    jmp     .st
.fill:
    inc     ecx
.f1:
    cmp     ecx, r13d
    ja      .rn
    mov     [rdi + rcx*8], rax
    inc     ecx
    jmp     .f1
.rn:
    lea     rax, [fl_first]
    mov     [rax + r14*4], ebp
    inc     r14d
    jmp     .ray
.done:
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

section .rdata
align 32, db 0
V4 fl_idsv, 0.0625                      ; 1 / FL_DS
V4 fl_kuv, 457.142857                   ; FL_NR / (2·FL_P)
V4 fl_u0v, 319.5                        ; FL_NR/2 − 0.5
V4 fl_umaxv, 638.999                    ; FL_NR − 1.001
V4 fl_nw, FL_N                          ; луч · FL_N (pmaddwd)
V4 fl_hv, 20.0                          ; высота руки, px
V4 fl_szv, 1.0                          ; размер стекла фонарика, px
V4 fl_pen0v, 0.75                       ; полутень не уже, px
V4 lt_i255v, 0.00392157
V4 fl_szlv, 1.5                         ; размер стекла — полутени фигур, px
align 8, db 0
fl_fmin:    dd 16.0                     ; фигура у самой руки — без тени, 8.8
fl_fcos:    dd 0.2                      ; веер: cos до края (78°)
fl_frb:     dd 90.0                     ; радиус фигуры с запасом, 8.8
fl_fpad:    dd 32.0
fl_none:    dq 0x3F800000C9742400       ; нет заслонителя: m = −1e6, 1/k* = 1
fl_dp:      dd 0.0021875                ; 2·FL_P / FL_NR
fl_P:       dd 0.7
fl_step:    dd 4096.0                   ; шаг луча: FL_DS · 256
fl_spt:     dd 16.0                     ; слотов на тайл
fl_eps:     dd 0.001
pm_none:    dq 0x00000000C9742400       ; k* = 0 (нет заслонителя), m = −1e6
pm_mnone:   dd -1.0e6
pm_4nr:     dd 0.001953125              ; 4 / PM_NR
pm_nr4:     dd 512.0                    ; PM_NR / 4
pm_4096:    dd 4096.0                   ; шаг луча: PM_DS · 256
pm_ids:     dd 0.0625                   ; 1 / PM_DS
pm_pen0:    dd 1.0                      ; полутень не уже, px
; фонарику в избе: высота (px) и отступ от краёв тайла (ячейки 1/16) по виду тайла
hf_h:       db 0, 0, 0, 36, 36, 36, 0, 0, 0, 0, 30, 9, 0, 36, 30, 0
            db 0, 0, 0, 0, 0, 0, 9, 30, 8, 6, 0, 0, 0, 0
hf_in:      db 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0
            db 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 0, 0, 0, 0
%if ($ - hf_in) != T_STUMP + 1
  %error "hf_in: не все виды тайлов"
%endif

section .data
align 32, db 0
fl_rmaxv:   times 8 dd 0.0              ; последний слот луча фонарика − 0.001
fl_fld:     dq 0                        ; карта высот лучей фонарика
fl_ax:      dd 0.0                      ; ось фонарика по горизонтали
fl_ay:      dd 0.0
fl_lx:      dd 0                        ; рука, мир 8.8 · 256
fl_ly:      dd 0
fl_n:       dd 0                        ; последний слот
fl_nf:      dd 0                        ; зомби с тенью в луче
align 8, db 0
pm_fld:     dq 0                        ; карта высот лучей
pm_out:     dq 0                        ; карта горизонта (hz_pool)
pm_tout:    dq 0                        ; её пороги (hzt_pool)
pm_lx:      dd 0                        ; огонь, мир 8.8 · 256
pm_ly:      dd 0
pm_lxf:     dd 0.0                      ; он же, 8.8
pm_lyf:     dd 0.0
pm_h:       dd 0.0                      ; высота огня, px
pm_n:       dd 0                        ; последний слот луча
pm_skip:    dd 0                        ; шагов — сам огонь
pm_sz:      dd 0.0                      ; размер огня (полутень), px
pm_half:    dd 0                        ; окно: только снаружи
pm_wax:     dd 0
pm_wpc:     dd 0.0
pm_wsg:     dd 0.0
pm_f:       dd 0                        ; сэмплов на четверть тайла
pm_fs:      dd 0.0                      ; шаг сэмпла, 8.8
pm_x0:      dd 0                        ; начало карты, сэмплы
pm_y0:      dd 0
pm_w:       dd 0                        ; сторона, сэмплы
pm_cw:      dd 0                        ; сторона порогов, клетки
hzt_used:   dd 0                        ; занято в hzt_pool, слова
h16_fl:     dd 0
h16_cx1:    dd 0
h16_cy1:    dd 0

section .bss
alignb 16
hm16:       resb HM16W*HM16W            ; высоты 16 на тайл
hmf16:      resb HM16W*HM16W            ; они же фонарику (в избе — стены и мебель)
pm_buf:     resq PM_NR*PM_N             ; лучи огня: (m, k*) по слотам
pm_dir:     resd PM_NR*2                ; шаг луча
pm_inv:     resd PM_INVN                ; 1/j
pm_thr:     resw PM_TMAX                ; пороги сэмплов огня
alignb 16
fl_buf:     resq FL_NR*FL_N             ; лучи фонарика: (m, 1/k*) по слотам
fl_dir:     resd FL_NR*2                ; шаг луча
fl_first:   resd FL_NR                  ; первый слот, где есть заслонитель (FL_N + 1 — нигде)
alignb 16
fl_fgl:     resb FL_FMAX*AL_FSZ         ; зомби в луче: как AL_FIG (рамка тени, номер)
fl_frl:     resd FL_FMAX                ; их лучи: первый | последний << 16
fl_fr0:     resd FL_FMAX                ; слот, с которого тень
dl_tfg:     resw DT_W*DT_H              ; плитка: чьи тени от фонарика её задевают (биты)

section .text
