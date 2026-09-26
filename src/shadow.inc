; ----------------------------------------------------------------------------
;  shadow.inc — высоты и AO, развёртка теней луны, зомби в луче фонарика
;               (тени от огней — карты горизонта в lights.inc)
; ----------------------------------------------------------------------------
section .text

; ---------------------------------------------------------------- высоты
; hmap8: 8 ячеек на тайл, по краям HPAD нулей; биты 0-6 — высота заслонителя (px),
; бит 7 — под крышей. Формы — с картинок: избы — скаты и трубы (roof_zs); забор —
; доска по кромке тайла, как он рисуется; ели, берёзы, фонари, кресты, снеговики,
; скворечники — тела вращения по силуэту спрайта (редкая крона — редкие ячейки);
; лавки, будки, колодцы, столбы верёвки — фигуры столкновения своей высоты
calc_hmap:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    call    moon_tabs
    xor     eax, eax
    lea     rdi, [hmap8]
    mov     ecx, HW8*HW8
    rep     stosb
    xor     r13d, r13d                  ; ty
.ty:
    xor     r12d, r12d                  ; tx
.tx:
    mov     r14d, r13d
    shl     r14d, 7
    add     r14d, r12d                  ; тайл
    mov     eax, r13d
    shl     eax, 3
    add     eax, HPAD
    imul    eax, eax, HW8
    lea     eax, [eax + r12d*8 + HPAD]
    lea     rdi, [hmap8]
    add     rdi, rax                    ; ячейка (0, 0) тайла
    lea     rdx, [rmap]
    movzx   eax, byte [rdx + r14]
    test    eax, eax
    jnz     .house
    ; растения и мелочи двора — своими картами высот с моделей (veg_hmap, в конце)
    lea     rdx, [vmap]
    cmp     byte [rdx + r14*2], 0
    jne     .next
    lea     rdx, [tmap]
    movzx   ebx, byte [rdx + r14]       ; вид
    lea     rdx, [th_m]
    mov     eax, 36 | 0x80
    cmp     byte [rdx + rbx], 2
    je      .fill
    mov     eax, 36
    cmp     ebx, T_WALL
    je      .fill
    cmp     ebx, T_WIN
    je      .fill
    cmp     ebx, T_WINL
    je      .fill
    mov     eax, 12
    cmp     ebx, T_PILE
    je      .fill
    cmp     ebx, T_FENCE
    je      .fence
    lea     rdx, [hm_prof]
    movzx   eax, byte [rdx + rbx]
    cmp     eax, 0xFF
    jne     .prof
    lea     rdx, [hm_csh]
    cmp     word [rdx + rbx*2], 0
    jne     .shapes
    jmp     .next

    ; ----- весь тайл одной высоты
.fill:
    xor     ecx, ecx
.f1:
    mov     edx, ecx
    shr     edx, 3
    imul    edx, edx, HW8
    mov     esi, ecx
    and     esi, 7
    add     edx, esi
    mov     [rdi + rdx], al
    inc     ecx
    cmp     ecx, 64
    jb      .f1
    jmp     .next

    ; ----- забор: доска по кромке +y (соседи по x) и/или +x (соседи по y)
.fence:
    lea     rdx, [cmap]
    movzx   ebx, byte [rdx + r14]
    sub     ebx, CS_FY - 1              ; бит 0 — кромка +y, бит 1 — кромка +x
    test    ebx, 1
    jz      .fe2
    xor     ecx, ecx
.fe1:
    mov     byte [rdi + 7*HW8 + rcx], 10
    inc     ecx
    cmp     ecx, 8
    jb      .fe1
.fe2:
    test    ebx, 2
    jz      .next
    xor     ecx, ecx
.fe3:
    imul    edx, ecx, HW8
    mov     byte [rdi + rdx + 7], 10
    inc     ecx
    cmp     ecx, 8
    jb      .fe3
    jmp     .next

    ; ----- тело вращения: высота и плотность по расстоянию от основания (0.6, 0.6)
.prof:
    shl     eax, 6
    lea     rbp, [prof]
    add     rbp, rax
    xor     ecx, ecx
.p1:
    mov     eax, ecx
    and     eax, 7
    shl     eax, 5
    sub     eax, 154 - 16               ; от основания, 8.8
    cvtsi2ss xmm0, eax
    mulss   xmm0, xmm0
    mov     eax, ecx
    shr     eax, 3
    shl     eax, 5
    sub     eax, 154 - 16
    cvtsi2ss xmm1, eax
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    sqrtss  xmm0, xmm0
    mulss   xmm0, [f_0_125]             ; 1/32 тайла
    cvttss2si eax, xmm0
    cmp     eax, 32
    jae     .p2
    movzx   r15d, byte [rbp + rax]      ; высота
    test    r15d, r15d
    jz      .p2
    cmp     eax, 1
    jbe     .p3                         ; ствол, столб — всегда
    movzx   esi, byte [rbp + rax + 32]  ; плотность
    mov     eax, r12d
    shl     eax, 3
    mov     edx, ecx
    and     edx, 7
    add     eax, edx
    mov     edx, r13d
    shl     edx, 3
    mov     r8d, ecx
    shr     r8d, 3
    add     edx, r8d
    HASH    eax, edx
    and     eax, 255
    cmp     eax, esi
    jae     .p2
