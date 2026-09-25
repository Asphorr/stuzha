; ----------------------------------------------------------------------------
;  fig.inc — люди и зомби объёмом: скелет из капсул в мире, поза по шагу, удару,
;  испугу, смерти; одежда по типу (ватник и ушанка, пальто и платок, тулуп, свитер,
;  телогрейка и кепка, бабка в шали). На кадр готовит у каждой фигуры список капсул
;  (fig_prep, главный поток); полосы рисуют их лучом в G-буфер (figdraw.inc):
;  у пикселя своя нормаль и своя точка в мире — луна, фонари и фонарик ложатся на
;  фигуру, как на избу, с любой стороны, куда бы она ни смотрела.
;  Единицы U, как у растений (veg.inc): тайл = 22.627 U, высота z·0.866 px
; ----------------------------------------------------------------------------
FG_MAXP     equ 28
FP_SZ       equ 64
FP_AX       equ 0                   ; начало оси, U от ступней
FP_AY       equ 4
FP_AZ       equ 8
FP_BAX      equ 12                  ; ось b − a
FP_BAY      equ 16
FP_BAZ      equ 20
FP_R        equ 24
FP_R2       equ 28
FP_BABA     equ 32                  ; |b − a|², 0 — шар
FP_IBB      equ 36
FP_BARD     equ 40                  ; (b − a)·луч
FP_MAT      equ 44                  ; цвет палитры | узор << 8 | кровь << 16
FP_X0       equ 48                  ; рамка на экране от точки ступней, px (включительно)
FP_X1       equ 52
FP_Y0       equ 56
FP_Y1       equ 60
FG_N        equ 0
FG_X0       equ 4
FG_X1       equ 8
FG_Y0       equ 12
FG_Y1       equ 16
FG_SNOW     equ 20                  ; снег на том, что смотрит вверх: порог 0..255
FG_SEED     equ 24
FG_BLOOD    equ 28                  ; пятна крови на одежде: порог 0..255
FG_COL      equ 32                  ; палитра, 12 цветов (старший байт 0xC1 — светится)
FG_HDR      equ 96
FG_SZ       equ FG_HDR + FG_MAXP*FP_SZ
; палитра
FM_SKIN     equ 0
FM_HAIR     equ 1
FM_HAT      equ 2
FM_COAT     equ 3
FM_TRIM     equ 4
FM_PANTS    equ 5
FM_BOOT     equ 6
FM_MITT     equ 7
FM_EYE      equ 8
FM_LENS     equ 9
FM_WOOD     equ 10
FM_METAL    equ 11
; узоры
FT_QUILT    equ 1 << 8              ; стёжка ватника
FT_KNIT     equ 2 << 8              ; вязка
FT_FUR      equ 3 << 8              ; мех, овчина, шерсть
FT_DOTS     equ 4 << 8              ; платок в горошек
FT_BLOOD    equ 1 << 16             ; на этом бывает кровь
; суставы: локально (вперёд f, вправо s, вверх u) -> мир (x, y, z)
J_PEL       equ 0
J_HIPL      equ 1
J_HIPR      equ 2
J_KNL       equ 3
J_KNR       equ 4
J_ANL       equ 5
J_ANR       equ 6
J_TOL       equ 7
J_TOR       equ 8
J_WST       equ 9
J_CHM       equ 10
J_CHT       equ 11
J_NCK       equ 12
J_HED       equ 13
J_SHL       equ 14
J_SHR       equ 15
J_ELL       equ 16
J_ELR       equ 17
J_HAL       equ 18
J_HAR       equ 19
J_X0        equ 20                  ; дальше — по одежде и в руках
NJ          equ 40
; типы
FZ_PLAYER   equ 6

; figdraw.inc: клетка точки фигуры по оси — (координата U + 64)·масштаб -> eax (портит xmm0)
%macro FDCELL 2
    movss   xmm0, [%1]
    addss   xmm0, [vk_64]
    mulss   xmm0, [%2]
    cvttss2si eax, xmm0
%endmacro

section .text

; ---------------------------------------------------------------- суставы
; ecx = сустав, xmm0-2 = f, s, u (портит rax)
fj_put:
    shl     ecx, 4
    lea     rax, [fj]
    movss   [rax + rcx], xmm0
    movss   [rax + rcx + 4], xmm1
    movss   [rax + rcx + 8], xmm2
    ret
; ecx = сустав, edx = от какого, xmm0-2 = смещение (портит rax, ecx, edx)
fj_off:
    shl     ecx, 4
    shl     edx, 4
    lea     rax, [fj]
    addss   xmm0, [rax + rdx]
    addss   xmm1, [rax + rdx + 4]
    addss   xmm2, [rax + rdx + 8]
    movss   [rax + rcx], xmm0
    movss   [rax + rcx + 4], xmm1
    movss   [rax + rcx + 8], xmm2
    ret
; конечность: ecx = куда, edx = откуда, xmm0 = длина, xmm1 = α (0 — вниз, π/2 —
; вперёд, π — вверх), xmm2 = вбок (+ вправо, доля), xmm3 = рыск ψ (к правому боку +):
; L·(sin α·cos ψ, sin α·sin ψ + вбок, −cos α)
fj_limb:
    movss   [fl_L], xmm0
    movss   [fl_side], xmm2
    movss   [fl_psi], xmm3
    mov     [fl_dst], ecx
    mov     [fl_src], edx
    movaps  xmm0, xmm1
    call    vg_cs
    movss   [fl_ca], xmm0
    movss   [fl_sa], xmm1
    movss   xmm0, [fl_psi]
    call    vg_cs
    mulss   xmm0, [fl_sa]
    mulss   xmm1, [fl_sa]
    addss   xmm1, [fl_side]
    movss   xmm2, [fl_ca]
    xorps   xmm2, [f_signmask]
    mulss   xmm0, [fl_L]
    mulss   xmm1, [fl_L]
    mulss   xmm2, [fl_L]
    mov     ecx, [fl_dst]
    mov     edx, [fl_src]
    jmp     fj_off
; ecx = куда, edx = откуда, r8d = от какого сустава направление (edx − r8d), xmm0 = длина:
; продолжение отрезка (инструмент в руке)
fj_ext:
    push    rcx
    shl     edx, 4
    shl     r8d, 4
    lea     rax, [fj]
    movss   xmm1, [rax + rdx]
    subss   xmm1, [rax + r8]
    movss   xmm2, [rax + rdx + 4]
    subss   xmm2, [rax + r8 + 4]
    movss   xmm3, [rax + rdx + 8]
    subss   xmm3, [rax + r8 + 8]
    movaps  xmm4, xmm1
    mulss   xmm4, xmm1
    movaps  xmm5, xmm2
    mulss   xmm5, xmm2
    addss   xmm4, xmm5
    movaps  xmm5, xmm3
    mulss   xmm5, xmm3
    addss   xmm4, xmm5
    maxss   xmm4, [vk_1em4]
    sqrtss  xmm4, xmm4
    divss   xmm0, xmm4
    mulss   xmm1, xmm0
    mulss   xmm2, xmm0
    mulss   xmm3, xmm0
    movaps  xmm0, xmm1
    movaps  xmm1, xmm2
    movaps  xmm2, xmm3
    pop     rcx
    shr     edx, 4
    jmp     fj_off

; fj -> fw: стоя — f·F + s·Rt + u·Up, лёжа (на спине, головой по F) — u·F + s·Rt + f·Up;
; f, s — ×fz_w, u — ×fz_h
fj_world:
    lea     r8, [fj]
    lea     r9, [fw]
    mov     ecx, NJ
.j:
    movss   xmm0, [r8]
    mulss   xmm0, [fz_w]                ; f
    movss   xmm1, [r8 + 4]
    mulss   xmm1, [fz_w]                ; s
    movss   xmm2, [r8 + 8]
    mulss   xmm2, [fz_h]                ; u
    cmp     dword [fz_lying], 0
    je      .st
    movaps  xmm3, xmm0
    addss   xmm3, [fz_lift]
    movaps  xmm0, xmm2                  ; вдоль тела — по F
    movaps  xmm2, xmm3                  ; грудью — вверх
.st:
    ; x = f·Fx + s·Fy, y = f·Fy − s·Fx
    movaps  xmm3, xmm0
    mulss   xmm3, [fz_fx]
    movaps  xmm4, xmm1
    mulss   xmm4, [fz_fy]
    addss   xmm3, xmm4
    movss   [r9], xmm3
    movaps  xmm3, xmm0
    mulss   xmm3, [fz_fy]
    movaps  xmm4, xmm1
    mulss   xmm4, [fz_fx]
    subss   xmm3, xmm4
    movss   [r9 + 4], xmm3
    movss   [r9 + 8], xmm2
    add     r8, 16
    add     r9, 16
    dec     ecx
    jnz     .j
    ret

