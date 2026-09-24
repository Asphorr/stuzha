; ----------------------------------------------------------------------------
;  render.inc — камера, сетка света, пол, объекты, отложенное освещение, снег, HUD
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
    call    build_ntab                  ; луна и небо по нормалям
    call    moon_grid                   ; облака
    PROF    12
    call    build_flash_occ
    call    compute_lightgrid
    call    ent_shadows                 ; тени людей от фонарей и окон
    PROF    1
    call    floor_pass
    PROF    2
    ; тени луны и маска позёмки нужны только свету: считаются фоном, пока главный
    ; рисует объекты и крыши (им развёртка не нужна, а они — ей)
    lea     rcx, [sky_job]
    mov     edx, 1
    mov     r8d, 1
    call    par_async
    call    draw_objects
    PROF    3
    call    draw_roofs                  ; крыши поверх, с тестом глубины
    PROF    4
    call    draw_wires
    call    par_wait
    PROF    0
    call    light_pass
    PROF    5
    call    bloom_pass
    PROF    14
    call    draw_cones
    PROF    15
    call    draw_parts
    call    draw_swing                  ; след лопаты — где она достаёт
    PROF    16
    call    draw_flakes
    PROF    17
    call    draw_frost                  ; иней по краям кадра, когда игрок мёрзнет
    call    draw_hitboxes               ; F3
    PROF    6
    call    draw_hud
    call    draw_text                   ; надписи — очередью
    cmp     dword [pr_on], 0
    jne     .async
    mov     ecx, [fb_cur]               ; снимки: сразу, в этот же буфер
    call    tq_run
.async:
    PROF    7
    add     rsp, 32
    pop     rbx
    ret

; фоновое задание кадра: развёртка теней луны, маска позёмки (время — в sky_ticks)
sky_job:
    sub     rsp, 40
    lea     rcx, [sky_t0]
    call    [QueryPerformanceCounter]
    call    sky_sweep                   ; тени луны по высотам (+ облака в ячейках)
    call    build_wgrid                 ; где у земли метёт — маска позёмки
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

; строки сетки ecx .. edx−1 (с любого потока пула; своё — только на стеке)
LG_ST equ 32                            ; [rsp + LG_ST]: небо, огни и окна сэмпла
LG_END equ 48                           ; конец диапазона
lg_rows:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 56
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
    addps   xmm0, xmm1
    ; видимое игроком (фонарик, аура, обзор) копится отдельно — в lgv, сгладится после
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
    addps   xmm0, xmm7
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
    movaps  xmm0, [rsp + LG_ST]
    jmp     .store
.novis:
    imul    eax, r14d, GW
    add     eax, r15d
    lea     rcx, [losg]
    mov     byte [rcx + rax], 0
    movaps  xmm0, [rsp + LG_ST]
.nov2:
    lea     rcx, [vgrid]
    mov     byte [rcx + rax], 0
    mov     dword [rdi + (lgv - lgrid)], 0
    jmp     .store
.dark:
    xorps   xmm0, xmm0
    imul    eax, r14d, GW
    add     eax, r15d
    lea     rcx, [losg]
    mov     byte [rcx + rax], 0
    jmp     .nov2
.store:
    cvtps2dq xmm0, xmm0
    packssdw xmm0, xmm0
    packuswb xmm0, xmm0
    movd    [rdi], xmm0
    add     rdi, 4
    inc     r15d
    cmp     r15d, GW
    jb      .col
    inc     r14d
    cmp     r14d, [rsp + LG_END]
    jb      .row
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
    xor     ecx, ecx
.a:
    movdqa  xmm0, [rdi + rcx]
    paddusb xmm0, [rsi + rcx]
    movdqa  [rdi + rcx], xmm0
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

; ---------------------------------------------------------------- объекты
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

draw_objects:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    ; список сущностей (ключ = x+y, индекс; -1 = игрок)
    xor     r15d, r15d
    lea     rdi, [dlist]
    mov     eax, [plw_x]
    add     eax, [plw_y]
    mov     [rdi], eax
    mov     dword [rdi+4], -1
    add     rdi, 8
    inc     r15d
    lea     rbx, [ents]
    xor     r12d, r12d
.e:
    mov     eax, [rbx + E_ST]
    test    eax, eax
    jz      .en
    movss   xmm0, [rbx + E_X]
    mulss   xmm0, [f_256]
    cvtss2si ecx, xmm0
    movss   xmm0, [rbx + E_Y]
    mulss   xmm0, [f_256]
    cvtss2si edx, xmm0
    call    vis_at
    cmp     dword [rbx + E_ST], ST_CORPSE
    jne     .alive
    cmp     eax, 24
    jb      .en
    lea     eax, [ecx + edx - 160]
    jmp     .add
.alive:
    cmp     eax, 60
    jb      .en
    lea     eax, [ecx + edx]
.add:
    mov     [rdi], eax
    mov     [rdi+4], r12d
    add     rdi, 8
    inc     r15d
    cmp     r15d, 300
    jae     .built
.en:
    add     rbx, ESZ
    inc     r12d
    cmp     r12d, MAXE
    jb      .e
.built:
    ; сортировка вставками
    lea     rax, [dlist]
    mov     ecx, 1
.s1:
    cmp     ecx, r15d
    jae     .sorted
    mov     r8, [rax + rcx*8]
    mov     edx, ecx
.s2:
    test    edx, edx
    jz      .s3
    mov     r10, [rax + rdx*8 - 8]
    cmp     r10d, r8d
    jle     .s3
    mov     [rax + rdx*8], r10
    dec     edx
    jmp     .s2
.s3:
    mov     [rax + rdx*8], r8
    inc     ecx
    jmp     .s1
.sorted:
    ; обход диагоналей от дальних к ближним
    mov     eax, [camx]
    add     eax, [camy]
    sar     eax, 8
    mov     ecx, [camx]
    sub     ecx, [camy]
    sar     ecx, 8
    mov     [cmx], ecx
    lea     r12d, [eax - 24]
    lea     r13d, [eax + 34]
    xor     r14d, r14d
.d:
    lea     ebp, [r12d + 1]
    shl     ebp, 8
.fl:
    cmp     r14d, r15d
    jae     .tiles
    lea     rax, [dlist]
    cmp     [rax + r14*8], ebp
    jge     .tiles
    mov     ecx, [rax + r14*8 + 4]
    call    draw_entity
    inc     r14d
    jmp     .fl
.tiles:
    mov     esi, r12d
    add     esi, [cmx]
    sub     esi, 22
    sar     esi, 1
    lea     eax, [r12d - 127]
    cmp     esi, eax
    cmovl   esi, eax
    xor     eax, eax
    cmp     esi, eax
    cmovl   esi, eax
    mov     edi, r12d
    add     edi, [cmx]
    add     edi, 23
    sar     edi, 1
    cmp     edi, r12d
    cmovg   edi, r12d
    mov     eax, MAPW-1
    cmp     edi, eax
    cmovg   edi, eax
.tx:
    cmp     esi, edi
    jg      .dn
    mov     ecx, esi
    mov     edx, r12d
    sub     edx, esi
    call    draw_tile
    inc     esi
    jmp     .tx
