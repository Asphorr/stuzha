; ----------------------------------------------------------------------------
;  wind.inc — ветер. Погода за ночь: к вечеру крепчает при ясном небе (позёмка),
;  к полуночи метель — во фронте ветер поворачивает с ЮЮВ на ЮЮЗ, — под утро
;  стихает. Поле ветра по месту: порывы — два слоя шума, бегущие по ветру;
;  затишье за избами, заборами, елями и подпор перед ними — развёртка по ветру
;  над картой заслонителей. Сила 1 ≈ 12 м/с ≈ 6 тайлов/с.
;  Здесь же: маска позёмки на кадр, текстуры полос и инея, стрелка ветра в HUD.
; ----------------------------------------------------------------------------
section .text

; ---------------------------------------------------------------- новая ночь
; события погоды — свои на каждую игру (в снимках — ровно как в таблице),
; карта заслонителей, затишье — пересчитать
wind_init:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32
    call    wind_luts
    lea     rsi, [wv_def]
    lea     rdi, [wv_ev]
    mov     ecx, 12
    rep     movsd
    cmp     dword [shotmode], 0
    jne     .fixed
    lea     rbx, [wv_ev]
    lea     rsi, [wv_rnd]
    mov     edi, 3
.ev:
    call    wrandf                      ; центр ± разброс
    subss   xmm0, [f_0_5]
    addss   xmm0, xmm0
    mulss   xmm0, [rsi]
    addss   xmm0, [rbx]
    movss   [rbx], xmm0
    call    wrandf                      ; ширина ×0.8..1.25
    mulss   xmm0, [f_0_45]
    addss   xmm0, [f_0_8]
    mulss   xmm0, [rbx + 4]
    movss   [rbx + 4], xmm0
    call    wrandf                      ; сила ×0.85..1.15
    mulss   xmm0, [f_0_3]
    addss   xmm0, [f_0_85]
    mulss   xmm0, [rbx + 8]
    movss   [rbx + 8], xmm0
    add     rbx, 16
    add     rsi, 4
    dec     edi
    jnz     .ev
.fixed:
    call    wind_obst
    mov     dword [wsh_th], __float32__(99.0)
    mov     dword [pz_f1], 0
    mov     dword [pz_f2], 0
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; доля затишья по высоте заслона S (px): за ним 255·(1 − e^(−S/15.8)),
; перед лицом стены — 0.6 от того же при e^(−S/12)
wind_luts:
    movss   xmm0, [f_1]
    movss   xmm1, [f_1]
    lea     rdx, [wsl_lee]
    lea     r8, [wsl_front]
    xor     ecx, ecx
.l:
    movss   xmm2, [f_1]
    subss   xmm2, xmm0
    mulss   xmm2, [f_255]
    cvtss2si eax, xmm2
    mov     [rdx + rcx], al
    movss   xmm2, [f_1]
    subss   xmm2, xmm1
    mulss   xmm2, [f_153]
    cvtss2si eax, xmm2
    mov     [r8 + rcx], al
    mulss   xmm0, [f_wel]
    mulss   xmm1, [f_wef]
    inc     ecx
    cmp     ecx, 128
    jb      .l
    ret

; wob: 2 ячейки на тайл — высота заслона для ветра, px: наибольшая в карте высот
; под ячейкой × проницаемость вида тайла (ель пропускает половину, берёза — почти всё,
; забор из досок встречного ветра держит, как стена в полторы своих высоты)
wind_obst:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    xor     r13d, r13d                  ; cy
.y:
    xor     r12d, r12d                  ; cx
.x:
    mov     eax, r13d
    shr     eax, 1
    shl     eax, 7
    mov     ecx, r12d
    shr     ecx, 1
    add     eax, ecx                    ; тайл
    lea     rdx, [tmap]
    movzx   ebx, byte [rdx + rax]
    lea     rdx, [wob_k]
    movzx   ebx, byte [rdx + rbx]       ; ×128
    lea     rdx, [rmap]
    cmp     byte [rdx + rax], 0
    je      .k
    mov     ebx, 128                    ; изба: стены и крыша целиком
.k:
    lea     eax, [r13d*4 + HPAD]
    imul    eax, eax, HW8
    lea     eax, [eax + r12d*4 + HPAD]
    lea     rsi, [hmap8]
    add     rsi, rax
    xor     edi, edi
    xor     r8d, r8d
.r:
    xor     r9d, r9d
.c:
    movzx   eax, byte [rsi + r9]
    and     eax, 0x7F
    cmp     edi, eax
    cmovb   edi, eax
    inc     r9d
    cmp     r9d, 4
    jb      .c
    add     rsi, HW8
    inc     r8d
    cmp     r8d, 4
    jb      .r
    imul    edi, ebx
    shr     edi, 7
    mov     eax, 127
    cmp     edi, eax
    cmova   edi, eax
    mov     eax, r13d
    shl     eax, 8
    add     eax, r12d
    lea     rdx, [wob]
    mov     [rdx + rax], dil
    inc     r12d
    cmp     r12d, WSW
    jb      .x
    inc     r13d
    cmp     r13d, WSW
    jb      .y
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; развёртка затишья вдоль (xmm0, xmm1) — единичный вектор: S(p) = max(h(p), S(p − шаг) − спад),
; выше по ветру за краем карты открыто. xmm2 = спад, px на тайл; rdi = таблица S -> доля
; затишья (128 байт); r8d = 0 — писать в wsh, 1 — брать больший из двух
wind_sweep:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    mov     r15, rdi
    mov     ebp, r8d
    ; обе дорожки — нули (поля по 2 с каждой стороны — всегда нули)
    lea     rdi, [ws_a]
    xor     eax, eax
    mov     ecx, 2*(WSW + 4)
    rep     stosd
    movaps  xmm3, xmm0
    andps   xmm3, [f_absmask]
    movaps  xmm4, xmm1
    andps   xmm4, [f_absmask]
    comiss  xmm3, xmm4
    jb      .ym
    mov     r12d, 1                     ; шаг по главной оси, байты
    mov     r13d, WSW                   ; по поперечной
    movaps  xmm5, xmm0
    movaps  xmm6, xmm1
    movaps  xmm7, xmm3
    jmp     .go
.ym:
    mov     r12d, WSW
    mov     r13d, 1
    movaps  xmm5, xmm1
    movaps  xmm6, xmm0
    movaps  xmm7, xmm4
.go:
    maxss   xmm7, [f_0_05]
    divss   xmm6, xmm7                  ; f — сдвиг поперёк за шаг, |f| ≤ 1
    mulss   xmm2, [f_0_5]
    divss   xmm2, xmm7                  ; спад за шаг (ячейка — полтайла)
    ; точка выше по ветру в прошлой дорожке: r + q, q = −f; соседи n0, n0+1
    xorps   xmm0, xmm0
    subss   xmm0, xmm6
    movaps  xmm1, xmm0
    addss   xmm1, [f_2]
    cvttss2si eax, xmm1
    sub     eax, 2                      ; n0 = floor(q)
    cvtsi2ss xmm1, eax
    subss   xmm0, xmm1                  ; вес n0+1
    movss   xmm8, [f_1]
    subss   xmm8, xmm0                  ; вес n0
    movss   xmm9, xmm0
    movsxd  r14, eax
    xor     ebx, ebx                    ; главный индекс
    mov     esi, 1
    comiss  xmm5, [f_0]
    jae     .dir
    mov     ebx, WSW - 1
    mov     esi, -1
