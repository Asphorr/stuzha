; ----------------------------------------------------------------------------
;  figsh.inc — тени фигур капсулами: на кадр из капсул fig_prep — те, что крупнее
;  кисти (корпус, бёдра, ноги, руки, голова), в мире (px-единицы: тайл = 19.61 по
;  x, y, высота — px), и рамка фигуры. Свет попиксельно (dl_row, dl_row8) проверяет
;  отрезок пиксель — огонь против каждой: где ближе радиуса — тень, край — по
;  полутени (размер огня · доля пути до него), своя капсула себя не тенит
; ----------------------------------------------------------------------------
SC_MAX      equ 16                      ; капсул тени на фигуру
SC_CSZ      equ 64                      ; капсула: A, E = B − A, |E|², 1/|E|², ρ, (ρ + 0.6)²,
                                        ; середина и полудлина + ρ — отсев (+ полутень огня)
SC_AX       equ 0
SC_AY       equ 4
SC_AZ       equ 8
SC_EX       equ 12
SC_EY       equ 16
SC_EZ       equ 20
SC_EE       equ 24
SC_IE       equ 28
SC_RHO      equ 32
SC_RS2      equ 36
SC_CX       equ 40
SC_CY       equ 44
SC_CZ       equ 48
SC_CR       equ 52
SC_HSZ      equ 48                      ; заголовок: число, ступни x, y, верх, радиус рамки
SC_N        equ 0
SC_FX       equ 4
SC_FY       equ 8
SC_TOP      equ 12
SC_RB       equ 16
SC_SZ       equ SC_HSZ + SC_MAX*SC_CSZ

section .text

; после fig_prep: запись тени каждой фигуры кадра (зомби по номеру, игрок — MAXE)
sc_prep:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 32
    xor     r12d, r12d                  ; номер
.e:
    imul    edi, r12d, SC_SZ
    lea     rax, [sc_buf]
    add     rdi, rax
    mov     dword [rdi + SC_N], 0
    imul    esi, r12d, FG_SZ
    lea     rax, [figs]
    add     rsi, rax                    ; запись фигуры
    cmp     dword [rsi + FG_N], 0
    je      .n
    lea     rbx, [pl]
    cmp     r12d, MAXE
    je      .ok
    imul    ebx, r12d, ESZ
    lea     rax, [ents]
    add     rbx, rax
    mov     eax, [rbx + E_ST]
    cmp     eax, ST_WANDER
    jb      .n
    cmp     eax, ST_CORPSE
    jae     .n                          ; труп лежит — тень у него своя, в снегу
.ok:
    call    sc_one
.n:
    inc     r12d
    cmp     r12d, MAXE
    jbe     .e
    add     rsp, 32
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; rbx = сущность, rsi = фигура, rdi = запись тени. Капсулы fz_emit идут по порядку:
; корпус (2), бёдра, [подол], бедро и голень каждой ноги, ступни, плечо и предплечье
; каждой руки, кисти, шея, голова. Тени хватает крупных: корпус от пояса до шеи, подол,
; нога от бедра до лодыжки, рука от плеча до кисти, голова — 6-7 капсул
sc_one:
    movss   xmm8, [rbx + E_X]
    mulss   xmm8, [sc_tile]             ; ступни, px-единицы
    movss   xmm9, [rbx + E_Y]
    mulss   xmm9, [sc_tile]
    movss   [rdi + SC_FX], xmm8
    movss   [rdi + SC_FY], xmm9
    xorps   xmm10, xmm10                ; верх
    xorps   xmm11, xmm11                ; радиус рамки²
    lea     r8, [rdi + SC_HSZ]
    xor     r9d, r9d                    ; капсул
    ; подол: 4-я капсула толще бедра
    mov     r11d, 3
    movss   xmm0, [rsi + FG_HDR + 3*FP_SZ + FP_R]
    comiss  xmm0, [sc_rskirt]
    jbe     .ns
    inc     r11d
.ns:
    lea     eax, [r11d + 14]
    cmp     [rsi + FG_N], eax
    jb      .pd                         ; не человек целиком — без тени
    ; корпус
    xor     eax, eax
    mov     ecx, 1
    movss   xmm0, [rsi + FG_HDR + FP_SZ + FP_R]
    mulss   xmm0, [sc_k96]
    call    .seg
    cmp     r11d, 3
    je      .nsk
    mov     eax, 3
    mov     ecx, 3
    movss   xmm0, [rsi + FG_HDR + 3*FP_SZ + FP_R]
    call    .seg                        ; подол