; капсула: ecx = сустав a, edx = сустав b (мир), xmm0 = радиус (×fz_w), r8d = материал
fp_emit:
    push    rbx
    sub     rsp, 32
    movdqu  [rsp], xmm6
    movdqu  [rsp + 16], xmm7
    mov     rbx, [fg_rec]
    mov     eax, [rbx + FG_N]
    cmp     eax, FG_MAXP
    jae     .out
    inc     dword [rbx + FG_N]
    shl     eax, 6
    lea     r9, [rbx + rax + FG_HDR]
    mov     [r9 + FP_MAT], r8d
    mulss   xmm0, [fz_w]
    movss   [r9 + FP_R], xmm0
    movaps  xmm1, xmm0
    mulss   xmm1, xmm0
    movss   [r9 + FP_R2], xmm1
    shl     ecx, 4
    shl     edx, 4
    lea     r10, [fw]
    movss   xmm1, [r10 + rcx]
    movss   xmm2, [r10 + rcx + 4]
    movss   xmm3, [r10 + rcx + 8]
    movss   [r9 + FP_AX], xmm1
    movss   [r9 + FP_AY], xmm2
    movss   [r9 + FP_AZ], xmm3
    movss   xmm4, [r10 + rdx]
    subss   xmm4, xmm1
    movss   xmm5, [r10 + rdx + 4]
    subss   xmm5, xmm2
    movss   xmm6, [r10 + rdx + 8]
    subss   xmm6, xmm3
    movss   [r9 + FP_BAX], xmm4
    movss   [r9 + FP_BAY], xmm5
    movss   [r9 + FP_BAZ], xmm6
    ; |ba|², 1/|ba|², ba·луч (луч = −T = −(0.612, 0.612, 0.5))
    movaps  xmm7, xmm4
    mulss   xmm7, xmm4
    movaps  xmm0, xmm5
    mulss   xmm0, xmm5
    addss   xmm7, xmm0
    movaps  xmm0, xmm6
    mulss   xmm0, xmm6
    addss   xmm7, xmm0
    xorps   xmm0, xmm0
    comiss  xmm7, [vk_0_01]
    ja      .cap
    xorps   xmm7, xmm7                  ; шар
    jmp     .bb
.cap:
    movss   xmm0, [f_1]
    divss   xmm0, xmm7
.bb:
    movss   [r9 + FP_BABA], xmm7
    movss   [r9 + FP_IBB], xmm0
    movaps  xmm0, xmm4
    addss   xmm0, xmm5
    mulss   xmm0, [vk_c6]
    movaps  xmm7, xmm6
    mulss   xmm7, [f_0_5]
    addss   xmm0, xmm7
    xorps   xmm0, [f_signmask]
    movss   [r9 + FP_BARD], xmm0
    ; на экране: su = (x − y)·√½, sv = (x + y)·√⅛ − z·cos 30°
    movaps  xmm0, xmm1
    subss   xmm0, xmm2
    mulss   xmm0, [vk_s2]               ; su(a)
    movaps  xmm7, xmm1
    addss   xmm7, xmm2
    mulss   xmm7, [vk_s8]
    movaps  xmm1, xmm3
    mulss   xmm1, [vk_c30]
    subss   xmm7, xmm1                  ; sv(a)
    addss   xmm4, [r9 + FP_AX]          ; b
    addss   xmm5, [r9 + FP_AY]
    addss   xmm6, [r9 + FP_AZ]
    movaps  xmm1, xmm4
    subss   xmm1, xmm5
    mulss   xmm1, [vk_s2]               ; su(b)
    addss   xmm4, xmm5
    mulss   xmm4, [vk_s8]
    mulss   xmm6, [vk_c30]
    subss   xmm4, xmm6                  ; sv(b)
    movss   xmm5, [r9 + FP_R]
    movaps  xmm2, xmm0
    minss   xmm2, xmm1
    subss   xmm2, xmm5
    VFLOOR  eax, xmm2, xmm3
    mov     [r9 + FP_X0], eax
    cmp     eax, [rbx + FG_X0]
    jge     .b1
    mov     [rbx + FG_X0], eax
.b1:
    maxss   xmm0, xmm1
    addss   xmm0, xmm5
    VFLOOR  eax, xmm0, xmm3
    mov     [r9 + FP_X1], eax
    cmp     eax, [rbx + FG_X1]
    jle     .b2
    mov     [rbx + FG_X1], eax
.b2:
    movaps  xmm2, xmm7
    minss   xmm2, xmm4
    subss   xmm2, xmm5
    VFLOOR  eax, xmm2, xmm3
    mov     [r9 + FP_Y0], eax
    cmp     eax, [rbx + FG_Y0]
    jge     .b3
    mov     [rbx + FG_Y0], eax
.b3:
    maxss   xmm7, xmm4
    addss   xmm7, xmm5
    VFLOOR  eax, xmm7, xmm3
    mov     [r9 + FP_Y1], eax
    cmp     eax, [rbx + FG_Y1]
    jle     .out
    mov     [rbx + FG_Y1], eax
.out:
    movdqu  xmm6, [rsp]
    movdqu  xmm7, [rsp + 16]
    add     rsp, 32
    pop     rbx
    ret

; ---------------------------------------------------------------- поза
; из fz_* -> fj (локально): ноги по фазе шага, таз — на прямой ноге, корпус с
; наклоном и креном, руки, голова
fz_skel:
    sub     rsp, 40
    movss   xmm0, [fz_ph]
    call    vg_cs
    movss   [fz_cph], xmm0
    movss   [fz_sph], xmm1
    ; бёдра: tL = A·sin φ, tR = −A·sin φ; колени гнутся, пока нога идёт вперёд. На бегу
    ; (fz_run 0..1) пятка забрасывается выше (×1.1 -> ×2.4), опорное колено пружинит
    movss   xmm0, [fz_A]
    mulss   xmm0, [fz_sph]
    movss   [fz_tL], xmm0
    xorps   xmm0, [f_signmask]
    movss   [fz_tR], xmm0
    movss   xmm2, [fz_run]
    mulss   xmm2, [vk_1_3]
    addss   xmm2, [vk_1_1]
    movss   xmm1, [fz_A]
    mulss   xmm1, xmm2
    movss   xmm2, [fz_run]
    mulss   xmm2, [vk_0_28f]
    addss   xmm2, [vk_0_12]             ; колено опорной
    movss   xmm0, [fz_cph]
    maxss   xmm0, [f_0]
    mulss   xmm0, xmm1
    addss   xmm0, xmm2
    movss   [fz_kL], xmm0
    movss   xmm0, [fz_cph]
    xorps   xmm0, [f_signmask]
    maxss   xmm0, [f_0]
    mulss   xmm0, xmm1
    addss   xmm0, xmm2
    movss   [fz_kR], xmm0
    ; хромая нога: шаг вдвое короче, колено не гнётся
    mov     eax, [fz_limp]
    test    eax, eax
    jz      .nl
    lea     rcx, [fz_tL]
    lea     rdx, [fz_kL]
    cmp     eax, 1
    je      .lp
    lea     rcx, [fz_tR]
    lea     rdx, [fz_kR]
.lp:
    movss   xmm0, [rcx]
    mulss   xmm0, [vk_0_45]
    movss   [rcx], xmm0
    mov     dword [rdx], __float32__(0.05)
