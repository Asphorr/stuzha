; ----------------------------------------------------------------------------
;  game.inc — новая игра, игрок, зомби, следы, кровь, снег, время суток
; ----------------------------------------------------------------------------
section .text

new_game:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32
    xor     eax, eax
    lea     rdi, [ents]
    mov     ecx, MAXE*ESZ
    rep     stosb
    lea     rdi, [tracks]
    mov     ecx, DECW*DECW
    rep     stosb
    lea     rdi, [blood]
    mov     ecx, DECW*DECW
    rep     stosb
    lea     rdi, [pl]
    mov     ecx, ESZ
    rep     stosb
    call    gen_map
    call    build_opq
    call    build_opq_df                ; для los_ray: перешагивать открытое
    call    build_cmap
    call    calc_hmap                   ; до света: фонари бросают тени по высотам
    call    wind_init                   ; погода на ночь, заслоны ветру — по карте высот
    call    calc_static_light
    call    dl_bake                     ; огни попиксельно: точки, окна, карты горизонта
    call    calc_ao
    call    gen_drifts
    xor     eax, eax
    lea     rdi, [parts]
    mov     ecx, MAXP*P_SZ
    rep     stosb
    mov     dword [br_t], __float32__(1.0)
    mov     dword [pl + E_X], __float32__(64.5)
    mov     dword [pl + E_Y], __float32__(61.5)
    mov     dword [pl + E_FX], __float32__(0.7071)
    mov     dword [pl + E_FY], __float32__(-0.7071)
    mov     dword [pl_hp], __float32__(100.0)
    mov     dword [pl_sta], __float32__(100.0)
    mov     dword [pl_bat], __float32__(100.0)
    mov     dword [pl_cd], 0
    mov     dword [pl_swing], 0
    mov     dword [pl_hurt], 0
    mov     dword [pl_flash], 0
    mov     dword [gtime], __float32__(1040.0)
    mov     dword [elapsed], 0
    mov     dword [gstate], 0
    mov     dword [kills], 0
    mov     dword [spawn_t], __float32__(4.0)
    mov     dword [cam_xf], __float32__(64.5)
    mov     dword [cam_yf], __float32__(61.5)
    mov     ebx, 44
.z:
    mov     ecx, 13
    call    spawn_zombie
    dec     ebx
    jnz     .z
    ; снежинки: u [0,1024), v [0,512), z [0,320), падение [16,56] px/с, фаза, отклик на ветер
    ; [1.5, 4.5]/с (мелкие послушнее) — своим ГСЧ
    lea     rsi, [flakes]
    mov     ebx, NFLK
.f:
    call    wrandf
    mulss   xmm0, [f_1024]
    movss   [rsi], xmm0
    call    wrandf
    mulss   xmm0, [f_512]
    movss   [rsi+4], xmm0
    call    wrandf
    mulss   xmm0, [f_320]
    movss   [rsi+8], xmm0
    call    wrandf
    mulss   xmm0, [f_40]
    addss   xmm0, [f_16]
    movss   [rsi+12], xmm0
    mov     dword [rsi+16], 0
    mov     dword [rsi+20], 0
    call    wrandf
    mulss   xmm0, [f_6_28]
    movss   [rsi+24], xmm0
    movss   xmm0, [rsi+12]
    mulss   xmm0, [f_m0_075]
    addss   xmm0, [f_5_7]
    movss   [rsi+28], xmm0
    add     rsi, FL_SZ
    dec     ebx
    jnz     .f
    mov     dword [pl_warm], __float32__(100.0)
    mov     dword [pl_wsm], 0
    xor     eax, eax                    ; запас вещей и сообщение — с нуля
    mov     [inv], eax
    mov     [inv + 4], eax
    mov     [inv + 8], eax
    mov     [inv + 12], eax
    mov     [lm_t], eax
    mov     [lp_kind], eax
    call    audio_newgame               ; голоса зомби, будки, собаки — заново
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; rbx = сущность; поля зомби по умолчанию (позиция не трогается)
init_zombie_fields:
    push    rsi
    sub     rsp, 32
    mov     dword [rbx + E_HP], 3
    mov     dword [rbx + E_ST], ST_WANDER
    mov     dword [rbx + E_VX], 0
    mov     dword [rbx + E_VY], 0
    mov     dword [rbx + E_FX], __float32__(1.0)
    mov     dword [rbx + E_FY], 0
    mov     dword [rbx + E_CD], 0
    mov     dword [rbx + E_SA], 0
    mov     dword [rbx + E_STUN], 0
    mov     dword [rbx + E_MEM], 0
    mov     dword [rbx + E_FOOT], 0
    mov     eax, [rbx + E_X]
    mov     [rbx + E_TX], eax
    mov     eax, [rbx + E_Y]
    mov     [rbx + E_TY], eax
    call    randf
    mulss   xmm0, [f_3]
    movss   [rbx + E_TM], xmm0
    call    randf
    movss   [rbx + E_AN], xmm0
    call    randf
    mulss   xmm0, [f_0_45]
    addss   xmm0, [f_0_8]
    movss   [rbx + E_SPD], xmm0
    mov     ecx, 10
    call    randn
    lea     rsi, [zcoats]
    mov     eax, [rsi + rax*4]
    mov     [rbx + E_COAT], eax
    mov     ecx, 4
    call    randn
    lea     rsi, [zpants]
    mov     eax, [rsi + rax*4]
    mov     [rbx + E_PANTS], eax
    add     rsp, 32
    pop     rsi
    ret

; ecx = минимальная дистанция от игрока (тайлы)
spawn_zombie:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40
    mov     r12d, ecx
    lea     rbx, [ents]
    mov     ecx, MAXE
.fs:
    cmp     dword [rbx + E_ST], ST_FREE
    je      .got
    add     rbx, ESZ
    dec     ecx
    jnz     .fs
    lea     rbx, [ents]                 ; нет места — переиспользуем труп
    mov     ecx, MAXE
.fc:
    cmp     dword [rbx + E_ST], ST_CORPSE
    je      .got
    add     rbx, ESZ
    dec     ecx
    jnz     .fc
    jmp     .out
.got:
    mov     edi, 60
.try:
    call    rand
    cmp     eax, 0x66666666
    jae     .any
    mov     ecx, CEM_W-2
    call    randn
    lea     esi, [eax + CEM_X0 + 1]
    mov     ecx, CEM_H-2
    call    randn
    add     eax, CEM_Y0 + 1
    jmp     .chk
.any:
    mov     ecx, 120
    call    randn
    lea     esi, [eax + 4]
    mov     ecx, 120
    call    randn
    add     eax, 4
.chk:
    mov     [sp_y], eax
    mov     ecx, esi
    mov     edx, eax
    call    get_tile
    lea     rdx, [tprop]
    test    byte [rdx + rax], 1
    jnz     .retry
    cvtsi2ss xmm0, esi
    addss   xmm0, [f_0_5]
    subss   xmm0, [pl + E_X]
    mulss   xmm0, xmm0
    cvtsi2ss xmm1, dword [sp_y]
    addss   xmm1, [f_0_5]
    subss   xmm1, [pl + E_Y]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    cvtsi2ss xmm1, r12d
    mulss   xmm1, xmm1
    comiss  xmm0, xmm1
    jb      .retry
    cvtsi2ss xmm0, esi
    addss   xmm0, [f_0_5]
    movss   [rbx + E_X], xmm0
    cvtsi2ss xmm0, dword [sp_y]
    addss   xmm0, [f_0_5]
    movss   [rbx + E_Y], xmm0
    call    init_zombie_fields
    jmp     .out
