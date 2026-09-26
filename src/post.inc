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
    cmp     dword [have_avx2], 0
    je      .bd
    lea     rcx, [bloom_down_x]
.bd:
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
    cmp     dword [have_avx2], 0
    je      .ba
    lea     rcx, [bloom_add_x]
.ba:
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

; то же на AVX2: два блока рядом — в половинах ymm (те же операции по словам)
bloom_down_x:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    mov     r12d, ecx
    mov     r14d, edx
    vpxor   ymm15, ymm15, ymm15
    vbroadcasti128 ymm14, [bl_hot11]
    vbroadcasti128 ymm13, [w_one]
    vbroadcasti128 ymm12, [rgb_mask]
    imul    eax, ecx, BLW*8
    lea     rdi, [blA]
    add     rdi, rax
.by:
    imul    esi, r12d, SCR_W*4*4
    lea     rbx, [albedo]
    add     rbx, rsi
    xor     r13d, r13d
.bx:
    vpxor   ymm0, ymm0, ymm0
    mov     rsi, rbx
    mov     ecx, 4
.rr:
    vmovdqu ymm1, [rsi]
    vpsrad  ymm2, ymm1, 31              ; эмиссивные
    vpslld  ymm3, ymm1, 1
    vpsrad  ymm3, ymm3, 31              ; горячие
    vpand   ymm3, ymm3, ymm2
    vpand   ymm1, ymm1, ymm2
    vpand   ymm1, ymm1, ymm12
    vpand   ymm3, ymm3, ymm14
    vpunpcklbw ymm4, ymm1, ymm15
    vpunpcklbw ymm5, ymm3, ymm15
    vpaddw  ymm5, ymm5, ymm13
    vpmullw ymm4, ymm4, ymm5
    vpaddw  ymm0, ymm0, ymm4
    vpunpckhbw ymm1, ymm1, ymm15
    vpunpckhbw ymm3, ymm3, ymm15
    vpaddw  ymm3, ymm3, ymm13
    vpmullw ymm1, ymm1, ymm3
    vpaddw  ymm0, ymm0, ymm1
    add     rsi, SCR_W*4
    dec     ecx
    jnz     .rr
    vpsrldq ymm1, ymm0, 8
    vpaddw  ymm0, ymm0, ymm1
    vpsrlw  ymm0, ymm0, 4
    vpermq  ymm0, ymm0, 0x08            ; блоки — подряд
    vmovdqu [rdi], xmm0
    add     rdi, 16
    add     rbx, 32
    add     r13d, 2
    cmp     r13d, BLW
    jb      .bx
    inc     r12d
    cmp     r12d, r14d
    jb      .by
    vzeroupper
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
    movdqa  xmm11, [bl_gainv]           ; с халацией — по палитре (grade.inc)
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

; то же на AVX2: пиксели x = 4t+1 .. 4t+4 берут одну пару текселей (t+1, t+2) с весами
; k = 0..3 — четыре пикселя одним ymm (vpbroadcastq текселей, веса — bl_wx), пустую пару
; видно раз на четвёрку; вертикаль строки — по 4 текселя. Края (x = 0, 637..639) — как у
; bloom_add. Слова — те же операции
bloom_add_x:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, BL_ROWSZ + 8
    mov     [rsp + BL_ROWSZ], edx
    mov     r12d, ecx
    mov     r8, [fbbits]
    movdqa  xmm11, [bl_gainv]
    movdqa  xmm10, [w_400]
    mov     rbx, rsp
    lea     r15, [bl_wt]
    vbroadcasti128 ymm12, [bl_gainv]
    vbroadcasti128 ymm13, [w_400]
    vmovdqu ymm14, [bl_wx]              ; веса a: 4, 3, 2, 1 — по пикселю
    vmovdqu ymm15, [bl_wx + 32]         ; веса b: 0, 1, 2, 3
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
    vpbroadcastq ymm8, [r15 + rcx]
    vpbroadcastq ymm9, [r15 + rcx + 8]
    xor     ecx, ecx