.dn:
    inc     r12d
    cmp     r12d, r13d
    jle     .d
.fr:
    cmp     r14d, r15d
    jae     .done
    lea     rax, [dlist]
    mov     ecx, [rax + r14*8 + 4]
    call    draw_entity
    inc     r14d
    jmp     .fr
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

; ecx = tx, edx = ty
draw_tile:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32
    mov     eax, edx
    shl     eax, 7
    add     eax, ecx
    lea     r8, [tmap]
    movzx   eax, byte [r8 + rax]
    lea     r8, [tdraw]
    movzx   eax, byte [r8 + rax]
    test    eax, eax
    jz      .out
    mov     [bk_kind], eax
    mov     [bk_tx], ecx
    mov     [bk_ty], edx
    mov     r8d, ecx
    shl     r8d, 8
    sub     r8d, [camx]
    mov     r9d, edx
    shl     r9d, 8
    sub     r9d, [camy]
    mov     eax, r8d
    sub     eax, r9d
    sar     eax, 4
    add     eax, CX0
    mov     [bk_sx], eax
    add     r8d, r9d
    sar     r8d, 5
    add     r8d, CY0
    mov     [bk_sy], r8d
    cmp     eax, -40
    jl      .out
    cmp     eax, SCR_W + 40
    jg      .out
    cmp     r8d, -20
    jl      .out
    cmp     r8d, SCR_H + 72
    jg      .out
    mov     eax, ecx
    mov     r9d, edx
    HASH    eax, r9d
    mov     [bk_seed], eax
    ; перекрывает ли игрока (для срезки стен / прозрачности деревьев)
    lea     eax, [ecx + edx]
    sub     eax, [pdiag]
    mov     r12d, eax                   ; od
    mov     eax, [bk_sx]
    sub     eax, [psx]
    mov     r13d, eax
    sar     r13d, 31
    xor     eax, r13d
    sub     eax, r13d
    mov     r13d, eax                   ; |dx| на экране
    mov     dword [bk_cut], 0
    mov     dword [bk_faces], 3
    mov     dword [bk_notop], 0
    mov     eax, [bk_kind]
    cmp     eax, D_WINL
    jbe     .wall
    cmp     eax, D_FENCE
    je      .fence
    cmp     eax, D_STOVE
    je      .stove
    cmp     eax, D_STOVEL
    je      .stove
    cmp     eax, D_TABLE
    je      .table
    cmp     eax, D_PILE
    je      .pile
    jmp     .sprite
.wall:
    mov     dword [bk_H], 36
    call    .occl
    test    eax, eax
    jz      .blk
    mov     dword [bk_H], 5
    mov     dword [bk_cut], 1
    jmp     .blk
.stove:
    mov     dword [bk_H], 22
    call    .occl
    test    eax, eax
    jz      .blk
    mov     dword [bk_H], 5
    mov     dword [bk_cut], 1
    jmp     .blk
.table:
    mov     dword [bk_H], 8
    jmp     .blk
.pile:
    mov     dword [bk_H], 12
    jmp     .blk
.fence:
    mov     dword [bk_H], 11
    mov     dword [bk_notop], 1
    mov     ebx, [bk_tx]
    mov     esi, [bk_ty]
    xor     edi, edi
    lea     ecx, [ebx-1]
    mov     edx, esi
    call    get_tile
    cmp     eax, T_FENCE
    je      .fx
    lea     ecx, [ebx+1]
    mov     edx, esi
    call    get_tile
    cmp     eax, T_FENCE
    jne     .fy
.fx:
    or      edi, 1
.fy:
    mov     ecx, ebx
    lea     edx, [esi-1]
    call    get_tile
    cmp     eax, T_FENCE
    je      .fyy
    mov     ecx, ebx
    lea     edx, [esi+1]
    call    get_tile
    cmp     eax, T_FENCE
    jne     .fz
.fyy:
    or      edi, 2
.fz:
    test    edi, edi
    jnz     .fset
    mov     edi, 3
.fset:
    mov     [bk_faces], edi
.blk:
    call    draw_block
    ; печная труба: у топящейся — открытое жерло, у холодной — снежная шапка
    mov     eax, [bk_kind]
    cmp     eax, D_STOVE
    je      .chim
    cmp     eax, D_STOVEL
    jne     .out
.chim:
    cmp     dword [bk_cut], 0
    jne     .out
    mov     dword [img_sway], 0
    lea     rcx, [spr_chims]
    cmp     dword [bk_kind], D_STOVEL
    jne     .ch1
    lea     rcx, [spr_chim]
.ch1:
    mov     eax, [bk_tx]
    shl     eax, 8
    add     eax, 128
    mov     edx, [bk_ty]
    shl     edx, 8
    add     edx, 128
    shl     edx, 16
    or      eax, edx
    mov     [cm_gpos], eax
    mov     edx, [bk_sx]
    sub     edx, 4
    mov     r8d, [bk_sy]
    sub     r8d, 41
    mov     r9d, 8 | (24 << 8)
    call    draw_img
    jmp     .out
.sprite:
    ; картинка из таблицы
    sub     eax, D_SPRUCE
    shl     eax, 5
    lea     rbx, [sprtab]
    add     rbx, rax
    ; качание на ветру: наклон по местному ветру (сила², вбок по экрану) + своё колебание
    mov     dword [img_sway], 0
    mov     eax, [rbx + SPT_AMP]
    test    eax, eax
    jz      .nosw
    call    .treewind
    mov     eax, [bk_seed]
    and     eax, 1023
    cvtsi2ss xmm0, eax
    mulss   xmm0, [f_inv163]            ; фаза
    mov     eax, [bk_seed]
    shr     eax, 12
    and     eax, 7
    cvtsi2ss xmm1, eax
    mulss   xmm1, [f_0_08]
    addss   xmm1, [f_1_9]               ; своя частота
    mulss   xmm1, [wind_t]
    addss   xmm0, xmm1
    call    fsin
    ; колебание: 0.12·сила + 0.4·|вбок|; наклон: вбок·(0.9 + 0.8·сила) — в бурю гнёт
    movss   xmm1, [sw_vs]
    andps   xmm1, [f_absmask]
    mulss   xmm1, [f_0_4]
    movss   xmm2, [sw_g]
    mulss   xmm2, [f_0_12]
    addss   xmm1, xmm2
    mulss   xmm0, xmm1
    movss   xmm1, [sw_g]
    mulss   xmm1, [f_0_8]
    addss   xmm1, [f_0_9]
    mulss   xmm1, [sw_vs]
    addss   xmm0, xmm1
    cvtsi2ss xmm1, dword [rbx + SPT_AMP]
    mulss   xmm0, xmm1
    cvtss2si eax, xmm0
    mov     [img_sway], eax
    mov     eax, [rbx + SPT_MODE]
    mov     [img_mode], eax
.nosw:
    mov     rcx, [rbx + SPT_PTR]
    mov     r9d, [rbx + SPT_WH]
    mov     r10d, [rbx + SPT_AX]
    ; полупрозрачность, если заслоняет игрока
    mov     dword [bk_dith], 0
    cmp     dword [rbx + SPT_DITH], 0
    je      .nod
    cmp     r12d, 1
    jl      .nod
    cmp     r12d, 8
    jg      .nod
    cmp     r13d, 36
    jg      .nod
    or      r9d, 1 << 16
    mov     dword [bk_dith], 1 << 16
