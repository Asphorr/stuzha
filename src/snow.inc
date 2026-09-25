; ----------------------------------------------------------------------------
;  snow.inc — рельеф снега (8 ячеек на тайл): сугробы у стен и заборов (с подветренной
;  стороны выше), гребень под капелью — только вдоль скатов крыши, валы по краям
;  дороги и колеи на ней, лунки у елей, заструги поперёк ветра и пологие волны.
;  drift: байт 0 — высота (1/8 px), байты 1-2 — наклон нормали (x, y) со знаком
; ----------------------------------------------------------------------------
section .text

; xmm6 = px, xmm7 = py (тайлы), ecx = tx, edx = ty -> xmm0 = расстояние до квадрата тайла
dr_rect:
    cvtsi2ss xmm0, ecx
    movaps  xmm1, xmm0
    addss   xmm1, [f_1]
    subss   xmm0, xmm6
    movaps  xmm2, xmm6
    subss   xmm2, xmm1
    maxss   xmm0, xmm2
    maxss   xmm0, [f_0]
    cvtsi2ss xmm1, edx
    movaps  xmm2, xmm1
    addss   xmm2, [f_1]
    subss   xmm1, xmm7
    movaps  xmm3, xmm7
    subss   xmm3, xmm2
    maxss   xmm1, xmm3
    maxss   xmm1, [f_0]
    mulss   xmm0, xmm0
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    sqrtss  xmm0, xmm0
    ret

; eax = x, ecx = y (тексели) -> eax = шум облаков (портит edx)
dr_tex:
    and     eax, 63
    and     ecx, 63
    shl     ecx, 6
    add     eax, ecx
    lea     rdx, [clouds]
    movzx   eax, byte [rdx + rax]
    ret

; r12d = cx, r13d = cy -> eax = билинейный шум (8 ячеек на тексель), 0..255·64
dr_wave:
    push    rbx
    push    rsi
    push    rdi
    mov     ebx, r12d
    shr     ebx, 3
    mov     esi, r13d
    shr     esi, 3
    mov     eax, ebx
    mov     ecx, esi
    call    dr_tex
    mov     edi, eax                    ; n00
    lea     eax, [ebx + 1]
    mov     ecx, esi
    call    dr_tex
    mov     r8d, eax                    ; n10
    mov     eax, ebx
    lea     ecx, [esi + 1]
    call    dr_tex
    mov     r9d, eax                    ; n01
    lea     eax, [ebx + 1]
    lea     ecx, [esi + 1]
    call    dr_tex                      ; n11
    mov     ecx, r12d
    and     ecx, 7                      ; fx
    mov     edx, r13d
    and     edx, 7                      ; fy
    ; строка y0: n00 + (n10 − n00)·fx/8, строка y1 — так же
    sub     r8d, edi
    imul    r8d, ecx
    shl     edi, 3
    add     edi, r8d                    ; ·8
    sub     eax, r9d
    imul    eax, ecx
    shl     r9d, 3
    add     r9d, eax                    ; ·8
    sub     r9d, edi
    imul    r9d, edx
    shl     edi, 3
    lea     eax, [edi + r9d]            ; ·64
    pop     rdi
    pop     rsi
    pop     rbx
    ret

gen_drifts:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    ; ----- 1) высоты, 1/16 px, int16 во временном буфере (tracks + blood)
    xor     r13d, r13d                  ; cy
.cy:
    xor     r12d, r12d                  ; cx
