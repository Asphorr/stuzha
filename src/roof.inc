; ----------------------------------------------------------------------------
;  roof.inc — избы: запись дома, высота крыши, снятие крыши, проход крыш.
;  Крыша рисуется после объектов: каждый кусок — выпуклый четырёхугольник в своей
;  мировой плоскости; на пиксель восстанавливается точка крыши (мир + высота),
;  сравнивается глубина с G-буфером (x+y по одной экранной точке), шейдер даёт цвет
;  и нормаль, свет — свой (небо по нормали и видимость дома), луна — в light_pass.
;  Локальные оси: u — вдоль конька, v — поперёк; +v и +u смотрят к камере.
; ----------------------------------------------------------------------------
section .text

; ---------------------------------------------------------------- запись дома
; ecx, edx = печь (тайл); размеры и флаги — H_x, H_y, H_w, H_h, H_lit, H_tv
add_house:
    push    rbx
    push    rsi
    push    rdi
    mov     eax, [nhouses]
    cmp     eax, MAXH
    jae     .out
    imul    edi, eax, HS_SZ
    lea     rbx, [houses]
    add     rbx, rdi
    inc     eax
    mov     [nhouses], eax
    mov     r8d, eax                    ; номер + 1 — в rmap
    mov     [rbx + HS_SX], ecx
    mov     [rbx + HS_SY], edx
    mov     eax, [H_x]
    mov     [rbx + HS_X0], eax
    mov     eax, [H_y]
    mov     [rbx + HS_Y0], eax
    mov     eax, [H_w]
    mov     [rbx + HS_W], eax
    mov     eax, [H_h]
    mov     [rbx + HS_H], eax
    mov     dword [rbx + HS_FADE], __float32__(1.0)
    mov     dword [rbx + HS_OCC], 0
    ; облик — от хеша места, а не от ГСЧ: село не перетасуется
    mov     eax, [H_x]
    imul    eax, eax, 7
    add     eax, 3
    mov     ecx, [H_y]
    imul    ecx, ecx, 13
    add     ecx, 5
    HASH    eax, ecx
    mov     [rbx + HS_SEED], eax
    mov     esi, eax
    ; конёк — вдоль длинной стороны
    xor     ecx, ecx
    mov     eax, [H_w]
    cmp     eax, [H_h]
    ja      .ax
    mov     ecx, 1
    jb      .ax
    mov     ecx, esi
    and     ecx, 1
.ax:
    mov     [rbx + HS_AX], ecx
    mov     eax, esi
    shr     eax, 4
    and     eax, 3
    cmp     eax, 3
    jb      .p
    mov     eax, 1
.p:
    add     eax, 11
    mov     [rbx + HS_P], eax
    mov     eax, [H_tv]
    add     eax, eax
    or      eax, [H_lit]
    test    esi, 0x300
    jnz     .f1
    or      eax, HF_SLIDE
.f1:
    test    esi, 0xC00
    jnz     .f2
    or      eax, HF_CROW
.f2:
    test    esi, 0x1000
    jz      .f3
    or      eax, HF_WASH
.f3:
    test    esi, 0x2000
    jz      .f4
    or      eax, HF_SUN
.f4:
    test    esi, 0x4000
    jz      .f5
    or      eax, HF_BARGE
.f5:
    mov     [rbx + HS_FL], eax
    mov     eax, esi
    shr     eax, 16
    and     eax, 3
    mov     [rbx + HS_MAT], eax
    mov     eax, esi
    shr     eax, 20
    and     eax, 7
    cmp     eax, 6
    jb      .g
    sub     eax, 6
.g:
    lea     rdx, [gab_pal]
    mov     eax, [rdx + rax*4]
    mov     [rbx + HS_GAB], eax
    ; снег на коньке и верх трубы
    mov     eax, [H_h]
    cmp     dword [rbx + HS_AX], 0
    je      .zr
    mov     eax, [H_w]
.zr:
    imul    eax, [rbx + HS_P]
    shr     eax, 1
    add     eax, RZW + RTS
    mov     [rbx + HS_ZR], eax
    mov     r10, rbx
    mov     ecx, [rbx + HS_SX]
    shl     ecx, 8
    add     ecx, 128
    mov     edx, [rbx + HS_SY]
    shl     edx, 8
    add     edx, 128
    call    roof_zs
    add     eax, RCHH
    mov     [rbx + HS_ZT], eax
    mov     [H_zt], eax
    ; тайлы под крышей
    mov     esi, [H_y]
.ry:
    mov     eax, [H_y]
    add     eax, [H_h]
    cmp     esi, eax
    jge     .out
    mov     edi, [H_x]
.rx:
    mov     eax, [H_x]
    add     eax, [H_w]
    cmp     edi, eax
    jge     .rn
    mov     eax, esi
    shl     eax, 7
    add     eax, edi
    lea     rdx, [rmap]
    mov     [rdx + rax], r8b
    inc     edi
    jmp     .rx
.rn:
    inc     esi
    jmp     .ry
.out:
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; r10 = дом, ecx = wx, edx = wy (8.8) -> eax = высота снега на крыше, px (портит ecx, edx)
roof_zs:
    cmp     dword [r10 + HS_AX], 0
    jne     .ay
    mov     eax, [r10 + HS_Y0]
    mov     ecx, [r10 + HS_H]
    jmp     .c
.ay:
    mov     edx, ecx
    mov     eax, [r10 + HS_X0]
    mov     ecx, [r10 + HS_W]
.c:
    shl     eax, 8
    shl     ecx, 7                      ; полупролёт
    add     eax, ecx                    ; конёк
    sub     edx, eax
    mov     eax, edx
    sar     eax, 31
    xor     edx, eax
    sub     edx, eax                    ; от конька
    sub     ecx, edx
    imul    ecx, [r10 + HS_P]
    sar     ecx, 8
    lea     eax, [ecx + RZW + RTS]
    ret

; ---------------------------------------------------------------- снятие крыши
; игрок в избе — крыша тает за ~0.3 с (как в PZ), вышел — возвращается
update_roofs:
    movss   xmm2, [dt]
    mulss   xmm2, [f_7]
    minss   xmm2, [f_1]
    lea     rdx, [houses]
    mov     ecx, [nhouses]
.l:
    test    ecx, ecx
    jz      .out
    movss   xmm3, [f_1]
    cvtsi2ss xmm0, dword [rdx + HS_X0]
    movss   xmm4, [pl + E_X]
    comiss  xmm0, xmm4
    ja      .t
    cvtsi2ss xmm1, dword [rdx + HS_W]
    addss   xmm0, xmm1
    comiss  xmm0, xmm4
    jbe     .t
    cvtsi2ss xmm0, dword [rdx + HS_Y0]
    movss   xmm4, [pl + E_Y]
    comiss  xmm0, xmm4
    ja      .t
    cvtsi2ss xmm1, dword [rdx + HS_H]
    addss   xmm0, xmm1
    comiss  xmm0, xmm4
    jbe     .t
    xorps   xmm3, xmm3
.t:
    movss   xmm0, [rdx + HS_FADE]
    subss   xmm3, xmm0
    mulss   xmm3, xmm2
    addss   xmm0, xmm3
    movss   [rdx + HS_FADE], xmm0
    add     rdx, HS_SZ
    dec     ecx
    jmp     .l
.out:
    ret

; ---------------------------------------------------------------- растеризатор
; xmm0 = p, xmm1 = q, xmm2 = r, xmm3 = t: плоскость p·wx + q·wy + r·z = t (8.8 и px).
; Экранная точка: A = 16(sx − CX0) + 8, B = 32(sy − CY0) + 16; вдоль её луча
; a = (s + A)/2, b = (s − A)/2, z = (s − B)/32 (a, b — от камеры); s линейна по A, B
rq_plane:
    cvtsi2ss xmm4, dword [camx]
    mulss   xmm4, xmm0
    subss   xmm3, xmm4
    cvtsi2ss xmm4, dword [camy]
    mulss   xmm4, xmm1
    subss   xmm3, xmm4                  ; t'
    movaps  xmm4, xmm0
    addss   xmm4, xmm1
    mulss   xmm4, [f_0_5]
    movaps  xmm5, xmm2
    mulss   xmm5, [f_inv32]
    addss   xmm4, xmm5
    movss   xmm5, [f_1]
    divss   xmm5, xmm4                  ; 1/знаменатель
    mulss   xmm3, xmm5
    movss   [rq_s0], xmm3
    movaps  xmm4, xmm1
    subss   xmm4, xmm0
    mulss   xmm4, [f_0_5]
    mulss   xmm4, xmm5
    movss   [rq_sa], xmm4
    mulss   xmm2, [f_inv32]
    mulss   xmm2, xmm5
    movss   [rq_sb], xmm2
    ret