.p3:
    mov     edx, ecx
    shr     edx, 3
    imul    edx, edx, HW8
    mov     eax, ecx
    and     eax, 7
    add     edx, eax
    mov     [rdi + rdx], r15b
.p2:
    inc     ecx
    cmp     ecx, 64
    jb      .p1
    jmp     .next

    ; ----- фигуры столкновения тайла, у каждой своя высота
.shapes:
    lea     rdx, [cmap]
    movzx   eax, byte [rdx + r14]
    imul    eax, eax, CS_SZ
    lea     rbp, [csets]
    add     rbp, rax
    xor     ecx, ecx
.s1:
    mov     eax, ecx
    and     eax, 7
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    mulss   xmm0, [f_0_125]             ; u
    mov     eax, ecx
    shr     eax, 3
    cvtsi2ss xmm1, eax
    addss   xmm1, [f_0_5]
    mulss   xmm1, [f_0_125]             ; v
    xor     r15d, r15d                  ; высота
    mov     esi, [rbp]                  ; фигур
    lea     r8, [rbp + 4]
    lea     rdx, [hm_csh]
    lea     r9, [rdx + rbx*2]
.s2:
    test    esi, esi
    jz      .s4
    movss   xmm2, [r8]
    subss   xmm2, xmm0
    movaps  xmm3, xmm0
    subss   xmm3, [r8 + 8]
    maxss   xmm2, xmm3
    maxss   xmm2, [f_0]                 ; до прямоугольника по x
    movss   xmm3, [r8 + 4]
    subss   xmm3, xmm1
    movaps  xmm4, xmm1
    subss   xmm4, [r8 + 12]
    maxss   xmm3, xmm4
    maxss   xmm3, [f_0]
    mulss   xmm2, xmm2
    mulss   xmm3, xmm3
    addss   xmm2, xmm3
    movss   xmm3, [r8 + 16]
    addss   xmm3, [f_0_04]              ; ячейка задевает фигуру краем
    mulss   xmm3, xmm3
    comiss  xmm2, xmm3
    ja      .s3
    movzx   eax, byte [r9]
    cmp     r15d, eax
    cmovb   r15d, eax
.s3:
    add     r8, 20
    inc     r9
    dec     esi
    jmp     .s2
.s4:
    test    r15d, r15d
    jz      .s5
    mov     edx, ecx
    shr     edx, 3
    imul    edx, edx, HW8
    mov     eax, ecx
    and     eax, 7
    add     edx, eax
    mov     [rdi + rdx], r15b
.s5:
    inc     ecx
    cmp     ecx, 64
    jb      .s1
    jmp     .next

    ; ----- изба: скаты и труба по центрам ячеек
.house:
    dec     eax
    imul    eax, eax, HS_SZ
    lea     r10, [houses]
    add     r10, rax
    lea     rdx, [tmap]
    movzx   ebx, byte [rdx + r14]
    lea     rdx, [th_m]
    movzx   ebx, byte [rdx + rbx]       ; 2 — под крышей
    xor     r15d, r15d                  ; ячейка
.h1:
    mov     esi, r15d
    and     esi, 7
    shl     esi, 5
    mov     eax, r12d
    shl     eax, 8
    lea     esi, [esi + eax + 16]       ; wx
    mov     ebp, r15d
    shr     ebp, 3
    shl     ebp, 5
    mov     eax, r13d
    shl     eax, 8
    lea     ebp, [ebp + eax + 16]       ; wy
    mov     eax, [r10 + HS_SX]
    shl     eax, 8
    add     eax, 128
    sub     eax, esi
    cdq
    xor     eax, edx
    sub     eax, edx
    cmp     eax, RCH
    jg      .hroof
    mov     eax, [r10 + HS_SY]
    shl     eax, 8
    add     eax, 128
    sub     eax, ebp
    cdq
    xor     eax, edx
    sub     eax, edx
    cmp     eax, RCH
    jg      .hroof
    mov     eax, [r10 + HS_ZT]
    jmp     .hcl
.hroof:
    mov     ecx, esi
    mov     edx, ebp
    call    roof_zs
.hcl:
    xor     ecx, ecx
    cmp     eax, ecx
    cmovl   eax, ecx
    mov     ecx, 127
    cmp     eax, ecx
    cmovg   eax, ecx
    cmp     ebx, 2
    jne     .h2
    or      eax, 0x80