.dir:
    lea     r10, [ws_a + 8]             ; прошлая дорожка (индекс 0)
    lea     r11, [ws_b + 8]             ; текущая
    mov     r9d, WSW
.col:
    mov     eax, ebx
    imul    eax, r12d
    lea     rdi, [wob]
    add     rdi, rax
    lea     rdx, [wsh]
    add     rdx, rax
    xor     r8d, r8d
.row:
    lea     rax, [r8 + r14]
    movss   xmm0, [r10 + rax*4]
    mulss   xmm0, xmm8
    movss   xmm1, [r10 + rax*4 + 4]
    mulss   xmm1, xmm9
    addss   xmm0, xmm1
    subss   xmm0, xmm2
    movzx   eax, byte [rdi]
    cvtsi2ss xmm1, eax
    maxss   xmm0, xmm1
    movss   [r11 + r8*4], xmm0
    cvttss2si eax, xmm0
    movzx   eax, byte [r15 + rax]
    test    ebp, ebp
    jz      .put
    cmp     al, [rdx]
    jbe     .skip
.put:
    mov     [rdx], al
.skip:
    add     rdi, r13
    add     rdx, r13
    inc     r8d
    cmp     r8d, WSW
    jb      .row
    xchg    r10, r11
    add     ebx, esi
    dec     r9d
    jnz     .col
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; затишье за заслонителями (спад 4.7 px на тайл) и подпор перед ними (16 px на тайл)
wind_shelter:
    sub     rsp, 40
    movss   xmm0, [wind_dx]
    movss   xmm1, [wind_dy]
    movss   xmm2, [f_wslee]
    lea     rdi, [wsl_lee]
    xor     r8d, r8d
    call    wind_sweep
    movss   xmm0, [wind_dx]
    xorps   xmm0, [f_signmask]
    movss   xmm1, [wind_dy]
    xorps   xmm1, [f_signmask]
    movss   xmm2, [f_wsfront]
    lea     rdi, [wsl_front]
    mov     r8d, 1
    call    wind_sweep
    add     rsp, 40
    ret

; ---------------------------------------------------------------- кадр
update_wind:
    push    rbx
    sub     rsp, 32
    movss   xmm0, [wind_t]
    addss   xmm0, [dt]
    movss   [wind_t], xmm0
    ; ----- погода: час ночи -> средняя сила и снегопад (сумма «горбов» событий)
    movss   xmm0, [gtime]
    mulss   xmm0, [f_inv60]
    movss   [wv_h], xmm0
    movss   xmm4, [f_wb0]
    movss   xmm5, [f_sb0]
    lea     rbx, [wv_ev]
    mov     ecx, 3
.ev:
    movss   xmm0, [wv_h]
    subss   xmm0, [rbx]
    divss   xmm0, [rbx + 4]
    mulss   xmm0, xmm0
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    maxss   xmm1, [f_0]
    mulss   xmm1, xmm1                  ; (1 − x²)²
    movss   xmm0, xmm1
    mulss   xmm0, [rbx + 8]
    addss   xmm4, xmm0
    mulss   xmm1, [rbx + 12]
    addss   xmm5, xmm1
    add     rbx, 16
    dec     ecx
    jnz     .ev
    minss   xmm4, [f_1_2]
    minss   xmm5, [f_1]
    movss   [wnd_base], xmm4
    movss   [wnd_snow], xmm5
    ; ----- направление (куда дует): до метели −0.3 рад (с ЮЮВ), после +0.4 (с ЮЮЗ),
    ; и медленно гуляет ±7°
    movss   xmm0, [wv_h]
    subss   xmm0, [wv_ev + 16]
    movss   xmm1, [wv_ev + 20]
    mulss   xmm1, [f_0_6]
    addss   xmm0, xmm1
    addss   xmm1, xmm1
    divss   xmm0, xmm1
    call    sstep
    mulss   xmm0, [f_0_7]
    addss   xmm0, [f_m0_3]
    movss   [wv_th], xmm0
    movss   xmm0, [wind_t]
    mulss   xmm0, [fw_dirw]
    call    fsin
    mulss   xmm0, [f_0_12]
    addss   xmm0, [wv_th]
    movss   [wv_th], xmm0
    call    fsin
    movss   [wind_dy], xmm0
    movss   xmm0, [wv_th]
    addss   xmm0, [f_pi_2]
    call    fsin
    movss   [wind_dx], xmm0
    ; ----- сила: средняя · медленные волны порывистости (три синуса, ±35%)
    movss   xmm0, [wind_t]
    mulss   xmm0, [fw_g1w]
    call    fsin
    mulss   xmm0, [fw_g1a]
    movss   [tmp_b], xmm0
    movss   xmm0, [wind_t]
    mulss   xmm0, [fw_g2w]
    addss   xmm0, [fw_g2p]
    call    fsin
    mulss   xmm0, [fw_g2a]
    addss   xmm0, [tmp_b]
    movss   [tmp_b], xmm0
    movss   xmm0, [wind_t]
    mulss   xmm0, [fw_g3w]
    addss   xmm0, [fw_g3p]
    call    fsin
    mulss   xmm0, [fw_g3a]
    addss   xmm0, [tmp_b]
    mulss   xmm0, [f_0_6]
    addss   xmm0, [f_1]
    mulss   xmm0, [wnd_base]
    maxss   xmm0, [f_0_03]
    movss   [wind_g], xmm0
    ; размах порывов по месту: 0.2 + 0.3·сила
    movss   xmm1, [wnd_base]
    mulss   xmm1, [f_0_3]
    addss   xmm1, [f_0_2]
    movss   [wnd_gust], xmm1
    ; экранные составляющие
    movss   xmm1, [wind_dx]
    subss   xmm1, [wind_dy]
    mulss   xmm1, [f_0_707]
    movss   [wind_sx], xmm1
    movss   xmm1, [wind_dx]
    addss   xmm1, [wind_dy]
    mulss   xmm1, [f_0_707]
    movss   [wind_sy], xmm1
    ; облака плывут по ветру
    mulss   xmm0, [f_0_8]
    addss   xmm0, [f_0_6]
    mulss   xmm0, [dt]
    movss   xmm1, [wind_dx]
    mulss   xmm1, xmm0
    addss   xmm1, [cloud_ox]
    movss   [cloud_ox], xmm1
    movss   xmm1, [wind_dy]
    mulss   xmm1, xmm0
    addss   xmm1, [cloud_oy]
    movss   [cloud_oy], xmm1
    ; ----- пятна порывов бегут по ветру: 2 + 6·сила тайлов/с, мелкие — на треть быстрее
    movss   xmm3, [wnd_base]
    mulss   xmm3, [f_6]
    addss   xmm3, [f_2]
    mulss   xmm3, [dt]
    movss   xmm2, [f_gp1]
    movss   xmm0, [wind_dx]
    mulss   xmm0, xmm3
    movss   xmm1, [gst_ox]
    call    wrap_add
    movss   [gst_ox], xmm1
    movss   xmm0, [wind_dy]
    mulss   xmm0, xmm3
    movss   xmm1, [gst_oy]
    call    wrap_add
    movss   [gst_oy], xmm1
    mulss   xmm3, [f_1_3]
    movss   xmm2, [f_gp2]
    movss   xmm0, [wind_dx]
    mulss   xmm0, xmm3
    movss   xmm1, [gs2_ox]
    call    wrap_add
    movss   [gs2_ox], xmm1
    movss   xmm0, [wind_dy]
    mulss   xmm0, xmm3
    movss   xmm1, [gs2_oy]
    call    wrap_add
    movss   [gs2_oy], xmm1
    ; ----- позёмка: полосы бегут вдоль ветра — слой 1 (тексель 1/16 тайла) на половине
    ; скорости ветра, слой 2 (1/24) на 0.35
    movss   xmm3, [wind_g]
    mulss   xmm3, [f_wspd]
    mulss   xmm3, [dt]                  ; путь ветра за кадр, тайлы
    movss   xmm2, [f_256]
    movss   xmm0, xmm3
    mulss   xmm0, [f_8]
    movss   xmm1, [pz_f1]
    call    wrap_add
    movss   [pz_f1], xmm1
    cvttss2si eax, xmm1
    mov     [pz_s1], eax
    movss   xmm0, xmm3
    mulss   xmm0, [f_8_4]
    movss   xmm1, [pz_f2]
    call    wrap_add
    movss   [pz_f2], xmm1
    cvttss2si eax, xmm1
    mov     [pz_s2], eax
    movss   xmm0, [wind_dx]
    mulss   xmm0, [f_256]
    cvtss2si eax, xmm0
    mov     [pz_dx], eax
    movss   xmm0, [wind_dy]
    mulss   xmm0, [f_256]
    cvtss2si eax, xmm0
    mov     [pz_dy], eax
    ; ----- мгла: снег гуще 0.35 при ветре сильнее 0.3 — видно ближе, фонарик бьёт короче
    movss   xmm0, [wnd_snow]
    subss   xmm0, [f_0_35]
    mulss   xmm0, [f_1_6]
    CLAMP01 xmm0
    movss   xmm1, [wind_g]
    subss   xmm1, [f_0_3]
    mulss   xmm1, [f_1_8]
    CLAMP01 xmm1
    mulss   xmm0, xmm1
    movss   [wnd_wo], xmm0
    movss   xmm1, xmm0
    mulss   xmm1, [f_8]
    movss   xmm2, [f_21]
    subss   xmm2, xmm1
    movss   [vis_r], xmm2
    mulss   xmm2, xmm2
    movss   [vis_r2], xmm2
    movss   xmm1, xmm0
    mulss   xmm1, [f_0_18]
    movss   xmm2, [f_0_33]
    subss   xmm2, xmm1
    movss   [vis_k], xmm2
    movss   xmm1, xmm0
    mulss   xmm1, [f_6]
    movss   xmm2, [f_17]
    subss   xmm2, xmm1
    movss   [fl_reach], xmm2
    ; «память» за обзором в мгле светлее и с белёсой пеленой
    movss   xmm1, xmm0
    mulss   xmm1, [f_0_4]
    addss   xmm1, [f_1]
    shufps  xmm1, xmm1, 0
    mulps   xmm1, [memtint_f]
    cvtps2dq xmm1, xmm1
    packssdw xmm1, xmm1
    movq    [memtint_v], xmm1
    shufps  xmm0, xmm0, 0
    mulps   xmm0, [haze_f]
    cvtps2dq xmm0, xmm0
    packssdw xmm0, xmm0
    movq    [haze_v], xmm0
    ; снежинки: 30 + 40·снег — в густом снегу воздух светлее
    movss   xmm0, [wnd_snow]
    mulss   xmm0, [f_40]
    addss   xmm0, [f_30]
    cvttss2si eax, xmm0
    mov     [flk_base], eax
    ; конусы фонарей видны по снегу в воздухе: ×(1 + 1.4·(снег − 0.15)), до 2.2
    movss   xmm0, [wnd_snow]
    subss   xmm0, [f_0_15]
    mulss   xmm0, [f_1_4]
    addss   xmm0, [f_1]
    minss   xmm0, [f_2_2]
    movss   [cone_k], xmm0
    ; ----- затишье пересчитать, если ветер повернул больше чем на градус
    movss   xmm0, [wv_th]
    subss   xmm0, [wsh_th]
    andps   xmm0, [f_absmask]
    comiss  xmm0, [f_0_018]
    jb      .nosh
    movss   xmm0, [wv_th]
    movss   [wsh_th], xmm0
    call    wind_shelter