.nl:
    ; таз: на высоте прямой ноги
    movss   xmm0, [fz_tL]
    call    fz_legh
    movss   [fz_hL], xmm0
    movss   xmm0, [fz_tR]
    movss   xmm1, [fz_kR]
    movss   [fz_kt], xmm1
    call    fz_legh2
    maxss   xmm0, [fz_hL]
    addss   xmm0, [vk_1_1]
    ; бег: полёт — таз выше опорной ноги, пока обе ноги врозь (15.3 + 1.4·|sin φ|)
    movss   xmm1, [fz_sph]
    andps   xmm1, [f_absmask]
    mulss   xmm1, [vk_1_4]
    addss   xmm1, [vk_15_3f]
    subss   xmm1, xmm0
    maxss   xmm1, [f_0]
    mulss   xmm1, [fz_run]
    addss   xmm0, xmm1
    movaps  xmm2, xmm0
    xorps   xmm0, xmm0
    movss   xmm1, [fz_lat]
    mov     ecx, J_PEL
    call    fj_put
    ; ноги
    xorps   xmm0, xmm0
    movss   xmm1, [vk_m1_9]
    xorps   xmm2, xmm2
    mov     ecx, J_HIPL
    mov     edx, J_PEL
    call    fj_off
    xorps   xmm0, xmm0
    movss   xmm1, [vk_1_9f]
    xorps   xmm2, xmm2
    mov     ecx, J_HIPR
    mov     edx, J_PEL
    call    fj_off
    movss   xmm0, [vk_7_6]
    movss   xmm1, [fz_tL]
    xorps   xmm2, xmm2
    xorps   xmm3, xmm3
    mov     ecx, J_KNL
    mov     edx, J_HIPL
    call    fj_limb
    movss   xmm1, [fz_tL]
    subss   xmm1, [fz_kL]
    movss   xmm0, [vk_7_4]
    xorps   xmm2, xmm2
    xorps   xmm3, xmm3
    mov     ecx, J_ANL
    mov     edx, J_KNL
    call    fj_limb
    movss   xmm0, [vk_7_6]
    movss   xmm1, [fz_tR]
    xorps   xmm2, xmm2
    xorps   xmm3, xmm3
    mov     ecx, J_KNR
    mov     edx, J_HIPR
    call    fj_limb
    movss   xmm1, [fz_tR]
    subss   xmm1, [fz_kR]
    movss   xmm0, [vk_7_4]
    xorps   xmm2, xmm2
    xorps   xmm3, xmm3
    mov     ecx, J_ANR
    mov     edx, J_KNR
    call    fj_limb
    movss   xmm0, [vk_2_3f]
    xorps   xmm1, xmm1
    movss   xmm2, [vk_m0_35]
    mov     ecx, J_TOL
    mov     edx, J_ANL
    call    fj_off
    movss   xmm0, [vk_2_3f]
    xorps   xmm1, xmm1
    movss   xmm2, [vk_m0_35]
    mov     ecx, J_TOR
    mov     edx, J_ANR
    call    fj_off
    ; корпус: ось (sin наклона, sin крена, cos наклона)
    movss   xmm0, [fz_lean]
    call    vg_cs
    movss   [fz_tu], xmm0
    movss   [fz_tf], xmm1
    movss   xmm0, [fz_roll]
    call    vg_cs
    movss   [fz_ts], xmm1
    mulss   xmm0, [fz_tu]
    movss   [fz_tu], xmm0
    xorps   xmm0, xmm0
    xorps   xmm1, xmm1
    movss   xmm2, [vk_2_2]
    mov     ecx, J_WST
    mov     edx, J_PEL
    call    fj_off
    movss   xmm3, [vk_4_2]
    mov     ecx, J_CHM
    mov     edx, J_WST
    call    fz_axis
    movss   xmm3, [vk_8_4]
    mov     ecx, J_CHT
    mov     edx, J_WST
    call    fz_axis
    movss   xmm3, [vk_1_5]
    mov     ecx, J_NCK
    mov     edx, J_CHT
    call    fz_axis
    ; голова: ось корпуса + кивок и наклон вбок
    movss   xmm0, [fz_tf]
    addss   xmm0, [fz_hnod]
    movss   xmm1, [fz_ts]
    addss   xmm1, [fz_htilt]
    movss   xmm2, [fz_tu]
    mulss   xmm0, [vk_2_3f]
    mulss   xmm1, [vk_2_3f]
    mulss   xmm2, [vk_2_3f]
    mov     ecx, J_HED
    mov     edx, J_NCK
    call    fj_off
    ; плечи
    xorps   xmm0, xmm0
    movss   xmm1, [vk_m3_4]
    movss   xmm2, [vk_m0_5]
    mov     ecx, J_SHL
    mov     edx, J_CHT
    call    fj_off
    xorps   xmm0, xmm0
    movss   xmm1, [vk_3_4]
    movss   xmm2, [vk_m0_5]
    mov     ecx, J_SHR
    mov     edx, J_CHT
    call    fj_off
    ; руки
    movss   xmm0, [vk_5_6]
    movss   xmm1, [fz_aL]
    movss   xmm2, [fz_sL]
    movss   xmm3, [fz_yL]
    mov     ecx, J_ELL
    mov     edx, J_SHL
    call    fj_limb
    movss   xmm1, [fz_aL]
    addss   xmm1, [fz_bL]
    movss   xmm0, [vk_5]
    movss   xmm2, [fz_sL]
    movss   xmm3, [fz_yL]
    mov     ecx, J_HAL
    mov     edx, J_ELL
    call    fj_limb
    movss   xmm0, [vk_5_6]
    movss   xmm1, [fz_aR]
    movss   xmm2, [fz_sR]
    movss   xmm3, [fz_yR]
    mov     ecx, J_ELR
    mov     edx, J_SHR
    call    fj_limb
    movss   xmm1, [fz_aR]
    addss   xmm1, [fz_bR]
    movss   xmm0, [vk_5]
    movss   xmm2, [fz_sR]
    movss   xmm3, [fz_yR]
    mov     ecx, J_HAR
    mov     edx, J_ELR
    call    fj_limb
    add     rsp, 40
    ret
; xmm3 = длина: ecx = edx + длина·ось корпуса
fz_axis:
    movss   xmm0, [fz_tf]
    mulss   xmm0, xmm3
    movss   xmm1, [fz_ts]
    mulss   xmm1, xmm3
    movss   xmm2, [fz_tu]
    mulss   xmm2, xmm3
    jmp     fj_off
; xmm0 = бедро tL -> xmm0 = высота ноги (колено fz_kL)
fz_legh:
    movss   xmm1, [fz_kL]
    movss   [fz_kt], xmm1
; xmm0 = бедро, fz_kt = колено -> xmm0 = 7.6 cos t + 7.4 cos(t − k)
fz_legh2:
    sub     rsp, 40
    movss   [fz_tt], xmm0
    call    vg_cs
    mulss   xmm0, [vk_7_6]
    movss   [fz_hh], xmm0
    movss   xmm0, [fz_tt]
    subss   xmm0, [fz_kt]
    call    vg_cs
    mulss   xmm0, [vk_7_4]
    addss   xmm0, [fz_hh]
    add     rsp, 40
    ret

; ---------------------------------------------------------------- одежда
; по типу — лишние точки (локально, до fj_world)
fz_extras:
    sub     rsp, 40
    mov     eax, [fz_type]
    ; шапка / платок / волосы / кепка — точка над головой
    movss   xmm0, [vk_m0_15]
    xorps   xmm1, xmm1
    movss   xmm2, [vk_0_9]
    cmp     eax, 1
    je      .scarf
    cmp     eax, 5
    je      .scarf
    cmp     eax, 2
    je      .hair
    cmp     eax, 3
    je      .hair
    jmp     .h1
.scarf:
    movss   xmm0, [vk_m0_45]
    movss   xmm2, [vk_0_35]
    jmp     .h1
.hair:
    movss   xmm0, [vk_m0_4]
    movss   xmm2, [vk_0_4]
.h1:
    mov     ecx, J_X0
    mov     edx, J_HED
    call    fj_off
    ; уши ушанки (или узел платка, козырёк кепки)
    mov     eax, [fz_type]
    cmp     eax, 4
    je      .brim
    cmp     eax, 1
    je      .knot
    movss   xmm0, [vk_m0_2]
    movss   xmm1, [vk_m2_1]
    movss   xmm2, [vk_0_4]
    mov     ecx, J_X0 + 1
    mov     edx, J_HED
    call    fj_off
    movss   xmm0, [vk_m0_1]
    movss   xmm1, [vk_m2_2]
    movss   xmm2, [vk_m1_6]
    mov     ecx, J_X0 + 2
    mov     edx, J_HED
    call    fj_off
    movss   xmm0, [vk_m0_2]
    movss   xmm1, [vk_2_1]
    movss   xmm2, [vk_0_4]
    mov     ecx, J_X0 + 3
    mov     edx, J_HED
    call    fj_off
    movss   xmm0, [vk_m0_1]
    movss   xmm1, [vk_2_2]
    movss   xmm2, [vk_m1_6]
    mov     ecx, J_X0 + 4
    mov     edx, J_HED
    call    fj_off
    jmp     .eyes
.brim:
    movss   xmm0, [f_1]
    movss   xmm1, [vk_m1_1]
    movss   xmm2, [vk_1_4]
    mov     ecx, J_X0 + 1
    mov     edx, J_HED
    call    fj_off
    movss   xmm0, [f_1]
    movss   xmm1, [vk_1_1]
    movss   xmm2, [vk_1_4]
    mov     ecx, J_X0 + 2
    mov     edx, J_HED
    call    fj_off
    jmp     .eyes
.knot:
    movss   xmm0, [vk_1_2]
    xorps   xmm1, xmm1
    movss   xmm2, [vk_m2]
    mov     ecx, J_X0 + 1
    mov     edx, J_HED
    call    fj_off
.eyes:
    ; глаза
    movss   xmm0, [vk_2_05]
    movss   xmm1, [vk_m0_85]
    movss   xmm2, [vk_0_3]
    mov     ecx, J_X0 + 5
    mov     edx, J_HED
    call    fj_off
    movss   xmm0, [vk_2_05]
    movss   xmm1, [vk_0_85]
    movss   xmm2, [vk_0_3]
    mov     ecx, J_X0 + 6
    mov     edx, J_HED
    call    fj_off
    ; подол длинного пальто, тулупа, бабкиного пальто
    xorps   xmm0, xmm0
    xorps   xmm1, xmm1
    movss   xmm2, [f_1]
    mov     ecx, J_X0 + 7
    mov     edx, J_PEL
    call    fj_off
    movss   xmm0, [fz_lean]
    mulss   xmm0, [vk_2]
    addss   xmm0, [vk_0_25]
    xorps   xmm1, xmm1
    movss   xmm2, [vk_m6_2]
    cmp     dword [fz_type], 5
    jne     .sk
    movss   xmm2, [vk_m8]