.h2:
    mov     edx, r15d
    shr     edx, 3
    imul    edx, edx, HW8
    mov     ecx, r15d
    and     ecx, 7
    add     edx, ecx
    mov     [rdi + rdx], al
    inc     r15d
    cmp     r15d, 64
    jb      .h1

.next:
    inc     r12d
    cmp     r12d, MAPW
    jb      .tx
    inc     r13d
    cmp     r13d, MAPW
    jb      .ty
    call    calc_hm16                   ; вдвое мельче — огням и фонарику (hz16.inc)
    call    veg_hmap                    ; растения — картами высот своих моделей
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

; таблицы теней луны: веса выборки 2x2, тела вращения спрайтов
moon_tabs:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40
    lea     rdi, [wt8x]
    lea     rsi, [wt8y]
    xor     ecx, ecx
.w:
    mov     eax, 32
    sub     eax, ecx
    mov     [rdi + rcx*8], ax
    mov     [rdi + rcx*8 + 2], cx
    mov     [rdi + rcx*8 + 4], ax
    mov     [rdi + rcx*8 + 6], cx
    mov     [rsi + rcx*8], ax
    mov     [rsi + rcx*8 + 2], ax
    mov     [rsi + rcx*8 + 4], cx
    mov     [rsi + rcx*8 + 6], cx
    inc     ecx
    cmp     ecx, 32
    jb      .w
    ; силуэты спрайтов -> тела вращения
    xor     r12d, r12d
.p:
    lea     rax, [pf_spt]
    movzx   eax, byte [rax + r12]
    shl     eax, 5
    lea     rbx, [sprtab]
    add     rbx, rax
    lea     rdi, [prof]
    mov     eax, r12d
    shl     eax, 6
    add     rdi, rax
    lea     rax, [pf_gain]
    movzx   r10d, byte [rax + r12]
    mov     rsi, [rbx + SPT_PTR]
    mov     r8d, [rbx + SPT_WH]
    mov     r9d, r8d
    shr     r9d, 8
    and     r9d, 255
    and     r8d, 255
    call    build_prof
    inc     r12d
    cmp     r12d, 6
    jb      .p
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; rsi = пиксели, r8d = w, r9d = h, rdi -> 32 высоты + 32 плотности, r10d = усиление (x64)
; строка спрайта на высоте z — круг радиуса полширины (1 px вбок = 1/22.6 тайла):
; высота на радиусе r — верх самой высокой строки не уже r, плотность — самой густой
build_prof:
    push    rbx
    push    r12
    push    r13
    push    r14
    xor     eax, eax
    mov     [rdi], rax
    mov     [rdi + 8], rax
    mov     [rdi + 16], rax
    mov     [rdi + 24], rax
    mov     [rdi + 32], rax
    mov     [rdi + 40], rax
    mov     [rdi + 48], rax
    mov     [rdi + 56], rax
    xor     r11d, r11d                  ; y
.y:
    mov     r12d, -1                    ; левый
    mov     r13d, -1                    ; правый
    xor     r14d, r14d                  ; непрозрачных
    xor     ecx, ecx
.x:
    mov     eax, r11d
    imul    eax, r8d
    add     eax, ecx
    test    dword [rsi + rax*4], 0xFF000000
    jz      .xn
    inc     r14d
    cmp     r12d, 0
    jge     .xr
    mov     r12d, ecx
.xr:
    mov     r13d, ecx
.xn:
    inc     ecx
    cmp     ecx, r8d
    jb      .x
    test    r14d, r14d
    jz      .yn
    sub     r13d, r12d
    inc     r13d                        ; ширина
    mov     eax, r14d
    imul    eax, eax, 255
    imul    eax, r10d
    xor     edx, edx
    mov     ecx, r13d
    shl     ecx, 6
    div     ecx
    mov     ecx, 255
    cmp     eax, ecx
    cmova   eax, ecx
    mov     ebx, eax                    ; плотность
    imul    r13d, r13d, 181
    shr     r13d, 8                     ; полширины, 1/32 тайла
    mov     ecx, 31
    cmp     r13d, ecx
    cmova   r13d, ecx
    mov     edx, r9d
    sub     edx, r11d                   ; верх строки над основанием, px
    xor     ecx, ecx
.r:
    cmp     [rdi + rcx], dl
    jae     .r1
    mov     [rdi + rcx], dl
.r1:
    cmp     [rdi + rcx + 32], bl
    jae     .r2
    mov     [rdi + rcx + 32], bl
.r2:
    inc     ecx
    cmp     ecx, r13d
    jbe     .r
.yn:
    inc     r11d
    cmp     r11d, r9d
    jb      .y
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret

; ---------------------------------------------------------------- AO
; сэмпл: 1 − Σ w·(1 − d/r)² по 8 соседним тайлам (d — до прямоугольника тайла)
; верх стен — 255, под крышей — 90
calc_ao:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 32
    lea     rdi, [aomap]
    xor     esi, esi                    ; j
.y:
    xor     ebx, ebx                    ; i
.x:
    mov     r12d, ebx
    shr     r12d, 2                     ; tx
    mov     r13d, esi
    shr     r13d, 2                     ; ty
    mov     eax, r13d
    shl     eax, 7
    add     eax, r12d
    lea     rdx, [tmap]
    movzx   eax, byte [rdx + rax]
    lea     rdx, [th_m]
    cmp     byte [rdx + rax], 2
    je      .roofd
    cmp     eax, T_WALL
    je      .full
    cmp     eax, T_WIN
    je      .full
    cmp     eax, T_WINL
    je      .full
    cvtsi2ss xmm6, ebx
    mulss   xmm6, [f_0_25]              ; px
    cvtsi2ss xmm7, esi
    mulss   xmm7, [f_0_25]              ; py
    xorps   xmm8, xmm8                  ; occ
    mov     r15d, -1
.ny:
    mov     r14d, -1
.nx:
    mov     eax, r14d
    or      eax, r15d
    jz      .nn                         ; свой тайл не считаем
    lea     ecx, [r12d + r14d]
    lea     edx, [r13d + r15d]
    call    get_tile
    lea     rdx, [ao_w]
    movzx   ecx, byte [rdx + rax]
    test    ecx, ecx
    jz      .nn
    cvtsi2ss xmm9, ecx
    mulss   xmm9, [f_0_01]              ; w
    lea     rdx, [ao_r]
    movzx   ecx, byte [rdx + rax]
    cvtsi2ss xmm10, ecx
    mulss   xmm10, [f_0_01]             ; r
    lea     eax, [r12d + r14d]
    cvtsi2ss xmm0, eax
    movaps  xmm1, xmm0
    addss   xmm1, [f_1]
    subss   xmm0, xmm6
    movaps  xmm2, xmm6
    subss   xmm2, xmm1
    maxss   xmm0, xmm2
    maxss   xmm0, [f_0]                 ; dx
    lea     eax, [r13d + r15d]
    cvtsi2ss xmm1, eax
    movaps  xmm2, xmm1
    addss   xmm2, [f_1]
    subss   xmm1, xmm7
    movaps  xmm3, xmm7
    subss   xmm3, xmm2
    maxss   xmm1, xmm3
    maxss   xmm1, [f_0]                 ; dy
    mulss   xmm0, xmm0
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    sqrtss  xmm0, xmm0
    divss   xmm0, xmm10
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    maxss   xmm1, [f_0]
    mulss   xmm1, xmm1
    mulss   xmm1, xmm9
    addss   xmm8, xmm1
.nn:
    inc     r14d
    cmp     r14d, 1
    jle     .nx
    inc     r15d
    cmp     r15d, 1
    jle     .ny
    minss   xmm8, [f_0_7]
    movss   xmm0, [f_1]
    subss   xmm0, xmm8
    mulss   xmm0, [f_255]
    cvttss2si eax, xmm0
    jmp     .put
.roofd:
    mov     eax, 90
    jmp     .put
.full:
    mov     eax, 255
.put:
    mov     [rdi], al
    inc     rdi
    inc     ebx
    cmp     ebx, SLW
    jb      .x
    inc     esi
    cmp     esi, SLW
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

; тело развёртки: 4 ячейки столбца подряд; %1 = 1 — и ячейки в mz8 (горизонт+64,
; облака, 4096/полутень), %2 = 1 — поперёк по x (подряд в памяти), 0 — по y
%macro SWEEP4 2
%%l:
    movups  xmm0, [rsi + r14*4]
    movups  xmm1, [rsi + r14*4 + 4]
    mulps   xmm0, xmm6
    mulps   xmm1, xmm7
    addps   xmm0, xmm1
    addps   xmm0, xmm5                  ; hz — луч от соседей ближе к луне
    movups  xmm2, [rsi + r14*4 + SWD]
    movups  xmm1, [rsi + r14*4 + SWD + 4]
    mulps   xmm2, xmm6
    mulps   xmm1, xmm7
    addps   xmm2, xmm1
    addps   xmm2, xmm4                  ; D — путь от заслонителя
