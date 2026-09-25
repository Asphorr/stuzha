; ----------------------------------------------------------------------------
;  sky.inc — синус, ветер, луна и облака, огни (прогрев, сломанный фонарь, ТВ)
; ----------------------------------------------------------------------------
section .text

; xmm0 = x (рад) -> xmm0 ≈ sin x (ошибка < 0.001); портит eax, xmm1
fsin:
    mulss   xmm0, [f_inv2pi]
    cvtss2si eax, xmm0
    cvtsi2ss xmm1, eax
    subss   xmm0, xmm1                  ; t в [-0.5, 0.5]
    movaps  xmm1, xmm0
    andps   xmm1, [f_absmask]
    mulss   xmm1, xmm0                  ; t|t|
    mulss   xmm0, [f_8]
    mulss   xmm1, [f_16]
    subss   xmm0, xmm1                  ; y = 8t - 16t|t|
    movaps  xmm1, xmm0
    andps   xmm1, [f_absmask]
    mulss   xmm1, xmm0
    subss   xmm1, xmm0                  ; y|y| - y
    mulss   xmm1, [f_0_225]
    addss   xmm0, xmm1
    ret

; xmm0 = t -> xmm0 = smoothstep(clamp01 t); портит xmm1
sstep:
    CLAMP01 xmm0
    movss   xmm1, [f_3]
    subss   xmm1, xmm0
    subss   xmm1, xmm0
    mulss   xmm1, xmm0
    mulss   xmm0, xmm1
    ret

; ---------------------------------------------------------------- небо
; (ветер — в wind.inc)
; xmm0 = азимут (рад) -> sky_dx, sky_dy (направление НА источник)
sky_dir:
    movss   [tmp_a], xmm0
    call    fsin
    movss   [sky_dy], xmm0
    movss   xmm0, [tmp_a]
    addss   xmm0, [f_pi_2]
    call    fsin
    movss   [sky_dx], xmm0
    ret

; направленный свет (закатное зарево / луна / рассвет) и облачность
update_sky:
    sub     rsp, 40
    movss   xmm0, [gtime]
    mulss   xmm0, [f_inv60]
    movss   [sk_h], xmm0
    comiss  xmm0, [fs_mrise]
    jb      .dusk
    comiss  xmm0, [fs_mset]
    jae     .dawn
    ; ----- луна: φ от восхода до заката
    subss   xmm0, [fs_mrise]
    mulss   xmm0, [fs_mspan]
    movss   [sk_phi], xmm0
    call    fsin
    ; высота e = 0.62·sin φ, tg ≈ e + e³/3
    mulss   xmm0, [fs_emax]
    movaps  xmm1, xmm0
    mulss   xmm1, xmm0
    mulss   xmm1, xmm0
    mulss   xmm1, [f_0_33]
    addss   xmm0, xmm1
    maxss   xmm0, [f_0_12]
    movss   [sky_tan], xmm0
    movss   xmm0, [sk_phi]
    mulss   xmm0, [fs_bk]
    addss   xmm0, [fs_b0]
    call    sky_dir
    ; сила: восход и закат плавно
    movss   xmm0, [sk_h]
    subss   xmm0, [fs_mrise]
    mulss   xmm0, [f_1_25]
    call    sstep
    movss   [sk_i], xmm0
    movss   xmm0, [sk_h]
    subss   xmm0, [fs_mfade]
    mulss   xmm0, [f_1_25]
    call    sstep
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    mulss   xmm1, [sk_i]
    shufps  xmm1, xmm1, 0
    mulps   xmm1, [col_moon]
    movaps  [moon_vec], xmm1
    mov     dword [sky_wrap], __float32__(0.1)
    jmp     .cloud
.dusk:
    ; зарево с запада, у самого горизонта
    movss   xmm0, [fs_bset]
    call    sky_dir
    mov     dword [sky_tan], __float32__(0.15)
    movss   xmm0, [sk_h]
    subss   xmm0, [fs_d0]
    mulss   xmm0, [fs_d1]
    call    sstep
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    mulss   xmm1, [f_0_45]
    shufps  xmm1, xmm1, 0
    mulps   xmm1, [amb_raw]
    movaps  [moon_vec], xmm1
    mov     dword [sky_wrap], __float32__(0.6)
    jmp     .cloud
.dawn:
    ; рассвет с востока
    movss   xmm0, [fs_b0]
    call    sky_dir
    mov     dword [sky_tan], __float32__(0.12)
    movss   xmm0, [sk_h]
    subss   xmm0, [fs_mset]
    mulss   xmm0, [f_1_1]
    call    sstep
    shufps  xmm0, xmm0, 0
    mulps   xmm0, [col_dawn]
    movaps  [moon_vec], xmm0
    mov     dword [sky_wrap], __float32__(0.6)