.nosh:
    add     rsp, 32
    pop     rbx
    ret

; xmm1 += xmm0 по модулю xmm2 (в [0, xmm2))
wrap_add:
    addss   xmm1, xmm0
    comiss  xmm1, xmm2
    jb      .lo
    subss   xmm1, xmm2
.lo:
    comiss  xmm1, [f_0]
    jae     .ok
    addss   xmm1, xmm2
.ok:
    ret

; ---------------------------------------------------------------- ветер в точке
; xmm0 = x, xmm1 = y (тайлы), xmm2 = высота над снегом, px ->
;   xmm0 = vx, xmm1 = vy (сила·направление), xmm2 = сила, xmm3 = завихрения
; затишье у земли целиком, выше 90 px его нет. Портит eax, ecx, edx, r8-r11, xmm4, xmm5.
; Своё держит на стеке — зовут и с потоков рендера (фоновая маска позёмки рядом с
; деревьями на главном); wv_sh, wv_gf — только выход, для тех, кто зовёт в одиночку
WV_X  equ 0
WV_Y  equ 4
WV_HF equ 8
WV_SH equ 12
WV_G1 equ 16
wind_vec:
    sub     rsp, 24
    movss   [rsp + WV_X], xmm0
    movss   [rsp + WV_Y], xmm1
    movss   xmm4, [f_90]
    subss   xmm4, xmm2
    mulss   xmm4, [f_inv70]
    CLAMP01 xmm4
    movss   [rsp + WV_HF], xmm4
    ; затишье: 2 ячейки на тайл, центры на +0.5, билинейно
    mulss   xmm0, [f_512]
    cvttss2si eax, xmm0
    sub     eax, 128
    mulss   xmm1, [f_512]
    cvttss2si ecx, xmm1
    sub     ecx, 128
    xor     edx, edx
    cmp     eax, edx
    cmovl   eax, edx
    cmp     ecx, edx
    cmovl   ecx, edx
    mov     edx, (WSW-2)*256 + 255
    cmp     eax, edx
    cmovg   eax, edx
    cmp     ecx, edx
    cmovg   ecx, edx
    mov     r8d, eax
    and     r8d, 255
    mov     r9d, ecx
    and     r9d, 255
    shr     eax, 8
    shr     ecx, 8
    shl     ecx, 8
    add     eax, ecx
    lea     rdx, [wsh]
    movzx   ecx, byte [rdx + rax]
    movzx   r10d, byte [rdx + rax + 1]
    sub     r10d, ecx
    imul    r10d, r8d
    shl     ecx, 8
    add     ecx, r10d
    movzx   r10d, byte [rdx + rax + WSW]
    movzx   r11d, byte [rdx + rax + WSW + 1]
    sub     r11d, r10d
    imul    r11d, r8d
    shl     r10d, 8
    add     r10d, r11d
    sub     r10d, ecx
    imul    r10d, r9d
    sar     r10d, 8
    add     ecx, r10d
    cvtsi2ss xmm4, ecx
    mulss   xmm4, [f_inv65280]
    mulss   xmm4, [rsp + WV_HF]
    movss   [rsp + WV_SH], xmm4