; то же в осях дома: xmm0 = при u, xmm1 = при v
rq_planel:
    cmp     dword [rq_ax], 0
    je      rq_plane
    movaps  xmm4, xmm0
    movaps  xmm0, xmm1
    movaps  xmm1, xmm4
    jmp     rq_plane

; xmm0 = wx, xmm1 = wy (8.8), xmm2 = z (px), ecx = номер угла -> rq_x, rq_y
rq_corner:
    cvtsi2ss xmm4, dword [camx]
    subss   xmm0, xmm4
    cvtsi2ss xmm4, dword [camy]
    subss   xmm1, xmm4
    movaps  xmm4, xmm0
    subss   xmm4, xmm1
    mulss   xmm4, [f_inv16]
    addss   xmm4, [f_320]
    movaps  xmm5, xmm0
    addss   xmm5, xmm1
    mulss   xmm5, [f_inv32]
    subss   xmm5, xmm2
    addss   xmm5, [f_196]
    lea     rdx, [rq_x]
    movss   [rdx + rcx*4], xmm4
    movss   [rdx + rcx*4 + 16], xmm5
    ret

; xmm0 = u, xmm1 = v, xmm2 = z, ecx = угол
rq_cornl:
    cmp     dword [rq_ax], 0
    je      rq_corner
    movaps  xmm4, xmm0
    movaps  xmm0, xmm1
    movaps  xmm1, xmm4
    jmp     rq_corner

; угол из памяти: u, v, z (f32), номер
%macro RC 4
    movss   xmm0, %1
    movss   xmm1, %2
    movss   xmm2, %3
    mov     ecx, %4
    call    rq_cornl
%endmacro

; шейдер, нормаль по умолчанию, заливка
%macro RFILL 2
    lea     rax, [%1]
    mov     [rq_sh], rax
    mov     eax, %2
    mov     [rq_n0], eax
    call    rq_fill
%endmacro

; заливка выпуклого четырёхугольника rq_x/rq_y. На пиксель: точка крыши, тест глубины,
; проявка ([rq_fade] из 16, Байер 4x4), окно вокруг игрока (если крыша перед ним), шейдер.
; Шейдер: r12d = u, r13d = v (8.8), r14d = z (px) -> eax = альбедо|флаги (0 — дыра),
; нормаль — в [rq_n]. Может портить rax, rcx, rdx, r8–r14, xmm0–xmm5
rq_fill:
    push    rbx
    push    rsi
    push    rdi
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 40
    ; строки
    movss   xmm0, [rq_y]
    movss   xmm1, xmm0
    minss   xmm0, [rq_y + 4]
    maxss   xmm1, [rq_y + 4]
    minss   xmm0, [rq_y + 8]
    maxss   xmm1, [rq_y + 8]
    minss   xmm0, [rq_y + 12]
    maxss   xmm1, [rq_y + 12]
    subss   xmm0, [f_0_5]
    cvttss2si esi, xmm0                 ; ceil
    cvtsi2ss xmm2, esi
    comiss  xmm2, xmm0
    jae     .c0
    inc     esi
.c0:
    subss   xmm1, [f_0_5]
    cvttss2si eax, xmm1                 ; floor
    cvtsi2ss xmm2, eax
    comiss  xmm2, xmm1
    jbe     .c1
    dec     eax
.c1:
    mov     ecx, SCR_H - 1
    cmp     eax, ecx
    cmovg   eax, ecx
    mov     [rq_r1], eax
    xor     ecx, ecx
    cmp     esi, ecx
    cmovl   esi, ecx
    cvtsi2ss xmm10, dword [camx]
    cvtsi2ss xmm11, dword [camy]
    movss   xmm12, [f_0_5]
    movss   xmm13, [f_inv32]
    movss   xmm14, [f_16]
    movss   xmm8, [rq_sa]
    mulss   xmm8, xmm14                 ; шаг s по пикселю
    lea     rbp, [albedo]
.row:
    cmp     esi, [rq_r1]
    jg      .done
    cvtsi2ss xmm0, esi
    addss   xmm0, xmm12                 ; середина строки
    movss   xmm4, [f_1e9]
    movss   xmm5, [f_m1e9]
    xor     ecx, ecx
.e:
    lea     rdx, [rq_y]
    movss   xmm1, [rdx + rcx*4]         ; yi
    lea     eax, [ecx + 1]
    and     eax, 3
    movss   xmm2, [rdx + rax*4]         ; yj
    movaps  xmm3, xmm0
    subss   xmm3, xmm1
    movaps  xmm6, xmm0
    subss   xmm6, xmm2
    mulss   xmm6, xmm3
    comiss  xmm6, [f_0]
    ja      .en                         ; строка не пересекает ребро
    movaps  xmm6, xmm2
    subss   xmm6, xmm1
    comiss  xmm6, [f_0]
    je      .en
    lea     rdx, [rq_x]
    movss   xmm7, [rdx + rax*4]
    subss   xmm7, [rdx + rcx*4]
    mulss   xmm7, xmm3
    divss   xmm7, xmm6
    addss   xmm7, [rdx + rcx*4]
    minss   xmm4, xmm7
    maxss   xmm5, xmm7
.en:
    inc     ecx
    cmp     ecx, 4
    jb      .e
    comiss  xmm4, xmm5
    ja      .rn
    subss   xmm4, xmm12
    cvttss2si edi, xmm4                 ; первая колонка = ceil(xl − 0.5)
    cvtsi2ss xmm2, edi
    comiss  xmm2, xmm4
    jae     .x0
    inc     edi
.x0:
    subss   xmm5, xmm12
    cvttss2si eax, xmm5
    cvtsi2ss xmm2, eax
    comiss  xmm2, xmm5
    jbe     .x1
    dec     eax
.x1:
    mov     ecx, SCR_W - 1
    cmp     eax, ecx
    cmovg   eax, ecx
    mov     [rq_c1], eax
    xor     ecx, ecx
    cmp     edi, ecx
    cmovl   edi, ecx
    cmp     edi, eax
    jg      .rn
    ; A, B, s в начале пролёта
    lea     eax, [esi - CY0]
    shl     eax, 5
    add     eax, 16
    cvtsi2ss xmm9, eax                  ; B
    lea     eax, [edi - CX0]
    shl     eax, 4
    add     eax, 8
    cvtsi2ss xmm7, eax                  ; A
    movss   xmm6, [rq_sa]
    mulss   xmm6, xmm7
    movss   xmm0, [rq_sb]
    mulss   xmm0, xmm9
    addss   xmm6, xmm0
    addss   xmm6, [rq_s0]               ; s
    imul    r15d, esi, SCR_W
    add     r15d, edi
.px:
    movaps  xmm0, xmm6
    addss   xmm0, xmm7
    mulss   xmm0, xmm12
    addss   xmm0, xmm10
    cvtss2si r12d, xmm0                 ; wx
    movaps  xmm1, xmm6
    subss   xmm1, xmm7
    mulss   xmm1, xmm12
    addss   xmm1, xmm11
    cvtss2si r13d, xmm1                 ; wy
    movaps  xmm2, xmm6
    subss   xmm2, xmm9
    mulss   xmm2, xmm13
    cvtss2si r14d, xmm2                 ; z
    lea     eax, [r12d + r13d]          ; глубина
    mov     edx, [rbp + r15*4]
    test    edx, edx
    jz      .dok
    mov     edx, [rbp + r15*4 + (gposb - albedo)]
    movzx   ecx, dx
    shr     edx, 16
    add     ecx, edx
    cmp     eax, ecx
    jl      .skip                       ; перед крышей что-то ближе
.dok:
    ; крыша ближе игрока — закрывает ли она саму фигуру (для окна со следующего кадра)
    cmp     eax, [rq_dpl]
    jle     .fd
    mov     edx, edi
    sub     edx, [psx]
    add     edx, 6
    cmp     edx, 12
    ja      .fd
    mov     edx, [psy]
    sub     edx, esi
    cmp     edx, 27
    ja      .fd
    mov     dword [rq_occn], 1