.retry:
    dec     edi
    jnz     .try
.out:
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- следы и кровь
; xmm0 = x, xmm1 = y (тайлы), ecx = сила, edx = размер (1/2); только снег и дорога
stamp_track:
    movss   xmm4, xmm0
    mulss   xmm4, [f_8]
    cvttss2si r8d, xmm4
    movss   xmm4, xmm1
    mulss   xmm4, [f_8]
    cvttss2si r9d, xmm4
    cmp     r8d, DECW-2
    jae     .no
    cmp     r9d, DECW-2
    jae     .no
    mov     eax, r9d
    shr     eax, 3
    shl     eax, 7
    mov     r10d, r8d
    shr     r10d, 3
    add     eax, r10d
    lea     r10, [tmap]
    movzx   eax, byte [r10 + rax]
    lea     r10, [tprop]
    movzx   eax, byte [r10 + rax]
    shr     eax, 4
    cmp     eax, 2
    je      .no
    lea     r10, [tracks]
    jmp     stamp_max
.no:
    ret

; r8d, r9d = ячейка, ecx = значение, edx = размер, r10 = буфер: buf = max(buf, v)
stamp_max:
    mov     eax, r9d
    shl     eax, 10
    add     eax, r8d
    cmp     [r10 + rax], cl
    jae     .a
    mov     [r10 + rax], cl
.a:
    cmp     edx, 1
    je      .done
    cmp     [r10 + rax + 1], cl
    jae     .b
    mov     [r10 + rax + 1], cl
.b:
    cmp     [r10 + rax + DECW], cl
    jae     .c
    mov     [r10 + rax + DECW], cl
.c:
    cmp     [r10 + rax + DECW + 1], cl
    jae     .done
    mov     [r10 + rax + DECW + 1], cl
.done:
    ret

; xmm0 = x, xmm1 = y, ecx = капель, xmm2 = разброс
blood_splat:
    push    rbx
    sub     rsp, 32
    movss   [bs_x], xmm0
    movss   [bs_y], xmm1
    movss   [bs_s], xmm2
    mov     ebx, ecx
.d:
    call    randf
    subss   xmm0, [f_0_5]
    mulss   xmm0, [bs_s]
    addss   xmm0, [bs_x]
    mulss   xmm0, [f_8]
    cvttss2si r8d, xmm0
    call    randf
    subss   xmm0, [f_0_5]
    mulss   xmm0, [bs_s]
    addss   xmm0, [bs_y]
    mulss   xmm0, [f_8]
    cvttss2si r9d, xmm0
    cmp     r8d, DECW-2
    jae     .n
    cmp     r9d, DECW-2
    jae     .n
    push    r8
    push    r9
    call    rand
    pop     r9
    pop     r8
    mov     edx, eax
    and     eax, 127
    add     eax, 120
    mov     ecx, eax
    shr     edx, 9
    and     edx, 1
    inc     edx
    lea     r10, [blood]
    call    stamp_max
.n:
    dec     ebx
    jnz     .d
    add     rsp, 32
    pop     rbx
    ret

; xmm0 = радиус шума вокруг игрока
make_noise:
    maxss   xmm0, [noise_r]
    movss   [noise_r], xmm0
    mov     eax, [pl + E_X]
    mov     [noise_x], eax
    mov     eax, [pl + E_Y]
    mov     [noise_y], eax
    ret

; ---------------------------------------------------------------- кадр
update:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 32
    movss   xmm0, [elapsed]
    addss   xmm0, [dt]
    movss   [elapsed], xmm0
    call    update_wind
    call    player_wind
    call    update_flakes
    call    decay_tracks
    mov     dword [noise_r], 0
    cmp     dword [gstate], 0
    jne     .over
    movss   xmm0, [dt]
    mulss   xmm0, [f_timescale]
    addss   xmm0, [gtime]
    movss   [gtime], xmm0
    comiss  xmm0, [f_dawn]
    jb      .tok
    mov     dword [gstate], 2
.tok:
    call    update_player
    call    update_zombies
    call    separate
    call    spawn_logic
    movss   xmm0, [pl_hp]
    comiss  xmm0, [f_0]
    ja      .env
    mov     dword [gstate], 1
    mov     dword [pl_flash], 0
    jmp     .env
.over:
    cmp     byte [k_r_edge], 0
    je      .env
    call    new_game
.env:
    call    update_env
    call    update_fx
    ; камера: игрок + взгляд вперёд
    movss   xmm2, [dt]
    mulss   xmm2, [f_4]
    minss   xmm2, [f_1]
    movss   xmm0, [pl + E_FX]
    mulss   xmm0, [f_1_3]
    addss   xmm0, [pl + E_X]
    subss   xmm0, [cam_xf]
    mulss   xmm0, xmm2
    addss   xmm0, [cam_xf]
    movss   [cam_xf], xmm0
    movss   xmm0, [pl + E_FY]
    mulss   xmm0, [f_1_3]
    addss   xmm0, [pl + E_Y]
    subss   xmm0, [cam_yf]
    mulss   xmm0, xmm2
    addss   xmm0, [cam_yf]
    movss   [cam_yf], xmm0
    call    audio_frame                 ; звук: фоны по месту игрока, голоса по таймерам
    add     rsp, 32
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; окружение: свет неба, фонари, фонарик, дальность зрения зомби, тонировка
update_env:
    sub     rsp, 40
    movss   xmm0, [gtime]
    mulss   xmm0, [f_inv60]
    movss   xmm6, xmm0                  ; час (16..32)
    subss   xmm0, [f_16]
    maxss   xmm0, [f_0]
    minss   xmm0, [f_15_99]
    cvttss2si eax, xmm0
    cvtsi2ss xmm1, eax
    subss   xmm0, xmm1
    shl     eax, 4
    lea     rdx, [amb_tab]
    movups  xmm2, [rdx + rax]
    movups  xmm3, [rdx + rax + 16]
    subps   xmm3, xmm2
    shufps  xmm0, xmm0, 0
    mulps   xmm3, xmm0
    addps   xmm2, xmm3
    movaps  [amb_raw], xmm2
    mulps   xmm2, [v_0_7]               ; часть неба теперь — направленный свет
    movaps  [amb_vec], xmm2
    ; фонари: зажигаются в 17:24, гаснут в 7:30
    movss   xmm0, xmm6
    subss   xmm0, [f_17_4]
    mulss   xmm0, [f_2_5]
    CLAMP01 xmm0
    movss   xmm1, [f_31_5]
    subss   xmm1, xmm6
    mulss   xmm1, [f_2_5]
    CLAMP01 xmm1
    mulss   xmm0, xmm1
    movss   [lamp_t], xmm0
    call    update_lights
    ; фонарик
    xorps   xmm0, xmm0
    cmp     dword [pl_flash], 0
    je      .fl
    movss   xmm0, [f_flash]
    movss   xmm1, [pl_bat]
    comiss  xmm1, [f_15]
    jae     .fl
    call    rand
    test    eax, 3
    jnz     .fl0
    call    randf
    mulss   xmm0, [f_flash]
    jmp     .fl
.fl0:
    movss   xmm0, [f_flash]
.fl:
    movss   [flash_i], xmm0
    ; зрение зомби: с фонарём видят далеко, в темноте — близко
    movss   xmm0, [f_12_5]
    cmp     dword [pl_flash], 0
    jne     .see
    movss   xmm0, [amb_raw + 8]
    mulss   xmm0, [f_0_07]
    addss   xmm0, [f_5]