.rb:
    vmovdqu ymm0, [rsi + rcx*8]
    vpmullw ymm0, ymm0, ymm8
    vpmullw ymm1, ymm9, [rdi + rcx*8]
    vpaddw  ymm0, ymm0, ymm1
    vpsrlw  ymm0, ymm0, 2
    vmovdqu [rbx + rcx*8 + 8], ymm0
    add     ecx, 4
    cmp     ecx, BLW
    jb      .rb
    mov     rax, [rbx + 8]
    mov     [rbx], rax
    mov     rax, [rbx + BLW*8]
    mov     [rbx + BLW*8 + 8], rax
    imul    r13d, r12d, SCR_W
    xor     r14d, r14d
    mov     ebp, 1
    call    .sc                         ; x = 0
    ; x = 1 .. 636: четвёрки
    lea     rdx, [r8 + r13*4 + 4]
    mov     eax, 1                      ; t + 1
.q:
    mov     rcx, [rbx + rax*8]
    or      rcx, [rbx + rax*8 + 8]
    jz      .qn
    vpbroadcastq ymm0, [rbx + rax*8]
    vpbroadcastq ymm1, [rbx + rax*8 + 8]
    vpmullw ymm0, ymm0, ymm14
    vpmullw ymm1, ymm1, ymm15
    vpaddw  ymm0, ymm0, ymm1
    vpsrlw  ymm0, ymm0, 2
    vpminsw ymm0, ymm0, ymm13
    vpmullw ymm0, ymm0, ymm12
    vpsrlw  ymm0, ymm0, 3
    vpackuswb ymm0, ymm0, ymm0
    vpermq  ymm0, ymm0, 0x08            ; пиксели k = 0, 1 | 2, 3 -> подряд
    vpaddusb xmm0, xmm0, [rdx]
    vmovdqu [rdx], xmm0
.qn:
    add     rdx, 16
    inc     eax
    cmp     eax, BLW
    jb      .q
    mov     r14d, SCR_W - 3
    mov     ebp, SCR_W
    call    .sc                         ; x = 637 .. 639
    inc     r12d
    cmp     r12d, [rsp + BL_ROWSZ]
    jb      .oy
    vzeroupper
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
; пиксели r14d .. ebp−1 строки — по одному, как bloom_add (VEX: верх ymm занят)
.sc:
    lea     eax, [r14d - 1]
    mov     ecx, eax
    sar     eax, 2
    and     ecx, 3
    inc     eax
    mov     rdx, [rbx + rax*8]
    or      rdx, [rbx + rax*8 + 8]
    jz      .scn
    vmovq   xmm0, [rbx + rax*8]
    vmovq   xmm1, [rbx + rax*8 + 8]
    shl     ecx, 4
    vpmullw xmm0, xmm0, [r15 + rcx]
    vpmullw xmm1, xmm1, [r15 + rcx + 8]
    vpaddw  xmm0, xmm0, xmm1
    vpsrlw  xmm0, xmm0, 2
    vpminsw xmm0, xmm0, xmm10
    vpmullw xmm0, xmm0, xmm11
    vpsrlw  xmm0, xmm0, 3
    vpackuswb xmm0, xmm0, xmm0
    lea     eax, [r13d + r14d]
    vmovd   xmm1, [r8 + rax*4]
    vpaddusb xmm1, xmm1, xmm0
    vmovd   [r8 + rax*4], xmm1
.scn:
    inc     r14d
    cmp     r14d, ebp
    jb      .sc
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
    xorps   xmm0, xmm0                  ; иначе ждёт пиксель прошлого шага (слияние верха)
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