%if %1
    movaps  xmm1, xmm2
    mulps   xmm1, xmm8
    addps   xmm1, xmm10
    minps   xmm1, xmm11                 ; полутень 2.5 + k·D, не шире 40 px
    rcpps   xmm1, xmm1
    mulps   xmm1, xmm12
    cvtps2dq xmm1, xmm1
    pslld   xmm1, 16
    movaps  xmm3, xmm0
    addps   xmm3, xmm9
    maxps   xmm3, xmm15
    minps   xmm3, xmm13
    cvtps2dq xmm3, xmm3
    por     xmm1, xmm3
    lea     eax, [r14d - MG8]
    shr     eax, 1                      ; сэмпл облаков — на 2 ячейки
  %if %2
    movzx   edx, byte [rcx + rax*4 + 1]
    shl     edx, 8
    movd    xmm3, edx
    movzx   edx, byte [rcx + rax*4 + 5]
  %else
    imul    eax, eax, GW*4
    movzx   edx, byte [rcx + rax + 1]
    shl     edx, 8
    movd    xmm3, edx
    movzx   edx, byte [rcx + rax + GW*4 + 1]
  %endif
    shl     edx, 8
    pinsrw  xmm3, edx, 2
    punpckldq xmm3, xmm3
    por     xmm1, xmm3
  %if %2
    movdqu  [r10], xmm1
    add     r10, 16
  %else
    movd    [r10], xmm1
    pshufd  xmm3, xmm1, 1
    movd    [r10 + SW8*4], xmm3
    pshufd  xmm3, xmm1, 2
    movd    [r10 + 2*SW8*4], xmm3
    pshufd  xmm3, xmm1, 3
    movd    [r10 + 3*SW8*4], xmm3
    add     r10, 4*SW8*4
  %endif
%endif
    ; заслонители 4 ячеек; пустая — «пол» −MFLR
%if %2
    movd    xmm1, [r8]
    punpcklbw xmm1, xmm15
    punpcklwd xmm1, xmm15
    add     r8, 4
%else
    movzx   eax, byte [r8]
    movd    xmm1, eax
    movzx   eax, byte [r8 + HW8]
    pinsrw  xmm1, eax, 2
    movzx   eax, byte [r8 + 2*HW8]
    pinsrw  xmm1, eax, 4
    movzx   eax, byte [r8 + 3*HW8]
    pinsrw  xmm1, eax, 6
    add     r8, 4*HW8
%endif
    pand    xmm1, [d_7f]
    movdqa  xmm3, xmm1
    pcmpeqd xmm3, xmm15
    cvtdq2ps xmm1, xmm1
    andps   xmm3, xmm14
    addps   xmm1, xmm3
    movaps  xmm3, xmm0
    cmpleps xmm3, xmm1                  ; луч не выше заслонителя —
    maxps   xmm0, xmm1                  ; дальше луч с его верха,
    andnps  xmm3, xmm2                  ; путь с нуля
    movaps  [rdi + r14*4], xmm0
    movaps  [rdi + r14*4 + SWD], xmm3
    add     r14d, 4
    cmp     r14d, r15d
    jb      %%l
%endmacro

; ---------------------------------------------------------------- развёртка теней луны
; 8 ячеек на тайл, окно SW8 вокруг сетки света. Идём от края, обращённого к свету;
; в ячейку приходит луч от соседей ближе к свету (линейно поперёк): высота hz и путь D
; от последнего заслонителя. В mz8 — hz+64, облака и 4096/полутень, полутень = 2.5 + k·D px:
; у основания предмета тень резкая, к концу расплывается (луна с ореолом — не точка).
; Дальше по лучу: заслонитель выше — луч с его верха и путь с нуля; над пустым снегом
; луч не опускается ниже −MFLR (иначе поперечная линейка съедала бы узкие тени).
; Тени игрока и зомби — не здесь: капсулами на луче к луне (figsh.inc, dl_row)
sky_sweep:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    ; начало развёртки (ячейки) — в пределах полей карты
    mov     eax, [gox]
    add     eax, eax
    sub     eax, MG8
    mov     ecx, -HPAD
    cmp     eax, ecx
    cmovl   eax, ecx
    mov     ecx, HW8 - HPAD - SW8
    cmp     eax, ecx
    cmovg   eax, ecx
    mov     [m8_ox], eax
    shl     eax, 5
    add     eax, 16
    mov     [mo_x], eax
    mov     eax, [goy]
    add     eax, eax
    sub     eax, MG8
    mov     ecx, -HPAD
    cmp     eax, ecx
    cmovl   eax, ecx
    mov     ecx, HW8 - HPAD - SW8
    cmp     eax, ecx
    cmovg   eax, ecx
    mov     [m8_oy], eax
    shl     eax, 5
    add     eax, 16
    mov     [mo_y], eax
    ; есть ли направленный свет
    mov     dword [moon_on], 0
    movss   xmm0, [moon_vec]
    addss   xmm0, [moon_vec + 8]
    comiss  xmm0, [f_0_5]
    jb      .out
    mov     dword [moon_on], 1
    ; сдвиг выборки по нормали: msd · (sky_dx, sky_dy), 8.8
    lea     rsi, [msd]
    lea     rdi, [msh]
    xor     ecx, ecx