.fd:
    mov     ecx, esi
    and     ecx, 3
    shl     ecx, 2
    mov     edx, edi
    and     edx, 3
    add     ecx, edx
    lea     rdx, [bayer4]
    movzx   ecx, byte [rdx + rcx]
    cmp     ecx, [rq_fade]
    jae     .skip
    ; крыша заслоняет игрока — круглое окно с растрированным краем
    cmp     dword [rq_occ], 0
    je      .cut0
    cmp     eax, [rq_dpl]
    jle     .cut0
    mov     edx, edi
    sub     edx, [psx]
    imul    edx, edx
    mov     r8d, esi
    sub     r8d, [psy]
    add     r8d, 14
    imul    r8d, r8d
    add     edx, r8d
    cmp     edx, RCUT_O2
    jae     .cut0
    cmp     edx, RCUT_I2
    jb      .skip
    imul    ecx, ecx, (RCUT_O2 - RCUT_I2) / 16
    neg     edx
    add     edx, RCUT_O2
    cmp     ecx, edx
    jb      .skip
.cut0:
    mov     ecx, r13d
    shl     ecx, 16
    mov     edx, r12d
    and     edx, 0xFFFF
    or      ecx, edx
    mov     [rq_gp], ecx
    cmp     dword [rq_ax], 0
    je      .ns
    xchg    r12d, r13d
.ns:
    mov     ecx, [rq_n0]
    mov     [rq_n], ecx
    call    [rq_sh]
    test    eax, eax
    jz      .skip
    mov     [rbp + r15*4], eax
    mov     ecx, [rq_gp]
    mov     [rbp + r15*4 + (gposb - albedo)], ecx
    mov     ecx, [rq_n]
    and     ecx, 0x3F
    mov     [rq_n], ecx
    mov     edx, [rbp + rcx*4 + (amb_n - albedo)]
    cmp     dword [rl_n], 0
    je      .nol
    ; фонари рядом — натрий на снегу крыши
    mov     [rq_l], edx
    movaps  xmm0, xmm6
    addss   xmm0, xmm7
    mulss   xmm0, xmm12
    addss   xmm0, xmm10
    movaps  xmm1, xmm6
    subss   xmm1, xmm7
    mulss   xmm1, xmm12
    addss   xmm1, xmm11
    movaps  xmm2, xmm6
    subss   xmm2, xmm9
    mulss   xmm2, xmm13
    call    rq_lamps
    movd    xmm0, eax
    movd    xmm1, [rq_l]
    paddusb xmm0, xmm1
    movd    edx, xmm0
    mov     ecx, [rq_n]
.nol:
    or      edx, [rq_vis]
    mov     [rbp + r15*4 + (lbuf - albedo)], edx
    or      ecx, NOWN
    mov     [rbp + r15 + (nbuf - albedo)], cl
.skip:
    addss   xmm6, xmm8
    addss   xmm7, xmm14
    inc     r15d
    inc     edi
    cmp     edi, [rq_c1]
    jle     .px
.rn:
    inc     esi
    jmp     .row
.done:
    add     rsp, 40
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; xmm0 = wx, xmm1 = wy (8.8), xmm2 = z (px), [rq_n] = нормаль -> eax = свет фонарей рядом
; с домом (B,G,R): затухание (1 − d²/R²)², N·L с мягкой обёрткой. Портит rax, rcx, rdx, xmm0–5
rq_lamps:
    movss   [rl_wx], xmm0
    movss   [rl_wy], xmm1
    movss   [rl_wz], xmm2
    mov     ecx, [rq_n]
    shl     ecx, 4
    lea     rdx, [nvec]
    add     rdx, rcx
    xorps   xmm5, xmm5
    lea     rax, [rl_lamps]
    mov     ecx, [rl_n]
.l:
    movss   xmm0, [rax]
    subss   xmm0, [rl_wx]
    mulss   xmm0, [f_pxu]               ; до фонаря, px
    movss   xmm1, [rax + 4]
    subss   xmm1, [rl_wy]
    mulss   xmm1, [f_pxu]
    movss   xmm2, [rax + 8]
    subss   xmm2, [rl_wz]
    movaps  xmm3, xmm0
    mulss   xmm3, xmm0
    movaps  xmm4, xmm1
    mulss   xmm4, xmm1
    addss   xmm3, xmm4
    movaps  xmm4, xmm2
    mulss   xmm4, xmm2
    addss   xmm3, xmm4                  ; d²
    movaps  xmm4, xmm3
    mulss   xmm4, [rax + 12]
    movss   [rl_t], xmm4
    movss   xmm4, [f_1]
    subss   xmm4, [rl_t]
    comiss  xmm4, [f_0]
    jbe     .n
    mulss   xmm4, xmm4
    movss   [rl_t], xmm4
    mulss   xmm0, [rdx]
    mulss   xmm1, [rdx + 4]
    mulss   xmm2, [rdx + 8]
    addss   xmm0, xmm1
    addss   xmm0, xmm2
    rsqrtss xmm3, xmm3
    mulss   xmm0, xmm3                  ; cos
    addss   xmm0, [f_0_25]
    mulss   xmm0, [f_0_8]
    maxss   xmm0, [f_0]
    mulss   xmm0, [rl_t]
    shufps  xmm0, xmm0, 0
    movups  xmm1, [rax + 16]
    mulps   xmm0, xmm1
    addps   xmm5, xmm0
.n:
    add     rax, 32
    dec     ecx
    jnz     .l
    cvtps2dq xmm5, xmm5
    packssdw xmm5, xmm5
    packuswb xmm5, xmm5
    movd    eax, xmm5
    ret

; отрезок в осях дома (la_u0.., la_u1..) толщиной в пиксель, с тестом глубины и проявкой;
; цвет [la_col], нормаль N_BC
rq_line:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32
    movss   xmm0, [la_u0]
    movss   xmm1, [la_v0]
    movss   xmm2, [la_z0]
    xor     ecx, ecx
    call    rq_cornl
    movss   xmm0, [la_u1]
    movss   xmm1, [la_v1]
    movss   xmm2, [la_z1]
    mov     ecx, 1
    call    rq_cornl
    movss   xmm0, [rq_x + 4]
    subss   xmm0, [rq_x]
    andps   xmm0, [f_absmask]
    movss   xmm1, [rq_y + 4]
    subss   xmm1, [rq_y]
    andps   xmm1, [f_absmask]
    maxss   xmm0, xmm1
    cvttss2si ebx, xmm0
    add     ebx, 1                      ; шагов
    cmp     ebx, 200
    ja      .out
    cvtsi2ss xmm7, ebx
    movss   xmm6, [f_1]
    divss   xmm6, xmm7                  ; dt
    xor     esi, esi
.s:
    cmp     esi, ebx
    jg      .out
    cvtsi2ss xmm7, esi
    mulss   xmm7, xmm6                  ; t
    ; мировая точка (в осях дома)
    movss   xmm0, [la_u1]
    subss   xmm0, [la_u0]
    mulss   xmm0, xmm7
    addss   xmm0, [la_u0]
    movss   xmm1, [la_v1]
    subss   xmm1, [la_v0]
    mulss   xmm1, xmm7
    addss   xmm1, [la_v0]
    movss   xmm2, [la_z1]
    subss   xmm2, [la_z0]
    mulss   xmm2, xmm7
    addss   xmm2, [la_z0]
    cmp     dword [rq_ax], 0
    je      .nsw
    movaps  xmm3, xmm0
    movaps  xmm0, xmm1
    movaps  xmm1, xmm3
.nsw:
    cvtss2si r12d, xmm0                 ; wx
    cvtss2si r13d, xmm1                 ; wy
    mov     ecx, 2
    call    rq_corner
    cvttss2si eax, [rq_x + 8]
    cvttss2si edx, [rq_y + 8]
    cmp     eax, SCR_W
    jae     .sn
    cmp     edx, SCR_H
    jae     .sn
    mov     ecx, edx
    and     ecx, 3
    shl     ecx, 2
    mov     r8d, eax
    and     r8d, 3
    add     ecx, r8d
    lea     r8, [bayer4]
    movzx   ecx, byte [r8 + rcx]
    cmp     ecx, [rq_fade]
    jae     .sn
    imul    edx, edx, SCR_W
    add     edx, eax
    lea     rdi, [albedo]
    lea     eax, [r12d + r13d]
    mov     ecx, [rdi + rdx*4]
    test    ecx, ecx
    jz      .ok
    mov     ecx, [rdi + rdx*4 + (gposb - albedo)]
    movzx   r8d, cx
    shr     ecx, 16
    add     ecx, r8d
    cmp     eax, ecx
    jl      .sn