; ---------------------------------------------------------------- световые столбы
; В ясный тихий мороз (frost_k) в воздухе висят ледяные пластинки: плашмя они ловят
; свет фонаря и тянут над ним узкий столб — к верху тает, по высоте мерцает; вокруг
; лампы — мягкий ореол и бледное кольцо. Складывается с кадром до цвета (grade.inc);
; что ближе к глазу (x + y в G-буфере больше), то закрывает
PL_H        equ 170                     ; высота столба, px
PL_COL      equ 32                      ; цвет (B, G, R, 0), с силой мороза
PL_SX       equ 48                      ; лампа на экране, f32
PL_SY       equ 52
PL_FX       equ 56                      ; доля x внутри пикселя
PL_KEY      equ 60                      ; x + y лампы, мир 8.8
PL_IX       equ 64                      ; пиксель лампы
PL_IY       equ 68
PL_E        equ 72                      ; e^(−h/55)
PL_A        equ 76                      ; сила строки
PL_IW       equ 80                      ; 1 / полуширина строки
PL_VOK      equ 84                      ; строки — по 8 пикселей (AVX2)
PL_DX2      equ 96                      ; ореол: dx² семи групп
PL_PV       equ 320                     ; столб: x + 0.5 − FX двух групп
PL_SZ       equ 408

; ymm15 = x + y лампы, ymm11..14 — цвет по каналам, ymm10 — байты по пикселям
%macro PLCONST 0
    vpbroadcastd ymm15, [rsp + PL_KEY]
    vbroadcastss ymm14, [rsp + PL_COL]
    vbroadcastss ymm13, [rsp + PL_COL + 4]
    vbroadcastss ymm12, [rsp + PL_COL + 8]
    vbroadcastss ymm11, [rsp + PL_COL + 12]
    vmovdqu ymm10, [pl_tr]
%endmacro
; .add на 8 пикселях с rax: ymm0 = сила по дорожкам (0 — мимо); что ближе к глазу —
; закрывает; цвет·сила -> cvttps2dq -> packssdw -> packuswb по каналам, как у .add;
; четыре канала восьми пикселей -> байты по пикселям одним vpshufb (портит ymm1–4)
%macro PLADD8 0
    vmovdqu ymm1, [rsi + rax*4]
    vpsrld  ymm2, ymm1, 16
    vpand   ymm1, ymm1, [lx_dffff]
    vpaddd  ymm1, ymm1, ymm2
    vpcmpgtd ymm1, ymm1, ymm15
    vpandn  ymm0, ymm1, ymm0
    vmulps  ymm1, ymm0, ymm14
    vmulps  ymm2, ymm0, ymm13
    vmulps  ymm3, ymm0, ymm12
    vmulps  ymm4, ymm0, ymm11
    vcvttps2dq ymm1, ymm1
    vcvttps2dq ymm2, ymm2
    vcvttps2dq ymm3, ymm3
    vcvttps2dq ymm4, ymm4
    vpackssdw ymm1, ymm1, ymm2
    vpackssdw ymm3, ymm3, ymm4
    vpackuswb ymm1, ymm1, ymm3          ; B0..3 G0..3 R0..3 ·0..3 | то же 4..7
    vpshufb ymm1, ymm1, ymm10
    vpaddusb ymm1, ymm1, [r14 + rax*4]
    vmovdqu [r14 + rax*4], ymm1
%endmacro
; группа %1 строки столба (edi — пиксель её дорожки 0 группы 0), ymm8 = 1/полуширина,
; ymm9 = сила строки; %2 — у группы лишь три дорожки
%macro PLCOL 2
    vmulps  ymm0, ymm8, [rsp + PL_PV + 32*%1]
    vmulps  ymm0, ymm0, ymm0
    vmovaps ymm1, [lt_one4]
    vsubps  ymm1, ymm1, ymm0            ; 1 − u²
    vxorps  ymm2, ymm2, ymm2
    vcmpps  ymm2, ymm1, ymm2, 0x1E      ; > 0
%if %2
    vandps  ymm2, ymm2, [pl_g6m]
