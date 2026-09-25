; ----------------------------------------------------------------------------
;  furn.inc — нутро избы в полосах объектов: стены изнутри, мебель коробками, вещи
;             на столе, лут (данные и расстановка — interior.inc)
; ----------------------------------------------------------------------------
; Собирается по копии на полосу (bands.inc), как objects.inc. Мебель — коробки с
; попиксельными gpos и нормалями (draw_box), так что свет, тени и память видимости
; ложатся на неё, как на срубы

; ================================================================ коробка
; коробка в тайле bk_tx, bk_ty (экран его угла — bk_sx, bk_sy): [bx_x0, bx_x1] ×
; [bx_y0, bx_y1] в 1/16 тайла от угла, от bx_z0 вверх на bx_h px; грани bx_fc: 1 — +y
; (левая), 2 — +x (правая), 4 — верх. Цвет — [bx_sh]: ecx = грань (0 левая, 1 правая,
; 2 верх), edx = u вдоль грани слева направо (px) или X верха, r8d = высота над низом
; коробки или Y верха (1/256 тайла от угла) -> eax (0 — прозрачно); нормаль — sh_n
; (задана по грани). Пиксель — тот, чей центр в проекции: соседние коробки стыкуются
; без щелей и наложений. Шейдер портит только rax, rcx, rdx, r8 .. r11
draw_box:
    push    rbx
    push    rsi
    push    rdi
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 40
    mov     r12d, [bx_x0]
    sub     r12d, [bx_y1]               ; c — колонка от угла тайла
    mov     r13d, [bx_x1]
    sub     r13d, [bx_y0]               ; за последней
    mov     eax, [bx_z0]
    add     eax, [bx_h]
    mov     [bx_zt], eax
.col:
    cmp     r12d, r13d
    jge     .done
    mov     ebx, [bk_sx]
    add     ebx, r12d
    cmp     ebx, SCR_W
    jae     .next
    test    dword [bx_fc], 4
    jz      .sides
    ; ----- верх: строки между краями ромба (·2: 2sy + max(2X0 − c, c + 2Y0) ..
    ; 2sy + min(c + 2Y1, 2X1 − c)), поднятые на zt
    mov     eax, [bx_x0]
    add     eax, eax
    sub     eax, r12d
    mov     ecx, [bx_y0]
    add     ecx, ecx
    add     ecx, r12d
    cmp     eax, ecx
    cmovl   eax, ecx
    mov     ecx, [bx_y1]
    add     ecx, ecx
    add     ecx, r12d
    mov     edx, [bx_x1]
    add     edx, edx
    sub     edx, r12d
    cmp     ecx, edx
    cmovg   ecx, edx
    mov     edx, [bk_sy]
    sub     edx, [bx_zt]
    add     edx, edx
    add     eax, edx
    sar     eax, 1
    add     ecx, edx
    sar     ecx, 1
    cmp     eax, [band_y0]
    jge     .t0
    mov     eax, [band_y0]
.t0:
    cmp     ecx, [band_y1]
    jle     .t1
    mov     ecx, [band_y1]
.t1:
    mov     esi, eax
    mov     r14d, ecx
.trow:
    cmp     esi, r14d
    jge     .sides
    ; X + Y = 2(r − sy + zt) + 1, X − Y = c (1/16) -> X, Y (1/256 от угла)
    mov     eax, esi
    sub     eax, [bk_sy]
    add     eax, [bx_zt]
    lea     eax, [rax*2 + 1]
    lea     edx, [rax + r12]
    shl     edx, 3
    mov     r8d, eax
    sub     r8d, r12d
    shl     r8d, 3
    mov     [bx_px], edx
    mov     [bx_py], r8d
    mov     dword [sh_n], N_UP
    mov     ecx, 2
    call    [bx_sh]
    test    eax, eax
    jz      .tn
    imul    r8d, esi, SCR_W
    add     r8d, ebx
    lea     r9, [albedo]
    mov     [r9 + r8*4], eax
    mov     eax, [bk_tx]
    shl     eax, 8
    add     eax, [bx_px]
    mov     ecx, [bk_ty]
    shl     ecx, 8
    add     ecx, [bx_py]
    shl     ecx, 16
    or      eax, ecx
    lea     r9, [gposb]
    mov     [r9 + r8*4], eax
    mov     eax, [sh_n]
    lea     r9, [nbuf]
    mov     [r9 + r8], al
.tn:
    inc     esi
    jmp     .trow
.sides:
    mov     eax, [bx_x1]
    sub     eax, [bx_y1]                ; с колонки X1 − Y1 — правая грань
    cmp     r12d, eax
    jge     .right
    test    dword [bx_fc], 1
    jz      .next
    ; ----- грань +y (левая): низ ·2 — 2(sy − z0) + c + 2Y1, u = c − (X0 − Y1)
    mov     ecx, [bx_y1]
    add     ecx, ecx
    add     ecx, r12d
    mov     edx, r12d
    sub     edx, [bx_x0]
    add     edx, [bx_y1]
    mov     eax, [bx_y1]
    add     eax, r12d
    shl     eax, 4
    add     eax, 8                      ; gx
    mov     r9d, [bx_y1]
    shl     r9d, 4
    add     r9d, 32                     ; gy — чуть перед гранью (как у сруба)
    xor     r10d, r10d
    jmp     .face
.right:
    test    dword [bx_fc], 2
    jz      .next
    ; ----- грань +x (правая): низ ·2 — 2(sy − z0) + 2X1 − c, u = c − (X1 − Y1)
    mov     edx, r12d
    sub     edx, eax
    mov     ecx, [bx_x1]
    add     ecx, ecx
    sub     ecx, r12d
    mov     eax, [bx_x1]
    shl     eax, 4
    add     eax, 32
    mov     r9d, [bx_x1]
    sub     r9d, r12d
    shl     r9d, 4
    sub     r9d, 8
    mov     r10d, 1
.face:
    mov     [bx_u], edx
    mov     [bx_face], r10d
    mov     edx, [bk_tx]
    shl     edx, 8
    add     eax, edx
    mov     edx, [bk_ty]
    shl     edx, 8
    add     r9d, edx
    shl     r9d, 16
    or      eax, r9d
    mov     [bx_gp], eax
    mov     edx, [bk_sy]
    sub     edx, [bx_z0]
    add     edx, edx
    add     ecx, edx                    ; низ ·2
    mov     eax, ecx
    sar     eax, 1                      ; за нижней строкой
    mov     r15d, eax
    mov     edx, [bx_h]
    add     edx, edx
    sub     ecx, edx
    sar     ecx, 1                      ; верхняя
    cmp     ecx, [band_y0]
    jge     .f0
    mov     ecx, [band_y0]
.f0:
    cmp     eax, [band_y1]
    jle     .f1
    mov     eax, [band_y1]
.f1:
    mov     esi, ecx
    mov     r14d, eax
.frow:
    cmp     esi, r14d
    jge     .next
    mov     eax, [bx_face]
    add     eax, N_FY
    mov     [sh_n], eax
    mov     ecx, [bx_face]
    mov     edx, [bx_u]
    lea     r8d, [r15d - 1]
    sub     r8d, esi                    ; высота над низом
    call    [bx_sh]
    test    eax, eax
    jz      .fn
    imul    r8d, esi, SCR_W
    add     r8d, ebx
    lea     r9, [albedo]
    mov     [r9 + r8*4], eax
    mov     eax, [bx_gp]
    lea     r9, [gposb]
    mov     [r9 + r8*4], eax
    mov     eax, [sh_n]
    lea     r9, [nbuf]
    mov     [r9 + r8], al
.fn:
    inc     esi
    jmp     .frow
.next:
    inc     r12d
    jmp     .col
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

; eax = 0xRRGGBB · ecx / 8 по каналам (ecx 0..8) -> eax
fu_scl:
    push    rdx
    mov     edx, eax
    and     eax, 0xFF00FF
    and     edx, 0x00FF00
    imul    eax, ecx
    imul    edx, ecx
    shr     eax, 3
    shr     edx, 3
    and     eax, 0xFF00FF
    and     edx, 0x00FF00
    or      eax, edx
    pop     rdx
    ret