.sk:
    mov     ecx, J_X0 + 8
    mov     edx, J_PEL
    call    fj_off
    ; воротник, шаль по плечам
    xorps   xmm0, xmm0
    xorps   xmm1, xmm1
    movss   xmm2, [vk_0_8]
    mov     ecx, J_X0 + 9
    mov     edx, J_SHL
    call    fj_off
    xorps   xmm0, xmm0
    xorps   xmm1, xmm1
    movss   xmm2, [vk_0_8]
    mov     ecx, J_X0 + 10
    mov     edx, J_SHR
    call    fj_off
    ; в руках у игрока: лопата — черенок в правом кулаке (fz_sth, fz_shg), штык
    ; плоский, чуть загнут; фонарик — продолжением левого предплечья
    cmp     dword [fz_type], FZ_PLAYER
    jne     .out
    call    fz_shovel
    movss   xmm0, [vk_2_2]
    mov     ecx, J_X0 + 13
    mov     edx, J_HAL
    mov     r8d, J_ELL
    call    fj_ext
    movss   xmm0, [f_0_5]
    mov     ecx, J_X0 + 14
    mov     edx, J_X0 + 13
    mov     r8d, J_HAL
    call    fj_ext
.out:
    add     rsp, 40
    ret

; лопата от правого кулака (локально): J_X0+11 — верх черенка, +12 — низ у штыка;
; штык плоский (две капсулы рядом): +15, +16 — у черенка, +17, +18 — край, загнут вниз
fz_shovel:
    push    rbx
    sub     rsp, 32
    lea     rbx, [fj]
    cmp     dword [fz_shm], 0
    je      .th
    ; по предплечью: кулак − локоть
    movss   xmm0, [rbx + J_HAR*16]
    subss   xmm0, [rbx + J_ELR*16]
    movss   xmm1, [rbx + J_HAR*16 + 4]
    subss   xmm1, [rbx + J_ELR*16 + 4]
    movss   xmm2, [rbx + J_HAR*16 + 8]
    subss   xmm2, [rbx + J_ELR*16 + 8]
    jmp     .nd
.th:
    movss   xmm0, [fz_sth]
    call    vg_cs                       ; xmm0 = cos θ — вперёд, xmm1 = sin θ
    movaps  xmm2, xmm1
    xorps   xmm2, [f_signmask]          ; вниз
    movss   xmm1, [fz_shs]
.nd:
    call    fz_n3
    movss   [fz_d], xmm0
    movss   [fz_d + 4], xmm1
    movss   [fz_d + 8], xmm2
    subss   xmm2, [vk_0_35]             ; штык загнут вниз
    call    fz_n3
    mulss   xmm0, [vk_3_4f]
    mulss   xmm1, [vk_3_4f]
    mulss   xmm2, [vk_3_4f]
    movss   [fz_db], xmm0
    movss   [fz_db + 4], xmm1
    movss   [fz_db + 8], xmm2
    ; вбок: (d.s, −d.f, 0), 0.75 — капсулы штыка внахлёст, сплошной лист
    movss   xmm0, [fz_d + 4]
    movss   xmm1, [fz_d]
    xorps   xmm1, [f_signmask]
    xorps   xmm2, xmm2
    call    fz_n3
    mulss   xmm0, [vk_0_75f]
    mulss   xmm1, [vk_0_75f]
    movss   [fz_sd], xmm0
    movss   [fz_sd + 4], xmm1
    mov     dword [fz_sd + 8], 0
    ; верх: кулак − g·L·d, низ: кулак + (1 − g)·L·d (L = 13)
    movss   xmm3, [fz_shg]
    mulss   xmm3, [vk_13f]
    xorps   xmm3, [f_signmask]
    call    .dl
    mov     ecx, J_X0 + 11
    mov     edx, J_HAR
    call    fj_off
    movss   xmm3, [f_1]
    subss   xmm3, [fz_shg]
    mulss   xmm3, [vk_13f]
    call    .dl
    mov     ecx, J_X0 + 12
    mov     edx, J_HAR
    call    fj_off
    ; штык
    movss   xmm0, [fz_sd]
    movss   xmm1, [fz_sd + 4]
    xorps   xmm2, xmm2
    mov     ecx, J_X0 + 15
    mov     edx, J_X0 + 12
    call    fj_off
    movss   xmm0, [fz_sd]
    xorps   xmm0, [f_signmask]
    movss   xmm1, [fz_sd + 4]
    xorps   xmm1, [f_signmask]
    xorps   xmm2, xmm2
    mov     ecx, J_X0 + 16
    mov     edx, J_X0 + 12
    call    fj_off
    movss   xmm0, [fz_db]
    addss   xmm0, [fz_sd]
    movss   xmm1, [fz_db + 4]
    addss   xmm1, [fz_sd + 4]
    movss   xmm2, [fz_db + 8]
    mov     ecx, J_X0 + 17
    mov     edx, J_X0 + 12
    call    fj_off
    movss   xmm0, [fz_db]
    subss   xmm0, [fz_sd]
    movss   xmm1, [fz_db + 4]
    subss   xmm1, [fz_sd + 4]
    movss   xmm2, [fz_db + 8]
    mov     ecx, J_X0 + 18
    mov     edx, J_X0 + 12
    call    fj_off
    add     rsp, 32
    pop     rbx
    ret
; xmm3 = длина -> xmm0-2 = fz_d·длина
.dl:
    movss   xmm0, [fz_d]
    mulss   xmm0, xmm3
    movss   xmm1, [fz_d + 4]
    mulss   xmm1, xmm3
    movss   xmm2, [fz_d + 8]
    mulss   xmm2, xmm3
    ret
; xmm0-2 -> единичный (портит xmm3, xmm4)
fz_n3:
    movaps  xmm3, xmm0
    mulss   xmm3, xmm0
    movaps  xmm4, xmm1
    mulss   xmm4, xmm1
    addss   xmm3, xmm4
    movaps  xmm4, xmm2
    mulss   xmm4, xmm2
    addss   xmm3, xmm4
    maxss   xmm3, [vk_1em4]
    sqrtss  xmm3, xmm3
    movss   xmm4, [f_1]
    divss   xmm4, xmm3
    mulss   xmm0, xmm4
    mulss   xmm1, xmm4
    mulss   xmm2, xmm4
    ret

; капсулы фигуры по типу (fw уже в мире)
%macro FPE 4
    mov     ecx, %1
    mov     edx, %2
    movss   xmm0, [%3]
    mov     r8d, %4
    call    fp_emit
%endmacro
fz_emit:
    push    rbx
    push    rsi
    sub     rsp, 40
    mov     ebx, [fz_type]
    ; одежда: узор и кровь по типу
    mov     esi, FM_COAT | FT_QUILT
    cmp     ebx, 1
    je      .cp
    cmp     ebx, 5
    je      .cp
    cmp     ebx, 2
    je      .cf
    cmp     ebx, 3
    jne     .c1
    mov     esi, FM_COAT | FT_KNIT
    jmp     .c1
.cf:
    mov     esi, FM_COAT | FT_FUR
    jmp     .c1
.cp:
    mov     esi, FM_COAT
.c1:
    cmp     ebx, FZ_PLAYER
    je      .c2
    or      esi, FT_BLOOD
.c2:
    ; корпус
    FPE     J_WST, J_CHM, fr_torso1, esi
    FPE     J_CHM, J_NCK, fr_torso2, esi
    FPE     J_HIPL, J_HIPR, fr_hips, esi
    ; подол длинной одежды
    cmp     ebx, 1
    je      .sk
    cmp     ebx, 2
    je      .sk
    cmp     ebx, 5
    jne     .nsk
.sk:
    FPE     J_X0 + 7, J_X0 + 8, fr_skirt, esi
.nsk:
    ; ноги: валенки до колен у ватников, тулупа, игрока
    mov     esi, FM_PANTS
    cmp     ebx, FZ_PLAYER
    je      .p1
    or      esi, FT_BLOOD
.p1:
    FPE     J_HIPL, J_KNL, fr_thigh, esi
    FPE     J_HIPR, J_KNR, fr_thigh, esi
    mov     r8d, esi
    cmp     ebx, 0
    je      .vb
    cmp     ebx, 2
    je      .vb
    cmp     ebx, FZ_PLAYER
    jne     .vn
.vb:
    mov     esi, FM_BOOT
.vn:
    FPE     J_KNL, J_ANL, fr_shin, esi
    FPE     J_KNR, J_ANR, fr_shin, esi
    FPE     J_ANL, J_TOL, fr_foot, FM_BOOT
    FPE     J_ANR, J_TOR, fr_foot, FM_BOOT
    ; руки
    mov     esi, FM_COAT | FT_QUILT
    cmp     ebx, 3
    jne     .a0
    mov     esi, FM_COAT | FT_KNIT
.a0:
    cmp     ebx, 2
    jne     .a1
    mov     esi, FM_COAT | FT_FUR
.a1:
    cmp     ebx, 1
    je      .ap
    cmp     ebx, 5
    jne     .a2
.ap:
    mov     esi, FM_COAT
.a2:
    FPE     J_SHL, J_ELL, fr_uarm, esi
    FPE     J_SHR, J_ELR, fr_uarm, esi
    FPE     J_ELL, J_HAL, fr_farm, esi
    FPE     J_ELR, J_HAR, fr_farm, esi
    mov     esi, FM_SKIN
    cmp     ebx, 0
    je      .mt
    cmp     ebx, FZ_PLAYER
    jne     .hd