.nod:
    mov     eax, [bk_tx]
    shl     eax, 8
    add     eax, 168
    mov     edx, [bk_ty]
    shl     edx, 8
    add     edx, 168
    shl     edx, 16
    or      eax, edx
    mov     [cm_gpos], eax
    mov     edx, [bk_sx]
    sub     edx, r10d
    mov     r8d, r9d
    shr     r8d, 8
    and     r8d, 255
    neg     r8d
    add     r8d, [bk_sy]
    add     r8d, 10
    mov     eax, [rbx + SPT_NB]
    mov     [img_nb], eax
    call    draw_img
    mov     dword [img_nb], N_BL
    mov     dword [img_sway], 0
    mov     eax, [bk_kind]
    cmp     eax, D_LINE
    je      .laundry
    cmp     eax, D_LAMP
    je      .lamphead
    jmp     .out
.laundry:
    ; бельё треплет сильнее, чем деревья; висит с прищепок, в порыв хлопает чаще
    call    .treewind
    mov     eax, [bk_seed]
    and     eax, 1023
    cvtsi2ss xmm0, eax
    mulss   xmm0, [f_inv163]
    movss   xmm1, [wind_t]
    mulss   xmm1, [f_3_1]
    addss   xmm0, xmm1
    call    fsin
    movss   xmm1, [sw_vs]
    andps   xmm1, [f_absmask]
    mulss   xmm1, [f_0_5]
    movss   xmm2, [sw_g]
    mulss   xmm2, [f_0_2]
    addss   xmm1, xmm2
    mulss   xmm0, xmm1
    movss   xmm1, [sw_g]
    mulss   xmm1, [f_0_4]
    addss   xmm1, [f_0_6]
    mulss   xmm1, [sw_vs]
    addss   xmm0, xmm1
    mulss   xmm0, [f_40]
    cvtss2si eax, xmm0
    mov     [img_sway], eax
    mov     dword [img_mode], 2
    mov     dword [img_anchor], 5
    lea     rcx, [spr_linei]
    mov     r9d, 32 | (26 << 8)
    or      r9d, [bk_dith]
    mov     edx, [bk_sx]
    sub     edx, 16
    mov     r8d, [bk_sy]
    sub     r8d, 16
    call    draw_img
    mov     dword [img_sway], 0
    mov     dword [img_anchor], 0
    jmp     .out
.lamphead:
    ; лампа не горит днём и у сломанного фонаря в тёмные мгновения
    movss   xmm0, [lamp_t]
    mov     eax, [bk_tx]
    cmp     eax, [brk_x]
    jne     .lh1
    mov     eax, [bk_ty]
    cmp     eax, [brk_y]
    jne     .lh1
    mulss   xmm0, [brk_lvl]
.lh1:
    comiss  xmm0, [f_0_3]
    jae     .out
    mov     eax, [bk_sy]
    sub     eax, 31
    cmp     eax, SCR_H
    jae     .out
    imul    eax, eax, SCR_W
    mov     ecx, [bk_sx]
    add     ecx, 4
    cmp     ecx, SCR_W - 1
    jae     .out
    add     eax, ecx
    lea     rdx, [albedo]
    cmp     dword [rdx + rax*4], 0xC1FFE0A0
    jne     .lh2
    mov     dword [rdx + rax*4], 0x01383C44
.lh2:
    cmp     dword [rdx + rax*4 + 4], 0xC1FFE0A0
    jne     .out
    mov     dword [rdx + rax*4 + 4], 0x01383C44
.out:
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
; eax = 1, если блок перед игроком и рядом по экрану
.occl:
    xor     eax, eax
    cmp     r12d, 1
    jl      .oc0
    cmp     r12d, 6
    jg      .oc0
    cmp     r13d, 56
    jg      .oc0
    mov     eax, 1
.oc0:
    ret
; ветер у дерева (белья): точка на 0.8 тайла выше по ветру — своя крона себя не заслоняет,
; а чужие заслоны (лес, изба) — да. -> sw_g — сила, sw_vs — вбок по экрану (со знаком, с силой)
.treewind:
    sub     rsp, 40
    cvtsi2ss xmm0, dword [bk_tx]
    addss   xmm0, [f_0_5]
    movss   xmm2, [wind_dx]
    mulss   xmm2, [f_0_8]
    subss   xmm0, xmm2
    cvtsi2ss xmm1, dword [bk_ty]
    addss   xmm1, [f_0_5]
    movss   xmm2, [wind_dy]
    mulss   xmm2, [f_0_8]
    subss   xmm1, xmm2
    movss   xmm2, [f_20]
    call    wind_vec
    movss   [sw_g], xmm2
    subss   xmm0, xmm1
    mulss   xmm0, [f_0_707]
    movss   [sw_vs], xmm0
    add     rsp, 40
    ret

; изометрический блок: верхняя грань + левая (+y) и правая (+x) боковые
draw_block:
    push    rbx
    push    rsi
    push    rdi
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    mov     r12d, -15                   ; p — колонка относительно вершины
.col:
    mov     ebx, [bk_sx]
    add     ebx, r12d
    cmp     ebx, SCR_W
    jae     .nextcol
    mov     eax, r12d
    cdq
    xor     eax, edx
    sub     eax, edx
    inc     eax
    shr     eax, 1
    mov     ebp, eax                    ; q0
    mov     r14d, [bk_sy]
    add     r14d, 16
    sub     r14d, ebp                   ; низ колонки
    mov     r13d, r14d
    sub     r13d, [bk_H]                ; начало боковой грани
    cmp     dword [bk_notop], 0
    jne     .side
    mov     esi, [bk_sy]
    add     esi, ebp
    sub     esi, [bk_H]
    mov     edi, r13d
    test    esi, esi
    jns     .t0
    xor     esi, esi
.t0:
    cmp     edi, SCR_H
    jle     .tl
    mov     edi, SCR_H
.tl:
    cmp     esi, edi
    jge     .side
    mov     eax, esi
    sub     eax, [bk_sy]
    add     eax, [bk_H]
    shl     eax, 4
    mov     r15d, r12d
    shl     r15d, 3
    lea     ecx, [eax + r15d]
    mov     edx, eax
    sub     edx, r15d
    mov     [pix_lx], ecx
    mov     [pix_ly], edx
    call    shade_top
    test    eax, eax
    jz      .tn
    imul    r8d, esi, SCR_W
    add     r8d, ebx
    lea     r9, [albedo]
    mov     [r9 + r8*4], eax
    mov     eax, [bk_tx]
    shl     eax, 8
    add     eax, [pix_lx]
    mov     ecx, [bk_ty]
    shl     ecx, 8
    add     ecx, [pix_ly]
    shl     ecx, 16
    or      eax, ecx
    lea     r9, [gposb]
    mov     [r9 + r8*4], eax
    lea     r9, [nbuf]
    mov     byte [r9 + r8], N_UP
.tn:
    inc     esi
    jmp     .tl
.side:
    xor     r15d, r15d
    lea     ebp, [r12d + 15]            ; u на левой грани
    test    r12d, r12d
    js      .fl
    mov     r15d, 1
    mov     ebp, r12d                   ; u на правой грани