.gust:
    ; порывы: крупные пятна (тексель 1.6 тайла) и мелкие (0.55), бегут по ветру
    movss   xmm0, [rsp + WV_X]
    subss   xmm0, [gst_ox]
    addss   xmm0, [f_gp1]
    mulss   xmm0, [f_gk1]
    cvttss2si eax, xmm0
    movss   xmm0, [rsp + WV_Y]
    subss   xmm0, [gst_oy]
    addss   xmm0, [f_gp1]
    mulss   xmm0, [f_gk1]
    cvttss2si ecx, xmm0
    call    nz64
    mov     [rsp + WV_G1], eax
    movss   xmm0, [rsp + WV_X]
    subss   xmm0, [gs2_ox]
    addss   xmm0, [f_gp2]
    mulss   xmm0, [f_gk2]
    cvttss2si eax, xmm0
    movss   xmm0, [rsp + WV_Y]
    subss   xmm0, [gs2_oy]
    addss   xmm0, [f_gp2]
    mulss   xmm0, [f_gk2]
    cvttss2si ecx, xmm0
    call    nz64
    cvtsi2ss xmm5, eax
    mulss   xmm5, [f_inv65280]          ; G2
    cvtsi2ss xmm0, dword [rsp + WV_G1]
    mulss   xmm0, [f_inv65280]          ; G1
    mulss   xmm0, [f_0_65]
    movss   xmm1, xmm5
    mulss   xmm1, [f_0_35]
    addss   xmm0, xmm1
    mulss   xmm0, [f_1_6]
    subss   xmm0, [f_0_3]
    call    sstep
    addss   xmm0, xmm0
    subss   xmm0, [f_1]
    mulss   xmm0, [wnd_gust]
    addss   xmm0, [f_1]                 ; порыв: 1 ± размах
    movss   [wv_gf], xmm0
    ; сила = общая · порыв · (1 − 0.9·затишье)
    movss   xmm2, [rsp + WV_SH]
    movss   [wv_sh], xmm2
    mulss   xmm2, [f_0_9]
    movss   xmm1, [f_1]
    subss   xmm1, xmm2
    mulss   xmm0, xmm1
    mulss   xmm0, [wind_g]
    movss   xmm2, xmm0
    ; завихрения: сильнее всего на краю затишья
    movss   xmm3, [rsp + WV_SH]
    movss   xmm1, [f_1]
    subss   xmm1, xmm3
    mulss   xmm3, xmm1
    mulss   xmm3, [f_1_6]
    addss   xmm3, [f_0_12]
    mulss   xmm3, [wind_g]
    ; направление качается с мелкими порывами: ±0.25 рад
    subss   xmm5, [f_0_5]
    mulss   xmm5, [f_0_5]
    movss   xmm0, [wind_dy]
    mulss   xmm0, xmm5
    movss   xmm4, [wind_dx]
    subss   xmm4, xmm0                  ; dx − δ·dy
    movss   xmm1, [wind_dx]
    mulss   xmm1, xmm5
    addss   xmm1, [wind_dy]             ; dy + δ·dx
    movaps  xmm0, xmm4
    mulss   xmm0, xmm2
    mulss   xmm1, xmm2
    add     rsp, 24
    ret

; то же над крышами и кронами — без затишья (xmm2 не нужен)
wind_top:
    sub     rsp, 24
    movss   [rsp + WV_X], xmm0
    movss   [rsp + WV_Y], xmm1
    mov     dword [rsp + WV_SH], 0
    jmp     wind_vec.gust

; xmm0 = x, xmm1 = y (тайлы) -> xmm0 = сила ветра у земли (порывы, затишье)
wind_at:
    xorps   xmm2, xmm2
    call    wind_vec
    movaps  xmm0, xmm2
    ret

; eax = u, ecx = v (тексели·256, ≥ 0) -> eax = шум порывов 0..65280, билинейно с заворотом
; портит ecx, edx, r8-r11
nz64:
    mov     r8d, eax
    and     r8d, 255
    mov     r9d, ecx
    and     r9d, 255
    shr     eax, 8
    shr     ecx, 8
    mov     r10d, eax
    inc     r10d
    and     r10d, 63
    and     eax, 63
    mov     r11d, ecx
    inc     r11d
    and     r11d, 63
    shl     r11d, 6
    and     ecx, 63
    shl     ecx, 6
    lea     rdx, [gustn]
    add     rcx, rdx
    add     r11, rdx
    movzx   edx, byte [rcx + rax]
    movzx   ecx, byte [rcx + r10]
    sub     ecx, edx
    imul    ecx, r8d
    shl     edx, 8
    add     edx, ecx                    ; строка v0, ·256
    movzx   ecx, byte [r11 + rax]
    movzx   eax, byte [r11 + r10]
    sub     eax, ecx
    imul    eax, r8d
    shl     ecx, 8
    add     ecx, eax                    ; строка v1
    sub     ecx, edx
    imul    ecx, r9d
    sar     ecx, 8
    lea     eax, [edx + ecx]
    ret

; свой ГСЧ ветра и красоты — основной (карта, зомби) не сбивается. eax (портит ecx)
wrand:
    mov     eax, [wrng]
    mov     ecx, eax
    shl     ecx, 13
    xor     eax, ecx
    mov     ecx, eax
    shr     ecx, 17
    xor     eax, ecx
    mov     ecx, eax
    shl     ecx, 5
    xor     eax, ecx
    mov     [wrng], eax
    ret

; xmm0 = [0,1) (портит eax, ecx)
wrandf:
    call    wrand
    shr     eax, 8
    cvtsi2ss xmm0, eax
    mulss   xmm0, [f_inv24]
    ret

; ---------------------------------------------------------------- позёмка: маска на кадр
; сетка света (4 на тайл): сила ветра у земли -> 255·clamp((s − 0.42)·2.2)², на полу
; изб — нет. Считается через сэмпл (2 на тайл), промежуточные — средние соседей
build_wgrid:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32
    lea     rbx, [wgrid]
    ; тихо: даже порыв на юру не дотянет до порога — не метёт нигде
    movss   xmm0, [wnd_gust]
    addss   xmm0, [f_1]
    mulss   xmm0, [wind_g]
    comiss  xmm0, [f_0_45]
    ja      .calc
    mov     rdi, rbx
    xor     eax, eax
    mov     ecx, (GW*GW + 7) / 8
    rep     stosq
    jmp     .out
.calc:
    xor     r13d, r13d
.y:
    xor     r12d, r12d
.x:
    mov     eax, [gox]
    add     eax, r12d
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    mulss   xmm0, [f_0_25]
    mov     eax, [goy]
    add     eax, r13d
    cvtsi2ss xmm1, eax
    addss   xmm1, [f_0_5]
    mulss   xmm1, [f_0_25]
    xorps   xmm2, xmm2
    call    wind_vec
    subss   xmm2, [f_0_45]
    mulss   xmm2, [f_1_6]
    CLAMP01 xmm2
    mulss   xmm2, xmm2
    mulss   xmm2, [f_255]
    cvttss2si esi, xmm2
    ; пол избы — не метёт; за картой — тоже
    mov     eax, [gox]
    add     eax, r12d
    sar     eax, 2
    mov     ecx, [goy]
    add     ecx, r13d
    sar     ecx, 2
    cmp     eax, MAPW
    jae     .z
    cmp     ecx, MAPW
    jae     .z
    shl     ecx, 7
    add     eax, ecx
    lea     rdx, [tmap]
    movzx   eax, byte [rdx + rax]
    lea     rdx, [tprop]
    movzx   eax, byte [rdx + rax]
    and     eax, 0x30
    cmp     eax, 0x20
    jne     .put