.ok:
    mov     eax, [la_col]
    mov     [rdi + rdx*4], eax
    mov     eax, r13d
    shl     eax, 16
    and     r12d, 0xFFFF
    or      eax, r12d
    mov     [rdi + rdx*4 + (gposb - albedo)], eax
    mov     eax, [amb_n + N_BC*4]
    or      eax, [rq_vis]
    mov     [rdi + rdx*4 + (lbuf - albedo)], eax
    mov     byte [rdi + rdx + (nbuf - albedo)], N_BC | NOWN
.sn:
    inc     esi
    jmp     .s
.out:
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- проход крыш
draw_roofs:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    mov     eax, [plw_x]
    add     eax, [plw_y]
    add     eax, 48
    mov     [rq_dpl], eax
    lea     rbx, [houses]
    mov     r15d, [nhouses]
.h:
    test    r15d, r15d
    jz      .done
    movss   xmm0, [rbx + HS_FADE]
    mulss   xmm0, [f_16]
    cvtss2si eax, xmm0
    mov     [rq_fade], eax
    test    eax, eax
    jz      .hn
    ; грубая отсечка по центру дома
    mov     eax, [rbx + HS_W]
    shl     eax, 7
    mov     ecx, [rbx + HS_X0]
    shl     ecx, 8
    add     ecx, eax
    mov     eax, [rbx + HS_H]
    shl     eax, 7
    mov     edx, [rbx + HS_Y0]
    shl     edx, 8
    add     edx, eax
    call    w2s
    cmp     eax, -200
    jl      .hn
    cmp     eax, SCR_W + 200
    jg      .hn
    cmp     edx, -60
    jl      .hn
    cmp     edx, SCR_H + 140
    jg      .hn
    mov     eax, [rbx + HS_OCC]
    mov     [rq_occ], eax
    mov     dword [rq_occn], 0
    call    roof_setup
    call    roof_draw
    mov     eax, [rq_occn]
    mov     [rbx + HS_OCC], eax
    jmp     .hx
.hn:
    mov     dword [rbx + HS_OCC], 0
.hx:
    add     rbx, HS_SZ
    dec     r15d
    jmp     .h
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

; rbx = дом -> ri_*, rf_*, нормали, видимость
roof_setup:
    push    rsi
    push    rdi
    mov     eax, [rbx + HS_AX]
    mov     [rq_ax], eax
    mov     ecx, [rbx + HS_X0]
    mov     edx, [rbx + HS_W]
    mov     r8d, [rbx + HS_Y0]
    mov     r9d, [rbx + HS_H]
    mov     r10d, [rbx + HS_SX]
    mov     r11d, [rbx + HS_SY]
    test    eax, eax
    jz      .ax0
    xchg    ecx, r8d
    xchg    edx, r9d
    xchg    r10d, r11d
.ax0:
    shl     ecx, 8
    mov     [ri_U0], ecx
    shl     edx, 8
    add     edx, ecx
    mov     [ri_U1], edx
    shl     r8d, 8
    mov     [ri_V0], r8d
    shl     r9d, 7
    mov     [ri_hs], r9d
    add     r8d, r9d
    mov     [ri_Vm], r8d
    add     r8d, r9d
    mov     [ri_V1], r8d
    shl     r10d, 8
    add     r10d, 128
    mov     [ri_uc], r10d
    shl     r11d, 8
    add     r11d, 128
    mov     [ri_vc], r11d
    mov     eax, [rbx + HS_P]
    mov     [ri_P], eax
    mov     eax, [rbx + HS_ZR]
    mov     [ri_zr], eax
    mov     eax, [rbx + HS_ZT]
    mov     [ri_zt], eax
    mov     eax, [rbx + HS_FL]
    mov     [ri_fl], eax
    mov     eax, [rbx + HS_SEED]
    mov     [ri_seed], eax
    mov     eax, [rbx + HS_MAT]
    mov     [ri_mat], eax
    mov     eax, [rbx + HS_GAB]
    mov     [ri_gab], eax
    mov     ecx, 0xD2D6DA               ; причелины белёные
    test    dword [rbx + HS_FL], HF_BARGE
    jz      .bg
    mov     ecx, eax
.bg:
    mov     [ri_barge], ecx
    ; f32
    cvtsi2ss xmm0, dword [ri_U0]
    subss   xmm0, [f_rog]
    movss   [rf_u0], xmm0
    cvtsi2ss xmm0, dword [ri_U1]
    movss   [rf_ug], xmm0
    addss   xmm0, [f_rog]
    movss   [rf_u1], xmm0
    movss   [rf_uv], xmm0
    cvtsi2ss xmm0, dword [ri_V0]
    subss   xmm0, [f_rov]
    movss   [rf_v0], xmm0
    cvtsi2ss xmm0, dword [ri_V1]
    addss   xmm0, [f_rov]
    movss   [rf_v1], xmm0
    cvtsi2ss xmm0, dword [ri_Vm]
    movss   [rf_vm], xmm0
    cvtsi2ss xmm0, dword [ri_P]
    mulss   xmm0, [f_inv256]
    movss   [rf_k], xmm0
    cvtsi2ss xmm1, dword [ri_zr]
    movss   [rf_zr], xmm1
    cvtsi2ss xmm2, dword [ri_hs]
    addss   xmm2, [f_rov]
    mulss   xmm2, xmm0
    subss   xmm1, xmm2
    movss   [rf_ze], xmm1               ; снег у кромки свеса
    cvtss2si eax, xmm1
    mov     [ri_ze], eax
    ; нормали кусков
    mov     dword [rn_f], N_RF
    mov     dword [rn_b], N_RF + 1
    mov     dword [rn_g], N_FX
    mov     dword [rn_a], N_FY
    cmp     dword [rq_ax], 0
    je      .nv
    mov     dword [rn_f], N_RF + 2
    mov     dword [rn_b], N_RF + 3
    mov     dword [rn_g], N_FY
    mov     dword [rn_a], N_FX
.nv:
    ; видимость — лучшее из кольца тайлов вокруг сруба
    xor     r9d, r9d
    mov     esi, [rbx + HS_Y0]
    dec     esi
.vy:
    mov     eax, [rbx + HS_Y0]
    add     eax, [rbx + HS_H]
    cmp     esi, eax
    jg      .vd
    mov     edi, [rbx + HS_X0]
    dec     edi
.vx:
    mov     eax, [rbx + HS_X0]
    add     eax, [rbx + HS_W]
    cmp     edi, eax
    jg      .vyn
    ; только кольцо
    mov     eax, [rbx + HS_Y0]
    dec     eax
    cmp     esi, eax
    je      .vr
    add     eax, [rbx + HS_H]
    inc     eax
    cmp     esi, eax
    je      .vr
    mov     eax, [rbx + HS_X0]
    dec     eax
    cmp     edi, eax
    je      .vr
    add     eax, [rbx + HS_W]
    inc     eax
    cmp     edi, eax
    jne     .vxn
.vr:
    lea     eax, [edi*4 + 2]
    sub     eax, [gox]
    cmp     eax, GW
    jae     .vxn
    lea     ecx, [esi*4 + 2]
    sub     ecx, [goy]
    cmp     ecx, GW
    jae     .vxn
    imul    ecx, ecx, GW
    add     eax, ecx
    lea     rdx, [lgrid]
    movzx   eax, byte [rdx + rax*4 + 3]
    cmp     eax, r9d
    cmova   r9d, eax
.vxn:
    inc     edi
    jmp     .vx
.vyn:
    inc     esi
    jmp     .vy
.vd:
    shl     r9d, 24
    mov     [rq_vis], r9d
    ; до двух уличных фонарей, до которых свет дотягивается
    mov     dword [rl_n], 0
    lea     rsi, [lights]
    mov     edi, [nlights]
    lea     r10, [rl_lamps]
.lp:
    test    edi, edi
    jz      .lpd
    mov     eax, [rsi + L_KIND]
    lea     r11, [lampf_vec]
    cmp     eax, LK_LAMP
    je      .lk
    lea     r11, [lampb_vec]
    cmp     eax, LK_BROKEN
    jne     .lpn