.sh:
    movss   xmm0, [rsi + rcx*4]
    mulss   xmm0, [f_256]
    movaps  xmm1, xmm0
    mulss   xmm0, [sky_dx]
    mulss   xmm1, [sky_dy]
    cvtss2si eax, xmm0
    mov     [rdi + rcx*8], eax
    cvtss2si eax, xmm1
    mov     [rdi + rcx*8 + 4], eax
    inc     ecx
    cmp     ecx, NN
    jb      .sh
    ; рост полутени на px пути: 2α(1 + tg²e), α ≈ 0.04
    movss   xmm0, [sky_tan]
    mulss   xmm0, xmm0
    addss   xmm0, [f_1]
    mulss   xmm0, [f_0_08]
    movss   [mk_k], xmm0
    ; оси: вдоль — большая составляющая направления на луну
    movss   xmm0, [sky_dx]
    movss   xmm1, [sky_dy]
    movaps  xmm2, xmm0
    andps   xmm2, [f_absmask]
    movaps  xmm3, xmm1
    andps   xmm3, [f_absmask]
    mov     ebp, 1
    mov     dword [sw_ymaj], 0
    comiss  xmm2, xmm3
    jb      .ymaj
    ; x вдоль, y поперёк: столбец идёт поперёк строк карты и mz8
    mov     dword [sw_ha], 1
    mov     dword [sw_oa], 4
    mov     dword [sw_op], SW8*4
    mov     dword [sw_ca], 4
    movaps  xmm4, xmm1
    divss   xmm4, xmm2                  ; сдвиг поперёк за шаг
    movss   xmm5, [f_1]
    divss   xmm5, xmm2                  ; длина шага, ячейки
    comiss  xmm0, [f_0]
    jae     .axes
    mov     ebp, -1
    jmp     .axes
.ymaj:
    ; y вдоль, x поперёк: столбец лежит в памяти подряд
    mov     dword [sw_ymaj], 1
    mov     dword [sw_ha], HW8
    mov     dword [sw_oa], SW8*4
    mov     dword [sw_op], 4
    mov     dword [sw_ca], GW*4
    movaps  xmm4, xmm0
    divss   xmm4, xmm3
    movss   xmm5, [f_1]
    divss   xmm5, xmm3
    comiss  xmm1, [f_0]
    jae     .axes
    mov     ebp, -1
.axes:
    mov     [sw_sgn], ebp
    maxss   xmm4, [f_m0_9999]
    minss   xmm4, [f_0_9999]
    ; луч сносит поперёк в одну сторону: с другой стороны окна поля не нужны
    mov     r13d, 16                    ; (n0 + 4)·4: соседи v + n0, v + n0 + 1 (рамка 4)
    mov     dword [sw_v0], MG8
    mov     dword [sw_v1], SW8
    comiss  xmm4, [f_0]
    jae     .fpos
    mov     r13d, 12
    mov     dword [sw_v0], 0
    mov     dword [sw_v1], MG8 + MW8
    addss   xmm4, [f_1]
.fpos:
    movss   xmm6, [f_1]
    subss   xmm6, xmm4
    shufps  xmm6, xmm6, 0               ; w0
    shufps  xmm4, xmm4, 0
    movaps  xmm7, xmm4                  ; w1
    mulss   xmm5, [f_2_45]              ; путь за шаг, px
    movaps  xmm0, xmm5
    mulss   xmm0, [sky_tan]             ; спуск луча за шаг, px
    shufps  xmm5, xmm5, 0
    movaps  xmm4, xmm5                  ; путь
    xorps   xmm5, xmm5
    subss   xmm5, xmm0
    shufps  xmm5, xmm5, 0               ; −спуск
    movss   xmm8, [mk_k]
    shufps  xmm8, xmm8, 0               ; рост полутени на px пути
    movaps  xmm9, [v_64]
    movaps  xmm10, [v_2_5]
    movaps  xmm11, [v_40]
    movaps  xmm12, [v_4096]
    movaps  xmm13, [v_255]
    movaps  xmm14, [v_mflr]
    xorps   xmm15, xmm15
    ; дорожки (высота луча, путь) с рамкой 4: пусто, луч на «полу»
    mov     eax, -MFLR
    cvtsi2ss xmm0, eax
    movd    eax, xmm0
    lea     rdi, [cols]
    xor     ecx, ecx
.cl:
    mov     [rdi + rcx*4], eax
    mov     dword [rdi + rcx*4 + SWD], 0
    mov     [rdi + rcx*4 + 2*SWD], eax
    mov     dword [rdi + rcx*4 + 3*SWD], 0
    inc     ecx
    cmp     ecx, SW8 + 8
    jb      .cl
    mov     [sw_P], rdi
    add     rdi, 2*SWD
    mov     [sw_C], rdi
    ; карта от начала окна
    mov     eax, [m8_oy]
    add     eax, HPAD
    imul    eax, eax, HW8
    add     eax, [m8_ox]
    add     eax, HPAD
    lea     rdx, [hmap8]
    add     rdx, rax
    mov     [sw_hb], rdx
    ; столбцы от края к свету до дальнего края окна (за окном тень никому не нужна)
    xor     r12d, r12d