.z:
    xor     esi, esi
.put:
    imul    eax, r13d, GW
    add     eax, r12d
    mov     [rbx + rax], sil
    add     r12d, 2
    cmp     r12d, GW
    jb      .x
    add     r13d, 2
    cmp     r13d, GW
    jb      .y
    ; нечётные столбцы чётных строк
    xor     r13d, r13d
.hy:
    imul    edi, r13d, GW
    add     rdi, rbx
    mov     r12d, 1
.hx:
    movzx   eax, byte [rdi + r12 - 1]
    lea     ecx, [r12d + 1]
    cmp     ecx, GW
    jae     .h1
    movzx   ecx, byte [rdi + r12 + 1]
    add     eax, ecx
    inc     eax
    shr     eax, 1
.h1:
    mov     [rdi + r12], al
    add     r12d, 2
    cmp     r12d, GW
    jb      .hx
    add     r13d, 2
    cmp     r13d, GW
    jb      .hy
    ; нечётные строки — средние соседних строк
    mov     r13d, 1
.vy:
    imul    edi, r13d, GW
    add     rdi, rbx
    lea     rsi, [rdi + GW]             ; строка ниже
    lea     eax, [r13d + 1]
    cmp     eax, GW
    jb      .v0
    lea     rsi, [rdi - GW]             ; последняя — копия предыдущей
.v0:
    xor     r12d, r12d
.vx:
    movzx   eax, byte [rdi + r12 - GW]
    movzx   ecx, byte [rsi + r12]
    add     eax, ecx
    inc     eax
    shr     eax, 1
    mov     [rdi + r12], al
    inc     r12d
    cmp     r12d, GW
    jb      .vx
    add     r13d, 2
    cmp     r13d, GW
    jb      .vy
.out:
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- текстуры
; шум порывов 64x64 — своим ГСЧ
gen_gust:
    push    r12
    push    r13
    sub     rsp, 40
    lea     r12, [gustn]
    lea     r13, [wrandf]
    call    gen_noise64
    add     rsp, 40
    pop     r13
    pop     r12
    ret

; полосы позёмки 256 (вдоль ветра, 1/16 тайла) x 64 (поперёк, 1/32): «змейки» —
; вытянутые, виляющие, к концам тают; с заворотом по обеим осям
gen_streaks:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    lea     rdi, [strk_acc]
    xor     eax, eax
    mov     ecx, 256*64
    rep     stosw
    mov     r15d, 70 + 10               ; змеек, потом — широкие бледные клубы
.s:
    call    wrandf
    mulss   xmm0, [f_256]
    movss   [gs_u0], xmm0
    call    wrandf
    mulss   xmm0, [f_64]
    movss   [gs_v0], xmm0
    cmp     r15d, 10
    jbe     .puff
    call    wrandf
    mulss   xmm0, [f_36]
    addss   xmm0, [f_12]
    cvttss2si r14d, xmm0                ; длина 12..48 текселей (0.75..3 тайла)
    call    wrandf
    mulss   xmm0, [f_1_4]
    addss   xmm0, [f_0_8]
    movss   [gs_w], xmm0                ; полуширина 0.8..2.2
    call    wrandf
    mulss   xmm0, [f_110]
    addss   xmm0, [f_120]
    movss   [gs_a], xmm0                ; яркость
    call    wrandf
    mulss   xmm0, [f_4]
    addss   xmm0, [f_1]
    movss   [gs_m], xmm0                ; размах виляния 1..5
    call    wrandf
    mulss   xmm0, [f_0_22s]
    addss   xmm0, [f_0_08]
    movss   [gs_k], xmm0                ; частота виляния, рад на тексель
    jmp     .ph
.puff:
    call    wrandf
    mulss   xmm0, [f_40]
    addss   xmm0, [f_24]
    cvttss2si r14d, xmm0
    call    wrandf
    mulss   xmm0, [f_5]
    addss   xmm0, [f_5]
    movss   [gs_w], xmm0
    call    wrandf
    mulss   xmm0, [f_40]
    addss   xmm0, [f_30]
    movss   [gs_a], xmm0
    call    wrandf
    mulss   xmm0, [f_3]
    movss   [gs_m], xmm0
    mov     dword [gs_k], __float32__(0.06)
.ph:
    call    wrandf
    mulss   xmm0, [f_6_28]
    movss   [gs_ph], xmm0
    cvtsi2ss xmm0, r14d
    movss   xmm1, [f_1]
    divss   xmm1, xmm0
    movss   [gs_il], xmm1
    xor     r13d, r13d                  ; k
.k:
    ; к концам тает: 4t(1 − t)
    cvtsi2ss xmm0, r13d
    mulss   xmm0, [gs_il]
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    mulss   xmm1, xmm0
    mulss   xmm1, [f_4]
    mulss   xmm1, [gs_a]
    movss   [gs_i], xmm1
    ; середина поперёк
    cvtsi2ss xmm0, r13d
    mulss   xmm0, [gs_k]
    addss   xmm0, [gs_ph]
    call    fsin
    mulss   xmm0, [gs_m]
    addss   xmm0, [gs_v0]
    addss   xmm0, [f_64]                ; положительно
    movss   [gs_vc], xmm0
    cvttss2si ebx, xmm0
    cvttss2si eax, [gs_u0]
    add     eax, r13d
    and     eax, 255
    mov     esi, eax                    ; u
    cvttss2si ebp, [gs_w]
    inc     ebp
    mov     r12d, ebp
    neg     r12d                        ; поперёк от −(w + 1) до w + 1
    inc     ebp
.dv:
    lea     eax, [ebx + r12d]
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    subss   xmm0, [gs_vc]
    andps   xmm0, [f_absmask]
    divss   xmm0, [gs_w]
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    comiss  xmm1, [f_0]
    jbe     .dn
    mulss   xmm1, [gs_i]
    cvttss2si ecx, xmm1
    and     eax, 63
    shl     eax, 8
    add     eax, esi
    lea     rdx, [strk_acc]
    movzx   edi, word [rdx + rax*2]
    add     edi, ecx
    mov     [rdx + rax*2], di
.dn:
    inc     r12d
    cmp     r12d, ebp
    jl      .dv
    inc     r13d
    cmp     r13d, r14d
    jb      .k
    dec     r15d
    jnz     .s
    ; в байты
    lea     rsi, [strk_acc]
    lea     rdi, [strk]
    xor     ecx, ecx
.b:
    movzx   eax, word [rsi + rcx*2]
    mov     edx, 255
    cmp     eax, edx
    cmova   eax, edx
    mov     [rdi + rcx], al
    inc     ecx
    cmp     ecx, 256*64
    jb      .b
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

