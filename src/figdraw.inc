; ----------------------------------------------------------------------------
;  figdraw.inc — фигура из капсул в G-буфер лучом (код полосы, как objects.inc)
; ----------------------------------------------------------------------------
; fd_rec -> запись фигуры (fig.inc), fd_sx/fd_sy — ступни на экране, fd_gx/fd_gy — в
; мире (1/256 тайла). Луч пикселя: из плоскости экрана через ступни, из-за спины
; зрителя (K = 60 U вдоль T) по −T; у каждой капсулы, чья рамка накрывает пиксель, —
; вход луча (цилиндр или полусфера на конце), берётся ближний к глазу. У пикселя —
; нормаль в точке входа, сама точка (в gpos) и цвет палитры с узором одежды, кровью,
; снегом сверху. Пишется, только если то, что уже лежит, не ближе к глазу
draw_fig:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 168
    movdqu  [rsp], xmm6
    movdqu  [rsp + 16], xmm7
    movdqu  [rsp + 32], xmm8
    movdqu  [rsp + 48], xmm9
    movdqu  [rsp + 64], xmm10
    movdqu  [rsp + 80], xmm11
    movdqu  [rsp + 96], xmm12
    movdqu  [rsp + 112], xmm13
    movdqu  [rsp + 128], xmm14
    movdqu  [rsp + 144], xmm15
    mov     rbx, [fd_rec]
    ; строки фигуры в полосе
    mov     r12d, [fd_sy]
    add     r12d, [rbx + FG_Y0]
    mov     eax, [band_y0]
    cmp     r12d, eax
    cmovl   r12d, eax
    mov     r13d, [fd_sy]
    add     r13d, [rbx + FG_Y1]
    inc     r13d
    mov     eax, [band_y1]
    cmp     r13d, eax
    cmovg   r13d, eax
    cmp     r12d, r13d
    jge     .out
.row:
    ; капсулы, чья рамка задевает строку
    mov     eax, r12d
    sub     eax, [fd_sy]
    mov     [fd_iv], eax
    xor     ecx, ecx
    lea     r8, [rbx + FG_HDR]
    mov     edx, [rbx + FG_N]
    xor     r9d, r9d
    lea     r10, [fd_list]
.sl:
    cmp     r9d, edx
    jae     .sld
    cmp     eax, [r8 + FP_Y0]
    jl      .sln
    cmp     eax, [r8 + FP_Y1]
    jg      .sln
    mov     [r10 + rcx], r9b
    inc     ecx
.sln:
    add     r8, FP_SZ
    inc     r9d
    jmp     .sl
.sld:
    mov     [fd_ln], ecx
    test    ecx, ecx
    jz      .rown
    ; начало луча строки: v·вниз + K·T (вниз по экрану = (√⅛, √⅛, −cos 30°))
    xorps   xmm0, xmm0                  ; xorps: cvtsi2ss не ждёт прошлый столбец
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    movaps  xmm1, xmm0
    mulss   xmm1, [vk_s8]
    addss   xmm1, [fk_kt6]
    movss   [fd_oxr], xmm1
    mulss   xmm0, [vk_c30]
    movss   xmm1, [fk_kt5]
    subss   xmm1, xmm0
    movss   [fd_ozr], xmm1
    ; столбцы
    mov     esi, [fd_sx]
    add     esi, [rbx + FG_X0]
    xor     eax, eax
    cmp     esi, eax
    cmovl   esi, eax
    mov     edi, [fd_sx]
    add     edi, [rbx + FG_X1]
    mov     eax, SCR_W - 1
    cmp     edi, eax
    cmovg   edi, eax
.col:
    cmp     esi, edi
    jg      .rown
    mov     eax, esi
    sub     eax, [fd_sx]
    mov     [fd_iu], eax
    xorps   xmm0, xmm0
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    mulss   xmm0, [vk_s2]
    movss   xmm8, [fd_oxr]
    addss   xmm8, xmm0                  ; O: x = … + u·√½
    movss   xmm9, [fd_oxr]
    subss   xmm9, xmm0                  ;    y = … − u·√½
    movss   xmm10, [fd_ozr]
    movss   xmm11, [vk_big]             ; ближайший вход
    xor     r14d, r14d                  ; его капсула
    xor     r15d, r15d