; ================================================================ мебель
; коробка мебели в её системе: ecx = u0, edx = v0, r8d = u1, r9d = v1 (1/16 тайла; u —
; вдоль стены за спиной, v — от неё), fo_z0, fo_h, шейдер fo_sh; поворот fo_d
fbox:
    sub     rsp, 40
    mov     eax, r8d
    sub     eax, ecx
    mov     [fo_flen], eax              ; лицо — вдоль u
    mov     eax, r9d
    sub     eax, edx
    mov     [fo_slen], eax              ; бок — вдоль v
    mov     eax, [fo_d]
    cmp     eax, 1
    jb      .d0
    je      .d1
    cmp     eax, 2
    je      .d2
    ; спиной к +x: X = 16 − v, Y = u
    mov     eax, 16
    sub     eax, r9d
    mov     [bx_x0], eax
    mov     eax, 16
    sub     eax, edx
    mov     [bx_x1], eax
    mov     [bx_y0], ecx
    mov     [bx_y1], r8d
    jmp     .go
.d0:
    ; к −y: X = u, Y = v
    mov     [bx_x0], ecx
    mov     [bx_x1], r8d
    mov     [bx_y0], edx
    mov     [bx_y1], r9d
    jmp     .go
.d1:
    ; к −x: X = v, Y = 16 − u
    mov     [bx_x0], edx
    mov     [bx_x1], r9d
    mov     eax, 16
    sub     eax, r8d
    mov     [bx_y0], eax
    mov     eax, 16
    sub     eax, ecx
    mov     [bx_y1], eax
    jmp     .go
.d2:
    ; к +y: X = 16 − u, Y = 16 − v
    mov     eax, 16
    sub     eax, r8d
    mov     [bx_x0], eax
    mov     eax, 16
    sub     eax, ecx
    mov     [bx_x1], eax
    mov     eax, 16
    sub     eax, r9d
    mov     [bx_y0], eax
    mov     eax, 16
    sub     eax, edx
    mov     [bx_y1], eax
.go:
    mov     eax, [fo_z0]
    mov     [bx_z0], eax
    mov     eax, [fo_h]
    mov     [bx_h], eax
    mov     dword [bx_fc], 7
    lea     rax, [fo_wrap]
    mov     [bx_sh], rax
    call    draw_box
    add     rsp, 40
    ret

; draw_box -> шейдер мебели: ecx = часть (0 лицо, 1 бок, 2 спина, 3 верх), edx = u на
; грани (px) или u верха, r8d = высота или v верха (1/256, в системе мебели); мир
; верха — fo_wx, fo_wy
fo_wrap:
    cmp     ecx, 2
    je      .top
    mov     eax, [fo_d]
    lea     r9, [fo_pm]
    lea     r9, [r9 + rax*2]
    movzx   ecx, byte [r9 + rcx]
    jmp     [fo_sh]
.top:
    mov     eax, [bk_tx]
    shl     eax, 8
    add     eax, edx
    mov     [fo_wx], eax
    mov     eax, [bk_ty]
    shl     eax, 8
    add     eax, r8d
    mov     [fo_wy], eax
    mov     eax, [fo_d]
    cmp     eax, 1
    jb      .t
    je      .t1
    cmp     eax, 2
    je      .t2
    mov     eax, 256                    ; u = Y, v = 256 − X
    sub     eax, edx
    mov     edx, r8d
    mov     r8d, eax
    jmp     .t
.t1:
    mov     eax, 256                    ; u = 256 − Y, v = X
    sub     eax, r8d
    mov     r8d, edx
    mov     edx, eax
    jmp     .t
.t2:
    mov     eax, 256                    ; u = 256 − X, v = 256 − Y
    sub     eax, edx
    mov     edx, eax
    mov     eax, 256
    sub     eax, r8d
    mov     r8d, eax
.t:
    mov     ecx, 3
    jmp     [fo_sh]

; тайл мебели (draw_tile, виды D_BED .. D_LAVKA): только у избы с видным нутром
furn_tile:
    push    rbx
    push    rsi
    sub     rsp, 40
    mov     eax, [bk_ty]
    shl     eax, 7
    add     eax, [bk_tx]
    lea     rdx, [rmap]
    movzx   ecx, byte [rdx + rax]
    test    ecx, ecx
    jz      .out
    dec     ecx
    imul    ecx, ecx, HS_SZ
    lea     rbx, [houses]
    add     rbx, rcx
    cmp     dword [rbx + HS_IVIS], 0
    je      .out
    mov     [fo_hp], rbx
    lea     rdx, [hflags]
    movzx   ecx, byte [rdx + rax]
    mov     [fo_fl], ecx
    shr     ecx, 2
    and     ecx, 3
    mov     [fo_d], ecx
    mov     eax, [rbx + HS_WOOD]
    mov     [fo_wood], eax
    mov     eax, [bk_kind]
    cmp     eax, D_BED
    je      .bed
    cmp     eax, D_CUPB
    je      .cupb
    cmp     eax, D_CHEST
    je      .chest
    ; ----- лавка: две ножки, сиденье
    lea     rax, [fu_lavka]
    mov     [fo_sh], rax
    mov     dword [fo_box], 0
    mov     dword [fo_z0], 0
    mov     dword [fo_h], 5
    mov     ecx, 2
    mov     edx, 2
    mov     r8d, 4
    mov     r9d, 5
    call    fbox
    mov     ecx, 12
    mov     edx, 2
    mov     r8d, 14
    mov     r9d, 5
    call    fbox
    mov     dword [fo_box], 1
    mov     dword [fo_z0], 5
    mov     dword [fo_h], 2
    xor     ecx, ecx
    mov     edx, 1
    mov     r8d, 16
    mov     r9d, 6
    call    fbox
    jmp     .out
.chest:
    ; ----- сундук: короб, крышка — закрытая сверху или откинутая к стене
    lea     rax, [fu_chest]
    mov     [fo_sh], rax
    mov     eax, [rbx + HS_CHPAL]
    mov     [fo_col], eax
    mov     dword [fo_box], 0
    mov     dword [fo_z0], 0
    mov     dword [fo_h], 8
    mov     ecx, 2
    mov     edx, 1
    mov     r8d, 14
    mov     r9d, 10
    call    fbox
    mov     dword [fo_z0], 8
    test    dword [fo_fl], FM_OPEN
    jnz     .chopen
    mov     dword [fo_box], 1
    mov     dword [fo_h], 3
    mov     ecx, 2
    mov     edx, 1
    mov     r8d, 14
    mov     r9d, 10
    call    fbox
    jmp     .out
.chopen:
    mov     dword [fo_box], 2
    mov     dword [fo_h], 10
    mov     ecx, 2
    mov     edx, 0
    mov     r8d, 14
    mov     r9d, 1
    call    fbox
    jmp     .out
.cupb:
    ; ----- шкаф (две двери) или буфет (низ с дверцами, верх под стеклом с посудой);
    ; перед игроком — срезан, как печь
    lea     rax, [fu_cupb]
    mov     [fo_sh], rax
    mov     dword [fo_z0], 0
    call    .occl
    mov     esi, eax
    test    dword [fo_fl], FM_ALT
    jnz     .buffet
    mov     dword [fo_box], 0
    mov     dword [fo_h], 30
    test    esi, esi
    jz      .cw
    mov     dword [fo_h], 5
.cw:
    mov     ecx, 1
    xor     edx, edx
    mov     r8d, 15
    mov     r9d, 9
    call    fbox
    jmp     .out
.buffet:
    mov     dword [fo_box], 1
    mov     dword [fo_h], 13
    test    esi, esi
    jz      .bw
    mov     dword [fo_h], 5
