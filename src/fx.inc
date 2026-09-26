; ----------------------------------------------------------------------------
;  fx.inc — частицы: пар изо рта, дым из труб, снег с веток и из-под ног
; ----------------------------------------------------------------------------
section .text

; копирует заготовку np в свободный слот (портит eax, ecx, edx, r8, xmm0, xmm1)
spawn_part:
    mov     eax, [p_cur]
    mov     ecx, MAXP
    lea     rdx, [parts]
.f:
    mov     r8d, eax
    shl     r8d, 5
    cmp     byte [rdx + r8 + P_KIND], 0
    je      .got
    inc     eax
    cmp     eax, MAXP
    jb      .ok
    xor     eax, eax
.ok:
    dec     ecx
    jnz     .f
    ret
.got:
    movdqu  xmm0, [np]
    movdqu  xmm1, [np + 16]
    movdqu  [rdx + r8], xmm0
    movdqu  [rdx + r8 + 16], xmm1
    inc     eax
    cmp     eax, MAXP
    jb      .s
    xor     eax, eax
.s:
    mov     [p_cur], eax
    ret

update_fx:
    sub     rsp, 40
    call    emit_breath
    call    emit_smoke
    call    emit_puffs
    call    emit_spray
    call    corpse_timers
    call    update_parts
    add     rsp, 40
    ret

; выдох: 4 клуба пара у рта; при беге и усталости чаще; у мёртвых — нет
emit_breath:
    push    rbx
    sub     rsp, 32
    cmp     dword [gstate], 1
    je      .out
    movss   xmm0, [br_t]
    subss   xmm0, [dt]
    movss   [br_t], xmm0
    comiss  xmm0, [f_0]
    ja      .out
    movss   xmm0, [pl_sta]
    mulss   xmm0, [f_0_015]
    addss   xmm0, [f_0_9]
    cmp     dword [pl_sprint], 0
    je      .ns
    movss   xmm0, [f_0_7]
.ns:
    movss   [tmp_b], xmm0
    call    randf
    mulss   xmm0, [f_0_4]
    addss   xmm0, [tmp_b]
    movss   [br_t], xmm0
    call    snd_breath                  ; выдох слышно, когда устал, бежишь или продрог
    mov     ebx, 4
.p:
    call    randf
    subss   xmm0, [f_0_5]
    mulss   xmm0, [f_0_06]
    movss   xmm1, [pl + E_FX]
    mulss   xmm1, [f_0_22]
    addss   xmm0, xmm1
    addss   xmm0, [pl + E_X]
    movss   [np + P_X], xmm0
    call    randf
    subss   xmm0, [f_0_5]
    mulss   xmm0, [f_0_06]
    movss   xmm1, [pl + E_FY]
    mulss   xmm1, [f_0_22]
    addss   xmm0, xmm1
    addss   xmm0, [pl + E_Y]
    movss   [np + P_Y], xmm0
    call    randf
    mulss   xmm0, [f_1_5]
    addss   xmm0, [f_19]
    movss   [np + P_Z], xmm0
    call    randf
    mulss   xmm0, [f_0_25]
    addss   xmm0, [f_0_4]
    movss   [tmp_b], xmm0
    mulss   xmm0, [pl + E_FX]
    movss   xmm1, [pl_wvx]              ; ветер у игрока сразу подхватывает
    mulss   xmm1, [f_1_2]
    addss   xmm0, xmm1
    movss   [np + P_VX], xmm0
    movss   xmm0, [tmp_b]
    mulss   xmm0, [pl + E_FY]
    movss   xmm1, [pl_wvy]
    mulss   xmm1, [f_1_2]
    addss   xmm0, xmm1
    movss   [np + P_VY], xmm0
    call    randf
    mulss   xmm0, [f_2]
    addss   xmm0, [f_2]
    movss   [np + P_VZ], xmm0
    call    randf
    mulss   xmm0, [f_0_5]
    addss   xmm0, [f_1]
    movss   [np + P_LIFE], xmm0
    mov     byte [np + P_KIND], PK_BREATH
    call    rand
    mov     [np + P_SEED], al
    call    spawn_part
    dec     ebx
    jnz     .p
.out:
    add     rsp, 32
    pop     rbx
    ret

; дым из труб топящихся печей рядом с камерой
emit_smoke:
    push    rbx
    push    rsi
    sub     rsp, 40
    lea     rsi, [smk]
    mov     ebx, [nsmk]