; eax = x, ecx = y (px), edx = сдвиг (размер ячейки 2^сдвиг) -> eax = шум значений 0..255
; (портит ecx, edx, r8-r11)
vnoise:
    push    rbx
    mov     r8d, eax
    mov     r9d, ecx
    mov     ecx, edx
    mov     r10d, 1
    shl     r10d, cl
    dec     r10d                        ; маска
    mov     r11d, r8d
    and     r11d, r10d
    shl     r11d, 8
    shr     r11d, cl                    ; fx 0..255
    mov     ebx, r9d
    and     ebx, r10d
    shl     ebx, 8
    shr     ebx, cl                     ; fy
    shr     r8d, cl
    shr     r9d, cl
    ; углы
    mov     eax, r8d
    mov     edx, r9d
    call    .h
    mov     r10d, eax                   ; h00
    lea     eax, [r8d + 1]
    mov     edx, r9d
    call    .h
    sub     eax, r10d
    imul    eax, r11d
    shl     r10d, 8
    add     r10d, eax                   ; верх ·256
    push    r10
    mov     eax, r8d
    lea     edx, [r9d + 1]
    call    .h
    mov     r10d, eax
    lea     eax, [r8d + 1]
    lea     edx, [r9d + 1]
    call    .h
    sub     eax, r10d
    imul    eax, r11d
    shl     r10d, 8
    add     r10d, eax                   ; низ
    pop     rax
    sub     r10d, eax
    imul    r10d, ebx
    sar     r10d, 8
    add     eax, r10d
    shr     eax, 8
    pop     rbx
    ret
.h:
    HASH    eax, edx
    shr     eax, 24
    ret

; иней по краям кадра — «когда проступает» (255 — сразу, 0 — никогда): налёт — полоса у
; краёв, к углам шире, с мелким шумом; иглы-дендриты растут от краёв внутрь (к концу
; значение падает — чем холоднее, тем дальше дорастают), с веточками вбок
gen_frost:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    lea     rbx, [frost]
    xor     r13d, r13d                  ; y
.y:
    xor     r12d, r12d                  ; x
.x:
    ; до краёв (по вертикали ×1.6): e = min + max/2
    mov     eax, r12d
    mov     ecx, SCR_W - 1
    sub     ecx, r12d
    cmp     eax, ecx
    cmova   eax, ecx
    mov     ecx, r13d
    mov     edx, SCR_H - 1
    sub     edx, r13d
    cmp     ecx, edx
    cmova   ecx, edx
    imul    ecx, ecx, 13
    shr     ecx, 3
    mov     edx, eax
    cmp     edx, ecx
    cmova   edx, ecx                    ; min
    cmp     eax, ecx
    cmovb   eax, ecx                    ; max
    shr     eax, 1
    add     edx, eax
    imul    edx, edx, 27
    shr     edx, 3                      ; ·3.4
    mov     r14d, 235
    sub     r14d, edx
    mov     eax, r12d
    mov     ecx, r13d
    mov     edx, 3
    call    vnoise
    sub     eax, 128
    imul    eax, eax, 70
    sar     eax, 8
    add     r14d, eax
    xor     eax, eax
    cmp     r14d, eax
    cmovl   r14d, eax
    mov     eax, 255
    cmp     r14d, eax
    cmovg   r14d, eax
    imul    eax, r13d, SCR_W
    add     eax, r12d
    mov     [rbx + rax], r14b
    inc     r12d
    cmp     r12d, SCR_W
    jb      .x
    inc     r13d
    cmp     r13d, SCR_H
    jb      .y
    ; ----- иглы
    mov     r15d, 340
.nd:
    ; край: 0 верх, 1 низ, 2 лево, 3 право; нормаль внутрь
    call    wrandf
    mulss   xmm0, [f_4]
    cvttss2si esi, xmm0
    call    wrandf
    movss   xmm4, xmm0                  ; где на краю
    xorps   xmm2, xmm2                  ; nx
    xorps   xmm3, xmm3                  ; ny
    cmp     esi, 1
    jb      .top
    je      .bot
    cmp     esi, 2
    je      .lft
    mov     dword [fr_x], __float32__(639.0)
    mulss   xmm4, [f_360]
    movss   [fr_y], xmm4
    movss   xmm2, [f_m1]
    jmp     .dir
.top:
    mulss   xmm4, [f_640]
    movss   [fr_x], xmm4
    mov     dword [fr_y], 0
    movss   xmm3, [f_1]
    jmp     .dir
.bot:
    mulss   xmm4, [f_640]
    movss   [fr_x], xmm4
    mov     dword [fr_y], __float32__(359.0)
    movss   xmm3, [f_m1]
    jmp     .dir
.lft:
    mov     dword [fr_x], 0
    mulss   xmm4, [f_360]
    movss   [fr_y], xmm4
    movss   xmm2, [f_1]
.dir:
    movss   [fr_nx], xmm2
    movss   [fr_ny], xmm3
    ; повернуть на ±0.65 рад
    call    wrandf
    subss   xmm0, [f_0_5]
    mulss   xmm0, [f_1_3]
    movss   [fr_a], xmm0
    call    fsin
    movss   [fr_s], xmm0
    movss   xmm0, [fr_a]
    addss   xmm0, [f_pi_2]
    call    fsin                        ; cos
    movss   xmm2, [fr_nx]
    mulss   xmm2, xmm0
    movss   xmm1, [fr_ny]
    mulss   xmm1, [fr_s]
    subss   xmm2, xmm1
    movss   [fr_dx], xmm2               ; nx·cos − ny·sin
    movss   xmm3, [fr_ny]
    mulss   xmm3, xmm0
    movss   xmm1, [fr_nx]
    mulss   xmm1, [fr_s]
    addss   xmm3, xmm1
    movss   [fr_dy], xmm3               ; nx·sin + ny·cos
    ; длина 14..60, начало 238..254
    call    wrandf
    mulss   xmm0, [f_46]
    addss   xmm0, [f_14]
    cvttss2si r13d, xmm0
    call    wrandf
    mulss   xmm0, [f_16]
    addss   xmm0, [f_238]
    movss   [fr_v0], xmm0
    xor     r14d, r14d                  ; шаг
.st:
    movss   xmm0, [fr_x]
    addss   xmm0, [fr_dx]
    movss   [fr_x], xmm0
    movss   xmm1, [fr_y]
    addss   xmm1, [fr_dy]
    movss   [fr_y], xmm1
    cvtsi2ss xmm2, r14d
    mulss   xmm2, [f_3_3]
    movss   xmm3, [fr_v0]
    subss   xmm3, xmm2
    movss   [fr_v], xmm3
    call    fr_stamp
    ; веточки: каждый 5-й шаг, в обе стороны на ±0.95 рад, длиной в четверть остатка
    mov     eax, r14d
    xor     edx, edx
    mov     ecx, 5
    div     ecx
    cmp     edx, 4
    jne     .sn
    lea     eax, [r14d + 3]
    cmp     eax, r13d
    jae     .sn
    mov     ebp, 2
    movss   xmm0, [f_0_8134]
    movss   [fr_bs], xmm0
