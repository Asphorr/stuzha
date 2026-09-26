; ----------------------------------------------------------------------------
;  render.inc — камера, сетка света, пол, объекты, отложенное освещение, снег
; ----------------------------------------------------------------------------
section .text

; замер прохода: время с прошлой отметки -> prof_acc[n] (в --shotN пишется в prof.bin)
%macro PROF 1
    lea     rcx, [prof_now]
    call    [QueryPerformanceCounter]
    mov     rax, [prof_now]
    mov     rdx, rax
    sub     rax, [prof_last]
    mov     [prof_last], rdx
    add     [prof_acc + %1*8], rax
%endmacro

render:
    push    rbx
    sub     rsp, 32
    call    [GdiFlush]
    lea     rcx, [prof_last]
    call    [QueryPerformanceCounter]
    call    cam_snap
    call    grade_frame                 ; палитра кадра, глаз -> тоновая кривая (grade.inc)
    call    interior_frame              ; у каких изб видно нутро (interior.inc)
    call    build_ntab                  ; луна и небо по нормалям
    call    moon_grid                   ; облака
    call    dl_frame                    ; огни кадра по плиткам экрана (lights.inc)
    ; тени луны, маска позёмки, лучи тени фонарика нужны только свету (и частицам):
    ; фоном, на своём потоке, пока идут сетка света, пол, объекты и крыши
    lea     rcx, [sky_job]
    call    bg_go
    PROF    12
    call    build_flash_occ
    call    compute_lightgrid
    PROF    1
    call    floor_pass
    call    ifloor_pass                 ; крашеные доски и половики в избах без крыши
    PROF    2
    call    fig_prep                    ; позы фигур кадра -> капсулы (fig.inc)
    call    sc_prep                     ; и их тени (figsh.inc)
    call    mn_prep                     ; и под луной
    ; объекты и крыши (поверх, с тестом глубины) — полосами экрана на всех ядрах
    lea     rcx, [obj_job]
    mov     edx, NBANDS
    mov     r8d, 1
    call    par_for
    call    band_merge
    PROF    3
    call    draw_wires
    call    bg_wait
    PROF    0
    ; плитки экрана: границы G-буфера в мире — огни, что не достают, долой (lights.inc)
    lea     rcx, [dl_tiles]
    mov     edx, DT_H
    mov     r8d, 2
    call    par_for
    call    light_pass
    PROF    5
    call    bloom_pass
    PROF    14
    call    beam_pass                   ; луч фонарика в снегу (lights.inc)
    call    draw_cones
    call    draw_pillars                ; в мороз: столбы над фонарями, ореол (post.inc)
    PROF    15
    call    draw_parts
    call    draw_swing                  ; след лопаты — где она достаёт
    PROF    16
    call    draw_flakes
    PROF    17
    call    draw_frost                  ; иней по краям кадра, когда игрок мёрзнет
    PROF    6
    call    grade_pass                  ; цвет кадра, замер для глаза (grade.inc)
    PROF    20
    call    draw_hitboxes               ; F3
    call    hud_build                   ; HUD — списком, рисует поток вывода (hud.inc)
    PROF    7
    add     rsp, 32
    pop     rbx
    ret

; фоновое задание кадра: развёртка теней луны, маска позёмки, лучи тени фонарика
; (время — в sky_ticks)
sky_job:
    sub     rsp, 40
    lea     rcx, [sky_t0]
    call    [QueryPerformanceCounter]
    call    sky_sweep                   ; тени луны по высотам (+ облака в ячейках)
    call    build_wgrid                 ; где у земли метёт — маска позёмки
    cmp     dword [dl_fon], 0
    je      .nofl
    call    fl_map                      ; веер лучей из руки (hz16.inc)
.nofl:
    lea     rcx, [sky_t1]
    call    [QueryPerformanceCounter]
    mov     rax, [sky_t1]
    sub     rax, [sky_t0]
    mov     [sky_ticks], rax
    add     rsp, 40
    ret

; камера привязывается к целому пикселю изометрической решётки
cam_snap:
    movss   xmm0, [cam_xf]
    mulss   xmm0, [f_256]
    cvtss2si eax, xmm0
    movss   xmm0, [cam_yf]
    mulss   xmm0, [f_256]
    cvtss2si edx, xmm0
    mov     ecx, eax
    sub     ecx, edx
    add     ecx, 8
    sar     ecx, 4                      ; a
    add     eax, edx
    add     eax, 16
    sar     eax, 5                      ; b
    mov     [cam_a], ecx
    mov     [cam_b], eax
    shl     eax, 4
    shl     ecx, 3
    lea     edx, [eax + ecx]
    mov     [camx], edx
    sub     eax, ecx
    mov     [camy], eax
    mov     eax, [camx]
    sar     eax, 6
    sub     eax, GW/2
    mov     [gox], eax
    mov     eax, [camy]
    sar     eax, 6
    sub     eax, GW/2
    mov     [goy], eax
    movss   xmm0, [pl + E_X]
    mulss   xmm0, [f_256]
    cvtss2si eax, xmm0
    mov     [plw_x], eax
    movss   xmm0, [pl + E_Y]
    mulss   xmm0, [f_256]
    cvtss2si edx, xmm0
    mov     [plw_y], edx
    lea     ecx, [eax + edx]
    sar     ecx, 8
    mov     [pdiag], ecx
    sub     eax, [camx]
    sub     edx, [camy]
    mov     ecx, eax
    sub     ecx, edx
    sar     ecx, 4
    add     ecx, CX0
    mov     [psx], ecx
    add     eax, edx
    sar     eax, 5
    add     eax, CY0
    mov     [psy], eax
    ret

; ---------------------------------------------------------------- сетка света
; на каждый сэмпл (4 на тайл): небо·AO + фонари + (фонарик + аура) * видимость, A = конус обзора
compute_lightgrid:
    sub     rsp, 40
    lea     rcx, [lg_rows]
    mov     edx, GW
    mov     r8d, 2
    call    par_for
    call    lg_soften
    add     rsp, 40
    ret

; строки сетки ecx .. edx−1 (с любого потока пула; своё — только на стеке).
; Три сетки: lgs — поверхностям (небо·AO, аура, видимость: огни они считают сами,
; попиксельно), lgst — свет огней и фонарика на снегу (отсвет снизу), lgrid — всё
; вместе, частицам и снежинкам в воздухе
LG_ST equ 32                            ; [rsp + LG_ST]: огни, окна, фонарик у сэмпла
LG_A equ 48                             ; небо·AO
LG_END equ 64                           ; конец диапазона
lg_rows:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 88
    mov     [rsp + LG_END], edx
    mov     r14d, ecx                   ; j
    imul    eax, ecx, GW*4
    lea     rdi, [lgrid]
    add     rdi, rax
    pxor    xmm15, xmm15
    mov     r12d, [gox]
    mov     r13d, [goy]
.row:
    xor     r15d, r15d                  ; i
.col:
    mov     eax, r12d
    add     eax, r15d                   ; gx
    mov     ebx, r13d
    add     ebx, r14d                   ; gy
    cmp     eax, SLW-1
    ja      .dark
    cmp     ebx, SLW-1
    ja      .dark
    mov     ecx, ebx
    shl     ecx, 9
    add     ecx, eax
    ; окна и комнаты
    lea     rdx, [slight]
    movd    xmm0, [rdx + rcx*4]
    punpcklbw xmm0, xmm15
    punpcklwd xmm0, xmm15
    cvtdq2ps xmm0, xmm0
    mulps   xmm0, [winf_vec]
    ; фонари (прогрев натрия)
    lea     rdx, [slightL]
    mov     r8d, [rdx + rcx*4]
    test    r8d, r8d
    jz      .nol
    movd    xmm1, r8d
    punpcklbw xmm1, xmm15
    punpcklwd xmm1, xmm15
    cvtdq2ps xmm1, xmm1
    mulps   xmm1, [lampf_vec]
    addps   xmm0, xmm1
.nol:
    lea     rdx, [slightB]
    mov     r8d, [rdx + rcx*4]
    test    r8d, r8d
    jz      .nob
    movd    xmm1, r8d
    punpcklbw xmm1, xmm15
    punpcklwd xmm1, xmm15
    cvtdq2ps xmm1, xmm1
    mulps   xmm1, [lampb_vec]
    addps   xmm0, xmm1
.nob:
    lea     rdx, [slightT]
    mov     r8d, [rdx + rcx*4]
    test    r8d, r8d
    jz      .notv
    movd    xmm1, r8d
    punpcklbw xmm1, xmm15
    punpcklwd xmm1, xmm15
    cvtdq2ps xmm1, xmm1
    mulps   xmm1, [tv_vec]
    addps   xmm0, xmm1