.l:
    test    ebx, ebx
    jz      .out
    movzx   eax, byte [rsi]
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    movss   [tmp_a], xmm0
    subss   xmm0, [cam_xf]
    andps   xmm0, [f_absmask]
    comiss  xmm0, [f_26]
    jae     .n
    movzx   eax, byte [rsi + 1]
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    movss   [tmp_b], xmm0
    subss   xmm0, [cam_yf]
    andps   xmm0, [f_absmask]
    comiss  xmm0, [f_26]
    jae     .n
    ; ветер над трубой: в сильный дым рвёт — клубы чаще, мельче и бледнее (шлейф)
    movss   xmm0, [tmp_a]
    movss   xmm1, [tmp_b]
    call    wind_top
    movss   [sm_s], xmm2
    call    randf
    movss   xmm1, [sm_s]
    mulss   xmm1, [f_2_5]
    addss   xmm1, [f_1]
    movss   xmm2, [frost_k]             ; в мороз печи топят жарче — гуще
    mulss   xmm2, [f_0_6]
    addss   xmm1, xmm2
    mulss   xmm1, [dt]
    mulss   xmm1, [f_9]                 ; ~9 клубов в секунду, в ветер — до 3.5 раза чаще
    comiss  xmm0, xmm1
    jae     .n
    call    randf
    subss   xmm0, [f_0_5]
    mulss   xmm0, [f_0_1]
    addss   xmm0, [tmp_a]
    movss   [np + P_X], xmm0
    call    randf
    subss   xmm0, [f_0_5]
    mulss   xmm0, [f_0_1]
    addss   xmm0, [tmp_b]
    movss   [np + P_Y], xmm0
    call    randf
    mov     rax, rsi                    ; из жерла трубы над крышей
    lea     rdx, [smk]
    sub     rax, rdx
    shr     rax, 1
    lea     rdx, [smkz]
    movzx   eax, byte [rdx + rax]
    inc     eax
    cvtsi2ss xmm1, eax
    addss   xmm0, xmm1
    movss   [np + P_Z], xmm0
    movss   xmm0, [wind_dx]
    mulss   xmm0, [wind_g]
    mulss   xmm0, [f_0_3]
    movss   [np + P_VX], xmm0
    movss   xmm0, [wind_dy]
    mulss   xmm0, [wind_g]
    mulss   xmm0, [f_0_3]
    movss   [np + P_VY], xmm0
    call    randf
    mulss   xmm0, [f_3]
    addss   xmm0, [f_7]
    movss   [np + P_VZ], xmm0
    call    randf
    mulss   xmm0, [f_2]
    addss   xmm0, [f_5]
    movss   xmm1, [frost_k]             ; в мороз столб выше: живёт до 1.8 раза дольше
    mulss   xmm1, [f_0_8]
    addss   xmm1, [f_1]
    mulss   xmm0, xmm1
    movss   [np + P_LIFE], xmm0
    mov     byte [np + P_KIND], PK_SMOKE
    call    rand
    mov     [np + P_SEED], al
    movss   xmm0, [sm_s]
    mulss   xmm0, [f_100]
    cvttss2si eax, xmm0
    mov     ecx, 255
    cmp     eax, ecx
    cmova   eax, ecx
    mov     [np + P_WS], al
    movss   xmm0, [frost_k]
    mulss   xmm0, [f_255]
    cvttss2si eax, xmm0
    mov     [np + P_FR], al
    call    spawn_part
    mov     byte [np + P_WS], 0
    mov     byte [np + P_FR], 0
.n:
    add     rsi, 2
    dec     ebx
    jmp     .l
.out:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret

; порыв сильнее 0.75 изредка стряхивает снег с деревьев вокруг камеры
emit_puffs:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32
    cvttss2si r12d, [cam_xf]
    cvttss2si r13d, [cam_yf]
    lea     esi, [r13d - 20]
.y:
    lea     eax, [r13d + 20]
    cmp     esi, eax
    jg      .out
    cmp     esi, MAPW
    jae     .ny
    lea     ebx, [r12d - 20]
.x:
    lea     eax, [r12d + 20]
    cmp     ebx, eax
    jg      .ny
    cmp     ebx, MAPW
    jae     .nx
    mov     eax, esi
    shl     eax, 7
    add     eax, ebx
    lea     rdx, [tmap]
    movzx   edi, byte [rdx + rax]
    cmp     edi, T_SPRUCE
    je      .tree
    cmp     edi, T_PINE
    je      .tree
    cmp     edi, T_YOUNG
    je      .tree
    cmp     edi, T_BIRCH
    jne     .nx
.tree:
    cvtsi2ss xmm0, ebx
    addss   xmm0, [f_0_5]
    cvtsi2ss xmm1, esi
    addss   xmm1, [f_0_5]
    call    wind_top                    ; крона над затишьем
    movaps  xmm0, xmm2
    subss   xmm0, [f_0_75]
    comiss  xmm0, [f_0]
    jbe     .nx
    mulss   xmm0, [f_0_12]
    mulss   xmm0, [dt]
    movss   [tmp_c], xmm0
    call    randf
    comiss  xmm0, [tmp_c]
    jae     .nx
    call    snd_flump
    call    puff_burst