.fl:
    mov     eax, 1
    mov     ecx, r15d
    shl     eax, cl
    test    eax, [bk_faces]
    jz      .nextcol
    test    r15d, r15d
    jnz     .gr
    lea     eax, [r12d + 16]
    shl     eax, 4
    mov     ecx, [bk_tx]
    shl     ecx, 8
    add     eax, ecx
    mov     ecx, [bk_ty]
    shl     ecx, 8
    add     ecx, 288
    jmp     .gset
.gr:
    mov     eax, [bk_tx]
    shl     eax, 8
    add     eax, 288
    mov     ecx, r12d
    shl     ecx, 4
    neg     ecx
    add     ecx, 248
    mov     edx, [bk_ty]
    shl     edx, 8
    add     ecx, edx
.gset:
    shl     ecx, 16
    or      eax, ecx
    mov     [col_gpos], eax
    ; сугроб, привалившийся к грани (высота рельефа прямо у неё)
    mov     ecx, eax
    and     ecx, 0xFFFF
    shr     ecx, 5
    mov     edx, eax
    shr     edx, 21
    shl     edx, 10
    add     edx, ecx
    lea     r9, [drift]
    movzx   ecx, byte [r9 + rdx*4]
    shr     ecx, 3
    mov     [col_dh], ecx
    mov     esi, r13d
    test    esi, esi
    jns     .s0
    xor     esi, esi
.s0:
    mov     edi, r14d
    cmp     edi, SCR_H
    jle     .sl
    mov     edi, SCR_H
.sl:
    cmp     esi, edi
    jge     .nextcol
    mov     ecx, ebp
    mov     edx, r14d
    dec     edx
    sub     edx, esi
    lea     eax, [r15d + N_FY]
    mov     [sh_n], eax
    mov     r8d, r15d
    call    shade_side
    test    eax, eax
    jz      .sn
    imul    r8d, esi, SCR_W
    add     r8d, ebx
    lea     r9, [albedo]
    mov     [r9 + r8*4], eax
    mov     eax, [col_gpos]
    lea     r9, [gposb]
    mov     [r9 + r8*4], eax
    mov     eax, [sh_n]
    lea     r9, [nbuf]
    mov     [r9 + r8], al
.sn:
    inc     esi
    jmp     .sl
.nextcol:
    inc     r12d
    cmp     r12d, 15
    jle     .col
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
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

; ecx = lx, edx = ly (0..255 внутри тайла) -> eax
shade_top:
    mov     eax, [bk_kind]
    cmp     eax, D_WINL
    jbe     .wall
    cmp     eax, D_STOVE
    je      .stove
    cmp     eax, D_STOVEL
    je      .stove
    cmp     eax, D_TABLE
    je      .table
    jmp     .snow
.wall:
    ; верх сруба — под крышей, виден только когда её сняли или стену срезали
    mov     eax, ecx
    shr     eax, 5
    shr     edx, 5
    add     eax, edx
    test    eax, 1
    mov     eax, 0x0170482A
    jz      .r
    mov     eax, 0x01603C22
.r:
    ret
.snow:
    shr     ecx, 4
    shr     edx, 4
    add     ecx, [bk_seed]
    HASH    ecx, edx
    and     ecx, 7
    imul    ecx, ecx, 0x010101
    mov     eax, 0x01ECF0F6
    sub     eax, ecx
    ret
.stove:
    mov     eax, 0x01DDE0E6
    ret
.table:
    and     edx, 63
    mov     eax, 0x018C5C36
    cmp     edx, 6
    jae     .r
    mov     eax, 0x015A3C24
    ret

; ecx = u (вдоль грани), edx = h (высота), r8d = грань (0 лев, 1 прав) -> eax (0 = прозрачно)
shade_side:
    push    rbx
    push    rsi
    push    rdi
    mov     ebx, ecx
    mov     esi, edx
    mov     edi, r8d
    ; низ грани занесло: сугроб смотрит в небо и от стены
    cmp     esi, [col_dh]
    jae     .nodr
    lea     eax, [edi + N_DY]
    mov     [sh_n], eax
    lea     ecx, [ebx + ebx*2]
    add     ecx, [bk_seed]
    mov     edx, esi
    HASH    ecx, edx
    and     ecx, 3
    imul    ecx, ecx, 0x010101
    mov     eax, 0x01E4EAF0
    sub     eax, ecx
    lea     ecx, [esi + 1]
    cmp     ecx, [col_dh]
    jne     .ret
    mov     eax, 0x01F0F4F8             ; гребень сугроба
    jmp     .ret
.nodr:
    mov     eax, [bk_kind]
    cmp     eax, D_FENCE
    je      .fence
    cmp     eax, D_STOVE
    je      .stove
    cmp     eax, D_STOVEL
    je      .stove
    cmp     eax, D_TABLE
    je      .table
    cmp     eax, D_PILE
    je      .pile
    cmp     eax, D_WALL
    je      .logs
    cmp     dword [bk_cut], 0
    jne     .logs
    ; ----- окно с наличником
    mov     eax, ebx
    sub     eax, 3
    sub     eax, edi                    ; wu
    lea     ecx, [esi - 7]              ; wh
    cmp     ecx, -1
    jne     .w1
    lea     edx, [eax + 1]
    cmp     edx, 10
    ja      .logs
    mov     eax, 0x01E0E4E8
    jmp     .retface
.w1:
    cmp     eax, 8
    ja      .logs
    cmp     ecx, 21
    ja      .logs
    cmp     ecx, 18
    jbe     .wframe
    ; кокошник над окном
    mov     edx, eax
    sub     edx, 4
    mov     r8d, edx
    sar     r8d, 31
    xor     edx, r8d
    sub     edx, r8d
    mov     r8d, 21
    sub     r8d, ecx
    shl     r8d, 1
    cmp     edx, r8d
    jg      .logs
    lea     edx, [eax + ecx]
    test    edx, 1
    jz      .white
    jmp     .blue
.wframe:
    test    eax, eax
    jz      .white
    cmp     eax, 8
    je      .white
    test    ecx, ecx
    jz      .white
    cmp     ecx, 18
    je      .white
    cmp     eax, 1
    je      .blue
    cmp     eax, 7
    je      .blue
    cmp     ecx, 1
    je      .blue
    cmp     ecx, 17
    je      .blue
    ; силуэты на подоконнике освещённого окна: кот или герань
    cmp     dword [bk_kind], D_WINL
    jne     .nosil
    lea     edx, [ecx - 2]
    cmp     edx, 4
    ja      .nosil
    lea     r8d, [eax - 2]
    cmp     r8d, 4
    ja      .nosil
    mov     r9d, [bk_seed]
    add     r9d, edi
    imul    r9d, r9d, 0x2C1B3C6D
    shr     r9d, 28
    cmp     r9d, 1
    jbe     .cat
    cmp     r9d, 5
    ja      .nosil
    lea     r9, [ger_pot]
    movzx   r9d, byte [r9 + rdx]
    bt      r9d, r8d
    jnc     .g2
    mov     eax, 0x81302018
    jmp     .ret