.notv:
    ; небо: рассеянный·AO (луна — попиксельно, по высоте и нормали, в light_pass)
    lea     rdx, [aomap]
    movzx   r8d, byte [rdx + rcx]
    cvtsi2ss xmm1, r8d
    mulss   xmm1, [f_inv255]
    shufps  xmm1, xmm1, 0
    mulps   xmm1, [amb_vec]
    movaps  [rsp + LG_A], xmm1
    ; видимое игроком (аура, обзор) копится отдельно — в lgv, сгладится после;
    ; фонарик на снегу — к огням сэмпла (поверхностям он светит попиксельно)
    movaps  [rsp + LG_ST], xmm0
    xorps   xmm0, xmm0
    shl     eax, 6
    shl     ebx, 6
    mov     esi, eax                    ; wx
    mov     ebp, ebx                    ; wy
    sub     eax, [plw_x]
    cvtsi2ss xmm1, eax
    mulss   xmm1, [f_inv256]            ; dx
    mov     eax, ebp
    sub     eax, [plw_y]
    cvtsi2ss xmm2, eax
    mulss   xmm2, [f_inv256]            ; dy
    movaps  xmm9, xmm1
    movaps  xmm10, xmm2
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    movaps  xmm4, xmm2
    mulss   xmm4, xmm2
    addss   xmm3, xmm4                  ; d2
    comiss  xmm3, [vis_r2]              ; в метель видно ближе
    jae     .novis
    cmp     dword [gstate], 1
    je      .novis
    mov     ecx, [plw_x]
    mov     edx, [plw_y]
    mov     r8d, esi
    mov     r9d, ebp
    call    los_ray
    test    eax, eax
    jz      .novis
    imul    eax, r14d, GW
    add     eax, r15d
    lea     rcx, [losg]
    mov     byte [rcx + rax], 255
    sqrtss  xmm4, xmm3                  ; d
    movaps  xmm5, xmm4
    maxss   xmm5, [f_0_05]
    mulss   xmm1, [pl + E_FX]
    mulss   xmm2, [pl + E_FY]
    addss   xmm1, xmm2
    divss   xmm1, xmm5                  ; c = cos угла от взгляда
    ; конус обзора
    movaps  xmm5, xmm1
    addss   xmm5, [f_0_1]
    mulss   xmm5, [f_2_2]
    CLAMP01 xmm5
    movss   xmm6, [f_2_4]
    subss   xmm6, xmm4
    mulss   xmm6, [f_0_8]
    CLAMP01 xmm6
    maxss   xmm5, xmm6
    movss   xmm6, [vis_r]
    subss   xmm6, xmm4
    mulss   xmm6, [vis_k]               ; в мгле край обзора длиннее и мягче
    CLAMP01 xmm6
    mulss   xmm5, xmm6
    mulss   xmm5, [f_255]               ; видимость 0..255
    ; фонарик
    movss   xmm6, [flash_i]
    comiss  xmm6, [f_0]
    jbe     .noflash
    movaps  xmm7, xmm1
    subss   xmm7, [f_0_86]
    mulss   xmm7, [f_14]
    CLAMP01 xmm7
    ; горячее пятно в центре луча
    movaps  xmm8, xmm1
    subss   xmm8, [f_0_965]
    mulss   xmm8, [f_40]
    CLAMP01 xmm8
    mulss   xmm8, [f_0_35]
    addss   xmm7, xmm8
    ; тёмное кольцо рефлектора
    movaps  xmm8, xmm1
    subss   xmm8, [f_0_915]
    andps   xmm8, [f_absmask]
    mulss   xmm8, [f_50]
    movss   xmm11, [f_1]
    subss   xmm11, xmm8
    maxss   xmm11, [f_0]
    mulss   xmm11, [f_0_12]
    subss   xmm7, xmm11
    maxss   xmm7, [f_0]
    movaps  xmm8, xmm1
    subss   xmm8, [f_0_5]
    mulss   xmm8, [f_3]
    CLAMP01 xmm8
    mulss   xmm8, [f_0_16]
    addss   xmm7, xmm8
    ; тени зомби, стоящих в луче
    mov     ecx, [nocc]
    test    ecx, ecx
    jz      .noocc
    lea     rdx, [occ]
    movss   xmm11, [f_1]
.oc:
    movaps  xmm12, xmm9
    mulss   xmm12, [rdx]
    movaps  xmm13, xmm10
    mulss   xmm13, [rdx + 4]
    addss   xmm12, xmm13                ; вдоль
    comiss  xmm12, [rdx + 12]
    jbe     .ocn
    movaps  xmm13, xmm9
    mulss   xmm13, [rdx + 4]
    movaps  xmm14, xmm10
    mulss   xmm14, [rdx]
    subss   xmm13, xmm14
    andps   xmm13, [f_absmask]          ; поперёк
    movaps  xmm14, xmm12
    mulss   xmm14, [rdx + 16]
    subss   xmm14, xmm13
    movaps  xmm13, xmm12
    subss   xmm13, [rdx + 8]
    mulss   xmm13, [f_0_03]
    addss   xmm13, [f_0_05]
    divss   xmm14, xmm13
    addss   xmm14, [f_0_5]
    CLAMP01 xmm14
    mulss   xmm14, [f_0_85]
    movss   xmm13, [f_1]
    subss   xmm13, xmm14
    mulss   xmm11, xmm13
.ocn:
    add     rdx, OC_SZ
    dec     ecx
    jnz     .oc
    mulss   xmm7, xmm11
.noocc:
    movaps  xmm8, xmm3
    mulss   xmm8, [f_0_035]
    addss   xmm8, [f_1]
    divss   xmm6, xmm8
    movss   xmm8, [fl_reach]            ; луч вязнет в снегу
    subss   xmm8, xmm4
    mulss   xmm8, [f_0_25]
    CLAMP01 xmm8
    mulss   xmm6, xmm8
    mulss   xmm7, xmm6
    shufps  xmm7, xmm7, 0
    mulps   xmm7, [flash_col]
    addps   xmm7, [rsp + LG_ST]
    movaps  [rsp + LG_ST], xmm7
.noflash:
    ; слабая аура вокруг игрока
    movss   xmm7, [f_1]
    mulss   xmm4, [f_0_3]
    subss   xmm7, xmm4
    CLAMP01 xmm7
    mulss   xmm7, xmm7
    shufps  xmm7, xmm7, 0
    mulps   xmm7, [aura_col]
    addps   xmm0, xmm7
    xorps   xmm6, xmm6
    movss   xmm6, xmm5
    pslldq  xmm6, 12
    addps   xmm0, xmm6
    cvttss2si eax, xmm5
    imul    edx, r14d, GW
    add     edx, r15d
    lea     rcx, [vgrid]
    mov     [rcx + rdx], al             ; резкая видимость — игре (кого рисовать)
    cvtps2dq xmm0, xmm0
    packssdw xmm0, xmm0
    packuswb xmm0, xmm0
    movd    [rdi + (lgv - lgrid)], xmm0
    jmp     .store
.novis:
    imul    eax, r14d, GW
    add     eax, r15d
    lea     rcx, [losg]
    mov     byte [rcx + rax], 0
.nov2:
    lea     rcx, [vgrid]
    mov     byte [rcx + rax], 0
    mov     dword [rdi + (lgv - lgrid)], 0
    jmp     .store
.dark:
    xorps   xmm0, xmm0
    movaps  [rsp + LG_ST], xmm0
    movaps  [rsp + LG_A], xmm0
    imul    eax, r14d, GW
    add     eax, r15d
    lea     rcx, [losg]
    mov     byte [rcx + rax], 0
    jmp     .nov2
.store:
    movaps  xmm0, [rsp + LG_ST]
    movaps  xmm1, [rsp + LG_A]
    movaps  xmm2, xmm0
    addps   xmm2, xmm1
    cvtps2dq xmm0, xmm0
    packssdw xmm0, xmm0
    packuswb xmm0, xmm0
    movd    [rdi + (lgst - lgrid)], xmm0
    cvtps2dq xmm1, xmm1
    packssdw xmm1, xmm1
    packuswb xmm1, xmm1
    movd    [rdi + (lgs - lgrid)], xmm1
    cvtps2dq xmm2, xmm2
    packssdw xmm2, xmm2
    packuswb xmm2, xmm2
    movd    [rdi], xmm2
    add     rdi, 4
    inc     r15d
    cmp     r15d, GW
    jb      .col
    inc     r14d
    cmp     r14d, [rsp + LG_END]
    jb      .row
    add     rsp, 88
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; видимое игроком сглаживается: луч видимости на сэмпл — да/нет, край обзора за стеной,
; окном, углом и пятно фонарика у стен иначе режутся ступеньками по сэмплам.
; 2 прохода [1 2 1] по строкам и столбцам (≈ биномиальное 5x5), затем в сетку света
lg_soften:
    push    rsi
    push    rdi
    mov     r9d, 2
.pass:
    lea     rsi, [lgv]
    lea     rdi, [lgt]
    movdqa  xmm0, [rsi]
    movdqa  [rdi], xmm0
    mov     ecx, 16
.h:
    movdqu  xmm0, [rsi + rcx - 4]
    movdqu  xmm1, [rsi + rcx + 4]
    pavgb   xmm0, xmm1
    pavgb   xmm0, [rsi + rcx]
    movdqa  [rdi + rcx], xmm0
    add     ecx, 16
    cmp     ecx, GW*GW*4 - 16
    jb      .h
    movdqa  xmm0, [rsi + rcx]
    movdqa  [rdi + rcx], xmm0
    mov     ecx, GW*4
.v:
    movdqa  xmm0, [rdi + rcx - GW*4]
    pavgb   xmm0, [rdi + rcx + GW*4]
    pavgb   xmm0, [rdi + rcx]
    movdqa  [rsi + rcx], xmm0
    add     ecx, 16
    cmp     ecx, GW*GW*4 - GW*4
    jb      .v
    dec     r9d
    jnz     .pass
    lea     rdi, [lgrid]
    lea     rdx, [lgs]
    xor     ecx, ecx
.a:
    movdqa  xmm1, [rsi + rcx]
    movdqa  xmm0, [rdi + rcx]
    paddusb xmm0, xmm1
    movdqa  [rdi + rcx], xmm0
    paddusb xmm1, [rdx + rcx]
    movdqa  [rdx + rcx], xmm1
    add     ecx, 16
    cmp     ecx, GW*GW*4
    jb      .a
    pop     rdi
    pop     rsi
    ret

; ---------------------------------------------------------------- пол
; обратная изометрия по пикселю: wx += 8, wy -= 8 вдоль строки
floor_pass:
    sub     rsp, 40
    lea     rcx, [floor_rows]
    cmp     dword [have_avx2], 0
    je      .sse
    lea     rcx, [floor_rows_x]         ; то же на AVX2, по 8 пикселей
.sse:
    mov     edx, SCR_H
    mov     r8d, 8
    call    par_for
    add     rsp, 40
    ret

; строки ecx .. edx−1 (с любого потока пула)
floor_rows:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 8
    mov     [rsp], edx                  ; конец диапазона
    mov     ebp, ecx
    imul    eax, ecx, SCR_W
    lea     rsi, [nbuf]
    add     rsi, rax
    lea     rdi, [albedo]
    lea     rdi, [rdi + rax*4]
    lea     rbx, [tmap]
    pxor    xmm15, xmm15