.nx:
    inc     ebx
    jmp     .x
.ny:
    inc     esi
    jmp     .y
.out:
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ebx = tx, esi = ty: 7 комков снега из кроны
puff_burst:
    push    r12
    sub     rsp, 32
    mov     r12d, 7
.p:
    call    randf
    subss   xmm0, [f_0_5]
    mulss   xmm0, [f_0_7]
    cvtsi2ss xmm1, ebx
    addss   xmm0, xmm1
    addss   xmm0, [f_0_5]
    movss   [np + P_X], xmm0
    call    randf
    subss   xmm0, [f_0_5]
    mulss   xmm0, [f_0_7]
    cvtsi2ss xmm1, esi
    addss   xmm0, xmm1
    addss   xmm0, [f_0_5]
    movss   [np + P_Y], xmm0
    call    randf
    mulss   xmm0, [f_34]
    addss   xmm0, [f_18]
    movss   [np + P_Z], xmm0
    movss   xmm0, [wind_dx]
    mulss   xmm0, [wind_g]
    movss   [np + P_VX], xmm0
    movss   xmm0, [wind_dy]
    mulss   xmm0, [wind_g]
    movss   [np + P_VY], xmm0
    call    randf
    mulss   xmm0, [f_m6]
    subss   xmm0, [f_2]
    movss   [np + P_VZ], xmm0
    call    randf
    mulss   xmm0, [f_0_8]
    addss   xmm0, [f_1_2]
    movss   [np + P_LIFE], xmm0
    mov     byte [np + P_KIND], PK_PUFF
    call    rand
    mov     [np + P_SEED], al
    call    spawn_part
    dec     r12d
    jnz     .p
    add     rsp, 32
    pop     r12
    ret

; снег из-под ног на бегу (вызывается на шаге)
emit_kick:
    push    rbx
    sub     rsp, 32
    mov     ebx, 3
.p:
    call    randf
    subss   xmm0, [f_0_5]
    mulss   xmm0, [f_0_2]
    addss   xmm0, [pl + E_X]
    movss   [np + P_X], xmm0
    call    randf
    subss   xmm0, [f_0_5]
    mulss   xmm0, [f_0_2]
    addss   xmm0, [pl + E_Y]
    movss   [np + P_Y], xmm0
    mov     dword [np + P_Z], __float32__(1.0)
    call    randf
    subss   xmm0, [f_0_5]
    movss   xmm1, [mv_x]
    mulss   xmm1, [f_m0_8]
    addss   xmm0, xmm1
    movss   [np + P_VX], xmm0
    call    randf
    subss   xmm0, [f_0_5]
    movss   xmm1, [mv_y]
    mulss   xmm1, [f_m0_8]
    addss   xmm0, xmm1
    movss   [np + P_VY], xmm0
    call    randf
    mulss   xmm0, [f_12]
    addss   xmm0, [f_14]
    movss   [np + P_VZ], xmm0
    call    randf
    mulss   xmm0, [f_0_3]
    addss   xmm0, [f_0_35]
    movss   [np + P_LIFE], xmm0
    mov     byte [np + P_KIND], PK_KICK
    call    rand
    mov     [np + P_SEED], al
    call    spawn_part
    dec     ebx
    jnz     .p
    add     rsp, 32
    pop     rbx
    ret

; время с момента смерти (снег заносит тела)
corpse_timers:
    lea     rdx, [ents]
    mov     ecx, MAXE
    movss   xmm1, [dt]
.l:
    cmp     dword [rdx + E_ST], ST_CORPSE
    jne     .n
    movss   xmm0, [rdx + E_TM]
    addss   xmm0, xmm1
    movss   [rdx + E_TM], xmm0
.n:
    add     rdx, ESZ
    dec     ecx
    jnz     .l
    cmp     dword [gstate], 1
    jne     .out
    movss   xmm0, [pl + E_TM]
    addss   xmm0, xmm1
    movss   [pl + E_TM], xmm0
.out:
    ret

update_parts:
    push    rbx
    push    r12
    sub     rsp, 40
    lea     rbx, [parts]
    mov     r12d, MAXP
.l:
    movzx   eax, byte [rbx + P_KIND]
    test    eax, eax
    jz      .n
    mov     [up_kind], eax
    mov     dword [up_s], 0
    cmp     eax, PK_KICK
    je      .life
    ; ветер у частицы, тайлы/с (дым и снег с конька — над затишьем)
    movss   xmm0, [rbx + P_X]
    movss   xmm1, [rbx + P_Y]
    movss   xmm2, [rbx + P_Z]
    cmp     eax, PK_SMOKE
    je      .top
    cmp     eax, PK_SPRAY
    je      .top
    call    wind_vec
    jmp     .gw
