; ----------------------------------------------------------------------------
;  coll.inc — столкновения по тому, что нарисовано: фигуры тайлов (скруглённые
;  прямоугольники в координатах тайла), выталкивание шагами, удар не сквозь стену
; ----------------------------------------------------------------------------
section .text

; cmap[тайл] = набор фигур (CS_*); у забора — по соседям, как он нарисован:
; доски стоят по передним кромкам тайла (+y — левая на экране, +x — правая)
build_cmap:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40
    lea     rdi, [cmap]
    xor     esi, esi                    ; y
.y:
    xor     ebx, ebx                    ; x
.x:
    mov     ecx, ebx
    mov     edx, esi
    call    get_tile
    lea     r8, [tcol]
    movzx   r12d, byte [r8 + rax]
    cmp     eax, T_FENCE
    jne     .put
    ; вдоль x, если сосед слева или справа — забор; вдоль y — сверху или снизу
    xor     r12d, r12d
    lea     ecx, [ebx - 1]
    mov     edx, esi
    call    get_tile
    cmp     eax, T_FENCE
    je      .fx
    lea     ecx, [ebx + 1]
    mov     edx, esi
    call    get_tile
    cmp     eax, T_FENCE
    jne     .fy
.fx:
    or      r12d, 1
.fy:
    mov     ecx, ebx
    lea     edx, [esi - 1]
    call    get_tile
    cmp     eax, T_FENCE
    je      .fyy
    mov     ecx, ebx
    lea     edx, [esi + 1]
    call    get_tile
    cmp     eax, T_FENCE
    jne     .fz
.fyy:
    or      r12d, 2
.fz:
    test    r12d, r12d
    jnz     .fset
    mov     r12d, 3
.fset:
    add     r12d, CS_FY - 1             ; 1 -> CS_FY, 2 -> CS_FX, 3 -> CS_FXY
.put:
    mov     eax, esi
    shl     eax, 7
    add     eax, ebx
    mov     [rdi + rax], r12b
    inc     ebx
    cmp     ebx, MAPW
    jb      .x
    inc     esi
    cmp     esi, MAPW
    jb      .y
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; xmm2 = x, xmm3 = y (тайлы): круг тела f_rad выталкивается из всех фигур рядом
; (по нормали к ближайшей точке) -> xmm2, xmm3; eax = число касаний
; портит rdx, r8-r11, xmm0, xmm1, xmm4, xmm5
coll_push:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    xor     r12d, r12d
    movss   xmm0, [f_rad]
    movss   xmm1, [f_mapr]
    maxss   xmm2, xmm0
    maxss   xmm3, xmm0
    minss   xmm2, xmm1
    minss   xmm3, xmm1
    ; тайлы, чьи фигуры могут достать (фигуры выходят за тайл не дальше 0.15)
    movss   xmm0, xmm2
    subss   xmm0, [f_creach]
    cvttss2si r8d, xmm0
    movss   xmm0, xmm2
    addss   xmm0, [f_creach]
    cvttss2si r9d, xmm0
    movss   xmm0, xmm3
    subss   xmm0, [f_creach]
    cvttss2si r10d, xmm0
    movss   xmm0, xmm3
    addss   xmm0, [f_creach]
    cvttss2si r11d, xmm0
    mov     eax, MAPW - 1
    cmp     r9d, eax
    cmova   r9d, eax
    cmp     r11d, eax
    cmova   r11d, eax
    mov     r13d, r10d                  ; ty
.ty:
    cmp     r13d, r11d
    jg      .done
    mov     edi, r8d                    ; tx
.tx:
    cmp     edi, r9d
    jg      .tyn
    mov     eax, r13d
    shl     eax, 7
    add     eax, edi
    lea     rdx, [cmap]
    movzx   eax, byte [rdx + rax]
    test    eax, eax
    jz      .txn
    imul    eax, eax, CS_SZ
    lea     rsi, [csets]
    add     rsi, rax
    mov     ebx, [rsi]                  ; фигур в наборе
    add     rsi, 4