.pk:
    cmp     r15d, [fd_ln]
    jae     .pkd
    lea     rax, [fd_list]
    movzx   eax, byte [rax + r15]
    shl     eax, 6
    lea     rbp, [rbx + rax + FG_HDR]
    mov     eax, [fd_iu]
    cmp     eax, [rbp + FP_X0]
    jl      .pkn
    cmp     eax, [rbp + FP_X1]
    jg      .pkn
    ; oa = O − a
    movaps  xmm0, xmm8
    subss   xmm0, [rbp + FP_AX]
    movaps  xmm1, xmm9
    subss   xmm1, [rbp + FP_AY]
    movaps  xmm2, xmm10
    subss   xmm2, [rbp + FP_AZ]
    movss   xmm5, [rbp + FP_BABA]
    xorps   xmm6, xmm6
    comiss  xmm5, xmm6
    je      .capa                       ; шар
    ; луч·oa, |oa|², ba·oa
    movaps  xmm3, xmm0
    addss   xmm3, xmm1
    mulss   xmm3, [vk_c6]
    movaps  xmm4, xmm2
    mulss   xmm4, [f_0_5]
    addss   xmm3, xmm4
    xorps   xmm3, [f_signmask]          ; rdoa
    movaps  xmm4, xmm0
    mulss   xmm4, xmm0
    movaps  xmm6, xmm1
    mulss   xmm6, xmm1
    addss   xmm4, xmm6
    movaps  xmm6, xmm2
    mulss   xmm6, xmm2
    addss   xmm4, xmm6                  ; oaoa
    movss   xmm6, [rbp + FP_BAX]
    mulss   xmm6, xmm0
    movss   xmm7, [rbp + FP_BAY]
    mulss   xmm7, xmm1
    addss   xmm6, xmm7
    movss   xmm7, [rbp + FP_BAZ]
    mulss   xmm7, xmm2
    addss   xmm6, xmm7                  ; baoa
    movss   xmm7, [rbp + FP_BARD]
    ; A = baba − bard², B = baba·rdoa − baoa·bard, C = baba·oaoa − baoa² − r²·baba
    movaps  xmm12, xmm7
    mulss   xmm12, xmm7
    movaps  xmm13, xmm5
    subss   xmm13, xmm12                ; A
    movaps  xmm12, xmm5
    mulss   xmm12, [fk_eps]
    comiss  xmm13, xmm12
    jbe     .both                       ; ось вдоль луча — только концы
    movaps  xmm14, xmm5
    mulss   xmm14, xmm3
    movaps  xmm12, xmm6
    mulss   xmm12, xmm7
    subss   xmm14, xmm12                ; B
    movaps  xmm15, xmm5
    mulss   xmm15, xmm4
    movaps  xmm12, xmm6
    mulss   xmm12, xmm6
    subss   xmm15, xmm12
    movss   xmm12, [rbp + FP_R2]
    mulss   xmm12, xmm5
    subss   xmm15, xmm12                ; C
    movaps  xmm12, xmm14
    mulss   xmm12, xmm14
    mulss   xmm15, xmm13
    subss   xmm12, xmm15                ; h = B² − A·C
    xorps   xmm15, xmm15
    comiss  xmm12, xmm15
    jb      .pkn
    sqrtss  xmm12, xmm12
    addss   xmm12, xmm14
    xorps   xmm12, [f_signmask]
    divss   xmm12, xmm13                ; t = (−B − √h)/A
    movaps  xmm13, xmm12
    mulss   xmm13, xmm7
    addss   xmm13, xmm6                 ; y = baoa + t·bard
    comiss  xmm13, xmm15
    jbe     .capa
    comiss  xmm13, xmm5
    jae     .capb
    ; бок: n = (oa + t·rd − ba·y/baba) / r
    comiss  xmm12, xmm11
    jae     .pkn
    movss   xmm11, xmm12
    mov     r14, rbp
    mulss   xmm13, [rbp + FP_IBB]       ; y/baba
    movss   xmm14, [f_1]
    divss   xmm14, [rbp + FP_R]
    movaps  xmm4, xmm12
    mulss   xmm4, [vk_c6]               ; t·0.612
    movaps  xmm3, xmm0
    subss   xmm3, xmm4
    movss   xmm5, [rbp + FP_BAX]
    mulss   xmm5, xmm13
    subss   xmm3, xmm5
    mulss   xmm3, xmm14
    movss   [fd_nx], xmm3
    movaps  xmm3, xmm1
    subss   xmm3, xmm4
    movss   xmm5, [rbp + FP_BAY]
    mulss   xmm5, xmm13
    subss   xmm3, xmm5
    mulss   xmm3, xmm14
    movss   [fd_ny], xmm3
    movaps  xmm4, xmm12
    mulss   xmm4, [f_0_5]
    movaps  xmm3, xmm2
    subss   xmm3, xmm4
    movss   xmm5, [rbp + FP_BAZ]
    mulss   xmm5, xmm13
    subss   xmm3, xmm5
    mulss   xmm3, xmm14
    movss   [fd_nz], xmm3
    jmp     .pkn
.both:
    ; ось почти вдоль луча: оба конца, ближний
    call    .cap
    subss   xmm0, [rbp + FP_BAX]
    subss   xmm1, [rbp + FP_BAY]
    subss   xmm2, [rbp + FP_BAZ]
    call    .cap
    jmp     .pkn