.cx:
    cvtsi2ss xmm6, r12d
    addss   xmm6, [f_0_5]
    mulss   xmm6, [f_0_125]             ; px
    cvtsi2ss xmm7, r13d
    addss   xmm7, [f_0_5]
    mulss   xmm7, [f_0_125]             ; py
    mov     ebx, r12d
    shr     ebx, 3                      ; tx
    mov     esi, r13d
    shr     esi, 3                      ; ty
    mov     eax, esi
    shl     eax, 7
    add     eax, ebx
    lea     rdx, [tmap]
    movzx   eax, byte [rdx + rax]
    lea     rdx, [tprop]
    movzx   eax, byte [rdx + rax]
    shr     eax, 4                      ; пол: 0 снег, 1 дорога, 2 доски
    xorps   xmm8, xmm8                  ; высота
    cmp     eax, 2
    je      .store
    mov     r15d, eax
    ; пологие волны ±1.2 px
    call    dr_wave
    sub     eax, 128*64
    imul    eax, eax, 5
    sar     eax, 11
    cvtsi2ss xmm8, eax
    cmp     r15d, 1
    je      .road
    ; заструги: гребни поперёк ветра (ветер к +x −y), пологий наветренный склон,
    ; крутой подветренный; пятнами
    mov     eax, r12d
    shr     eax, 1
    mov     ecx, r13d
    shr     ecx, 1
    add     eax, 17
    call    dr_tex                      ; изгиб гребней
    mov     r14d, r12d
    sub     r14d, r13d
    imul    r14d, r14d, 30
    add     r14d, eax
    and     r14d, 255
    cmp     r14d, 192
    jae     .sd
    imul    r14d, r14d, 14
    shr     r14d, 6                     ; 0..42 -> /3 ниже
    jmp     .sa
.sd:
    neg     r14d
    add     r14d, 255
    imul    r14d, r14d, 42
    shr     r14d, 6
.sa:
    mov     eax, r12d
    shr     eax, 4
    add     eax, 32
    mov     ecx, r13d
    shr     ecx, 4
    add     ecx, 11
    call    dr_tex                      ; где заструги есть
    sub     eax, 110
    jle     .nsa
    imul    r14d, eax
    imul    r14d, r14d, 3
    shr     r14d, 10                    ; до ~1 px
    cvtsi2ss xmm0, r14d
    addss   xmm8, xmm0
.nsa:
    ; соседние тайлы: сугробы, валы, лунки
    xorps   xmm9, xmm9                  ; наибольший сугроб
    xorps   xmm10, xmm10                ; наиболее глубокая лунка
    mov     edi, -1                     ; dy
.ny:
    mov     ebp, -1                     ; dx
.nx:
    lea     ecx, [ebx + ebp]
    lea     edx, [esi + edi]
    cmp     ecx, MAPW
    jae     .nn
    cmp     edx, MAPW
    jae     .nn
    mov     eax, edx
    shl     eax, 7
    add     eax, ecx
    mov     r14d, eax                   ; индекс тайла
    lea     r8, [tmap]
    movzx   eax, byte [r8 + rax]
    lea     r8, [dr_kind]
    movzx   eax, byte [r8 + rax]
    test    eax, eax
    jz      .nn
    mov     r15d, eax
    call    dr_rect                     ; xmm0 = d
    cmp     r15d, 4
    jae     .tree
    ; сугроб у препятствия: H·(1 − d/r)², с подветренной стороны выше
    lea     r8, [dr_h]
    movss   xmm4, [r8 + r15*4]          ; H
    lea     r8, [dr_r]
    movss   xmm5, [r8 + r15*4]          ; r
    movaps  xmm1, xmm0
    divss   xmm1, xmm5
    movss   xmm2, [f_1]
    subss   xmm2, xmm1
    comiss  xmm2, [f_0]
    jbe     .eave
    mulss   xmm2, xmm2
    mulss   xmm2, xmm4
    ; от препятствия к ячейке (−dx, −dy), ветер к (+x, −y): подветренная сторона выше
    cvtsi2ss xmm1, edi
    cvtsi2ss xmm3, ebp
    subss   xmm1, xmm3
    mulss   xmm1, [f_0_3]
    addss   xmm1, [f_1]
    mulss   xmm2, xmm1
    cmp     r15d, 1
    jne     .dmax
    ; стена избы: вдоль скатов под свесом снега меньше, по линии капели — гребень
    lea     r8, [rmap]
    movzx   eax, byte [r8 + r14]
    test    eax, eax
    jz      .dmax
    call    dr_eave
    jmp     .nn
.dmax:
    maxss   xmm9, xmm2
    jmp     .nn
.eave:
    cmp     r15d, 1
    jne     .nn
    lea     r8, [rmap]
    movzx   eax, byte [r8 + r14]
    test    eax, eax
    jz      .nn
    xorps   xmm2, xmm2
    call    dr_eave
    jmp     .nn