.sh:
    ; ближайшая точка прямоугольника к центру круга
    cvtsi2ss xmm4, edi
    movss   xmm0, xmm2
    subss   xmm0, xmm4                  ; lx
    cvtsi2ss xmm5, r13d
    movss   xmm1, xmm3
    subss   xmm1, xmm5                  ; ly
    movss   xmm4, xmm0
    maxss   xmm4, [rsi]
    minss   xmm4, [rsi + 8]
    subss   xmm0, xmm4                  ; dx
    movss   xmm5, xmm1
    maxss   xmm5, [rsi + 4]
    minss   xmm5, [rsi + 12]
    subss   xmm1, xmm5                  ; dy
    movss   xmm4, xmm0
    mulss   xmm4, xmm0
    movss   xmm5, xmm1
    mulss   xmm5, xmm1
    addss   xmm4, xmm5                  ; d²
    movss   xmm5, [f_rad]
    addss   xmm5, [rsi + 16]            ; R = тело + скругление фигуры
    movss   [cp_R], xmm5
    mulss   xmm5, xmm5
    comiss  xmm4, xmm5
    jae     .shn
    inc     r12d
    comiss  xmm4, [f_1em6]
    jb      .inside
    sqrtss  xmm4, xmm4
    movss   xmm5, [cp_R]
    addss   xmm5, [f_0_001]             ; с запасом — следующая проверка чистая
    subss   xmm5, xmm4
    divss   xmm5, xmm4
    mulss   xmm0, xmm5
    mulss   xmm1, xmm5
    addss   xmm2, xmm0
    addss   xmm3, xmm1
    jmp     .shn
.inside:
    ; центр внутри прямоугольника: наружу через ближайшую сторону
    cvtsi2ss xmm4, edi
    movss   xmm0, xmm2
    subss   xmm0, xmm4                  ; lx
    cvtsi2ss xmm5, r13d
    movss   xmm1, xmm3
    subss   xmm1, xmm5                  ; ly
    movss   xmm4, xmm0
    subss   xmm4, [rsi]
    xor     eax, eax
    movss   xmm5, [rsi + 8]
    subss   xmm5, xmm0
    comiss  xmm5, xmm4
    jae     .i1
    movss   xmm4, xmm5
    mov     eax, 1
.i1:
    movss   xmm5, xmm1
    subss   xmm5, [rsi + 4]
    comiss  xmm5, xmm4
    jae     .i2
    movss   xmm4, xmm5
    mov     eax, 2
.i2:
    movss   xmm5, [rsi + 12]
    subss   xmm5, xmm1
    comiss  xmm5, xmm4
    jae     .i3
    movss   xmm4, xmm5
    mov     eax, 3
.i3:
    addss   xmm4, [cp_R]
    addss   xmm4, [f_0_001]
    cmp     eax, 1
    jb      .ixm
    je      .ixp
    cmp     eax, 2
    je      .iym
    addss   xmm3, xmm4
    jmp     .shn
.ixm:
    subss   xmm2, xmm4
    jmp     .shn
.ixp:
    addss   xmm2, xmm4
    jmp     .shn
.iym:
    subss   xmm3, xmm4
.shn:
    add     rsi, 20
    dec     ebx
    jnz     .sh
.txn:
    inc     edi
    jmp     .tx
.tyn:
    inc     r13d
    jmp     .ty