.top:
    call    wind_top
.gw:
    mulss   xmm0, [f_wspd]
    mulss   xmm1, [f_wspd]
    movss   [up_wx], xmm0
    movss   [up_wy], xmm1
    movss   [up_s], xmm2
    ; завихрения — плавная тяга вбок, своя у каждой частицы
    mulss   xmm3, [f_wspd]
    movss   [up_t], xmm3
    movzx   eax, byte [rbx + P_SEED]
    cvtsi2ss xmm0, eax
    mulss   xmm0, [f_0_025]
    movss   [up_ph], xmm0
    movss   xmm1, [wind_t]
    mulss   xmm1, [f_2_6]
    addss   xmm0, xmm1
    call    fsin
    mulss   xmm0, [up_t]
    movss   [up_ax], xmm0
    movss   xmm0, [up_ph]
    mulss   xmm0, [f_1_7]
    movss   xmm1, [wind_t]
    mulss   xmm1, [f_1_9]
    addss   xmm0, xmm1
    call    fsin
    mulss   xmm0, [up_t]
    movss   [up_ay], xmm0
.life:
    ; жизнь: дым и пар в сильный ветер рвёт быстрее
    movss   xmm7, [dt]
    movss   xmm0, xmm7
    mov     eax, [up_kind]
    cmp     eax, PK_SMOKE
    je      .fast
    cmp     eax, PK_BREATH
    jne     .lf
.fast:
    movss   xmm1, [up_s]
    mulss   xmm1, [f_1_2]
    addss   xmm1, [f_1]
    mulss   xmm0, xmm1
.lf:
    movss   xmm1, [rbx + P_LIFE]
    subss   xmm1, xmm0
    movss   [rbx + P_LIFE], xmm1
    comiss  xmm1, [f_0]
    ja      .alive
    mov     byte [rbx + P_KIND], 0
    jmp     .n
.alive:
    movss   xmm1, [rbx + P_VX]
    movss   xmm2, [rbx + P_VY]
    movss   xmm3, [rbx + P_VZ]
    cmp     eax, PK_SMOKE
    je      .smoke
    cmp     eax, PK_PUFF
    je      .puff
    cmp     eax, PK_KICK
    je      .kick
    cmp     eax, PK_SPRAY
    je      .spray
    ; пар: вязко тянется к 0.6 ветра, чуть всплывает
    movss   xmm5, [f_0_6]
    movss   xmm6, [f_1_6]
    call    .toward
    movss   xmm4, xmm7
    mulss   xmm4, [f_1_5]
    addss   xmm3, xmm4
    jmp     .move
.smoke:
    ; дым догоняет ветер (в сильный — быстрее) и не поднимается — прибивает к крышам
    movss   xmm5, [f_0_9]
    movss   xmm6, [up_s]
    mulss   xmm6, [f_0_8]
    addss   xmm6, [f_0_6]
    call    .toward
    movss   xmm4, [up_s]
    mulss   xmm4, [f_0_8]
    mulss   xmm4, xmm7
    movss   xmm0, [f_1]
    subss   xmm0, xmm4
    maxss   xmm0, [f_0]
    mulss   xmm3, xmm0
    ; мороз: над слоем инверсии (88..119 px) дым не поднимается, а расползается
    movss   xmm0, [frost_k]
    comiss  xmm0, [f_0_05]
    jb      .move
    movzx   eax, byte [rbx + P_SEED]
    mov     ecx, eax
    and     ecx, 31
    add     ecx, 88
    cvtsi2ss xmm4, ecx
    comiss  xmm4, [rbx + P_Z]
    jae     .move
    movss   xmm4, xmm7
    mulss   xmm4, [f_4]
    mulss   xmm4, xmm0
    movss   xmm5, [f_1]
    subss   xmm5, xmm4
    maxss   xmm5, [f_0]
    mulss   xmm3, xmm5                  ; вверх — стихает
    mulss   xmm0, [sm_spr]              ; вбок — к своей стороне (по номеру), тайлы/с
    mov     ecx, eax
    and     ecx, 15
    cvtsi2ss xmm4, ecx
    subss   xmm4, [lt_7_5]
    mulss   xmm4, xmm0
    subss   xmm4, xmm1
    mulss   xmm4, xmm7
    addss   xmm1, xmm4
    shr     eax, 4
    cvtsi2ss xmm4, eax
    subss   xmm4, [lt_7_5]
    mulss   xmm4, xmm0
    subss   xmm4, xmm2
    mulss   xmm4, xmm7
    addss   xmm2, xmm4
    jmp     .move
.puff:
    movss   xmm4, xmm7
    mulss   xmm4, [f_22]
    subss   xmm3, xmm4
    maxss   xmm3, [f_m26]
    movss   xmm5, [f_0_8]
    movss   xmm6, [f_1]
    call    .toward
    jmp     .move