.mt:
    mov     esi, FM_MITT
.hd:
    FPE     J_HAL, J_HAL, fr_hand, esi
    FPE     J_HAR, J_HAR, fr_hand, esi
    ; шея, голова
    FPE     J_CHT, J_NCK, fr_neck, FM_SKIN
    FPE     J_HED, J_HED, fr_head, FM_SKIN
    ; по типу: шапка, платок, волосы, кепка, шаль, воротник
    cmp     ebx, 1
    je      .t1
    cmp     ebx, 2
    je      .t2
    cmp     ebx, 3
    je      .t3
    cmp     ebx, 4
    je      .t4
    cmp     ebx, 5
    je      .t5
    ; ушанка (ватник, игрок)
    FPE     J_X0, J_X0, fr_hat, FM_HAT | FT_FUR
    FPE     J_X0 + 1, J_X0 + 2, fr_flap, FM_HAT | FT_FUR
    FPE     J_X0 + 3, J_X0 + 4, fr_flap, FM_HAT | FT_FUR
    jmp     .eyes
.t1:
    FPE     J_X0, J_X0, fr_scarf, FM_HAT | FT_DOTS
    FPE     J_X0 + 1, J_X0 + 1, fr_knot, FM_HAT | FT_DOTS
    jmp     .eyes
.t2:
    FPE     J_X0 + 9, J_X0 + 10, fr_collar, FM_TRIM | FT_FUR
    cmp     dword [fz_bald], 0
    jne     .eyes
.t3:
    FPE     J_X0, J_X0, fr_hair, FM_HAIR | FT_FUR
    jmp     .eyes
.t4:
    FPE     J_X0, J_X0, fr_cap, FM_HAT
    FPE     J_X0 + 1, J_X0 + 2, fr_brim, FM_HAT
    jmp     .eyes
.t5:
    FPE     J_X0, J_X0, fr_scarf, FM_HAT | FT_FUR
    FPE     J_X0 + 9, J_X0 + 10, fr_shawl, FM_HAT | FT_FUR
.eyes:
    cmp     ebx, FZ_PLAYER
    je      .tools
    FPE     J_X0 + 5, J_X0 + 5, fr_eye, FM_EYE
    FPE     J_X0 + 6, J_X0 + 6, fr_eye, FM_EYE
    jmp     .out
.tools:
    cmp     dword [fz_lying], 0
    jne     .out
    FPE     J_X0 + 11, J_X0 + 12, fr_handle, FM_WOOD
    FPE     J_X0 + 15, J_X0 + 17, fr_blade, FM_METAL
    FPE     J_X0 + 16, J_X0 + 18, fr_blade, FM_METAL
    FPE     J_HAL, J_X0 + 13, fr_flash, FM_MITT
    FPE     J_X0 + 14, J_X0 + 14, fr_lens, FM_LENS
.out:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- фигура целиком
; rbx = сущность, r14d = номер (−1 — игрок), rdi = запись фигуры
fig_one:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 32
    mov     [fg_rec], rdi
    xor     eax, eax
    mov     [rdi + FG_N], eax
    mov     [rdi + FG_SNOW], eax
    mov     [rdi + FG_BLOOD], eax
    mov     dword [rdi + FG_X0], 1000
    mov     dword [rdi + FG_X1], -1000
    mov     dword [rdi + FG_Y0], 1000
    mov     dword [rdi + FG_Y1], -1000
    ; куда смотрит
    movss   xmm0, [rbx + E_FX]
    movss   xmm1, [rbx + E_FY]
    movaps  xmm2, xmm0
    mulss   xmm2, xmm0
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    addss   xmm2, xmm3
    comiss  xmm2, [vk_1em4]
    ja      .fok
    movss   xmm0, [f_1]
    xorps   xmm1, xmm1
    movss   xmm2, [f_1]
.fok:
    sqrtss  xmm2, xmm2
    divss   xmm0, xmm2
    divss   xmm1, xmm2
    movss   [fz_fx], xmm0
    movss   [fz_fy], xmm1
    ; поза по умолчанию
    xor     eax, eax
    lea     rcx, [fz_pose]
    mov     edx, FZ_POSEN
.z:
    mov     [rcx], eax
    add     rcx, 4
    dec     edx
    jnz     .z
    mov     dword [fz_w], __float32__(1.0)
    mov     dword [fz_h], __float32__(1.0)
    mov     dword [fz_lying], 0
    mov     dword [fz_lift], __float32__(3.4)
    mov     dword [fz_bL], __float32__(0.25)
    mov     dword [fz_bR], __float32__(0.25)
    mov     dword [fz_sL], __float32__(-0.08)
    mov     dword [fz_sR], __float32__(0.08)
    cmp     r14d, -1
    jne     .zomb
    call    fz_player
    jmp     .build
.zomb:
    call    fz_zombie
.build:
    ; лёжа: ноги прямые, руки раскинуты
    cmp     dword [fz_lying], 0
    je      .b1
    mov     dword [fz_A], 0
    mov     dword [fz_lean], 0
    mov     dword [fz_aL], __float32__(0.5)
    mov     dword [fz_aR], __float32__(0.9)
    mov     dword [fz_sL], __float32__(-0.6)
    mov     dword [fz_sR], __float32__(0.5)
    mov     dword [fz_bL], __float32__(0.2)
    mov     dword [fz_bR], __float32__(0.5)
    mov     dword [fz_htilt], __float32__(0.45)
    mov     dword [fz_limp], 0
.b1:
    call    fz_skel
    call    fz_extras
    call    fj_world
    call    fz_emit
    add     rsp, 32
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; игрок: ватник, ушанка, валенки, варежки; лопата в правой, фонарик в левой
fz_player:
    sub     rsp, 40
    mov     dword [fz_type], FZ_PLAYER
    mov     rdi, [fg_rec]
    lea     rsi, [fg_ppal]
    lea     rcx, [rdi + FG_COL]
    mov     edx, 12
.p:
    mov     eax, [rsi]
    mov     [rcx], eax
    add     rsi, 4
    add     rcx, 4
    dec     edx
    jnz     .p
    cmp     dword [pl_flash], 0
    je      .nl
    mov     dword [rdi + FG_COL + FM_LENS*4], 0xC1FFF2DC
.nl:
    cmp     dword [gstate], 1
    jne     .alive
    mov     dword [fz_lying], 1
    jmp     .out
.alive:
    ; шаг: цикл — два шага (фаза копится по пройденному, на бегу шаг длиннее — game.inc);
    ; бег (pl_runb 0..1, плавно): шире шаг, пятка выше, полёт, корпус вперёд
    mov     dword [fz_run], 0
    cmp     dword [pl_moving], 0
    je      .idle
    mov     eax, [pl_runb]
    mov     [fz_run], eax
    movss   xmm0, [rbx + E_AN]
    mulss   xmm0, [fk_pph]
    movss   [fz_ph], xmm0
    movss   xmm0, [fz_run]
    mulss   xmm0, [vk_0_28f]
    addss   xmm0, [vk_0_42f]
    movss   [fz_A], xmm0
    movss   xmm0, [fz_run]
    mulss   xmm0, [vk_0_24f]
    addss   xmm0, [vk_0_06f]
    movss   [fz_lean], xmm0