.g2:
    lea     r9, [ger_leaf]
    movzx   r9d, byte [r9 + rdx]
    bt      r9d, r8d
    jnc     .g3
    mov     eax, 0x81183010
    jmp     .ret
.g3:
    lea     r9, [ger_flow]
    movzx   r9d, byte [r9 + rdx]
    bt      r9d, r8d
    jnc     .nosil
    mov     eax, 0x81501018
    jmp     .ret
.cat:
    lea     r9, [cat_rows]
    movzx   r9d, byte [r9 + rdx]
    bt      r9d, r8d
    jnc     .nosil
    mov     eax, 0x81100C0C
    jmp     .ret
.nosil:
    cmp     eax, 4
    je      .white
    cmp     ecx, 9
    je      .white
    cmp     dword [bk_kind], D_WINL
    je      .lit
    mov     edx, 18
    sub     edx, ecx
    add     edx, eax
    cmp     edx, 6
    je      .refl
    cmp     edx, 7
    je      .refl
    mov     eax, 0x0122303E
    jmp     .retface
.refl:
    mov     eax, 0x01465666
    jmp     .retface
.lit:
    cmp     eax, 2
    je      .curt
    cmp     eax, 6
    je      .curt
    ; в доме смотрят телевизор — стекло мерцает голубым
    mov     edx, [bk_ty]
    shl     edx, 7
    add     edx, [bk_tx]
    lea     r9, [hflags]
    test    byte [r9 + rdx], 2
    jz      .notv
    mov     eax, [tv_pane]
    jmp     .ret
.notv:
    cmp     ecx, 9
    mov     eax, 0x81FFC878
    ja      .ret
    mov     eax, 0x81FFDC96
    jmp     .ret
.curt:
    mov     eax, 0x81D89050
    jmp     .ret
.white:
    mov     eax, 0x01E6E8EC
    jmp     .retface
.blue:
    mov     eax, 0x013C6AC0
    jmp     .retface
    ; ----- брёвна (снежная кромка и сосульки теперь у свеса крыши)
.logs:
.l1:
    lea     rdx, [logtab]
    movzx   eax, byte [rdx + rsi]
    mov     r10d, eax
    and     r10d, 15                    ; ряд внутри бревна
    shr     eax, 4                      ; номер бревна
    test    r10d, r10d
    jnz     .l2
    imul    ecx, ebx, 3
    add     ecx, [bk_seed]
    mov     edx, eax
    HASH    ecx, edx
    test    ecx, 3
    jnz     .l2
    mov     dword [sh_n], N_UP          ; снег лежит в пазу — смотрит в небо
    mov     eax, 0x01D8DEE6
    jmp     .retface
.l2:
    ; бревно — цилиндр: верхняя половина смотрит в небо, нижняя — в снег
    lea     eax, [edi + edi + N_LYU]
    cmp     r10d, 4
    jae     .lt
    inc     eax
    cmp     r10d, 2
    jb      .lt
    lea     eax, [edi + N_FY]
.lt:
    mov     [sh_n], eax
    lea     rdx, [logshade]
    movzx   ecx, byte [rdx + r10]
    mov     r11d, ebx
    add     r11d, [bk_seed]
    mov     edx, esi
    HASH    r11d, edx
    and     r11d, 15
    sub     ecx, r11d
    lea     rdx, [c_log]
    test    edi, edi
    jnz     .l3
    test    ebx, ebx
    jnz     .l4
    lea     rdx, [c_logend]
    jmp     .l4
.l3:
    cmp     ebx, 15
    jne     .l4
    lea     rdx, [c_logend]
.l4:
    ; у самого снега брёвна темнее — контактная тень
    mov     eax, esi
    sub     eax, [col_dh]
    cmp     eax, 8
    jae     .l5
    imul    eax, eax, 10
    add     eax, 176
    imul    ecx, eax
    shr     ecx, 8
.l5:
    ; верхние венцы — в тени свеса крыши (срезанная стена открыта небу)
    cmp     dword [bk_cut], 0
    jne     .l6
    mov     eax, [bk_H]
    sub     eax, esi                    ; 1 — верхний ряд
    cmp     eax, 5
    ja      .l6
    imul    eax, eax, 24
    add     eax, 136
    imul    ecx, eax
    shr     ecx, 8
.l6:
    call    shade_col
    jmp     .retface
    ; ----- штакетник
.fence:
    mov     eax, ebx
    and     eax, 3
    mov     r10d, [bk_H]
    dec     r10d
    cmp     esi, r10d
    jne     .f1
    cmp     eax, 1
    jne     .transp
    mov     eax, 0x01E8ECF0
    jmp     .retface
.f1:
    cmp     eax, 3
    jne     .fp
    cmp     esi, 3
    je      .fp
    cmp     esi, 7
    je      .fp
    jmp     .transp
.fp:
    mov     ecx, ebx
    add     ecx, [bk_seed]
    mov     edx, esi
    HASH    ecx, edx
    and     ecx, 31
    neg     ecx
    add     ecx, 236
    lea     rdx, [c_fence]
    call    shade_col
    jmp     .retface
.transp:
    xor     eax, eax
    jmp     .ret
    ; ----- русская печь
.stove:
    test    edi, edi
    jnz     .s1
    cmp     ebx, 5
    jb      .s1
    cmp     ebx, 10
    ja      .s1
    cmp     esi, 3
    jb      .s1
    cmp     esi, 9
    ja      .s1
    cmp     ebx, 5
    je      .sfr
    cmp     ebx, 10
    je      .sfr
    cmp     esi, 3
    je      .sfr
    cmp     esi, 9
    je      .sfr
    cmp     dword [bk_kind], D_STOVEL
    jne     .sdark
    mov     eax, ebx
    add     eax, [frame_no]
    mov     edx, esi
    HASH    eax, edx
    test    eax, 0x300
    mov     eax, 0x81FF7A28
    jnz     .ret
    mov     eax, 0x81FFB448
    jmp     .ret
.sdark:
    mov     eax, 0x01181818
    jmp     .ret
.sfr:
    mov     eax, 0x01404044
    jmp     .retface
.s1:
    mov     ecx, 248
    test    esi, 3
    jnz     .s2
    mov     ecx, 212
.s2:
    mov     eax, esi
    shr     eax, 2
    and     eax, 1
    imul    eax, eax, 3
    add     eax, ebx
    imul    edx, eax, 43
    shr     edx, 8
    imul    edx, edx, 6
    cmp     eax, edx
    jne     .s3
    mov     ecx, 212
.s3:
    lea     rdx, [c_stove]
    call    shade_col
    jmp     .retface
    ; ----- стол
.table:
    cmp     esi, 6
    jae     .tapron
    cmp     ebx, 1
    jb      .transp
    cmp     ebx, 2
    jbe     .tleg
    cmp     ebx, 12
    jb      .transp
    cmp     ebx, 13
    ja      .transp
.tleg:
    mov     eax, 0x01352518
    jmp     .retface
.tapron:
    mov     eax, 0x01583A22
    jmp     .retface
    ; ----- поленница: торцы брёвен
.pile:
    mov     eax, [bk_H]
    dec     eax
    cmp     esi, eax
    jne     .p1
    mov     eax, 0x01E4E8EE
    jmp     .retface