.see:
    ; в мгле метели и зомби видят ближе (до 0.55)
    movss   xmm1, [wnd_wo]
    mulss   xmm1, [f_0_45]
    movss   xmm2, [f_1]
    subss   xmm2, xmm1
    mulss   xmm0, xmm2
    mulss   xmm0, xmm0
    movss   [see2], xmm0
    ; тонировка кадра
    mov     eax, 0x01000100
    mov     edx, 0x00000100
    cmp     dword [gstate], 1
    jne     .t1
    mov     eax, 0x00780078
    mov     edx, 0x000000C0
    jmp     .t3
.t1:
    movss   xmm0, [pl_hurt]
    comiss  xmm0, [f_0]
    jbe     .t3
    mov     eax, 0x00960096
    mov     edx, 0x00000100
.t3:
    mov     [tint_vec], eax
    mov     [tint_vec+4], edx
    ; зерно и блеск снега
    call    rand
    and     eax, 0xFFFF
    mov     [grain_off], eax
    movss   xmm0, [pl + E_X]
    mulss   xmm0, [f_1_5]
    cvttss2si eax, xmm0
    imul    eax, eax, 73
    movss   xmm0, [pl + E_Y]
    mulss   xmm0, [f_1_5]
    cvttss2si ecx, xmm0
    imul    ecx, ecx, 151
    add     eax, ecx
    imul    eax, eax, 0x2545F491
    mov     [glint], eax
    call    update_sky
    call    update_roofs
    add     rsp, 40
    ret

; ---------------------------------------------------------------- игрок
update_player:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40
    lea     rbx, [pl]
    movss   xmm0, [dt]
    movss   xmm1, [pl_cd]
    subss   xmm1, xmm0
    movss   [pl_cd], xmm1
    movss   xmm1, [pl_swing]
    subss   xmm1, xmm0
    movss   [pl_swing], xmm1
    movss   xmm1, [pl_hurt]
    subss   xmm1, xmm0
    movss   [pl_hurt], xmm1
    ; вектор движения (экранные WASD -> мир)
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    movss   xmm6, [f_1]
    cmp     byte [k_w], 0
    je      .nw
    subss   xmm4, xmm6
    subss   xmm5, xmm6
.nw:
    cmp     byte [k_s], 0
    je      .ns
    addss   xmm4, xmm6
    addss   xmm5, xmm6
.ns:
    cmp     byte [k_a], 0
    je      .na
    subss   xmm4, xmm6
    addss   xmm5, xmm6
.na:
    cmp     byte [k_d], 0
    je      .nd
    addss   xmm4, xmm6
    subss   xmm5, xmm6
.nd:
    mov     dword [pl_moving], 0
    mov     dword [pl_sprint], 0
    movaps  xmm6, xmm4
    mulss   xmm6, xmm4
    movaps  xmm7, xmm5
    mulss   xmm7, xmm5
    addss   xmm6, xmm7
    comiss  xmm6, [f_0_01]
    jb      .nomove
    mov     dword [pl_moving], 1
    sqrtss  xmm6, xmm6
    divss   xmm4, xmm6
    divss   xmm5, xmm6                  ; единичное направление
    movss   [mv_x], xmm4
    movss   [mv_y], xmm5
    ; ветер вдоль хода (со знаком и силой): по ветру легче, против — тяжелее
    mulss   xmm4, [pl_wvx]
    mulss   xmm5, [pl_wvy]
    addss   xmm4, xmm5
    maxss   xmm4, [f_m1]
    minss   xmm4, [f_1]
    movss   [mv_wp], xmm4
    movss   xmm7, [f_walk]
    cmp     byte [k_shift], 0
    je      .nsprint
    movss   xmm0, [pl_sta]
    comiss  xmm0, [f_3]
    jbe     .nsprint
    movss   xmm7, [f_run]
    mov     dword [pl_sprint], 1
    ; бег против ветра выматывает до 1.6 раза быстрее
    movss   xmm1, [mv_wp]
    minss   xmm1, [f_0]
    mulss   xmm1, [f_m0_6]
    addss   xmm1, [f_1]
    movss   xmm0, [dt]
    mulss   xmm0, [f_24]
    mulss   xmm0, xmm1
    movss   xmm1, [pl_sta]
    subss   xmm1, xmm0
    movss   [pl_sta], xmm1
    movss   xmm0, [f_6_5]
    call    make_noise
    movss   xmm7, [f_run]
.nsprint:
    ; ход: ×(1 + 0.18·ветер вдоль), озябший (тепло < 20) — медленнее, до ×0.85
    movss   xmm0, [mv_wp]
    mulss   xmm0, [f_0_18]
    addss   xmm0, [f_1]
    mulss   xmm7, xmm0
    movss   xmm0, [pl_warm]
    minss   xmm0, [f_20]
    mulss   xmm0, [f_0_0075]
    addss   xmm0, [f_0_85]
    mulss   xmm7, xmm0
    movss   xmm0, [pl_hurt]
    comiss  xmm0, [f_0]
    jbe     .nhurt
    mulss   xmm7, [f_0_6]
.nhurt:
    mulss   xmm7, [dt]                  ; шаг
    movss   [mv_step], xmm7
    movss   xmm0, [mv_x]
    mulss   xmm0, xmm7
    movss   xmm1, [mv_y]
    mulss   xmm1, xmm7
    mov     rcx, rbx
    call    move_ent
    ; фаза шага и следы: на бегу шаг длиннее (0.5 тайла)
    movss   xmm0, [mv_step]
    cmp     dword [pl_sprint], 0
    je      .nst
    mulss   xmm0, [f_0_72]
.nst:
    movss   xmm1, [rbx + E_AN]
    addss   xmm1, xmm0
    movss   [rbx + E_AN], xmm1
    movss   xmm1, [rbx + E_SA]
    addss   xmm1, xmm0
    movss   [rbx + E_SA], xmm1
    comiss  xmm1, [f_0_36]
    jb      .nomove
    subss   xmm1, [f_0_36]
    movss   [rbx + E_SA], xmm1
    xor     dword [rbx + E_FOOT], 1
    ; след: смещение поперёк хода
    movss   xmm2, [f_0_1]
    cmp     dword [rbx + E_FOOT], 0
    jne     .fp
    movss   xmm2, [f_m0_1]
.fp:
    movss   xmm0, [mv_y]
    xorps   xmm0, [f_signmask]
    mulss   xmm0, xmm2
    addss   xmm0, [rbx + E_X]
    movss   xmm1, [mv_x]
    mulss   xmm1, xmm2
    addss   xmm1, [rbx + E_Y]
    call    snd_foot                    ; звук: нахожено ли тут и глубок ли снег — до оттиска
    mov     ecx, 215
    mov     edx, 2
    call    stamp_track
    call    snd_step_pl
    cmp     dword [pl_sprint], 0
    je      .nomove
    call    emit_kick                   ; снег из-под ног
.nomove:
    ; поза бега — к бегу или шагу за ~0.15 с
    cvtsi2ss xmm0, dword [pl_sprint]
    subss   xmm0, [pl_runb]
    movss   xmm1, [dt]
    mulss   xmm1, [f_7]
    minss   xmm1, [f_1]
    mulss   xmm0, xmm1
    addss   xmm0, [pl_runb]
    movss   [pl_runb], xmm0
    cmp     dword [pl_sprint], 0
    jne     .nregen
    ; силы копятся 12/с; озябшему (тепло < 40) — медленнее, до трети
    movss   xmm1, [pl_warm]
    mulss   xmm1, [f_inv60]
    addss   xmm1, [f_0_35]
    minss   xmm1, [f_1]
    movss   xmm0, [dt]
    mulss   xmm0, [f_12]
    mulss   xmm0, xmm1
    addss   xmm0, [pl_sta]
    minss   xmm0, [f_100]
    movss   [pl_sta], xmm0