.cloud:
    ; покрытие облаками 0.15..0.7 — медленно; к метели (снег гуще 0.2) затягивает до 0.9
    movss   xmm0, [wind_t]
    mulss   xmm0, [fs_cw]
    addss   xmm0, [f_2]
    call    fsin
    mulss   xmm0, [f_0_5]
    addss   xmm0, [f_0_5]
    mulss   xmm0, [fs_ca]
    addss   xmm0, [fs_c0]
    movss   xmm1, [wnd_snow]
    subss   xmm1, [f_0_2]
    mulss   xmm1, [f_1_6]
    CLAMP01 xmm1
    movss   xmm2, [f_0_9]
    subss   xmm2, xmm0
    mulss   xmm2, xmm1
    addss   xmm0, xmm2
    movss   xmm1, [frost_k]             ; в мороз небо чистое
    mulss   xmm1, [f_0_85]
    movss   xmm2, [f_1]
    subss   xmm2, xmm1
    mulss   xmm0, xmm2
    movss   [sk_cover], xmm0
    ; таблица ослабления: шум n -> 255·(1 − cover·sstep((n/255 − 0.35)·2.5))
    lea     rdx, [cl_tab]
    xor     ecx, ecx
.ct:
    cvtsi2ss xmm0, ecx
    mulss   xmm0, [f_inv255]
    subss   xmm0, [f_0_35]
    mulss   xmm0, [f_2_5]
    call    sstep
    mulss   xmm0, [sk_cover]
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    mulss   xmm1, [f_255]
    cvtss2si eax, xmm1
    mov     [rdx + rcx], al
    inc     ecx
    cmp     ecx, 256
    jb      .ct
    ; сдвиг облаков: тексель шума = 6 сэмплов, 8.8
    movss   xmm0, [cloud_ox]
    mulss   xmm0, [fs_cls]
    cvtss2si eax, xmm0
    mov     [cl_ox], eax
    movss   xmm0, [cloud_oy]
    mulss   xmm0, [fs_cls]
    cvtss2si eax, xmm0
    mov     [cl_oy], eax
    ; свет неба для дыма
    movss   xmm0, [sk_cover]
    mulss   xmm0, [f_0_6]
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    shufps  xmm1, xmm1, 0
    mulps   xmm1, [moon_vec]
    addps   xmm1, [amb_vec]
    mulps   xmm1, [v_1_4]
    cvtps2dq xmm1, xmm1
    packssdw xmm1, xmm1
    packuswb xmm1, xmm1
    movd    [sky_b], xmm1
    add     rsp, 40
    ret

; ---------------------------------------------------------------- таблицы нормалей
; на кадр, для каждой нормали G-буфера:
;   moon_n = луна · f,  f = (N·L + w) / max(Lz + w, 0.25(1 + w)) — земля = 1, до 2.4
;   a = 0.8 + 0.2·Nz + 0.1·(N·азимут луны) + 0.07·(Ny − Nx) — сколько неба видит грань
;   amb_d = небо·(1 − a) (вычесть из сетки), amb_n = небо·a (свой свет крыш)
build_ntab:
    push    rbx
    push    rsi
    push    rdi
    movss   xmm0, [sky_tan]
    movaps  xmm1, xmm0
    mulss   xmm1, xmm0
    addss   xmm1, [f_1]
    sqrtss  xmm1, xmm1
    movss   xmm2, [f_1]
    divss   xmm2, xmm1                  ; cos высоты
    movss   xmm8, [sky_dx]
    mulss   xmm8, xmm2                  ; Lx
    movss   xmm9, [sky_dy]
    mulss   xmm9, xmm2                  ; Ly
    movss   xmm10, xmm0
    mulss   xmm10, xmm2                 ; Lz
    movss   xmm11, [sky_wrap]
    movss   xmm0, xmm10
    addss   xmm0, xmm11
    movss   xmm1, xmm11
    addss   xmm1, [f_1]
    mulss   xmm1, [f_0_25]
    maxss   xmm0, xmm1
    movss   xmm12, [f_1]
    divss   xmm12, xmm0
    lea     rsi, [nvec]
    lea     rdi, [moon_n]
    xor     ebx, ebx