.p1:
    mov     eax, esi
    shr     eax, 2
    and     eax, 1
    add     eax, eax
    add     eax, ebx
    and     eax, 3
    lea     eax, [eax*2 - 3]
    imul    eax, eax
    mov     ecx, esi
    and     ecx, 3
    lea     ecx, [ecx*2 - 3]
    imul    ecx, ecx
    add     eax, ecx
    cmp     eax, 2
    je      .ph
    cmp     eax, 10
    je      .pr
    mov     eax, 0x013A2C1E
    jmp     .retface
.ph:
    mov     eax, 0x01C4A070
    jmp     .retface
.pr:
    mov     eax, 0x01A07850
.retface:
    ; грани различает свет по нормали (sh_n), а не запечённое затемнение
.ret:
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; rcx = картинка BGRA, edx = x0, r8d = y0, r9d = w | h<<8 | dither<<16; gpos в [cm_gpos]
; качание: [img_sway] (px*16 на краю), [img_mode] 1 — низ неподвижен (сдвиг ~ высота²),
; 2 — висит со строки [img_anchor] (сдвиг ~ расстояние вниз)
draw_img:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    mov     rsi, rcx
    mov     r12d, edx
    mov     r13d, r8d
    movzx   r14d, r9b
    mov     r15d, r9d
    shr     r15d, 8
    and     r15d, 255
    mov     ebp, r9d
    shr     ebp, 16
    lea     rbx, [albedo]
    lea     rdi, [gposb]
    ; нормали по третям ширины: объём ствола, кроны, фигуры
    imul    eax, r14d, 85
    shr     eax, 8
    mov     [di_t1], eax
    imul    eax, r14d, 171
    shr     eax, 8
    mov     [di_t2], eax
    xor     r10d, r10d
.r:
    cmp     r10d, r15d
    jge     .done
    ; сдвиг строки
    xor     r9d, r9d
    mov     eax, [img_sway]
    test    eax, eax
    jz      .noff
    lea     r8d, [r15d - 1]
    cmp     dword [img_mode], 2
    je      .hang
    mov     ecx, r8d
    sub     ecx, r10d                   ; высота над основанием
    imul    ecx, ecx
    imul    eax, ecx
    imul    r8d, r8d
    jmp     .div
.hang:
    mov     ecx, r10d
    sub     ecx, [img_anchor]
    jle     .noff
    imul    eax, ecx
    sub     r8d, [img_anchor]
.div:
    cdq
    idiv    r8d
    add     eax, 8
    sar     eax, 4
    mov     r9d, eax
.noff:
    mov     eax, r13d
    add     eax, r10d
    cmp     eax, SCR_H
    jae     .nr
    imul    r11d, eax, SCR_W
    xor     ecx, ecx
.c:
    cmp     ecx, r14d
    jge     .nr
    mov     eax, r10d
    imul    eax, r14d
    add     eax, ecx
    mov     eax, [rsi + rax*4]
    test    eax, 0xFF000000
    jz      .nc
    mov     edx, r12d
    add     edx, ecx
    add     edx, r9d
    cmp     edx, SCR_W
    jae     .nc
    test    ebp, ebp
    jz      .nd
    lea     r8d, [edx + r10d]
    add     r8d, r13d
    test    r8d, 1
    jnz     .nc
.nd:
    add     edx, r11d
    mov     [rbx + rdx*4], eax
    mov     eax, [cm_gpos]
    mov     [rdi + rdx*4], eax
    mov     eax, [img_nb]
    cmp     ecx, [di_t1]
    jb      .nw
    inc     eax
    cmp     ecx, [di_t2]
    jb      .nw
    inc     eax
.nw:
    lea     r8, [nbuf]
    mov     [r8 + rdx], al
.nc:
    inc     ecx
    jmp     .c
.nr:
    inc     r10d
    jmp     .r
.done:
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; rcx = карта символов, edx = x0, r8d = y0, r9d = w | h<<8 | flip<<16; цвета из pal
draw_cmap:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    mov     rsi, rcx
    mov     r12d, edx
    mov     r13d, r8d
    movzx   r14d, r9b
    mov     r15d, r9d
    shr     r15d, 8
    and     r15d, 255
    mov     ebp, r9d
    shr     ebp, 16
    lea     rbx, [pal]
    lea     rdi, [albedo]
    ; стоящий — объёмный по третям, лежащий (24 px) — смотрит в небо
    mov     dword [cm_n0], N_BL
    imul    eax, r14d, 85
    shr     eax, 8
    mov     [cm_t1], eax
    imul    eax, r14d, 171
    shr     eax, 8
    mov     [cm_t2], eax
    cmp     r14d, 24
    jne     .nf0
    mov     dword [cm_n0], N_UP
    mov     dword [cm_t1], 255
    mov     dword [cm_t2], 255
.nf0:
    xor     r10d, r10d
.r:
    cmp     r10d, r15d
    jge     .done
    mov     eax, r13d
    add     eax, r10d
    cmp     eax, SCR_H
    jae     .nr
    imul    r11d, eax, SCR_W
    xor     ecx, ecx
.c:
    cmp     ecx, r14d
    jge     .nr
    mov     edx, ecx
    test    ebp, ebp
    jz      .nf
    mov     edx, r14d
    dec     edx
    sub     edx, ecx
.nf:
    mov     eax, r10d
    imul    eax, r14d
    add     eax, edx
    movzx   eax, byte [rsi + rax]
    cmp     eax, '.'
    je      .nc
    and     eax, 127
    mov     eax, [rbx + rax*4]
    ; снег: на открытых сверху пикселях чаще, внутри — изредка
    cmp     dword [cm_snow], 0
    je      .put
    imul    r8d, r10d, 31
    add     r8d, edx
    add     r8d, [cm_seed]
    imul    r8d, r8d, 0x9E3779B1
    shr     r8d, 24
    test    r10d, r10d
    jz      .top
    lea     r9d, [r10d - 1]
    imul    r9d, r14d
    add     r9d, edx
    cmp     byte [rsi + r9], '.'
    jne     .inner
.top:
    cmp     r8d, [cm_snow]
    jae     .put
    mov     eax, 0x41E4EAF2
    jmp     .put
.inner:
    shl     r8d, 2
    cmp     r8d, [cm_snow]
    jae     .put
    mov     eax, 0x01D0D8E2
.put:
    mov     edx, r12d
    add     edx, ecx
    cmp     edx, SCR_W
    jae     .nc
    add     edx, r11d
    mov     r9d, [rdi + rdx*4 + (gposb - albedo)]
    movzx   r8d, r9w
    shr     r9d, 16
    add     r9d, r8d
    cmp     r9d, [cm_dlim]
    jg      .nc
    mov     [rdi + rdx*4], eax
    mov     eax, [cm_gpos]
    lea     r8, [gposb]
    mov     [r8 + rdx*4], eax
    mov     eax, [cm_n0]
    cmp     ecx, [cm_t1]
    jb      .nw
    inc     eax
    cmp     ecx, [cm_t2]
    jb      .nw
    inc     eax
.nw:
    lea     r8, [nbuf]
    mov     [r8 + rdx], al
.nc:
    inc     ecx
    jmp     .c