.br:
    ; направление ветки: поворот хода на ±0.95
    movss   xmm0, [fr_dx]
    mulss   xmm0, [f_0_5817]
    movss   xmm1, [fr_dy]
    mulss   xmm1, [fr_bs]
    subss   xmm0, xmm1
    movss   [fr_bx], xmm0
    movss   xmm0, [fr_dx]
    mulss   xmm0, [fr_bs]
    movss   xmm1, [fr_dy]
    mulss   xmm1, [f_0_5817]
    addss   xmm0, xmm1
    movss   [fr_by], xmm0
    mov     eax, [fr_x]
    mov     [fr_px], eax
    mov     eax, [fr_y]
    mov     [fr_py], eax
    mov     eax, r13d
    sub     eax, r14d
    shr     eax, 2
    add     eax, 3
    mov     r12d, eax                   ; длина ветки
    mov     eax, [fr_v]
    mov     [fr_bv], eax
.bs:
    movss   xmm0, [fr_px]
    addss   xmm0, [fr_bx]
    movss   [fr_px], xmm0
    movss   xmm1, [fr_py]
    addss   xmm1, [fr_by]
    movss   [fr_py], xmm1
    movss   xmm3, [fr_bv]
    subss   xmm3, [f_4_5]
    movss   [fr_bv], xmm3
    call    fr_stamp
    dec     r12d
    jnz     .bs
    movss   xmm0, [fr_bs]
    xorps   xmm0, [f_signmask]
    movss   [fr_bs], xmm0
    dec     ebp
    jnz     .br
.sn:
    inc     r14d
    cmp     r14d, r13d
    jb      .st
    dec     r15d
    jnz     .nd
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

; xmm0 = x, xmm1 = y, xmm3 = значение -> frost = max(frost, значение) (rbx = frost)
fr_stamp:
    cvtss2si eax, xmm0
    cmp     eax, SCR_W
    jae     .o
    cvtss2si ecx, xmm1
    cmp     ecx, SCR_H
    jae     .o
    cvttss2si edx, xmm3
    test    edx, edx
    jle     .o
    imul    ecx, ecx, SCR_W
    add     eax, ecx
    cmp     [rbx + rax], dl
    jae     .o
    mov     [rbx + rax], dl
.o:
    ret

; иней на «стекле» кадра — чем холоднее игроку, тем глубже от краёв (после снега и HUD-полос)
draw_frost:
    movss   xmm0, [frost_lvl]
    comiss  xmm0, [f_0_01]
    jb      .out
    mulss   xmm0, [f_215]
    movss   xmm1, [f_255]
    subss   xmm1, xmm0
    cvttss2si eax, xmm1
    mov     [frost_thr], eax            ; порог
    sub     rsp, 40
    lea     rcx, [frost_rows]
    mov     edx, SCR_H
    mov     r8d, 8
    call    par_for
    add     rsp, 40
.out:
    ret

; строки ecx .. edx−1 (с любого потока пула)
frost_rows:
    push    rsi
    imul    ecx, ecx, SCR_W
    imul    esi, edx, SCR_W
    mov     r8d, [frost_thr]
    mov     r9, [fbbits]
    lea     r10, [frost]
    movq    xmm5, [frost_col]
    pxor    xmm4, xmm4
    ; 16 байт маски, ни один не выше порога, — мимо разом (порог < 0 — без пропусков)
    mov     eax, r8d
    mov     edx, 255
    cmp     eax, edx
    cmovg   eax, edx
    movd    xmm3, eax
    punpcklbw xmm3, xmm3
    pshuflw xmm3, xmm3, 0
    punpcklqdq xmm3, xmm3
    mov     r11d, r8d
    sar     r11d, 31
.ch:
    cmp     ecx, esi
    jae     .done
    lea     edx, [rcx + 16]
    test    r11d, r11d
    jnz     .l
    movdqu  xmm0, [r10 + rcx]
    psubusb xmm0, xmm3
    pcmpeqb xmm0, xmm4
    pmovmskb eax, xmm0
    cmp     eax, 0xFFFF
    jne     .l
    mov     ecx, edx
    jmp     .ch
.l:
    movzx   eax, byte [r10 + rcx]
    sub     eax, r8d
    jle     .n
    imul    eax, eax, 3
    cmp     eax, 58                     ; не гуще 45%: сквозь иней видно
    jbe     .a
    mov     eax, 58
.a:
    movd    xmm0, [r9 + rcx*4]
    punpcklbw xmm0, xmm4
    movdqa  xmm1, xmm5
    psubw   xmm1, xmm0
    movd    xmm2, eax
    pshuflw xmm2, xmm2, 0
    pmullw  xmm1, xmm2
    psraw   xmm1, 7
    paddw   xmm0, xmm1
    packuswb xmm0, xmm0
    movd    [r9 + rcx*4], xmm0
.n:
    inc     ecx
    cmp     ecx, edx
    jb      .l
    jmp     .ch
.done:
    pop     rsi
    ret

; ---------------------------------------------------------------- отладка
; --wind: снимок — вид сверху на поле ветра вокруг игрока (360x360 сэмплов, 4 на тайл):
; сила у земли — жёлто-белым, затишье — синим, заслоны — серым; ветер дует вправо-вниз
; по картинке, если дует к +x (x — вправо, y — вниз)
dump_wind:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32
    mov     rdi, [fbbits]
    xor     eax, eax
    mov     ecx, SCR_W*SCR_H
    rep     stosd
    movss   xmm0, [pl + E_X]
    mulss   xmm0, [f_4]
    cvttss2si r12d, xmm0
    sub     r12d, 180
    movss   xmm0, [pl + E_Y]
    mulss   xmm0, [f_4]
    cvttss2si r13d, xmm0
    sub     r13d, 180
    xor     esi, esi
.y:
    xor     ebx, ebx
.x:
    lea     eax, [r12d + ebx]
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    mulss   xmm0, [f_0_25]
    lea     eax, [r13d + esi]
    cvtsi2ss xmm1, eax
    addss   xmm1, [f_0_5]
    mulss   xmm1, [f_0_25]
    xorps   xmm2, xmm2
    call    wind_vec
    ; R, G — сила, B — затишье
    mulss   xmm2, [f_160]
    minss   xmm2, [f_255]
    cvttss2si eax, xmm2
    mov     ecx, eax
    shl     eax, 16
    imul    ecx, ecx, 3
    shr     ecx, 2
    shl     ecx, 8
    or      eax, ecx
    movss   xmm0, [wv_sh]
    mulss   xmm0, [f_196]
    cvttss2si ecx, xmm0
    or      eax, ecx
    ; заслон в ячейке — серым
    lea     ecx, [r12d + ebx]
    sar     ecx, 1
    lea     edx, [r13d + esi]
    sar     edx, 1
    cmp     ecx, WSW
    jae     .put
    cmp     edx, WSW
    jae     .put
    shl     edx, 8
    add     ecx, edx
    lea     rdx, [wob]
    movzx   ecx, byte [rdx + rcx]
    test    ecx, ecx
    jz      .put
    add     ecx, 60
    imul    eax, ecx, 0x010101
.put:
    mov     rdi, [fbbits]
    imul    edx, esi, SCR_W
    add     edx, ebx
    mov     [rdi + rdx*4 + 140*4], eax
    inc     ebx
    cmp     ebx, 360
    jb      .x
    inc     esi
    cmp     esi, 360
    jb      .y
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- HUD: строка ветра
; tbuf <- «ветер Ю 7 м/с · метель»: откуда дует (8 румбов: С = +x, В = +y), сколько
; у игрока (с порывами, сглажено), погода — метель / позёмка / снег; в затишье — «тихо»
wind_text:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32
    lea     rdi, [tbuf]
    movss   xmm0, [pl_wsm]
    mulss   xmm0, [f_12]
    cvtss2si ebx, xmm0
    test    ebx, ebx
    jnz     .wind
    lea     rsi, [s_calm]
    call    wput_str
    jmp     .suf