.nsk:
    ; ноги: бедро -> конец голени
    lea     eax, [r11d]
    lea     ecx, [r11d + 2]
    call    .segr
    lea     eax, [r11d + 1]
    lea     ecx, [r11d + 3]
    call    .segr
    ; руки: плечо -> конец предплечья
    lea     eax, [r11d + 6]
    lea     ecx, [r11d + 8]
    call    .segr
    lea     eax, [r11d + 7]
    lea     ecx, [r11d + 9]
    call    .segr
    ; голова (с шапкой)
    lea     eax, [r11d + 13]
    mov     ecx, eax
    imul    edx, eax, FP_SZ
    movss   xmm0, [rsi + FG_HDR + rdx + FP_R]
    mulss   xmm0, [sc_k110]
    call    .seg
.pd:
    mov     [rdi + SC_N], r9d
    movss   [rdi + SC_TOP], xmm10
    sqrtss  xmm11, xmm11
    movss   [rdi + SC_RB], xmm11
    ret
; eax, ecx — капсулы начала и конца, ρ — у начала
.segr:
    imul    edx, eax, FP_SZ
    movss   xmm0, [rsi + FG_HDR + rdx + FP_R]
; eax = капсула, чьё начало — A, ecx — чей конец — B, xmm0 = ρ (U) -> капсула тени r8
.seg:
    imul    eax, eax, FP_SZ
    lea     r10, [rsi + FG_HDR]
    add     r10, rax
    mulss   xmm0, [sc_u]
    movss   [r8 + SC_RHO], xmm0         ; ρ
    movaps  xmm1, xmm0
    addss   xmm1, [sc_rpad]
    mulss   xmm1, xmm1
    movss   [r8 + SC_RS2], xmm1
    movss   xmm1, [r10 + FP_AX]
    movss   xmm2, [r10 + FP_AY]
    movss   xmm3, [r10 + FP_AZ]
    imul    ecx, ecx, FP_SZ
    lea     r10, [rsi + FG_HDR]
    add     r10, rcx
    movss   xmm4, [r10 + FP_AX]
    addss   xmm4, [r10 + FP_BAX]
    subss   xmm4, xmm1
    movss   xmm5, [r10 + FP_AY]
    addss   xmm5, [r10 + FP_BAY]
    subss   xmm5, xmm2
    movss   xmm6, [r10 + FP_AZ]
    addss   xmm6, [r10 + FP_BAZ]
    subss   xmm6, xmm3                  ; B − A, U
    mulss   xmm1, [sc_u]
    addss   xmm1, xmm8
    movss   [r8 + SC_AX], xmm1          ; A, px-единицы
    mulss   xmm2, [sc_u]
    addss   xmm2, xmm9
    movss   [r8 + SC_AY], xmm2
    mulss   xmm3, [sc_uz]
    movss   [r8 + SC_AZ], xmm3
    mulss   xmm4, [sc_u]
    mulss   xmm5, [sc_u]
    mulss   xmm6, [sc_uz]
    ; шар — отрезок в сотую: те же формулы, без деления на ноль
    movaps  xmm7, xmm4
    mulss   xmm7, xmm4
    movaps  xmm12, xmm5
    mulss   xmm12, xmm5
    addss   xmm7, xmm12
    movaps  xmm12, xmm6
    mulss   xmm12, xmm6
    addss   xmm7, xmm12
    comiss  xmm7, [sc_emin]
    jae     .cap
    movss   xmm6, [sc_eball]
    movaps  xmm7, xmm6
    mulss   xmm7, xmm6
.cap:
    movss   [r8 + SC_EX], xmm4          ; E = B − A
    movss   [r8 + SC_EY], xmm5
    movss   [r8 + SC_EZ], xmm6
    movss   [r8 + SC_EE], xmm7
    movss   xmm12, [f_1]
    divss   xmm12, xmm7
    movss   [r8 + SC_IE], xmm12
    ; отсев: шар вокруг середины — полудлина + ρ (+ полутень огня — у света)
    movaps  xmm12, xmm4
    mulss   xmm12, [f_0_5]
    addss   xmm12, xmm1
    movss   [r8 + SC_CX], xmm12
    movaps  xmm12, xmm5
    mulss   xmm12, [f_0_5]
    addss   xmm12, xmm2
    movss   [r8 + SC_CY], xmm12
    movaps  xmm12, xmm6
    mulss   xmm12, [f_0_5]
    addss   xmm12, xmm3
    movss   [r8 + SC_CZ], xmm12
    movaps  xmm12, xmm7
    sqrtss  xmm12, xmm12
    mulss   xmm12, [f_0_5]
    addss   xmm12, xmm0
    movss   [r8 + SC_CR], xmm12
    ; верх и радиус рамки: концы капсулы от ступней + ρ
    movaps  xmm12, xmm3
    addss   xmm12, xmm6
    maxss   xmm12, xmm3
    addss   xmm12, xmm0
    maxss   xmm10, xmm12
    subss   xmm1, xmm8
    subss   xmm2, xmm9
    call    .rb
    addss   xmm1, xmm4
    addss   xmm2, xmm5
    call    .rb
    add     r8, SC_CSZ
    inc     r9d
    ret