.bw:
    mov     ecx, 1
    xor     edx, edx
    mov     r8d, 15
    mov     r9d, 9
    call    fbox
    test    esi, esi
    jnz     .out
    mov     dword [fo_box], 2
    mov     dword [fo_z0], 13
    mov     dword [fo_h], 17
    mov     ecx, 1
    xor     edx, edx
    mov     r8d, 15
    mov     r9d, 6
    call    fbox
    jmp     .out
.bed:
    ; ----- кровать: спинка на конце (к паре — FM_ALT: конец у u = 0), рама с одеялом,
    ; на изголовье — подушка. Спинка у дальнего от глаза конца — прежде рамы
    lea     rax, [fu_bed]
    mov     [fo_sh], rax
    mov     eax, [rbx + HS_BLNK]
    shl     eax, 2
    mov     [fo_pal], eax               ; номер первого цвета палитры
    ; конец у u = 0 (FM_ALT) ближе к глазу при повороте 1 и 2
    xor     esi, esi                    ; 1 — спинка ближе рамы
    mov     eax, [fo_d]
    lea     ecx, [rax - 1]
    cmp     ecx, 1
    setbe   cl
    movzx   ecx, cl                     ; ближний конец — u = 0
    mov     edx, [fo_fl]
    shr     edx, 5
    and     edx, 1                      ; спинка у u = 0
    cmp     ecx, edx
    sete    sil
    test    esi, esi
    jnz     .bframe
    call    .board
.bframe:
    mov     dword [fo_box], 0
    mov     dword [fo_z0], 0
    mov     dword [fo_h], 10
    xor     ecx, ecx
    mov     r8d, 14
    test    dword [fo_fl], FM_ALT
    jz      .bf1
    mov     ecx, 2
    mov     r8d, 16
.bf1:
    mov     edx, 1
    mov     r9d, 15
    call    fbox
    test    dword [fo_fl], FM_HEAD
    jz      .bb
    ; подушка — у спинки
    mov     dword [fo_box], 2
    mov     dword [fo_z0], 10
    mov     dword [fo_h], 2
    mov     ecx, 9
    mov     r8d, 14
    test    dword [fo_fl], FM_ALT
    jz      .bp
    mov     ecx, 2
    mov     r8d, 7
.bp:
    mov     edx, 3
    mov     r9d, 13
    call    fbox
.bb:
    test    esi, esi
    jz      .out
    call    .board
.out:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret
; спинка: изголовье выше, изножье ниже
.board:
    sub     rsp, 8
    mov     dword [fo_box], 1
    mov     dword [fo_z0], 0
    mov     dword [fo_h], 13
    test    dword [fo_fl], FM_HEAD
    jz      .bd1
    mov     dword [fo_h], 18
.bd1:
    mov     ecx, 14
    mov     r8d, 16
    test    dword [fo_fl], FM_ALT
    jz      .bd2
    xor     ecx, ecx
    mov     r8d, 2
.bd2:
    mov     edx, 1
    mov     r9d, 15
    call    fbox
    add     rsp, 8
    ret
; eax = 1, если тайл перед игроком и рядом по экрану (как draw_tile.occl)
.occl:
    mov     eax, [bk_tx]
    add     eax, [bk_ty]
    sub     eax, [pdiag]
    cmp     eax, 1
    jl      .oc0
    cmp     eax, 6
    jg      .oc0
    mov     eax, [bk_sx]
    sub     eax, [psx]
    cdq
    xor     eax, edx
    sub     eax, edx
    cmp     eax, 56
    jg      .oc0
    mov     eax, 1
    ret
.oc0:
    xor     eax, eax
    ret

; ----- шейдеры мебели: ecx = часть (0 лицо, 1 бок, 2 спина, 3 верх), edx = u / u верха,
; r8d = высота / v верха; fo_box — какая коробка -> eax (0 — прозрачно)

; кровать: 0 — рама с одеялом, 1 — спинка, 2 — подушка
fu_bed:
    mov     eax, [fo_box]
    cmp     eax, 1
    je      .board
    ja      .pillow
    cmp     ecx, 3
    je      .blanket
    ; рама: низ в тени, брус, одеяло свисает
    cmp     r8d, 3
    jae     .fr1
    mov     eax, 0x01181410
    ret
.fr1:
    cmp     r8d, 6
    jae     .drape
    mov     eax, [fo_wood]
    mov     ecx, 7
    cmp     r8d, 5
    jne     .fr2
    mov     ecx, 8
.fr2:
    call    fu_scl
    or      eax, 0x01000000
    ret
.drape:
    ; свес одеяла: лоскуты по 4 px, нижний край — подвёрнут
    shr     edx, 2
    mov     ecx, r8d
    shr     ecx, 2
    add     edx, ecx
    and     edx, 3
    mov     eax, [fo_pal]
    add     eax, edx
    lea     rcx, [ir_blnk]
    mov     eax, [rcx + rax*4]
    cmp     r8d, 6
    jne     .dr1
    mov     ecx, 5
    call    fu_scl
.dr1:
    or      eax, 0x01000000
    ret
.blanket:
    ; одеяло лоскутами по четверти тайла (мир — стык пары без шва); у изголовья —
    ; отвёрнутая простыня
    test    dword [fo_fl], FM_HEAD
    jz      .bl1
    mov     eax, edx
    test    dword [fo_fl], FM_ALT
    jnz     .bl0
    mov     eax, 256
    sub     eax, edx
.bl0:
    cmp     eax, 128                    ; от спинки: подушка и простыня
    jae     .bl1
    cmp     eax, 104
    jb      .sheet
    mov     eax, 0x01D8D8D0
    ret
.sheet:
    mov     eax, 0x01E6E6E0
    ret
.bl1:
    mov     eax, [fo_wx]
    shr     eax, 6
    mov     ecx, [fo_wy]
    shr     ecx, 6
    add     eax, ecx
    and     eax, 3
    add     eax, [fo_pal]
    lea     rcx, [ir_blnk]
    mov     eax, [rcx + rax*4]
    ; шов между лоскутами — темнее
    mov     ecx, [fo_wx]
    and     ecx, 63
    cmp     ecx, 6
    jb      .seam
    mov     ecx, [fo_wy]
    and     ecx, 63
    cmp     ecx, 6
    jae     .bl2
.seam:
    mov     ecx, 6
    call    fu_scl
.bl2:
    or      eax, 0x01000000
    ret
.pillow:
    mov     eax, 0x01ECECE6
    cmp     ecx, 3
    jne     .pl1
    ; складка посередине
    mov     ecx, r8d
    sub     ecx, 128
    cmp     ecx, 12
    jae     .pl2
    mov     eax, 0x01D4D4CE
.pl2:
    ret
.pl1:
    mov     eax, 0x01D8D8D2
    ret
.board:
    ; спинка поперёк кровати: широкая грань — «бок» (1), узкие — лицо и спина.
    ; Деревянная — щит с рамкой; железная — прутья с перекладинами
    test    dword [fo_fl], FM_IRON
    jnz     .iron
    mov     eax, [fo_wood]
    cmp     ecx, 3
    je      .bw1
    cmp     r8d, 2
    jb      .bw0
    cmp     ecx, 1
    jne     .bw6
    ; щит: рамка светлее, филёнка темнее
    mov     r9d, [fo_h]
    dec     r9d
    cmp     r8d, r9d
    je      .bw1
    test    edx, edx
    jz      .bw1
    mov     r9d, [fo_slen]
    dec     r9d
    cmp     edx, r9d
    je      .bw1
    mov     ecx, 7
    call    fu_scl
    or      eax, 0x01000000
    ret
.bw6:
    mov     ecx, 6
    call    fu_scl
    or      eax, 0x01000000
    ret
.bw0:
    mov     ecx, 5
    call    fu_scl
.bw1:
    or      eax, 0x01000000
    ret