%endif
    vmulps  ymm1, ymm1, ymm1
    vmulps  ymm1, ymm1, ymm9
    vandps  ymm0, ymm1, ymm2
    lea     eax, [rdi + 8*%1]
    PLADD8
%endmacro
; группа %1 строки ореола, ymm9 = dy²; %2 — лишь три дорожки
%macro PLHALO 2
    vaddps  ymm5, ymm9, [rsp + PL_DX2 + 32*%1]  ; r²
    vcmpltps ymm6, ymm5, [pl_routv]     ; дальше кольца — пусто
    vcmpltps ymm7, ymm5, [pl_rdiskv]
    vcmpnltps ymm8, ymm5, [pl_rinv]     ; между диском и кольцом — пусто
    vorps   ymm7, ymm7, ymm8
    vandps  ymm6, ymm6, ymm7
%if %2
    vandps  ymm6, ymm6, [pl_g6m]
%endif
    vtestps ymm6, ymm6
    jz      %%n
    vmulps  ymm7, ymm5, [pl_ig2v]
    vmovaps ymm8, [lt_one4]
    vsubps  ymm7, ymm8, ymm7
    vmaxps  ymm7, ymm7, [lt_zero4]
    vmulps  ymm7, ymm7, ymm7
    vmulps  ymm7, ymm7, [pl_agv]        ; диск
    vsqrtps ymm5, ymm5
    vsubps  ymm5, ymm5, [pl_r0v]
    vmulps  ymm5, ymm5, [pl_irwv]
    vmulps  ymm5, ymm5, ymm5
    vsubps  ymm5, ymm8, ymm5
    vmaxps  ymm5, ymm5, [lt_zero4]
    vmulps  ymm5, ymm5, ymm5
    vmulps  ymm5, ymm5, [pl_arv]        ; кольцо
    vaddps  ymm0, ymm7, ymm5
    vcmpps  ymm5, ymm0, [pl_epsv], 0x1D ; ≥ eps
    vandps  ymm6, ymm6, ymm5
    vandps  ymm0, ymm0, ymm6
    lea     eax, [rdi + 8*%1]
    PLADD8
%%n:
%endmacro

draw_pillars:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, PL_SZ
    movss   xmm0, [frost_k]
    comiss  xmm0, [f_0_03]
    jb      .out
    movss   xmm0, [lamp_t]
    comiss  xmm0, [f_0_05]
    jb      .out
    mov     r14, [fbbits]
    lea     rsi, [gposb]
    lea     rbx, [lights]
    mov     r15d, [nlights]
.l:
    test    r15d, r15d
    jz      .out
    mov     eax, [rbx + L_KIND]
    movups  xmm7, [col_lamp]
    cmp     eax, LK_LAMP
    jne     .lb
    movups  xmm0, [lampf_vec]
    jmp     .lc
.lb:
    cmp     eax, LK_BROKEN
    jne     .n
    movups  xmm0, [lampb_vec]