; xmm1, xmm2 = конец капсулы от ступней, xmm0 = ρ: xmm11 = max(xmm11, (|конец| + ρ)²)
.rb:
    movaps  xmm12, xmm1
    mulss   xmm12, xmm1
    movaps  xmm13, xmm2
    mulss   xmm13, xmm2
    addss   xmm12, xmm13
    sqrtss  xmm12, xmm12
    addss   xmm12, xmm0
    mulss   xmm12, xmm12
    maxss   xmm11, xmm12
    ret

; ----- луна (после sc_prep): тени фигур теми же капсулами. Луч от пикселя к луне
; D = (к луне по земле, tg высоты)·Lh, Lh = min(60/tg, 500) px-единиц — там он выше
; любой фигуры с полутенью; полутень растёт на 0.05 на px пути (луна с ореолом).
; mn_fgl — фигуры кадра (игрок первым): как AL_FIG (ступни, рамка тени, номер) + длина
; полосы тени по земле и её поле (мир 8.8) — плиткам (dl_tiles)
MN_FMAX     equ 16
MN_FSZ      equ 48
mn_prep:
    mov     dword [mn_nf], 0
    movss   xmm0, [moon_vec]
    addss   xmm0, [moon_vec + 8]
    comiss  xmm0, [f_0_5]
    jb      .x                          ; направленного света нет (как moon_on)
    movss   xmm0, [mn_h]
    divss   xmm0, [sky_tan]
    minss   xmm0, [mn_lmax]
    movss   [mn_lh], xmm0               ; Lh
    movss   xmm1, [sky_dx]
    mulss   xmm1, xmm0
    movss   xmm2, [sky_dy]
    mulss   xmm2, xmm0
    movss   xmm3, [sky_tan]
    mulss   xmm3, xmm0                  ; D
    movaps  xmm4, xmm1
    mulss   xmm4, xmm1
    movaps  xmm5, xmm2
    mulss   xmm5, xmm2
    addss   xmm4, xmm5
    movaps  xmm5, xmm3
    mulss   xmm5, xmm3
    addss   xmm4, xmm5                  ; |D|²
    movss   xmm5, [f_1]
    divss   xmm5, xmm4
    sqrtss  xmm6, xmm4
    mulss   xmm6, [mn_pen]              ; размер: полутень у t = 1 без w0
    movaps  xmm7, xmm6
    addss   xmm7, [sc_w0v]
    mulss   xmm7, [f_0_5]               ; наибольшая полуширина полутени
    movss   [mn_whalf], xmm7
    addss   xmm7, [f_0_5]
    movss   [mn_wm], xmm7               ; отсев капсул: + полуширина с запасом
    movss   xmm7, [sky_dx]
    xorps   xmm7, [f_signmask]
    movss   [mn_ux], xmm7               ; прочь от луны
    movss   xmm7, [sky_dy]
    xorps   xmm7, [f_signmask]
    movss   [mn_uy], xmm7
    lea     rax, [mn_v]                 ; по 8 дорожек: D, |D|², 1/|D|², размер
    shufps  xmm1, xmm1, 0
    movaps  [rax], xmm1
    movaps  [rax + 16], xmm1
    shufps  xmm2, xmm2, 0
    movaps  [rax + 32], xmm2
    movaps  [rax + 48], xmm2
    shufps  xmm3, xmm3, 0
    movaps  [rax + 64], xmm3
    movaps  [rax + 80], xmm3
    shufps  xmm4, xmm4, 0
    movaps  [rax + 96], xmm4
    movaps  [rax + 112], xmm4
    shufps  xmm5, xmm5, 0
    movaps  [rax + 128], xmm5
    movaps  [rax + 144], xmm5
    shufps  xmm6, xmm6, 0
    movaps  [rax + 160], xmm6
    movaps  [rax + 176], xmm6
    ; фигуры: игрок, затем зомби по номеру
    xor     r8d, r8d
    xor     r9d, r9d                    ; в списке
    lea     r10, [mn_fgl]