.a:
    mov     eax, r12d                   ; u
    cmp     dword [sw_sgn], 0
    jl      .un
    mov     eax, SW8 - 1
    sub     eax, r12d
.un:
    mov     [sw_u], eax
    mov     r8d, eax
    imul    r8d, [sw_ha]
    add     r8, [sw_hb]
    mov     r14d, [sw_v0]
    mov     ecx, r14d
    cmp     dword [sw_ymaj], 0
    jne     .hy
    imul    ecx, ecx, HW8
.hy:
    add     r8, rcx
    mov     rsi, [sw_P]
    add     rsi, r13
    mov     rdi, [sw_C]
    add     rdi, 16
    sub     eax, MG8
    cmp     eax, MW8
    jae     .marg
    ; облака этого столбца (сэмпл сетки — 2x2 ячейки)
    shr     eax, 1
    imul    eax, [sw_ca]
    lea     rcx, [mzg]
    add     rcx, rax
    mov     r15d, MG8
    cmp     r14d, r15d
    jae     .e0
    call    .prop
.e0:
    mov     eax, [sw_u]
    imul    eax, [sw_oa]
    lea     r10, [mz8]
    add     r10, rax
    mov     eax, MG8
    imul    eax, [sw_op]
    add     r10, rax
    mov     r15d, MG8 + MW8
    call    .emit
    mov     r15d, [sw_v1]
    cmp     r14d, r15d
    jae     .swap
    call    .prop
    jmp     .swap
.marg:
    mov     r15d, [sw_v1]
    call    .prop
.swap:
    mov     rax, [sw_P]
    mov     rdx, [sw_C]
    mov     [sw_P], rdx
    mov     [sw_C], rax
    inc     r12d
    cmp     r12d, SW8 - MG8
    jb      .a
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

; r14 = v (кратно 4), r15 = конец, r8 -> карта в (u, v); rsi, rdi — дорожки;
; r10 -> mz8, rcx -> облака столбца (mzg)
.prop:
    cmp     dword [sw_ymaj], 0
    jne     .propy
    SWEEP4  0, 0
    ret
.propy:
    SWEEP4  0, 1
    ret
.emit:
    cmp     dword [sw_ymaj], 0
    jne     .emity
    SWEEP4  1, 0
    ret
.emity:
    SWEEP4  1, 1
    ret


; mzg (GW x GW): байт 1 — пропускание облаков (шум 64x64); до развёртки — она кладёт
; облака в ячейки mz8. Строки — пулом
moon_grid:
    sub     rsp, 40
    lea     rcx, [moon_grid_rows]
    mov     edx, GW
    mov     r8d, 8
    call    par_for
    add     rsp, 40
    ret

; строки mzg ecx .. edx−1 (с любого потока пула)
moon_grid_rows:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    mov     r14d, ecx                   ; j
    mov     r13d, edx
    imul    eax, ecx, GW*4
    lea     rdi, [mzg]
    add     rdi, rax
    lea     r12, [cl_tab]
.r:
    mov     eax, [goy]
    add     eax, r14d
    imul    ebp, eax, 43
    sub     ebp, [cl_oy]
    mov     eax, ebp
    shr     eax, 8
    and     eax, 63
    lea     ecx, [eax + 1]
    and     ecx, 63
    shl     eax, 6
    shl     ecx, 6
    lea     r10, [clouds]
    lea     r11, [r10 + rcx]
    add     r10, rax
    and     ebp, 255                    ; fy
    xor     r15d, r15d                  ; i
.c:
    mov     eax, [gox]
    add     eax, r15d
    imul    esi, eax, 43
    sub     esi, [cl_ox]
    mov     r8d, esi
    shr     r8d, 8
    and     r8d, 63
    lea     r9d, [r8d + 1]
    and     r9d, 63
    and     esi, 255                    ; fx
    movzx   eax, byte [r10 + r8]
    movzx   ecx, byte [r10 + r9]
    sub     ecx, eax
    imul    ecx, esi
    sar     ecx, 8
    add     eax, ecx
    movzx   edx, byte [r11 + r8]
    movzx   ecx, byte [r11 + r9]
    sub     ecx, edx
    imul    ecx, esi
    sar     ecx, 8
    add     edx, ecx
    sub     edx, eax
    imul    edx, ebp
    sar     edx, 8
    add     eax, edx
    movzx   eax, byte [r12 + rax]
    shl     eax, 8
    mov     [rdi], eax
    add     rdi, 4
    inc     r15d
    cmp     r15d, GW
    jb      .c
    inc     r14d
    cmp     r14d, r13d
    jb      .r
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- заслонители луча фонарика
; для каждого живого зомби в луче: ux, uy, d, начало тени, 0.21/d
build_flash_occ:
    push    rbx
    push    rdi
    mov     dword [nocc], 0
    cmp     dword [pl_flash], 0
    je      .out
    lea     rbx, [ents]
    lea     rdi, [occ]
    mov     ecx, MAXE