.iron:
    mov     eax, 0x015A6E80
    cmp     ecx, 3
    je      .ir9
    cmp     ecx, 1
    jne     .ir9                        ; узкие грани — стойки
    mov     r9d, [fo_h]
    lea     r10d, [r9d - 2]
    cmp     r8d, r10d
    jae     .ir9                        ; верхняя перекладина
    cmp     r8d, 7
    je      .ir9
    cmp     r8d, 8
    je      .ir9                        ; нижняя
    test    edx, edx
    jz      .ir9
    mov     r9d, [fo_slen]
    dec     r9d
    cmp     edx, r9d
    je      .ir9                        ; стойки по краям
    ; прутья: через 3 px
    mov     eax, edx
    xor     edx, edx
    mov     r9d, 3
    div     r9d
    mov     eax, 0x015A6E80
    test    edx, edx
    jz      .ir9
    xor     eax, eax
.ir9:
    ret

; шкаф и буфет: 0 — шкаф, 1 — низ буфета, 2 — верх буфета под стеклом
fu_cupb:
    mov     r9d, [fo_box]
    cmp     ecx, 3
    je      .top
    cmp     ecx, 0
    jne     .side
    mov     r10d, [fo_flen]
    dec     r10d                        ; последний столбец лица
    cmp     r9d, 2
    je      .glass
    ; ----- лицо с дверцами: цоколь, карниз, рамка, шов посередине, филёнки, ручки
    cmp     r8d, 2
    jb      .dark
    mov     r11d, [fo_h]
    lea     eax, [r11d - 2]
    cmp     r8d, eax
    jae     .light
    test    edx, edx
    jz      .dark6
    cmp     edx, r10d
    je      .dark6
    mov     eax, r10d
    shr     eax, 1
    cmp     edx, eax
    je      .dark5                      ; шов между дверцами
    ; ручки у шва
    lea     ecx, [eax - 1]
    cmp     edx, ecx
    je      .hdl
    lea     ecx, [eax + 1]
    cmp     edx, ecx
    je      .hdl
    jmp     .panel
.hdl:
    mov     ecx, r11d
    shr     ecx, 1
    cmp     r8d, ecx
    jb      .panel
    add     ecx, 2
    cmp     r8d, ecx
    ja      .panel
    mov     eax, 0x01C8A050
    ret
.panel:
    ; филёнка: рамка в 2 px от краёв дверцы
    mov     eax, r10d
    shr     eax, 1                      ; шов
    mov     ecx, edx
    cmp     edx, eax
    jb      .pa
    sub     ecx, eax                    ; от шва
.pa:
    cmp     ecx, 2
    je      .dark7
    mov     r9d, eax
    sub     r9d, 2
    cmp     ecx, r9d
    je      .dark7
    cmp     r8d, 4
    je      .dark7
    lea     eax, [r11d - 5]
    cmp     r8d, eax
    je      .dark7
    mov     eax, [fo_wood]
    or      eax, 0x01000000
    ret
.glass:
    ; ----- верх буфета: рама, за стеклом полки с тарелками, чашками, рюмками
    mov     r11d, [fo_h]
    lea     eax, [r11d - 2]
    cmp     r8d, eax
    jae     .light
    cmp     r8d, 1
    jb      .dark
    test    edx, edx
    jz      .dark6
    cmp     edx, r10d
    je      .dark6
    mov     eax, r10d
    shr     eax, 1
    cmp     edx, eax
    je      .dark6
    ; полки
    cmp     r8d, 5
    je      .dark7
    cmp     r8d, 10
    je      .dark7
    cmp     r8d, 5
    jb      .cups
    cmp     r8d, 10
    jb      .plates
    ; верхняя полка: рюмки через пиксель
    cmp     r8d, 13
    jae     .behind
    test    edx, 1
    jz      .behind
    mov     eax, 0x419AB4C8
    cmp     r8d, 12
    je      .gl1
    mov     eax, 0x017A94A8
.gl1:
    ret
.plates:
    ; тарелки стоят: круг по 3 px
    mov     eax, edx
    xor     edx, edx
    mov     ecx, 3
    div     ecx
    test    edx, edx
    jz      .behind
    cmp     r8d, 6
    je      .rim
    cmp     r8d, 9
    je      .rim
    mov     eax, 0x01E0E2EA
    ret
.rim:
    mov     eax, 0x013A5AA0
    ret
.cups:
    cmp     r8d, 3
    ja      .behind
    test    edx, 1
    jz      .behind
    mov     eax, 0x01B03A30
    cmp     r8d, 3
    jne     .cp1
    mov     eax, 0x01E8E0D8
.cp1:
    ret
.behind:
    mov     eax, 0x01241C18
    ret
.side:
    ; бок и спина — гладкие, темнее
    mov     eax, [fo_wood]
    mov     ecx, 6
    cmp     r8d, 2
    jae     .sd
    mov     ecx, 4
.sd:
    call    fu_scl
    or      eax, 0x01000000
    ret
.top:
    ; верх: у буфета — столешница светлее (её видно перед верхом)
    mov     eax, [fo_wood]
    mov     ecx, 8
    cmp     r9d, 1
    jne     .tp
    mov     eax, [fo_wood]
    add     eax, 0x101008
.tp:
    or      eax, 0x01000000
    ret
.light:
    mov     eax, [fo_wood]
    add     eax, 0x0C0A06
    or      eax, 0x01000000
    ret
.dark:
    mov     ecx, 4
    jmp     .dk
.dark5:
    mov     ecx, 5
    jmp     .dk
.dark6:
    mov     ecx, 6
    jmp     .dk
.dark7:
    mov     ecx, 7
.dk:
    mov     eax, [fo_wood]
    call    fu_scl
    or      eax, 0x01000000
    ret

; сундук: 0 — короб, 1 — крышка закрытая, 2 — откинутая
fu_chest:
    mov     r9d, [fo_box]
    cmp     r9d, 2
    je      .lid
    cmp     ecx, 3
    je      .top
    ; грани: краска, железные полосы, у короба — замок и нижний край
    cmp     ecx, 1
    jne     .fr
    ; бок: полоса посередине
    mov     eax, [fo_slen]
    shr     eax, 1
    cmp     edx, eax
    je      .iron
    jmp     .paint6
.fr:
    mov     r10d, [fo_flen]
    cmp     edx, 1
    je      .iron
    lea     eax, [r10d - 2]
    cmp     edx, eax
    je      .iron
    test    r9d, r9d
    jnz     .paint7                     ; крышка спереди
    test    r8d, r8d
    jz      .dark
    cmp     r8d, 6
    je      .iron
    ; замок: посередине, 2x3
    cmp     ecx, 0
    jne     .paint
    mov     eax, r10d
    shr     eax, 1
    lea     ecx, [eax - 1]
    cmp     edx, ecx
    jb      .paint
    cmp     edx, eax
    ja      .paint
    cmp     r8d, 2
    jb      .paint
    cmp     r8d, 4
    ja      .paint
    mov     eax, 0x41C8A040
    cmp     r8d, 3
    jne     .lk
    mov     eax, 0x01302418             ; скважина
.lk:
    ret
.top:
    test    r9d, r9d
    jnz     .ltop
    ; короб сверху: открыт — нутро тёмное, по краю — стенки
    mov     eax, edx
    sub     eax, 2*16 + 16
    cmp     eax, 12*16 - 32
    jae     .rim
    mov     eax, r8d
    sub     eax, 1*16 + 16
    cmp     eax, 9*16 - 32
    jae     .rim
    mov     eax, 0x012A1C14
    ret
.rim:
    mov     eax, [fo_col]
    mov     ecx, 6
    call    fu_scl
    or      eax, 0x01000000
    ret
.ltop:
    ; крышка сверху: полосы там же, где на лице; середина чуть светлее — горбом
    mov     eax, edx
    shr     eax, 4
    cmp     eax, 3
    je      .iron
    cmp     eax, 12
    je      .iron
    mov     eax, r8d
    sub     eax, 88
    cmp     eax, 32
    jb      .paint9
    jmp     .paint