.spray:
    ; сорванный с конька снег лёгок: почти сразу летит с ветром, медленно оседает
    movss   xmm4, xmm7
    mulss   xmm4, [f_9]
    subss   xmm3, xmm4
    maxss   xmm3, [f_m18]
    movss   xmm5, [f_1]
    movss   xmm6, [f_3]
    call    .toward
    jmp     .move
.kick:
    movss   xmm4, xmm7
    mulss   xmm4, [f_70]
    subss   xmm3, xmm4
.move:
    movss   [rbx + P_VX], xmm1
    movss   [rbx + P_VY], xmm2
    movss   [rbx + P_VZ], xmm3
    mulss   xmm1, xmm7
    addss   xmm1, [rbx + P_X]
    movss   [rbx + P_X], xmm1
    mulss   xmm2, xmm7
    addss   xmm2, [rbx + P_Y]
    movss   [rbx + P_Y], xmm2
    mulss   xmm3, xmm7
    addss   xmm3, [rbx + P_Z]
    movss   [rbx + P_Z], xmm3
    comiss  xmm3, [f_0]
    ja      .n
    mov     byte [rbx + P_KIND], 0      ; упала в снег
.n:
    add     rbx, P_SZ
    dec     r12d
    jnz     .l
    add     rsp, 40
    pop     r12
    pop     rbx
    ret
; xmm1, xmm2 += ((ветер·xmm5 + завихрения) − v)·xmm6·dt
.toward:
    movss   xmm0, [up_wx]
    mulss   xmm0, xmm5
    addss   xmm0, [up_ax]
    subss   xmm0, xmm1
    mulss   xmm0, xmm6
    mulss   xmm0, xmm7
    addss   xmm1, xmm0
    movss   xmm0, [up_wy]
    mulss   xmm0, xmm5
    addss   xmm0, [up_ay]
    subss   xmm0, xmm2
    mulss   xmm0, xmm6
    mulss   xmm0, xmm7
    addss   xmm2, xmm0
    ret

; снег с коньков: порыв поперёк конька срывает с гребня шлейф (избы у камеры)
emit_spray:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32
    lea     rbx, [houses]
    mov     r12d, [nhouses]
.h:
    test    r12d, r12d
    jz      .out
    cvtsi2ss xmm0, dword [rbx + HS_X0]
    cvtsi2ss xmm1, dword [rbx + HS_W]
    mulss   xmm1, [f_0_5]
    addss   xmm0, xmm1
    movss   [sp_cx], xmm0
    cvtsi2ss xmm2, dword [rbx + HS_Y0]
    cvtsi2ss xmm3, dword [rbx + HS_H]
    mulss   xmm3, [f_0_5]
    addss   xmm2, xmm3
    movss   [sp_cy], xmm2
    subss   xmm0, [cam_xf]
    andps   xmm0, [f_absmask]
    comiss  xmm0, [f_26]
    jae     .nh
    subss   xmm2, [cam_yf]
    andps   xmm2, [f_absmask]
    comiss  xmm2, [f_26]
    jae     .nh
    movss   xmm0, [rbx + HS_FADE]       ; снятая крыша (игрок в избе) не пылит
    comiss  xmm0, [f_0_5]
    jb      .nh
    movss   xmm0, [sp_cx]
    movss   xmm1, [sp_cy]
    call    wind_top
    movss   [sp_wx], xmm0
    movss   [sp_wy], xmm1
    ; поперёк конька (конёк вдоль x — это y, и наоборот)·0.8 + сила·0.2 − 0.5
    cmp     dword [rbx + HS_AX], 0
    je      .ax0
    movaps  xmm1, xmm0
.ax0:
    andps   xmm1, [f_absmask]
    mulss   xmm1, [f_0_8]
    mulss   xmm2, [f_0_2]
    addss   xmm1, xmm2
    subss   xmm1, [f_0_5]
    comiss  xmm1, [f_0]
    jbe     .nh
    ; частиц за кадр: ·длина конька·16·dt, дробь — жребием
    mov     eax, [rbx + HS_W]
    cmp     dword [rbx + HS_AX], 0
    je      .l0
    mov     eax, [rbx + HS_H]
.l0:
    mov     [sp_len], eax
    cvtsi2ss xmm0, eax
    mulss   xmm1, xmm0
    mulss   xmm1, [f_16]
    mulss   xmm1, [dt]
    cvttss2si r13d, xmm1
    cvtsi2ss xmm0, r13d
    subss   xmm1, xmm0
    movss   [tmp_c], xmm1
    call    wrandf
    comiss  xmm0, [tmp_c]
    jae     .cnt
    inc     r13d
.cnt:
    cmp     r13d, 10
    jbe     .p
    mov     r13d, 10