.nregen:
    call    warm_update
    call    loot_update                 ; вещи рядом: E — взять, обыскать; 1..4 — в ход
    ; взгляд: курсор на фигуре зомби — прямо на его ноги, иначе — на курсор
    ; (от груди: точка на уровне груди зомби даёт направление на его ноги)
    call    aim_pick
    test    eax, eax
    jnz     .aimv
    mov     eax, [mx]
    sub     eax, [psx]
    cvtsi2ss xmm0, eax                  ; dxs
    mov     eax, [my]
    sub     eax, [psy]
    add     eax, 14
    cvtsi2ss xmm1, eax                  ; dys
    addss   xmm1, xmm1
    movaps  xmm2, xmm0
    addss   xmm2, xmm1                  ; wx = dxs + 2dys
    subss   xmm1, xmm0                  ; wy = 2dys - dxs
.aimv:
    movaps  xmm3, xmm2
    mulss   xmm3, xmm2
    movaps  xmm4, xmm1
    mulss   xmm4, xmm1
    addss   xmm3, xmm4
    comiss  xmm3, [f_1]
    jb      .noaim
    sqrtss  xmm3, xmm3
    divss   xmm2, xmm3
    divss   xmm1, xmm3
    movss   [rbx + E_FX], xmm2
    movss   [rbx + E_FY], xmm1
.noaim:
    ; удар лопатой
    mov     al, [k_lmb]
    or      al, [k_space]
    jz      .noatk
    movss   xmm0, [pl_cd]
    comiss  xmm0, [f_0]
    ja      .noatk
    movss   xmm0, [pl_sta]
    comiss  xmm0, [f_6]
    jb      .noatk
    mov     dword [pl_cd], __float32__(0.55)
    mov     dword [pl_swing], __float32__(0.22)
    subss   xmm0, [f_9]
    movss   [pl_sta], xmm0
    movss   xmm0, [f_5_5]
    call    make_noise
    call    snd_swing
    call    player_swing
.noatk:
    ; фонарик
    cmp     byte [k_f_edge], 0
    je      .nof
    xor     eax, eax
    cmp     dword [pl_flash], 0
    jne     .fset
    movss   xmm0, [pl_bat]
    comiss  xmm0, [f_0]
    jbe     .fset
    mov     eax, 1
.fset:
    mov     [pl_flash], eax
    call    snd_click
.nof:
    cmp     dword [pl_flash], 0
    je      .out
    movss   xmm0, [dt]
    mulss   xmm0, [f_0_2]
    movss   xmm1, [pl_bat]
    subss   xmm1, xmm0
    maxss   xmm1, [f_0]
    movss   [pl_bat], xmm1
    comiss  xmm1, [f_0]
    ja      .out
    mov     dword [pl_flash], 0
.out:
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

player_swing:
    push    rbx
    push    rsi
    sub     rsp, 40
    lea     rbx, [ents]
    mov     esi, MAXE
.l:
    mov     eax, [rbx + E_ST]
    cmp     eax, ST_WANDER
    jb      .n
    cmp     eax, ST_CORPSE
    jae     .n
    ; лопата метёт сектор ±55° радиусом f_sw_r от центра игрока; зомби задет,
    ; если его круг (f_rad) цепляет сектор: d − r ≤ R и угол ≤ θ + asin(r/d),
    ; т. е. f·d ≥ cos θ·√(d² − r²) − sin θ·r
    movss   xmm4, [rbx + E_X]
    subss   xmm4, [pl + E_X]
    movss   xmm5, [rbx + E_Y]
    subss   xmm5, [pl + E_Y]
    movaps  xmm6, xmm4
    mulss   xmm6, xmm4
    movaps  xmm7, xmm5
    mulss   xmm7, xmm5
    addss   xmm6, xmm7                  ; d²
    comiss  xmm6, [f_sw_far2]
    jae     .n
    comiss  xmm6, [f_body2]             ; тела сошлись — задет в любом случае
    jb      .los
    movaps  xmm1, xmm6
    subss   xmm1, [f_body2]
    sqrtss  xmm1, xmm1
    mulss   xmm1, [f_sw_cos]
    subss   xmm1, [f_sw_sinr]
    movaps  xmm0, xmm4
    mulss   xmm0, [pl + E_FX]
    movaps  xmm2, xmm5
    mulss   xmm2, [pl + E_FY]
    addss   xmm0, xmm2
    comiss  xmm0, xmm1
    jb      .n
.los:
    ; не сквозь стену
    movss   xmm0, [pl + E_X]
    movss   xmm1, [pl + E_Y]
    movss   xmm2, [rbx + E_X]
    movss   xmm3, [rbx + E_Y]
    call    melee_clear
    test    eax, eax
    jz      .n
    movss   xmm4, [rbx + E_X]
    subss   xmm4, [pl + E_X]
    movss   xmm5, [rbx + E_Y]
    subss   xmm5, [pl + E_Y]
    sqrtss  xmm6, xmm6
    maxss   xmm6, [f_0_05]
    divss   xmm4, xmm6
    divss   xmm5, xmm6                  ; направление на зомби
    ; попадание
    dec     dword [rbx + E_HP]
    mov     dword [rbx + E_STUN], __float32__(0.45)
    mulss   xmm4, [f_4_5]
    mulss   xmm5, [f_4_5]
    movss   [rbx + E_VX], xmm4
    movss   [rbx + E_VY], xmm5
    mov     dword [rbx + E_ST], ST_CHASE
    mov     dword [rbx + E_MEM], __float32__(6.0)
    mov     eax, [pl + E_X]
    mov     [rbx + E_TX], eax
    mov     eax, [pl + E_Y]
    mov     [rbx + E_TY], eax
    movss   xmm0, [rbx + E_X]
    movss   xmm1, [rbx + E_Y]
    movss   xmm2, [f_0_5]
    mov     ecx, 6
    call    blood_splat
    call    snd_hit
    cmp     dword [rbx + E_HP], 0
    jg      .n
    mov     dword [rbx + E_ST], ST_CORPSE
    mov     dword [rbx + E_TM], 0       ; с этого момента тело заносит снегом
    inc     dword [kills]
    movss   xmm0, [rbx + E_X]
    movss   xmm1, [rbx + E_Y]
    movss   xmm2, [f_1_1]
    mov     ecx, 28
    call    blood_splat
.n:
    add     rbx, ESZ
    dec     esi
    jnz     .l
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- зомби
; rbx = сущность -> eax = видит ли её игрок по прямой (из сетки прошлого кадра)
los_at_ent:
    movss   xmm0, [rbx + E_X]
    mulss   xmm0, [f_4]
    cvttss2si eax, xmm0
    sub     eax, [gox]
    cmp     eax, GW
    jae     .no
    movss   xmm0, [rbx + E_Y]
    mulss   xmm0, [f_4]
    cvttss2si edx, xmm0
    sub     edx, [goy]
    cmp     edx, GW
    jae     .no
    imul    edx, edx, GW
    add     eax, edx
    lea     rdx, [losg]
    movzx   eax, byte [rdx + rax]
    ret
.no:
    xor     eax, eax
    ret