.n:
    movss   xmm0, [rsi]
    mulss   xmm0, xmm8
    movss   xmm1, [rsi + 4]
    mulss   xmm1, xmm9
    addss   xmm0, xmm1
    movss   xmm1, [rsi + 8]
    mulss   xmm1, xmm10
    addss   xmm0, xmm1                  ; N·L
    addss   xmm0, xmm11
    mulss   xmm0, xmm12
    maxss   xmm0, [f_0]
    minss   xmm0, [f_2_4]
    shufps  xmm0, xmm0, 0
    mulps   xmm0, [moon_vec]
    cvtps2dq xmm0, xmm0
    packssdw xmm0, xmm0
    pminsw  xmm0, [w255]
    movq    [rdi + rbx*8], xmm0
    ; доля неба
    movss   xmm0, [rsi]
    mulss   xmm0, [sky_dx]
    movss   xmm1, [rsi + 4]
    mulss   xmm1, [sky_dy]
    addss   xmm0, xmm1
    mulss   xmm0, [f_0_1]
    movss   xmm1, [rsi + 8]
    mulss   xmm1, [f_0_2]
    addss   xmm0, xmm1
    movss   xmm1, [rsi + 4]
    subss   xmm1, [rsi]
    mulss   xmm1, [f_0_07]
    addss   xmm0, xmm1
    addss   xmm0, [f_0_8]
    maxss   xmm0, [f_0_3]
    minss   xmm0, [f_1]
    shufps  xmm0, xmm0, 0
    movaps  xmm1, [amb_vec]
    mulps   xmm1, xmm0
    movaps  xmm2, [amb_vec]
    subps   xmm2, xmm1
    cvtps2dq xmm2, xmm2
    packssdw xmm2, xmm2
    movq    [rdi + rbx*8 + (amb_d - moon_n)], xmm2
    cvtps2dq xmm1, xmm1
    packssdw xmm1, xmm1
    packuswb xmm1, xmm1
    movd    [rdi + rbx*4 + (amb_n - moon_n)], xmm1
    add     rsi, 16
    inc     ebx
    cmp     ebx, NN
    jb      .n
    ; снежинкам — средняя луна сквозь облака
    movss   xmm0, [sk_cover]
    mulss   xmm0, [f_0_6]
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    movss   xmm0, [moon_vec]
    addss   xmm0, [moon_vec + 4]
    addss   xmm0, [moon_vec + 4]
    addss   xmm0, [moon_vec + 8]
    mulss   xmm0, xmm1
    mulss   xmm0, [f_0_25]
    mulss   xmm0, [f_1_5]
    cvttss2si eax, xmm0
    mov     [flk_moon], eax
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- огни
; вход: [lamp_t] — доля включения; прогрев натрия, сломанный фонарь, телевизоры
update_lights:
    sub     rsp, 40
    movss   xmm0, [lamp_t]
    movaps  xmm1, xmm0
    shufps  xmm1, xmm1, 0
    movaps  [winf_vec], xmm1            ; окна и комнаты — просто вкл/выкл
    ; натрий: красное раньше, синее позже (B = t³, G = t², R = t(2−t))
    movss   xmm1, xmm0
    mulss   xmm1, xmm0
    movss   xmm2, xmm1
    mulss   xmm2, xmm0
    movss   xmm3, [f_2]
    subss   xmm3, xmm0
    mulss   xmm3, xmm0
    movss   [lampf_vec], xmm2
    movss   [lampf_vec + 4], xmm1
    movss   [lampf_vec + 8], xmm3
    movss   [lampf_vec + 12], xmm0
    ; сломанный фонарь: 0 горит, 1 дребезжит, 2 погас
    movss   xmm0, [brk_t]
    subss   xmm0, [dt]
    movss   [brk_t], xmm0
    comiss  xmm0, [f_0]
    ja      .bstate
    call    randf
    mov     eax, [brk_st]
    cmp     eax, 1
    je      .from1
    ja      .from2
    mov     ecx, 1                      ; из «горит»: 70% дребезг, 30% гаснет
    comiss  xmm0, [f_0_7]
    jb      .nst
    mov     ecx, 2
    jmp     .nst
.from1:
    xor     ecx, ecx
    comiss  xmm0, [f_0_6]
    jb      .nst
    mov     ecx, 2
    jmp     .nst
.from2:
    mov     ecx, 1
.nst:
    mov     [brk_st], ecx
    call    randf
    mov     eax, [brk_st]
    test    eax, eax
    jnz     .d1
    mulss   xmm0, [f_4]
    addss   xmm0, [f_2]                 ; горит 2..6 с
    jmp     .dset
.d1:
    cmp     eax, 1
    jne     .d2
    mulss   xmm0, [f_1_1]
    addss   xmm0, [f_0_4]               ; дребезг 0.4..1.5 с
    jmp     .dset
.d2:
    mulss   xmm0, [f_2]
    addss   xmm0, [f_0_8]               ; темно 0.8..2.8 с
.dset:
    movss   [brk_t], xmm0
