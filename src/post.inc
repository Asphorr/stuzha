; ----------------------------------------------------------------------------
;  post.inc — блум эмиссивных пикселей, конусы света фонарей, провода
; ----------------------------------------------------------------------------
section .text

; ecx = wx, edx = wy (8.8) -> eax = sx, edx = sy (на земле)
w2s:
    sub     ecx, [camx]
    sub     edx, [camy]
    mov     eax, ecx
    sub     eax, edx
    sar     eax, 4
    add     eax, CX0
    add     edx, ecx
    sar     edx, 5
    add     edx, CY0
    ret

; ---------------------------------------------------------------- блум
; 1) блоки 4x4 эмиссивных пикселей альбедо (бит 30 — «горячий», вес 12) -> blA
; 2) ящик 7 по горизонтали и вертикали, дважды
; 3) билинейно обратно и сложить с кадром с насыщением
bloom_pass:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    lea     rcx, [bloom_down]
    mov     edx, BLH
    mov     r8d, 4
    call    par_for
    ; размытие: ящик по строкам, потом по столбцам, дважды (линии — на всех ядрах)
    mov     r15d, 2
.bit:
    lea     rax, [blA]
    mov     [bb_src], rax
    lea     rax, [blB]
    mov     [bb_dst], rax
    mov     qword [bb_al], 8
    mov     qword [bb_ac], BLW*8
    mov     dword [bb_len], BLW
    lea     rcx, [blur_lines]
    mov     edx, BLH/2
    mov     r8d, 4
    call    par_for
    lea     rax, [blB]
    mov     [bb_src], rax
    lea     rax, [blA]
    mov     [bb_dst], rax
    mov     qword [bb_al], BLW*8
    mov     qword [bb_ac], 8
    mov     dword [bb_len], BLH
    lea     rcx, [blur_lines]
    mov     edx, BLW/2
    mov     r8d, 8
    call    par_for
    dec     r15d
    jnz     .bit
    lea     rcx, [bloom_add]
    mov     edx, SCR_H
    mov     r8d, 8
    call    par_for
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

; строки блоков ecx .. edx−1: эмиссивное альбедо 4x4 -> blA (с любого потока пула)
bloom_down:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    mov     r12d, ecx
    mov     r14d, edx
    pxor    xmm15, xmm15
    movdqa  xmm14, [bl_hot11]
    movdqa  xmm13, [w_one]
    movdqa  xmm12, [rgb_mask]
    imul    eax, ecx, BLW*8
    lea     rdi, [blA]
    add     rdi, rax
.by:
    imul    esi, r12d, SCR_W*4*4
    lea     rbx, [albedo]
    add     rbx, rsi
    xor     r13d, r13d
.bx:
    pxor    xmm0, xmm0
    mov     rsi, rbx
    mov     ecx, 4
.rr:
    movdqa  xmm1, [rsi]
    movdqa  xmm2, xmm1
    psrad   xmm2, 31                    ; эмиссивные
    movdqa  xmm3, xmm1
    pslld   xmm3, 1
    psrad   xmm3, 31                    ; горячие
    pand    xmm3, xmm2
    pand    xmm1, xmm2
    pand    xmm1, xmm12
    pand    xmm3, xmm14
    movdqa  xmm4, xmm1
    punpcklbw xmm4, xmm15
    movdqa  xmm5, xmm3
    punpcklbw xmm5, xmm15
    paddw   xmm5, xmm13
    pmullw  xmm4, xmm5
    paddw   xmm0, xmm4
    punpckhbw xmm1, xmm15
    punpckhbw xmm3, xmm15
    paddw   xmm3, xmm13
    pmullw  xmm1, xmm3
    paddw   xmm0, xmm1
    add     rsi, SCR_W*4
    dec     ecx
    jnz     .rr
    movdqa  xmm1, xmm0
    psrldq  xmm1, 8
    paddw   xmm0, xmm1
    psrlw   xmm0, 4
    movq    [rdi], xmm0
    add     rdi, 8
    add     rbx, 16
    inc     r13d
    cmp     r13d, BLW
    jb      .bx
    inc     r12d
    cmp     r12d, r14d
    jb      .by
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; строки кадра ecx .. edx−1: blA билинейно, сложить с кадром (с любого потока пула;
; строка блума по x — на своём стеке)
BL_ROWSZ equ (BLW+2)*8
bloom_add:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, BL_ROWSZ + 8
    mov     [rsp + BL_ROWSZ], edx       ; конец диапазона
    mov     r12d, ecx                   ; y
    mov     r8, [fbbits]
    movdqa  xmm11, [bl_gain]
    movdqa  xmm10, [w_400]
    mov     rbx, rsp                    ; строка блума
    lea     r15, [bl_wt]