.l:
    mov     eax, [rbx + E_ST]
    cmp     eax, ST_WANDER
    jb      .n
    cmp     eax, ST_CORPSE
    jae     .n
    movss   xmm1, [rbx + E_X]
    subss   xmm1, [pl + E_X]
    movss   xmm2, [rbx + E_Y]
    subss   xmm2, [pl + E_Y]
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    movaps  xmm4, xmm2
    mulss   xmm4, xmm2
    addss   xmm3, xmm4
    comiss  xmm3, [f_256]
    jae     .n
    comiss  xmm3, [f_0_09]
    jb      .n
    sqrtss  xmm3, xmm3
    divss   xmm1, xmm3
    divss   xmm2, xmm3
    movaps  xmm4, xmm1
    mulss   xmm4, [pl + E_FX]
    movaps  xmm5, xmm2
    mulss   xmm5, [pl + E_FY]
    addss   xmm4, xmm5
    comiss  xmm4, [f_0_45]
    jb      .n
    movss   [rdi], xmm1
    movss   [rdi + 4], xmm2
    movss   [rdi + 8], xmm3
    movaps  xmm4, xmm3
    addss   xmm4, [f_0_28]
    movss   [rdi + 12], xmm4
    movss   xmm4, [f_0_21]
    divss   xmm4, xmm3
    movss   [rdi + 16], xmm4
    add     rdi, OC_SZ
    inc     dword [nocc]
    cmp     dword [nocc], MAXOCC
    jae     .out
.n:
    add     rbx, ESZ
    dec     ecx
    jnz     .l
.out:
    pop     rdi
    pop     rbx
    ret

; --lm: снимок — вид сверху на запечённый свет огней вокруг игрока (360x360 сэмплов,
; 4 на тайл, огни комнат + фонари + сломанный), заслонители карты высот — синим
dump_lmap:
    push    rbx
    push    rsi
    push    rdi
    mov     rdi, [fbbits]
    xor     eax, eax
    mov     ecx, SCR_W*SCR_H
    rep     stosd
    mov     rdi, [fbbits]
    movss   xmm0, [pl + E_X]
    mulss   xmm0, [f_4]
    cvttss2si r8d, xmm0
    sub     r8d, 180
    movss   xmm0, [pl + E_Y]
    mulss   xmm0, [f_4]
    cvttss2si r9d, xmm0
    sub     r9d, 180
    xor     esi, esi
.y:
    xor     ebx, ebx
.x:
    lea     ecx, [r8d + ebx]
    lea     edx, [r9d + esi]
    cmp     ecx, SLW-1
    ja      .n
    cmp     edx, SLW-1
    ja      .n
    mov     eax, edx
    shl     eax, 9
    add     eax, ecx
    lea     r10, [slightL]
    mov     r11d, [r10 + rax*4]
    ; ячейки карты высот 2x2 вокруг сэмпла — голубым
    lea     eax, [edx*2 + HPAD - 1]
    imul    eax, eax, HW8
    lea     eax, [eax + ecx*2 + HPAD - 1]
    lea     r10, [hmap8]
    movzx   ecx, byte [r10 + rax]
    and     ecx, 0x7F
    movzx   edx, byte [r10 + rax + 1]
    and     edx, 0x7F
    cmp     ecx, edx
    cmovb   ecx, edx
    movzx   edx, byte [r10 + rax + HW8]
    and     edx, 0x7F
    cmp     ecx, edx
    cmovb   ecx, edx
    movzx   edx, byte [r10 + rax + HW8 + 1]
    and     edx, 0x7F
    cmp     ecx, edx
    cmovb   ecx, edx
    test    ecx, ecx
    jz      .put
    shl     ecx, 2                      ; высота·4 — синим, 10 px забора — тускло
    mov     edx, 255
    cmp     ecx, edx
    cmova   ecx, edx
    mov     r11d, ecx
    or      r11d, 0x00300000
.put:
    mov     eax, esi
    imul    eax, eax, SCR_W
    add     eax, ebx
    mov     [rdi + rax*4 + 140*4], r11d
.n:
    inc     ebx
    cmp     ebx, 360
    jb      .x
    inc     esi
    cmp     esi, 360
    jb      .y
    pop     rdi
    pop     rsi
    pop     rbx
    ret