.done:
    movss   xmm0, [f_rad]
    movss   xmm1, [f_mapr]
    maxss   xmm2, xmm0
    maxss   xmm3, xmm0
    minss   xmm2, xmm1
    minss   xmm3, xmm1
    mov     eax, r12d
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; rcx = сущность, xmm0 = dx, xmm1 = dy: шагами не длиннее 0.12 (тонкий забор
; не проскочить), на каждом выталкивание — вдоль стены скользит, столб обходит;
; если зажало так, что не вытолкнуть, — шаг не делается
; портит rax, rdx, r8-r11, xmm0-xmm5
move_ent:
    push    rbx
    push    rsi
    movss   [mv_dx], xmm0
    movss   [mv_dy], xmm1
    andps   xmm0, [f_absmask]
    andps   xmm1, [f_absmask]
    maxss   xmm0, xmm1
    mulss   xmm0, [f_inv_mstep]
    cvttss2si ebx, xmm0
    inc     ebx
    mov     eax, 16
    cmp     ebx, eax
    cmova   ebx, eax
    cvtsi2ss xmm0, ebx
    movss   xmm1, [f_1]
    divss   xmm1, xmm0
    movss   xmm0, [mv_dx]
    mulss   xmm0, xmm1
    movss   [mv_dx], xmm0
    movss   xmm0, [mv_dy]
    mulss   xmm0, xmm1
    movss   [mv_dy], xmm0
.step:
    movss   xmm2, [rcx + E_X]
    addss   xmm2, [mv_dx]
    movss   xmm3, [rcx + E_Y]
    addss   xmm3, [mv_dy]
    mov     esi, 4
.res:
    call    coll_push
    test    eax, eax
    jz      .ok
    dec     esi
    jnz     .res
    jmp     .next
.ok:
    movss   [rcx + E_X], xmm2
    movss   [rcx + E_Y], xmm3
.next:
    dec     ebx
    jnz     .step
    pop     rsi
    pop     rbx
    ret

; xmm0, xmm1 = откуда, xmm2, xmm3 = куда -> eax = 1, если между ними нет стены
; (сруб, окно, печь); низкий забор, столбы и деревья удар не держат
; портит rdx, r8, r9, xmm2-xmm5
melee_clear:
    subss   xmm2, xmm0
    subss   xmm3, xmm1
    mulss   xmm2, [f_0_125]
    mulss   xmm3, [f_0_125]
    movss   xmm4, xmm0
    movss   xmm5, xmm1
    lea     r8, [cmap]
    mov     r9d, 7
.l:
    addss   xmm4, xmm2
    addss   xmm5, xmm3
    cvttss2si eax, xmm4
    cvttss2si edx, xmm5
    cmp     eax, MAPW
    jae     .n
    cmp     edx, MAPW
    jae     .n
    shl     edx, 7
    add     edx, eax
    cmp     byte [r8 + rdx], CS_TALL
    je      .blocked
.n:
    dec     r9d
    jnz     .l
    mov     eax, 1
    ret
.blocked:
    xor     eax, eax
    ret

; курсор на фигуре живого видимого зомби -> eax = 1, xmm2, xmm1 = направление
; на его ноги (мир ×32, как у прицела по курсору); из нескольких — ближний к глазу
aim_pick:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40
    xor     edi, edi
    mov     r12d, 0x80000000
    lea     rbx, [ents]
    mov     esi, MAXE
.l:
    mov     eax, [rbx + E_ST]
    cmp     eax, ST_WANDER
    jb      .n
    cmp     eax, ST_CORPSE
    jae     .n
    movss   xmm0, [rbx + E_X]
    mulss   xmm0, [f_256]
    cvtss2si ecx, xmm0
    movss   xmm0, [rbx + E_Y]
    mulss   xmm0, [f_256]
    cvtss2si edx, xmm0
    mov     eax, ecx
    sub     eax, [camx]
    mov     r9d, edx
    sub     r9d, [camy]
    mov     r10d, eax
    sub     r10d, r9d
    sar     r10d, 4
    add     r10d, CX0                   ; sx
    add     eax, r9d
    sar     eax, 5
    add     eax, CY0                    ; sy
    ; фигура: 12 px в ширину, от макушки (27 px) до ног
    mov     r9d, [mx]
    sub     r9d, r10d
    add     r9d, 7
    cmp     r9d, 14
    jae     .n
    mov     r9d, [my]
    sub     r9d, eax
    add     r9d, 29
    cmp     r9d, 32
    jae     .n
    call    vis_at
    cmp     eax, 60                     ; как в draw_objects: невидимых не рисуем
    jb      .n
    add     ecx, edx
    cmp     ecx, r12d
    jle     .n
    mov     r12d, ecx
    mov     rdi, rbx