.lk:
    cvtsi2ss xmm0, dword [rbx + HS_X0]
    subss   xmm0, [rsi + L_X]
    cvtsi2ss xmm2, dword [rbx + HS_W]
    movaps  xmm1, xmm0
    addss   xmm1, xmm2
    xorps   xmm1, [f_signmask]
    maxss   xmm0, xmm1
    maxss   xmm0, [f_0]                 ; до сруба по x
    cvtsi2ss xmm3, dword [rbx + HS_Y0]
    subss   xmm3, [rsi + L_Y]
    cvtsi2ss xmm2, dword [rbx + HS_H]
    movaps  xmm1, xmm3
    addss   xmm1, xmm2
    xorps   xmm1, [f_signmask]
    maxss   xmm3, xmm1
    maxss   xmm3, [f_0]
    mulss   xmm0, xmm0
    mulss   xmm3, xmm3
    addss   xmm0, xmm3
    movss   xmm1, [rsi + L_R]
    mulss   xmm1, [f_0_6]               ; дальше свет на крыше — меньше процента
    mulss   xmm1, xmm1
    comiss  xmm0, xmm1
    jae     .lpn
    movss   xmm0, [rsi + L_X]
    mulss   xmm0, [f_256]
    movss   [r10], xmm0
    movss   xmm0, [rsi + L_Y]
    mulss   xmm0, [f_256]
    movss   [r10 + 4], xmm0
    mov     dword [r10 + 8], __float32__(39.0)  ; голова фонаря, px
    movss   xmm0, [rsi + L_R]
    mulss   xmm0, [f_19_6]
    mulss   xmm0, xmm0
    movss   xmm1, [f_1]
    divss   xmm1, xmm0
    movss   [r10 + 12], xmm1
    movups  xmm0, [rsi + L_COL]
    movups  xmm1, [r11]
    mulps   xmm0, xmm1                  ; натрий греется / фонарь мигает
    movups  [r10 + 16], xmm0
    add     r10, 32
    inc     dword [rl_n]
    cmp     dword [rl_n], 2
    jae     .lpd
.lpn:
    add     rsi, L_SZ
    dec     edi
    jmp     .lp
.lpd:
    pop     rdi
    pop     rsi
    ret

; куски крыши — от дальних к ближним (глубина всё равно сверяется)
roof_draw:
    sub     rsp, 40
    ; ----- задний скат: z = zr − k(vm − v)
    xorps   xmm0, xmm0
    movss   xmm1, [rf_k]
    xorps   xmm1, [f_signmask]
    movss   xmm2, [f_1]
    movss   xmm3, [rf_k]
    mulss   xmm3, [rf_vm]
    movss   xmm4, [rf_zr]
    subss   xmm4, xmm3
    movaps  xmm3, xmm4
    call    rq_planel
    RC      [rf_u0], [rf_v0], [rf_ze], 0
    RC      [rf_u1], [rf_v0], [rf_ze], 1
    RC      [rf_u1], [rf_vm], [rf_zr], 2
    RC      [rf_u0], [rf_vm], [rf_zr], 3
    RFILL   sh_slope, [rn_b]
    ; ----- фронтон: плоскость u = стена, треугольник под тёсом
    movss   xmm0, [f_1]
    xorps   xmm1, xmm1
    xorps   xmm2, xmm2
    movss   xmm3, [rf_ug]
    call    rq_planel
    cvtsi2ss xmm0, dword [ri_V0]
    movss   [rt_a], xmm0
    cvtsi2ss xmm0, dword [ri_V1]
    movss   [rt_b], xmm0
    mov     eax, [ri_zr]
    sub     eax, RTS
    cvtsi2ss xmm0, eax
    movss   [rt_c], xmm0
    RC      [rf_ug], [rt_a], [f_rzw], 0
    RC      [rf_ug], [rt_b], [f_rzw], 1
    RC      [rf_ug], [rf_vm], [rt_c], 2
    RC      [rf_ug], [rf_vm], [rt_c], 3
    RFILL   sh_gable, [rn_g]
    ; ----- передний скат: z = zr − k(v − vm)
    xorps   xmm0, xmm0
    movss   xmm1, [rf_k]
    movss   xmm2, [f_1]
    movss   xmm3, [rf_k]
    mulss   xmm3, [rf_vm]
    addss   xmm3, [rf_zr]
    call    rq_planel
    RC      [rf_u0], [rf_vm], [rf_zr], 0
    RC      [rf_u1], [rf_vm], [rf_zr], 1
    RC      [rf_u1], [rf_v1], [rf_ze], 2
    RC      [rf_u0], [rf_v1], [rf_ze], 3
    RFILL   sh_slope, [rn_f]
    ; ----- торец свеса: снег, тёс и резные причелины (плоскость u = край свеса)
    movss   xmm0, [f_1]
    xorps   xmm1, xmm1
    xorps   xmm2, xmm2
    movss   xmm3, [rf_uv]
    call    rq_planel
    movss   xmm0, [rf_ze]
    addss   xmm0, [f_1]
    movss   [rt_a], xmm0                ; верх у свеса
    movss   xmm0, [rf_zr]
    addss   xmm0, [f_1]
    movss   [rt_b], xmm0                ; верх у конька
    movss   xmm0, [rf_ze]
    subss   xmm0, [f_12]
    movss   [rt_c], xmm0
    movss   xmm0, [rf_zr]
    subss   xmm0, [f_12]
    movss   [rt_d], xmm0
    RC      [rf_uv], [rf_v0], [rt_a], 0
    RC      [rf_uv], [rf_vm], [rt_b], 1
    RC      [rf_uv], [rf_vm], [rt_d], 2
    RC      [rf_uv], [rf_v0], [rt_c], 3
    RFILL   sh_verge, [rn_g]
    RC      [rf_uv], [rf_vm], [rt_b], 0
    RC      [rf_uv], [rf_v1], [rt_a], 1
    RC      [rf_uv], [rf_v1], [rt_c], 2
    RC      [rf_uv], [rf_vm], [rt_d], 3
    RFILL   sh_verge, [rn_g]
    ; ----- полотенце под коньком
    movss   xmm0, [f_1]
    xorps   xmm1, xmm1
    xorps   xmm2, xmm2
    movss   xmm3, [rf_uv]
    addss   xmm3, [f_2]
    movss   [rt_e], xmm3
    call    rq_planel
    movss   xmm0, [rf_vm]
    subss   xmm0, [f_32]
    movss   [rt_a], xmm0
    movss   xmm0, [rf_vm]
    addss   xmm0, [f_32]
    movss   [rt_b], xmm0
    movss   xmm0, [rf_zr]
    subss   xmm0, [f_4]
    movss   [rt_c], xmm0
    movss   xmm0, [rf_zr]
    subss   xmm0, [f_30]
    movss   [rt_d], xmm0
    RC      [rt_e], [rt_a], [rt_c], 0
    RC      [rt_e], [rt_b], [rt_c], 1
    RC      [rt_e], [rt_b], [rt_d], 2
    RC      [rt_e], [rt_a], [rt_d], 3
    RFILL   sh_towel, [rn_g]
    ; ----- кромка свеса: снежный карниз, тёс, подзор, сосульки (плоскость v = край)
    xorps   xmm0, xmm0
    movss   xmm1, [f_1]
    xorps   xmm2, xmm2
    movss   xmm3, [rf_v1]
    call    rq_planel
    movss   xmm0, [rf_ze]
    addss   xmm0, [f_1]
    movss   [rt_a], xmm0
    movss   xmm0, [rf_ze]
    subss   xmm0, [f_22]
    movss   [rt_b], xmm0
    RC      [rf_u0], [rf_v1], [rt_a], 0
    RC      [rf_u1], [rf_v1], [rt_a], 1
    RC      [rf_u1], [rf_v1], [rt_b], 2
    RC      [rf_u0], [rf_v1], [rt_b], 3
    RFILL   sh_fascia, [rn_a]
    ; ----- труба: грань +u, грань +v, верх
    cvtsi2ss xmm0, dword [ri_uc]
    addss   xmm0, [f_rch]
    movss   [rt_a], xmm0                ; uc + RCH
    subss   xmm0, [f_rch]
    subss   xmm0, [f_rch]
    movss   [rt_b], xmm0                ; uc − RCH
    cvtsi2ss xmm0, dword [ri_vc]
    addss   xmm0, [f_rch]
    movss   [rt_c], xmm0                ; vc + RCH
    subss   xmm0, [f_rch]
    subss   xmm0, [f_rch]
    movss   [rt_d], xmm0                ; vc − RCH
    cvtsi2ss xmm0, dword [ri_zt]
    movss   [rt_e], xmm0
    subss   xmm0, [f_40]
    movss   [rt_f], xmm0                ; низ — заведомо под снегом
    movss   xmm0, [f_1]
    xorps   xmm1, xmm1
    xorps   xmm2, xmm2
    movss   xmm3, [rt_a]
    call    rq_planel
    RC      [rt_a], [rt_d], [rt_f], 0
    RC      [rt_a], [rt_c], [rt_f], 1
    RC      [rt_a], [rt_c], [rt_e], 2
    RC      [rt_a], [rt_d], [rt_e], 3
    mov     dword [rq_face], 0
    RFILL   sh_chim, [rn_g]
    xorps   xmm0, xmm0
    movss   xmm1, [f_1]
    xorps   xmm2, xmm2
    movss   xmm3, [rt_c]
    call    rq_planel
    RC      [rt_b], [rt_c], [rt_f], 0
    RC      [rt_a], [rt_c], [rt_f], 1
    RC      [rt_a], [rt_c], [rt_e], 2
    RC      [rt_b], [rt_c], [rt_e], 3
    mov     dword [rq_face], 1
    RFILL   sh_chim, [rn_a]
    xorps   xmm0, xmm0
    xorps   xmm1, xmm1
    movss   xmm2, [f_1]
    movss   xmm3, [rt_e]
    call    rq_planel
    RC      [rt_b], [rt_d], [rt_e], 0
    RC      [rt_a], [rt_d], [rt_e], 1
    RC      [rt_a], [rt_c], [rt_e], 2
    RC      [rt_b], [rt_c], [rt_e], 3
    mov     dword [rq_face], 2
    RFILL   sh_chim, N_UP
    ; ----- конёк-охлупень с конской головой над передним фронтоном (плоскость v = конёк)
    xorps   xmm0, xmm0
    movss   xmm1, [f_1]
    xorps   xmm2, xmm2
    movss   xmm3, [rf_vm]
    call    rq_planel
    movss   xmm0, [rf_uv]
    subss   xmm0, [f_8]
    movss   [rt_a], xmm0
    addss   xmm0, [f_192]
    movss   [rt_b], xmm0
    movss   xmm0, [rf_zr]
    subss   xmm0, [f_3]
    movss   [rt_c], xmm0
    addss   xmm0, [f_17]
    movss   [rt_d], xmm0
    RC      [rt_a], [rf_vm], [rt_c], 0
    RC      [rt_b], [rf_vm], [rt_c], 1
    RC      [rt_b], [rf_vm], [rt_d], 2
    RC      [rt_a], [rf_vm], [rt_d], 3
    RFILL   sh_horse, [rn_a]
    ; ----- антенна над домом с телевизором
    test    dword [ri_fl], HF_TV
    jz      .noant
    call    roof_antenna