.nr:
    inc     r10d
    jmp     .r
.done:
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ecx = sx, edx = sy — тень-клякса под ногами
blob:
    push    rbx
    push    rsi
    push    rdi
    lea     r10, [albedo]
    lea     r11, [blob_w]
    mov     r8, -2
.y:
    cmp     r8, 2
    jg      .done
    lea     eax, [rdx + r8]
    cmp     eax, SCR_H
    jae     .ny
    imul    ebx, eax, SCR_W
    movzx   r9d, byte [r11 + r8 + 2]
    mov     esi, ecx
    sub     esi, r9d
    lea     r9d, [ecx + r9d]            ; x1
.x:
    cmp     esi, r9d
    jg      .ny
    cmp     esi, SCR_W
    jae     .xn
    lea     eax, [ebx + esi]
    mov     edi, [r10 + rax*4 + (gposb - albedo)]
    push    rdx
    movzx   edx, di
    shr     edi, 16
    add     edi, edx
    pop     rdx
    cmp     edi, [cm_dlim]              ; не на то, что стоит перед ногами
    jg      .xn
    mov     edi, [r10 + rax*4]
    test    edi, 0xFF000000
    jz      .xn
    shr     edi, 1
    and     edi, 0x007F7F7F
    or      edi, 0x01000000
    mov     [r10 + rax*4], edi
.xn:
    inc     esi
    jmp     .x
.ny:
    inc     r8
    jmp     .y
.done:
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ecx = индекс сущности или -1 (игрок)
draw_entity:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    mov     r14d, ecx
    cmp     ecx, -1
    jne     .z
    lea     rbx, [pl]
    mov     r15d, 1
    jmp     .have
.z:
    imul    eax, ecx, ESZ
    lea     rbx, [ents]
    add     rbx, rax
    xor     r15d, r15d
.have:
    movss   xmm0, [rbx + E_X]
    mulss   xmm0, [f_256]
    cvtss2si r12d, xmm0
    movss   xmm0, [rbx + E_Y]
    mulss   xmm0, [f_256]
    cvtss2si r13d, xmm0
    lea     eax, [r12d + 24]
    and     eax, 0xFFFF
    lea     ecx, [r13d + 24]
    shl     ecx, 16
    or      eax, ecx
    mov     [cm_gpos], eax
    ; тест глубины: пиксель, чей x+y дальше ног на 0.25 тайла к глазу, — перед
    ; фигурой (забор по кромке тайла, у которого она стоит сзади)
    lea     eax, [r12d + r13d + 112]
    mov     [cm_dlim], eax
    mov     eax, r12d
    sub     eax, [camx]
    mov     ecx, r13d
    sub     ecx, [camy]
    mov     esi, eax
    sub     esi, ecx
    sar     esi, 4
    add     esi, CX0                    ; sx
    add     eax, ecx
    sar     eax, 5
    add     eax, CY0
    mov     edi, eax                    ; sy
    cmp     esi, -20
    jl      .out
    cmp     esi, SCR_W + 20
    jg      .out
    cmp     edi, -4
    jl      .out
    cmp     edi, SCR_H + 30
    jg      .out
    ; разворот по экрану
    xor     ebp, ebp
    movss   xmm0, [rbx + E_FX]
    subss   xmm0, [rbx + E_FY]
    comiss  xmm0, [f_0]
    jae     .nflip
    mov     ebp, 1
.nflip:
    shl     ebp, 16
    lea     r8, [pal]
    test    r15d, r15d
    jz      .zpal
    mov     dword [r8 + 'c'*4], 0x013E4A34
    mov     dword [r8 + 'C'*4], 0x012A3424
    mov     dword [r8 + 'p'*4], 0x012A2A30
    mov     dword [r8 + 'b'*4], 0x015C5C62
    mov     dword [r8 + 'B'*4], 0x01404046
    ; фонарик в руке: включён — светится стекло
    mov     dword [r8 + 'L'*4], 0x01303036
    cmp     dword [pl_flash], 0
    je      .nl
    mov     dword [r8 + 'L'*4], 0xC1FFF2DC
.nl:
    mov     dword [cm_snow], 0          ; живой — снег на нём тает
    cmp     dword [gstate], 1
    je      .corpse
    jmp     .frames
.zpal:
    mov     eax, [rbx + E_COAT]
    or      eax, 0x01000000
    mov     [r8 + 'c'*4], eax
    mov     ecx, eax
    shr     ecx, 1
    and     ecx, 0x7F7F7F
    mov     edx, eax
    shr     edx, 3
    and     edx, 0x1F1F1F
    add     ecx, edx
    or      ecx, 0x01000000
    mov     [r8 + 'C'*4], ecx
    mov     eax, [rbx + E_PANTS]
    or      eax, 0x01000000
    mov     [r8 + 'p'*4], eax
    mov     dword [r8 + 'b'*4], 0x013A2E26
    mov     dword [r8 + 'B'*4], 0x01241C16
    ; мёртвые не греют — снег лежит на плечах
    mov     dword [cm_snow], 64
    imul    eax, r14d, 37
    mov     [cm_seed], eax
    ; глаза отсвечивают в луче фонарика, если зомби смотрит на игрока
    mov     dword [r8 + 'x'*4], 0x01601010
    cmp     dword [pl_flash], 0
    je      .noeye
    movss   xmm0, [flash_i]
    comiss  xmm0, [f_40]
    jb      .noeye
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
    jae     .noeye
    comiss  xmm3, [f_0_3]
    jb      .noeye
    sqrtss  xmm3, xmm3
    divss   xmm1, xmm3
    divss   xmm2, xmm3
    movaps  xmm4, xmm1
    mulss   xmm4, [pl + E_FX]
    movaps  xmm5, xmm2
    mulss   xmm5, [pl + E_FY]
    addss   xmm4, xmm5
    comiss  xmm4, [f_0_88]
    jb      .noeye
    mulss   xmm1, [rbx + E_FX]
    mulss   xmm2, [rbx + E_FY]
    addss   xmm1, xmm2
    comiss  xmm1, [f_m0_3]
    ja      .noeye
    mov     dword [r8 + 'x'*4], 0xC1B4F0C8
.noeye:
    cmp     dword [rbx + E_ST], ST_CORPSE
    jne     .frames
.corpse:
    ; тело понемногу заносит снегом
    movss   xmm0, [rbx + E_TM]
    mulss   xmm0, [f_1_7]
    addss   xmm0, [f_20]
    minss   xmm0, [f_210]
    cvttss2si eax, xmm0
    mov     [cm_snow], eax
    imul    eax, r14d, 37
    mov     [cm_seed], eax
    lea     rcx, [cm_corpse]
    lea     edx, [esi - 12]
    lea     r8d, [edi - 6]
    mov     r9d, 24 | (8 << 8)
    or      r9d, ebp
    call    draw_cmap
    jmp     .out