.oy:
    lea     eax, [r12d - 1]
    mov     ecx, eax
    sar     eax, 2                      ; t0
    and     ecx, 3                      ; k
    lea     edx, [eax + 1]
    xor     r9d, r9d
    cmp     eax, r9d
    cmovl   eax, r9d
    mov     r9d, BLH-1
    cmp     edx, r9d
    cmovg   edx, r9d
    cmp     eax, r9d
    cmovg   eax, r9d
    imul    eax, eax, BLW*8
    imul    edx, edx, BLW*8
    lea     rsi, [blA]
    lea     rdi, [rsi + rdx]
    add     rsi, rax
    shl     ecx, 4
    movq    xmm8, [r15 + rcx]
    movq    xmm9, [r15 + rcx + 8]
    xor     ecx, ecx
.rb:
    movq    xmm0, [rsi + rcx*8]
    movq    xmm1, [rdi + rcx*8]
    pmullw  xmm0, xmm8
    pmullw  xmm1, xmm9
    paddw   xmm0, xmm1
    psrlw   xmm0, 2
    movq    [rbx + rcx*8 + 8], xmm0
    inc     ecx
    cmp     ecx, BLW
    jb      .rb
    mov     rax, [rbx + 8]
    mov     [rbx], rax
    mov     rax, [rbx + BLW*8]
    mov     [rbx + BLW*8 + 8], rax
    imul    r13d, r12d, SCR_W
    xor     r14d, r14d                  ; x
.ox:
    lea     eax, [r14d - 1]
    mov     ecx, eax
    sar     eax, 2
    and     ecx, 3
    inc     eax
    mov     rdx, [rbx + rax*8]
    or      rdx, [rbx + rax*8 + 8]
    jz      .oxn
    movq    xmm0, [rbx + rax*8]
    movq    xmm1, [rbx + rax*8 + 8]
    shl     ecx, 4
    movq    xmm2, [r15 + rcx]
    movq    xmm3, [r15 + rcx + 8]
    pmullw  xmm0, xmm2
    pmullw  xmm1, xmm3
    paddw   xmm0, xmm1
    psrlw   xmm0, 2
    pminsw  xmm0, xmm10
    pmullw  xmm0, xmm11
    psrlw   xmm0, 3
    packuswb xmm0, xmm0
    lea     eax, [r13d + r14d]
    movd    xmm1, [r8 + rax*4]
    paddusb xmm1, xmm0
    movd    [r8 + rax*4], xmm1
.oxn:
    inc     r14d
    cmp     r14d, SCR_W
    jb      .ox
    inc     r12d
    cmp     r12d, [rsp + BL_ROWSZ]
    jb      .oy
    add     rsp, BL_ROWSZ + 8
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ящик 7 (края прижаты) по линиям: bb_src -> bb_dst, тексели по 4 слова; bb_al — шаг
; вдоль (байты), bb_ac — поперёк, bb_len — длина. Скользящая сумма:
; +src[min(p+4, n−1)] − src[max(p−3, 0)]. Линии — парами, пара в одном xmm (те же
; операции над словами); ecx .. edx−1 — пары (с любого потока пула)
blur_lines:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    mov     ebp, edx
    mov     rsi, [bb_src]
    mov     rdi, [bb_dst]
    mov     r8, [bb_al]
    mov     r9, [bb_ac]
    mov     r10d, [bb_len]
    movdqa  xmm7, [w_div7]
    lea     r15d, [r10d - 1]
    imul    r15, r8                     ; смещение последнего
    mov     r12d, ecx