.lid:
    ; откинутая крышка: лицом к комнате — изнанка (доски), верх — кромка
    cmp     ecx, 3
    je      .paint
    cmp     ecx, 0
    jne     .paint6
    mov     eax, [fo_wood]
    cmp     r8d, 1
    jb      .lw
    mov     ecx, [fo_h]
    sub     ecx, 2
    cmp     r8d, ecx
    ja      .lw
    add     eax, 0x0C0A06
.lw:
    or      eax, 0x01000000
    ret
.iron:
    mov     eax, 0x014A4A52
    ret
.dark:
    mov     eax, 0x01201A16
    ret
.paint9:
    mov     eax, [fo_col]
    add     eax, 0x0C0C0C
    or      eax, 0x01000000
    ret
.paint:
    mov     eax, [fo_col]
    or      eax, 0x01000000
    ret
.paint7:
    mov     eax, [fo_col]
    mov     ecx, 7
    jmp     .ps
.paint6:
    mov     eax, [fo_col]
    mov     ecx, 6
.ps:
    call    fu_scl
    or      eax, 0x01000000
    ret

; лавка: 0 — ножки, 1 — сиденье (две доски вдоль)
fu_lavka:
    mov     eax, [fo_wood]
    cmp     dword [fo_box], 0
    jne     .seat
    mov     ecx, 5
    call    fu_scl
    or      eax, 0x01000000
    ret
.seat:
    cmp     ecx, 3
    jne     .edge
    mov     ecx, r8d
    sub     ecx, 56
    cmp     ecx, 12
    jae     .s1
    mov     ecx, 6
    call    fu_scl
.s1:
    add     eax, 0x080604
    or      eax, 0x01000000
    ret
.edge:
    mov     ecx, 7
    call    fu_scl
    or      eax, 0x01000000
    ret

; ================================================================ на столе
; телевизор (в избе смотрят — hflags бит 1), иначе самовар или кринка с геранью
table_top:
    push    rbx
    sub     rsp, 32
    mov     eax, [bk_hid]
    dec     eax
    imul    eax, eax, HS_SZ
    lea     rbx, [houses]
    add     rbx, rax
    mov     eax, [bk_ty]
    shl     eax, 7
    add     eax, [bk_tx]
    lea     rdx, [hflags]
    test    byte [rdx + rax], 2
    jz      .notv
    ; экран — к комнате: на грань, за которой пол избы
    mov     dword [fo_scr], 2
    lea     rdx, [tmap]
    lea     r8, [tprop]
    movzx   ecx, byte [rdx + rax + 1]
    test    byte [r8 + rcx], 4
    jz      .s1
    mov     dword [fo_scr], 1
.s1:
    movzx   ecx, byte [rdx + rax + MAPW]
    test    byte [r8 + rcx], 4
    jz      .s2
    mov     dword [fo_scr], 0
.s2:
    mov     dword [bx_x0], 4
    mov     dword [bx_x1], 12
    mov     dword [bx_y0], 5
    mov     dword [bx_y1], 11
    mov     dword [bx_z0], 8
    mov     dword [bx_h], 10
    mov     dword [bx_fc], 7
    lea     rax, [fu_tv]
    mov     [bx_sh], rax
    call    draw_box
    jmp     .out
.notv:
    mov     eax, [rbx + HS_ONTAB]
    cmp     eax, 1
    jb      .out
    ja      .pot
    ; самовар: тулово, горловина, чайник сверху
    lea     rax, [fu_samo]
    mov     [bx_sh], rax
    mov     dword [bx_fc], 7
    mov     dword [fo_box], 0
    mov     dword [bx_x0], 6
    mov     dword [bx_x1], 10
    mov     dword [bx_y0], 6
    mov     dword [bx_y1], 10
    mov     dword [bx_z0], 8
    mov     dword [bx_h], 7
    call    draw_box
    mov     dword [fo_box], 1
    mov     dword [bx_x0], 7
    mov     dword [bx_x1], 9
    mov     dword [bx_y0], 7
    mov     dword [bx_y1], 9
    mov     dword [bx_z0], 15
    mov     dword [bx_h], 1
    call    draw_box
    mov     dword [fo_box], 2
    mov     dword [bx_x0], 7
    mov     dword [bx_x1], 10
    mov     dword [bx_y0], 7
    mov     dword [bx_y1], 10
    mov     dword [bx_z0], 16
    mov     dword [bx_h], 3
    call    draw_box
    jmp     .out
.pot:
    ; кринка с геранью
    lea     rax, [fu_pot]
    mov     [bx_sh], rax
    mov     dword [bx_fc], 7
    mov     dword [fo_box], 0
    mov     dword [bx_x0], 7
    mov     dword [bx_x1], 10
    mov     dword [bx_y0], 7
    mov     dword [bx_y1], 10
    mov     dword [bx_z0], 8
    mov     dword [bx_h], 4
    call    draw_box
    mov     dword [fo_box], 1
    mov     dword [bx_x0], 6
    mov     dword [bx_x1], 11
    mov     dword [bx_y0], 6
    mov     dword [bx_y1], 11
    mov     dword [bx_z0], 12
    mov     dword [bx_h], 4
    call    draw_box
.out:
    add     rsp, 32
    pop     rbx
    ret

; телевизор: ecx = грань (0 левая, 1 правая, 2 верх); экран светит (tv_pane) на грани fo_scr
fu_tv:
    cmp     ecx, 2
    je      .case
    cmp     ecx, [fo_scr]
    jne     .back
    ; длина лица: левая — по X (8), правая — по Y (6)
    mov     r9d, 8
    test    ecx, ecx
    jz      .l
    mov     r9d, 6
.l:
    cmp     r8d, 2
    jb      .base
    cmp     r8d, 9
    jae     .case
    lea     eax, [r9d - 3]
    cmp     edx, eax
    jae     .panel
    test    edx, edx
    jz      .case
    cmp     r8d, 8
    je      .case
    ; экран: по углам — темнее
    mov     eax, [tv_pane]
    cmp     edx, 1
    je      .corner
    lea     ecx, [r9d - 4]
    cmp     edx, ecx
    jne     .scr
.corner:
    cmp     r8d, 2
    je      .dim
    cmp     r8d, 7
    jne     .scr
.dim:
    mov     ecx, eax
    shr     ecx, 2
    and     ecx, 0x3F3F3F
    sub     eax, ecx
.scr:
    ret
.panel:
    ; ручки справа
    cmp     r8d, 6
    je      .knob
    cmp     r8d, 4
    je      .knob
    jmp     .case
.knob:
    lea     eax, [r9d - 2]
    cmp     edx, eax
    jne     .case
    mov     eax, 0x01C8C4B8
    ret
.base:
    mov     eax, 0x01302418
    ret
.back:
    ; задник: пластмасса с прорезями
    mov     eax, 0x01403020
    cmp     r8d, 5
    jb      .b1
    cmp     r8d, 8
    ja      .b1
    test    edx, 1
    jnz     .b1
    mov     eax, 0x01201810
.b1:
    ret
.case:
    mov     eax, 0x01583A22
    ret

; самовар: 0 — тулово (латунь, блик, пояс), 1 — горловина, 2 — чайник с горошком
fu_samo:
    mov     r9d, [fo_box]
    cmp     r9d, 1
    je      .neck
    ja      .pot
    cmp     ecx, 2
    je      .ttop
    cmp     r8d, 2
    jb      .foot
    cmp     r8d, 5
    je      .band
    cmp     edx, 1
    je      .hi
    cmp     edx, 3
    je      .lo
    mov     eax, 0x01B88A36
    ret
.hi:
    mov     eax, 0x41F0D080
    ret
.lo:
    mov     eax, 0x01805E24
    ret
.band:
    mov     eax, 0x018A6A2A
    ret
.foot:
    mov     eax, 0x016A4A1E
    ret
.ttop:
    mov     eax, 0x01A07830
    ret
.neck:
    mov     eax, 0x017A5A22
    ret
.pot:
    mov     eax, 0x01E8E8E6
    cmp     ecx, 2
    je      .pt
    lea     eax, [edx + r8d]
    xor     edx, edx
    mov     ecx, 3
    div     ecx
    mov     eax, 0x01E8E8E6
    test    edx, edx
    jnz     .pt
    mov     eax, 0x013A5AA0