.row:
    mov     eax, ebp
    sub     eax, CY0
    shl     eax, 4
    mov     r8d, [camx]
    add     r8d, eax
    sub     r8d, 8*CX0
    mov     r9d, [camy]
    add     r9d, eax
    add     r9d, 8*CX0
    mov     r15d, SCR_W
.px:
    cmp     r8d, 32767
    ja      .void
    cmp     r9d, 32767
    ja      .void
    mov     eax, r9d
    shl     eax, 16
    or      eax, r8d
    mov     [rdi + (gposb - albedo)], eax
    mov     eax, r9d
    shr     eax, 8
    shl     eax, 7
    mov     edx, r8d
    shr     edx, 8
    add     eax, edx
    movzx   eax, byte [rbx + rax]
    lea     rdx, [tprop]
    movzx   r14d, byte [rdx + rax]
    shr     r14d, 4                     ; вид пола
    cmp     r14d, 2
    je      .wood
    ; ----- снег / укатанная дорога
    mov     r10d, r8d
    shr     r10d, 4
    mov     ecx, r9d
    shr     ecx, 4
    HASH    r10d, ecx
    mov     r11d, r8d
    shr     r11d, 7
    mov     ecx, r9d
    shr     ecx, 7
    HASH    r11d, ecx
    mov     eax, r10d
    and     eax, 7
    mov     ecx, r11d
    and     ecx, 15
    add     eax, ecx
    mov     ecx, 240
    test    r14d, r14d
    jz      .sb
    mov     ecx, 198
.sb:
    sub     ecx, eax
    movd    xmm0, ecx
    pshuflw xmm0, xmm0, 0
    paddw   xmm0, [snow_off]
    test    r14d, r14d
    jz      .noice
    mov     eax, r11d
    shr     eax, 8
    and     eax, 7
    jnz     .noice
    paddw   xmm0, [ice_off]
.noice:
    mov     eax, r9d
    shr     eax, 5
    shl     eax, 10
    mov     ecx, r8d
    shr     ecx, 5
    add     eax, ecx
    mov     r12d, eax
    lea     rdx, [tracks]
    movzx   ecx, byte [rdx + rax]
    test    ecx, ecx
    jz      .notr
    movd    xmm1, ecx
    pshuflw xmm1, xmm1, 0
    pmullw  xmm1, [trk_mul]
    psrlw   xmm1, 8
    psubw   xmm0, xmm1
.notr:
    mov     r13d, 0x01000000
    test    r14d, r14d
    jnz     .relief
    mov     eax, r10d
    xor     eax, [glint]
    imul    eax, eax, 0x2545F491
    shr     eax, 24
    cmp     eax, 3
    jae     .relief
    mov     r13d, 0x41000000
    paddw   xmm0, [sparkle_add]
.relief:
    ; нормаль: рельеф снега (сугробы, валы, колеи, заструги) + стенки следов;
    ; без следов по соседству — готовая из карты
    lea     rdx, [tracks]
    mov     eax, [rdx + r12 - 1]
    and     eax, 0x00FF00FF
    movzx   ecx, byte [rdx + r12 - DECW]
    or      eax, ecx
    movzx   ecx, byte [rdx + r12 + DECW]
    or      eax, ecx
    lea     rdx, [drift]
    jnz     .rtr
    movzx   r10d, byte [rdx + r12*4 + 3]
    jmp     .blood
.rtr:
    mov     eax, [rdx + r12*4]
    mov     ecx, eax
    shr     ecx, 8
    movsx   r10d, cl
    shr     eax, 16
    movsx   r11d, al
    lea     rdx, [tracks]
    movzx   eax, byte [rdx + r12 + 1]
    movzx   ecx, byte [rdx + r12 - 1]
    sub     eax, ecx
    sar     eax, 4
    add     r10d, eax
    movzx   eax, byte [rdx + r12 + DECW]
    movzx   ecx, byte [rdx + r12 - DECW]
    sub     eax, ecx
    sar     eax, 4
    add     r11d, eax
    mov     eax, -8
    cmp     r10d, eax
    cmovl   r10d, eax
    cmp     r11d, eax
    cmovl   r11d, eax
    mov     eax, 7
    cmp     r10d, eax
    cmovg   r10d, eax
    cmp     r11d, eax
    cmovg   r11d, eax
    add     r10d, 8
    add     r11d, 8
    lea     rdx, [q5]
    movzx   r10d, byte [rdx + r10]
    movzx   r11d, byte [rdx + r11]
    lea     eax, [r11d + r11d*4]
    add     eax, r10d
    lea     rdx, [gn_lut]
    movzx   r10d, byte [rdx + rax]
    jmp     .blood
.wood:
    ; ----- дощатый пол
    mov     eax, r9d
    shr     eax, 6
    mov     r12d, eax                   ; доска
    imul    ecx, eax, 97
    add     ecx, r8d
    mov     r13d, ecx                   ; x со сдвигом стыков
    shr     ecx, 8
    mov     r10d, ecx
    mov     ecx, r12d
    HASH    r10d, ecx
    mov     r11d, r8d
    shr     r11d, 2
    mov     ecx, r12d
    HASH    r11d, ecx
    mov     eax, r10d
    and     eax, 31
    sub     eax, 16
    and     r11d, 7
    add     eax, r11d
    movd    xmm1, eax
    pshuflw xmm1, xmm1, 0
    pmullw  xmm1, [wood_var]
    psraw   xmm1, 3
    movdqa  xmm0, [wood_base]
    paddw   xmm0, xmm1
    mov     eax, r9d
    and     eax, 63
    cmp     eax, 4
    jb      .seam
    mov     eax, r13d
    and     eax, 255
    cmp     eax, 3
    jae     .noseam
.seam:
    psrlw   xmm0, 1
.noseam:
    mov     eax, r9d
    shr     eax, 5
    shl     eax, 10
    mov     ecx, r8d
    shr     ecx, 5
    add     eax, ecx
    mov     r12d, eax
    mov     r13d, 0x01000000
    xor     r10d, r10d                  ; пол ровный
.blood:
    lea     rdx, [blood]
    movzx   ecx, byte [rdx + r12]
    test    ecx, ecx
    jz      .pack
    packuswb xmm0, xmm0
    punpcklbw xmm0, xmm15
    movd    xmm1, ecx
    pshuflw xmm1, xmm1, 0
    movdqa  xmm2, [w256]
    psubw   xmm2, xmm1
    pmullw  xmm0, xmm2
    pmullw  xmm1, [blood_col]
    paddw   xmm0, xmm1
    psrlw   xmm0, 8
.pack:
    packuswb xmm0, xmm0
    movd    eax, xmm0
    and     eax, 0x00FFFFFF
    or      eax, r13d
    mov     [rdi], eax
    mov     [rsi], r10b
    jmp     .next
.void:
    mov     dword [rdi], 0
    mov     dword [rdi + (gposb - albedo)], 0
.next:
    add     rdi, 4
    inc     rsi
    add     r8d, 8
    sub     r9d, 8
    dec     r15d
    jnz     .px
    inc     ebp
    cmp     ebp, [rsp]
    jb      .row
    add     rsp, 8
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- пол на AVX2
; floor_rows по 8 пикселей. Числа пикселя — дворд на дорожку, цвет — по каналу
; (B, G, R) двордами: у floor_rows слова не выходят за 2^15 (и не уходят в минус),
; так что те же сложения, сдвиги и насыщение к 0..255 дают те же байты. Развилки
; (пустота, снег, дорога, доски, лёд, искра, следы, рельеф, кровь) — масками
section .rdata
align 32, db 0
fl_lane:    dd 0, 8, 16, 24, 32, 40, 48, 56
fl_hi:      times 8 dd 0xFFFF8000
fl_ff00ff:  times 8 dd 0x00FF00FF
fl_255:     times 8 dd 255
fl_safe:    times 8 dd DECW + 1         ; ячейка следов для пустоты (соседи — в массиве)
fl_nbs:     db 0, 4, 8, 12, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128
            db 0, 4, 8, 12, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128
fl_glk:     times 8 dd 0x2545F491
fl_fl0:     times 8 dd 0x01000000
fl_fls:     times 8 dd 0x40000000       ; искра
fl_m8:      times 8 dd -8
fl_2:       times 8 dd 2
fl_3:       times 8 dd 3
fl_4:       times 8 dd 4
fl_5:       times 8 dd 5
fl_6:       times 8 dd 6
fl_7:       times 8 dd 7
fl_8:       times 8 dd 8
fl_10:      times 8 dd 10
fl_14:      times 8 dd 14
fl_15:      times 8 dd 15
fl_16:      times 8 dd 16
fl_31:      times 8 dd 31
fl_40:      times 8 dd 40
fl_63:      times 8 dd 63
fl_64:      times 8 dd 64
fl_72:      times 8 dd 72
fl_97:      times 8 dd 97
fl_112:     times 8 dd 112
fl_198:     times 8 dd 198
fl_240:     times 8 dd 240
fl_256:     times 8 dd 256
fl_t44:     times 8 dd 44               ; следы: trk_mul
fl_t58:     times 8 dd 58
fl_t72:     times 8 dd 72
fl_bb:      times 8 dd 22 << 16         ; кровь: blood_col << 16
fl_bg:      times 8 dd 14 << 16
fl_br:      times 8 dd 110 << 16

section .text
; HASH над дорожками: %1 = HASH(%1, %2) (портит %2)
%macro HASHV 2
    vpmulld %1, %1, [lt_hx]
    vpmulld %2, %2, [lt_hy]
    vpxor   %1, %1, %2
    vpsrld  %2, %1, 15
    vpxor   %1, %1, %2
    vpmulld %1, %1, [lt_hm]
    vpsrld  %2, %1, 13
    vpxor   %1, %1, %2
%endmacro

; канал: %1 = clamp(%1, 0, 255), потом кровь — (x·(256 − b) + b·цвет) >> 8 одним
; vpmaddwd: ymm8 = 256 − b, ymm9 = b << 16, %2 — цвет << 16 (портит ymm14)
%macro FLBLOOD 2
    vpxor   ymm14, ymm14, ymm14
    vpmaxsd %1, %1, ymm14
    vpminsd %1, %1, [fl_255]
    vpor    %1, %1, ymm9
    vpor    ymm14, ymm8, [%2]
    vpmaddwd %1, %1, ymm14
    vpsrld  %1, %1, 8