.ln:
    lea     rax, [r12 + r12]
    imul    rax, r9
    lea     rbx, [rsi + rax]
    lea     r14, [rdi + rax]
    ; сумма для p = 0: 4·s[0] + s[1] + s[2] + s[3] (у второй линии — в старшей половине)
    movq    xmm0, [rbx]
    movhps  xmm0, [rbx + r9]
    movdqa  xmm2, xmm0                  ; s[0] — левый край
    psllw   xmm0, 2
    lea     rax, [rbx + r8]
    movq    xmm1, [rax]
    movhps  xmm1, [rax + r9]
    paddw   xmm0, xmm1
    add     rax, r8
    movq    xmm1, [rax]
    movhps  xmm1, [rax + r9]
    paddw   xmm0, xmm1
    add     rax, r8
    movq    xmm1, [rax]
    movhps  xmm1, [rax + r9]
    paddw   xmm0, xmm1
    movq    xmm3, [rbx + r15]           ; правый край
    add     r15, r9
    movhps  xmm3, [rbx + r15]
    sub     r15, r9
    xor     r13d, r13d
    mov     rcx, r14                    ; куда писать
    lea     rdx, [rbx + r8*4]           ; входящий s[p+4]
    mov     r11, rbx                    ; уходящий s[p−3] (после p ≥ 3)
.ps:
    movdqa  xmm1, xmm0
    pmulhuw xmm1, xmm7
    movq    [rcx], xmm1
    movhps  [rcx + r9], xmm1
    add     rcx, r8
    ; входящий
    lea     eax, [r13d + 4]
    cmp     eax, r10d
    jb      .in
    movdqa  xmm1, xmm3
    jmp     .ina
.in:
    movq    xmm1, [rdx]
    movhps  xmm1, [rdx + r9]
    add     rdx, r8
.ina:
    paddw   xmm0, xmm1
    ; уходящий
    cmp     r13d, 3
    jae     .out3
    psubw   xmm0, xmm2
    jmp     .nx
.out3:
    movq    xmm1, [r11]
    movhps  xmm1, [r11 + r9]
    psubw   xmm0, xmm1
    add     r11, r8
.nx:
    inc     r13d
    cmp     r13d, r10d
    jb      .ps
    inc     r12d
    cmp     r12d, ebp
    jb      .ln
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- конусы света в снегопад
draw_cones:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    movss   xmm0, [lamp_t]
    comiss  xmm0, [f_0_05]
    jb      .out
    lea     rbx, [lights]
    mov     r15d, [nlights]
.l:
    test    r15d, r15d
    jz      .out
    mov     eax, [rbx + L_KIND]
    cmp     eax, LK_LAMP
    je      .lamp
    cmp     eax, LK_BROKEN
    jne     .n
.lamp:
    movss   xmm7, [lamp_t]
    cmp     eax, LK_BROKEN
    jne     .i1
    mulss   xmm7, [brk_lvl]