.p:
    test    r13d, r13d
    jz      .nh
    call    wrandf
    cvtsi2ss xmm1, dword [sp_len]
    addss   xmm1, [f_0_4]
    mulss   xmm0, xmm1
    subss   xmm0, [f_0_2]               ; вдоль конька, со свесами
    cmp     dword [rbx + HS_AX], 0
    jne     .py
    cvtsi2ss xmm1, dword [rbx + HS_X0]
    addss   xmm0, xmm1
    movss   [np + P_X], xmm0
    movss   xmm0, [sp_cy]
    movss   [np + P_Y], xmm0
    jmp     .pz
.py:
    cvtsi2ss xmm1, dword [rbx + HS_Y0]
    addss   xmm0, xmm1
    movss   [np + P_Y], xmm0
    movss   xmm0, [sp_cx]
    movss   [np + P_X], xmm0
.pz:
    call    wrandf
    mulss   xmm0, [f_2]
    cvtsi2ss xmm1, dword [rbx + HS_ZR]
    addss   xmm0, xmm1
    addss   xmm0, [f_1]
    movss   [np + P_Z], xmm0
    call    wrandf
    mulss   xmm0, [f_0_4]
    addss   xmm0, [f_0_5]
    mulss   xmm0, [f_wspd]
    movss   xmm1, xmm0
    mulss   xmm0, [sp_wx]
    movss   [np + P_VX], xmm0
    mulss   xmm1, [sp_wy]
    movss   [np + P_VY], xmm1
    call    wrandf
    mulss   xmm0, [f_6]
    addss   xmm0, [f_3]
    movss   [np + P_VZ], xmm0
    call    wrandf
    mulss   xmm0, [f_0_8]
    addss   xmm0, [f_0_7]
    movss   [np + P_LIFE], xmm0
    mov     byte [np + P_KIND], PK_SPRAY
    call    wrand
    mov     [np + P_SEED], al
    call    spawn_part
    dec     r13d
    jmp     .p
.nh:
    add     rbx, HS_SZ
    dec     r12d
    jmp     .h
.out:
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- рисование частиц
; поверх освещённого кадра: свет берётся из сетки под частицей, диск с мягким краем,
; не рисуется поверх того, что ближе к камере (сравнение глубины по gposb)
draw_parts:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    lea     rbx, [parts]
    mov     r15d, MAXP
    pxor    xmm15, xmm15
    mov     dword [pp_n], 0
.l:
    movzx   eax, byte [rbx + P_KIND]
    test    eax, eax
    jz      .n
    mov     [dp_kind], eax
    movss   xmm0, [rbx + P_X]
    mulss   xmm0, [f_256]
    cvtss2si r12d, xmm0                 ; wx
    movss   xmm0, [rbx + P_Y]
    mulss   xmm0, [f_256]
    cvtss2si r13d, xmm0                 ; wy
    lea     eax, [r12d + r13d]
    mov     [dp_depth], eax
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
    cvttss2si ecx, [rbx + P_Z]
    sub     eax, ecx
    mov     edi, eax                    ; sy
    cmp     esi, -8
    jl      .n
    cmp     esi, SCR_W + 8
    jg      .n
    cmp     edi, -8
    jl      .n
    cmp     edi, SCR_H + 8
    jg      .n
    ; свет у земли под частицей
    mov     eax, r12d
    sar     eax, 6
    sub     eax, [gox]
    mov     ecx, r13d
    sar     ecx, 6
    sub     ecx, [goy]
    xor     edx, edx
    cmp     eax, edx
    cmovl   eax, edx
    cmp     ecx, edx
    cmovl   ecx, edx
    mov     edx, GW-1
    cmp     eax, edx
    cmovg   eax, edx
    cmp     ecx, edx
    cmovg   ecx, edx
    imul    ecx, ecx, GW
    add     eax, ecx
    lea     rdx, [lgrid]
    mov     r8d, [rdx + rax*4]          ; B,G,R,V
    ; + луна на высоте частицы: пар над сугробом бывает в тени, а выше — в свете
    ; (ячейка развёртки под частицей: горизонт+64, облака)
    mov     r9d, r12d
    sar     r9d, 5
    sub     r9d, [m8_ox]
    mov     r10d, r13d
    sar     r10d, 5
    sub     r10d, [m8_oy]
    xor     r11d, r11d
    cmp     r9d, r11d
    cmovl   r9d, r11d
    cmp     r10d, r11d
    cmovl   r10d, r11d
    mov     r11d, SW8-1
    cmp     r9d, r11d
    cmovg   r9d, r11d
    cmp     r10d, r11d
    cmovg   r10d, r11d
    imul    r10d, r10d, SW8
    add     r9d, r10d
    lea     rdx, [mz8]
    mov     r9d, [rdx + r9*4]
    movzx   r10d, r9b
    sub     r10d, 64                    ; горизонт
    shr     r9d, 8
    and     r9d, 255                    ; облака
    cvttss2si r11d, [rbx + P_Z]
    sub     r11d, r10d
    shl     r11d, 5
    add     r11d, 256
    xor     r10d, r10d
    cmp     r11d, r10d
    cmovl   r11d, r10d
    mov     r10d, 256
    cmp     r11d, r10d
    cmovg   r11d, r10d
    imul    r11d, r9d
    shr     r11d, 8
    movd    xmm0, r11d
    pshuflw xmm0, xmm0, 0
    movq    xmm1, [moon_n + N_BC*8]
    pmullw  xmm0, xmm1
    psrlw   xmm0, 8
    packuswb xmm0, xmm0
    movd    xmm1, r8d
    paddusb xmm1, xmm0
    movd    r8d, xmm1
    movss   xmm2, [rbx + P_LIFE]
    mov     ecx, [dp_kind]
    cmp     ecx, PK_SMOKE
    je      .ksmoke
    cmp     ecx, PK_BREATH
    je      .kbreath
    ; снег
    mov     r14d, 236
    movss   xmm0, xmm2
    mulss   xmm0, [f_2]
    minss   xmm0, [f_1]
    mulss   xmm0, [f_0_8]
    movss   xmm1, [f_0_8]
    cmp     ecx, PK_SPRAY
    jne     .kset
    mulss   xmm0, [f_0_65]              ; сорванный с конька — пыль в пиксель
    movss   xmm1, [f_0_4]
    jmp     .kset