.capb:
    subss   xmm0, [rbp + FP_BAX]        ; oc = oa − ba
    subss   xmm1, [rbp + FP_BAY]
    subss   xmm2, [rbp + FP_BAZ]
.capa:
    call    .cap
.pkn:
    inc     r15d
    jmp     .pk
.pkd:
    test    r14, r14
    jz      .coln
    ; точка входа P = O − t·T
    movaps  xmm0, xmm11
    mulss   xmm0, [vk_c6]
    movaps  xmm1, xmm8
    subss   xmm1, xmm0
    movss   [fd_px], xmm1
    movaps  xmm1, xmm9
    subss   xmm1, xmm0
    movss   [fd_py], xmm1
    movaps  xmm0, xmm11
    mulss   xmm0, [f_0_5]
    movaps  xmm1, xmm10
    subss   xmm1, xmm0
    movss   [fd_pz], xmm1
    ; в мире: ступни + P (U -> 1/256 тайла)
    movss   xmm0, [fd_px]
    mulss   xmm0, [vk_u2t]
    cvtss2si ecx, xmm0
    add     ecx, [fd_gx]
    movss   xmm0, [fd_py]
    mulss   xmm0, [vk_u2t]
    cvtss2si edx, xmm0
    add     edx, [fd_gy]
    ; то, что уже есть, ближе к глазу?
    imul    eax, r12d, SCR_W
    add     eax, esi
    mov     [fd_pix], eax
    lea     r8, [albedo]
    cmp     dword [r8 + rax*4], 0
    je      .w
    mov     r9d, [r8 + rax*4 + (gposb - albedo)]
    movzx   r10d, r9w
    shr     r9d, 16
    add     r10d, r9d
    lea     r9d, [ecx + edx + 12]
    cmp     r10d, r9d
    jg      .coln
.w:
    and     ecx, 0xFFFF
    shl     edx, 16
    or      ecx, edx
    mov     [r8 + rax*4 + (gposb - albedo)], ecx
    ; цвет
    call    fd_color
    mov     eax, [fd_pix]
    lea     r8, [albedo]
    mov     [r8 + rax*4], ecx
    ; нормаль
    movss   xmm0, [fd_nx]
    movss   xmm1, [fd_ny]
    movss   xmm2, [fd_nz]
    call    nq_id
    mov     ecx, [fd_pix]
    lea     r8, [nbuf]
    mov     [r8 + rcx], al
.coln:
    inc     esi
    jmp     .col
.rown:
    inc     r12d
    cmp     r12d, r13d
    jl      .row
.out:
    movdqu  xmm6, [rsp]
    movdqu  xmm7, [rsp + 16]
    movdqu  xmm8, [rsp + 32]
    movdqu  xmm9, [rsp + 48]
    movdqu  xmm10, [rsp + 64]
    movdqu  xmm11, [rsp + 80]
    movdqu  xmm12, [rsp + 96]
    movdqu  xmm13, [rsp + 112]
    movdqu  xmm14, [rsp + 128]
    movdqu  xmm15, [rsp + 144]
    add     rsp, 168
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
; полусфера конца: xmm0-2 = oc (от её центра); ближе прежнего — в лучшие
; (портит xmm3-5, xmm12-14; xmm0-2 сохраняет)
.cap:
    movaps  xmm3, xmm0
    addss   xmm3, xmm1
    mulss   xmm3, [vk_c6]
    movaps  xmm4, xmm2
    mulss   xmm4, [f_0_5]
    addss   xmm3, xmm4
    xorps   xmm3, [f_signmask]          ; b = луч·oc
    movaps  xmm4, xmm0
    mulss   xmm4, xmm0
    movaps  xmm5, xmm1
    mulss   xmm5, xmm1
    addss   xmm4, xmm5
    movaps  xmm5, xmm2
    mulss   xmm5, xmm2
    addss   xmm4, xmm5
    subss   xmm4, [rbp + FP_R2]         ; c = |oc|² − r²
    movaps  xmm12, xmm3
    mulss   xmm12, xmm3
    subss   xmm12, xmm4                 ; h = b² − c
    xorps   xmm13, xmm13
    comiss  xmm12, xmm13
    jb      .cr
    sqrtss  xmm12, xmm12
    addss   xmm12, xmm3
    xorps   xmm12, [f_signmask]         ; t = −b − √h
    comiss  xmm12, xmm11
    jae     .cr
    movss   xmm11, xmm12
    mov     r14, rbp
    movss   xmm14, [f_1]
    divss   xmm14, [rbp + FP_R]
    movaps  xmm4, xmm12
    mulss   xmm4, [vk_c6]
    movaps  xmm5, xmm0
    subss   xmm5, xmm4
    mulss   xmm5, xmm14
    movss   [fd_nx], xmm5
    movaps  xmm5, xmm1
    subss   xmm5, xmm4
    mulss   xmm5, xmm14
    movss   [fd_ny], xmm5
    movaps  xmm4, xmm12
    mulss   xmm4, [f_0_5]
    movaps  xmm5, xmm2
    subss   xmm5, xmm4
    mulss   xmm5, xmm14
    movss   [fd_nz], xmm5