; rbx = зомби, [z_pd2] = квадрат расстояния до игрока -> eax: 0 — не чует, 1 — слабо,
; 2 — сильно. Запах тянется от игрока по ветру у игрока шлейфом длиной L = 3 + 11·s,
; расширяется (слабо — до 1.6 L, сильно — до 0.55 L); в тишь (s < 0.15) — кругом:
; сильно до 2.2 тайла, слабо до 5. Против ветра не чуют вовсе
scent_at:
    movss   xmm0, [rbx + E_X]
    subss   xmm0, [pl + E_X]
    movss   xmm1, [rbx + E_Y]
    subss   xmm1, [pl + E_Y]
    movss   xmm2, [pl_ws]
    comiss  xmm2, [f_0_15]
    jae     .plume
    movss   xmm2, [z_pd2]
    xor     eax, eax
    comiss  xmm2, [f_25]
    jae     .out
    inc     eax
    comiss  xmm2, [f_4_84]
    jae     .out
    inc     eax
.out:
    ret
.plume:
    movss   xmm3, [f_1]
    divss   xmm3, xmm2
    movaps  xmm4, xmm0
    mulss   xmm4, [pl_wvx]
    movaps  xmm5, xmm1
    mulss   xmm5, [pl_wvy]
    addss   xmm4, xmm5
    mulss   xmm4, xmm3                  ; вдоль ветра, тайлы
    mulss   xmm0, [pl_wvy]
    mulss   xmm1, [pl_wvx]
    subss   xmm0, xmm1
    andps   xmm0, [f_absmask]
    mulss   xmm0, xmm3                  ; поперёк
    xor     eax, eax
    comiss  xmm4, [f_m0_5]
    jb      .out
    mulss   xmm2, [f_11]
    addss   xmm2, [f_3]                 ; L
    movaps  xmm5, xmm2
    mulss   xmm5, [f_1_6]
    comiss  xmm4, xmm5
    jae     .out
    movaps  xmm5, xmm4
    mulss   xmm5, [f_0_35]
    addss   xmm5, [f_1]
    comiss  xmm0, xmm5
    jae     .out
    inc     eax
    mulss   xmm2, [f_0_55]
    comiss  xmm4, xmm2
    jae     .out
    movaps  xmm5, xmm4
    mulss   xmm5, [f_0_22s]
    addss   xmm5, [f_0_5]
    comiss  xmm0, xmm5
    jae     .out
    inc     eax
    ret

update_zombies:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    lea     rbx, [ents]
    mov     r12d, MAXE
.loop:
    mov     eax, [rbx + E_ST]
    cmp     eax, ST_WANDER
    jb      .next
    cmp     eax, ST_CORPSE
    jae     .next
    movss   xmm0, [rbx + E_CD]
    subss   xmm0, [dt]
    movss   [rbx + E_CD], xmm0
    ; отброс от удара
    movss   xmm0, [rbx + E_VX]
    mulss   xmm0, [dt]
    movss   xmm1, [rbx + E_VY]
    mulss   xmm1, [dt]
    mov     rcx, rbx
    call    move_ent
    movss   xmm2, [dt]
    mulss   xmm2, [f_7]
    movss   xmm3, [f_1]
    subss   xmm3, xmm2
    maxss   xmm3, [f_0]
    movss   xmm0, [rbx + E_VX]
    mulss   xmm0, xmm3
    movss   [rbx + E_VX], xmm0
    movss   xmm0, [rbx + E_VY]
    mulss   xmm0, xmm3
    movss   [rbx + E_VY], xmm0
    ; оглушение
    movss   xmm0, [rbx + E_STUN]
    comiss  xmm0, [f_0]
    jbe     .nostun
    subss   xmm0, [dt]
    movss   [rbx + E_STUN], xmm0
    jmp     .next
.nostun:
    ; ветер у зомби: слух (вой ветра глушит), ход
    movss   xmm0, [rbx + E_X]
    movss   xmm1, [rbx + E_Y]
    movss   xmm2, [f_10]
    call    wind_vec
    movss   [z_wvx], xmm0
    movss   [z_wvy], xmm1
    movss   [z_ws], xmm2
    movss   xmm4, [pl + E_X]
    subss   xmm4, [rbx + E_X]
    movss   xmm5, [pl + E_Y]
    subss   xmm5, [rbx + E_Y]
    mulss   xmm4, xmm4
    mulss   xmm5, xmm5
    addss   xmm4, xmm5
    movss   [z_pd2], xmm4
    ; видит игрока?
    comiss  xmm4, [see2]
    jae     .nosee
    call    los_at_ent
    test    eax, eax
    jz      .nosee
    mov     dword [rbx + E_ST], ST_CHASE
    mov     eax, [pl + E_X]
    mov     [rbx + E_TX], eax
    mov     eax, [pl + E_Y]
    mov     [rbx + E_TY], eax
    mov     dword [rbx + E_MEM], __float32__(6.0)
    jmp     .move
.nosee:
    ; слышит шум? По ветру от шума слышно дальше, против — ближе, а вой ветра у самого
    ; зомби глушит: r' = r·(1 + 0.5·ветер у шума вдоль «шум -> зомби») / (1 + 0.7·сила у зомби)
    movss   xmm0, [noise_r]
    comiss  xmm0, [f_0]
    jbe     .nonoise
    movss   xmm1, [rbx + E_X]
    subss   xmm1, [noise_x]
    movss   xmm2, [rbx + E_Y]
    subss   xmm2, [noise_y]
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    movaps  xmm4, xmm2
    mulss   xmm4, xmm2
    addss   xmm3, xmm4                  ; d²
    sqrtss  xmm4, xmm3
    maxss   xmm4, [f_0_1]
    mulss   xmm1, [pl_wvx]
    mulss   xmm2, [pl_wvy]
    addss   xmm1, xmm2
    divss   xmm1, xmm4
    mulss   xmm1, [f_0_5]
    addss   xmm1, [f_1]
    maxss   xmm1, [f_0_3]
    mulss   xmm0, xmm1
    movss   xmm1, [z_ws]
    mulss   xmm1, [f_0_7]
    addss   xmm1, [f_1]
    divss   xmm0, xmm1
    mulss   xmm0, xmm0
    comiss  xmm3, xmm0
    jae     .nonoise
    mov     eax, [noise_x]
    mov     [rbx + E_TX], eax
    mov     eax, [noise_y]
    mov     [rbx + E_TY], eax
    cmp     dword [rbx + E_ST], ST_CHASE
    je      .move
    mov     dword [rbx + E_ST], ST_INVEST
    jmp     .move
.nonoise:
    mov     eax, [rbx + E_ST]
    cmp     eax, ST_CHASE
    jne     .nchase
    movss   xmm0, [rbx + E_MEM]
    subss   xmm0, [dt]
    movss   [rbx + E_MEM], xmm0
    comiss  xmm0, [f_0]
    ja      .move
    mov     dword [rbx + E_ST], ST_INVEST
    jmp     .move
.nchase:
    cmp     eax, ST_INVEST
    je      .move
    ; чует по ветру: сильный запах — замирает, поворачивается на него и идёт проверять
    ; (в среднем через полсекунды в шлейфе), слабый — бредёт в ту сторону
    call    scent_at
    mov     [z_scent], eax
    cmp     eax, 2
    jne     .nsniff
    call    wrandf
    movss   xmm1, [dt]
    mulss   xmm1, [f_2]
    comiss  xmm0, xmm1
    jae     .nsniff
    mov     dword [rbx + E_ST], ST_INVEST
    mov     eax, [pl + E_X]
    mov     [rbx + E_TX], eax
    mov     eax, [pl + E_Y]
    mov     [rbx + E_TY], eax
    mov     dword [rbx + E_STUN], __float32__(0.6)
    movss   xmm4, [pl + E_X]
    subss   xmm4, [rbx + E_X]
    movss   xmm5, [pl + E_Y]
    subss   xmm5, [rbx + E_Y]
    movss   xmm6, [z_pd2]
    sqrtss  xmm6, xmm6
    maxss   xmm6, [f_0_05]
    divss   xmm4, xmm6
    divss   xmm5, xmm6
    movss   [rbx + E_FX], xmm4
    movss   [rbx + E_FY], xmm5
    call    snd_sniff
    jmp     .next