%endmacro

FX_B        equ 0                       ; свой кадр (выровнен на 32)
FX_G        equ 32
FX_R        equ 64
FX_FL       equ 96                      ; флаги альбедо
FX_N        equ 128                     ; нормаль
FX_SNOW     equ 160                     ; снег (не дорога)
FX_WOOD     equ 192                     ; доски
FX_SZ       equ 224

; строки ecx .. edx−1 — как floor_rows (с любого потока пула)
floor_rows_x:
    push    rbx
    push    rsi
    push    rdi
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    mov     rbp, rsp
    sub     rsp, FX_SZ
    and     rsp, -32
    mov     r12d, ecx                   ; строка
    mov     r13d, edx                   ; конец
    lea     rbx, [albedo]
    lea     r14, [tprop]
    lea     r15, [gn_lut]
.row:
    imul    eax, r12d, SCR_W
    lea     rdi, [rbx + rax*4]
    lea     rsi, [rbx + rax + (nbuf - albedo)]
    mov     eax, r12d
    sub     eax, CY0
    shl     eax, 4
    mov     ecx, [camx]
    add     ecx, eax
    sub     ecx, 8*CX0
    mov     edx, [camy]
    add     edx, eax
    add     edx, 8*CX0
    vmovd   xmm0, ecx
    vpbroadcastd ymm0, xmm0
    vpaddd  ymm0, ymm0, [fl_lane]       ; wx: +8 на пиксель
    vmovd   xmm1, edx
    vpbroadcastd ymm1, xmm1
    vpsubd  ymm1, ymm1, [fl_lane]       ; wy: −8
    mov     r8d, SCR_W/8
.grp:
    ; пустота: wx или wy вне 0..32767
    vpor    ymm2, ymm0, ymm1
    vpand   ymm2, ymm2, [fl_hi]
    vpxor   ymm3, ymm3, ymm3
    vpcmpeqd ymm2, ymm2, ymm3           ; пол есть
    vptest  ymm2, ymm2
    jz      .void
    vpslld  ymm3, ymm1, 16
    vpor    ymm3, ymm3, ymm0
    vpand   ymm3, ymm3, ymm2
    vmovdqu [rdi + (gposb - albedo)], ymm3
    ; вид пола: tprop[tmap[тайл]] >> 4 (0 снег, 2 доски, прочее — дорога)
    vpsrld  ymm3, ymm1, 8
    vpslld  ymm3, ymm3, 7
    vpsrld  ymm4, ymm0, 8
    vpaddd  ymm3, ymm3, ymm4
    vpand   ymm3, ymm3, ymm2            ; пустоте — тайл 0
    vpcmpeqd ymm5, ymm5, ymm5
    vpxor   ymm4, ymm4, ymm4
    vpgatherdd ymm4, [rbx + ymm3*1 + (tmap - albedo)], ymm5
    vpand   ymm4, ymm4, [fl_255]
    vpcmpeqd ymm5, ymm5, ymm5
    vpxor   ymm3, ymm3, ymm3
    vpgatherdd ymm3, [r14 + ymm4*1], ymm5
    vpand   ymm4, ymm3, [fl_255]
    vpsrld  ymm4, ymm4, 4
    vpxor   ymm5, ymm5, ymm5
    vpcmpeqd ymm5, ymm5, ymm4           ; снег
    vmovdqa [rsp + FX_SNOW], ymm5
    vpcmpeqd ymm4, ymm4, [fl_2]
    vmovdqa [rsp + FX_WOOD], ymm4
    ; ячейка следов, крови и рельефа (8 на тайл)
    vpsrld  ymm3, ymm1, 5
    vpslld  ymm3, ymm3, 10
    vpsrld  ymm6, ymm0, 5
    vpaddd  ymm3, ymm3, ymm6
    vmovdqa ymm6, [fl_safe]
    vpblendvb ymm3, ymm6, ymm3, ymm2
    ; ----- снег и укатанная дорога: хеши ячеек 1/16 и 1/2 тайла
    vpsrld  ymm7, ymm0, 4
    vpsrld  ymm8, ymm1, 4
    HASHV   ymm7, ymm8                  ; r10
    vpsrld  ymm8, ymm0, 7
    vpsrld  ymm9, ymm1, 7
    HASHV   ymm8, ymm9                  ; r11
    vpand   ymm9, ymm7, [fl_7]
    vpand   ymm10, ymm8, [fl_15]
    vpaddd  ymm9, ymm9, ymm10
    vmovdqa ymm10, [fl_198]
    vpblendvb ymm10, ymm10, [fl_240], ymm5
    vpsubd  ymm9, ymm10, ymm9           ; c
    vpaddd  ymm10, ymm9, [fl_10]        ; + snow_off
    vpaddd  ymm11, ymm9, [fl_3]
    vpsubd  ymm12, ymm9, [fl_3]
    ; лёд на дороге: ((r11 >> 8) & 7) = 0
    vpsrld  ymm13, ymm8, 8
    vpand   ymm13, ymm13, [fl_7]
    vpxor   ymm14, ymm14, ymm14
    vpcmpeqd ymm13, ymm13, ymm14
    vpandn  ymm13, ymm5, ymm13
    vpand   ymm14, ymm13, [fl_14]
    vpaddd  ymm10, ymm10, ymm14
    vpand   ymm14, ymm13, [fl_6]
    vpaddd  ymm11, ymm11, ymm14
    ; следы: дворд с ячейки слева — байты t[−1], t, t[+1]
    vpcmpeqd ymm15, ymm15, ymm15
    vpxor   ymm13, ymm13, ymm13
    vpgatherdd ymm13, [rbx + ymm3*1 + (tracks - albedo) - 1], ymm15
    vpsrld  ymm14, ymm13, 8
    vpand   ymm14, ymm14, [fl_255]      ; t
    vpmaddwd ymm15, ymm14, [fl_t44]
    vpsrld  ymm15, ymm15, 8
    vpsubd  ymm10, ymm10, ymm15
    vpmaddwd ymm15, ymm14, [fl_t58]
    vpsrld  ymm15, ymm15, 8
    vpsubd  ymm11, ymm11, ymm15
    vpmaddwd ymm15, ymm14, [fl_t72]
    vpsrld  ymm15, ymm15, 8
    vpsubd  ymm12, ymm12, ymm15
    ; искра на снегу: ((r10 ^ glint)·0x2545F491) >> 24 < 3
    vpbroadcastd ymm15, [glint]
    vpxor   ymm15, ymm15, ymm7
    vpmulld ymm15, ymm15, [fl_glk]
    vpsrld  ymm15, ymm15, 24
    vmovdqa ymm6, [fl_3]
    vpcmpgtd ymm15, ymm6, ymm15
    vpand   ymm15, ymm15, ymm5
    vpand   ymm6, ymm15, [fl_14]
    vpaddd  ymm10, ymm10, ymm6
    vpaddd  ymm11, ymm11, ymm6
    vpaddd  ymm12, ymm12, ymm6
    vmovdqa [rsp + FX_B], ymm10
    vmovdqa [rsp + FX_G], ymm11
    vmovdqa [rsp + FX_R], ymm12
    vpand   ymm6, ymm15, [fl_fls]
    vpor    ymm6, ymm6, [fl_fl0]
    vmovdqa [rsp + FX_FL], ymm6
    ; нормаль: без следов по соседству — готовая из карты (байт 3)
    vpcmpeqd ymm15, ymm15, ymm15
    vpxor   ymm6, ymm6, ymm6
    vpgatherdd ymm6, [rbx + ymm3*4 + (drift - albedo)], ymm15
    vpsrld  ymm7, ymm6, 24
    vpcmpeqd ymm15, ymm15, ymm15
    vpxor   ymm8, ymm8, ymm8
    vpgatherdd ymm8, [rbx + ymm3*1 + (tracks - albedo) - DECW], ymm15
    vpcmpeqd ymm15, ymm15, ymm15
    vpxor   ymm9, ymm9, ymm9
    vpgatherdd ymm9, [rbx + ymm3*1 + (tracks - albedo) + DECW], ymm15
    vpand   ymm8, ymm8, [fl_255]        ; t[−DECW]
    vpand   ymm9, ymm9, [fl_255]        ; t[+DECW]
    vpand   ymm10, ymm13, [fl_ff00ff]
    vpor    ymm10, ymm10, ymm8
    vpor    ymm10, ymm10, ymm9
    vpxor   ymm11, ymm11, ymm11
    vpcmpeqd ymm10, ymm10, ymm11        ; следов рядом нет
    vpandn  ymm11, ymm10, ymm2
    vptest  ymm11, ymm11
    jz      .nrel
    ; со следами: наклон из карты + стенки следов, прижать, q5, gn_lut
    vpslld  ymm11, ymm6, 16
    vpsrad  ymm11, ymm11, 24            ; nx
    vpsrld  ymm12, ymm13, 16
    vpand   ymm12, ymm12, [fl_255]      ; t[+1]
    vpand   ymm14, ymm13, [fl_255]      ; t[−1]
    vpsubd  ymm12, ymm12, ymm14
    vpsrad  ymm12, ymm12, 4
    vpaddd  ymm11, ymm11, ymm12
    vpslld  ymm12, ymm6, 8
    vpsrad  ymm12, ymm12, 24            ; ny
    vpsubd  ymm9, ymm9, ymm8
    vpsrad  ymm9, ymm9, 4
    vpaddd  ymm12, ymm12, ymm9
    vpmaxsd ymm11, ymm11, [fl_m8]
    vpmaxsd ymm12, ymm12, [fl_m8]
    vpminsd ymm11, ymm11, [fl_7]
    vpminsd ymm12, ymm12, [fl_7]
    vpaddd  ymm11, ymm11, [fl_8]
    vpaddd  ymm12, ymm12, [fl_8]
    vbroadcasti128 ymm14, [q5]
    vpshufb ymm11, ymm14, ymm11         ; q5[nx] (q5[0] = 0 — старшие байты дворда)
    vpshufb ymm12, ymm14, ymm12         ; q5[ny]
    vpslld  ymm9, ymm12, 2
    vpaddd  ymm9, ymm9, ymm12
    vpaddd  ymm9, ymm9, ymm11
    vpcmpeqd ymm15, ymm15, ymm15
    vpxor   ymm8, ymm8, ymm8
    vpgatherdd ymm8, [r15 + ymm9*1], ymm15
    vpand   ymm8, ymm8, [fl_255]
    vpblendvb ymm7, ymm8, ymm7, ymm10