.i1:
    comiss  xmm7, [f_0_05]
    jb      .n
    cvttss2si r12d, [rbx + L_X]
    cvttss2si r13d, [rbx + L_Y]
    mov     eax, r12d
    shl     eax, 8
    sub     eax, [camx]
    mov     ecx, r13d
    shl     ecx, 8
    sub     ecx, [camy]
    mov     esi, eax
    sub     esi, ecx
    sar     esi, 4
    add     esi, CX0 + 5                ; середина лампы
    add     eax, ecx
    sar     eax, 5
    add     eax, CY0 - 30
    mov     edi, eax                    ; сразу под лампой
    cmp     esi, -16
    jl      .n
    cmp     esi, SCR_W + 16
    jg      .n
    cmp     edi, SCR_H
    jg      .n
    cmp     edi, -44
    jl      .n
    lea     eax, [r12d + r13d + 1]
    shl     eax, 8
    add     eax, 128
    mov     [dc_depth], eax
    mulss   xmm7, [f_0_13]
    mulss   xmm7, [cone_k]              ; конус виден по снегу в воздухе: в метель ярче
    xor     r12d, r12d                  ; строка
.r:
    cmp     r12d, 39
    jge     .n
    lea     eax, [edi + r12d]
    cmp     eax, SCR_H
    jae     .rn
    imul    r14d, eax, SCR_W
    cvtsi2ss xmm0, r12d
    mulss   xmm0, [f_inv39]             ; t
    movss   xmm1, xmm0
    mulss   xmm1, [f_9_5]
    addss   xmm1, [f_1_5]               ; полуширина
    movss   xmm2, [f_1]
    divss   xmm2, xmm1
    movss   xmm3, xmm0
    mulss   xmm3, [f_0_65]
    movss   xmm4, [f_1]
    subss   xmm4, xmm3
    mulss   xmm4, xmm7
    cvttss2si ebp, xmm1
    inc     ebp
    mov     r13d, ebp
    neg     r13d
.x:
    cmp     r13d, ebp
    jge     .rn
    lea     eax, [esi + r13d]
    cmp     eax, SCR_W
    jae     .xn
    cvtsi2ss xmm0, r13d
    addss   xmm0, [f_0_5]
    mulss   xmm0, xmm2
    mulss   xmm0, xmm0
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    comiss  xmm1, [f_0]
    jbe     .xn
    add     eax, r14d
    lea     rdx, [gposb]
    mov     ecx, [rdx + rax*4]
    movzx   edx, cx
    shr     ecx, 16
    add     ecx, edx
    cmp     ecx, [dc_depth]
    jg      .xn
    mulss   xmm1, xmm4
    mulss   xmm1, [f_256]
    cvttss2si ecx, xmm1
    movd    xmm0, ecx
    pshuflw xmm0, xmm0, 0
    pmullw  xmm0, [cone_col]
    psrlw   xmm0, 8
    packuswb xmm0, xmm0
    mov     rdx, [fbbits]
    movd    xmm1, [rdx + rax*4]
    paddusb xmm1, xmm0
    movd    [rdx + rax*4], xmm1
.xn:
    inc     r13d
    jmp     .x
.rn:
    inc     r12d
    jmp     .r
.n:
    add     rbx, L_SZ
    dec     r15d
    jmp     .l
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

; ---------------------------------------------------------------- провода
; соседние фонари одной линии, не дальше 17 тайлов
gen_wires:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32
    mov     dword [nwires], 0
    lea     rsi, [lamps]
    xor     ebx, ebx
.i:
    cmp     ebx, [nlamps]
    jae     .out
    lea     edi, [ebx + 1]