.noant:
    add     rsp, 40
    ret

; телеантенна: мачта у дальнего фронтона, «волновой канал», кабель по скату; качается
roof_antenna:
    sub     rsp, 40
    ; качание верхушки по ветру
    cvtsi2ss xmm0, dword [ri_uc]
    mulss   xmm0, [f_inv256]
    cvtsi2ss xmm1, dword [ri_vc]
    mulss   xmm1, [f_inv256]
    call    wind_at
    movss   [ra_g], xmm0
    movss   xmm0, [wind_t]
    mulss   xmm0, [f_2_6]
    mov     eax, [ri_seed]
    and     eax, 255
    cvtsi2ss xmm1, eax
    mulss   xmm1, [f_0_025]
    addss   xmm0, xmm1
    call    fsin
    mulss   xmm0, [f_0_3]
    addss   xmm0, [f_1]
    mulss   xmm0, [ra_g]
    mulss   xmm0, [f_14]                ; сдвиг верхушки, 8.8
    movss   [ra_s], xmm0
    ; основание мачты: треть конька от дальнего фронтона
    mov     eax, [ri_U1]
    sub     eax, [ri_U0]
    imul    eax, eax, 77
    sar     eax, 8
    add     eax, [ri_U0]
    cvtsi2ss xmm0, eax
    movss   [ra_u], xmm0
    mov     dword [la_col], 0x01303438
    ; мачта
    movss   xmm0, [ra_u]
    movss   [la_u0], xmm0
    addss   xmm0, [ra_s]
    movss   [la_u1], xmm0
    movss   xmm0, [rf_vm]
    movss   [la_v0], xmm0
    movss   [la_v1], xmm0
    movss   xmm0, [rf_zr]
    subss   xmm0, [f_1]
    movss   [la_z0], xmm0
    addss   xmm0, [f_26]
    movss   [la_z1], xmm0
    call    rq_line
    ; стрела вдоль конька
    movss   xmm0, [la_u1]
    movss   [ra_t], xmm0                ; u верхушки
    subss   xmm0, [f_40]
    movss   [la_u0], xmm0
    movss   xmm0, [ra_t]
    addss   xmm0, [f_64]
    movss   [la_u1], xmm0
    movss   xmm0, [la_z1]
    movss   [la_z0], xmm0
    call    rq_line
    ; вибраторы поперёк — короче к переду
    lea     rsi, [ra_elem]
    mov     edi, 4
.el:
    movsx   eax, word [rsi]
    cvtsi2ss xmm0, eax
    addss   xmm0, [ra_t]
    movss   [la_u0], xmm0
    movss   [la_u1], xmm0
    movsx   eax, word [rsi + 2]
    cvtsi2ss xmm1, eax
    movss   xmm0, [rf_vm]
    subss   xmm0, xmm1
    movss   [la_v0], xmm0
    movss   xmm0, [rf_vm]
    addss   xmm0, xmm1
    movss   [la_v1], xmm0
    call    rq_line
    add     rsi, 4
    dec     edi
    jnz     .el
    ; кабель по переднему скату к свесу
    mov     dword [la_col], 0x01202226
    movss   xmm0, [ra_u]
    addss   xmm0, [f_12]
    movss   [la_u0], xmm0
    movss   [la_u1], xmm0
    movss   xmm0, [rf_vm]
    movss   [la_v0], xmm0
    movss   xmm0, [rf_v1]
    movss   [la_v1], xmm0
    movss   xmm0, [rf_zr]
    addss   xmm0, [f_2]
    movss   [la_z0], xmm0
    movss   xmm0, [rf_ze]
    movss   [la_z1], xmm0
    call    rq_line
    add     rsp, 40
    ret

; ---------------------------------------------------------------- шейдеры
; eax = 0xRRGGBB, ecx = множитель 0..256 -> eax (портит xmm0, xmm1)
col_mul:
    movd    xmm0, eax
    pxor    xmm1, xmm1
    punpcklbw xmm0, xmm1
    movd    xmm1, ecx
    pshuflw xmm1, xmm1, 0
    pmullw  xmm0, xmm1
    psrlw   xmm0, 8
    packuswb xmm0, xmm0
    movd    eax, xmm0
    ret

; кровля без снега: r12d = u, r10d = хеш -> eax
roof_mat:
    mov     eax, [ri_mat]
    cmp     eax, 1
    je      .slate
    cmp     eax, 2
    je      .green
    cmp     eax, 3
    je      .rust
    ; тёс: доски по 4 px, щели
    mov     eax, r12d
    and     eax, 63
    cmp     eax, 10
    mov     eax, 0x2A2018
    jb      .r
    mov     ecx, r12d
    sar     ecx, 6
    imul    ecx, ecx, 0x2C1B3C6D
    shr     ecx, 28
    mov     eax, 0x5A4632
    sub     eax, ecx
    sub     eax, ecx
    ret
.slate:
    ; шифер: волна через 2 px
    mov     eax, r12d
    and     eax, 31
    cmp     eax, 11
    mov     eax, 0x8C8E8A
    jb      .r
    cmp     eax, 22
    mov     eax, 0x747672
    jb      .r
    mov     eax, 0x5C5E5A
    ret
.green:
    mov     eax, r12d
    and     eax, 63
    cmp     eax, 7
    mov     eax, 0x6E9076
    jb      .r
    mov     eax, 0x3E6048
    ret
.rust:
    mov     eax, r12d
    and     eax, 63
    cmp     eax, 7
    mov     eax, 0x8E5E48
    jb      .r
    mov     eax, 0x6E3C2C
.r:
    ret