.nrel:
    vmovdqa [rsp + FX_N], ymm7
    ; ----- дощатый пол
    vmovdqa ymm4, [rsp + FX_WOOD]
    vpand   ymm4, ymm4, ymm2
    vptest  ymm4, ymm4
    jz      .nwood
    vpsrld  ymm7, ymm1, 6               ; доска
    vpmulld ymm8, ymm7, [fl_97]
    vpaddd  ymm8, ymm8, ymm0            ; x со сдвигом стыков
    vpsrld  ymm9, ymm8, 8
    vmovdqa ymm10, ymm7
    HASHV   ymm9, ymm10
    vpsrld  ymm10, ymm0, 2
    vmovdqa ymm11, ymm7
    HASHV   ymm10, ymm11
    vpand   ymm9, ymm9, [fl_31]
    vpsubd  ymm9, ymm9, [fl_16]
    vpand   ymm10, ymm10, [fl_7]
    vpaddd  ymm9, ymm9, ymm10           ; разброс
    vpmulld ymm10, ymm9, [fl_3]         ; · wood_var >> 3, + wood_base
    vpsrad  ymm10, ymm10, 3
    vpaddd  ymm10, ymm10, [fl_40]
    vpmulld ymm11, ymm9, [fl_5]
    vpsrad  ymm11, ymm11, 3
    vpaddd  ymm11, ymm11, [fl_72]
    vpmulld ymm12, ymm9, [fl_7]
    vpsrad  ymm12, ymm12, 3
    vpaddd  ymm12, ymm12, [fl_112]
    ; стык: (wy & 63) < 4 или (x & 255) < 3 — вдвое темнее
    vpand   ymm13, ymm1, [fl_63]
    vmovdqa ymm14, [fl_4]
    vpcmpgtd ymm13, ymm14, ymm13
    vpand   ymm14, ymm8, [fl_255]
    vmovdqa ymm15, [fl_3]
    vpcmpgtd ymm14, ymm15, ymm14
    vpor    ymm13, ymm13, ymm14
    vpsrld  ymm14, ymm10, 1
    vpblendvb ymm10, ymm10, ymm14, ymm13
    vpsrld  ymm14, ymm11, 1
    vpblendvb ymm11, ymm11, ymm14, ymm13
    vpsrld  ymm14, ymm12, 1
    vpblendvb ymm12, ymm12, ymm14, ymm13
    vmovdqa ymm4, [rsp + FX_WOOD]
    vmovdqa ymm14, [rsp + FX_B]
    vpblendvb ymm14, ymm14, ymm10, ymm4
    vmovdqa [rsp + FX_B], ymm14
    vmovdqa ymm14, [rsp + FX_G]
    vpblendvb ymm14, ymm14, ymm11, ymm4
    vmovdqa [rsp + FX_G], ymm14
    vmovdqa ymm14, [rsp + FX_R]
    vpblendvb ymm14, ymm14, ymm12, ymm4
    vmovdqa [rsp + FX_R], ymm14
    vmovdqa ymm14, [rsp + FX_FL]
    vpblendvb ymm14, ymm14, [fl_fl0], ymm4
    vmovdqa [rsp + FX_FL], ymm14
    vmovdqa ymm14, [rsp + FX_N]
    vpandn  ymm14, ymm4, ymm14          ; пол ровный
    vmovdqa [rsp + FX_N], ymm14
.nwood:
    ; ----- кровь: (x·(256 − b) + b·blood_col) >> 8; без крови (b = 0) — x
    vpcmpeqd ymm15, ymm15, ymm15
    vpxor   ymm9, ymm9, ymm9
    vpgatherdd ymm9, [rbx + ymm3*1 + (blood - albedo)], ymm15
    vpand   ymm9, ymm9, [fl_255]
    vmovdqa ymm8, [fl_256]
    vpsubd  ymm8, ymm8, ymm9            ; 256 − b
    vpslld  ymm9, ymm9, 16              ; b << 16
    vmovdqa ymm10, [rsp + FX_B]
    FLBLOOD ymm10, fl_bb
    vmovdqa ymm11, [rsp + FX_G]
    FLBLOOD ymm11, fl_bg
    vmovdqa ymm12, [rsp + FX_R]
    FLBLOOD ymm12, fl_br
    ; альбедо: B | G << 8 | R << 16 | флаги; пустота — 0
    vpslld  ymm11, ymm11, 8
    vpslld  ymm12, ymm12, 16
    vpor    ymm10, ymm10, ymm11
    vpor    ymm10, ymm10, ymm12
    vpor    ymm10, ymm10, [rsp + FX_FL]
    vpand   ymm10, ymm10, ymm2
    vmovdqu [rdi], ymm10
    ; нормаль — байтами; у пустоты — прежняя
    vmovdqa ymm7, [rsp + FX_N]
    vpshufb ymm7, ymm7, [fl_nbs]
    vextracti128 xmm8, ymm7, 1
    vpunpckldq xmm7, xmm7, xmm8
    vpshufb ymm9, ymm2, [fl_nbs]
    vextracti128 xmm8, ymm9, 1
    vpunpckldq xmm9, xmm9, xmm8
    vmovq   xmm8, [rsi]
    vpblendvb xmm7, xmm8, xmm7, xmm9
    vmovq   [rsi], xmm7
    jmp     .next
.void:
    vpxor   ymm3, ymm3, ymm3
    vmovdqu [rdi], ymm3
    vmovdqu [rdi + (gposb - albedo)], ymm3
.next:
    add     rdi, 32
    add     rsi, 8
    vpaddd  ymm0, ymm0, [fl_64]         ; 8 пикселей: wx += 64, wy −= 64
    vpsubd  ymm1, ymm1, [fl_64]
    dec     r8d
    jnz     .grp
    inc     r12d
    cmp     r12d, r13d
    jb      .row
    vzeroupper
    mov     rsp, rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- объекты
; (сами объекты и крыши рисуются полосами: objects.inc, roofdraw.inc, bands.inc)
; ecx = wx, edx = wy (8.8) -> eax = видимость 0..255 (ecx, edx сохраняются)
vis_at:
    lea     eax, [ecx + 32]
    sar     eax, 6
    sub     eax, [gox]
    cmp     eax, GW
    jae     .no
    lea     r8d, [edx + 32]
    sar     r8d, 6
    sub     r8d, [goy]
    cmp     r8d, GW
    jae     .no
    imul    r8d, r8d, GW
    add     eax, r8d
    lea     r8, [vgrid]                 ; резкая: сглаженная пускала бы за угол
    movzx   eax, byte [r8 + rax]
    ret
.no:
    xor     eax, eax
    ret

; оттенки брёвен для быстрого пути draw_block: shade_col(c_log / c_logend, f), f = 0..256
init_blk_lut:
    xor     ecx, ecx
.l:
    lea     rdx, [c_log]
    call    shade_col
    lea     r8, [lut_log]
    mov     [r8 + rcx*4], eax
    lea     rdx, [c_logend]
    call    shade_col
    lea     r8, [lut_logend]
    mov     [r8 + rcx*4], eax
    inc     ecx
    cmp     ecx, 256
    jbe     .l
    ret

; rdx -> 4 слова (B,G,R,0), ecx = множитель 0..256 -> eax
shade_col:
    movq    xmm0, [rdx]
    movd    xmm1, ecx
    pshuflw xmm1, xmm1, 0
    pmullw  xmm0, xmm1
    psrlw   xmm0, 8
    packuswb xmm0, xmm0
    movd    eax, xmm0
    or      eax, 0x01000000
    ret

; ---------------------------------------------------------------- освещение
; на пиксель: билинейка сетки света (SSE2) или свой свет (крыши);
; луна — отдельно: высота пикселя z из gpos и строки экрана, мягкая тень — 2x2 ячейки
; развёртки mz8 (z против горизонта через полутень ячейки), облака из mzg (билинейно),
; сила — по нормали (moon_n);
; albedo * L, память вне обзора — серым.
; Все попиксельные буферы и таблицы адресуются от albedo (одна секция .bss)
light_pass:
    sub     rsp, 40
    lea     rcx, [light_rows]
    cmp     dword [have_avx2], 0
    je      .sse
    lea     rcx, [light_rows_x]         ; то же на AVX2, по 4 пикселя (lightx.inc)
.sse:
    mov     edx, SCR_H
    mov     r8d, 8
    call    par_for
    inc     dword [frame_no]
    add     rsp, 40
    ret

; строки ecx .. edx−1 (с любого потока пула: пишет только свои пиксели кадра)
light_rows:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 56
    imul    eax, edx, SCR_W
    mov     [rsp], eax                  ; конец диапазона, пиксель
    mov     ebx, CY0                    ; CY0 − строка
    sub     ebx, ecx
    imul    ecx, ecx, SCR_W
    lea     rsi, [albedo]
    mov     ebp, [grain_off]
.row:
    ; свет огней этой строки — попиксельно, в dlb (lights.inc; портит xmm и r8–r11)
    mov     [rsp + 8], ecx
    mov     ecx, CY0
    sub     ecx, ebx
    call    dl_row
    mov     ecx, [rsp + 8]
    mov     r8, [fbbits]
    mov     r9d, [camx]
    add     r9d, [camy]
    mov     r10d, [gox]
    shl     r10d, 6
    mov     r11d, [goy]
    shl     r11d, 6
    pxor    xmm15, xmm15
    movdqa  xmm14, [tint_vec]
    movdqa  xmm13, [memtint_v]          ; в мгле «память» светлее
    movdqa  xmm12, [w255]
    movdqa  xmm11, [gray_w]
    mov     edi, SCR_W