.pt:
    ret

; кринка с геранью: 0 — горшок, 1 — куст (листья с просветами, красные цветы)
fu_pot:
    cmp     dword [fo_box], 0
    jne     .leaf
    mov     eax, 0x01A0502E
    cmp     ecx, 2
    jne     .p1
    mov     eax, 0x013A2A1C
.p1:
    ret
.leaf:
    ; шум по пикселю грани или по месту верха
    mov     eax, edx
    mov     r9d, r8d
    cmp     ecx, 2
    jne     .l1
    shr     eax, 4
    shr     r9d, 4
.l1:
    imul    r10d, ecx, 7
    add     eax, r10d
    HASH    eax, r9d
    mov     r9d, eax
    and     r9d, 15
    cmp     r9d, 2
    jb      .fl
    cmp     ecx, 2
    je      .gr
    and     eax, 0x30
    jz      .none                       ; просвет
.gr:
    mov     eax, 0x013A6A2A
    test    r9d, 4
    jz      .g1
    mov     eax, 0x012A5020
.g1:
    ret
.fl:
    mov     eax, 0x01C83040
    ret
.none:
    xor     eax, eax
    ret

; ================================================================ стены изнутри
; внутренняя грань сруба (изба без крыши): ebx = u, esi = h, edi = грань -> eax — цвет
; (0 — брёвна, как снаружи), sh_n. Окно — изнутри: рама, ночь за стеклом, шторы; на
; глухой стене — убранство (bk_deco), под ним — отделка избы (HS_WALL)
wall_inner:
    push    r12
    push    r13
    mov     eax, [bk_hid]
    dec     eax
    imul    eax, eax, HS_SZ
    lea     r12, [houses]
    add     r12, rax
    lea     eax, [edi + N_FY]
    mov     [sh_n], eax
    cmp     dword [bk_cut], 0
    jne     .fin
    cmp     dword [bk_kind], D_WALL
    je      .deco
    cmp     edi, [bk_wface]
    jne     .fin
    call    .window
    test    eax, eax
    jnz     .out
    jmp     .fin
.deco:
    mov     eax, [bk_deco]
    test    eax, eax
    jz      .fin
    call    .decor
    test    eax, eax
    jnz     .out
.fin:
    call    .finish
.out:
    pop     r13
    pop     r12
    ret

; ----- окно изнутри (ebx = u, esi = h, edi = грань) -> eax, 0 — не окно
.window:
    mov     eax, ebx
    sub     eax, 3
    sub     eax, edi                    ; wu
    lea     ecx, [esi - 7]              ; wh
    cmp     ecx, -1
    jne     .wi1
    ; подоконник
    lea     edx, [eax + 1]
    cmp     edx, 10
    ja      .wno
    mov     eax, 0x01E0E4E8
    ret
.wi1:
    cmp     eax, 8
    ja      .wno
    cmp     ecx, 18
    ja      .wno
    ; рама, переплёт — белые
    test    eax, eax
    jz      .wwhite
    cmp     eax, 8
    je      .wwhite
    test    ecx, ecx
    jz      .wwhite
    cmp     ecx, 18
    je      .wwhite
    cmp     eax, 4
    je      .wwhite
    cmp     ecx, 9
    je      .wwhite
    ; ламбрекен поверху, шторы по краям — ткань избы, складками
    cmp     ecx, 15
    jae     .curt
    cmp     eax, 2
    jbe     .curt
    cmp     eax, 6
    jae     .curt
    ; на подоконнике — кот или герань (как снаружи, только освещены комнатой)
    cmp     dword [bk_kind], D_WINL
    jne     .glass
    lea     edx, [ecx - 2]
    cmp     edx, 4
    ja      .glass
    lea     r8d, [eax - 2]
    cmp     r8d, 4
    ja      .glass
    mov     r9d, [bk_seed]
    add     r9d, edi
    imul    r9d, r9d, 0x2C1B3C6D
    shr     r9d, 28
    cmp     r9d, 1
    jbe     .cat
    cmp     r9d, 5
    ja      .glass
    lea     r9, [ger_pot]
    movzx   r9d, byte [r9 + rdx]
    bt      r9d, r8d
    jnc     .g2
    mov     eax, 0x018A4A30
    ret
.g2:
    lea     r9, [ger_leaf]
    movzx   r9d, byte [r9 + rdx]
    bt      r9d, r8d
    jnc     .g3
    mov     eax, 0x013A6A2A
    ret
.g3:
    lea     r9, [ger_flow]
    movzx   r9d, byte [r9 + rdx]
    bt      r9d, r8d
    jnc     .glass
    mov     eax, 0x01C83040
    ret
.cat:
    lea     r9, [cat_rows]
    movzx   r9d, byte [r9 + rdx]
    bt      r9d, r8d
    jnc     .glass
    mov     eax, 0x013A3634
    ret
.glass:
    ; за стеклом ночь; блик наискось
    mov     edx, 18
    sub     edx, ecx
    add     edx, eax
    cmp     edx, 6
    je      .wrefl
    cmp     edx, 7
    je      .wrefl
    mov     eax, 0x010E1622
    ret
.wrefl:
    mov     eax, 0x01222E3C
    ret
.curt:
    mov     eax, [r12 + HS_CURT]
    test    esi, 1
    jz      .cu1
    mov     ecx, 7
    call    fu_scl
.cu1:
    or      eax, 0x01000000
    ret
.wwhite:
    mov     eax, 0x01E2E6EA
    ret
.wno:
    xor     eax, eax
    ret

; ----- убранство (bk_deco) -> eax, 0 — нет его в этом пикселе
.decor:
    cmp     eax, 3
    jb      .carpet
    je      .clock
    cmp     eax, 5
    jb      .icon
    je      .photo
    cmp     eax, 7
    jb      .calendar
    je      .mirror
    jmp     .shelf
.carpet:
    ; ковёр на две клетки: U = часть·16 + u, поле 2..29 × 9..32
    lea     edx, [eax - 1]
    shl     edx, 4
    add     edx, ebx                    ; U
    lea     eax, [edx - 2]
    cmp     eax, 27
    ja      .dno
    lea     eax, [esi - 9]
    cmp     eax, 23
    ja      .dno
    mov     eax, [r12 + HS_CARP]
    shl     eax, 4
    lea     r8, [ir_carp]
    add     r8, rax                     ; поле, узор, кайма, светлое
    ; от края ковра
    mov     eax, edx
    sub     eax, 2
    mov     ecx, 29
    sub     ecx, edx
    cmp     ecx, eax
    cmovl   eax, ecx
    mov     ecx, esi
    sub     ecx, 9
    cmp     ecx, eax
    cmovl   eax, ecx
    mov     ecx, 32
    sub     ecx, esi
    cmp     ecx, eax
    cmovl   eax, ecx                    ; до края
    cmp     eax, 2
    jae     .ca1
    ; кайма: внешний ряд тёмный, второй — в светлую точку
    mov     ecx, eax
    mov     eax, [r8 + 8]
    test    ecx, ecx
    jz      .cadone
    lea     ecx, [edx + esi]
    test    ecx, 1
    jnz     .cadone
    mov     eax, [r8 + 12]
    jmp     .cadone
.ca1:
    cmp     eax, 2
    jne     .ca2
    mov     eax, [r8 + 12]
    jmp     .cadone
.ca2:
    ; медальон: ромб от середины (15.5, 20.5), в полупикселях
    lea     eax, [edx*2 - 31]
    cdq
    xor     eax, edx
    sub     eax, edx
    lea     ecx, [esi*2 - 41]
    mov     r9d, ecx
    sar     r9d, 31
    xor     ecx, r9d
    sub     ecx, r9d
    add     eax, ecx
    cmp     eax, 8
    jb      .cak
    cmp     eax, 16
    jb      .cam
    cmp     eax, 19
    jb      .cal
    ; поле: по углам — узорные точки
    cmp     eax, 34
    jb      .caf
    lea     ecx, [ebx + esi]
    test    ecx, 3
    jnz     .caf