; скат: снег с застругами, искрами, светлым коньком и карнизом; проталина у тёплой
; трубы, съехавший пласт у края, вороньи следы; r12d = u, r13d = v, r14d = z
sh_slope:
    mov     eax, r13d
    sub     eax, [ri_Vm]
    cdq
    xor     eax, edx
    sub     eax, edx
    mov     r8d, eax                    ; от конька
    mov     r9d, [ri_hs]
    add     r9d, ROV
    sub     r9d, r8d                    ; до кромки свеса
    mov     ecx, r12d
    sar     ecx, 4
    add     ecx, [ri_seed]
    mov     edx, r13d
    sar     edx, 4
    HASH    ecx, edx
    mov     r10d, ecx                   ; шум ячейки 1x1 px
    ; тёплая труба: снег вокруг подтаял
    test    dword [ri_fl], HF_LIT
    jz      .nomelt
    mov     eax, r12d
    sub     eax, [ri_uc]
    imul    eax, eax
    mov     edx, r13d
    sub     edx, [ri_vc]
    imul    edx, edx
    add     eax, edx
    cmp     eax, (RCH + 30) * (RCH + 30)
    jb      .wet
    cmp     eax, (RCH + 52) * (RCH + 52)
    jae     .nomelt
    sub     eax, (RCH + 30) * (RCH + 30)
    imul    eax, eax, 1024
    xor     edx, edx
    mov     ecx, (RCH + 52) * (RCH + 52) - (RCH + 30) * (RCH + 30)
    div     ecx
    mov     edx, r10d
    shr     edx, 22
    cmp     edx, eax
    ja      .wet
.nomelt:
    test    dword [ri_fl], HF_SLIDE
    jz      .snow
    ; пласт съехал волнистым краем, местами удержался (плавный шум вдоль свеса)
    mov     eax, r12d
    shr     eax, 4
    add     eax, [ri_seed]
    and     eax, 63
    mov     ecx, [ri_seed]
    shr     ecx, 8
    and     ecx, 63
    shl     ecx, 6
    add     eax, ecx
    lea     rdx, [clouds]
    movzx   eax, byte [rdx + rax]
    sub     eax, 90
    jle     .snow
    shr     eax, 1                      ; глубина съезда
    cmp     r9d, eax
    jl      .bare
    add     eax, 7
    cmp     r9d, eax
    jl      .rim
.snow:
    ; снег гуще и тоньше пятнами — крупный плавный шум
    mov     ecx, r12d
    shr     ecx, 5
    and     ecx, 63
    mov     edx, r13d
    shr     edx, 5
    and     edx, 63
    shl     edx, 6
    add     ecx, edx
    lea     rdx, [clouds]
    movzx   ecx, byte [rdx + rcx]
    shr     ecx, 5
    mov     eax, r10d
    and     eax, 7
    add     eax, ecx
    imul    eax, eax, 0x010101
    mov     ecx, 0xEAF0F6
    sub     ecx, eax
    mov     eax, ecx
    ; заструги: гребни поперёк ветра — наклон нормали; пятнами, не по всей крыше
    mov     ecx, r12d
    shr     ecx, 6
    add     ecx, 23
    and     ecx, 63
    mov     edx, r13d
    shr     edx, 6
    add     edx, 41
    and     edx, 63
    shl     edx, 6
    add     ecx, edx
    lea     rdx, [clouds]
    cmp     byte [rdx + rcx], 150
    jb      .z2
    mov     ecx, r12d
    add     ecx, r13d
    add     ecx, r13d
    mov     edx, r10d
    shr     edx, 8
    and     edx, 31
    add     ecx, edx
    shr     ecx, 5
    and     ecx, 3
    mov     edx, [rq_n0]
    sub     edx, N_RF
    lea     edx, [edx*2 + N_RV]         ; круче
    cmp     ecx, 1
    jne     .z1
    mov     [rq_n], edx
    jmp     .z2
.z1:
    cmp     ecx, 3
    jne     .z2
    inc     edx                         ; положе
    mov     [rq_n], edx
.z2:
    cmp     r8d, 12
    jae     .nr
    add     eax, 0x040404               ; снег на коньке
.nr:
    cmp     r9d, 18
    jae     .nc
    add     eax, 0x050505               ; карниз
.nc:
    ; вороньи следы — цепочка по переднему скату
    test    dword [ri_fl], HF_CROW
    jz      .nt
    mov     ecx, [rq_n0]
    cmp     ecx, [rn_f]
    jne     .nt
    ; петляет (плавный шум вдоль конька), лапы — то слева, то справа
    mov     ecx, r12d
    shr     ecx, 5
    and     ecx, 63
    mov     edx, [ri_seed]
    shr     edx, 20
    and     edx, 63
    shl     edx, 6
    add     ecx, edx
    lea     rdx, [clouds]
    movzx   ecx, byte [rdx + rcx]
    sub     ecx, 128
    mov     edx, [ri_hs]
    shr     edx, 1
    add     edx, ecx
    mov     ecx, r12d
    shr     ecx, 6
    and     ecx, 1
    imul    ecx, ecx, 22
    sub     ecx, 11
    add     edx, ecx
    sub     edx, r8d
    mov     ecx, edx
    sar     ecx, 31
    xor     edx, ecx
    sub     edx, ecx
    cmp     edx, 12
    jae     .nt
    mov     ecx, r12d
    and     ecx, 63
    cmp     ecx, 22
    jae     .nt
    sub     eax, 0x483E2C               ; отпечаток — синеватая ямка
.nt:
    mov     ecx, r10d
    shr     ecx, 24
    cmp     ecx, 3
    jae     .nsp
    or      eax, 0x41000000             ; искра
    ret
.nsp:
    or      eax, 0x01000000
    ret
.rim:
    ; срез пласта: торец снега смотрит вниз по скату
    mov     eax, 0xF0F4F8
    mov     edx, [rq_n0]
    sub     edx, N_RF
    lea     edx, [edx*2 + N_RV]
    mov     [rq_n], edx
    or      eax, 0x01000000
    ret
.bare:
    call    roof_mat
    mov     ecx, r10d
    and     ecx, 15
    jnz     .bm
    mov     eax, 0xB8C0C8               ; снежные крошки
.bm:
    or      eax, 0x01000000
    ret
.wet:
    call    roof_mat
    mov     ecx, 180                    ; мокрая кровля темнее
    call    col_mul
    mov     ecx, r10d
    shr     ecx, 25
    jnz     .wm
    or      eax, 0x40000000             ; блик на мокром
.wm:
    or      eax, 0x01000000
    ret

; фронтон: вертикальная обшивка в краске дома, чердачное окошко, «солнце»
sh_gable:
    mov     r8d, r14d
    sub     r8d, RZW                    ; над срубом
    mov     r9d, r13d
    sub     r9d, [ri_Vm]
    mov     eax, r9d
    cdq
    xor     eax, edx
    sub     eax, edx                    ; |dv|
    cmp     r8d, 1
    jg      .nt
    mov     eax, 0x01D4D8DC             ; нижняя доска
    ret
.nt:
    cmp     eax, 44
    jge     .sun
    cmp     r8d, 8
    jl      .sun
    cmp     r8d, 18
    jg      .sun
    cmp     eax, 32
    jge     .frame
    cmp     r8d, 9
    jle     .frame
    cmp     r8d, 17
    jge     .frame
    cmp     eax, 6
    jl      .frame
    cmp     r8d, 15
    jne     .glass
    test    r9d, r9d
    jns     .glass
    mov     eax, 0x41506878             ; луна в стекле
    ret
.glass:
    mov     eax, 0x01161C26
    ret
.frame:
    mov     eax, 0x01DADEE2
    ret
.sun:
    test    dword [ri_fl], HF_SUN
    jz      .boards
    mov     ecx, r9d
    sar     ecx, 4                      ; px по горизонтали
    lea     edx, [r8d - 13]
    cmp     edx, 1
    jl      .boards
    mov     eax, ecx
    imul    eax, eax
    mov     r10d, edx
    imul    r10d, r10d
    add     eax, r10d
    cmp     eax, 100
    jb      .boards
    cmp     eax, 150
    ja      .boards
    add     ecx, edx
    test    ecx, 1
    jz      .boards
    mov     eax, 0x01D0D4D8
    ret
.boards:
    mov     ecx, r13d
    sub     ecx, [ri_V0]
    imul    ecx, ecx, 1639
    shr     ecx, 16                     ; доска (40 единиц)
    imul    edx, ecx, 40
    mov     eax, r13d
    sub     eax, [ri_V0]
    sub     eax, edx                    ; в доске
    imul    ecx, ecx, 0x2C1B3C6D
    add     ecx, [ri_seed]
    shr     ecx, 27
    lea     ecx, [ecx + 206]            ; выгорание
    cmp     eax, 7
    jae     .bc
    mov     ecx, 130                    ; щель