.n:
    add     rbx, ESZ
    dec     esi
    jnz     .l
    xor     eax, eax
    test    rdi, rdi
    jz      .out
    movss   xmm2, [rdi + E_X]
    subss   xmm2, [pl + E_X]
    mulss   xmm2, [f_32]
    movss   xmm1, [rdi + E_Y]
    subss   xmm1, [pl + E_Y]
    mulss   xmm1, [f_32]
    mov     eax, 1
.out:
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; след лопаты: серп досягаемости удара, пока идёт взмах — снежная пыль по дуге
; (осветляет уже освещённый кадр, прячется за тем, что ближе к глазу)
draw_swing:
    movss   xmm0, [pl_swing]
    comiss  xmm0, [f_0]
    jbe     .ret
    cmp     dword [gstate], 0
    jne     .ret
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    ; сколько дуги уже пройдено: n = (1 − swing/0.22)·SW_N + 1
    divss   xmm0, [f_0_22s]
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    mulss   xmm1, [f_swn]
    cvttss2si r15d, xmm1
    inc     r15d
    mov     eax, SW_N
    cmp     r15d, eax
    cmova   r15d, eax
    ; начальный луч: взгляд, повёрнутый на −θ
    movss   xmm0, [pl + E_FX]
    movss   xmm1, [pl + E_FY]
    movss   xmm2, xmm0
    mulss   xmm2, [f_sw_cos]
    movss   xmm3, xmm1
    mulss   xmm3, [f_sw_sin]
    addss   xmm2, xmm3                  ; vx = fx cos θ + fy sin θ
    movss   xmm3, xmm1
    mulss   xmm3, [f_sw_cos]
    movss   xmm4, xmm0
    mulss   xmm4, [f_sw_sin]
    subss   xmm3, xmm4                  ; vy = fy cos θ − fx sin θ
    movss   [sw_vx], xmm2
    movss   [sw_vy], xmm3
    mov     eax, [plw_x]
    add     eax, [plw_y]
    mov     [sw_s0], eax
    pxor    xmm7, xmm7
    movdqa  xmm6, [sw_add]
    xor     r14d, r14d                  ; k
.k:
    cmp     r14d, r15d
    jge     .done
    ; яркость хвоста: 0 у начала, 128 у переднего края
    mov     eax, r14d
    sub     eax, r15d
    add     eax, SW_TAIL + 1
    jle     .kn
    shl     eax, 7
    xor     edx, edx
    mov     ecx, SW_TAIL
    div     ecx
    mov     r13d, eax                   ; 0..128
    ; радиусы от кромки внутрь, слабее к телу
    xor     r12d, r12d
.r:
    cmp     r12d, 5
    jge     .kn
    cvtsi2ss xmm0, r12d
    mulss   xmm0, [f_0_07]
    movss   xmm1, [f_sw_r]
    subss   xmm1, xmm0                  ; r
    movss   xmm2, [sw_vx]
    mulss   xmm2, xmm1
    movss   xmm3, [sw_vy]
    mulss   xmm3, xmm1
    mulss   xmm2, [f_256]
    mulss   xmm3, [f_256]
    cvtss2si ecx, xmm2                  ; dx (8.8)
    cvtss2si edx, xmm3                  ; dy
    lea     eax, [ecx + edx]
    add     eax, [sw_s0]
    mov     ebp, eax                    ; глубина точки
    mov     eax, ecx
    sub     eax, edx
    sar     eax, 4
    add     eax, [psx]
    mov     esi, eax                    ; sx
    lea     eax, [ecx + edx]
    sar     eax, 5
    add     eax, [psy]
    sub     eax, 10                     ; на высоте пояса
    cmp     eax, SCR_H
    jae     .rn
    imul    edi, eax, SCR_W
    ; сила: край ярче, внутрь слабее
    mov     ebx, 5
    sub     ebx, r12d
    imul    ebx, r13d
    imul    ebx, ebx, 21
    shr     ebx, 7                      ; 0..105
    xor     r8d, r8d