.cam:
    mov     eax, [r8 + 4]
    jmp     .cadone
.cak:
    mov     eax, [r8 + 8]
    jmp     .cadone
.cal:
    mov     eax, [r8 + 12]
    jmp     .cadone
.caf:
    mov     eax, [r8]
    mov     ecx, ebx
    xor     ecx, esi
    test    ecx, 1
    jz      .cadone
    mov     ecx, 7
    call    fu_scl
.cadone:
    or      eax, 0x01000000
    ret
.clock:
    ; ходики: корпус с крышей, циферблат, стрелки, маятник, гири на цепочках
    mov     eax, ebx
    sub     eax, 5
    cmp     eax, 5
    ja      .ck2
    lea     ecx, [esi - 21]
    cmp     ecx, 6
    ja      .ck1
    ; корпус и циферблат
    cmp     eax, 1
    jb      .wood
    cmp     eax, 4
    ja      .wood
    cmp     ecx, 1
    jb      .wood
    cmp     ecx, 5
    ja      .wood
    cmp     ebx, 7
    jne     .ck0
    cmp     esi, 24
    jb      .dial
    cmp     esi, 25
    ja      .dial
    mov     eax, 0x01201810
    ret
.ck0:
    cmp     ebx, 8
    jne     .dial
    cmp     esi, 24
    jne     .dial
    mov     eax, 0x01201810
    ret
.dial:
    mov     eax, 0x01E8E4D8
    ret
.ck1:
    ; крыша корпуса
    cmp     esi, 28
    jne     .ck1a
    cmp     eax, 1
    jb      .dno
    cmp     eax, 4
    ja      .dno
    jmp     .wood
.ck1a:
    cmp     esi, 29
    jne     .ck2
    cmp     eax, 2
    jb      .dno
    cmp     eax, 3
    ja      .dno
    jmp     .wood
.ck2:
    ; маятник качается (clk_sw), гири — у краёв
    cmp     esi, 20
    ja      .dno
    mov     eax, 7
    add     eax, [clk_sw]
    cmp     esi, 12
    jb      .ck3
    cmp     esi, 13
    ja      .ck4
    ; груз маятника 2x2
    cmp     ebx, eax
    jb      .ck3
    lea     ecx, [eax + 1]
    cmp     ebx, ecx
    ja      .ck3
    mov     eax, 0x41C8A050
    ret
.ck4:
    cmp     ebx, eax
    jne     .ck3
    mov     eax, 0x01B8903C
    ret
.ck3:
    cmp     ebx, 6
    je      .ch
    cmp     ebx, 9
    jne     .dno
.ch:
    cmp     esi, 11
    ja      .chain
    cmp     esi, 8
    jb      .dno
    mov     eax, 0x017A6030                 ; гиря-шишка
    ret
.chain:
    test    esi, 1
    jz      .dno
    mov     eax, 0x01707070
    ret
.icon:
    ; икона в золочёной рамке, полочка с лампадой, рушник
    lea     eax, [ebx - 5]
    cmp     eax, 5
    ja      .ic5
    lea     ecx, [esi - 21]
    cmp     ecx, 8
    ja      .ic5
    test    eax, eax
    jz      .gold
    cmp     eax, 5
    je      .gold
    test    ecx, ecx
    jz      .gold
    cmp     ecx, 8
    je      .gold
    ; нимб, лик, одежды
    cmp     ecx, 7
    jne     .ic1
    cmp     eax, 1
    jb      .icd
    cmp     eax, 4
    ja      .icd
    jmp     .gold
.ic1:
    cmp     ecx, 5
    jb      .ic2
    cmp     eax, 2
    jb      .ic1a
    cmp     eax, 3
    ja      .ic1a
    mov     eax, 0x01C89870
    ret
.ic1a:
    cmp     ecx, 6
    je      .gold
.icd:
    mov     eax, 0x013A2418
    ret
.ic2:
    cmp     eax, 1
    jb      .icd
    cmp     eax, 4
    ja      .icd
    mov     eax, 0x017A2A1E
    ret
.ic5:
    ; полочка и лампада
    cmp     esi, 20
    jne     .ic6
    lea     eax, [ebx - 4]
    cmp     eax, 7
    ja      .ic7
    jmp     .wood
.ic6:
    cmp     esi, 19
    jne     .ic7
    cmp     ebx, 7
    jb      .ic7
    cmp     ebx, 8
    ja      .ic7
    mov     eax, 0xC1E03020                 ; лампада — горит
    ret
.ic7:
    ; рушник: поверху и концами по бокам, вышивка рядами
    cmp     esi, 30
    jb      .ic8
    cmp     esi, 31
    ja      .dno
    lea     eax, [ebx - 3]
    cmp     eax, 9
    ja      .dno
    jmp     .towel
.ic8:
    cmp     esi, 16
    jb      .dno
    cmp     ebx, 3
    je      .towel
    cmp     ebx, 4
    je      .towel
    cmp     ebx, 11
    je      .towel
    cmp     ebx, 12
    jne     .dno
.towel:
    test    esi, 3
    jnz     .tw1
    mov     eax, 0x01B02820
    ret
.tw1:
    mov     eax, 0x01E8E6E0
    ret
.gold:
    mov     eax, 0x01C8A040
    ret
.photo:
    ; три карточки в рамках
    mov     r8d, 2
    mov     r9d, 21
    mov     r10d, 5
    mov     r11d, 6
    call    .frame
    test    eax, eax
    jnz     .pret
    mov     r8d, 9
    mov     r9d, 19
    mov     r10d, 5
    mov     r11d, 6
    call    .frame
    test    eax, eax
    jnz     .pret
    mov     r8d, 8
    mov     r9d, 26
    mov     r10d, 3
    mov     r11d, 4
    call    .frame
.pret:
    ret
; рамка r8d, r9d (левый нижний), r10d x r11d: край — тёмное дерево, внутри — сепия
; с головой и плечами -> eax (0 — мимо)
.frame:
    mov     eax, ebx
    sub     eax, r8d
    cmp     eax, r10d
    jae     .dno
    mov     ecx, esi
    sub     ecx, r9d
    cmp     ecx, r11d
    jae     .dno
    test    eax, eax
    jz      .fdark
    dec     r10d
    cmp     eax, r10d
    je      .fdark
    test    ecx, ecx
    jz      .fdark
    dec     r11d
    cmp     ecx, r11d
    je      .fdark
    ; голова и плечи
    shr     r10d, 1
    cmp     eax, r10d
    jne     .fs1
    cmp     ecx, 3
    jb      .fs1
    mov     eax, 0x016A5438
    ret
.fs1:
    cmp     ecx, 1
    jne     .fsep
    mov     eax, 0x016A5438
    ret
.fsep:
    mov     eax, 0x01B8A078
    ret
.fdark:
    mov     eax, 0x013A2818
    ret
.calendar:
    ; отрывной календарь: гвоздь, красная шапка, листок с числом
    cmp     ebx, 8
    jne     .cl1
    cmp     esi, 27
    jne     .cl1
    mov     eax, 0x01707070
    ret
.cl1:
    lea     eax, [ebx - 6]
    cmp     eax, 4
    ja      .dno
    cmp     esi, 24
    jb      .cl2
    cmp     esi, 26
    ja      .dno
    mov     eax, 0x01B02820
    ret
.cl2:
    cmp     esi, 17
    jb      .dno
    ; «7»: верхняя черта и наклон
    cmp     esi, 22
    jne     .cl3
    cmp     eax, 1
    jb      .cpap
    cmp     eax, 3
    ja      .cpap
    jmp     .ink
.cl3:
    cmp     esi, 18
    jb      .cpap
    mov     ecx, esi
    sub     ecx, 18
    shr     ecx, 1
    add     ecx, 2
    cmp     eax, ecx
    jne     .cpap
.ink:
    mov     eax, 0x01302828
    ret