.kbreath:
    ; пар в свете фонарика подсвечивается
    cmp     dword [pl_flash], 0
    je      .kb1
    movd    xmm0, r8d
    movd    xmm1, [breath_add]
    paddusb xmm0, xmm1
    movd    r8d, xmm0
.kb1:
    mov     r14d, 230
    movss   xmm0, xmm2
    mulss   xmm0, [f_2]
    minss   xmm0, [f_1]
    mulss   xmm0, [f_0_32]
    movss   xmm1, [f_1_5]
    subss   xmm1, xmm2
    maxss   xmm1, [f_0]
    mulss   xmm1, [f_1_6]
    addss   xmm1, [f_1]
    jmp     .kset
.ksmoke:
    ; дым серый, светит небом; выпущенный в мороз — белее (пар), живёт дольше: жизнь
    ; приводим к обычной (/(1 + 0.8·мороз)), клуб шире (·(1 + 0.5·мороз))
    mov     r8d, [sky_b]
    or      r8d, 0xFF000000
    movzx   eax, byte [rbx + P_FR]
    cvtsi2ss xmm3, eax
    mulss   xmm3, [f_inv255]
    movss   [dp_fr], xmm3
    mulss   xmm3, [f_0_8]
    addss   xmm3, [f_1]
    divss   xmm2, xmm3
    imul    eax, eax, 30
    shr     eax, 8
    lea     r14d, [eax + 200]
    movss   xmm0, xmm2
    mulss   xmm0, [f_0_4]
    minss   xmm0, [f_1]                 ; тает к концу жизни
    movss   xmm3, [f_7]
    subss   xmm3, xmm2
    mulss   xmm3, [f_2]
    minss   xmm3, [f_1]                 ; быстро густеет у трубы
    mulss   xmm0, xmm3
    mulss   xmm0, [f_0_6]
    movss   xmm1, [f_7]
    subss   xmm1, xmm2
    maxss   xmm1, [f_0]
    mulss   xmm1, [f_0_45]
    addss   xmm1, [f_1_2]
    ; выпущен в ветер (сила ·100 в байте) — мельче и бледнее: радиус ×(1 − 0.5 s), альфа ×(1 − 0.4 s)
    movzx   eax, byte [rbx + P_WS]
    cvtsi2ss xmm3, eax
    mulss   xmm3, [f_0_005]
    movss   xmm4, [f_1]
    subss   xmm4, xmm3
    maxss   xmm4, [f_0_4]
    mulss   xmm1, xmm4
    mulss   xmm3, [f_0_8]
    movss   xmm4, [f_1]
    subss   xmm4, xmm3
    maxss   xmm4, [f_0_4]
    mulss   xmm0, xmm4
    movss   xmm4, [dp_fr]
    mulss   xmm4, [f_0_5]
    addss   xmm4, [f_1]
    mulss   xmm1, xmm4
.kset:
    ; видимость (дым над крышами виден и из «памяти»)
    mov     eax, r8d
    shr     eax, 24
    cmp     dword [dp_kind], PK_SMOKE
    je      .v0
    cmp     dword [dp_kind], PK_SPRAY
    jne     .v1
.v0:
    mov     eax, 255                    ; дым и снег с коньков над крышами видны отовсюду