.lc:
    mulps   xmm7, xmm0
    movss   xmm0, [frost_k]
    mulss   xmm0, [pl_gain]
    shufps  xmm0, xmm0, 0
    mulps   xmm7, xmm0
    movaps  [rsp + PL_COL], xmm7
    movhlps xmm0, xmm7
    comiss  xmm0, [f_2]                 ; красный слабее двух — нечего рисовать
    jb      .n
    ; лампа на экране: над точкой фонаря на высоте стекла
    movss   xmm0, [rbx + L_X]
    mulss   xmm0, [f_256]
    movss   xmm1, [rbx + L_Y]
    mulss   xmm1, [f_256]
    movaps  xmm2, xmm0
    addss   xmm2, xmm1
    cvttss2si eax, xmm2
    add     eax, 64
    mov     [rsp + PL_KEY], eax
    cvtsi2ss xmm2, dword [camx]
    subss   xmm0, xmm2
    cvtsi2ss xmm2, dword [camy]
    subss   xmm1, xmm2
    movaps  xmm2, xmm0
    subss   xmm2, xmm1
    mulss   xmm2, [f_inv16]
    addss   xmm2, [lt_cx0]
    movss   [rsp + PL_SX], xmm2
    addss   xmm0, xmm1
    mulss   xmm0, [f_inv32]
    addss   xmm0, [lt_cy0]
    subss   xmm0, [pl_z]
    movss   [rsp + PL_SY], xmm0
    comiss  xmm2, [pl_xm0]
    jb      .n
    comiss  xmm2, [pl_xm1]
    ja      .n
    comiss  xmm0, [pl_ym0]
    jb      .n
    comiss  xmm0, [pl_ym1]
    ja      .n
    cvttss2si eax, xmm2
    mov     [rsp + PL_IX], eax
    cvtsi2ss xmm1, eax
    subss   xmm2, xmm1
    movss   [rsp + PL_FX], xmm2
    cvtss2si eax, xmm0
    mov     [rsp + PL_IY], eax
    ; AVX2 и лампа не у края (x − 25 .. x + 30 в кадре): строки столба и ореола — по 8
    ; пикселей подряд; dx по группам — заранее (те же операции, что по пикселю ниже)
    mov     dword [rsp + PL_VOK], 0
    cmp     dword [have_avx2], 0
    je      .nv
    mov     eax, [rsp + PL_IX]
    cmp     eax, 25
    jl      .nv
    cmp     eax, SCR_W - 31
    jg      .nv
    mov     dword [rsp + PL_VOK], 1
    vbroadcastss ymm1, [rsp + PL_FX]
    vbroadcastss ymm2, [f_0_5]
    xor     ecx, ecx
.vg:
    lea     eax, [rcx*8 - 25]
    vmovd   xmm0, eax
    vpbroadcastd ymm0, xmm0
    vpaddd  ymm0, ymm0, [pl_lane8]
    vcvtdq2ps ymm0, ymm0
    vaddps  ymm0, ymm0, ymm2
    vsubps  ymm0, ymm0, ymm1            ; x + 0.5 − FX
    vmulps  ymm0, ymm0, ymm0
    mov     eax, ecx
    shl     eax, 5
    vmovups [rsp + rax + PL_DX2], ymm0  ; ореол: dx²
    inc     ecx
    cmp     ecx, 7
    jb      .vg
    mov     eax, -5
    vmovd   xmm0, eax
    vpbroadcastd ymm0, xmm0
    vpaddd  ymm0, ymm0, [pl_lane8]
    vcvtdq2ps ymm3, ymm0
    vaddps  ymm3, ymm3, ymm2
    vsubps  ymm3, ymm3, ymm1
    vmovups [rsp + PL_PV], ymm3         ; столб: x + 0.5 − FX, x = −5 .. 2
    vpaddd  ymm0, ymm0, [pl_d8]
    vcvtdq2ps ymm3, ymm0
    vaddps  ymm3, ymm3, ymm2
    vsubps  ymm3, ymm3, ymm1
    vmovups [rsp + PL_PV + 32], ymm3    ; 3 .. 10
    vzeroupper
.nv:
    ; ----- столб: строка h над лампой
    mov     dword [rsp + PL_E], __float32__(1.0)
    mov     r12d, 1