.frames:
    mov     ecx, esi
    mov     edx, edi
    call    blob
    ; зомби шатает (на ходу и стоя), игрок чуть подпрыгивает на шаге
    xor     r12d, r12d
    xor     r13d, r13d
    test    r15d, r15d
    jnz     .pbob
    movss   xmm0, [rbx + E_AN]
    mulss   xmm0, [f_7]
    cvtsi2ss xmm1, r14d
    addss   xmm0, xmm1
    call    fsin
    movss   [tmp_d], xmm0
    movss   xmm0, [wind_t]
    mulss   xmm0, [f_1_1]
    cvtsi2ss xmm1, r14d
    mulss   xmm1, [f_0_7]
    addss   xmm0, xmm1
    call    fsin
    mulss   xmm0, [f_0_6]
    addss   xmm0, [tmp_d]
    maxss   xmm0, [f_m1]
    minss   xmm0, [f_1]
    cvtss2si r12d, xmm0
    jmp     .up0
.pbob:
    cmp     dword [pl_moving], 0
    je      .up0
    movss   xmm0, [rbx + E_AN]
    mulss   xmm0, [f_2_6]
    cvttss2si eax, xmm0
    and     eax, 1
    mov     r13d, eax
.up0:
    ; верх
    lea     rcx, [cm_zup]
    test    r15d, r15d
    jz      .up
    lea     rcx, [cm_pup]
    movss   xmm0, [pl_swing]
    comiss  xmm0, [f_0]
    jbe     .up
    lea     rcx, [cm_patk]
.up:
    lea     edx, [esi - 8]
    add     edx, r12d
    lea     r8d, [edi - 27]
    add     r8d, r13d
    mov     r9d, 16 | (19 << 8)
    or      r9d, ebp
    call    draw_cmap
    ; ноги (у зомби — снег на ботинках)
    test    r15d, r15d
    jnz     .lg0
    mov     dword [cm_snow], 24
.lg0:
    lea     rcx, [cm_lidle]
    test    r15d, r15d
    jz      .walk
    cmp     dword [pl_moving], 0
    je      .legs
.walk:
    movss   xmm0, [rbx + E_AN]
    mulss   xmm0, [f_2_6]
    cvttss2si eax, xmm0
    lea     rcx, [cm_la]
    test    eax, 1
    jz      .legs
    lea     rcx, [cm_lb]
.legs:
    lea     edx, [esi - 8]
    lea     r8d, [edi - 8]
    mov     r9d, 16 | (9 << 8)
    or      r9d, ebp
    call    draw_cmap
.out:
    mov     dword [cm_snow], 0
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
    sub     rsp, 8
    imul    eax, edx, SCR_W
    mov     [rsp], eax                  ; конец диапазона, пиксель
    mov     ebx, CY0                    ; CY0 − строка
    sub     ebx, ecx
    imul    ecx, ecx, SCR_W
    lea     rsi, [albedo]
    mov     r8, [fbbits]
    mov     r9d, [camx]
    add     r9d, [camy]
    mov     r10d, [gox]
    shl     r10d, 6
    mov     r11d, [goy]
    shl     r11d, 6
    mov     ebp, [grain_off]
    pxor    xmm15, xmm15
    movdqa  xmm14, [tint_vec]
    movdqa  xmm13, [memtint_v]          ; в мгле «память» светлее
    movdqa  xmm12, [w255]
    movdqa  xmm11, [gray_w]
.row:
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
    and     eax, 0x3F
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
.nomoon:
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
    movq    xmm0, [rsi + r13*4 + (lgrid - albedo)]
    movq    xmm1, [rsi + r13*4 + (lgrid - albedo) + GW*4]
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
    jmp     .moon
.own:
    and     r12d, 0x3F
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
    pminsw  xmm0, xmm12
.lit:
    punpcklbw xmm4, xmm15
    pmullw  xmm4, xmm0
    psrlw   xmm4, 7
    test    dword [rsi + rcx*4], 0x40000000
    jz      .nospark
    paddw   xmm4, xmm4                  ; искра на снегу
.nospark:
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

; ---------------------------------------------------------------- HUD
; ecx = x, edx = y, r8d = w, r9d = h, eax = цвет
fill_rect:
    push    rbx
    mov     r10, [fbbits]
    mov     r11d, edx
    add     r9d, edx
.y:
    cmp     r11d, r9d
    jge     .done
    imul    ebx, r11d, SCR_W
    add     ebx, ecx
    mov     edx, r8d
.x:
    test    edx, edx
    jle     .ny
    mov     [r10 + rbx*4], eax
    inc     ebx
    dec     edx
    jmp     .x
.ny:
    inc     r11d
    jmp     .y
.done:
    pop     rbx
    ret

; xmm0 = значение 0..100 -> eax = 0..100
bar_len:
    maxss   xmm0, [f_0]
    minss   xmm0, [f_100]
    cvttss2si eax, xmm0
    ret

draw_hud:
    push    rbx
    sub     rsp, 32
    call    draw_windhud
    mov     ecx, 10
    mov     edx, 304
    mov     r8d, 102
    mov     r9d, 7
    mov     eax, 0x00101418
    call    fill_rect
    movss   xmm0, [pl_hp]
    call    bar_len
    mov     r8d, eax
    mov     ecx, 11
    mov     edx, 305
    mov     r9d, 5
    mov     eax, 0x00C03030
    call    fill_rect
    ; тепло: рыжее, остывая — голубеет (ниже 60)
    mov     ecx, 10
    mov     edx, 316
    mov     r8d, 102
    mov     r9d, 7
    mov     eax, 0x00101418
    call    fill_rect
    movss   xmm0, [pl_warm]
    mulss   xmm0, [f_inv60]
    CLAMP01 xmm0
    mulss   xmm0, [f_256]
    cvttss2si ecx, xmm0                 ; t·256
    mov     eax, 0xE0 - 0x90
    imul    eax, ecx
    sar     eax, 8
    add     eax, 0x90
    shl     eax, 16                     ; R
    mov     edx, 0x90 - 0xC0
    imul    edx, ecx
    sar     edx, 8
    add     edx, 0xC0
    shl     edx, 8                      ; G
    or      eax, edx
    mov     edx, 0x40 - 0xF0
    imul    edx, ecx
    sar     edx, 8
    add     edx, 0xF0                   ; B
    or      eax, edx
    mov     ebx, eax
    movss   xmm0, [pl_warm]
    call    bar_len
    mov     r8d, eax
    mov     ecx, 11
    mov     edx, 317
    mov     r9d, 5
    mov     eax, ebx
    call    fill_rect
    mov     ecx, 10
    mov     edx, 328
    mov     r8d, 102
    mov     r9d, 7
    mov     eax, 0x00101418
    call    fill_rect
    movss   xmm0, [pl_sta]
    call    bar_len
    mov     r8d, eax
    mov     ecx, 11
    mov     edx, 329
    mov     r9d, 5
    mov     eax, 0x00D8B040
    call    fill_rect
    mov     ecx, 10
    mov     edx, 340
    mov     r8d, 102
    mov     r9d, 7
    mov     eax, 0x00101418
    call    fill_rect
    movss   xmm0, [pl_bat]
    call    bar_len
    mov     r8d, eax
    mov     ecx, 11
    mov     edx, 341
    mov     r9d, 5
    mov     eax, 0x00406080
    cmp     dword [pl_flash], 0
    je      .b
    mov     eax, 0x0070B8F0
.b:
    call    fill_rect
    add     rsp, 32
    pop     rbx
    ret