.px:
    mov     eax, [rsi + rcx*4]
    test    eax, eax
    jz      .black
    js      .emissive
    movd    xmm4, eax
    mov     edx, [rsi + rcx*4 + (gposb - albedo)]
    movzx   r12d, dx
    mov     r13d, edx
    shr     r13d, 16
    lea     r14d, [r12d + r13d]
    sub     r14d, r9d
    sar     r14d, 5
    add     r14d, ebx                   ; z пикселя, px
    ; луна: 2x2 ячейки развёртки (8 на тайл) вокруг точки, сдвинутой к луне по нормали;
    ; каждая сравнивает z со своим горизонтом через полутень, результат — билинейно
    xor     r15d, r15d
    cmp     dword [moon_on], 0
    je      .nomoon
    movzx   eax, byte [rsi + rcx + (nbuf - albedo)]
    and     eax, 0x7F
    mov     r15d, [rsi + rax*8 + (msh - albedo)]
    mov     edx, [rsi + rax*8 + (msh - albedo) + 4]
    add     r15d, r12d
    add     edx, r13d
    sub     r15d, [mo_x]
    sub     edx, [mo_y]
    mov     eax, MLIM
    cmp     r15d, eax
    cmova   r15d, eax                   ; и отрицательные: за окном развёртки — край
    cmp     edx, eax
    cmova   edx, eax
    mov     eax, edx
    and     edx, 31
    movq    xmm8, [rsi + rdx*8 + (wt8y - albedo)]
    shr     eax, 5
    imul    eax, eax, SW8
    mov     edx, r15d
    shr     edx, 5
    add     eax, edx
    and     r15d, 31
    movq    xmm9, [rsi + r15*8 + (wt8x - albedo)]
    pmullw  xmm8, xmm9                  ; веса, в сумме 1024
    movq    xmm9, [rsi + rax*4 + (mz8 - albedo)]
    movq    xmm10, [rsi + rax*4 + (mz8 - albedo) + SW8*4]
    punpcklqdq xmm9, xmm10              ; горизонт+64 | облака<<8 | ip<<16
    movdqa  xmm10, xmm9
    psrld   xmm10, 16                   ; 4096 / полутень
    movdqa  xmm0, xmm9
    pslld   xmm0, 16
    psrld   xmm0, 24                    ; облака
    pslld   xmm9, 24
    psrld   xmm9, 24
    lea     eax, [r14d + 64]
    movd    xmm1, eax
    pshufd  xmm1, xmm1, 0
    psubd   xmm1, xmm9
    pmaddwd xmm1, xmm10                 ; (z − hz)·ip
    psrad   xmm1, 4
    paddd   xmm1, [d128]
    packssdw xmm1, xmm1
    pmaxsw  xmm1, xmm15
    pminsw  xmm1, [w256]                ; 0..256 на ячейку
    packssdw xmm0, xmm0
    pmullw  xmm1, xmm0
    psrlw   xmm1, 8                     ; · облака: 0..255
    pmaddwd xmm1, xmm8
    pshufd  xmm9, xmm1, 1
    paddd   xmm1, xmm9
    movd    r15d, xmm1
    shr     r15d, 10                    ; дошло луны 0..255
    movzx   eax, byte [rsi + rcx + (mfsh - albedo)]
    inc     eax
    imul    r15d, eax
    shr     r15d, 8                     ; и мимо фигур (dl_row)
.nomoon:
    ; AO у земли (ao16.inc): билинейно у точки; к AO_ZF px на нет -> xmm6 = (m, m, m,
    ; 256), m 0..256 (свой свет — без него)
    movq    xmm6, [lt_w256]
    cmp     r14d, AO_ZF
    jge     .noao
    test    byte [rsi + rcx + (nbuf - albedo)], NOWN
    jnz     .noao
    lea     eax, [r12d - 16]
    lea     edx, [r13d - 16]
    cmp     eax, AOLIM
    cmova   eax, [ao_lim]
    cmp     edx, AOLIM
    cmova   edx, [ao_lim]
    mov     [rsp + 16], eax
    and     eax, 31
    movq    xmm9, [rsi + rax*8 + (wt8x - albedo)]
    mov     eax, edx
    and     edx, 31
    movq    xmm8, [rsi + rdx*8 + (wt8y - albedo)]
    pmullw  xmm8, xmm9                  ; веса 2x2, в сумме 1024
    shr     eax, 5
    shl     eax, 10                     ; · AOW
    mov     edx, [rsp + 16]
    shr     edx, 5
    add     eax, edx
    movzx   edx, word [rsi + rax + (ao16 - albedo) + AOW]
    movzx   eax, word [rsi + rax + (ao16 - albedo)]
    shl     edx, 16
    or      eax, edx
    movd    xmm9, eax
    punpcklbw xmm9, xmm15
    pmaddwd xmm9, xmm8
    pshufd  xmm8, xmm9, 1
    paddd   xmm9, xmm8
    movd    eax, xmm9
    shr     eax, 10                     ; AO 0..255
    mov     edx, eax
    shr     edx, 7
    add     eax, edx                    ; 0..256
    neg     eax
    add     eax, 256
    mov     edx, AO_ZF
    sub     edx, r14d
    cmp     edx, AO_ZF
    cmovg   edx, [ao_zf]                ; ниже земли — целиком
    imul    eax, edx
    shr     eax, 5
    neg     eax
    add     eax, 256
    movd    xmm6, eax
    pshuflw xmm6, xmm6, 0
    pinsrw  xmm6, [lt_w256], 3          ; видимость — как есть
.noao:
    sub     r12d, r10d
    sub     r13d, r11d
    xor     eax, eax
    cmp     r12d, eax
    cmovl   r12d, eax
    cmp     r13d, eax
    cmovl   r13d, eax
    mov     eax, GLIM
    cmp     r12d, eax
    cmovg   r12d, eax
    cmp     r13d, eax
    cmovg   r13d, eax
    mov     eax, r13d
    shr     eax, 6
    imul    eax, eax, GW
    mov     edx, r12d
    shr     edx, 6
    add     eax, edx                    ; сэмпл
    and     r12d, 63
    shl     r12d, 4
    and     r13d, 63
    shl     r13d, 4
    movdqa  xmm2, [rsi + r12 + (wtab - albedo)]
    movdqa  xmm3, [rsi + r13 + (wtab - albedo)]
    mov     r13d, eax
    xchg    r14d, r15d                  ; r14d — луна дошла (с облаками) 0..255, r15d — z
    movzx   r12d, byte [rsi + rcx + (nbuf - albedo)]
    test    r12d, NOWN
    jnz     .own
    movq    xmm0, [rsi + r13*4 + (lgs - albedo)]
    movq    xmm1, [rsi + r13*4 + (lgs - albedo) + GW*4]
    punpcklbw xmm0, xmm15
    punpcklbw xmm1, xmm15
    pmullw  xmm0, xmm2
    pmullw  xmm1, xmm2
    movdqa  xmm7, xmm0
    psrldq  xmm7, 8
    paddw   xmm0, xmm7
    movdqa  xmm7, xmm1
    psrldq  xmm7, 8
    paddw   xmm1, xmm7
    psrlw   xmm0, 6
    psrlw   xmm1, 6
    punpcklqdq xmm0, xmm1
    pmullw  xmm0, xmm3
    movdqa  xmm7, xmm0
    psrldq  xmm7, 8
    paddw   xmm0, xmm7
    psrlw   xmm0, 6                     ; L: B,G,R,видимость
    movq    xmm7, [rsi + r12*8 + (amb_d - albedo)]
    psubusw xmm0, xmm7                  ; грань видит меньше неба
    pmullw  xmm0, xmm6
    psrlw   xmm0, 8                     ; и низ у стен, заборов, стволов — AO
    jmp     .moon
.own:
    and     r12d, 0x7F
    movd    xmm0, [rsi + rcx*4 + (lbuf - albedo)]
    punpcklbw xmm0, xmm15
.moon:
    test    r14d, r14d
    jz      .lit
    movd    xmm1, r14d
    pshuflw xmm1, xmm1, 0
    movq    xmm7, [rsi + r12*8 + (moon_n - albedo)]
    pmullw  xmm1, xmm7
    psrlw   xmm1, 8
    paddw   xmm0, xmm1
.lit:
    ; огни — попиксельно (dl_row); свет до 1023, видимость — до 255
    movq    xmm1, [rsi + rcx*8 + (dlb - albedo)]
    paddw   xmm0, xmm1
    pminsw  xmm0, [w_lmax]
    ; альбедо·L/128: (a << 7)·(L << 2) >> 16 — без переполнения слова
    punpcklbw xmm4, xmm15
    psllw   xmm4, 7
    movdqa  xmm1, xmm0
    psllw   xmm1, 2
    pmulhuw xmm4, xmm1
    test    dword [rsi + rcx*4], 0x40000000
    jz      .nospark
    paddw   xmm4, xmm4                  ; искра на снегу