.ph:
    movss   xmm0, [rsp + PL_E]
    mulss   xmm0, [pl_decay]
    movss   [rsp + PL_E], xmm0
    mov     edi, [rsp + PL_IY]
    sub     edi, r12d                   ; строка
    js      .halo
    cmp     edi, SCR_H
    jae     .phn
    cmp     r12d, 4
    jb      .phn
    ; мерцание: два синуса по высоте, бегут медленно (xorps — cvtsi2ss не ждёт прошлое)
    xorps   xmm0, xmm0
    cvtsi2ss xmm0, r12d
    mulss   xmm0, [pl_f1]
    movss   xmm1, [wind_t]
    mulss   xmm1, [pl_w1]
    addss   xmm0, xmm1
    call    fsin
    movss   [rsp + PL_A], xmm0
    xorps   xmm0, xmm0
    cvtsi2ss xmm0, r12d
    mulss   xmm0, [pl_f2]
    movss   xmm1, [wind_t]
    mulss   xmm1, [pl_w2]
    subss   xmm0, xmm1
    call    fsin
    addss   xmm0, [rsp + PL_A]
    mulss   xmm0, [pl_sh]
    addss   xmm0, [pl_s0]
    mulss   xmm0, [rsp + PL_E]
    lea     eax, [r12d - 3]
    xorps   xmm1, xmm1
    cvtsi2ss xmm1, eax
    mulss   xmm1, [f_0_1]
    minss   xmm1, [f_1]
    mulss   xmm0, xmm1
    mov     eax, PL_H                   ; верхние 40 px — на нет
    sub     eax, r12d
    xorps   xmm1, xmm1
    cvtsi2ss xmm1, eax
    mulss   xmm1, [pl_itop]
    minss   xmm1, [f_1]
    mulss   xmm0, xmm1
    movss   [rsp + PL_A], xmm0
    ; ширина: 0.9 + 0.012·h px, профиль (1 − u²)²
    xorps   xmm0, xmm0
    cvtsi2ss xmm0, r12d
    mulss   xmm0, [pl_wk]
    addss   xmm0, [pl_w0]
    movss   xmm1, [f_1]
    divss   xmm1, xmm0
    movss   [rsp + PL_IW], xmm1
    imul    edi, edi, SCR_W
    cmp     dword [rsp + PL_VOK], 0
    je      .ps
    ; строка столба: пиксели x − 5 .. x + 10 (две группы, лишние дорожки — мимо)
    PLCONST
    vbroadcastss ymm8, [rsp + PL_IW]
    vbroadcastss ymm9, [rsp + PL_A]
    mov     eax, [rsp + PL_IX]
    lea     edi, [rdi + rax - 5]
    PLCOL   0, 0
    PLCOL   1, 1
    vzeroupper
    jmp     .phn
.ps:
    mov     r13d, -5
.px:
    mov     eax, [rsp + PL_IX]
    add     eax, r13d
    cmp     eax, SCR_W
    jae     .pxn
    cvtsi2ss xmm0, r13d
    addss   xmm0, [f_0_5]
    subss   xmm0, [rsp + PL_FX]
    mulss   xmm0, [rsp + PL_IW]
    mulss   xmm0, xmm0
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    comiss  xmm1, [f_0]
    jbe     .pxn
    mulss   xmm1, xmm1
    mulss   xmm1, [rsp + PL_A]
    add     eax, edi
    call    .add
.pxn:
    inc     r13d
    cmp     r13d, 5
    jle     .px
.phn:
    inc     r12d
    cmp     r12d, PL_H
    jb      .ph
    ; ----- ореол: мягкий диск (R 15) и бледное кольцо (r 21)
.halo:
    mov     r12d, -25
    cmp     dword [rsp + PL_VOK], 0
    je      .hy
    ; по 8 пикселей: x − 25 .. x + 30, семь групп (у последней три дорожки)
    PLCONST
.vhy:
    mov     edi, [rsp + PL_IY]
    add     edi, r12d
    cmp     edi, SCR_H
    jae     .vhyn
    imul    edi, edi, SCR_W
    add     edi, [rsp + PL_IX]
    sub     edi, 25
    vxorps  xmm9, xmm9, xmm9
    vcvtsi2ss xmm9, xmm9, r12d
    vmulss  xmm9, xmm9, xmm9
    vbroadcastss ymm9, xmm9             ; dy²
    PLHALO  0, 0
    PLHALO  1, 0
    PLHALO  2, 0
    PLHALO  3, 0
    PLHALO  4, 0
    PLHALO  5, 0
    PLHALO  6, 1