.px:
    lea     eax, [esi + r8d]
    cmp     eax, SCR_W
    jae     .pxn
    add     eax, edi
    lea     rdx, [gposb]
    mov     ecx, [rdx + rax*4]
    movzx   edx, cx
    shr     ecx, 16
    add     ecx, edx
    lea     edx, [ebp + 96]
    cmp     ecx, edx
    jg      .pxn
    mov     rdx, [fbbits]
    movd    xmm0, [rdx + rax*4]
    punpcklbw xmm0, xmm7
    movdqa  xmm1, xmm0
    psrlw   xmm1, 1
    paddw   xmm1, xmm0
    paddw   xmm1, xmm6                  ; цель: пиксель ×1.5 + иней
    pminsw  xmm1, [w255]
    psubw   xmm1, xmm0
    movd    xmm2, ebx
    pshuflw xmm2, xmm2, 0
    pmullw  xmm1, xmm2
    psraw   xmm1, 7
    paddw   xmm0, xmm1
    packuswb xmm0, xmm0
    movd    [rdx + rax*4], xmm0
.pxn:
    inc     r8d
    cmp     r8d, 2
    jb      .px
.rn:
    inc     r12d
    jmp     .r
.kn:
    ; следующий луч: поворот на шаг дуги
    movss   xmm0, [sw_vx]
    movss   xmm1, [sw_vy]
    movss   xmm2, xmm0
    mulss   xmm2, [f_sw_dc]
    movss   xmm3, xmm1
    mulss   xmm3, [f_sw_ds]
    subss   xmm2, xmm3
    movss   xmm3, xmm0
    mulss   xmm3, [f_sw_ds]
    movss   xmm4, xmm1
    mulss   xmm4, [f_sw_dc]
    addss   xmm3, xmm4
    movss   [sw_vx], xmm2
    movss   [sw_vy], xmm3
    inc     r14d
    jmp     .k
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
.ret:
    ret

; ---------------------------------------------------------------- F3: хитбоксы
; поверх готового кадра: фигуры тайлов (стены тускло, мелочь ярко), тела,
; сектор лопаты — ровно то, чем считаются столкновения и удары
draw_hitboxes:
    cmp     dword [dbg_hit], 0
    jne     .on
    ret
.on:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    mov     esi, [camy]
    sar     esi, 8
    lea     ebp, [esi + 22]
    sub     esi, 22                     ; ty
.ty:
    cmp     esi, ebp
    jg      .bodies
    cmp     esi, MAPW
    jae     .tyn
    mov     edi, [camx]
    sar     edi, 8
    lea     r15d, [edi + 22]
    sub     edi, 22                     ; tx
.tx:
    cmp     edi, r15d
    jg      .tyn
    cmp     edi, MAPW
    jae     .txn
    mov     eax, esi
    shl     eax, 7
    add     eax, edi
    lea     rdx, [cmap]
    movzx   eax, byte [rdx + rax]
    test    eax, eax
    jz      .txn
    mov     r13d, 0x00FFE040            ; мелочь и заборы
    cmp     eax, CS_LOW
    ja      .col
    mov     r13d, 0x00906040            ; срубы, печи, столы — тускло
.col:
    imul    eax, eax, CS_SZ
    lea     r12, [csets]
    add     r12, rax
    mov     ebx, [r12]
    add     r12, 4
    cvtsi2ss xmm6, edi
    cvtsi2ss xmm7, esi
.sh:
    call    hb_outline
    add     r12, 20
    dec     ebx
    jnz     .sh
.txn:
    inc     edi
    jmp     .tx
.tyn:
    inc     esi
    jmp     .ty