.bc:
    mov     eax, [ri_gab]
    call    col_mul
    or      eax, 0x01000000
    ret

; торец свеса: снег, тёс, резная причелина с фестонами
sh_verge:
    mov     eax, r13d
    sub     eax, [ri_Vm]
    cdq
    xor     eax, edx
    sub     eax, edx
    mov     ecx, [ri_hs]
    sub     ecx, eax
    imul    ecx, [ri_P]
    sar     ecx, 8
    add     ecx, RZW                    ; верх тёса
    mov     r8d, r14d
    sub     r8d, ecx                    ; от верха тёса
    cmp     r8d, RTS
    jg      .none
    jl      .s1
    mov     eax, 0x01F0F4F8
    ret
.s1:
    cmp     r8d, 0
    jl      .b
    mov     eax, 0x01DCE4EC
    ret
.b:
    cmp     r8d, -RTB
    jl      .barge
    mov     eax, 0x01382C22
    ret
.barge:
    neg     r8d
    sub     r8d, RTB                    ; 1.. вниз по причелине
    mov     eax, r13d
    shr     eax, 3
    and     eax, 7
    lea     rdx, [scal8]
    movzx   eax, byte [rdx + rax]
    add     eax, 3
    cmp     r8d, eax
    jg      .none
    cmp     r8d, 2
    jne     .bc
    mov     eax, r13d
    shr     eax, 4
    and     eax, 3
    cmp     eax, 1
    jne     .bc
    mov     eax, 0x01201814             ; прорезь
    ret
.bc:
    mov     eax, [ri_barge]
    or      eax, 0x01000000
    ret
.none:
    xor     eax, eax
    ret

; полотенце — резная доска под коньком
sh_towel:
    mov     eax, r13d
    sub     eax, [ri_Vm]
    cdq
    xor     eax, edx
    sub     eax, edx                    ; |dv|
    mov     r8d, [ri_zr]
    sub     r8d, RTS + RTB
    sub     r8d, r14d                   ; вниз от тёса у конька
    cmp     r8d, 0
    jl      .none
    cmp     r8d, 22
    jg      .none
    mov     ecx, 26
    cmp     r8d, 15
    jle     .w
    lea     ecx, [r8d - 15]
    imul    ecx, ecx, -4
    add     ecx, 26
.w:
    cmp     eax, ecx
    jg      .none
    cmp     eax, 8
    jge     .c
    mov     ecx, r8d
    and     ecx, 3
    cmp     ecx, 2
    jne     .c
    mov     eax, 0x01201814
    ret
.c:
    mov     eax, [ri_barge]
    or      eax, 0x01000000
    ret
.none:
    xor     eax, eax
    ret

; кромка свеса: снежный карниз, тёс, подзор, сосульки (у тёплых домов — длинные)
sh_fascia:
    mov     ecx, r12d
    sar     ecx, 4
    add     ecx, [ri_seed]
    imul    ecx, ecx, 0x2C1B3C6D
    mov     r10d, ecx                   ; хеш столбика
    mov     r8d, [ri_ze]
    sub     r8d, r14d                   ; вниз от снега
    cmp     r8d, 0
    jl      .none
    mov     eax, r10d
    shr     eax, 31                     ; карниз неровный
    lea     edx, [eax + RTS]
    cmp     r8d, edx
    jge     .nb
    test    r8d, r8d
    mov     eax, 0x01EEF2F6
    jz      .r
    mov     eax, 0x01D2DCE6
    ret
.nb:
    sub     r8d, edx
    cmp     r8d, RTB
    jge     .pz
    mov     eax, 0x01382C22
    ret
.pz:
    sub     r8d, RTB
    mov     eax, r12d
    shr     eax, 3
    and     eax, 7
    lea     rdx, [scal8]
    movzx   eax, byte [rdx + rax]
    add     eax, 2
    cmp     r8d, eax
    jge     .ice
    cmp     r8d, 1
    jne     .pc
    mov     ecx, r12d
    shr     ecx, 4
    and     ecx, 3
    cmp     ecx, 2
    jne     .pc
    mov     eax, 0x01201814
    ret
.pc:
    mov     eax, [ri_barge]
    or      eax, 0x01000000
    ret
.ice:
    sub     r8d, eax                    ; 0 — сразу под подзором
    mov     eax, r10d
    shr     eax, 24
    test    dword [ri_fl], HF_LIT
    jz      .cold
    cmp     eax, 140
    jae     .none
    mov     eax, r10d
    shr     eax, 8
    and     eax, 7
    add     eax, 2                      ; длина
    jmp     .len
.cold:
    cmp     eax, 20
    jae     .none
    mov     eax, r10d
    shr     eax, 8
    and     eax, 1
    inc     eax
.len:
    dec     eax
    cmp     r8d, eax
    jg      .none
    je      .tip
    mov     eax, 0x01C4D4E4
    ret
.tip:
    mov     eax, 0x41DCE8F4
    ret
.none:
    xor     eax, eax
.r:
    ret

; труба: кирпич (или побелка), сажа у жерла; сверху — жерло с жаром или снежная шапка
sh_chim:
    cmp     dword [rq_face], 2
    je      .top
    mov     r8d, [ri_zt]
    sub     r8d, r14d                   ; от верха, px
    mov     eax, r13d
    sub     eax, [ri_vc]
    cmp     dword [rq_face], 0
    je      .f0
    mov     eax, r12d
    sub     eax, [ri_uc]
.f0:
    add     eax, RCH
    sar     eax, 4                      ; колонка, px
    test    dword [ri_fl], HF_LIT
    jnz     .nosn
    test    r8d, r8d
    jnz     .nosn
    mov     eax, 0x01E0E6EC             ; снег по краю холодной трубы
    ret
.nosn:
    imul    ecx, r8d, 86
    shr     ecx, 8                      ; ряд кладки (по 3 px)
    imul    edx, ecx, 3
    mov     r9d, r8d
    sub     r9d, edx                    ; строка в ряду
    and     ecx, 1
    lea     eax, [eax + ecx*2]
    mov     edx, 0x7A3A2C               ; кирпич
    mov     r10d, 0x8E8078              ; раствор
    test    dword [ri_fl], HF_WASH
    jz      .cl
    mov     edx, 0xD6D2CA
    mov     r10d, 0xB4AEA6
.cl:
    cmp     r9d, 2
    je      .mortar
    test    eax, 3
    jz      .mortar
    mov     eax, edx
    jmp     .soot
.mortar:
    mov     eax, r10d
.soot:
    test    dword [ri_fl], HF_LIT
    jz      .out
    mov     ecx, 110
    cmp     r8d, 3
    jb      .sm
    mov     ecx, 180
    cmp     r8d, 6
    jb      .sm
    jmp     .out
.sm:
    call    col_mul
.out:
    or      eax, 0x01000000
    ret
.top:
    mov     eax, r12d
    sub     eax, [ri_uc]
    cdq
    xor     eax, edx
    sub     eax, edx
    mov     ecx, r13d
    sub     ecx, [ri_vc]
    mov     edx, ecx
    sar     edx, 31
    xor     ecx, edx
    sub     ecx, edx
    cmp     eax, ecx
    cmovb   eax, ecx                    ; max(|du|, |dv|)
    test    dword [ri_fl], HF_LIT
    jz      .cap
    cmp     eax, RCH - 14
    jbe     .hole
    mov     eax, 0x01483830
    ret
.hole:
    cmp     eax, 12
    jae     .dark
    mov     eax, 0x81301208             ; жар в глубине
    ret
.dark:
    mov     eax, 0x010C0808
    ret
.cap:
    mov     eax, 0x01E6ECF2
    ret

; конёк: охлупень кончается конской головой; снег на верхних кромках
sh_horse:
    mov     eax, r12d
    sub     eax, [ri_U1]
    sub     eax, ROG - 8
    sar     eax, 4                      ; колонка
    cmp     eax, 12
    jae     .none
    mov     ecx, [ri_zr]
    add     ecx, 13
    sub     ecx, r14d                   ; строка
    cmp     ecx, 16
    jae     .none
    lea     rdx, [horse]
    movzx   r8d, word [rdx + rcx*2]
    bt      r8d, eax
    jnc     .none
    test    ecx, ecx
    jz      .sn
    movzx   r8d, word [rdx + rcx*2 - 2]
    bt      r8d, eax
    jnc     .sn
    mov     eax, 0x013A2C20
    ret
.sn:
    mov     eax, 0x01E8EEF4
    ret
.none:
    xor     eax, eax
    ret
