; ----------------------------------------------------------------------------
;  interior.inc — нутро изб: мебель, отделка стен, половики, лут и как его брать
; ----------------------------------------------------------------------------
; Облик нутра — от хеша места и своим ГСЧ (ir_rng): село, зомби и всё, что видно
; снаружи, от него не меняются. Нутро видно только у изб, где крыша снята, тает или
; прорежена (HS_IVIS, считается в начале кадра): доски и половики перекрашивает
; ifloor_pass сразу после прохода пола, стены изнутри, мебель и лут рисуют полосы
; объектов (furn.inc). Мебель — тайлы T_BED..T_LAVKA, её поворот и состояние — в
; hflags (FM_*). Лут — список loot: лежит на виду (на полу, столе, кровати, лавке)
; или спрятан в сундуке и шкафу; E — взять или обыскать, 1..4 — пустить в ход
section .text

; ---------------------------------------------------------------- ГСЧ нутра
ir_rand:
    mov     eax, [ir_rng]
    mov     edx, eax
    shl     edx, 13
    xor     eax, edx
    mov     edx, eax
    shr     edx, 17
    xor     eax, edx
    mov     edx, eax
    shl     edx, 5
    xor     eax, edx
    mov     [ir_rng], eax
    ret

; ecx = n -> eax = 0..n−1 (портит edx)
ir_randn:
    push    rcx
    call    ir_rand
    pop     rcx
    xor     edx, edx
    div     ecx
    mov     eax, edx
    ret

; ecx, edx = тайл -> eax = 1, если это сруб или окно (к нему можно спиной)
ir_wallish:
    call    get_tile
    cmp     eax, T_WALL
    je      .y
    cmp     eax, T_WIN
    je      .y
    cmp     eax, T_WINL
    je      .y
    xor     eax, eax
    ret
.y:
    mov     eax, 1
    ret

; ecx, edx = тайл -> eax = 1, если там пустой пол, и это не вход и не место перед топкой
ir_free:
    call    get_tile
    cmp     eax, T_WOOD
    jne     .n
    cmp     ecx, [iz_ex]
    jne     .s
    cmp     edx, [iz_ey]
    je      .n
.s:
    cmp     ecx, [fr_sx]
    jne     .y
    cmp     edx, [fr_sy]
    je      .n
.y:
    mov     eax, 1
    ret
.n:
    xor     eax, eax
    ret

; ecx, edx = тайл, al = флаги -> hflags
ir_setfl:
    push    rdx
    shl     edx, 7
    add     edx, ecx
    lea     r11, [hflags]
    mov     [r11 + rdx], al
    pop     rdx
    ret

; ecx, edx = тайл, al = убранство стены -> wdeco
ir_setdeco:
    push    rdx
    shl     edx, 7
    add     edx, ecx
    lea     r11, [wdeco]
    mov     [r11 + rdx], al
    pop     rdx
    ret

; ---------------------------------------------------------------- убранство избы
; только что заведённая изба (make_lot: после add_house): H_x, H_y, H_w, H_h,
; H_lit, H_tv, печь — в записи дома
furnish:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    ; запись дома — эта изба (add_house мог не завести: мест нет)
    mov     eax, [nhouses]
    test    eax, eax
    jz      .out
    dec     eax
    imul    eax, eax, HS_SZ
    lea     rbx, [houses]
    mov     ecx, [H_x]
    cmp     [rbx + rax + HS_X0], ecx
    jne     .out
    mov     ecx, [H_y]
    cmp     [rbx + rax + HS_Y0], ecx
    jne     .out
    mov     eax, [H_x]
    imul    eax, eax, 31
    add     eax, 7
    mov     ecx, [H_y]
    imul    ecx, ecx, 17
    add     ecx, 11
    HASH    eax, ecx
    or      eax, 1
    mov     [ir_rng], eax
    mov     eax, [nhouses]
    mov     [fr_hid], eax               ; дом + 1
    dec     eax
    imul    eax, eax, HS_SZ
    lea     rbx, [houses]
    add     rbx, rax                    ; rbx = запись дома (во всех ir_*)
    mov     eax, [H_x]
    inc     eax
    mov     [fr_x0], eax
    mov     eax, [H_y]
    inc     eax
    mov     [fr_y0], eax
    mov     eax, [H_w]
    sub     eax, 2
    mov     [fr_w], eax
    mov     eax, [H_h]
    sub     eax, 2
    mov     [fr_h], eax
    mov     eax, [H_tv]
    add     eax, eax
    or      eax, [H_lit]
    mov     [fr_base], eax
    ; перед топкой (дверца — на грани +y печи) не ставим
    mov     eax, [rbx + HS_SX]
    mov     [fr_sx], eax
    mov     eax, [rbx + HS_SY]
    inc     eax
    mov     [fr_sy], eax
    call    ir_door
    call    ir_style
    call    ir_place
    call    ir_walls
    call    ir_rugs
    call    ir_loot
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

; дверь — где-то в срубе; вход — клетка комнаты рядом с ней (iz_ex, iz_ey), внутрь —
; (fr_ix, fr_iy)
ir_door:
    push    rsi
    push    rdi
    sub     rsp, 40
    mov     edi, [H_y]
.y:
    mov     eax, [H_y]
    add     eax, [H_h]
    cmp     edi, eax
    jge     .none
    mov     esi, [H_x]
.x:
    mov     eax, [H_x]
    add     eax, [H_w]
    cmp     esi, eax
    jge     .ny
    mov     ecx, esi
    mov     edx, edi
    call    get_tile
    cmp     eax, T_DOOR
    je      .got
    inc     esi
    jmp     .x
.ny:
    inc     edi
    jmp     .y
.got:
    ; вход: дверь, прижатая внутрь комнаты
    mov     eax, esi
    mov     ecx, [fr_x0]
    cmp     eax, ecx
    cmovl   eax, ecx
    add     ecx, [fr_w]
    dec     ecx
    cmp     eax, ecx
    cmovg   eax, ecx
    mov     [iz_ex], eax
    sub     eax, esi
    mov     [fr_ix], eax
    mov     eax, edi
    mov     ecx, [fr_y0]
    cmp     eax, ecx
    cmovl   eax, ecx
    add     ecx, [fr_h]
    dec     ecx
    cmp     eax, ecx
    cmovg   eax, ecx
    mov     [iz_ey], eax
    sub     eax, edi
    mov     [fr_iy], eax
    jmp     .out
.none:
    mov     eax, [fr_x0]
    mov     [iz_ex], eax
    mov     eax, [fr_y0]
    mov     [iz_ey], eax
    mov     dword [fr_ix], 1
    mov     dword [fr_iy], 0
.out:
    add     rsp, 40
    pop     rdi
    pop     rsi
    ret

; стены, шторы, ковёр, скатерть, одеяло, дерево мебели, краска сундука, что на столе,
; краска пола (fmap на всю комнату)
ir_style:
    push    rsi
    push    rdi
    sub     rsp, 40
    ; стены: брёвна 4/16, побелка с панелью 4/16, обои 6/16, вагонка 2/16
    call    ir_rand
    and     eax, 15
    xor     esi, esi
    cmp     eax, 4
    jb      .ws
    inc     esi
    cmp     eax, 8
    jb      .ws
    inc     esi
    cmp     eax, 14
    jb      .ws
    inc     esi
.ws:
    mov     [rbx + HS_WALL], esi
    cmp     esi, 1
    jne     .w2
    mov     ecx, 4
    call    ir_randn
    lea     rdx, [ir_panel]
    mov     eax, [rdx + rax*4]
    mov     [rbx + HS_WCOL], eax
    jmp     .w9
.w2:
    cmp     esi, 2
    jne     .w3
    mov     ecx, 6
    call    ir_randn
    lea     rdx, [ir_paper]
    mov     ecx, [rdx + rax*4]
    mov     [rbx + HS_WCOL], ecx
    lea     rdx, [ir_motif]
    mov     ecx, [rdx + rax*4]
    mov     [rbx + HS_WMOT], ecx
    jmp     .w9
.w3:
    mov     ecx, 4
    call    ir_randn
    lea     rdx, [ir_board]
    mov     eax, [rdx + rax*4]
    mov     [rbx + HS_WCOL], eax
.w9:
    mov     ecx, 6
    call    ir_randn
    lea     rdx, [ir_curt]
    mov     eax, [rdx + rax*4]
    mov     [rbx + HS_CURT], eax
    call    ir_rand
    mov     esi, eax
    mov     ecx, esi
    and     ecx, 3
    mov     [rbx + HS_CARP], ecx
    mov     ecx, esi
    shr     ecx, 2
    and     ecx, 3
    mov     [rbx + HS_CLOTH], ecx
    mov     ecx, esi
    shr     ecx, 4
    and     ecx, 3
    mov     [rbx + HS_BLNK], ecx
    mov     ecx, esi
    shr     ecx, 6
    and     ecx, 3
    lea     rdx, [ir_wood]
    mov     ecx, [rdx + rcx*4]
    mov     [rbx + HS_WOOD], ecx
    mov     ecx, esi
    shr     ecx, 8
    and     ecx, 3
    lea     rdx, [ir_chest]
    mov     ecx, [rdx + rcx*4]
    mov     [rbx + HS_CHPAL], ecx
    ; на столе без телевизора: самовар 1/2, кринка с геранью 1/4, пусто 1/4
    mov     ecx, esi
    shr     ecx, 10
    and     ecx, 3
    lea     rdx, [ir_ontab]
    movzx   ecx, byte [rdx + rcx]
    mov     [rbx + HS_ONTAB], ecx
    ; пол: некрашеный 1/4, сурик, охра, серый
    shr     esi, 12
    and     esi, 3
    mov     [fr_fs], esi
    mov     edi, [fr_y0]