.nsniff:
    ; брожение
    movss   xmm0, [rbx + E_TM]
    subss   xmm0, [dt]
    movss   [rbx + E_TM], xmm0
    comiss  xmm0, [f_0]
    ja      .move
    call    randf
    mulss   xmm0, [f_4]
    addss   xmm0, [f_2]
    movss   [rbx + E_TM], xmm0
    call    randf
    comiss  xmm0, [f_0_35]
    jb      .idle
    cmp     dword [z_scent], 0
    jne     .drift                      ; чуют по ветру — тянутся к игроку
    call    randf
    subss   xmm0, [f_0_5]
    mulss   xmm0, [f_10]
    addss   xmm0, [rbx + E_X]
    movss   [rbx + E_TX], xmm0
    call    randf
    subss   xmm0, [f_0_5]
    mulss   xmm0, [f_10]
    addss   xmm0, [rbx + E_Y]
    movss   [rbx + E_TY], xmm0
    jmp     .move
.drift:
    ; бредут в сторону игрока (на треть пути)
    movss   xmm0, [pl + E_X]
    subss   xmm0, [rbx + E_X]
    mulss   xmm0, [f_0_3]
    addss   xmm0, [rbx + E_X]
    movss   [rbx + E_TX], xmm0
    movss   xmm0, [pl + E_Y]
    subss   xmm0, [rbx + E_Y]
    mulss   xmm0, [f_0_3]
    addss   xmm0, [rbx + E_Y]
    movss   [rbx + E_TY], xmm0
    jmp     .move
.idle:
    mov     eax, [rbx + E_X]
    mov     [rbx + E_TX], eax
    mov     eax, [rbx + E_Y]
    mov     [rbx + E_TY], eax
.move:
    movss   xmm4, [rbx + E_TX]
    subss   xmm4, [rbx + E_X]
    movss   xmm5, [rbx + E_TY]
    subss   xmm5, [rbx + E_Y]
    movaps  xmm6, xmm4
    mulss   xmm6, xmm4
    movaps  xmm7, xmm5
    mulss   xmm7, xmm5
    addss   xmm6, xmm7
    comiss  xmm6, [f_0_04]
    jae     .go
    cmp     dword [rbx + E_ST], ST_INVEST
    jne     .attack
    mov     dword [rbx + E_ST], ST_WANDER
    mov     dword [rbx + E_TM], __float32__(1.5)
    jmp     .attack
.go:
    sqrtss  xmm6, xmm6
    divss   xmm4, xmm6
    divss   xmm5, xmm6
    movss   [rbx + E_FX], xmm4
    movss   [rbx + E_FY], xmm5
    movss   xmm7, [f_zwander]
    mov     eax, [rbx + E_ST]
    cmp     eax, ST_CHASE
    jne     .sp1
    movss   xmm7, [f_zchase]
.sp1:
    cmp     eax, ST_INVEST
    jne     .sp2
    movss   xmm7, [f_zinvest]
.sp2:
    mulss   xmm7, [rbx + E_SPD]
    ; по ветру идут легче, против — вязнут (±15%)
    movaps  xmm0, xmm4
    mulss   xmm0, [z_wvx]
    movaps  xmm1, xmm5
    mulss   xmm1, [z_wvy]
    addss   xmm0, xmm1
    maxss   xmm0, [f_m1]
    minss   xmm0, [f_1]
    mulss   xmm0, [f_0_15]
    addss   xmm0, [f_1]
    mulss   xmm7, xmm0
    mulss   xmm7, [dt]
    movss   [z_step], xmm7
    mov     eax, [rbx + E_X]
    mov     [z_ox], eax
    mov     eax, [rbx + E_Y]
    mov     [z_oy], eax
    movaps  xmm0, xmm4
    mulss   xmm0, xmm7
    movaps  xmm1, xmm5
    mulss   xmm1, xmm7
    mov     rcx, rbx
    call    move_ent
    ; застрял — пробуем вбок
    movss   xmm0, [rbx + E_X]
    subss   xmm0, [z_ox]
    mulss   xmm0, xmm0
    movss   xmm1, [rbx + E_Y]
    subss   xmm1, [z_oy]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    movss   xmm1, [z_step]
    mulss   xmm1, [f_0_3]
    mulss   xmm1, xmm1
    comiss  xmm0, xmm1
    jae     .moved
    movss   xmm0, [rbx + E_FY]
    movss   xmm1, [rbx + E_FX]
    xorps   xmm1, [f_signmask]          ; (fy, -fx)
    test    dword [rbx + E_FOOT], 2
    jz      .side
    xorps   xmm0, [f_signmask]
    xorps   xmm1, [f_signmask]
.side:
    mulss   xmm0, [z_step]
    mulss   xmm1, [z_step]
    mov     rcx, rbx
    call    move_ent
    call    rand
    test    eax, 0x3F
    jnz     .moved
    xor     dword [rbx + E_FOOT], 2
.moved:
    ; анимация и волочащиеся следы
    movss   xmm0, [z_step]
    movss   xmm1, [rbx + E_AN]
    addss   xmm1, xmm0
    movss   [rbx + E_AN], xmm1
    movss   xmm1, [rbx + E_SA]
    addss   xmm1, xmm0
    movss   [rbx + E_SA], xmm1
    comiss  xmm1, [f_0_12]
    jb      .attack
    subss   xmm1, [f_0_12]
    movss   [rbx + E_SA], xmm1
    xor     dword [rbx + E_FOOT], 1
    movss   xmm2, [f_0_08]
    test    dword [rbx + E_FOOT], 1
    jnz     .zf
    movss   xmm2, [f_m0_08]
.zf:
    movss   xmm0, [rbx + E_FY]
    xorps   xmm0, [f_signmask]
    mulss   xmm0, xmm2
    addss   xmm0, [rbx + E_X]
    movss   xmm1, [rbx + E_FX]
    mulss   xmm1, xmm2
    addss   xmm1, [rbx + E_Y]
    mov     ecx, 150
    mov     edx, 1
    call    stamp_track
    call    snd_step_z
.attack:
    ; дотягивается руками: тела сошлись (0.5) + вытянутые руки, не сквозь стену
    movss   xmm0, [z_pd2]
    comiss  xmm0, [f_zreach2]
    jae     .next
    movss   xmm0, [rbx + E_CD]
    comiss  xmm0, [f_0]
    ja      .next
    movss   xmm0, [rbx + E_X]
    movss   xmm1, [rbx + E_Y]
    movss   xmm2, [pl + E_X]
    movss   xmm3, [pl + E_Y]
    call    melee_clear
    test    eax, eax
    jz      .next
    ; разворачивается к игроку
    movss   xmm4, [pl + E_X]
    subss   xmm4, [rbx + E_X]
    movss   xmm5, [pl + E_Y]
    subss   xmm5, [rbx + E_Y]
    movss   xmm6, [z_pd2]
    sqrtss  xmm6, xmm6
    maxss   xmm6, [f_0_05]
    divss   xmm4, xmm6
    divss   xmm5, xmm6
    movss   [rbx + E_FX], xmm4
    movss   [rbx + E_FY], xmm5
    mov     dword [rbx + E_CD], __float32__(1.25)
    mov     ecx, 7
    call    randn
    add     eax, 9
    cvtsi2ss xmm1, eax
    movss   xmm0, [pl_hp]
    subss   xmm0, xmm1
    movss   [pl_hp], xmm0
    mov     dword [pl_hurt], __float32__(0.35)
    movss   xmm0, [pl + E_X]
    movss   xmm1, [pl + E_Y]
    movss   xmm2, [f_0_6]
    mov     ecx, 5
    call    blood_splat
    call    snd_zatk