.cr:
    ret

; цвет пикселя фигуры: r14 -> капсула, rbx -> фигура, fd_px/py/pz — точка, fd_nz —
; нормаль вверх, fd_pix — пиксель -> ecx (со старшим байтом)
; (портит eax, edx, r8-r11, xmm0)
fd_color:
    push    rsi
    mov     r10d, [r14 + FP_MAT]
    mov     eax, r10d
    and     eax, 15
    mov     ecx, [rbx + FG_COL + rax*4]
    test    ecx, 0x80000000
    jnz     .ret                        ; светится — как есть
    and     ecx, 0xFFFFFF
    mov     r11d, r10d
    shr     r11d, 8
    and     r11d, 15
    cmp     r11d, 1
    je      .quilt
    cmp     r11d, 2
    je      .knit
    cmp     r11d, 3
    je      .fur
    cmp     r11d, 4
    je      .dots
    jmp     .blood
.quilt:
    ; стёжка: строчки через 1.8 U
    movss   xmm0, [fd_pz]
    addss   xmm0, [vk_64]
    mulss   xmm0, [vk_0_55]
    cvttss2si eax, xmm0
    cvtsi2ss xmm1, eax
    subss   xmm0, xmm1
    comiss  xmm0, [vk_0_25]
    jae     .blood
    mov     r8d, 189
    call    vc_mul
    jmp     .blood
.knit:
    FDCELL  fd_pz, vk_1_2
    mov     esi, eax
    movss   xmm0, [fd_px]
    subss   xmm0, [fd_py]
    addss   xmm0, [vk_64]
    addss   xmm0, [vk_64]
    mulss   xmm0, [vk_1_2]
    cvttss2si eax, xmm0
    add     eax, esi
    test    eax, 1
    jz      .blood
    mov     r8d, 220
    call    vc_mul
    jmp     .blood
.fur:
    FDCELL  fd_px, vk_2
    mov     esi, eax
    FDCELL  fd_pz, vk_2
    imul    eax, eax, 977
    xor     esi, eax
    FDCELL  fd_py, vk_2
    HASH    esi, eax
    and     esi, 63
    lea     r8d, [esi + 210]
    call    vc_mul
    jmp     .blood
.dots:
    FDCELL  fd_px, vk_0_9
    mov     esi, eax
    FDCELL  fd_pz, vk_0_9
    imul    eax, eax, 131
    add     esi, eax
    FDCELL  fd_py, vk_0_9
    HASH    esi, eax
    test    esi, 7
    jnz     .blood
    mov     ecx, 0xE0D8C4
.blood:
    ; пятна крови на одежде зомби
    test    r10d, FT_BLOOD
    jz      .snow
    mov     edx, [rbx + FG_BLOOD]
    test    edx, edx
    jz      .snow
    FDCELL  fd_px, vk_1_3
    mov     esi, eax
    FDCELL  fd_pz, vk_1_3
    imul    eax, eax, 97
    add     esi, eax
    add     esi, [rbx + FG_SEED]
    FDCELL  fd_py, vk_1_3
    HASH    esi, eax
    and     esi, 255
    cmp     esi, [rbx + FG_BLOOD]
    jae     .snow
    mov     ecx, 0x5E1414
    test    esi, 16
    jz      .snow
    mov     ecx, 0x7A1818
.snow:
    ; снег на том, что смотрит вверх (плечи, шапка), — у мёртвых и у зомби
    mov     edx, [rbx + FG_SNOW]
    test    edx, edx
    jz      .noise
    movss   xmm0, [fd_nz]
    comiss  xmm0, [vk_0_45]
    jbe     .noise
    FDCELL  fd_px, vk_2_2
    mov     esi, eax
    FDCELL  fd_pz, vk_2_2
    imul    eax, eax, 57
    add     esi, eax
    add     esi, [rbx + FG_SEED]
    FDCELL  fd_py, vk_2_2
    HASH    esi, eax
    and     esi, 255
    cmp     esi, [rbx + FG_SNOW]
    jae     .noise
    mov     ecx, 0xE0E6EE
.noise:
    mov     esi, [fd_pix]
    mov     eax, [rbx + FG_SEED]
    HASH    esi, eax
    and     esi, 15
    lea     r8d, [esi + 248]
    call    vc_mul
    or      ecx, 0x01000000
.ret:
    pop     rsi
    ret