.nospark:
    ; пересвет белеет: каждому каналу — доля сильнейшего сверх порога (grade.inc)
    pshuflw xmm5, xmm4, 0xC9            ; G R B A
    pmaxsw  xmm5, xmm4
    pshuflw xmm6, xmm4, 0xD2            ; R B G A
    pmaxsw  xmm5, xmm6
    psubusw xmm5, [hl_k2]
    pmulhuw xmm5, [hl_kk]
    paddw   xmm4, xmm5
    ; тоновая кривая (с экспозицией глаза): пересвет плавно к 255
    pminsw  xmm4, [w_2047]
    pextrw  eax, xmm4, 0
    mov     eax, [rsi + rax*4 + (tm_lut - albedo)]
    pinsrw  xmm4, eax, 0
    pextrw  eax, xmm4, 1
    mov     eax, [rsi + rax*4 + (tm_lut - albedo)]
    pinsrw  xmm4, eax, 1
    pextrw  eax, xmm4, 2
    mov     eax, [rsi + rax*4 + (tm_lut - albedo)]
    pinsrw  xmm4, eax, 2
    pextrw  eax, xmm4, 3
    mov     eax, [rsi + rax*4 + (tm_lut - albedo)]
    pinsrw  xmm4, eax, 3
    ; ----- позёмка: у самой земли (z < 12 px) там, где метёт, — змейки снега по ветру;
    ; прячут следы и искры, заметают ноги и низ стен. Маска — сетка ветра (билинейно),
    ; полосы — два слоя текстуры в осях ветра, бегут с разной скоростью
    cmp     r15d, 12
    jge     .nodr
    movzx   eax, word [rsi + r13 + (wgrid - albedo)]
    movzx   edx, word [rsi + r13 + (wgrid - albedo) + GW]
    mov     r12d, eax
    or      r12d, edx
    jz      .nodr
    pextrw  r12d, xmm2, 4               ; fx 0..63
    pextrw  r14d, xmm3, 4               ; fy
    mov     r13d, eax
    shr     r13d, 8
    and     eax, 255
    sub     r13d, eax
    imul    r13d, r12d
    shl     eax, 6
    add     eax, r13d                   ; верхняя строка ·64
    mov     r13d, edx
    shr     r13d, 8
    and     edx, 255
    sub     r13d, edx
    imul    r13d, r12d
    shl     edx, 6
    add     edx, r13d                   ; нижняя
    sub     edx, eax
    imul    edx, r14d
    sar     edx, 6
    add     eax, edx
    shr     eax, 6                      ; M 0..255
    jz      .nodr
    mov     r14d, eax
    ; точка пикселя в осях ветра (от середины карты): вдоль, поперёк
    mov     edx, [rsi + rcx*4 + (gposb - albedo)]
    movzx   eax, dx
    sub     eax, 16384
    shr     edx, 16
    sub     edx, 16384
    mov     r12d, eax
    imul    r12d, [pz_dx]
    mov     r13d, edx
    imul    r13d, [pz_dy]
    add     r12d, r13d                  ; вдоль, тайлы·65536
    imul    edx, [pz_dx]
    imul    eax, [pz_dy]
    sub     edx, eax
    sar     edx, 11                     ; поперёк, 1/32 тайла
    ; слой 1: тексель 1/16 тайла вдоль
    mov     eax, r12d
    sar     eax, 12
    sub     eax, [pz_s1]
    and     eax, 255
    mov     r13d, edx
    and     r13d, 63
    shl     r13d, 8
    add     eax, r13d
    movzx   r13d, byte [rsi + rax + (strk - albedo)]
    ; слой 2: полосы короче (1/24 тайла), поперёк зеркально
    lea     eax, [r12d + r12d*2]
    sar     eax, 13
    sub     eax, [pz_s2]
    and     eax, 255
    neg     edx
    add     edx, 29
    and     edx, 63
    shl     edx, 8
    add     eax, edx
    movzx   eax, byte [rsi + rax + (strk - albedo)]
    add     eax, r13d
    ; доля: M·(полосы + дымка), у земли целиком, к 12 px на нет
    add     eax, 50
    imul    eax, r14d
    shr     eax, 11
    mov     edx, r15d
    xor     r13d, r13d
    cmp     edx, r13d
    cmovl   edx, r13d
    neg     edx
    add     edx, 12
    imul    eax, edx
    imul    eax, eax, 21
    shr     eax, 8
    mov     edx, 60
    cmp     eax, edx
    cmova   eax, edx
    test    eax, eax
    jz      .nodr
    ; цвет летящего снега: свет·190/64 — светлее лежащего (без самозатенения), не ярче 255
    movdqa  xmm7, xmm0
    pminsw  xmm7, xmm12
    pmullw  xmm7, [w_drift]
    psrlw   xmm7, 6
    pminsw  xmm7, xmm12
    psubw   xmm7, xmm4
    movd    xmm6, eax
    pshuflw xmm6, xmm6, 0
    pmullw  xmm7, xmm6
    psraw   xmm7, 6
    paddw   xmm4, xmm7
.nodr:
    packuswb xmm4, xmm4
    punpcklbw xmm4, xmm15               ; lit, насыщено до 255
    pextrw  eax, xmm0, 3
    cmp     eax, 255
    jae     .tint                       ; видно целиком — «память» не подмешиваем
    pshuflw xmm5, xmm0, 0xFF            ; V в каждом слове
    movdqa  xmm6, xmm4
    pmaddwd xmm6, xmm11
    pshufd  xmm7, xmm6, 1
    paddd   xmm6, xmm7
    psrld   xmm6, 2
    pshuflw xmm6, xmm6, 0               ; яркость
    pmullw  xmm6, xmm13
    psrlw   xmm6, 7                     ; «память» — тусклый серо-синий
    paddw   xmm6, [haze_v]              ; в мгле — белёсая пелена
    pmullw  xmm4, xmm5
    movdqa  xmm7, xmm12
    psubw   xmm7, xmm5
    pmullw  xmm6, xmm7
    paddw   xmm4, xmm6
    psrlw   xmm4, 8
.tint:
    pmullw  xmm4, xmm14
    psrlw   xmm4, 8
.post:
    movzx   eax, byte [rsi + rcx + (vign - albedo)]
    movd    xmm7, eax
    pshuflw xmm7, xmm7, 0
    pmullw  xmm4, xmm7
    psrlw   xmm4, 8
    lea     eax, [rcx + rbp]
    and     eax, 0xFFFF
    movsx   eax, byte [rsi + rax + (grain - albedo)]
    movd    xmm7, eax
    pshuflw xmm7, xmm7, 0
    paddsw  xmm4, xmm7
    packuswb xmm4, xmm4
    movd    [r8 + rcx*4], xmm4
.next:
    inc     ecx
    dec     edi
    jnz     .px
    dec     ebx
    cmp     ecx, [rsp]
    jb      .row
    jmp     .out
.emissive:
    movd    xmm4, eax
    punpcklbw xmm4, xmm15
    jmp     .post
.black:
    mov     dword [r8 + rcx*4], 0
    jmp     .next
.out:
    add     rsp, 56
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- снегопад
draw_flakes:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    mov     r12d, [cam_a]
    mov     r13d, [cam_b]
    mov     r14, [fbbits]
    lea     rsi, [flakes]
    mov     ebp, [flk_n]
    test    ebp, ebp
    jz      .out
.l:
    cvttss2si ebx, [rsi]
    sub     ebx, r12d
    add     ebx, CX0 + 192
    and     ebx, 1023
    sub     ebx, 192                    ; sx
    cmp     ebx, SCR_W
    jae     .n
    cvttss2si ecx, [rsi+4]
    sub     ecx, r13d
    add     ecx, CY0 + 76
    and     ecx, 511
    sub     ecx, 76                     ; земля под снежинкой
    ; ледяная пыль в мороз — не снежинка: искрит, где на неё падает свет
    mov     rax, rsi
    lea     rdx, [flakes]
    sub     rax, rdx
    shr     eax, 5
    imul    eax, eax, 0x2545F491
    shr     eax, 24
    cmp     eax, [fr_dth]
    jb      .dust
    cvttss2si edx, [rsi+8]
    mov     edi, ecx
    sub     edi, edx                    ; sy
    cmp     edi, SCR_H-1
    jae     .n
    ; свет в точке под снежинкой
    mov     eax, ebx
    sub     eax, CX0                    ; a'
    sub     ecx, CY0                    ; b'
    shl     ecx, 4
    lea     r8d, [eax*8]
    mov     r9d, [camx]
    add     r9d, r8d
    add     r9d, ecx                    ; wx
    mov     r10d, [camy]
    add     r10d, ecx
    sub     r10d, r8d                   ; wy
    sar     r9d, 6
    sub     r9d, [gox]
    sar     r10d, 6
    sub     r10d, [goy]
    xor     eax, eax
    cmp     r9d, eax
    cmovl   r9d, eax
    cmp     r10d, eax
    cmovl   r10d, eax
    mov     eax, GW-1
    cmp     r9d, eax
    cmovg   r9d, eax
    cmp     r10d, eax
    cmovg   r10d, eax
    imul    r10d, r10d, GW
    add     r10d, r9d
    lea     r8, [lgrid]
    mov     eax, [r8 + r10*4]
    movzx   ecx, al
    movzx   edx, ah
    add     ecx, edx
    add     ecx, edx
    shr     eax, 16
    movzx   eax, al
    add     ecx, eax
    shr     ecx, 2                      ; яркость света
    lea     ecx, [ecx*3]
    shr     ecx, 1
    add     ecx, [flk_base]             ; в густой снег воздух светлее
    add     ecx, [flk_moon]             ; луна (в сетке её больше нет)
    movss   xmm0, [rsi+12]
    comiss  xmm0, [f_40]
    jae     .near
    imul    ecx, ecx, 180
    shr     ecx, 8
.near:
    mov     eax, 255
    cmp     ecx, eax
    cmova   ecx, eax
    mov     r8d, ecx                    ; яркость
    imul    eax, ecx, 0x010101
    movd    xmm6, eax
    imul    eax, edi, SCR_W
    add     eax, ebx
    movd    xmm7, [r14 + rax*4]
    pmaxub  xmm7, xmm6
    movd    [r14 + rax*4], xmm7
    movss   xmm2, [rsi+12]
    comiss  xmm2, [f_46]
    jb      .tail
    movd    xmm7, [r14 + rax*4 + SCR_W*4]
    pmaxub  xmm7, xmm6
    movd    [r14 + rax*4 + SCR_W*4], xmm7
.tail:
    ; в ветер снежинка летит быстро — хвост назад по скорости (выдержка ~1/48 с), гаснет
    movss   xmm0, [rsi+16]              ; vu
    movss   xmm1, [rsi+20]
    addss   xmm1, [rsi+12]              ; vv + падение
    movaps  xmm2, xmm0
    mulss   xmm2, xmm0
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    addss   xmm2, xmm3
    sqrtss  xmm2, xmm2
    movaps  xmm3, xmm2
    mulss   xmm3, [f_inv48]
    cvttss2si r15d, xmm3
    test    r15d, r15d
    jz      .n
    mov     eax, 5
    cmp     r15d, eax
    cmova   r15d, eax
    movss   xmm3, [f_1]
    divss   xmm3, xmm2
    mulss   xmm0, xmm3
    mulss   xmm1, xmm3                  ; единичный вектор хода
    cvtsi2ss xmm2, r8d
    lea     eax, [r15d + 1]
    cvtsi2ss xmm3, eax
    movaps  xmm5, xmm2
    divss   xmm5, xmm3                  ; убыль яркости на шаг
    cvtsi2ss xmm3, ebx
    cvtsi2ss xmm4, edi