.j:
    cmp     edi, [nlamps]
    jae     .in
    mov     al, [rsi + rbx*4 + 2]
    cmp     al, [rsi + rdi*4 + 2]
    jne     .jn
    movzx   eax, byte [rsi + rbx*4]
    movzx   ecx, byte [rsi + rdi*4]
    sub     eax, ecx
    cdq
    xor     eax, edx
    sub     eax, edx
    movzx   ecx, byte [rsi + rbx*4 + 1]
    movzx   edx, byte [rsi + rdi*4 + 1]
    sub     ecx, edx
    mov     edx, ecx
    sar     edx, 31
    xor     ecx, edx
    sub     ecx, edx
    add     eax, ecx
    cmp     eax, 17
    ja      .in
    mov     eax, [nwires]
    cmp     eax, MAXWIRE
    jae     .out
    lea     rdx, [wires]
    lea     rdx, [rdx + rax*8]
    mov     cl, [rsi + rbx*4]
    mov     [rdx], cl
    mov     cl, [rsi + rbx*4 + 1]
    mov     [rdx + 1], cl
    mov     cl, [rsi + rdi*4]
    mov     [rdx + 2], cl
    mov     cl, [rsi + rdi*4 + 1]
    mov     [rdx + 3], cl
    call    rand
    mov     [rdx + 4], al
    inc     dword [nwires]
    jmp     .in
.jn:
    inc     edi
    jmp     .j
.in:
    inc     ebx
    jmp     .i
.out:
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; провода с провисом, качаются на ветру; свет — от земли под проводом
draw_wires:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    lea     rbx, [wires]
    mov     r15d, [nwires]
.w:
    test    r15d, r15d
    jz      .out
    movzx   eax, byte [rbx]
    shl     eax, 8
    add     eax, 128
    mov     [dw_ax], eax
    movzx   eax, byte [rbx + 1]
    shl     eax, 8
    add     eax, 128
    mov     [dw_ay], eax
    movzx   eax, byte [rbx + 2]
    shl     eax, 8
    add     eax, 128
    mov     [dw_bx], eax
    movzx   eax, byte [rbx + 3]
    shl     eax, 8
    add     eax, 128
    mov     [dw_by], eax
    mov     ecx, [dw_ax]
    mov     edx, [dw_ay]
    call    w2s
    mov     r12d, eax
    mov     r13d, edx
    mov     ecx, [dw_bx]
    mov     edx, [dw_by]
    call    w2s
    ; отсечка
    mov     ecx, eax
    cmp     ecx, r12d
    cmovl   ecx, r12d                   ; max x
    cmp     ecx, -4
    jl      .wn
    mov     ecx, eax
    cmp     ecx, r12d
    cmovg   ecx, r12d                   ; min x
    cmp     ecx, SCR_W + 4
    jg      .wn
    mov     ecx, edx
    cmp     ecx, r13d
    cmovl   ecx, r13d                   ; max y
    cmp     ecx, -4
    jl      .wn
    mov     ecx, edx
    cmp     ecx, r13d
    cmovg   ecx, r13d                   ; min y
    cmp     ecx, SCR_H + 44
    jg      .wn
    ; шагов = длина на экране
    sub     eax, r12d
    mov     ecx, eax
    sar     ecx, 31
    xor     eax, ecx
    sub     eax, ecx
    sub     edx, r13d
    mov     ecx, edx
    sar     ecx, 31
    xor     edx, ecx
    sub     edx, ecx
    cmp     eax, edx
    cmovl   eax, edx
    inc     eax
    mov     r14d, eax
    ; ветер в середине провода
    mov     eax, [dw_ax]
    add     eax, [dw_bx]
    cvtsi2ss xmm0, eax
    mulss   xmm0, [f_inv512]
    mov     eax, [dw_ay]
    add     eax, [dw_by]
    cvtsi2ss xmm1, eax
    mulss   xmm1, [f_inv512]
    movss   xmm2, [f_40]
    call    wind_vec
    movss   [dw_g], xmm2
    movss   [dw_wx], xmm0
    movss   [dw_wy], xmm1
    movzx   eax, byte [rbx + 4]
    cvtsi2ss xmm0, eax
    mulss   xmm0, [f_0_025]
    movss   [dw_ph], xmm0
    movss   xmm1, [wind_t]
    mulss   xmm1, [f_1_9]
    addss   xmm0, xmm1
    call    fsin
    mulss   xmm0, [dw_g]
    mulss   xmm0, [f_2]
    addss   xmm0, [f_14]
    movss   [dw_sag], xmm0              ; провис, px
    movss   xmm0, [dw_ph]
    mulss   xmm0, [f_1_7]
    movss   xmm1, [wind_t]
    mulss   xmm1, [f_1_3]
    addss   xmm0, xmm1
    call    fsin
    mulss   xmm0, [dw_g]
    mulss   xmm0, [f_14]
    movss   [dw_sw], xmm0               ; колебание поперёк (8.8)
    mov     eax, [dw_by]
    sub     eax, [dw_ay]
    cvtsi2ss xmm1, eax
    mov     eax, [dw_bx]
    sub     eax, [dw_ax]
    cvtsi2ss xmm2, eax
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    movaps  xmm4, xmm2
    mulss   xmm4, xmm2
    addss   xmm3, xmm4
    sqrtss  xmm3, xmm3
    xorps   xmm1, [f_signmask]
    divss   xmm1, xmm3
    divss   xmm2, xmm3                  ; нормаль к проводу по земле
    ; + прогиб по ветру: ветер поперёк провода · сила · 40
    movss   xmm4, [dw_wx]
    mulss   xmm4, xmm1
    movss   xmm5, [dw_wy]
    mulss   xmm5, xmm2
    addss   xmm4, xmm5
    mulss   xmm4, [dw_g]
    mulss   xmm4, [f_40]
    addss   xmm4, [dw_sw]
    movss   [dw_sw], xmm4
    mulss   xmm1, [dw_sw]
    mulss   xmm2, [dw_sw]
    movss   [dw_px], xmm1
    movss   [dw_py], xmm2
    cvtsi2ss xmm0, r14d
    movss   xmm1, [f_1]
    divss   xmm1, xmm0
    movss   [dw_dt], xmm1
    xor     ebp, ebp