.next:
    add     rbx, ESZ
    dec     r12d
    jnz     .loop
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

; расталкивание: зомби друг с другом и с игроком
separate:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32
    lea     rsi, [ents]
    mov     r12d, MAXE
.i:
    mov     eax, [rsi + E_ST]
    cmp     eax, ST_WANDER
    jb      .inext
    cmp     eax, ST_CORPSE
    jae     .inext
    ; с игроком
    movss   xmm4, [rsi + E_X]
    subss   xmm4, [pl + E_X]
    movss   xmm5, [rsi + E_Y]
    subss   xmm5, [pl + E_Y]
    movaps  xmm6, xmm4
    mulss   xmm6, xmm4
    movaps  xmm7, xmm5
    mulss   xmm7, xmm5
    addss   xmm6, xmm7
    comiss  xmm6, [f_0_25]
    jae     .others
    comiss  xmm6, [f_1em4]
    jb      .others
    sqrtss  xmm6, xmm6
    movss   xmm7, [f_0_5]
    subss   xmm7, xmm6
    divss   xmm7, xmm6
    movaps  xmm0, xmm4
    mulss   xmm0, xmm7
    movaps  xmm1, xmm5
    mulss   xmm1, xmm7
    mov     rcx, rsi
    call    move_ent
.others:
    lea     rdi, [rsi + ESZ]
    lea     r13d, [r12d - 1]
.j:
    test    r13d, r13d
    jz      .inext
    mov     eax, [rdi + E_ST]
    cmp     eax, ST_WANDER
    jb      .jnext
    cmp     eax, ST_CORPSE
    jae     .jnext
    movss   xmm4, [rdi + E_X]
    subss   xmm4, [rsi + E_X]
    movss   xmm5, [rdi + E_Y]
    subss   xmm5, [rsi + E_Y]
    movaps  xmm6, xmm4
    mulss   xmm6, xmm4
    movaps  xmm7, xmm5
    mulss   xmm7, xmm5
    addss   xmm6, xmm7
    comiss  xmm6, [f_0_36]
    jae     .jnext
    comiss  xmm6, [f_1em4]
    jb      .jnext
    sqrtss  xmm6, xmm6
    movss   xmm7, [f_0_6]
    subss   xmm7, xmm6
    mulss   xmm7, [f_0_5]
    divss   xmm7, xmm6
    mulss   xmm4, xmm7
    mulss   xmm5, xmm7
    movss   [sep_x], xmm4
    movss   [sep_y], xmm5
    movaps  xmm0, xmm4
    movaps  xmm1, xmm5
    mov     rcx, rdi
    call    move_ent
    movss   xmm0, [sep_x]
    xorps   xmm0, [f_signmask]
    movss   xmm1, [sep_y]
    xorps   xmm1, [f_signmask]
    mov     rcx, rsi
    call    move_ent
.jnext:
    add     rdi, ESZ
    dec     r13d
    jmp     .j
.inext:
    add     rsi, ESZ
    dec     r12d
    jnz     .i
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

spawn_logic:
    push    rbx
    sub     rsp, 32
    movss   xmm0, [spawn_t]
    subss   xmm0, [dt]
    movss   [spawn_t], xmm0
    comiss  xmm0, [f_0]
    ja      .out
    mov     dword [spawn_t], __float32__(4.5)
    ; живых
    lea     rdx, [ents]
    mov     ecx, MAXE
    xor     ebx, ebx
.c:
    mov     eax, [rdx + E_ST]
    dec     eax
    cmp     eax, 2
    ja      .cn
    inc     ebx
.cn:
    add     rdx, ESZ
    dec     ecx
    jnz     .c
    ; лимит растёт со временем: 38 + elapsed/12, максимум 95
    movss   xmm0, [elapsed]
    mulss   xmm0, [f_inv12]
    cvttss2si eax, xmm0
    add     eax, 38
    mov     ecx, 95
    cmp     eax, ecx
    cmova   eax, ecx
    cmp     ebx, eax
    jae     .out
    mov     ecx, 18
    call    spawn_zombie
.out:
    add     rsp, 32
    pop     rbx
    ret

; ---------------------------------------------------------------- снег
; летит часть снежинок — по снегопаду; каждую тянет к ветру там, где её земля
; (у земли за избой — затишье, наверху — нет), плюс порхание и завихрения
update_flakes:
    push    rbx
    push    rsi
    sub     rsp, 40
    movss   xmm0, [wnd_snow]
    mulss   xmm0, [f_1900]
    addss   xmm0, [f_500]
    cvttss2si ebx, xmm0
    mov     eax, NFLK
    cmp     ebx, eax
    cmova   ebx, eax
    mov     [flk_n], ebx
    lea     rsi, [flakes]
.l:
    ; земля под снежинкой -> мир (камера прошлого кадра)
    cvttss2si eax, [rsi]
    sub     eax, [cam_a]
    add     eax, CX0 + 192
    and     eax, 1023
    sub     eax, 192 + CX0              ; a'
    cvttss2si ecx, [rsi + 4]
    sub     ecx, [cam_b]
    add     ecx, CY0 + 76
    and     ecx, 511
    sub     ecx, 76 + CY0               ; b'
    shl     ecx, 4
    shl     eax, 3
    lea     edx, [ecx + eax]
    add     edx, [camx]                 ; wx = camx + 8a' + 16b'
    sub     ecx, eax
    add     ecx, [camy]                 ; wy = camy + 16b' − 8a'
    cvtsi2ss xmm0, edx
    mulss   xmm0, [f_inv256]
    cvtsi2ss xmm1, ecx
    mulss   xmm1, [f_inv256]
    movss   xmm2, [rsi + 8]
    call    wind_vec
    ; цель в экранных px/с: u = (vx − vy)·16·V, v = (vx + vy)·8·V
    movaps  xmm4, xmm0
    subss   xmm4, xmm1
    addss   xmm0, xmm1
    mulss   xmm4, [f_wsu]
    mulss   xmm0, [f_wsv]
    movss   [fl_tu], xmm4
    movss   [fl_tv], xmm0
    ; порхание 9 px/с в тишь, завихрения — до ~50
    mulss   xmm3, [f_70]
    addss   xmm3, [f_9]
    movss   [fl_amp], xmm3
    movss   xmm0, [wind_t]
    mulss   xmm0, [f_1_7]
    addss   xmm0, [rsi + 24]
    call    fsin
    mulss   xmm0, [fl_amp]
    addss   xmm0, [fl_tu]
    movss   [fl_tu], xmm0
    movss   xmm0, [wind_t]
    mulss   xmm0, [f_1_3]
    movss   xmm1, [rsi + 24]
    mulss   xmm1, [f_1_7]
    addss   xmm0, xmm1
    call    fsin
    mulss   xmm0, [fl_amp]
    mulss   xmm0, [f_0_5]
    addss   xmm0, [fl_tv]
    movss   [fl_tv], xmm0
    ; скорость тянется к цели
    movss   xmm2, [rsi + 28]
    mulss   xmm2, [dt]
    minss   xmm2, [f_1]
    movss   xmm0, [fl_tu]
    subss   xmm0, [rsi + 16]
    mulss   xmm0, xmm2
    addss   xmm0, [rsi + 16]
    movss   [rsi + 16], xmm0
    movss   xmm1, [fl_tv]
    subss   xmm1, [rsi + 20]
    mulss   xmm1, xmm2
    addss   xmm1, [rsi + 20]
    movss   [rsi + 20], xmm1
    ; ход
    mulss   xmm0, [dt]
    addss   xmm0, [rsi]
    comiss  xmm0, [f_1024]
    jb      .x1
    subss   xmm0, [f_1024]