.tree:
    cmp     r15d, 6
    je      .berm
    ; лунка у ствола: снег вокруг ели не держится (у основания растения, veg.inc)
    call    veg_base
    subss   xmm0, xmm6
    mulss   xmm0, xmm0
    subss   xmm1, xmm7
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    sqrtss  xmm0, xmm0
    movss   xmm1, [f_0_6]
    cmp     r15d, 4
    je      .tw
    movss   xmm1, [f_0_4]
.tw:
    divss   xmm0, xmm1
    movss   xmm2, [f_1]
    subss   xmm2, xmm0
    maxss   xmm2, [f_0]
    mulss   xmm2, [f_m40]
    minss   xmm10, xmm2
    jmp     .nn
.berm:
    ; вал от грейдера вдоль дороги
    movss   xmm1, xmm0
    divss   xmm1, [f_0_7]
    movss   xmm2, [f_1]
    subss   xmm2, xmm1
    comiss  xmm2, [f_0]
    jbe     .nn
    mulss   xmm2, xmm2
    mulss   xmm2, [f_56]
    maxss   xmm9, xmm2
.nn:
    inc     ebp
    cmp     ebp, 1
    jle     .nx
    inc     edi
    cmp     edi, 1
    jle     .ny
    addss   xmm8, xmm9
    addss   xmm8, xmm10
    jmp     .store
.road:
    ; укатанная дорога: две колеи и гребень между ними; волны вполовину
    mulss   xmm8, [f_0_5]
    mov     eax, esi
    sub     eax, 60
    cmp     eax, 2
    ja      .rv
    movss   xmm0, xmm7
    subss   xmm0, [f_61_5]
    call    dr_ruts
.rv:
    cmp     ebx, 36
    jb      .rv2
    cmp     ebx, 38
    ja      .rv2
    movss   xmm0, xmm6
    subss   xmm0, [f_37_5]
    call    dr_ruts
.rv2:
    cmp     ebx, 90
    jb      .store
    cmp     ebx, 92
    ja      .store
    movss   xmm0, xmm6
    subss   xmm0, [f_91_5]
    call    dr_ruts
.store:
    cvtss2si eax, xmm8
    mov     ecx, -2047
    cmp     eax, ecx
    cmovl   eax, ecx
    mov     ecx, 2047
    cmp     eax, ecx
    cmovg   eax, ecx
    mov     ecx, r13d
    shl     ecx, 10
    add     ecx, r12d
    lea     rdx, [tracks]
    mov     [rdx + rcx*2], ax
    inc     r12d
    cmp     r12d, DECW
    jb      .cx
    inc     r13d
    cmp     r13d, DECW
    jb      .cy
    ; ----- 2) наклоны нормалей и высота
    lea     rsi, [tracks]
    lea     rdi, [drift]
    xor     r13d, r13d
.gy:
    xor     r12d, r12d
.gx:
    mov     eax, r12d
    inc     eax
    cmp     eax, DECW - 1
    jbe     .g1
    mov     eax, DECW - 1
.g1:
    mov     ecx, r12d
    dec     ecx
    jns     .g2
    xor     ecx, ecx
.g2:
    mov     edx, r13d
    shl     edx, 10
    lea     r11, [rsi + rdx*2]
    movsx   r8d, word [r11 + rax*2]
    movsx   r9d, word [r11 + rcx*2]
    sub     r8d, r9d                    ; dh/dx
    mov     eax, r13d
    inc     eax
    cmp     eax, DECW - 1
    jbe     .g3
    mov     eax, DECW - 1
.g3:
    mov     ecx, r13d
    dec     ecx
    jns     .g4
    xor     ecx, ecx