.s:
    cmp     ebp, r14d
    jg      .wn
    cvtsi2ss xmm0, ebp
    mulss   xmm0, [dw_dt]               ; t
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    mulss   xmm1, xmm0
    mulss   xmm1, [f_4]                 ; 4t(1−t)
    mov     eax, [dw_bx]
    sub     eax, [dw_ax]
    cvtsi2ss xmm2, eax
    mulss   xmm2, xmm0
    movss   xmm3, [dw_px]
    mulss   xmm3, xmm1
    addss   xmm2, xmm3
    cvtss2si esi, xmm2
    add     esi, [dw_ax]                ; wx
    mov     eax, [dw_by]
    sub     eax, [dw_ay]
    cvtsi2ss xmm2, eax
    mulss   xmm2, xmm0
    movss   xmm3, [dw_py]
    mulss   xmm3, xmm1
    addss   xmm2, xmm3
    cvtss2si edi, xmm2
    add     edi, [dw_ay]                ; wy
    movss   xmm2, [dw_sag]
    mulss   xmm2, xmm1
    cvtss2si r12d, xmm2
    mov     ecx, esi
    mov     edx, edi
    call    w2s
    sub     edx, 40
    add     edx, r12d
    cmp     eax, SCR_W
    jae     .sn
    cmp     edx, SCR_H
    jae     .sn
    imul    edx, edx, SCR_W
    add     eax, edx
    lea     rdx, [gposb]
    mov     ecx, [rdx + rax*4]
    movzx   r8d, cx
    shr     ecx, 16
    add     ecx, r8d
    lea     r8d, [esi + edi + 64]
    cmp     ecx, r8d
    jg      .sn
    lea     rcx, [albedo]
    mov     dword [rcx + rax*4], 0x01464C56
    lea     rcx, [nbuf]
    mov     byte [rcx + rax], N_UP
    mov     ecx, edi
    shl     ecx, 16
    or      ecx, esi
    mov     [rdx + rax*4], ecx
.sn:
    inc     ebp
    jmp     .s
.wn:
    add     rbx, 8
    dec     r15d
    jmp     .w
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