.bodies:
    ; тела: игрок голубой, живые зомби красные
    xorps   xmm6, xmm6
    xorps   xmm7, xmm7
    lea     r12, [hb_shp]
    mov     eax, [f_rad]
    mov     [r12 + 16], eax
    lea     rbx, [pl]
    mov     r13d, 0x0040E0FF
    call    .body
    lea     rbx, [ents]
    mov     esi, MAXE
    mov     r13d, 0x00FF4848
.z:
    mov     eax, [rbx + E_ST]
    cmp     eax, ST_WANDER
    jb      .zn
    cmp     eax, ST_CORPSE
    jae     .zn
    movss   xmm0, [rbx + E_X]           ; только видимых — невидимых не выдаём
    mulss   xmm0, [f_256]
    cvtss2si ecx, xmm0
    movss   xmm0, [rbx + E_Y]
    mulss   xmm0, [f_256]
    cvtss2si edx, xmm0
    call    vis_at
    cmp     eax, 60
    jb      .zn
    ; цвет по состоянию: бредёт — тёмно-красный, идёт на шум/запах — жёлтый, гонится — красный
    mov     r13d, 0x00B04040
    mov     eax, [rbx + E_ST]
    cmp     eax, ST_INVEST
    jne     .zc
    mov     r13d, 0x00FFD040
.zc:
    cmp     eax, ST_CHASE
    jne     .zd
    mov     r13d, 0x00FF3030
.zd:
    call    .body
.zn:
    add     rbx, ESZ
    dec     esi
    jnz     .z
    ; сектор лопаты: края и дуга радиусом f_sw_r
    cmp     dword [gstate], 0
    jne     .out
    movss   xmm0, [pl + E_FX]
    movss   xmm1, [pl + E_FY]
    movss   xmm2, xmm0
    mulss   xmm2, [f_sw_cos]
    movss   xmm3, xmm1
    mulss   xmm3, [f_sw_sin]
    addss   xmm2, xmm3
    movss   xmm3, xmm1
    mulss   xmm3, [f_sw_cos]
    movss   xmm4, xmm0
    mulss   xmm4, [f_sw_sin]
    subss   xmm3, xmm4
    movss   [sw_vx], xmm2
    movss   [sw_vy], xmm3
    xor     r14d, r14d
.arc:
    movss   xmm2, [sw_vx]
    mulss   xmm2, [f_sw_r]
    addss   xmm2, [pl + E_X]
    movss   xmm3, [sw_vy]
    mulss   xmm3, [f_sw_r]
    addss   xmm3, [pl + E_Y]
    call    hb_proj
    mov     r12d, ecx
    mov     r13d, edx
    test    r14d, r14d
    jz      .edge
    cmp     r14d, SW_N
    jne     .noedge
.edge:
    mov     r8d, [psx]
    mov     r9d, [psy]
    mov     r10d, 0x00E8E8E8
    call    hb_line
    mov     ecx, r12d
    mov     edx, r13d
    test    r14d, r14d
    jz      .arcn
.noedge:
    mov     r8d, [hb_px]
    mov     r9d, [hb_py]
    mov     r10d, 0x00E8E8E8
    call    hb_line
.arcn:
    mov     [hb_px], r12d
    mov     [hb_py], r13d
    movss   xmm0, [sw_vx]
    movss   xmm1, [sw_vy]
    movss   xmm2, xmm0
    mulss   xmm2, [f_sw_dc]
    movss   xmm3, xmm1
    mulss   xmm3, [f_sw_ds]
    subss   xmm2, xmm3
    movss   xmm3, xmm0
    mulss   xmm3, [f_sw_ds]
    movss   xmm4, xmm1
    mulss   xmm4, [f_sw_dc]
    addss   xmm3, xmm4
    movss   [sw_vx], xmm2
    movss   [sw_vy], xmm3
    inc     r14d
    cmp     r14d, SW_N
    jbe     .arc
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
; rbx = сущность: круг тела в hb_shp
.body:
    mov     eax, [rbx + E_X]
    mov     [r12], eax
    mov     [r12 + 8], eax
    mov     eax, [rbx + E_Y]
    mov     [r12 + 4], eax
    mov     [r12 + 12], eax
    jmp     hb_outline