.tk:
    subss   xmm3, xmm0
    subss   xmm4, xmm1
    subss   xmm2, xmm5
    cvtss2si eax, xmm3
    cmp     eax, SCR_W
    jae     .tn
    cvtss2si edx, xmm4
    cmp     edx, SCR_H
    jae     .tn
    imul    edx, edx, SCR_W
    add     eax, edx
    cvttss2si edx, xmm2
    imul    edx, edx, 0x010101
    movd    xmm6, edx
    movd    xmm7, [r14 + rax*4]
    pmaxub  xmm7, xmm6
    movd    [r14 + rax*4], xmm7
.tn:
    dec     r15d
    jnz     .tk
.n:
    add     rsi, FL_SZ
    dec     ebp
    jnz     .l
.out:
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
    ; ----- кристалл ледяной пыли: грань ловит свет на миг — (max(0, sin))^8 со своей
    ; частотой; свет в точке — огни кадра (фонари, окна: (1 − d²/R²)²), конус фонарика,
    ; бледно — луна. Пиксель — прибавкой, яркий — крестиком; что ближе к глазу, закрывает
.dust:
    ; висит ниже снега: высота ·0.3 (до ~100 px) — у ламп и в луче фонарика
    movss   xmm10, [rsi + 8]
    mulss   xmm10, [dd_zk]
    cvttss2si edx, xmm10
    mov     edi, ecx
    sub     edi, edx                    ; sy
    cmp     edi, SCR_H-1
    jae     .n
    lea     eax, [ebx - CX0]            ; a'
    lea     r8d, [ecx - CY0]            ; b'
    shl     r8d, 4
    shl     eax, 3
    mov     r9d, [camx]
    add     r9d, eax
    add     r9d, r8d                    ; wx
    mov     r10d, [camy]
    add     r10d, r8d
    sub     r10d, eax                   ; wy
    movss   xmm0, [rsi + 24]
    mulss   xmm0, [f_7]
    movss   xmm1, [rsi + 28]
    mulss   xmm1, [dd_tw]
    addss   xmm1, [dd_tw0]
    mulss   xmm1, [wind_t]
    addss   xmm0, xmm1
    movss   [dd_t], xmm10
    call    fsin
    movss   xmm10, [dd_t]
    maxss   xmm0, [f_0]
    mulss   xmm0, xmm0
    mulss   xmm0, xmm0
    mulss   xmm0, xmm0
    mulss   xmm0, xmm0                  ; ^16 — вспыхивает на миг
    comiss  xmm0, [dd_eps]
    jb      .n
    movss   [dd_t], xmm0
    cvtsi2ss xmm8, r9d                  ; точка: мир 8.8, высота px
    cvtsi2ss xmm9, r10d
    xorps   xmm11, xmm11                ; свет B, G, R
    lea     r8, [al_buf]
    mov     ecx, [n_al]
.dl:
    test    ecx, ecx
    jz      .dfl
    mov     eax, [r8 + AL_KIND]
    movss   xmm5, [r8 + AL_IR2]
    cmp     eax, LK_LAMP
    je      .dl1
    cmp     eax, LK_BROKEN
    je      .dl1
    cmp     eax, LK_WIN
    jne     .dln
    movss   xmm5, [r8 + AL_IRG]         ; окно — только у стекла
.dl1:
    movss   xmm0, xmm8
    subss   xmm0, [r8 + AL_X]
    mulss   xmm0, [dd_pxu]
    mulss   xmm0, xmm0
    movss   xmm1, xmm9
    subss   xmm1, [r8 + AL_Y]
    mulss   xmm1, [dd_pxu]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    movss   xmm1, xmm10
    subss   xmm1, [r8 + AL_Z]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    mulss   xmm0, xmm5
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    comiss  xmm1, [f_0]
    jbe     .dln
    movaps  xmm0, xmm1
    mulss   xmm1, xmm1
    mulss   xmm1, xmm0                  ; (1 − d²/R²)³ — искрит у самой лампы
    shufps  xmm1, xmm1, 0
    movups  xmm0, [r8 + AL_COL]
    mulps   xmm0, xmm1
    addps   xmm11, xmm0
.dln:
    add     r8, AL_SZ
    dec     ecx
    jmp     .dl
.dfl:
    cmp     dword [dl_fon], 0
    je      .dmoon
    movss   xmm0, xmm8
    subss   xmm0, [dl_sx]
    mulss   xmm0, [dd_pxu]              ; от руки, px-единицы
    movss   xmm1, xmm9
    subss   xmm1, [dl_sy]
    mulss   xmm1, [dd_pxu]
    movss   xmm2, xmm10
    subss   xmm2, [lt_handz]
    movss   xmm3, xmm0
    mulss   xmm3, [dl_ax]
    movss   xmm4, xmm1
    mulss   xmm4, [dl_ay]
    addss   xmm3, xmm4
    movss   xmm4, xmm2
    mulss   xmm4, [dl_az]
    addss   xmm3, xmm4                  ; s — вдоль оси луча
    comiss  xmm3, [dd_smin]
    jbe     .dmoon
    comiss  xmm3, [dl_frc]
    jae     .dmoon
    mulss   xmm0, xmm0
    mulss   xmm1, xmm1
    mulss   xmm2, xmm2
    addss   xmm0, xmm1
    addss   xmm0, xmm2
    movss   xmm4, xmm3
    mulss   xmm4, xmm3
    subss   xmm0, xmm4                  ; h² — от оси
    movss   xmm1, xmm3
    mulss   xmm1, [lt_btan]
    mulss   xmm1, xmm1                  ; r² конуса
    divss   xmm0, xmm1
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    comiss  xmm1, [f_0]
    jbe     .dmoon
    ; луч не прошёл сквозь стену: точка под кристаллом видна от игрока (losg)
    mov     eax, r9d
    sar     eax, 6
    sub     eax, [gox]
    cmp     eax, GW
    jae     .dmoon
    mov     edx, r10d
    sar     edx, 6
    sub     edx, [goy]
    cmp     edx, GW
    jae     .dmoon
    imul    edx, edx, GW
    add     eax, edx
    lea     rdx, [losg]
    cmp     byte [rdx + rax], 0
    je      .dmoon
    mulss   xmm1, xmm1
    mulss   xmm4, [dd_fk]
    addss   xmm4, [f_1]
    divss   xmm1, xmm4
    mulss   xmm1, [flash_i]
    mulss   xmm1, [dd_fg]
    shufps  xmm1, xmm1, 0
    movups  xmm0, [dd_fcol]
    mulps   xmm0, xmm1
    addps   xmm11, xmm0
.dmoon:
    cvtsi2ss xmm0, dword [flk_moon]
    mulss   xmm0, [dd_mg]
    shufps  xmm0, xmm0, 0
    mulps   xmm0, [dd_mcol]
    addps   xmm11, xmm0
    movss   xmm0, [dd_t]
    mulss   xmm0, [dd_gain]
    shufps  xmm0, xmm0, 0
    mulps   xmm11, xmm0
    cvttps2dq xmm11, xmm11
    packssdw xmm11, xmm11
    packuswb xmm11, xmm11
    movd    eax, xmm11
    test    eax, eax
    jz      .n
    imul    eax, edi, SCR_W
    add     eax, ebx
    lea     rdx, [gposb]
    mov     ecx, [rdx + rax*4]
    movzx   r8d, cx
    shr     ecx, 16
    add     ecx, r8d
    lea     r8d, [r9d + r10d + 64]
    cmp     ecx, r8d
    jg      .n
    movd    xmm0, [r14 + rax*4]
    paddusb xmm0, xmm11
    movd    [r14 + rax*4], xmm0
    ; яркий — крестиком в полсилы
    movd    ecx, xmm11
    mov     edx, ecx
    shr     edx, 8
    and     edx, 255
    shr     ecx, 16
    and     ecx, 255
    add     ecx, edx
    cmp     ecx, 260
    jb      .n
    pxor    xmm1, xmm1
    pavgb   xmm11, xmm1
    movd    xmm0, [r14 + rax*4 + SCR_W*4]
    paddusb xmm0, xmm11
    movd    [r14 + rax*4 + SCR_W*4], xmm0
    test    edi, edi
    jz      .dc1
    movd    xmm0, [r14 + rax*4 - SCR_W*4]
    paddusb xmm0, xmm11
    movd    [r14 + rax*4 - SCR_W*4], xmm0
.dc1:
    test    ebx, ebx
    jz      .dc2
    movd    xmm0, [r14 + rax*4 - 4]
    paddusb xmm0, xmm11
    movd    [r14 + rax*4 - 4], xmm0
.dc2:
    cmp     ebx, SCR_W - 1
    jae     .n
    movd    xmm0, [r14 + rax*4 + 4]
    paddusb xmm0, xmm11
    movd    [r14 + rax*4 + 4], xmm0
    jmp     .n

section .rdata
align 16, db 0
dd_fcol:    dd 0.82, 0.94, 1.0, 0.0     ; фонарик: B, G, R
dd_mcol:    dd 1.0, 0.95, 0.85, 0.0     ; луна
f_2400:     dd 2400.0
dd_tw:      dd 1.3                      ; мерцание: 1.5..7.4 рад/с
dd_tw0:     dd 1.5
dd_eps:     dd 0.02
dd_pxu:     dd 0.0766                   ; мир 8.8 -> px-единицы
dd_smin:    dd 3.0
dd_fk:      dd 2.5e-4                   ; затухание луча — вчетверо круче, чем в beam_pass
dd_fg:      dd 420.0
dd_mg:      dd 0.35
dd_gain:    dd 1.3
dd_zk:      dd 0.3

section .text