.wind:
    lea     rsi, [s_wind]
    call    wput_str
    ; румб, ближайший к «откуда»: max (−ветер)·румб
    movss   xmm4, [wind_dx]
    xorps   xmm4, [f_signmask]
    movss   xmm5, [wind_dy]
    xorps   xmm5, [f_signmask]
    lea     rdx, [rh_tab]
    xor     ecx, ecx
    xor     eax, eax
    movss   xmm3, [f_m1e9]
.r:
    movss   xmm0, [rdx + rcx*8]
    mulss   xmm0, xmm4
    movss   xmm1, [rdx + rcx*8 + 4]
    mulss   xmm1, xmm5
    addss   xmm0, xmm1
    comiss  xmm0, xmm3
    jbe     .rn
    movss   xmm3, xmm0
    mov     eax, ecx
.rn:
    inc     ecx
    cmp     ecx, 8
    jb      .r
    lea     rdx, [rh_str]
    mov     rsi, [rdx + rax*8]
    call    wput_str
    mov     word [rdi], ' '
    add     rdi, 2
    mov     eax, ebx
    call    wput_num
    lea     rsi, [s_mps]
    call    wput_str
.suf:
    movss   xmm0, [wnd_snow]
    comiss  xmm0, [f_0_6]
    jb      .ns
    movss   xmm0, [wind_g]
    comiss  xmm0, [f_0_5]
    jb      .snow
    lea     rsi, [s_blizz]
    jmp     .put
.ns:
    movss   xmm0, [wind_g]
    comiss  xmm0, [f_0_45]
    jb      .ns2
    lea     rsi, [s_drift]
    jmp     .put
.ns2:
    movss   xmm0, [wnd_snow]
    comiss  xmm0, [f_0_45]
    jb      .done
.snow:
    lea     rsi, [s_snowing]
.put:
    call    wput_str
.done:
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- HUD: стрелка ветра
; под часами: куда дует у игрока (с порывами), длина — по силе
draw_windhud:
    sub     rsp, 40
    ; экранное направление ветра у игрока: (2(vx − vy), vx + vy)
    movss   xmm0, [pl_wvx]
    subss   xmm0, [pl_wvy]
    addss   xmm0, xmm0
    movss   xmm1, [pl_wvx]
    addss   xmm1, [pl_wvy]
    movaps  xmm2, xmm0
    mulss   xmm2, xmm0
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    addss   xmm2, xmm3
    sqrtss  xmm2, xmm2
    comiss  xmm2, [f_0_01]
    ja      .ok
    ; штиль — стрелка по общему направлению
    movss   xmm0, [wind_dx]
    subss   xmm0, [wind_dy]
    addss   xmm0, xmm0
    movss   xmm1, [wind_dx]
    addss   xmm1, [wind_dy]
    movaps  xmm2, xmm0
    mulss   xmm2, xmm0
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    addss   xmm2, xmm3
    sqrtss  xmm2, xmm2
.ok:
    movss   xmm3, [f_1]
    divss   xmm3, xmm2
    mulss   xmm0, xmm3
    mulss   xmm1, xmm3
    movss   [wh_ux], xmm0
    movss   [wh_uy], xmm1
    ; полудлина 3.5..7 px
    movss   xmm2, [pl_wsm]
    mulss   xmm2, [f_3_5]
    minss   xmm2, [f_3_5]
    addss   xmm2, [f_3_5]
    movss   [wh_r], xmm2
    ; тень, потом стрелка
    mov     dword [wh_col], 0x00101418
    mov     dword [wh_ox], 1
    call    .arrow
    mov     dword [wh_col], 0x00C8D8E8
    mov     dword [wh_ox], 0
    call    .arrow
    add     rsp, 40
    ret
.arrow:
    push    rsi
    sub     rsp, 32
    ; древко
    movss   xmm0, [wh_ux]
    mulss   xmm0, [wh_r]
    movss   xmm1, [wh_uy]
    mulss   xmm1, [wh_r]
    movss   xmm2, [f_wh_cx]
    subss   xmm2, xmm0
    movss   [wl_x0], xmm2
    movss   xmm2, [f_wh_cy]
    subss   xmm2, xmm1
    movss   [wl_y0], xmm2
    movss   xmm2, [f_wh_cx]
    addss   xmm2, xmm0
    movss   [wl_x1], xmm2
    movss   [wh_tx], xmm2
    movss   xmm2, [f_wh_cy]
    addss   xmm2, xmm1
    movss   [wl_y1], xmm2
    movss   [wh_ty], xmm2
    call    hud_line
    ; наконечник: назад 3.5 px, вбок ±2.5
    mov     esi, 2
    movss   xmm4, [f_2_5]
.hd:
    movss   xmm0, [wh_tx]
    movss   [wl_x0], xmm0
    movss   xmm0, [wh_ty]
    movss   [wl_y0], xmm0
    movss   xmm0, [wh_ux]
    mulss   xmm0, [f_3_5]
    movss   xmm1, [wh_uy]
    mulss   xmm1, xmm4
    movss   xmm2, [wh_tx]
    subss   xmm2, xmm0
    addss   xmm2, xmm1
    movss   [wl_x1], xmm2
    movss   xmm0, [wh_uy]
    mulss   xmm0, [f_3_5]
    movss   xmm1, [wh_ux]
    mulss   xmm1, xmm4
    movss   xmm2, [wh_ty]
    subss   xmm2, xmm0
    subss   xmm2, xmm1
    movss   [wl_y1], xmm2
    movss   [wh_side], xmm4
    call    hud_line
    movss   xmm4, [wh_side]
    xorps   xmm4, [f_signmask]
    dec     esi
    jnz     .hd
    add     rsp, 32
    pop     rsi
    ret

; отрезок (wl_x0, wl_y0) — (wl_x1, wl_y1) цветом wh_col со сдвигом wh_ox (тень)
hud_line:
    movss   xmm0, [wl_x1]
    subss   xmm0, [wl_x0]
    movss   xmm1, [wl_y1]
    subss   xmm1, [wl_y0]
    movaps  xmm2, xmm0
    andps   xmm2, [f_absmask]
    movaps  xmm3, xmm1
    andps   xmm3, [f_absmask]
    maxss   xmm2, xmm3
    cvtss2si ecx, xmm2
    inc     ecx
    cvtsi2ss xmm2, ecx
    divss   xmm0, xmm2
    divss   xmm1, xmm2
    movss   xmm2, [wl_x0]
    movss   xmm3, [wl_y0]
    mov     r9, [fbbits]
    mov     r10d, [wh_col]
    mov     r11d, [wh_ox]
.p:
    cvtss2si eax, xmm2
    add     eax, r11d
    cmp     eax, SCR_W
    jae     .n
    cvtss2si edx, xmm3
    add     edx, r11d
    cmp     edx, SCR_H
    jae     .n
    imul    edx, edx, SCR_W
    add     eax, edx
    mov     [r9 + rax*4], r10d
.n:
    addss   xmm2, xmm0
    addss   xmm3, xmm1
    dec     ecx
    jns     .p
    ret