.fy:
    mov     eax, [fr_y0]
    add     eax, [fr_h]
    cmp     edi, eax
    jge     .out
    mov     ecx, [fr_x0]
.fx:
    mov     eax, [fr_x0]
    add     eax, [fr_w]
    cmp     ecx, eax
    jge     .fny
    mov     eax, edi
    shl     eax, 7
    add     eax, ecx
    lea     rdx, [fmap]
    mov     [rdx + rax*2], si
    inc     ecx
    jmp     .fx
.fny:
    inc     edi
    jmp     .fy
.out:
    add     rsp, 40
    pop     rdi
    pop     rsi
    ret

; кровать (7/8), шкаф или буфет спиной к глухой задней стене (3/5), сундук (2/3),
; лавка (1/2) — пока пол связан со входом
ir_place:
    sub     rsp, 40
    mov     dword [fr_bed], 0
    call    ir_rand
    test    eax, 7
    jz      .nb
    call    ir_bed
.nb:
    mov     ecx, 5
    call    ir_randn
    cmp     eax, 3
    jae     .nc
    call    ir_rand
    and     eax, FM_ALT
    mov     r9d, eax
    mov     ecx, T_CUPB
    mov     edx, 3                      ; −y, −x: видно лицом
    mov     r8d, 1
    call    ir_put1
.nc:
    mov     ecx, 3
    call    ir_randn
    test    eax, eax
    jz      .nh
    mov     ecx, T_CHEST
    mov     edx, 15
    xor     r8d, r8d
    xor     r9d, r9d
    call    ir_put1
.nh:
    call    ir_rand
    test    eax, 1
    jz      .nl
    mov     ecx, T_LAVKA
    mov     edx, 15
    xor     r8d, r8d
    xor     r9d, r9d
    call    ir_put1
.nl:
    add     rsp, 40
    ret

; ecx = вид, edx = маска сторон (бит s: 1 −y, 2 −x, 4 +y, 8 +x), r8d = 1 — спиной только
; к глухому срубу, r9d = свои флаги -> eax = 1, если встал
ir_put1:
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    rbp
    sub     rsp, 40
    mov     [fr_kind], ecx
    mov     [fr_mask], edx
    mov     [fr_solid], r8d
    mov     [fr_vfl], r9d
    mov     r12d, 24
.try:
    mov     ecx, [fr_w]
    call    ir_randn
    add     eax, [fr_x0]
    mov     esi, eax
    mov     ecx, [fr_h]
    call    ir_randn
    add     eax, [fr_y0]
    mov     edi, eax
    mov     ecx, esi
    mov     edx, edi
    call    ir_free
    test    eax, eax
    jz      .next
    mov     ecx, 4
    call    ir_randn
    mov     r13d, eax
    xor     r14d, r14d
.side:
    lea     ebp, [r13d + r14d]
    and     ebp, 3
    mov     eax, [fr_mask]
    bt      eax, ebp
    jnc     .sn
    lea     rdx, [ir_sdx]
    mov     ecx, esi
    add     ecx, [rdx + rbp*4]
    lea     rdx, [ir_sdy]
    mov     edx, [rdx + rbp*4]
    add     edx, edi
    call    get_tile
    cmp     eax, T_WALL
    je      .ok
    cmp     dword [fr_solid], 0
    jne     .sn
    cmp     eax, T_WIN
    je      .ok
    cmp     eax, T_WINL
    je      .ok
.sn:
    inc     r14d
    cmp     r14d, 4
    jb      .side
    jmp     .next
.ok:
    mov     ecx, esi
    mov     edx, edi
    mov     eax, [fr_kind]
    call    set_tile
    lea     eax, [rbp*4]
    or      eax, [fr_base]
    or      eax, [fr_vfl]
    call    ir_setfl
    call    ir_conn
    test    eax, eax
    jnz     .done
    mov     ecx, esi
    mov     edx, edi
    mov     eax, T_WOOD
    call    set_tile
    mov     eax, [fr_base]
    call    ir_setfl
.next:
    dec     r12d
    jnz     .try
    xor     eax, eax
    jmp     .out
.done:
    mov     eax, 1
.out:
    add     rsp, 40
    pop     rbp
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    ret

; кровать: два тайла вдоль стены, изголовье — к углу (fr_bed, iz_bs, fr_b0*, fr_b1*)
ir_bed:
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 32
    mov     r12d, 30
.try:
    mov     ecx, [fr_w]
    call    ir_randn
    add     eax, [fr_x0]
    mov     esi, eax
    mov     ecx, [fr_h]
    call    ir_randn
    add     eax, [fr_y0]
    mov     edi, eax
    mov     ecx, esi
    mov     edx, edi
    call    ir_free
    test    eax, eax
    jz      .next
    mov     ecx, 4
    call    ir_randn
    mov     r13d, eax
    xor     r14d, r14d
.side:
    lea     ebp, [r13d + r14d]
    and     ebp, 3                      ; s — стена за спиной
    lea     rdx, [ir_sdx]
    mov     ecx, esi
    add     ecx, [rdx + rbp*4]
    lea     rdx, [ir_sdy]
    mov     edx, [rdx + rbp*4]
    add     edx, edi
    call    ir_wallish
    test    eax, eax
    jz      .sn
    ; вдоль стены: s чётная (−y, +y) — по x, нечётная — по y; пара — в сторону sg
    call    ir_rand
    mov     r15d, 1
    test    eax, 1
    jz      .sg
    neg     r15d
.sg:
    mov     dword [fr_k], 2
.pair:
    call    .along                      ; ecx, edx = пара
    mov     [fr_qx], ecx
    mov     [fr_qy], edx
    call    ir_free
    test    eax, eax
    jz      .flip
    lea     rax, [ir_sdx]
    add     ecx, [rax + rbp*4]
    lea     rax, [ir_sdy]
    add     edx, [rax + rbp*4]
    call    ir_wallish
    test    eax, eax
    jnz     .put
.flip:
    neg     r15d
    dec     dword [fr_k]
    jnz     .pair
.sn:
    inc     r14d
    cmp     r14d, 4
    jb      .side
.next:
    dec     r12d
    jnz     .try
    jmp     .out
.put:
    ; изголовье: у того конца, за которым стена (угол)
    mov     dword [fr_hd], 0            ; 0 — у (esi, edi), 1 — у пары
    neg     r15d
    call    .along                      ; клетка за (esi, edi) от пары
    neg     r15d
    call    ir_wallish
    test    eax, eax
    jnz     .hset
    mov     ecx, [fr_qx]
    mov     edx, [fr_qy]
    mov     eax, ebp
    and     eax, 1
    jnz     .hy
    add     ecx, r15d
    jmp     .hc
.hy:
    add     edx, r15d
.hc:
    call    ir_wallish
    test    eax, eax
    jz      .hset
    mov     dword [fr_hd], 1
.hset:
    xor     r13d, r13d
    call    ir_rand
    test    eax, 3
    jnz     .wood
    mov     r13d, FM_IRON
.wood:
    lea     eax, [rbp*4]
    or      eax, [fr_base]
    or      r13d, eax                   ; общие флаги
    ; пара к +u: sg совпадает с ir_usg[s]
    lea     rax, [ir_usg]
    mov     r14d, [rax + rbp*4]
    mov     eax, r13d
    cmp     dword [fr_hd], 0
    jne     .a1
    or      eax, FM_HEAD
.a1:
    cmp     r15d, r14d
    jne     .a2
    or      eax, FM_ALT
.a2:
    mov     ecx, esi
    mov     edx, edi
    call    ir_setfl
    mov     eax, T_BED
    call    set_tile
    mov     eax, r13d
    cmp     dword [fr_hd], 0
    je      .b1
    or      eax, FM_HEAD
.b1:
    neg     r14d
    cmp     r15d, r14d
    jne     .b2
    or      eax, FM_ALT
.b2:
    mov     ecx, [fr_qx]
    mov     edx, [fr_qy]
    call    ir_setfl
    mov     eax, T_BED
    call    set_tile
    call    ir_conn
    test    eax, eax
    jnz     .ok
    mov     ecx, esi
    mov     edx, edi
    mov     eax, T_WOOD
    call    set_tile
    mov     eax, [fr_base]
    call    ir_setfl
    mov     ecx, [fr_qx]
    mov     edx, [fr_qy]
    mov     eax, T_WOOD
    call    set_tile
    mov     eax, [fr_base]
    call    ir_setfl
    jmp     .next
.ok:
    mov     dword [fr_bed], 1
    mov     [iz_bs], ebp
    mov     [fr_b0x], esi
    mov     [fr_b0y], edi
    mov     eax, [fr_qx]
    mov     [fr_b1x], eax
    mov     eax, [fr_qy]
    mov     [fr_b1y], eax
.out:
    add     rsp, 32
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    ret
; (esi, edi) + sg·(вдоль стены s) -> ecx, edx
.along:
    mov     ecx, esi
    mov     edx, edi
    mov     eax, ebp
    and     eax, 1
    jnz     .ay
    add     ecx, r15d
    ret
.ay:
    add     edx, r15d
    ret

; eax = 1 — весь пол комнаты связан со входом, и к каждому сундуку и шкафу есть подход
ir_conn:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    sub     rsp, 40
    lea     rdi, [fr_vis]
    xor     eax, eax
    mov     ecx, 64
    rep     stosb
    mov     ecx, [iz_ex]
    mov     edx, [iz_ey]
    call    get_tile
    cmp     eax, T_WOOD
    jne     .no
    ; клетка: (x − x0) + (y − y0)·8
    mov     eax, [iz_ex]
    sub     eax, [fr_x0]
    mov     ecx, [iz_ey]
    sub     ecx, [fr_y0]
    lea     eax, [rax + rcx*8]
    lea     rsi, [fr_vis]
    mov     byte [rsi + rax], 1
    lea     rdi, [fr_q]
    mov     [rdi], al
    xor     r12d, r12d                  ; голова очереди
    mov     r13d, 1                     ; хвост
.bfs:
    cmp     r12d, r13d
    jae     .count
    movzx   ebx, byte [rdi + r12]
    inc     r12d
    xor     r14d, r14d
.nb:
    mov     r9d, ebx
    and     r9d, 7
    lea     rdx, [ir_sdx]
    add     r9d, [rdx + r14*4]
    cmp     r9d, [fr_w]
    jae     .nn
    mov     r10d, ebx
    shr     r10d, 3
    lea     rdx, [ir_sdy]
    add     r10d, [rdx + r14*4]
    cmp     r10d, [fr_h]
    jae     .nn
    lea     r8d, [r9 + r10*8]
    cmp     byte [rsi + r8], 0
    jne     .nn
    mov     ecx, r9d
    add     ecx, [fr_x0]
    mov     edx, r10d
    add     edx, [fr_y0]
    call    get_tile
    cmp     eax, T_WOOD
    jne     .nn
    mov     byte [rsi + r8], 1
    mov     [rdi + r13], r8b
    inc     r13d
.nn:
    inc     r14d
    cmp     r14d, 4
    jb      .nb
    jmp     .bfs
.count:
    xor     r14d, r14d                  ; всего пола
    xor     r10d, r10d
.cy:
    xor     r9d, r9d
.cx:
    mov     ecx, r9d
    add     ecx, [fr_x0]
    mov     edx, r10d
    add     edx, [fr_y0]
    call    get_tile
    cmp     eax, T_WOOD
    jne     .c1
    inc     r14d
    jmp     .cn
.c1:
    cmp     eax, T_CHEST
    je      .c2
    cmp     eax, T_CUPB
    jne     .cn
.c2:
    ; к тайнику — с посещённой клетки рядом
    xor     ebx, ebx
    xor     r8d, r8d
.c3:
    lea     rdx, [ir_sdx]
    mov     eax, r9d
    add     eax, [rdx + rbx*4]
    cmp     eax, [fr_w]
    jae     .c4
    lea     rdx, [ir_sdy]
    mov     ecx, r10d
    add     ecx, [rdx + rbx*4]
    cmp     ecx, [fr_h]
    jae     .c4
    lea     eax, [rax + rcx*8]
    cmp     byte [rsi + rax], 0
    je      .c4
    mov     r8d, 1
.c4:
    inc     ebx
    cmp     ebx, 4
    jb      .c3
    test    r8d, r8d
    jz      .no
.cn:
    inc     r9d
    cmp     r9d, [fr_w]
    jb      .cx
    inc     r10d
    cmp     r10d, [fr_h]
    jb      .cy
    cmp     r13d, r14d
    jne     .no
    mov     eax, 1
    jmp     .out
.no:
    xor     eax, eax
.out:
    add     rsp, 40
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ecx, edx = тайл задней стены (северной y = H_y или западной x = H_x) -> eax = 1, если
; на нём можно повесить: глухой сруб, пустой, и перед ним не шкаф и не печь
ir_wok:
    push    rcx
    push    rdx
    sub     rsp, 40
    call    get_tile
    cmp     eax, T_WALL
    jne     .no
    mov     eax, edx
    shl     eax, 7
    add     eax, ecx
    lea     r8, [wdeco]
    cmp     byte [r8 + rax], 0
    jne     .no
    cmp     edx, [H_y]
    jne     .w
    inc     edx
    jmp     .f
.w:
    inc     ecx
.f:
    call    get_tile
    cmp     eax, T_CUPB
    je      .no
    cmp     eax, T_STOVE
    je      .no
    cmp     eax, T_STOVEL
    je      .no
    mov     eax, 1
    jmp     .out
.no:
    xor     eax, eax
.out:
    add     rsp, 40
    pop     rdx
    pop     rcx
    ret

; на задних стенах (их видно изнутри): ковёр над кроватью, икона, ходики, фото,
; календарь, зеркало, полка — в wdeco (1, 2 — половины ковра, слева и справа на экране)
ir_walls:
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 40
    ; ковёр: над кроватью у задней стены, а нет — 2/5 где найдётся два глухих рядом
    cmp     dword [fr_bed], 0
    je      .cfree
    mov     eax, [iz_bs]
    cmp     eax, 1
    ja      .cfree
    je      .cw
    ; кровать у северной стены: левая половина — меньший x
    mov     ecx, [fr_b0x]
    mov     edx, [fr_b1x]
    cmp     ecx, edx
    jl      .cn1
    xchg    ecx, edx
.cn1:
    mov     esi, ecx
    mov     edi, edx
    mov     edx, [H_y]
    call    ir_wok
    test    eax, eax
    jz      .cfree
    mov     ecx, edi
    call    ir_wok
    test    eax, eax
    jz      .cfree
    mov     ecx, esi
    mov     al, 1
    call    ir_setdeco
    mov     ecx, edi
    mov     al, 2
    call    ir_setdeco
    jmp     .icon
.cw:
    ; у западной: левая половина на экране — больший y
    mov     ecx, [fr_b0y]
    mov     edx, [fr_b1y]
    cmp     ecx, edx
    jg      .cw1
    xchg    ecx, edx
.cw1:
    mov     esi, ecx
    mov     edi, edx
    mov     ecx, [H_x]
    mov     edx, esi
    call    ir_wok
    test    eax, eax
    jz      .cfree
    mov     edx, edi
    call    ir_wok
    test    eax, eax
    jz      .cfree
    mov     edx, esi
    mov     al, 1
    call    ir_setdeco
    mov     edx, edi
    mov     al, 2
    call    ir_setdeco
    jmp     .icon
.cfree:
    mov     ecx, 5
    call    ir_randn
    cmp     eax, 2
    jae     .icon
    ; северная стена: (x, x + 1)
    mov     esi, [fr_x0]
.cf1:
    lea     eax, [esi + 1]
    mov     ecx, [fr_x0]
    add     ecx, [fr_w]
    cmp     eax, ecx
    jge     .cf2
    mov     ecx, esi
    mov     edx, [H_y]
    call    ir_wok
    test    eax, eax
    jz      .cf1n
    inc     ecx
    call    ir_wok
    test    eax, eax
    jz      .cf1n
    mov     al, 2
    call    ir_setdeco
    dec     ecx
    mov     al, 1
    call    ir_setdeco
    jmp     .icon
.cf1n:
    inc     esi
    jmp     .cf1
.cf2:
    ; западная: (y, y − 1), левая половина — y
    mov     esi, [fr_y0]
    add     esi, [fr_h]
    dec     esi
.cf3:
    cmp     esi, [fr_y0]
    jle     .icon
    mov     ecx, [H_x]
    mov     edx, esi
    call    ir_wok
    test    eax, eax
    jz      .cf3n
    dec     edx
    call    ir_wok
    test    eax, eax
    jz      .cf3n
    mov     al, 2
    call    ir_setdeco
    inc     edx
    mov     al, 1
    call    ir_setdeco
    jmp     .icon
.cf3n:
    dec     esi
    jmp     .cf3
.icon:
    ; остальное — перемешанным запасом: икона первой (в 7/10), потом ходики, фото,
    ; календарь, зеркало, полка
    lea     rdi, [fr_pool]
    mov     dword [rdi], 0x07060503     ; 3 ходики, 5 фото, 6 календарь, 7 зеркало
    mov     byte [rdi + 4], 8           ; 8 полка
    mov     r12d, 4
.sh:
    lea     ecx, [r12d + 1]
    call    ir_randn
    mov     cl, [rdi + rax]
    mov     dl, [rdi + r12]
    mov     [rdi + rax], dl
    mov     [rdi + r12], cl
    dec     r12d
    jnz     .sh
    mov     byte [rdi + 5], 0
    xor     r13d, r13d                  ; взято из запаса
    mov     ecx, 10
    call    ir_randn
    cmp     eax, 7
    jae     .walk
    mov     byte [fr_icon], 1
    jmp     .walk0
.walk:
    mov     byte [fr_icon], 0
.walk0:
    ; обход: северная слева направо, западная справа налево
    mov     esi, [fr_x0]
.wn:
    mov     eax, [fr_x0]
    add     eax, [fr_w]
    cmp     esi, eax
    jge     .ww0
    mov     ecx, esi
    mov     edx, [H_y]
    call    .one
    inc     esi
    jmp     .wn
.ww0:
    mov     esi, [fr_y0]
.ww:
    mov     eax, [fr_y0]
    add     eax, [fr_h]
    cmp     esi, eax
    jge     .out
    mov     ecx, [H_x]
    mov     edx, esi
    call    .one
    inc     esi
    jmp     .ww
.out:
    add     rsp, 40
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    ret
; ecx, edx = тайл стены: икона, если её ещё нет; иначе в 11/20 — следующее из запаса
.one:
    sub     rsp, 8
    call    ir_wok
    test    eax, eax
    jz      .o9
    cmp     byte [fr_icon], 0
    je      .o1
    mov     byte [fr_icon], 0
    mov     al, 4
    call    ir_setdeco
    jmp     .o9
.o1:
    push    rcx
    push    rdx
    mov     ecx, 20
    call    ir_randn
    pop     rdx
    pop     rcx
    cmp     eax, 11
    jae     .o9
    lea     r8, [fr_pool]
    movzx   eax, byte [r8 + r13]
    test    eax, eax
    jz      .o9
    inc     r13d
    call    ir_setdeco
.o9:
    add     rsp, 8
    ret

; половики: дорожка от входа через комнату (3/5), и коврик 2x1 или 2x2 на свободном полу
; (1/2). fmap: байт 0 — краска пола (биты 0-1), половик (2), его края −x, +x, −y, +y
; (3-6), вид (7: 0 — полосатая дорожка, 1 — ковёр с узором); байт 1 — палитра (0-1),
; ось полос (2: 1 — вдоль y), зерно (3-7)
ir_rugs:
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 40
    mov     ecx, 5
    call    ir_randn
    cmp     eax, 3
    jae     .mat
    ; длина: пока пустой пол
    mov     esi, [iz_ex]
    mov     edi, [iz_ey]
    xor     r12d, r12d
.len:
    mov     eax, esi
    sub     eax, [fr_x0]
    cmp     eax, [fr_w]
    jae     .lend
    mov     eax, edi
    sub     eax, [fr_y0]
    cmp     eax, [fr_h]
    jae     .lend
    mov     ecx, esi
    mov     edx, edi
    call    get_tile
    cmp     eax, T_WOOD
    jne     .lend
    inc     r12d
    add     esi, [fr_ix]
    add     edi, [fr_iy]
    jmp     .len
.lend:
    cmp     r12d, 2
    jb      .mat
    call    ir_rand
    mov     r14d, eax
    and     r14d, 0xFB                  ; палитра, зерно; ось — ниже
    xor     r15d, r15d                  ; края вдоль: −y, +y (ось x)
    mov     r15d, 0x60
    cmp     dword [fr_ix], 0
    jne     .ax
    or      r14d, 4                     ; ось y
    mov     r15d, 0x18
.ax:
    mov     esi, [iz_ex]
    mov     edi, [iz_ey]
    xor     r13d, r13d                  ; номер клетки
.put:
    cmp     r13d, r12d
    jae     .mat
    mov     eax, r15d
    ; концы: у входа и у дальнего края — свой край поперёк оси (со стороны, куда смотрит)
    test    r13d, r13d
    jnz     .e1
    call    .endin
.e1:
    lea     ecx, [r13d + 1]
    cmp     ecx, r12d
    jne     .e2
    call    .endout
.e2:
    or      eax, 4
    or      eax, [fr_fs]
    mov     ecx, r14d
    shl     ecx, 8
    or      eax, ecx
    mov     ecx, edi
    shl     ecx, 7
    add     ecx, esi
    lea     rdx, [fmap]
    mov     [rdx + rcx*2], ax
    add     esi, [fr_ix]
    add     edi, [fr_iy]
    inc     r13d
    jmp     .put
.mat:
    call    ir_rand
    test    eax, 1
    jz      .out
    mov     r12d, 12
.mt:
    ; 2x1 вдоль x, 1x2 вдоль y или 2x2
    mov     ecx, 3
    call    ir_randn
    mov     r14d, 2                     ; w
    mov     r15d, 1                     ; h
    cmp     eax, 1
    jb      .ms
    ja      .m22
    mov     r14d, 1
    mov     r15d, 2
    jmp     .ms
.m22:
    mov     r15d, 2
.ms:
    mov     ecx, [fr_w]
    sub     ecx, r14d
    jl      .mn
    inc     ecx
    call    ir_randn
    add     eax, [fr_x0]
    mov     esi, eax
    mov     ecx, [fr_h]
    sub     ecx, r15d
    jl      .mn
    inc     ecx
    call    ir_randn
    add     eax, [fr_y0]
    mov     edi, eax
    ; все клетки — пустой пол без половика
    xor     r13d, r13d
.mc:
    mov     eax, r13d
    and     eax, 1
    cmp     eax, r14d
    jae     .mcn
    mov     ecx, r13d
    shr     ecx, 1
    cmp     ecx, r15d
    jae     .mcn
    add     ecx, edi
    mov     edx, ecx
    lea     ecx, [esi + eax]
    call    get_tile
    cmp     eax, T_WOOD
    jne     .mn
    mov     eax, edx
    shl     eax, 7
    add     eax, ecx
    lea     r8, [fmap]
    test    byte [r8 + rax*2], 4
    jnz     .mn
.mcn:
    inc     r13d
    cmp     r13d, 4
    jb      .mc
    ; кладём: края по контуру
    call    ir_rand
    and     eax, 0xFB
    shl     eax, 8
    or      eax, 0x84
    or      eax, [fr_fs]
    mov     r12d, eax
    xor     r13d, r13d
.mp:
    mov     eax, r13d
    and     eax, 1
    cmp     eax, r14d
    jae     .mpn
    mov     ecx, r13d
    shr     ecx, 1
    cmp     ecx, r15d
    jae     .mpn
    mov     r8d, r12d
    test    eax, eax
    jnz     .m1
    or      r8d, 0x08                   ; −x
.m1:
    lea     r9d, [eax + 1]
    cmp     r9d, r14d
    jne     .m2
    or      r8d, 0x10                   ; +x
.m2:
    test    ecx, ecx
    jnz     .m3
    or      r8d, 0x20                   ; −y
.m3:
    lea     r9d, [ecx + 1]
    cmp     r9d, r15d
    jne     .m4
    or      r8d, 0x40                   ; +y
.m4:
    add     ecx, edi
    shl     ecx, 7
    add     ecx, esi
    add     ecx, eax
    lea     rdx, [fmap]
    mov     [rdx + rcx*2], r8w
.mpn:
    inc     r13d
    cmp     r13d, 4
    jb      .mp
    jmp     .out
.mn:
    dec     r12d
    jnz     .mt
.out:
    add     rsp, 40
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    ret
; край у входа (откуда идём) и дальний: eax |= бит; вход — против (fr_ix, fr_iy)
.endin:
    cmp     dword [fr_ix], 0
    jg      .ei1
    jl      .ei2
    cmp     dword [fr_iy], 0
    jg      .ei3
    or      eax, 0x40                   ; идём к −y: вход с +y
    ret
.ei1:
    or      eax, 0x08                   ; к +x: вход с −x
    ret
.ei2:
    or      eax, 0x10
    ret
.ei3:
    or      eax, 0x20
    ret
.endout:
    cmp     dword [fr_ix], 0
    jg      .eo1
    jl      .eo2
    cmp     dword [fr_iy], 0
    jg      .eo3
    or      eax, 0x20
    ret
.eo1:
    or      eax, 0x10
    ret
.eo2:
    or      eax, 0x08
    ret
.eo3:
    or      eax, 0x40
    ret

; лут: 1–3 вещи (в тёмной, брошенной избе — на одну больше): в сундуке и шкафу, на столе,
; на кровати или лавке, на полу
ir_loot:
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 40
    ; где что в комнате: стол, тайники, кровать или лавка
    mov     dword [fr_tab], -1
    mov     dword [fr_nc], 0
    mov     dword [fr_nbl], 0
    mov     edi, [fr_y0]
.sy:
    mov     eax, [fr_y0]
    add     eax, [fr_h]
    cmp     edi, eax
    jge     .scan
    mov     esi, [fr_x0]
.sx:
    mov     eax, [fr_x0]
    add     eax, [fr_w]
    cmp     esi, eax
    jge     .syn
    mov     ecx, esi
    mov     edx, edi
    call    get_tile
    mov     ecx, edi
    shl     ecx, 7
    add     ecx, esi                    ; тайл
    cmp     eax, T_TABLE
    jne     .s1
    mov     [fr_tab], ecx
.s1:
    cmp     eax, T_CHEST
    je      .s2
    cmp     eax, T_CUPB
    jne     .s3
.s2:
    mov     edx, [fr_nc]
    cmp     edx, 4
    jae     .sxn
    lea     r8, [fr_cont]
    mov     [r8 + rdx*4], ecx
    inc     dword [fr_nc]
    jmp     .sxn
.s3:
    cmp     eax, T_BED
    je      .s4
    cmp     eax, T_LAVKA
    jne     .sxn
.s4:
    mov     edx, [fr_nbl]
    cmp     edx, 4
    jae     .sxn
    lea     r8, [fr_blt]
    mov     [r8 + rdx*4], ecx
    inc     dword [fr_nbl]
.sxn:
    inc     esi
    jmp     .sx
.syn:
    inc     edi
    jmp     .sy
.scan:
    mov     ecx, 3
    call    ir_randn
    lea     r12d, [eax + 1]
    cmp     dword [H_lit], 0
    jne     .n
    inc     r12d
.n:
    xor     r15d, r15d                  ; вещей на столе
.item:
    test    r12d, r12d
    jz      .out
    dec     r12d
    ; вид: батарейки 6/20, бинт 5/20, термос 4/20, тушёнка 5/20
    mov     ecx, 20
    call    ir_randn
    mov     r13d, 1
    cmp     eax, 6
    jb      .tw
    inc     r13d
    cmp     eax, 11
    jb      .tw
    inc     r13d
    cmp     eax, 15
    jb      .tw
    inc     r13d
.tw:
    mov     ecx, 20
    call    ir_randn
    mov     r14d, eax
    cmp     r14d, 9
    jae     .ontab
    cmp     dword [fr_nc], 0
    je      .ontab
    ; в тайнике — посередине тайла, не видно
    mov     ecx, [fr_nc]
    call    ir_randn
    lea     rdx, [fr_cont]
    mov     esi, [rdx + rax*4]
    mov     edi, 2
    mov     r8d, esi
    call    .center
    xor     ecx, ecx
    jmp     .add
.ontab:
    cmp     r14d, 14
    jae     .onbed
    cmp     dword [fr_tab], -1
    je      .onbed
    cmp     r15d, 3
    jae     .onbed
    ; на столе: в углах, что ближе к глазу (ключ отрисовки — после стола)
    mov     esi, [fr_tab]
    call    .center
    lea     rdx, [ir_tabxy]
    addss   xmm0, [rdx + r15*8]
    addss   xmm1, [rdx + r15*8 + 4]
    inc     r15d
    mov     ecx, 8
    mov     edi, 1
    xor     r8d, r8d
    jmp     .add
.onbed:
    cmp     r14d, 17
    jae     .floor
    cmp     dword [fr_nbl], 0
    je      .floor
    mov     ecx, [fr_nbl]
    call    ir_randn
    lea     rdx, [fr_blt]
    mov     esi, [rdx + rax*4]
    call    .center
    addss   xmm0, [ir_0_1]
    addss   xmm1, [ir_0_1]
    lea     rdx, [tmap]
    mov     ecx, 7                      ; лавка
    cmp     byte [rdx + rsi], T_BED
    jne     .ob
    mov     ecx, 10
.ob:
    mov     edi, 1
    xor     r8d, r8d
    jmp     .add
.floor:
    ; на полу: пустой тайл, место в нём — 0.25..0.75
    mov     r14d, 16
.ft:
    mov     ecx, [fr_w]
    call    ir_randn
    add     eax, [fr_x0]
    mov     esi, eax
    mov     ecx, [fr_h]
    call    ir_randn
    add     eax, [fr_y0]
    mov     ecx, esi
    mov     edx, eax
    mov     edi, eax
    call    get_tile
    cmp     eax, T_WOOD
    je      .fok
    dec     r14d
    jnz     .ft
    jmp     .item
.fok:
    cvtsi2ss xmm2, esi
    cvtsi2ss xmm3, edi
    mov     ecx, 50
    call    ir_randn
    cvtsi2ss xmm0, eax
    mulss   xmm0, [f_0_01]
    addss   xmm0, [ir_0_25]
    addss   xmm0, xmm2
    mov     ecx, 50
    call    ir_randn
    cvtsi2ss xmm1, eax
    mulss   xmm1, [f_0_01]
    addss   xmm1, [ir_0_25]
    addss   xmm1, xmm3
    xor     ecx, ecx
    mov     edi, 1
    xor     r8d, r8d
.add:
    ; xmm0, xmm1 = место, ecx = высота, r13d = вид, edi = 1 на виду / 2 в тайнике, r8d = тайник
    mov     eax, [nloot]
    cmp     eax, MAXLOOT
    jae     .item
    inc     dword [nloot]
    imul    eax, eax, LO_SZ
    lea     rdx, [loot]
    add     rdx, rax
    movss   [rdx + LO_X], xmm0
    movss   [rdx + LO_Y], xmm1
    mov     [rdx + LO_Z], cl
    mov     [rdx + LO_T], r13b
    mov     [rdx + LO_S], dil
    mov     eax, [fr_hid]
    mov     [rdx + LO_H], al
    mov     [rdx + LO_C], r8w
    jmp     .item
.out:
    add     rsp, 40
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    ret
; esi = тайл (y·128 + x) -> xmm0, xmm1 = его середина
.center:
    mov     eax, esi
    and     eax, MAPW - 1
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    mov     eax, esi
    shr     eax, 7
    cvtsi2ss xmm1, eax
    addss   xmm1, [f_0_5]
    ret

; ---------------------------------------------------------------- кадр
; видно ли нутро: крыша снята или тает (HS_FADE), или прорежена над игроком (HS_OCC);
; маятник ходиков — туда-сюда раз в ~1.2 с
interior_frame:
    movss   xmm0, [elapsed]
    mulss   xmm0, [ir_3_2]
    cvttss2si eax, xmm0
    and     eax, 3
    lea     rdx, [ir_swing]
    movsx   eax, byte [rdx + rax]
    mov     [clk_sw], eax
    lea     rdx, [houses]
    mov     ecx, [nhouses]
.l:
    test    ecx, ecx
    jz      .out
    xor     eax, eax
    cmp     dword [rdx + HS_OCC], 0
    jne     .v
    movss   xmm0, [rdx + HS_FADE]
    comiss  xmm0, [ir_0_999]
    jae     .s
.v:
    mov     eax, 1
.s:
    mov     [rdx + HS_IVIS], eax
    add     rdx, HS_SZ
    dec     ecx
    jmp     .l
.out:
    ret

; доски своей краски и половики — поверх прохода пола, у изб с видным нутром. Пиксель
; тайла — тот, чей gpos (записал проход пола) в этом тайле; кровь — как в проходе пола
ifloor_pass:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    lea     r15, [houses]
    xor     r14d, r14d
.h:
    cmp     r14d, [nhouses]
    jae     .out
    cmp     dword [r15 + HS_IVIS], 0
    je      .hn
    mov     r13d, [r15 + HS_Y0]
    inc     r13d
.ty:
    mov     eax, [r15 + HS_Y0]
    add     eax, [r15 + HS_H]
    dec     eax
    cmp     r13d, eax
    jge     .hn
    mov     r12d, [r15 + HS_X0]
    inc     r12d
.tx:
    mov     eax, [r15 + HS_X0]
    add     eax, [r15 + HS_W]
    dec     eax
    cmp     r12d, eax
    jge     .tyn
    mov     eax, r13d
    shl     eax, 7
    add     eax, r12d
    lea     rdx, [fmap]
    movzx   eax, word [rdx + rax*2]
    test    eax, 7                      ; некрашеный, без половика — как есть
    jz      .txn
    mov     [if_f], eax
    call    .tile
.txn:
    inc     r12d
    jmp     .tx
.tyn:
    inc     r13d
    jmp     .ty
.hn:
    add     r15, HS_SZ
    inc     r14d
    jmp     .h
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
; тайл r12d, r13d: рамка на экране sx − 16 .. sx + 16, sy .. sy + 16
.tile:
    sub     rsp, 8
    mov     eax, r12d
    shl     eax, 8
    sub     eax, [camx]
    mov     ecx, r13d
    shl     ecx, 8
    sub     ecx, [camy]
    mov     esi, eax
    sub     esi, ecx
    sar     esi, 4
    add     esi, CX0                    ; sx
    add     eax, ecx
    sar     eax, 5
    add     eax, CY0
    mov     edi, eax                    ; sy
    mov     ebp, edi
    cmp     ebp, 0
    jge     .r0
    xor     ebp, ebp
.r0:
    lea     eax, [edi + 17]
    cmp     eax, SCR_H
    jle     .r1
    mov     eax, SCR_H
.r1:
    mov     [if_r1], eax
    lea     eax, [esi - 16]
    cmp     eax, 0
    jge     .c0
    xor     eax, eax
.c0:
    mov     [if_c0], eax
    lea     eax, [esi + 17]
    cmp     eax, SCR_W
    jle     .c1
    mov     eax, SCR_W
.c1:
    mov     [if_c1], eax
.row:
    cmp     ebp, [if_r1]
    jge     .done
    mov     ebx, [if_c0]
.px:
    cmp     ebx, [if_c1]
    jge     .rn
    imul    eax, ebp, SCR_W
    add     eax, ebx
    mov     [if_p], eax
    lea     rdx, [gposb]
    mov     eax, [rdx + rax*4]
    mov     ecx, eax
    and     ecx, 0xFFFF
    shr     eax, 16
    mov     edx, ecx
    shr     edx, 8
    cmp     edx, r12d                   ; пиксель этого тайла
    jne     .pn
    mov     edx, eax
    shr     edx, 8
    cmp     edx, r13d
    jne     .pn
    mov     edx, eax                    ; wy
    call    if_color                    ; ecx = wx, edx = wy -> eax
    mov     edx, [if_p]
    lea     r8, [albedo]
    mov     [r8 + rdx*4], eax
.pn:
    inc     ebx
    jmp     .px
.rn:
    inc     ebp
    jmp     .row
.done:
    add     rsp, 8
    ret

; ecx = wx, edx = wy (8.8), [if_f] — fmap тайла -> eax = альбедо пола (с кровью)
if_color:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    mov     r12d, ecx
    mov     r13d, edx
    mov     esi, [if_f]
    test    esi, 4
    jz      .wood
    call    if_rug
    test    eax, eax
    jnz     .blood
.wood:
    ; доски как в проходе пола, краска — по стилю
    mov     eax, esi
    and     eax, 3
    imul    eax, eax, 24
    lea     rbx, [if_style]
    add     rbx, rax
    mov     eax, r13d
    shr     eax, 6                      ; доска
    mov     edi, eax
    imul    ecx, eax, 97
    add     ecx, r12d
    mov     r8d, ecx                    ; x со сдвигом стыков
    shr     ecx, 8
    mov     edx, edi
    HASH    ecx, edx
    mov     r9d, r12d
    shr     r9d, 2
    mov     edx, edi
    HASH    r9d, edx
    and     ecx, 31
    sub     ecx, 16
    and     r9d, 7
    add     ecx, r9d                    ; разброс
    ; стык: (wy & 63) < 4 или (x & 255) < 3 — вдвое темнее
    xor     r10d, r10d
    mov     eax, r13d
    and     eax, 63
    cmp     eax, 4
    jb      .seam
    mov     eax, r8d
    and     eax, 255
    cmp     eax, 3
    jae     .ns
.seam:
    mov     r10d, 1
.ns:
    xor     eax, eax
    xor     edi, edi                    ; канал
.ch:
    mov     edx, ecx
    imul    edx, [rbx + rdi*4 + 12]
    sar     edx, 3
    add     edx, [rbx + rdi*4]
    test    r10d, r10d
    jz      .c1
    shr     edx, 1
.c1:
    lea     r9d, [edi*8]
    push    rcx
    mov     ecx, r9d
    shl     edx, cl
    pop     rcx
    or      eax, edx
    inc     edi
    cmp     edi, 3
    jb      .ch
.blood:
    ; кровь: (c·(256 − b) + b·кровь) >> 8 по каналу
    mov     ecx, r13d
    shr     ecx, 5
    shl     ecx, 10
    mov     edx, r12d
    shr     edx, 5
    add     ecx, edx
    lea     rdx, [blood]
    movzx   ecx, byte [rdx + rcx]
    test    ecx, ecx
    jz      .out
    mov     r8d, eax
    xor     eax, eax
    xor     edi, edi
.bc:
    lea     r9d, [edi*8]
    push    rcx
    mov     ecx, r9d
    mov     edx, r8d
    shr     edx, cl
    pop     rcx
    and     edx, 255
    mov     r10d, 256
    sub     r10d, ecx
    imul    edx, r10d
    lea     r11, [if_blood]
    mov     r10d, [r11 + rdi*4]
    imul    r10d, ecx
    add     edx, r10d
    shr     edx, 8
    push    rcx
    mov     ecx, r9d
    shl     edx, cl
    pop     rcx
    or      eax, edx
    inc     edi
    cmp     edi, 3
    jb      .bc
.out:
    or      eax, 0x01000000
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; половик: r12d = wx, r13d = wy, esi = fmap -> eax = 0xRRGGBB (0 — тут пол)
if_rug:
    push    rbx
    push    rdi
    push    r14
    push    r15
    ; до краёв половика, что есть в этом тайле (нет края — 999)
    mov     ecx, r12d
    and     ecx, 255                    ; lx
    mov     edx, r13d
    and     edx, 255                    ; ly
    mov     r8d, 999                    ; −x
    mov     r9d, 999                    ; +x
    mov     r10d, 999                   ; −y
    mov     r11d, 999                   ; +y
    test    esi, 0x08
    jz      .e1
    mov     r8d, ecx
.e1:
    test    esi, 0x10
    jz      .e2
    mov     r9d, 255
    sub     r9d, ecx
.e2:
    test    esi, 0x20
    jz      .e3
    mov     r10d, edx
.e3:
    test    esi, 0x40
    jz      .e4
    mov     r11d, 255
    sub     r11d, edx
.e4:
    cmp     r9d, r8d
    cmovl   r8d, r9d                    ; до края по x
    cmp     r11d, r10d
    cmovl   r10d, r11d                  ; по y
    mov     ebx, esi
    shr     ebx, 8
    and     ebx, 3
    shl     ebx, 4
    lea     rdi, [ir_rug]
    add     rdi, rbx                    ; палитра: 4 цвета
    test    esi, 0x80
    jnz     .carpet
    ; ----- полосатая дорожка: вдоль оси — полосы, поперёк — кромка; на концах бахрома
    mov     r14d, r12d                  ; вдоль
    mov     r15d, r13d                  ; поперёк
    mov     ebx, r10d                   ; до кромки
    mov     eax, r8d                    ; до конца
    test    esi, 0x400
    jz      .ra
    mov     r14d, r13d
    mov     r15d, r12d
    mov     ebx, r8d
    mov     eax, r10d
.ra:
    cmp     ebx, IR_M
    jb      .none
    cmp     eax, IR_M
    jae     .body
    ; бахрома: нитки поперёк конца
    cmp     eax, 6
    jb      .none
    test    r15d, 16
    jnz     .none
    mov     eax, 0xD8CCB0
    jmp     .weave
.body:
    cmp     ebx, IR_M + 10
    jae     .stripe
    mov     eax, [rdi + 8]              ; кромка — тёмная
    mov     ecx, eax
    shr     ecx, 2
    and     ecx, 0x3F3F3F
    sub     eax, ecx
    jmp     .weave
.stripe:
    shr     r14d, 5
    mov     eax, esi
    shr     eax, 11                     ; зерно
    imul    eax, eax, 13
    add     eax, r14d
    mov     ecx, esi
    shr     ecx, 8
    HASH    eax, ecx
    and     eax, 7
    lea     rcx, [ir_rsk]
    movzx   eax, byte [rcx + rax]
    mov     eax, [rdi + rax*4]
    jmp     .weave
.carpet:
    ; ----- ковёр: кайма, полоса с точками, кайма, поле с ромбами
    cmp     r10d, r8d
    cmovl   r8d, r10d
    cmp     r8d, IR_M
    jb      .none
    mov     eax, [rdi + 8]
    cmp     r8d, IR_M + 5
    jb      .weave
    cmp     r8d, IR_M + 14
    jae     .c2
    mov     eax, [rdi + 12]
    lea     ecx, [r12d + r13d]
    shr     ecx, 4
    test    ecx, 3
    jnz     .weave
    mov     eax, [rdi + 4]
    jmp     .weave
.c2:
    mov     eax, [rdi + 8]
    cmp     r8d, IR_M + 18
    jb      .weave
    mov     ecx, r12d
    and     ecx, 127
    sub     ecx, 64
    mov     edx, ecx
    sar     edx, 31
    xor     ecx, edx
    sub     ecx, edx
    mov     edx, r13d
    and     edx, 127
    sub     edx, 64
    mov     r8d, edx
    sar     r8d, 31
    xor     edx, r8d
    sub     edx, r8d
    add     ecx, edx                    ; ромб: |du| + |dv|
    mov     eax, [rdi + 4]
    cmp     ecx, 16
    jb      .weave
    mov     eax, [rdi + 12]
    cmp     ecx, 26
    jb      .weave
    mov     eax, [rdi + 8]
    cmp     ecx, 32
    jb      .weave
    mov     eax, [rdi]
.weave:
    ; плетение: через пиксель в шахматку на 1/16 темнее
    mov     ecx, r12d
    xor     ecx, r13d
    test    ecx, 8
    jz      .out
    mov     ecx, eax
    shr     ecx, 4
    and     ecx, 0x0F0F0F
    sub     eax, ecx
    jmp     .out
.none:
    xor     eax, eax
.out:
    pop     r15
    pop     r14
    pop     rdi
    pop     rbx
    ret

; ---------------------------------------------------------------- лут в игре
; каждый кадр живой игры: что рядом — вещь на виду (до 1.25 тайла, в той же избе) или
; необысканный сундук, шкаф (до середины соседнего тайла 1.15); E — взять или обыскать;
; 1..4 — пустить в ход из запаса
loot_update:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32
    movss   xmm0, [lm_t]
    subss   xmm0, [dt]
    maxss   xmm0, [f_0]
    movss   [lm_t], xmm0
    mov     dword [lp_kind], 0
    cmp     dword [gstate], 0
    jne     .out
    cvttss2si ecx, [pl + E_X]
    cvttss2si edx, [pl + E_Y]
    mov     eax, edx
    shl     eax, 7
    add     eax, ecx
    and     eax, MAPW*MAPW - 1
    lea     r8, [rmap]
    movzx   r12d, byte [r8 + rax]       ; изба игрока + 1
    test    r12d, r12d
    jz      .keys
    ; ближайшая вещь на виду (до 1.25 — со стола дотянуться через край)
    movss   xmm5, [ir_1_5625]
    mov     esi, -1
    lea     rbx, [loot]
    xor     edi, edi
.l:
    cmp     edi, [nloot]
    jae     .ld
    cmp     byte [rbx + LO_S], 1
    jne     .ln
    movzx   eax, byte [rbx + LO_H]
    cmp     eax, r12d
    jne     .ln
    movss   xmm0, [rbx + LO_X]
    subss   xmm0, [pl + E_X]
    mulss   xmm0, xmm0
    movss   xmm1, [rbx + LO_Y]
    subss   xmm1, [pl + E_Y]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    comiss  xmm0, xmm5
    jae     .ln
    movss   xmm5, xmm0
    mov     esi, edi
.ln:
    add     rbx, LO_SZ
    inc     edi
    jmp     .l
.ld:
    test    esi, esi
    js      .cont
    mov     dword [lp_kind], 1
    mov     [lp_idx], esi
    imul    eax, esi, LO_SZ
    lea     rdx, [loot]
    movzx   eax, byte [rdx + rax + LO_T]
    mov     [lp_type], eax
    jmp     .act
.cont:
    ; тайник рядом
    cvttss2si r8d, [pl + E_X]
    cvttss2si r9d, [pl + E_Y]
    movss   xmm5, [ir_1_3225]           ; 1.15²
    mov     esi, -1
    mov     r13d, -1
.cy:
    mov     r10d, -1
.cx:
    lea     ecx, [r8d + r10d]
    lea     edx, [r9d + r13d]
    mov     eax, edx
    shl     eax, 7
    add     eax, ecx
    and     eax, MAPW*MAPW - 1
    lea     r11, [tmap]
    movzx   ebx, byte [r11 + rax]
    cmp     ebx, T_CHEST
    je      .c1
    cmp     ebx, T_CUPB
    jne     .cn
.c1:
    lea     r11, [rmap]
    movzx   edi, byte [r11 + rax]
    cmp     edi, r12d
    jne     .cn
    lea     r11, [hflags]
    test    byte [r11 + rax], FM_OPEN
    jnz     .cn
    cvtsi2ss xmm0, ecx
    addss   xmm0, [f_0_5]
    subss   xmm0, [pl + E_X]
    mulss   xmm0, xmm0
    cvtsi2ss xmm1, edx
    addss   xmm1, [f_0_5]
    subss   xmm1, [pl + E_Y]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    comiss  xmm0, xmm5
    jae     .cn
    movss   xmm5, xmm0
    mov     esi, eax
.cn:
    inc     r10d
    cmp     r10d, 1
    jle     .cx
    inc     r13d
    cmp     r13d, 1
    jle     .cy
    test    esi, esi
    js      .keys
    mov     dword [lp_kind], 2
    mov     [lp_idx], esi
    ; 1 — сундук, 2 — шкаф, 3 — буфет
    lea     rdx, [tmap]
    mov     eax, 1
    cmp     byte [rdx + rsi], T_CHEST
    je      .ct
    lea     rdx, [hflags]
    mov     eax, 2
    test    byte [rdx + rsi], FM_ALT
    jz      .ct
    mov     eax, 3
.ct:
    mov     [lp_type], eax
.act:
    cmp     byte [k_e_edge], 0
    je      .keys
    cmp     dword [lp_kind], 1
    jne     .search
    ; взять: «+ термос с чаем»
    mov     esi, [lp_idx]
    imul    ebx, esi, LO_SZ
    lea     rax, [loot]
    add     rbx, rax
    mov     byte [rbx + LO_S], 0
    movzx   r13d, byte [rbx + LO_T]
    lea     rdx, [inv]
    inc     dword [rdx + r13*4 - 4]
    lea     rdi, [lm_buf]
    lea     rsi, [ts_plus]
    call    wput_str
    lea     rdx, [lo_names]
    mov     rsi, [rdx + r13*8 - 8]
    call    wput_str
    mov     dword [lm_t], __float32__(2.5)
    mov     dword [lp_kind], 0
    call    snd_click
    jmp     .keys
.search:
    ; обыскать: всё, что там, — в запас; «в сундуке: бинт, батарейки» или «в шкафу пусто»
    mov     r12d, [lp_idx]
    lea     rdx, [hflags]
    or      byte [rdx + r12], FM_OPEN
    lea     rdi, [lm_buf]
    mov     eax, [lp_type]
    lea     rdx, [lo_cin]
    mov     rsi, [rdx + rax*8 - 8]
    call    wput_str
    xor     r13d, r13d                  ; нашлось
    lea     rbx, [loot]
    xor     ecx, ecx
.sl:
    cmp     ecx, [nloot]
    jae     .sd
    cmp     byte [rbx + LO_S], 2
    jne     .sn
    movzx   eax, word [rbx + LO_C]
    cmp     eax, r12d
    jne     .sn
    mov     byte [rbx + LO_S], 0
    movzx   r8d, byte [rbx + LO_T]      ; wput_str портит ax
    lea     rdx, [inv]
    inc     dword [rdx + r8*4 - 4]
    lea     rsi, [ts_colon]
    test    r13d, r13d
    jz      .s1
    lea     rsi, [ts_comma]
.s1:
    call    wput_str
    lea     rdx, [lo_names]
    mov     rsi, [rdx + r8*8 - 8]
    call    wput_str
    inc     r13d
.sn:
    add     rbx, LO_SZ
    inc     ecx
    jmp     .sl
.sd:
    test    r13d, r13d
    jnz     .sf
    lea     rsi, [ts_empty]
    call    wput_str
.sf:
    mov     dword [lm_t], __float32__(3.0)
    mov     dword [lp_kind], 0
    call    snd_click
.keys:
    xor     esi, esi
.k:
    lea     rdx, [k_n_edge]
    cmp     byte [rdx + rsi], 0
    je      .kn
    lea     ecx, [esi + 1]
    call    loot_use
.kn:
    inc     esi
    cmp     esi, NLT
    jb      .k
.out:
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ecx = вид 1..4: пустить в ход одну из запаса, если нужна
loot_use:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32
    cmp     dword [gstate], 0
    jne     .out
    mov     ebx, ecx
    lea     rdx, [inv]
    cmp     dword [rdx + rbx*4 - 4], 0
    je      .out
    cmp     ebx, 2
    jb      .bat
    je      .band
    cmp     ebx, 3
    je      .tea
    ; тушёнка: +12 здоровья, силы — все
    movss   xmm0, [pl_hp]
    comiss  xmm0, [ir_99]
    jb      .food
    movss   xmm0, [pl_sta]
    comiss  xmm0, [ir_99]
    jae     .no
.food:
    movss   xmm0, [pl_hp]
    addss   xmm0, [ir_12]
    minss   xmm0, [f_100]
    movss   [pl_hp], xmm0
    mov     dword [pl_sta], __float32__(100.0)
    jmp     .used
.bat:
    ; батарейки: фонарю +60
    movss   xmm0, [pl_bat]
    comiss  xmm0, [ir_99]
    jae     .no
    addss   xmm0, [ir_60]
    minss   xmm0, [f_100]
    movss   [pl_bat], xmm0
    jmp     .used
.band:
    ; бинт: +35 здоровья
    movss   xmm0, [pl_hp]
    comiss  xmm0, [ir_99]
    jae     .no
    addss   xmm0, [ir_35]
    minss   xmm0, [f_100]
    movss   [pl_hp], xmm0
    jmp     .used
.tea:
    ; чай из термоса: +45 тепла
    movss   xmm0, [pl_warm]
    comiss  xmm0, [ir_99]
    jae     .no
    addss   xmm0, [ir_45]
    minss   xmm0, [f_100]
    movss   [pl_warm], xmm0
.used:
    lea     rdx, [inv]
    dec     dword [rdx + rbx*4 - 4]
    lea     rdx, [lo_used]
    jmp     .msg
.no:
    lea     rdx, [lo_nneed]
.msg:
    mov     rsi, [rdx + rbx*8 - 8]
    lea     rdi, [lm_buf]
    call    wput_str
    mov     dword [lm_t], __float32__(2.5)
    call    snd_click
.out:
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- картинки лута
; из карт знаков (lsp_*) в BGRA (spr_loot): у каждой — своя ширина, «.» — пусто
gen_loot:
    push    rsi
    push    rdi
    lea     rdi, [spr_loot]
    lea     rsi, [lsp_all]
.c:
    movzx   eax, byte [rsi]
    test    eax, eax
    jz      .out
    inc     rsi
    xor     ecx, ecx                    ; «.» -> 0
    cmp     eax, '.'
    je      .put
    lea     rdx, [lsp_pal]
.f:
    movzx   r8d, byte [rdx]
    test    r8d, r8d
    jz      .put
    cmp     r8d, eax
    je      .got
    add     rdx, 5
    jmp     .f
.got:
    mov     ecx, [rdx + 1]
.put:
    mov     [rdi], ecx
    add     rdi, 4
    jmp     .c
.out:
    pop     rdi
    pop     rsi
    ret

; ================================================================ данные
section .data
align 4, db 0
ir_rng:     dd 1
ir_sdx:     dd 0, -1, 0, 1              ; стороны: −y, −x, +y, +x
ir_sdy:     dd -1, 0, 1, 0
ir_usg:     dd 1, -1, -1, 1             ; куда +u вдоль стены (знак по её оси)
ir_0_999:   dd 0.999
ir_0_1:     dd 0.1
ir_0_25:    dd 0.25
ir_1_3225:  dd 1.3225
ir_1_5625:  dd 1.5625
ir_12:      dd 12.0
ir_35:      dd 35.0
ir_45:      dd 45.0
ir_60:      dd 60.0
ir_99:      dd 99.0
ir_tabxy:   dd 0.24, 0.3,  -0.2, 0.32,  0.3, -0.14   ; на столе: от середины
ir_3_2:     dd 3.2
ir_ontab:   db 1, 1, 2, 0
ir_swing:   db 0, 1, 0, -1
align 4, db 0
; краски стен: панель под побелкой, обои с узором, вагонка
ir_panel:   dd 0x4E7E86, 0x5A74A0, 0x6E8A5A, 0x8A6A4A
ir_paper:   dd 0xB4C4A2, 0xA6B6C8, 0xCCB896, 0xC8A6A0, 0xC8C09A, 0x9EB4B0
ir_motif:   dd 0x6E8A4E, 0x5A6C9C, 0x9A6A46, 0x9C4E4E, 0x8C7A3A, 0x4E6E6A
ir_board:   dd 0xB07A46, 0x9A6A3C, 0xC0925A, 0xA88050
ir_curt:    dd 0xD08048, 0x6A8AC0, 0xE0D8C8, 0xB04A3A, 0x7AA060, 0xD8B860
ir_wood:    dd 0x6A4428, 0x8A5A30, 0x4A3020, 0x9A6A3A
ir_chest:   dd 0x2E5A8A, 0x3A6A3A, 0x8A2A22, 0x6A3A5A
; половики: 4 палитры по 4 цвета; дорожка — полосы по ir_rsk, ковёр — поле, узор, кайма, светлое
ir_rug:     dd 0x8C2A24, 0xC89A3C, 0x2C4A6E, 0xD8D0C0
            dd 0x3C5A7A, 0x9AA8B0, 0x6A2E3A, 0xC8B890
            dd 0x3E6A44, 0xB88A3A, 0x8A3A2E, 0xD0CCB8
            dd 0x6A4A32, 0xA8744A, 0x4A5A6A, 0xC8B8A0
ir_rsk:     db 0, 0, 1, 1, 2, 2, 3, 0
IR_M        equ 22                      ; от края тайла до половика (1/256 тайла)
; мебель: грань коробки (левая +y, правая +x) -> часть (0 лицо, 1 бок, 2 спина) по повороту
fo_pm:      db 0, 1,  1, 0,  2, 1,  1, 2
align 4, db 0
; одеяла лоскутные: 4 палитры по 4 цвета
ir_blnk:    dd 0xB03A30, 0xD8C8A8, 0x3A5A8A, 0xC89040
            dd 0x5A7AA8, 0xE0E0E0, 0x8AA8C8, 0x3A4A6A
            dd 0x7A9A5A, 0xD0B878, 0xA05A3A, 0xE0D8C0
            dd 0xA86A8A, 0xE8D8D8, 0x6A4A7A, 0xC8A0B0
; ковры на стену: поле, медальон, кайма, светлое
ir_carp:    dd 0x7A1C1C, 0x1E2E5A, 0x2A1414, 0xD8C090
            dd 0x22305A, 0x8A2020, 0x141A30, 0xD0C8A0
            dd 0x2A4A2E, 0xA07A2A, 0x1A2A1A, 0xD8C8A0
            dd 0x5A3420, 0x2A4A5A, 0x2E1C12, 0xD0B888
clk_sw:     dd 0                        ; маятник ходиков: −1, 0, 1 (кадр)
align 4, db 0
; доски пола: B, G, R и их разброс (·v >> 3) — некрашеные (как проход пола), сурик,
; охра, серая краска
if_style:   dd 40, 72, 112, 3, 5, 7
            dd 30, 42, 118, 1, 2, 3
            dd 38, 84, 140, 2, 3, 4
            dd 104, 96, 84, 2, 2, 2
if_blood:   dd 22, 14, 110
; лут: имена, что в запасе пишут, когда пускают в ход и когда не нужно
align 8, db 0
lo_names:   dq ts_lt1, ts_lt2, ts_lt3, ts_lt4
lo_used:    dq ts_u1, ts_u2, ts_u3, ts_u4
lo_nneed:   dq ts_n1, ts_n2, ts_n3, ts_n4
lo_cin:     dq ts_in1, ts_in2, ts_in3
lo_cask:    dq ts_ask1, ts_ask2, ts_ask3
; картинки: ширина, высота, начало в spr_loot (дворды)
lo_sw:      db 7, 6, 4, 6
lo_sh:      db 6, 5, 10, 6
align 4, db 0
lo_soff:    dd 0, 42, 72, 112
lsp_all:    db '..q..t.', 'TTTTTTT', 'RRRWBBB', 'RRRWBBB', 'RRRWBBB', 'rrrwbbb'
            db '.WWWW.', 'WWXWWw', 'WBBBWw', 'WWWWWw', '.gggg.'
            db '.SX.', 'sSSs', 'ssss', 'KKKK', 'EEEe', 'EFEe', 'EEFe', 'EEEe', 'EEEe', '.ee.'
            db '.MXMM.', 'MmmmmM', 'YYYYYy', 'kkkkkn', 'YYYYYy', '.GGGG.'
            db 0
lsp_pal:    db 't'
            dd 0x01D8B050
            db 'q'
            dd 0x41E8C060                ; искра: в свете блестит
            db 'T'
            dd 0x018A929C
            db 'R'
            dd 0x01B83028
            db 'W'
            dd 0x01E4E2D8
            db 'B'
            dd 0x012A4A9A
            db 'r'
            dd 0x018A2420
            db 'w'
            dd 0x01B8B4AA
            db 'b'
            dd 0x011E3670
            db 'X'
            dd 0x41F0F2F4
            db 'g'
            dd 0x019A9A92
            db 'S'
            dd 0x01C8D0D8
            db 's'
            dd 0x0198A0A8
            db 'K'
            dd 0x012A2A2E
            db 'E'
            dd 0x01B83A2E
            db 'e'
            dd 0x01842A22
            db 'F'
            dd 0x01E8D8A0
            db 'M'
            dd 0x01CDD2D8
            db 'm'
            dd 0x01A0A4A8
            db 'Y'
            dd 0x01C89838
            db 'y'
            dd 0x0196702A
            db 'k'
            dd 0x017A2A20
            db 'n'
            dd 0x015A1E18
            db 'G'
            dd 0x017A7E82
            db 0
align 2, db 0
ts_lt1:     dw __utf16__('батарейки'), 0
ts_lt2:     dw __utf16__('бинт'), 0
ts_lt3:     dw __utf16__('термос с чаем'), 0
ts_lt4:     dw __utf16__('тушёнка'), 0
ts_u1:      dw __utf16__('батарейки — в фонарь'), 0
ts_u2:      dw __utf16__('перевязался'), 0
ts_u3:      dw __utf16__('горячий чай — согрелся'), 0
ts_u4:      dw __utf16__('поел — силы вернулись'), 0
ts_n1:      dw __utf16__('фонарь и так полный'), 0
ts_n2:      dw __utf16__('и так цел'), 0
ts_n3:      dw __utf16__('и так тепло'), 0
ts_n4:      dw __utf16__('не голоден'), 0
ts_in1:     dw __utf16__('в сундуке'), 0
ts_in2:     dw __utf16__('в шкафу'), 0
ts_in3:     dw __utf16__('в буфете'), 0
ts_ask1:    dw __utf16__('обыскать сундук'), 0
ts_ask2:    dw __utf16__('обыскать шкаф'), 0
ts_ask3:    dw __utf16__('обыскать буфет'), 0
ts_take:    dw __utf16__('взять '), 0
ts_plus:    dw __utf16__('+ '), 0
ts_colon:   dw __utf16__(': '), 0
ts_comma:   dw __utf16__(', '), 0
ts_empty:   dw __utf16__(' пусто'), 0
align 4, db 0
nloot:      dd 0
lm_t:       dd 0.0                      ; сообщение: сколько ещё видно, с
lp_kind:    dd 0                        ; рядом: 0 ничего, 1 вещь, 2 тайник
lp_idx:     dd 0                        ;   какая (номер в loot) или где (тайл)
lp_type:    dd 0                        ;   вид вещи 1..4 или тайника 1..3
inv:        dd 0, 0, 0, 0               ; запас: батарейки, бинты, термосы, тушёнка

section .bss
alignb 4
fr_hid:     resd 1                      ; обставляемая изба + 1
fr_x0:      resd 1                      ; комната (без сруба)
fr_y0:      resd 1
fr_w:       resd 1
fr_h:       resd 1
fr_base:    resd 1                      ; флаги дома в hflags
iz_ex:      resd 1                      ; вход
iz_ey:      resd 1
fr_ix:      resd 1                      ; внутрь от двери
fr_iy:      resd 1
fr_sx:      resd 1                      ; перед топкой
fr_sy:      resd 1
fr_fs:      resd 1                      ; краска пола
fr_kind:    resd 1
fr_mask:    resd 1
fr_solid:   resd 1
fr_vfl:     resd 1
fr_k:       resd 1
fr_hd:      resd 1
fr_qx:      resd 1
fr_qy:      resd 1
fr_bed:     resd 1                      ; кровать: есть, у какой стены, тайлы
iz_bs:      resd 1
fr_b0x:     resd 1
fr_b0y:     resd 1
fr_b1x:     resd 1
fr_b1y:     resd 1
fr_tab:     resd 1                      ; тайл стола (−1 — нет)
fr_nc:      resd 1
fr_cont:    resd 4                      ; тайники
fr_nbl:     resd 1
fr_blt:     resd 4                      ; кровати, лавки
fr_icon:    resb 4
fr_pool:    resb 8
fr_vis:     resb 64
fr_q:       resb 64
if_f:       resd 1
if_p:       resd 1
if_r1:      resd 1
if_c0:      resd 1
if_c1:      resd 1
alignb 16
loot:       resb MAXLOOT*LO_SZ
lm_buf:     resw 96                     ; сообщение лута
spr_loot:   resd 160                    ; картинки лута (lo_soff)
wdeco:      resb MAPW*MAPW              ; убранство стены изнутри (ir_walls)
fmap:       resw MAPW*MAPW              ; краска пола и половики (ir_rugs)

section .text