.vhyn:
    inc     r12d
    cmp     r12d, 25
    jle     .vhy
    vzeroupper
    jmp     .n
.hy:
    mov     edi, [rsp + PL_IY]
    add     edi, r12d
    cmp     edi, SCR_H
    jae     .hyn
    imul    edi, edi, SCR_W
    mov     r13d, -25
.hx:
    mov     eax, [rsp + PL_IX]
    add     eax, r13d
    cmp     eax, SCR_W
    jae     .hxn
    xorps   xmm0, xmm0
    cvtsi2ss xmm0, r13d
    addss   xmm0, [f_0_5]
    subss   xmm0, [rsp + PL_FX]
    mulss   xmm0, xmm0
    xorps   xmm2, xmm2
    cvtsi2ss xmm2, r12d
    mulss   xmm2, xmm2
    addss   xmm0, xmm2                  ; r²
    comiss  xmm0, [pl_rout]             ; дальше кольца — пусто
    jae     .hxn
    comiss  xmm0, [pl_rdisk]            ; между диском и кольцом — пусто
    jb      .hin
    comiss  xmm0, [pl_rin]
    jb      .hxn
.hin:
    movaps  xmm3, xmm0
    mulss   xmm0, [pl_ig2]
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    maxss   xmm1, [f_0]
    mulss   xmm1, xmm1
    mulss   xmm1, [pl_ag]               ; диск
    sqrtss  xmm3, xmm3
    subss   xmm3, [pl_r0]
    mulss   xmm3, [pl_irw]
    mulss   xmm3, xmm3
    movss   xmm2, [f_1]
    subss   xmm2, xmm3
    maxss   xmm2, [f_0]
    mulss   xmm2, xmm2
    mulss   xmm2, [pl_ar]               ; кольцо
    addss   xmm1, xmm2
    comiss  xmm1, [pl_eps]
    jb      .hxn
    add     eax, edi
    call    .add
.hxn:
    inc     r13d
    cmp     r13d, 25
    jle     .hx
.hyn:
    inc     r12d
    cmp     r12d, 25
    jle     .hy
.n:
    add     rbx, L_SZ
    dec     r15d
    jmp     .l
.out:
    add     rsp, PL_SZ
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
; пиксель eax += цвет·xmm1, если ничто не ближе к глазу (портит ecx, edx, xmm0, xmm1)
.add:
    mov     ecx, [rsi + rax*4]
    movzx   edx, cx
    shr     ecx, 16
    add     ecx, edx
    cmp     ecx, [rsp + 8 + PL_KEY]
    jg      .ar
    shufps  xmm1, xmm1, 0
    mulps   xmm1, [rsp + 8 + PL_COL]
    cvttps2dq xmm1, xmm1
    packssdw xmm1, xmm1
    packuswb xmm1, xmm1
    movd    xmm0, [r14 + rax*4]
    paddusb xmm0, xmm1
    movd    [r14 + rax*4], xmm0
.ar:
    ret