.v1:
    cvtsi2ss xmm3, eax
    mulss   xmm3, [f_inv255]
    mulss   xmm0, xmm3
    mulss   xmm0, [f_128]
    cvttss2si eax, xmm0
    test    eax, eax
    jle     .n
    mov     [dp_a], eax
    ; цвет = альбедо·свет/128
    movd    xmm4, r8d
    punpcklbw xmm4, xmm15
    movd    xmm5, r14d
    pshuflw xmm5, xmm5, 0
    pmullw  xmm4, xmm5
    psrlw   xmm4, 7
    packuswb xmm4, xmm4
    punpcklbw xmm4, xmm15
    ; радиус
    movss   [dp_r], xmm1
    addss   xmm1, [f_0_5]
    mulss   xmm1, xmm1
    movss   xmm6, [f_1]
    divss   xmm6, xmm1
    cvttss2si ebp, [dp_r]
    inc     ebp
    ; в список: рисуют потоки пула полосами строк (parts_rows) — каждый все частицы
    ; по порядку, но в своих строках: у пикселя те же смешивания в том же порядке
    mov     eax, [pp_n]
    inc     dword [pp_n]
    shl     eax, 5
    lea     rdx, [pp_buf]
    add     rdx, rax
    mov     [rdx], esi                  ; sx
    mov     [rdx + 4], edi              ; sy
    mov     eax, [dp_depth]
    add     eax, 96
    mov     [rdx + 8], eax              ; предел глубины
    mov     eax, [dp_a]
    mov     [rdx + 12], eax
    movss   [rdx + 16], xmm6            ; 1 / (r + 0.5)²
    mov     [rdx + 20], ebp             ; радиус, px
    movq    [rdx + 24], xmm4            ; цвет словами
.n:
    add     rbx, P_SZ
    dec     r15d
    jnz     .l
    lea     rcx, [parts_rows]
    mov     edx, SCR_H
    mov     r8d, 16
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

; строки кадра ecx .. edx−1 (с любого потока пула): частицы pp_buf по порядку — диском
; с альфой к краю, за тем, что ближе к глазу, не видно (как было в draw_parts)
parts_rows:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    mov     r14d, ecx
    mov     r15d, edx
    pxor    xmm15, xmm15
    lea     rbx, [pp_buf]
    mov     r10d, [pp_n]
    mov     r11, [fbbits]
    lea     r8, [gposb]
.p:
    test    r10d, r10d
    jz      .out
    mov     esi, [rbx]                  ; sx
    mov     edi, [rbx + 4]              ; sy
    mov     ebp, [rbx + 20]             ; радиус
    movss   xmm6, [rbx + 16]
    movq    xmm4, [rbx + 24]
    cvtsi2ss xmm7, dword [rbx + 12]     ; сила
    mov     r12d, ebp
    neg     r12d                        ; dy
.dy:
    cmp     r12d, ebp
    jg      .pn
    lea     eax, [edi + r12d]
    cmp     eax, r14d
    jl      .dyn                        ; выше полосы
    cmp     eax, r15d
    jge     .pn                         ; ниже — дальше строки только ниже
    imul    r9d, eax, SCR_W
    mov     r13d, ebp
    neg     r13d                        ; dx
.dx:
    cmp     r13d, ebp
    jg      .dyn
    lea     eax, [esi + r13d]
    cmp     eax, SCR_W
    jae     .dxn
    mov     ecx, r13d
    imul    ecx, ecx
    mov     edx, r12d
    imul    edx, edx
    add     ecx, edx
    xorps   xmm0, xmm0                  ; cvtsi2ss сливает верх: без xor ждал бы прошлый пиксель
    cvtsi2ss xmm0, ecx
    mulss   xmm0, xmm6
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    comiss  xmm1, [f_0]
    jbe     .dxn
    add     eax, r9d
    mov     ecx, [r8 + rax*4]
    movzx   edx, cx
    shr     ecx, 16
    add     ecx, edx
    cmp     ecx, [rbx + 8]
    jg      .dxn
    movaps  xmm0, xmm7
    mulss   xmm0, xmm1
    cvttss2si ecx, xmm0                 ; 0..128
    movd    xmm0, [r11 + rax*4]
    punpcklbw xmm0, xmm15
    movdqa  xmm1, xmm4
    psubw   xmm1, xmm0
    movd    xmm2, ecx
    pshuflw xmm2, xmm2, 0
    pmullw  xmm1, xmm2
    psraw   xmm1, 7
    paddw   xmm0, xmm1
    packuswb xmm0, xmm0
    movd    [r11 + rax*4], xmm0
.dxn:
    inc     r13d
    jmp     .dx
.dyn:
    inc     r12d
    jmp     .dy
.pn:
    add     rbx, 32
    dec     r10d
    jmp     .p
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