.g4:
    shl     eax, 10
    shl     ecx, 10
    add     eax, r12d
    add     ecx, r12d
    movsx   r9d, word [rsi + rax*2]
    movsx   r10d, word [rsi + rcx*2]
    sub     r9d, r10d                   ; dh/dy
    ; нормаль наклонена против роста высоты; ед. = 0.045 уклона
    imul    r8d, r8d, -73
    sar     r8d, 8
    imul    r9d, r9d, -73
    sar     r9d, 8
    mov     eax, -60
    cmp     r8d, eax
    cmovl   r8d, eax
    cmp     r9d, eax
    cmovl   r9d, eax
    mov     eax, 60
    cmp     r8d, eax
    cmovg   r8d, eax
    cmp     r9d, eax
    cmovg   r9d, eax
    ; готовая нормаль — для клеток без следов
    mov     eax, r8d
    mov     ecx, r9d
    mov     edx, -8
    cmp     eax, edx
    cmovl   eax, edx
    cmp     ecx, edx
    cmovl   ecx, edx
    mov     edx, 7
    cmp     eax, edx
    cmovg   eax, edx
    cmp     ecx, edx
    cmovg   ecx, edx
    add     eax, 8
    add     ecx, 8
    lea     rdx, [q5]
    movzx   eax, byte [rdx + rax]
    movzx   ecx, byte [rdx + rcx]
    lea     ecx, [ecx + ecx*4]
    add     eax, ecx
    lea     rdx, [gn_lut]
    movzx   r10d, byte [rdx + rax]
    shl     r10d, 24
    mov     eax, r13d
    shl     eax, 10
    add     eax, r12d
    movsx   ecx, word [rsi + rax*2]
    sar     ecx, 1                      ; 1/8 px
    xor     edx, edx
    cmp     ecx, edx
    cmovl   ecx, edx
    mov     edx, 255
    cmp     ecx, edx
    cmovg   ecx, edx
    and     r8d, 255
    shl     r8d, 8
    or      ecx, r8d
    and     r9d, 255
    shl     r9d, 16
    or      ecx, r9d
    or      ecx, r10d                   ; байт 3 — нормаль без следов
    mov     [rdi + rax*4], ecx
    inc     r12d
    cmp     r12d, DECW
    jb      .gx
    inc     r13d
    cmp     r13d, DECW
    jb      .gy
    ; временный буфер больше не нужен
    lea     rdi, [tracks]
    xor     eax, eax
    mov     ecx, DECW*DECW*2
    rep     stosb
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

; xmm0 = от оси дороги, тайлы -> xmm8 += колеи (−1.6 px) и гребень между ними
dr_ruts:
    andps   xmm0, [f_absmask]
    movaps  xmm1, xmm0
    subss   xmm1, [f_0_55]
    andps   xmm1, [f_absmask]
    mulss   xmm1, [f_5]
    movss   xmm2, [f_1]
    subss   xmm2, xmm1
    maxss   xmm2, [f_0]
    mulss   xmm2, [f_m26]
    addss   xmm8, xmm2
    movaps  xmm1, xmm0
    mulss   xmm1, [f_3_3]
    movss   xmm2, [f_1]
    subss   xmm2, xmm1
    maxss   xmm2, [f_0]
    mulss   xmm2, [f_10]
    addss   xmm8, xmm2
    ret

; стена избы eax = номер + 1, xmm0 = d, xmm2 = обычный сугроб -> xmm9 = max(…)
; у стен под скатом: сугроб втрое ниже (свес укрывает), на линии капели — гребень 4 px
dr_eave:
    dec     eax
    imul    eax, eax, HS_SZ
    lea     r8, [houses]
    add     r8, rax
    ; ячейка напротив ската? (снаружи поперёк конька, в пределах длины)
    cmp     dword [r8 + HS_AX], 0
    jne     .ay
    cvtsi2ss xmm1, dword [r8 + HS_X0]
    comiss  xmm6, xmm1
    jb      .gab
    cvtsi2ss xmm3, dword [r8 + HS_W]
    addss   xmm1, xmm3
    comiss  xmm6, xmm1
    jae     .gab
    jmp     .side
.ay:
    cvtsi2ss xmm1, dword [r8 + HS_Y0]
    comiss  xmm7, xmm1
    jb      .gab
    cvtsi2ss xmm3, dword [r8 + HS_H]
    addss   xmm1, xmm3
    comiss  xmm7, xmm1
    jae     .gab
.side:
    mulss   xmm2, [f_0_33]
    maxss   xmm9, xmm2
    movaps  xmm1, xmm0
    subss   xmm1, [f_0_3]
    andps   xmm1, [f_absmask]
    mulss   xmm1, [f_6]
    movss   xmm3, [f_1]
    subss   xmm3, xmm1
    maxss   xmm3, [f_0]
    mulss   xmm3, [f_64]
    maxss   xmm9, xmm3
    ret
.gab:
    maxss   xmm9, xmm2
    ret