.idle:
    ; голова прямо, хоть корпус и наклонён
    movss   xmm0, [fz_lean]
    mulss   xmm0, [vk_m0_6f]
    movss   [fz_hnod], xmm0
    movss   xmm0, [fz_ph]
    call    fsin
    movss   [fz_sp], xmm0
    ; фонарик — вперёд; на бегу локоть согнут, рука работает против правой
    movss   xmm0, [fz_sp]
    mulss   xmm0, [vk_0_3]
    addss   xmm0, [vk_0_25]
    mulss   xmm0, [fz_run]
    movss   xmm1, [vk_0_8]
    subss   xmm1, xmm0
    movss   [fz_aL], xmm1
    movss   xmm0, [fz_run]
    mulss   xmm0, [vk_0_4]
    addss   xmm0, [vk_0_55]
    movss   [fz_bL], xmm0
    ; правая с лопатой: ходьба — плечо качается с шагом, бег — рука согнута и работает
    movss   xmm0, [fz_sp]
    mulss   xmm0, [fz_A]
    mulss   xmm0, [vk_0_6]
    movss   xmm1, [fz_sp]
    mulss   xmm1, [f_0_5]
    subss   xmm1, xmm0
    mulss   xmm1, [fz_run]
    addss   xmm0, xmm1
    addss   xmm0, [vk_0_3]
    movss   [fz_aR], xmm0
    movss   xmm0, [fz_run]
    mulss   xmm0, [f_1]
    addss   xmm0, [vk_0_25]
    movss   [fz_bR], xmm0
    movss   xmm0, [fz_run]
    mulss   xmm0, [vk_m0_04f]
    addss   xmm0, [vk_0_14f]
    movss   [fz_sR], xmm0
    ; черенок в кулаке: ходьба — висит вперёд-вниз, взят у верха (θ 1.15, хват 0.12);
    ; бег — почти лёжа вперёд, штык чуть вниз, взят посередине (θ 0.28, хват 0.45)
    mov     dword [fz_shm], 0
    movss   xmm0, [fz_sp]
    mulss   xmm0, [fz_A]
    mulss   xmm0, [vk_0_3]
    addss   xmm0, [vk_1_15f]            ; θ ходьбы
    movss   xmm1, [fz_sp]
    mulss   xmm1, [vk_m0_2]
    addss   xmm1, [vk_0_28f]            ; θ бега
    subss   xmm1, xmm0
    mulss   xmm1, [fz_run]
    addss   xmm0, xmm1
    movss   [fz_sth], xmm0
    movss   xmm0, [fz_run]
    mulss   xmm0, [vk_0_33f]
    addss   xmm0, [vk_0_12]
    movss   [fz_shg], xmm0
    movss   xmm0, [fz_run]
    mulss   xmm0, [vk_m0_06f]
    addss   xmm0, [vk_0_12]
    movss   [fz_shs], xmm0
    ; взмах лопатой: рука на уровне плеча, заносится справа-сзади и идёт влево-вперёд;
    ; черенок — продолжением предплечья, взят за конец
    movss   xmm0, [pl_swing]
    comiss  xmm0, [f_0]
    jbe     .out
    mov     dword [fz_shm], 1
    mov     dword [fz_shg], __float32__(0.08)
    mulss   xmm0, [fk_isw]
    movss   xmm1, [f_1]
    subss   xmm1, xmm0                  ; s 0..1
    mov     dword [fz_aR], __float32__(1.45)
    mov     dword [fz_bR], __float32__(0.1)
    mov     dword [fz_sR], 0
    movaps  xmm0, xmm1
    mulss   xmm0, [vk_m2_4]
    addss   xmm0, [vk_1_3]
    movss   [fz_yR], xmm0
    mulss   xmm1, [vk_0_3]
    subss   xmm1, [vk_0_15]
    movss   [fz_roll], xmm1
    movss   xmm0, [fz_lean]
    addss   xmm0, [vk_0_15]
    movss   [fz_lean], xmm0
.out:
    add     rsp, 40
    ret

; зомби: тип одежды, рост и сложение, хромота, наклон головы — от номера
fz_zombie:
    push    r12
    sub     rsp, 32
    lea     eax, [r14d*8 + 7]
    lea     ecx, [r14d + 1234]
    HASH    eax, ecx
    mov     r12d, eax
    mov     rdi, [fg_rec]
    mov     [rdi + FG_SEED], eax
    ; тип 0..5
    mov     eax, r12d
    and     eax, 0xFFFF
    imul    eax, eax, 6
    shr     eax, 16
    mov     ecx, [fz_force]             ; --figs: тип задан
    test    ecx, ecx
    cmovns  eax, ecx
    mov     [fz_type], eax
    ; рост 0.92..1.07, сложение 0.95..1.07
    mov     eax, r12d
    shr     eax, 8
    and     eax, 15
    cvtsi2ss xmm0, eax
    mulss   xmm0, [vk_0_01]
    addss   xmm0, [vk_0_92]
    movss   [fz_h], xmm0
    mov     eax, r12d
    shr     eax, 12
    and     eax, 15
    cvtsi2ss xmm0, eax
    mulss   xmm0, [vk_0_008]
    addss   xmm0, [vk_0_95]
    movss   [fz_w], xmm0
    ; лысый старик (в тулупе)
    mov     eax, r12d
    shr     eax, 30
    mov     [fz_bald], eax
    and     dword [fz_bald], 1
    ; палитра
    call    fz_zpal
    ; ----- состояние
    cmp     dword [rbx + E_ST], ST_CORPSE
    jne     .alive
    mov     dword [fz_lying], 1
    ; тело понемногу заносит снегом
    movss   xmm0, [rbx + E_TM]
    mulss   xmm0, [f_1_7]
    addss   xmm0, [f_20]
    minss   xmm0, [f_210]
    cvttss2si eax, xmm0
    mov     [rdi + FG_SNOW], eax
    mov     dword [rdi + FG_BLOOD], 34
    jmp     .out
.alive:
    mov     dword [rdi + FG_SNOW], 40
    mov     dword [rdi + FG_BLOOD], 14
    ; шаг: цикл 0.5 тайла, со своей фазой
    movss   xmm0, [rbx + E_AN]
    mulss   xmm0, [fk_zph]
    cvtsi2ss xmm1, r14d
    addss   xmm0, xmm1
    movss   [fz_ph], xmm0
    mov     dword [fz_A], __float32__(0.3)
    cmp     dword [rbx + E_ST], ST_CHASE
    jne     .a1
    mov     dword [fz_A], __float32__(0.38)
.a1:
    ; хромает каждый четвёртый — на свою ногу
    xor     eax, eax
    test    r12d, 3 << 17
    jnz     .lm
    mov     eax, r12d
    shr     eax, 16
    and     eax, 1
    inc     eax
.lm:
    mov     [fz_limp], eax
    ; сутулость, раскачка на ходу, голова набок
    mov     eax, r12d
    shr     eax, 4
    and     eax, 7
    cvtsi2ss xmm0, eax
    mulss   xmm0, [vk_0_02]
    addss   xmm0, [vk_0_2]
    movss   [fz_lean], xmm0
    movss   xmm0, [fz_ph]
    call    fsin
    movss   [fz_sph], xmm0
    movaps  xmm1, xmm0
    mulss   xmm1, [vk_0_8]
    movss   [fz_lat], xmm1
    mulss   xmm0, [vk_0_1]
    movss   [fz_roll], xmm0
    ; и на месте не замирает: шатается, как на ветру (своя фаза)
    movss   xmm0, [wind_t]
    mulss   xmm0, [f_1_1]
    cvtsi2ss xmm1, r14d
    mulss   xmm1, [f_0_7]
    addss   xmm0, xmm1
    call    fsin
    mulss   xmm0, [vk_0_07]
    addss   xmm0, [fz_roll]
    movss   [fz_roll], xmm0
    mov     dword [fz_hnod], __float32__(0.25)
    mov     eax, r12d
    shr     eax, 20
    and     eax, 15
    cvtsi2ss xmm0, eax
    subss   xmm0, [vk_7_5]
    mulss   xmm0, [vk_0_045]
    movss   [fz_htilt], xmm0
    ; руки: в погоне — вперёд к добыче, иначе висят и болтаются (у иных одна поднята)
    cmp     dword [rbx + E_ST], ST_CHASE
    jne     .hang
    movss   xmm0, [fz_sph]
    mulss   xmm0, [vk_0_1]
    addss   xmm0, [vk_1_3]
    movss   [fz_aL], xmm0
    movss   xmm1, [vk_2_6]
    subss   xmm1, xmm0
    movss   [fz_aR], xmm1
    mov     dword [fz_sL], __float32__(-0.12)
    mov     dword [fz_sR], __float32__(0.12)
    mov     dword [fz_bL], __float32__(0.1)
    mov     dword [fz_bR], __float32__(0.1)
    jmp     .att
.hang:
    movss   xmm0, [fz_sph]
    mulss   xmm0, [vk_0_15]
    movss   xmm1, [vk_0_12]
    addss   xmm1, xmm0
    movss   [fz_aL], xmm1
    movss   xmm1, [vk_0_12]
    subss   xmm1, xmm0
    movss   [fz_aR], xmm1
    mov     dword [fz_bL], __float32__(0.3)
    mov     dword [fz_bR], __float32__(0.3)
    test    r12d, 3 << 24
    jnz     .att
    mov     dword [fz_aR], __float32__(0.9)
.att:
    ; удар: первые 0.35 с после броска — выпад, руки вперёд
    movss   xmm0, [rbx + E_CD]
    comiss  xmm0, [vk_0_9]
    jbe     .stun
    movss   xmm1, [vk_1_25]
    subss   xmm1, xmm0
    mulss   xmm1, [vk_2_857]            ; / 0.35
    CLAMP01 xmm1
    mulss   xmm1, [vk_pi]
    movaps  xmm0, xmm1
    call    fsin                        ; горб выпада 0..1..0
    movss   [fz_tt], xmm0
    mulss   xmm0, [vk_0_45]
    addss   xmm0, [fz_lean]
    movss   [fz_lean], xmm0
    movss   xmm0, [fz_tt]
    mulss   xmm0, [vk_0_4]
    addss   xmm0, [vk_1_2]
    movss   [fz_aL], xmm0
    movss   [fz_aR], xmm0
    mov     dword [fz_bL], 0
    mov     dword [fz_bR], 0
.stun:
    ; оглушён лопатой: отшатнулся, руки вскинуты
    movss   xmm0, [rbx + E_STUN]
    comiss  xmm0, [f_0]
    jbe     .out
    mulss   xmm0, [vk_1_67]             ; / 0.6
    CLAMP01 xmm0
    movss   [fz_tt], xmm0
    mulss   xmm0, [vk_0_35]
    movss   xmm1, [fz_lean]
    subss   xmm1, xmm0
    movss   [fz_lean], xmm1
    movss   xmm0, [fz_tt]
    mulss   xmm0, [vk_1_6]
    addss   xmm0, [vk_0_6]
    movss   [fz_aL], xmm0
    movss   [fz_aR], xmm0
    mov     dword [fz_sL], __float32__(-0.3)
    mov     dword [fz_sR], __float32__(0.3)
    movss   xmm0, [fz_tt]
    mulss   xmm0, [vk_m0_3]
    movss   [fz_hnod], xmm0