.bstate:
    movss   xmm0, [f_1]
    mov     eax, [brk_st]
    test    eax, eax
    jz      .bl
    xorps   xmm0, xmm0
    cmp     eax, 2
    je      .bl
    call    rand
    test    eax, 0x100
    jz      .bl                         ; половина кадров — тьма
    call    randf
    mulss   xmm0, [f_0_3]
    addss   xmm0, [f_0_7]
.bl:
    movss   [brk_lvl], xmm0
    shufps  xmm0, xmm0, 0
    mulps   xmm0, [lampf_vec]
    movaps  [lampb_vec], xmm0
    ; телевизоры: смена сцены — все смотрят один канал
    movss   xmm0, [tv_t]
    subss   xmm0, [dt]
    movss   [tv_t], xmm0
    comiss  xmm0, [f_0]
    ja      .tvj
    call    randf
    mulss   xmm0, [f_0_6]
    addss   xmm0, [f_0_12]
    movss   [tv_t], xmm0
    call    randf
    mulss   xmm0, [f_0_65]
    addss   xmm0, [f_0_35]
    movss   [tv_lvl], xmm0
    mov     ecx, 3
    call    randn
    mov     [tv_hue], eax
.tvj:
    call    randf
    mulss   xmm0, [f_0_1]
    addss   xmm0, [f_0_9]
    mulss   xmm0, [tv_lvl]
    mulss   xmm0, [lamp_t]
    shufps  xmm0, xmm0, 0
    mov     eax, [tv_hue]
    shl     eax, 4
    lea     rdx, [tv_hues]
    mulps   xmm0, [rdx + rax]
    movaps  [tv_vec], xmm0
    mulps   xmm0, [tv_pane_col]
    cvtps2dq xmm0, xmm0
    packssdw xmm0, xmm0
    packuswb xmm0, xmm0
    movd    eax, xmm0
    and     eax, 0x00FFFFFF
    or      eax, 0x81000000
    mov     [tv_pane], eax
    add     rsp, 40
    ret

; ---------------------------------------------------------------- облака
gen_clouds:
    push    r12
    push    r13
    sub     rsp, 40
    lea     r12, [clouds]
    lea     r13, [randf]
    call    gen_noise64
    add     rsp, 40
    pop     r13
    pop     r12
    ret

; тайловый шум 64x64: белый шум, трижды размытый ящиком (с заворотом), растянутый в 0..255
; r12 = куда, r13 = ГСЧ (-> xmm0 в [0,1))
gen_noise64:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32
    lea     rdi, [cl_f]
    mov     ebx, 4096
.r:
    call    r13
    movss   [rdi], xmm0
    add     rdi, 4
    dec     ebx
    jnz     .r
    mov     ebx, 3
.it:
    lea     rsi, [cl_f]
    lea     rdi, [cl_g]
    mov     r8d, 1
    mov     r9d, 64
    call    blur64
    lea     rsi, [cl_g]
    lea     rdi, [cl_f]
    mov     r8d, 64
    mov     r9d, 1
    call    blur64
    dec     ebx
    jnz     .it
    lea     rsi, [cl_f]
    movss   xmm2, [rsi]
    movss   xmm3, xmm2
    mov     ecx, 4096
.mm:
    minss   xmm2, [rsi]
    maxss   xmm3, [rsi]
    add     rsi, 4
    dec     ecx
    jnz     .mm
    subss   xmm3, xmm2
    movss   xmm4, [f_255]
    divss   xmm4, xmm3
    lea     rsi, [cl_f]
    mov     rdi, r12
    mov     ecx, 4096
.nm:
    movss   xmm0, [rsi]
    subss   xmm0, xmm2
    mulss   xmm0, xmm4
    cvttss2si eax, xmm0
    mov     [rdi], al
    add     rsi, 4
    inc     rdi
    dec     ecx
    jnz     .nm
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; rsi -> rdi (64x64 f32): ящик радиуса 4 с заворотом; r8d = шаг вдоль, r9d = поперёк
blur64:
    xor     r10d, r10d
.ln:
    xor     r11d, r11d
.ps:
    xorps   xmm0, xmm0
    mov     ecx, -4
.k:
    lea     eax, [r11d + ecx]
    and     eax, 63
    imul    eax, r8d
    mov     edx, r10d
    imul    edx, r9d
    add     eax, edx
    addss   xmm0, [rsi + rax*4]
    inc     ecx
    cmp     ecx, 4
    jle     .k
    mulss   xmm0, [f_inv9]
    mov     eax, r11d
    imul    eax, r8d
    mov     edx, r10d
    imul    edx, r9d
    add     eax, edx
    movss   [rdi + rax*4], xmm0
    inc     r11d
    cmp     r11d, 64
    jb      .ps
    inc     r10d
    cmp     r10d, 64
    jb      .ln
    ret