section .rdata
align 4, db 0
pl_gain:    dd 0.95                     ; цвет фонаря (0..190) -> прибавка к кадру
pl_itop:    dd 0.025                    ; 1/40
pl_z:       dd 40.0                     ; стекло над снегом, px
pl_xm0:     dd -40.0                    ; лампа на экране: с запасом на ореол и столб
pl_xm1:     dd 680.0
pl_ym0:     dd -30.0
pl_ym1:     dd 510.0
pl_decay:   dd 0.98895                  ; e^(−1/90): к верху тает
pl_f1:      dd 0.31                     ; мерцание по высоте
pl_w1:      dd 0.9
pl_f2:      dd 0.117
pl_w2:      dd 0.53
pl_sh:      dd 0.11
pl_s0:      dd 0.78
pl_w0:      dd 1.7                      ; полуширина: у лампы, прирост на px высоты
pl_wk:      dd 0.016
pl_ig2:     dd 0.0044444                ; ореол: 1/15²
pl_ag:      dd 0.33
pl_r0:      dd 22.0                     ; кольцо: радиус, 1/полуширина, сила
pl_irw:     dd 0.35
pl_ar:      dd 0.045
pl_eps:     dd 0.004
pl_rdisk:   dd 225.0                    ; 15²
pl_rin:     dd 366.0                    ; (22 − 2.86)²
pl_rout:    dd 618.0                    ; (22 + 2.86)²
; по 8 дорожек (те же числа)
align 32, db 0
pl_lane8:   dd 0, 1, 2, 3, 4, 5, 6, 7
pl_d8:      times 8 dd 8
pl_g6m:     dd -1, -1, -1, 0, 0, 0, 0, 0    ; у последней группы — три дорожки
pl_tr:      times 2 db 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15
pl_ig2v:    times 8 dd 0.0044444
pl_agv:     times 8 dd 0.33
pl_r0v:     times 8 dd 22.0
pl_irwv:    times 8 dd 0.35
pl_arv:     times 8 dd 0.045
pl_epsv:    times 8 dd 0.004
pl_rdiskv:  times 8 dd 225.0
pl_rinv:    times 8 dd 366.0
pl_routv:   times 8 dd 618.0

section .text

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
    ; к изоляторам на столбе (основание модели — 154/256 тайла)
    movzx   eax, byte [rbx]
    shl     eax, 8
    add     eax, 154
    mov     [dw_ax], eax
    movzx   eax, byte [rbx + 1]
    shl     eax, 8
    add     eax, 154
    mov     [dw_ay], eax
    movzx   eax, byte [rbx + 2]
    shl     eax, 8
    add     eax, 154
    mov     [dw_bx], eax
    movzx   eax, byte [rbx + 3]
    shl     eax, 8
    add     eax, 154
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
    xorps   xmm0, xmm0                  ; xorps: cvtsi2ss не ждёт прошлый шаг
    cvtsi2ss xmm0, ebp
    mulss   xmm0, [dw_dt]               ; t
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    mulss   xmm1, xmm0
    mulss   xmm1, [f_4]                 ; 4t(1−t)
    mov     eax, [dw_bx]
    sub     eax, [dw_ax]
    xorps   xmm2, xmm2
    cvtsi2ss xmm2, eax
    mulss   xmm2, xmm0
    movss   xmm3, [dw_px]
    mulss   xmm3, xmm1
    addss   xmm2, xmm3
    cvtss2si esi, xmm2
    add     esi, [dw_ax]                ; wx
    mov     eax, [dw_by]
    sub     eax, [dw_ay]
    xorps   xmm2, xmm2                  ; y не ждёт цепочку x
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
    sub     edx, 41
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
    mov     r8d, [wr_alb]               ; в инее — белее (rime_step)
    mov     [rcx + rax*4], r8d
    lea     rcx, [nbuf]
    mov     byte [rcx + rax], N_UP
    mov     ecx, edi
    shl     ecx, 16
    or      ecx, esi
    mov     [rdx + rax*4], ecx
    ; густой иней — провод толще: ещё пиксель сверху
    cmp     dword [rime_t], 60
    jb      .sn
    cmp     eax, SCR_W
    jb      .sn
    sub     eax, SCR_W
    mov     r9d, [rdx + rax*4]
    movzx   r8d, r9w
    shr     r9d, 16
    add     r9d, r8d
    lea     r8d, [esi + edi + 64]
    cmp     r9d, r8d
    jg      .sn
    mov     [rdx + rax*4], ecx
    lea     r9, [albedo]
    mov     r8d, [wr_alb]
    mov     [r9 + rax*4], r8d
    lea     r9, [nbuf]
    mov     byte [r9 + rax], N_UP
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