.out:
    add     rsp, 32
    pop     r12
    ret

; палитра зомби (r12d — хеш номера, rbx — сущность, rdi — запись)
fz_zpal:
    push    rsi
    ; кожа: серо-зелёная, у каждого своя
    mov     eax, r12d
    shr     eax, 24
    and     eax, 31
    add     eax, 230
    mov     r8d, eax
    mov     ecx, 0x8A967E
    call    vc_mul
    or      ecx, 0x01000000
    mov     [rdi + FG_COL + FM_SKIN*4], ecx
    ; одежда — цвета сущности
    mov     eax, [rbx + E_COAT]
    or      eax, 0x01000000
    mov     [rdi + FG_COL + FM_COAT*4], eax
    mov     eax, [rbx + E_PANTS]
    or      eax, 0x01000000
    mov     [rdi + FG_COL + FM_PANTS*4], eax
    ; по таблицам: волосы, шапка/платок, валенки/сапоги
    mov     eax, r12d
    shr     eax, 5
    and     eax, 3
    lea     rsi, [fg_hair]
    mov     ecx, [rsi + rax*4]
    mov     [rdi + FG_COL + FM_HAIR*4], ecx
    mov     eax, r12d
    shr     eax, 7
    and     eax, 3
    lea     rsi, [fg_hat]
    cmp     dword [fz_type], 1
    jne     .h1
    lea     rsi, [fg_scarf]
.h1:
    mov     ecx, [rsi + rax*4]
    mov     [rdi + FG_COL + FM_HAT*4], ecx
    mov     dword [rdi + FG_COL + FM_BOOT*4], 0x014C4A4E
    mov     dword [rdi + FG_COL + FM_MITT*4], 0x01302C2A
    mov     dword [rdi + FG_COL + FM_TRIM*4], 0x01CEC4AE
    mov     dword [rdi + FG_COL + FM_WOOD*4], 0x017A5A38
    mov     dword [rdi + FG_COL + FM_METAL*4], 0x018A9098
    mov     dword [rdi + FG_COL + FM_LENS*4], 0x01303036
    cmp     dword [fz_type], 2
    jne     .t5
    mov     dword [rdi + FG_COL + FM_COAT*4], 0x017A5C3C        ; тулуп
    mov     dword [rdi + FG_COL + FM_BOOT*4], 0x012A2A2C
.t5:
    cmp     dword [fz_type], 5
    jne     .t1
    mov     dword [rdi + FG_COL + FM_COAT*4], 0x01403A40        ; бабкино пальто
    mov     dword [rdi + FG_COL + FM_HAT*4], 0x015E5A56         ; шаль
    mov     dword [rdi + FG_COL + FM_BOOT*4], 0x01262220
.t1:
    cmp     dword [fz_type], 1
    jne     .eye
    mov     dword [rdi + FG_COL + FM_BOOT*4], 0x01262220
.eye:
    ; глаза: тёмные впадины; в луче фонарика, если смотрит на игрока, — отсвет
    mov     dword [rdi + FG_COL + FM_EYE*4], 0x01301E1C
    cmp     dword [pl_flash], 0
    je      .out
    movss   xmm0, [flash_i]
    comiss  xmm0, [f_40]
    jb      .out
    movss   xmm1, [rbx + E_X]
    subss   xmm1, [pl + E_X]
    movss   xmm2, [rbx + E_Y]
    subss   xmm2, [pl + E_Y]
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    movaps  xmm4, xmm2
    mulss   xmm4, xmm2
    addss   xmm3, xmm4
    comiss  xmm3, [f_196]
    jae     .out
    comiss  xmm3, [f_0_3]
    jb      .out
    sqrtss  xmm3, xmm3
    divss   xmm1, xmm3
    divss   xmm2, xmm3
    movaps  xmm4, xmm1
    mulss   xmm4, [pl + E_FX]
    movaps  xmm5, xmm2
    mulss   xmm5, [pl + E_FY]
    addss   xmm4, xmm5
    comiss  xmm4, [f_0_88]
    jb      .out
    mulss   xmm1, [rbx + E_FX]
    mulss   xmm2, [rbx + E_FY]
    addss   xmm1, xmm2
    comiss  xmm1, [f_m0_3]
    ja      .out
    mov     dword [rdi + FG_COL + FM_EYE*4], 0xC1B4F0C8
.out:
    pop     rsi
    ret

; ---------------------------------------------------------------- на кадр
; перед полосами: у игрока и у каждой сущности в кадре — поза и капсулы
; (не в кадре — FG_N = 0, полосы её пропустят)
fig_prep:
    push    rbx
    push    rsi
    push    rdi
    push    r14
    sub     rsp, 40
    lea     rbx, [pl]
    mov     r14d, -1
    lea     rdi, [figs + MAXE*FG_SZ]
    call    fig_one
    lea     rbx, [ents]
    lea     rdi, [figs]
    xor     r14d, r14d
.e:
    mov     dword [rdi + FG_N], 0
    cmp     dword [rbx + E_ST], 0
    je      .n
    ; в кадре ли (как в draw_entity, с запасом)
    movss   xmm0, [rbx + E_X]
    mulss   xmm0, [f_256]
    cvtss2si eax, xmm0
    movss   xmm0, [rbx + E_Y]
    mulss   xmm0, [f_256]
    cvtss2si ecx, xmm0
    sub     eax, [camx]
    sub     ecx, [camy]
    mov     edx, eax
    sub     edx, ecx
    sar     edx, 4
    add     edx, CX0
    cmp     edx, -40
    jl      .n
    cmp     edx, SCR_W + 40
    jg      .n
    add     eax, ecx
    sar     eax, 5
    add     eax, CY0
    cmp     eax, -10
    jl      .n
    cmp     eax, SCR_H + 50
    jg      .n
    call    fig_one
.n:
    add     rbx, ESZ
    add     rdi, FG_SZ
    inc     r14d
    cmp     r14d, MAXE
    jb      .e
    add     rsp, 40
    pop     r14
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; --figs: в кадр — все типы (ряды: 6 зомби, игрок) в 8 поворотах (через 45°;
; вторая половина — в погоне, нечётные — на шаге), свет простой, слева-сверху
fig_gallery:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 48
    lea     rdi, [albedo]
    mov     ecx, SCR_W*SCR_H
    xor     eax, eax
    rep     stosd
    lea     rdi, [nbuf]
    mov     ecx, SCR_W*SCR_H
    rep     stosb
    mov     dword [band_y0_b0], 0
    mov     dword [band_y1_b0], SCR_H
    mov     dword [band_h_b0], SCR_H
    mov     eax, [pl_moving]
    mov     [fgg_pm], eax
    xor     r12d, r12d                  ; ряд (тип)
.t:
    xor     r13d, r13d                  ; поворот
.k:
    lea     rdi, [fgg_ent]
    mov     ecx, ESZ
    xor     eax, eax
    rep     stosb
    lea     rbx, [fgg_ent]
    cvtsi2ss xmm0, r13d
    mulss   xmm0, [fgg_45]
    addss   xmm0, [fgg_22]
    call    vg_cs
    movss   [rbx + E_FX], xmm0
    movss   [rbx + E_FY], xmm1
    mov     dword [rbx + E_ST], ST_WANDER
    cmp     r13d, 4
    jb      .w
    mov     dword [rbx + E_ST], ST_CHASE
.w:
    mov     eax, r13d
    and     eax, 1
    cvtsi2ss xmm0, eax
    mulss   xmm0, [fgg_an]
    movss   [rbx + E_AN], xmm0
    mov     [pl_moving], eax
    lea     eax, [r12d + r13d]
    and     eax, 7
    lea     rcx, [zcoats]
    mov     eax, [rcx + rax*4]
    mov     [rbx + E_COAT], eax
    mov     eax, [zpants]
    mov     [rbx + E_PANTS], eax
    lea     rdi, [fgg_rec]
    mov     r14d, -1
    mov     dword [pl_runb], 0
    cmp     r12d, 6
    je      .pl
    cmp     r12d, 7
    jne     .zg
    ; последний ряд — игрок бегом, фаза своя в каждом повороте
    mov     dword [pl_runb], __float32__(1.0)
    mov     dword [pl_moving], 1
    cvtsi2ss xmm0, r13d
    mulss   xmm0, [fgg_rph]
    movss   [rbx + E_AN], xmm0
    jmp     .pl
.zg:
    mov     [fz_force], r12d
    lea     r14d, [r12d + 5]