.f:
    lea     eax, [r8d - 1]
    test    r8d, r8d
    jnz     .f1
    mov     eax, MAXE
.f1:
    mov     r11d, eax                   ; номер записи тени
    imul    eax, eax, SC_SZ
    lea     rdx, [sc_buf]
    add     rdx, rax
    cmp     dword [rdx + SC_N], 0
    je      .n
    ; полоса: от ступней прочь от луны, пока луч ниже верха с полутенью; поле —
    ; рамка фигуры + полутень + 1 (мир 8.8)
    movss   xmm0, [rdx + SC_TOP]
    addss   xmm0, [mn_whalf]
    divss   xmm0, [sky_tan]
    minss   xmm0, [mn_lh]
    mulss   xmm0, [lt_ipxu]
    movss   [r10 + 28], xmm0
    movss   xmm1, [rdx + SC_RB]
    addss   xmm1, [mn_whalf]
    addss   xmm1, [f_1]
    mulss   xmm1, [lt_ipxu]
    movss   [r10 + 32], xmm1
    movss   xmm2, [rdx + SC_FX]
    mulss   xmm2, [lt_ipxu]
    movss   [r10], xmm2
    movss   xmm3, [rdx + SC_FY]
    mulss   xmm3, [lt_ipxu]
    movss   [r10 + 4], xmm3
    movss   xmm4, [mn_ux]
    mulss   xmm4, xmm0
    addss   xmm4, xmm2                  ; кончик
    movss   xmm5, [mn_uy]
    mulss   xmm5, xmm0
    addss   xmm5, xmm3
    movaps  xmm6, xmm2
    minss   xmm6, xmm4
    subss   xmm6, xmm1
    movss   [r10 + 8], xmm6
    maxss   xmm2, xmm4
    addss   xmm2, xmm1
    movss   [r10 + 12], xmm2
    movaps  xmm6, xmm3
    minss   xmm6, xmm5
    subss   xmm6, xmm1
    movss   [r10 + 16], xmm6
    maxss   xmm3, xmm5
    addss   xmm3, xmm1
    movss   [r10 + 20], xmm3
    mov     [r10 + 24], r11d
    add     r10, MN_FSZ
    inc     r9d
    cmp     r9d, MN_FMAX
    jae     .d
.n:
    inc     r8d
    cmp     r8d, MAXE
    jbe     .f
.d:
    mov     [mn_nf], r9d
.x:
    ret

section .rdata
align 32, db 0
V4 sc_w0v, 0.8                          ; полутень у капсулы не уже, px-единицы
V4 lt_255v, 255.0
sc_tile:    dd 19.6096                  ; тайл, px-единицы
sc_u:       dd 0.86664                  ; U -> px-единицы (19.6096 / 22.627)
sc_uz:      dd 0.8660                   ; U -> px по высоте
sc_rskirt:  dd 3.0                      ; подол толще бедра
sc_k96:     dd 0.96                     ; корпус — чуть уже груди
sc_k110:    dd 1.1                      ; голова с шапкой
sc_rpad:    dd 0.6                      ; своя капсула: пиксель на ней — в пределах ρ + 0.6
sc_wmax:    dd 6.0                      ; полутень у капсулы не шире (окно: 0.8 + 10·t, t ≤ 0.5)
sc_emin:    dd 0.0001
sc_eball:   dd 0.01
mn_h:       dd 60.0                     ; луч к луне у t = 1 — выше фигур с полутенью, px
mn_lmax:    dd 500.0                    ; и не дальше, px-единицы
mn_pen:     dd 0.05                     ; рост полутени на px пути

section .bss
alignb 32
mn_v:       resb 6*32                   ; луна по 8 дорожек: D (x, y, z), |D|², 1/|D|², размер
mn_fgl:     resb MN_FMAX*MN_FSZ         ; фигуры под луной
mn_nf:      resd 1
mn_lh:      resd 1                      ; Lh
mn_whalf:   resd 1                      ; наибольшая полуширина полутени
mn_wm:      resd 1                      ; отсев капсул: запас
mn_ux:      resd 1                      ; прочь от луны по земле
mn_uy:      resd 1
alignb 16
sc_buf:     resb (MAXE + 1)*SC_SZ       ; тени фигур: заголовок и капсулы
mfsh:       resb SCR_W*SCR_H            ; луна: доля без теней фигур 0..255 (dl_row)
dl_tmf:     resw DT_W*DT_H              ; фигуры под луной, чья тень задевает плитку

section .text