.cpap:
    mov     eax, 0x01E8E6DE
    cmp     esi, 17
    jne     .pp
    test    ebx, 1
    jz      .pp
    mov     eax, 0x01C8C6BE
.pp:
    ret
.mirror:
    ; зеркало в раме, блик наискось
    lea     eax, [ebx - 5]
    cmp     eax, 5
    ja      .dno
    lea     ecx, [esi - 18]
    cmp     ecx, 11
    ja      .dno
    test    eax, eax
    jz      .mfr
    cmp     eax, 5
    je      .mfr
    test    ecx, ecx
    jz      .mfr
    cmp     ecx, 11
    je      .mfr
    add     eax, ecx
    xor     edx, edx
    mov     ecx, 7
    div     ecx
    mov     eax, 0x018A9AA8
    test    edx, edx
    jnz     .mr
    mov     eax, 0x41C8D4DC
.mr:
    ret
.mfr:
    mov     eax, 0x018A6A3A
    ret
.shelf:
    ; полка на кронштейнах: банки, тарелка ребром, чашка
    cmp     esi, 21
    jne     .sh1
    lea     eax, [ebx - 1]
    cmp     eax, 13
    ja      .dno
    jmp     .wood
.sh1:
    cmp     esi, 19
    jb      .dno
    cmp     esi, 20
    ja      .sh2
    cmp     ebx, 2
    je      .wood
    cmp     ebx, 13
    je      .wood
    jmp     .dno
.sh2:
    cmp     esi, 26
    ja      .dno
    ; банка зелёного стекла с крышкой
    lea     eax, [ebx - 2]
    cmp     eax, 1
    ja      .sh3
    cmp     esi, 25
    ja      .dno
    mov     eax, 0x01808284
    cmp     esi, 25
    je      .shr
    mov     eax, 0x013A6A4A
    ret
.sh3:
    ; янтарная
    lea     eax, [ebx - 5]
    cmp     eax, 1
    ja      .sh4
    cmp     esi, 24
    ja      .dno
    mov     eax, 0x01A8702A
    ret
.sh4:
    ; тарелка: круг 4x5 с синей каймой
    lea     eax, [ebx*2 - 21]
    imul    eax, eax
    lea     ecx, [esi*2 - 48]
    imul    ecx, ecx
    add     eax, ecx
    cmp     eax, 20
    ja      .sh5
    cmp     eax, 10
    jb      .shp
    mov     eax, 0x013A5AA0
    ret
.shp:
    mov     eax, 0x01E0E0E8
    ret
.sh5:
    cmp     ebx, 13
    jne     .dno
    cmp     esi, 23
    ja      .dno
    mov     eax, 0x01B03A30
.shr:
    ret
.wood:
    mov     eax, 0x015A3A22
    ret
.dno:
    xor     eax, eax
    ret

; ----- отделка избы (ebx = u, esi = h, edi = грань) -> eax, 0 — брёвна
.finish:
    mov     eax, [r12 + HS_WALL]
    test    eax, eax
    jz      .dno
    ; вдоль стены — сквозная координата (узор не рвётся на стыке тайлов)
    mov     r13d, [bk_tx]
    test    edi, edi
    jz      .al
    mov     r13d, MAPW
    sub     r13d, [bk_ty]
.al:
    shl     r13d, 4
    add     r13d, ebx
    cmp     esi, 2
    jb      .plinth
    cmp     eax, 2
    jb      .white
    je      .paper
    ; ----- вагонка: рейки по 4 px, шов, свой оттенок у рейки, волокна
    mov     eax, [r12 + HS_WCOL]
    test    r13d, 3
    jnz     .v1
    mov     ecx, 5
    call    fu_scl
    or      eax, 0x01000000
    ret
.v1:
    mov     ecx, r13d
    shr     ecx, 2
    mov     edx, esi
    shr     edx, 3
    HASH    ecx, edx
    and     ecx, 1
    add     ecx, 7
    call    fu_scl
    or      eax, 0x01000000
    ret
.white:
    ; побелка над крашеной панелью
    cmp     esi, 12
    jae     .w1
    mov     eax, [r12 + HS_WCOL]
    cmp     esi, 11
    jne     .w0
    mov     ecx, 6
    call    fu_scl
.w0:
    or      eax, 0x01000000
    ret
.w1:
    mov     ecx, r13d
    mov     edx, esi
    HASH    ecx, edx
    and     ecx, 7
    imul    ecx, ecx, 0x010101
    mov     eax, 0x01D6DAE0
    sub     eax, ecx
    ret
.paper:
    ; обои: полоса каждые 6 px, цветочки в шахматку, бордюр поверху
    mov     eax, [r12 + HS_WCOL]
    cmp     esi, 33
    jb      .p0
    mov     ecx, 6
    call    fu_scl
    or      eax, 0x01000000
    ret
.p0:
    mov     eax, r13d
    xor     edx, edx
    mov     ecx, 6
    div     ecx                         ; eax — номер полосы, edx — в ней
    mov     r8d, eax
    mov     r9d, edx
    mov     eax, [r12 + HS_WCOL]
    test    r9d, r9d
    jnz     .p1
    mov     ecx, 7
    call    fu_scl
    or      eax, 0x01000000
    ret
.p1:
    ; цветок: середина полосы, по высоте через 8 со сдвигом у соседних полос
    mov     ecx, r8d
    and     ecx, 1
    shl     ecx, 2
    add     ecx, esi
    and     ecx, 7
    cmp     ecx, 4
    jne     .p9
    cmp     r9d, 3
    jne     .p2
    mov     eax, [r12 + HS_WMOT]
    or      eax, 0x01000000
    ret
.p2:
    cmp     r9d, 2
    je      .p3
    cmp     r9d, 4
    jne     .p9
.p3:
    ; лепестки — полутоном
    mov     ecx, [r12 + HS_WMOT]
    and     eax, 0xFEFEFE
    and     ecx, 0xFEFEFE
    add     eax, ecx
    shr     eax, 1
.p9:
    or      eax, 0x01000000
    ret
.plinth:
    mov     eax, 0x014A3020
    ret

; ================================================================ лут на виду
; ecx = номер в loot (draw_entity): картинка стоит низом на своей высоте над полом
draw_loot:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32
    imul    eax, ecx, LO_SZ
    lea     rbx, [loot]
    add     rbx, rax
    movss   xmm0, [rbx + LO_X]
    mulss   xmm0, [f_256]
    cvtss2si esi, xmm0
    movss   xmm0, [rbx + LO_Y]
    mulss   xmm0, [f_256]
    cvtss2si edi, xmm0
    mov     eax, edi
    shl     eax, 16
    or      eax, esi
    mov     [cm_gpos], eax
    mov     eax, esi
    sub     eax, [camx]
    mov     ecx, edi
    sub     ecx, [camy]
    mov     edx, eax
    sub     edx, ecx
    sar     edx, 4
    add     edx, CX0                    ; sx
    add     eax, ecx
    sar     eax, 5
    add     eax, CY0                    ; sy
    movzx   ecx, byte [rbx + LO_T]
    dec     ecx
    lea     r8, [lo_sw]
    movzx   r9d, byte [r8 + rcx]        ; w
    lea     r8, [lo_sh]
    movzx   r10d, byte [r8 + rcx]       ; h
    lea     r8, [lo_soff]
    mov     r11d, [r8 + rcx*4]
    mov     r8d, r9d
    shr     r8d, 1
    sub     edx, r8d                    ; x0
    movzx   r8d, byte [rbx + LO_Z]
    sub     eax, r8d
    sub     eax, r10d
    inc     eax
    mov     r8d, eax                    ; y0
    shl     r10d, 8
    or      r9d, r10d
    lea     rcx, [spr_loot]
    lea     rcx, [rcx + r11*4]
    mov     dword [img_sway], 0
    mov     dword [img_mode], 0
    mov     dword [img_nb], N_SM
    call    draw_img
    mov     dword [img_nb], N_BL
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret
