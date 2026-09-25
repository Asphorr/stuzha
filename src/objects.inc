; ----------------------------------------------------------------------------
;  objects.inc — объекты в G-буфер: блоки, картинки, фигуры
; ----------------------------------------------------------------------------
; Собирается NBANDS раз (bands.inc): у каждой копии свои переменные, и пишет она
; только строки своей полосы экрана [band_y0, band_y1)

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
    ; лут на виду — в избах с видным нутром (номер | 0x10000)
    lea     rbx, [loot]
    xor     r12d, r12d
.lo:
    cmp     r12d, [nloot]
    jae     .built
    cmp     r15d, 300
    jae     .built
    cmp     byte [rbx + LO_S], 1
    jne     .lon
    movzx   eax, byte [rbx + LO_H]
    dec     eax
    imul    eax, eax, HS_SZ
    lea     rdx, [houses]
    cmp     dword [rdx + rax + HS_IVIS], 0
    je      .lon
    movss   xmm0, [rbx + LO_X]
    addss   xmm0, [rbx + LO_Y]
    mulss   xmm0, [f_256]
    cvtss2si eax, xmm0
    mov     [rdi], eax
    lea     eax, [r12d + 0x10000]
    mov     [rdi+4], eax
    add     rdi, 8
    inc     r15d
.lon:
    add     rbx, LO_SZ
    inc     r12d
    jmp     .lo
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
    lea     r13d, [eax + 38]                ; снизу — высокие деревья, чьи макушки в кадре
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
    ; растения — объёмные картинки (vegdraw.inc), у них свои рамки на экране
    mov     r9d, [bk_kind]
    cmp     r9d, D_SPRUCE
    je      .veg
    cmp     r9d, D_BIRCH
    je      .veg
    cmp     r9d, D_PINE
    jae     .veg
    cmp     eax, -40
    jl      .out
    cmp     eax, SCR_W + 40
    jg      .out
    cmp     r8d, -20
    jl      .out
    cmp     r8d, SCR_H + 72
    jg      .out
    ; мимо полосы: пиксели тайла — строки sy − 54 .. sy + 15 (выше всех — ель)
    lea     r9d, [r8d + 16]
    cmp     r9d, [band_y0]
    jle     .out
    lea     r9d, [r8d - 54]
    cmp     r9d, [band_y1]
    jge     .out
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
    mov     dword [bk_inner], 0
    mov     dword [bk_hid], 0
    mov     dword [bk_cloth], 0
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
    cmp     eax, D_BED
    jae     .furn
    jmp     .sprite
.furn:
    call    furn_tile
    jmp     .out
.wall:
    mov     dword [bk_H], 36
    call    .winner
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
    call    .hvis
    test    eax, eax
    jz      .blk
    mov     eax, [bk_hid]
    dec     eax
    imul    eax, eax, HS_SZ
    lea     rdx, [houses]
    mov     eax, [rdx + rax + HS_CLOTH]
    mov     [bk_cloth], eax
    call    draw_block
    call    table_top                   ; телевизор, самовар, кринка
    jmp     .out
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
    mov     ecx, eax
    sub     ecx, [band_y0]
    cmp     ecx, [band_h]
    jae     .out                        ; не в полосе
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
    jmp     .out
.veg:
    mov     eax, [bk_ty]
    shl     eax, 7
    add     eax, [bk_tx]
    lea     rdx, [vmap]
    movzx   eax, word [rdx + rax*2]
    test    al, al
    jz      .out
    mov     [vd_fl], eax
    movzx   eax, al
    dec     eax
    imul    eax, eax, RS_SZ
    lea     rbx, [vrs]
    add     rbx, rax
    mov     [vd_rs], rbx
    ; грубо мимо экрана (высокие деревья снизу заходят в кадр)
    mov     eax, [bk_sx]
    cmp     eax, -64
    jl      .out
    cmp     eax, SCR_W + 64
    jg      .out
    mov     eax, [bk_sy]
    cmp     eax, -24
    jl      .out
    cmp     eax, SCR_H + 140
    jg      .out
    mov     eax, [bk_tx]
    mov     r9d, [bk_ty]
    HASH    eax, r9d
    mov     [bk_seed], eax
    ; перед игроком ли (как у прочих)
    mov     eax, [bk_tx]
    add     eax, [bk_ty]
    sub     eax, [pdiag]
    mov     r12d, eax
    mov     eax, [bk_sx]
    sub     eax, [psx]
    mov     r13d, eax
    sar     r13d, 31
    xor     eax, r13d
    sub     eax, r13d
    mov     r13d, eax
    ; качание на ветру — как у картинок, размах свой у варианта
    mov     dword [img_sway], 0
    cmp     dword [rbx + RS_AMP], 0
    je      .vns
    call    .treewind
    mov     eax, [bk_seed]
    and     eax, 1023
    cvtsi2ss xmm0, eax
    mulss   xmm0, [f_inv163]
    mov     eax, [bk_seed]
    shr     eax, 12
    and     eax, 7
    cvtsi2ss xmm1, eax
    mulss   xmm1, [f_0_08]
    addss   xmm1, [f_1_9]
    mulss   xmm1, [wind_t]
    addss   xmm0, xmm1
    call    fsin
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
    cvtsi2ss xmm1, dword [rbx + RS_AMP]
    mulss   xmm0, xmm1
    cvtss2si eax, xmm0
    mov     [img_sway], eax
.vns:
    ; просвечивает, если заслоняет игрока
    mov     dword [vd_dith], 0
    cmp     r12d, 1
    jl      .vnd
    cmp     r12d, 10
    jg      .vnd
    mov     eax, [rbx + RS_R]
    add     eax, 8
    cmp     r13d, eax
    jg      .vnd
    mov     dword [vd_dith], 1
.vnd:
    call    draw_veg
    mov     dword [img_sway], 0
.out:
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
; изба тайла, если её нутро видно (крыша снята, тает, прорежена) -> bk_hid = дом + 1,
; eax = 1; иначе eax = 0
.hvis:
    mov     eax, [bk_ty]
    shl     eax, 7
    add     eax, [bk_tx]
    lea     rdx, [rmap]
    movzx   ecx, byte [rdx + rax]
    test    ecx, ecx
    jz      .hv0
    lea     edx, [ecx - 1]
    imul    edx, edx, HS_SZ
    lea     r8, [houses]
    cmp     dword [r8 + rdx + HS_IVIS], 0
    je      .hv0
    mov     [bk_hid], ecx
    mov     eax, 1
    ret
.hv0:
    xor     eax, eax
    ret
; сруб: на какой грани окно — поперёк стены (стена вдоль x, сосед по x — сруб: на +y,
; иначе на +x; вторая грань — торец, там брёвна). У избы с видным нутром — какие грани
; смотрят в комнату (bk_inner) и что на стене (bk_deco)
.winner:
    mov     dword [bk_wface], 1
    cmp     dword [bk_kind], D_WALL
    je      .wi1
    mov     ecx, [bk_tx]
    inc     ecx
    mov     edx, [bk_ty]
    call    get_tile
    cmp     eax, T_WALL
    je      .wi0
    mov     ecx, [bk_tx]
    dec     ecx
    call    get_tile
    cmp     eax, T_WALL
    jne     .wi1
.wi0:
    mov     dword [bk_wface], 0
.wi1:
    call    .hvis
    test    eax, eax
    jz      .wi9
    mov     eax, [bk_ty]
    shl     eax, 7
    add     eax, [bk_tx]
    lea     rdx, [wdeco]
    movzx   ecx, byte [rdx + rax]
    mov     [bk_deco], ecx
    lea     rdx, [tmap]
    lea     r8, [tprop]
    movzx   ecx, byte [rdx + rax + MAPW]
    test    byte [r8 + rcx], 4
    jz      .wi2
    or      dword [bk_inner], 1
.wi2:
    movzx   ecx, byte [rdx + rax + 1]
    test    byte [r8 + rcx], 4
    jz      .wi3
    or      dword [bk_inner], 2
    ret
.wi3:
    ; столбец p = 0 (правая грань) сосед по x не закрывает: если за ним по диагонали
    ; комната — это стык двух внутренних граней, красим как левую (u = 15)
    movzx   ecx, byte [rdx + rax + MAPW + 1]
    test    byte [r8 + rcx], 4
    jz      .wi9
    or      dword [bk_inner], 5         ; левая грань угла закрыта соседом — пусть тоже
.wi9:
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
    cmp     esi, [band_y0]              ; строки полосы
    jge     .t0
    mov     esi, [band_y0]
.t0:
    cmp     edi, [band_y1]
    jle     .tl
    mov     edi, [band_y1]
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
    jnz     .fl
    test    dword [bk_inner], 4         ; стык внутренних граней — продолжение левой
    jz      .fl
    xor     r15d, r15d
    mov     ebp, 15
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
    cmp     esi, [band_y0]
    jge     .s0
    mov     esi, [band_y0]
.s0:
    mov     edi, r14d
    cmp     edi, [band_y1]
    jle     .s1
    mov     edi, [band_y1]
.s1:
    ; колонка из одних брёвен (стена, срезанная стена, окно не в этой колонке) — быстрый
    ; путь: то же, что shade_side.logs, без вызова и развилок по виду, оттенок — из
    ; таблицы; сугроб внизу колонки дорисует общий цикл
    mov     eax, [bk_kind]
    bt      dword [bk_inner], r15d      ; грань в комнату избы без крыши — отделка
    jc      .sl
    cmp     eax, D_WALL
    je      .flog
    cmp     eax, D_WINL
    ja      .sl
    cmp     dword [bk_cut], 0
    jne     .flog
    cmp     r15d, [bk_wface]            ; окно — только на своей грани, торец — брёвна
    jne     .flog
    lea     eax, [rbp - 2]
    sub     eax, r15d                   ; окно и подоконник — в u − 2 − грань = 0..10
    cmp     eax, 10
    jbe     .sl
.flog:
    lea     r9, [lut_log]               ; торец бревна — на краю грани
    test    r15d, r15d
    jnz     .fq1
    test    ebp, ebp
    jnz     .fq2
    lea     r9, [lut_logend]
    jmp     .fq2
.fq1:
    cmp     ebp, 15
    jne     .fq2
    lea     r9, [lut_logend]
.fq2:
    imul    r8d, ebp, 3
    add     r8d, [bk_seed]              ; соль снега в пазу: u·3 + seed
.fpx:
    cmp     esi, edi
    jge     .nextcol
    mov     edx, r14d
    dec     edx
    sub     edx, esi                    ; h
    cmp     edx, [col_dh]
    jb      .sl                         ; дальше вниз — сугроб
    lea     rax, [logtab]
    movzx   eax, byte [rax + rdx]
    mov     r10d, eax
    and     r10d, 15                    ; ряд в бревне
    shr     eax, 4                      ; бревно
    test    r10d, r10d
    jnz     .fp2
    mov     ecx, r8d
    HASH    ecx, eax
    test    ecx, 3
    jnz     .fp2
    mov     eax, 0x01D8DEE6             ; снег лежит в пазу — смотрит в небо
    mov     ecx, N_UP
    jmp     .fput
.fp2:
    mov     r11d, ebp
    add     r11d, [bk_seed]
    mov     ecx, edx
    HASH    r11d, ecx
    and     r11d, 15
    lea     rax, [logshade]
    movzx   ecx, byte [rax + r10]
    sub     ecx, r11d
    mov     eax, edx                    ; у самого снега темнее — контактная тень
    sub     eax, [col_dh]
    cmp     eax, 8
    jae     .fp5
    imul    eax, eax, 10
    add     eax, 176
    imul    ecx, eax
    shr     ecx, 8
.fp5:
    cmp     dword [bk_cut], 0           ; верхние венцы — в тени свеса крыши
    jne     .fp6
    mov     eax, [bk_H]
    sub     eax, edx
    cmp     eax, 5
    ja      .fp6
    imul    eax, eax, 24
    add     eax, 136
    imul    ecx, eax
    shr     ecx, 8
.fp6:
    mov     eax, [r9 + rcx*4]
    lea     rcx, [lognrm]
    lea     rcx, [rcx + r15*8]
    movzx   ecx, byte [rcx + r10]
.fput:
    imul    r11d, esi, SCR_W
    add     r11d, ebx
    lea     rdx, [albedo]
    mov     [rdx + r11*4], eax
    mov     eax, [col_gpos]
    lea     rdx, [gposb]
    mov     [rdx + r11*4], eax
    lea     rdx, [nbuf]
    mov     [rdx + r11], cl
    inc     esi
    jmp     .fpx
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
    mov     eax, [bk_cloth]
    test    eax, eax
    jnz     .cloth
    and     edx, 63
    mov     eax, 0x018C5C36
    cmp     edx, 6
    jae     .r
    mov     eax, 0x015A3C24
    ret
.cloth:
    cmp     eax, 2
    je      .coil
    ja      .cgrn
    ; белая скатерть: две красные полосы вдоль края
    mov     eax, ecx
    mov     r8d, 255
    sub     r8d, ecx
    cmp     r8d, eax
    cmovl   eax, r8d
    cmp     edx, eax
    cmovl   eax, edx
    mov     r8d, 255
    sub     r8d, edx
    cmp     r8d, eax
    cmovl   eax, r8d                    ; до края
    lea     r8d, [eax - 20]
    cmp     r8d, 8
    jb      .cred
    lea     r8d, [eax - 36]
    cmp     r8d, 6
    jb      .cred
    mov     eax, 0x01E8E4DA
    ret
.cred:
    mov     eax, 0x01B83028
    ret
.coil:
    ; клеёнка в клетку по 1/8 тайла
    shr     ecx, 5
    shr     edx, 5
    add     ecx, edx
    test    ecx, 1
    mov     eax, 0x01E4E8EC
    jz      .r
    mov     eax, 0x013A6AB0
    ret
.cgrn:
    ; сукно: ворс шахматкой чуть темнее
    xor     ecx, edx
    test    ecx, 16
    mov     eax, 0x013E6040
    jz      .r
    mov     eax, 0x01365838
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
    bt      dword [bk_inner], edi
    jc      .inner
    cmp     eax, D_WALL
    je      .logs
    cmp     dword [bk_cut], 0
    jne     .logs
    cmp     edi, [bk_wface]
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
.inner:
    ; грань в комнату избы без крыши: отделка, окно изнутри, убранство (furn.inc);
    ; 0 — брёвна, как снаружи
    call    wall_inner
    test    eax, eax
    jnz     .ret
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
    cmp     dword [bk_cloth], 0
    jne     .tcl
    cmp     esi, 6
    jae     .tapron
.tlegs:
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
.tcl:
    ; скатерть свисает на 5 px, под ней — ножки
    cmp     esi, 3
    jb      .tlegs
    mov     eax, [bk_cloth]
    cmp     eax, 2
    je      .toil
    ja      .tgrn
    ; белая: внизу кружево через пиксель, красная полоса
    cmp     esi, 3
    jne     .tw1
    test    ebx, 1
    jz      .transp
    mov     eax, 0x01D8D4CC
    jmp     .retface
.tw1:
    mov     eax, 0x01E8E4DA
    cmp     esi, 5
    jne     .retface
    mov     eax, 0x01B83028
    jmp     .retface
.toil:
    ; клеёнка в клетку по 2 px
    mov     eax, ebx
    shr     eax, 1
    mov     ecx, esi
    shr     ecx, 1
    add     eax, ecx
    test    eax, 1
    mov     eax, 0x01E4E8EC
    jz      .retface
    mov     eax, 0x013A6AB0
    jmp     .retface
.tgrn:
    ; зелёная суконная, с бахромой
    cmp     esi, 3
    jne     .tg1
    test    ebx, 1
    jz      .transp
.tg1:
    mov     eax, 0x013E6040
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
    ; строки картинки в полосе: от max(0, y0 − y) до min(h, y1 − y)
    mov     eax, [band_y1]
    sub     eax, r13d
    cmp     eax, r15d
    cmovg   eax, r15d
    mov     [di_r1], eax
    mov     r10d, [band_y0]
    sub     r10d, r13d
    xor     eax, eax
    cmp     r10d, eax
    cmovl   r10d, eax
.r:
    cmp     r10d, [di_r1]
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
    ; строки в полосе: от max(0, y0 − y) до min(h, y1 − y)
    mov     eax, [band_y1]
    sub     eax, r13d
    cmp     r15d, eax
    cmovg   r15d, eax
    mov     r10d, [band_y0]
    sub     r10d, r13d
    xor     eax, eax
    cmp     r10d, eax
    cmovl   r10d, eax
.r:
    cmp     r10d, r15d
    jge     .done
    mov     eax, r13d
    add     eax, r10d
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
    mov     ebx, eax
    sub     ebx, [band_y0]
    cmp     ebx, [band_h]
    jae     .ny                         ; не в полосе
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
    cmp     ecx, 0x10000                ; лут на виду (furn.inc)
    jb      .z1
    sub     ecx, 0x10000
    call    draw_loot
    jmp     .out
.z1:
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