; r12 -> фигура (x0, y0, x1, y1, r), xmm6, xmm7 = её начало в мире, r13d = цвет;
; контур по опорным точкам: угол прямоугольника по знаку направления + r·d
; портит rax, rcx, rdx, r8-r11, xmm0-xmm3
hb_outline:
    push    r14
    xor     r14d, r14d
.k:
    mov     eax, r14d
    and     eax, 31
    lea     rdx, [hb_dirs]
    movss   xmm0, [rdx + rax*8]
    movss   xmm1, [rdx + rax*8 + 4]
    movss   xmm2, [r12]
    comiss  xmm0, [f_0]
    jb      .xl
    movss   xmm2, [r12 + 8]
.xl:
    movss   xmm3, [r12 + 4]
    comiss  xmm1, [f_0]
    jb      .yl
    movss   xmm3, [r12 + 12]
.yl:
    mulss   xmm0, [r12 + 16]
    mulss   xmm1, [r12 + 16]
    addss   xmm2, xmm0
    addss   xmm3, xmm1
    addss   xmm2, xmm6
    addss   xmm3, xmm7
    call    hb_proj
    test    r14d, r14d
    jz      .first
    mov     r8d, [hb_px]
    mov     r9d, [hb_py]
    mov     [hb_px], ecx
    mov     [hb_py], edx
    mov     r10d, r13d
    call    hb_line
    jmp     .kn
.first:
    mov     [hb_px], ecx
    mov     [hb_py], edx
.kn:
    inc     r14d
    cmp     r14d, 32
    jbe     .k
    pop     r14
    ret

; xmm2, xmm3 = точка на земле (тайлы) -> ecx, edx = экран; портит eax
hb_proj:
    mulss   xmm2, [f_256]
    mulss   xmm3, [f_256]
    cvtss2si ecx, xmm2
    cvtss2si edx, xmm3
    sub     ecx, [camx]
    sub     edx, [camy]
    mov     eax, ecx
    sub     eax, edx
    sar     eax, 4
    add     eax, CX0
    add     ecx, edx
    sar     ecx, 5
    lea     edx, [ecx + CY0]
    mov     ecx, eax
    ret

; ecx, edx -> r8d, r9d (экран), r10d = цвет: линия в готовый кадр (16.16 DDA)
; портит rax, rcx, rdx, r8, r9, r11
hb_line:
    push    rbx
    push    rsi
    push    rdi
    mov     r11, [fbbits]
    sub     r8d, ecx                    ; dx
    sub     r9d, edx                    ; dy
    mov     esi, r8d
    neg     esi
    cmovl   esi, r8d
    mov     edi, r9d
    neg     edi
    cmovl   edi, r9d
    cmp     esi, edi
    cmovl   esi, edi                    ; шагов
    cmp     esi, 1500
    ja      .out                        ; далеко за экраном
    shl     ecx, 16
    add     ecx, 0x8000
    shl     edx, 16
    add     edx, 0x8000
    mov     ebx, edx
    xor     edi, edi
    test    esi, esi
    jz      .go
    mov     eax, r8d
    shl     eax, 16
    cdq
    idiv    esi
    mov     r8d, eax
    mov     eax, r9d
    shl     eax, 16
    cdq
    idiv    esi
    mov     r9d, eax
.go:
    inc     esi
.px:
    mov     eax, ecx
    sar     eax, 16
    cmp     eax, SCR_W
    jae     .nx
    mov     edx, ebx
    sar     edx, 16
    cmp     edx, SCR_H
    jae     .nx
    imul    edx, edx, SCR_W
    add     edx, eax
    mov     [r11 + rdx*4], r10d
.nx:
    add     ecx, r8d
    add     ebx, r9d
    dec     esi
    jnz     .px
.out:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