.pl:
    call    fig_one
    mov     dword [fz_force], -1
    lea     rax, [fgg_rec]
    mov     [fd_rec_b0], rax
    imul    eax, r13d, 75
    add     eax, 42
    mov     [fd_sx_b0], eax
    imul    eax, r12d, 44
    add     eax, 40
    mov     [fd_sy_b0], eax
    mov     dword [fd_gx_b0], 8000
    mov     dword [fd_gy_b0], 8000
    call    draw_fig_b0
    inc     r13d
    cmp     r13d, 8
    jb      .k
    inc     r12d
    cmp     r12d, 8
    jb      .t
    mov     eax, [fgg_pm]
    mov     [pl_moving], eax
    mov     dword [pl_runb], 0
    ; простой свет: 0.3 + 0.7·N·L, L = (0.2, 0.6, 0.77)
    mov     r15, [fbbits]
    xor     esi, esi
.px:
    lea     rax, [albedo]
    mov     ecx, [rax + rsi*4]
    mov     edx, 0x00404850
    test    ecx, ecx
    jz      .put
    lea     rax, [nbuf]
    movzx   eax, byte [rax + rsi]
    and     eax, 0x7F
    imul    eax, eax, 6
    lea     r8, [nraw]
    movsx   edx, word [r8 + rax]
    imul    edx, edx, 20
    movsx   r9d, word [r8 + rax + 2]
    imul    r9d, r9d, 60
    add     edx, r9d
    movsx   r9d, word [r8 + rax + 4]
    imul    r9d, r9d, 77
    add     edx, r9d
    xor     r9d, r9d
    cmp     edx, r9d
    cmovl   edx, r9d
    imul    eax, edx, 180
    cdq
    mov     r9d, 10000
    idiv    r9d
    add     eax, 76
    mov     r8d, eax
    and     ecx, 0xFFFFFF
    call    vc_mul
    mov     edx, ecx
.put:
    mov     [r15 + rsi*4], edx
    inc     esi
    cmp     esi, SCR_W*SCR_H
    jb      .px
    add     rsp, 48
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ============================================================================
section .rdata
align 4, db 0
fgg_45:     dd 0.7853982
fgg_22:     dd 0.3926991
fgg_an:     dd 0.18
fgg_rph:    dd 0.09                     ; бег: восьмая цикла на поворот
fk_pph:     dd 8.7266463                ; 2π / 0.72 тайла
fk_zph:     dd 12.566371                ; 2π / 0.5
fk_isw:     dd 4.5454545                ; 1 / 0.22 (взмах)
fk_kt6:     dd 36.742346                ; начало луча: K·T, K = 60 U
fk_kt5:     dd 30.0
fk_eps:     dd 0.001
vk_m1_9:    dd -1.9
vk_1_9f:    dd 1.9
vk_7_6:     dd 7.6
vk_7_4:     dd 7.4
vk_2_3f:    dd 2.3
vk_m0_35:   dd -0.35
vk_8_4:     dd 8.4
vk_m3_4:    dd -3.4
vk_3_4:     dd 3.4
vk_m0_5:    dd -0.5
vk_5_6:     dd 5.6
vk_m0_15:   dd -0.15
vk_m0_45:   dd -0.45
vk_m0_4:    dd -0.4
vk_m2_1:    dd -2.1
vk_2_1:     dd 2.1
vk_m0_1:    dd -0.1
vk_m2_2:    dd -2.2
vk_2_2f:    dd 2.2
vk_m1_6:    dd -1.6
vk_m1_1:    dd -1.1
vk_2_05:    dd 2.05
vk_m0_85:   dd -0.85
vk_0_85f:   dd 0.85
vk_m6_2:    dd -6.2
vk_m8:      dd -8.0
vk_m2_4:    dd -2.4
vk_0_008:   dd 0.008
vk_0_95:    dd 0.95
vk_0_045:   dd 0.045
vk_1_25:    dd 1.25
vk_2_857:   dd 2.857143
vk_pi:      dd 3.1415927
vk_1_67:    dd 1.6666667
vk_m0_3:    dd -0.3
vk_0_28f:   dd 0.28
vk_15_3f:   dd 15.3
vk_0_36f:   dd 0.36
vk_0_42f:   dd 0.42
vk_0_24f:   dd 0.24
vk_0_06f:   dd 0.06
vk_m0_06f:  dd -0.06
vk_m0_6f:   dd -0.6
vk_m0_04f:  dd -0.04
vk_0_14f:   dd 0.14
vk_1_15f:   dd 1.15
vk_0_33f:   dd 0.33
vk_3_4f:    dd 3.4
vk_13f:     dd 13.0
vk_0_75f:   dd 0.75
; радиусы капсул, U
fr_torso1:  dd 4.3
fr_torso2:  dd 4.6
fr_hips:    dd 3.3
fr_skirt:   dd 4.0
fr_thigh:   dd 2.1
fr_shin:    dd 1.9
fr_foot:    dd 1.5
fr_uarm:    dd 1.7
fr_farm:    dd 1.55
fr_hand:    dd 1.25
fr_neck:    dd 1.5
fr_head:    dd 2.65
fr_hat:     dd 2.95
fr_flap:    dd 1.0
fr_scarf:   dd 2.9
fr_knot:    dd 0.95
fr_collar:  dd 2.2
fr_hair:    dd 2.7
fr_cap:     dd 2.8
fr_brim:    dd 0.75
fr_shawl:   dd 2.5
fr_eye:     dd 0.55
fr_handle:  dd 0.45
fr_blade:   dd 0.95                     ; штык — две внахлёст
fr_flash:   dd 0.7
fr_lens:    dd 0.6
; палитра игрока: кожа, волосы, ушанка, ватник, отделка, штаны, валенки, варежки,
; глаза, стекло фонарика (горит — светится), черенок, лопата
fg_ppal:    dd 0x01D8A888, 0x015A3A22, 0x014A3626, 0x013E4A34, 0x012E3826, 0x012A2A30
            dd 0x014A4A50, 0x012A2A2E, 0x012A2020, 0x01303036, 0x017A5A38, 0x018A9098
fg_hair:    dd 0x018A8884, 0x012E2622, 0x016A5A40, 0x01403024
fg_hat:     dd 0x013E3028, 0x014A4642, 0x012A2826, 0x01544034
fg_scarf:   dd 0x017A2424, 0x012E3E6A, 0x013E5A34, 0x016A5A2A

section .data
align 8, db 0
fg_rec:     dq 0
fz_fx:      dd 0.0
fz_fy:      dd 0.0
fz_w:       dd 1.0
fz_h:       dd 1.0
fz_lying:   dd 0
fz_lift:    dd 0.0
fz_type:    dd 0
fz_bald:    dd 0
fz_force:   dd -1                       ; --figs: тип зомби задан
fgg_pm:     dd 0
dbg_figs:   dd 0                        ; --figs: в снимке — фигуры рядами
; поза (обнуляется на фигуру)
fz_pose:
fz_A:       dd 0.0                      ; размах бёдер
fz_ph:      dd 0.0                      ; фаза шага
fz_lean:    dd 0.0                      ; наклон корпуса вперёд
fz_roll:    dd 0.0                      ; крен вбок
fz_lat:     dd 0.0                      ; таз вбок
fz_hnod:    dd 0.0                      ; голова вперёд
fz_htilt:   dd 0.0                      ; голова набок
fz_aL:      dd 0.0                      ; руки: подъём, локоть, вбок, рыск
fz_aR:      dd 0.0
fz_bL:      dd 0.0
fz_bR:      dd 0.0
fz_sL:      dd 0.0
fz_sR:      dd 0.0
fz_yL:      dd 0.0
fz_yR:      dd 0.0
fz_limp:    dd 0                        ; хромая нога: 0 нет, 1 левая, 2 правая
fz_run:     dd 0.0                      ; бег 0..1: шаг шире, пятка выше, полёт
fz_shm:     dd 0                        ; черенок: 0 — по fz_sth, 1 — по предплечью (взмах)
fz_sth:     dd 0.0                      ; наклон черенка вниз от горизонтали
fz_shg:     dd 0.0                      ; доля черенка позади кулака
fz_shs:     dd 0.0                      ; черенок вбок
FZ_POSEN    equ ($ - fz_pose) / 4
fz_sp:      dd 0.0
fz_d:       dd 0.0, 0.0, 0.0            ; черенок: направление, штык, вбок
fz_db:      dd 0.0, 0.0, 0.0
fz_sd:      dd 0.0, 0.0, 0.0
fz_cph:     dd 0.0
fz_sph:     dd 0.0
fz_tL:      dd 0.0
fz_tR:      dd 0.0
fz_kL:      dd 0.0
fz_kR:      dd 0.0
fz_kt:      dd 0.0
fz_tt:      dd 0.0
fz_hh:      dd 0.0
fz_hL:      dd 0.0
fz_tf:      dd 0.0
fz_ts:      dd 0.0
fz_tu:      dd 0.0
fl_L:       dd 0.0
fl_side:    dd 0.0
fl_psi:     dd 0.0
fl_ca:      dd 0.0
fl_sa:      dd 0.0
fl_dst:     dd 0
fl_src:     dd 0

section .bss
alignb 16
fj:         resd NJ*4                   ; суставы локально
fw:         resd NJ*4                   ; в мире, U от ступней
figs:       resb (MAXE + 1)*FG_SZ       ; фигуры кадра (последняя — игрок)
fgg_rec:    resb FG_SZ                  ; --figs
fgg_ent:    resb ESZ
section .text