.x1:
    comiss  xmm0, [f_0]
    jae     .x2
    addss   xmm0, [f_1024]
.x2:
    movss   [rsi], xmm0
    mulss   xmm1, [dt]
    addss   xmm1, [rsi + 4]
    comiss  xmm1, [f_512]
    jb      .y1
    subss   xmm1, [f_512]
.y1:
    comiss  xmm1, [f_0]
    jae     .y2
    addss   xmm1, [f_512]
.y2:
    movss   [rsi + 4], xmm1
    movss   xmm0, [rsi + 12]
    mulss   xmm0, [dt]
    movss   xmm2, [rsi + 8]
    subss   xmm2, xmm0
    comiss  xmm2, [f_0]
    jae     .zok
    addss   xmm2, [f_320]
.zok:
    movss   [rsi + 8], xmm2
    add     rsi, FL_SZ
    dec     ebx
    jnz     .l
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- ветер у игрока
; для хода, тепла, запаха; сглаженная сила — цифре в HUD
player_wind:
    sub     rsp, 40
    movss   xmm0, [pl + E_X]
    movss   xmm1, [pl + E_Y]
    movss   xmm2, [f_10]
    call    wind_vec
    movss   [pl_wvx], xmm0
    movss   [pl_wvy], xmm1
    movss   [pl_ws], xmm2
    movss   xmm0, [dt]
    mulss   xmm0, [f_2]
    minss   xmm0, [f_1]
    subss   xmm2, [pl_wsm]
    mulss   xmm2, xmm0
    addss   xmm2, [pl_wsm]
    movss   [pl_wsm], xmm2
    add     rsp, 40
    ret

; ---------------------------------------------------------------- тепло
; на улице ветер выдувает: −(0.07 + 0.45·min(s, 1.1)²)/с — в тишь за ночь не замёрзнешь,
; в метель на юру за ~2.5 мин; бег греет на 0.1. В избе +0.6/с, у топящейся печи +3/с.
; Замёрз — теряет здоровье 0.5/с. Иней по краям кадра — с 50 и ниже
warm_update:
    sub     rsp, 40
    cmp     dword [gstate], 0
    jne     .frost
    cvttss2si eax, [pl + E_X]
    cvttss2si ecx, [pl + E_Y]
    and     eax, MAPW - 1
    and     ecx, MAPW - 1
    shl     ecx, 7
    add     eax, ecx
    lea     rdx, [rmap]
    cmp     byte [rdx + rax], 0
    jne     .inside
    movss   xmm0, [pl_ws]
    minss   xmm0, [f_1_1]               ; порыв сверх 13 м/с уже не важен
    mulss   xmm0, xmm0
    mulss   xmm0, [f_0_45]
    addss   xmm0, [f_0_07]
    cmp     dword [pl_sprint], 0
    je      .ns
    subss   xmm0, [f_0_1]
.ns:
    mulss   xmm0, [dt]
    movss   xmm1, [pl_warm]
    subss   xmm1, xmm0
    jmp     .set
.inside:
    call    near_stove
    movss   xmm0, [f_0_6]
    test    eax, eax
    jz      .ni
    movss   xmm0, [f_3]
.ni:
    mulss   xmm0, [dt]
    movss   xmm1, [pl_warm]
    addss   xmm1, xmm0
.set:
    maxss   xmm1, [f_0]
    minss   xmm1, [f_100]
    movss   [pl_warm], xmm1
    comiss  xmm1, [f_0]
    ja      .frost
    movss   xmm0, [dt]
    mulss   xmm0, [f_0_5]
    movss   xmm1, [pl_hp]
    subss   xmm1, xmm0
    movss   [pl_hp], xmm1
.frost:
    movss   xmm0, [f_50]
    subss   xmm0, [pl_warm]
    mulss   xmm0, [f_0_02]
    CLAMP01 xmm0
    movss   [frost_lvl], xmm0
    add     rsp, 40
    ret

; eax = 1, если в квадрате 5x5 тайлов вокруг игрока топится печь
near_stove:
    cvttss2si r8d, [pl + E_X]
    cvttss2si r9d, [pl + E_Y]
    mov     r10d, -2
.y:
    mov     r11d, -2
.x:
    lea     eax, [r8d + r11d]
    lea     ecx, [r9d + r10d]
    cmp     eax, MAPW
    jae     .n
    cmp     ecx, MAPW
    jae     .n
    shl     ecx, 7
    add     eax, ecx
    lea     rdx, [tmap]
    cmp     byte [rdx + rax], T_STOVEL
    jne     .n
    mov     eax, 1
    ret
.n:
    inc     r11d
    cmp     r11d, 2
    jle     .x
    inc     r10d
    cmp     r10d, 2
    jle     .y
    xor     eax, eax
    ret

; следы заметает: полоса в 32К ячеек за кадр (вся карта — за ~0.5 с). В затишье и в тишь —
; по 1, на открытом ветру и в снегопад — до 5; кровь заносит только ветром и снегом.
; Скорость — по затишью куска (16 ячеек = 2 тайла) из таблицы на кадр: 16 ступеней затишья
decay_tracks:
    push    rbx
    push    rsi
    push    rdi
    ; dk[i] = 1 + ветер·3.2·(1 − i/15) + снег·0.8
    lea     rdi, [dk_tab]
    xor     ecx, ecx
.t:
    cvtsi2ss xmm0, ecx
    mulss   xmm0, [f_inv15]
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    mulss   xmm1, [wind_g]
    mulss   xmm1, [f_3_2]
    movss   xmm0, [wnd_snow]
    mulss   xmm0, [f_0_8]
    addss   xmm1, xmm0
    addss   xmm1, [f_1]
    cvttss2si eax, xmm1
    mov     edx, 5
    cmp     eax, edx
    cmova   eax, edx
    mov     [rdi + rcx], al
    inc     ecx
    cmp     ecx, 16
    jb      .t
    mov     eax, [decay_pos]
    lea     rsi, [tracks]
    lea     rbx, [wsh]
    mov     ecx, 32768/16
    pxor    xmm3, xmm3
.l:
    ; затишье куска: ячейка 8/тайл -> 2/тайл
    mov     edx, eax
    and     edx, DECW - 1
    shr     edx, 2
    mov     r8d, eax
    shr     r8d, 12
    shl     r8d, 8
    add     edx, r8d
    movzx   edx, byte [rbx + rdx]
    shr     edx, 4
    movzx   edx, byte [rdi + rdx]
    imul    edx, edx, 0x01010101
    movd    xmm1, edx
    pshufd  xmm1, xmm1, 0
    movdqa  xmm0, [rsi + rax]
    psubusb xmm0, xmm1
    movdqa  [rsi + rax], xmm0
    ; кровь: на 1 медленнее
    psubusb xmm1, [ones16]
    movdqa  xmm0, [rsi + rax + (blood - tracks)]
    psubusb xmm0, xmm1
    movdqa  [rsi + rax + (blood - tracks)], xmm0
    add     eax, 16
    dec     ecx
    jnz     .l
    and     eax, DECW*DECW - 1
    mov     [decay_pos], eax
    pop     rdi
    pop     rsi
    pop     rbx
    ret
