; ----------------------------------------------------------------------------
;  world.inc — ГСЧ, таблицы, процедурные спрайты, генерация села, статический свет
; ----------------------------------------------------------------------------
section .text

; xorshift32 -> eax (портит ecx)
rand:
    mov     eax, [rng]
    mov     ecx, eax
    shl     ecx, 13
    xor     eax, ecx
    mov     ecx, eax
    shr     ecx, 17
    xor     eax, ecx
    mov     ecx, eax
    shl     ecx, 5
    xor     eax, ecx
    mov     [rng], eax
    ret

; xmm0 = [0,1) (портит eax, ecx)
randf:
    call    rand
    shr     eax, 8
    cvtsi2ss xmm0, eax
    mulss   xmm0, [f_inv24]
    ret

; ecx = n -> eax в [0,n) (портит ecx, edx, r8)
randn:
    mov     r8d, ecx
    call    rand
    mul     r8d
    mov     eax, edx
    ret

; ---------------------------------------------------------------- таблицы
init_tables:
    push    rbx
    push    rsi
    push    rdi
    ; веса билинейки: [64-f]x4, [f]x4
    lea     rdi, [wtab]
    xor     ecx, ecx
.w:
    mov     eax, 64
    sub     eax, ecx
    mov     [rdi], ax
    mov     [rdi+2], ax
    mov     [rdi+4], ax
    mov     [rdi+6], ax
    mov     [rdi+8], cx
    mov     [rdi+10], cx
    mov     [rdi+12], cx
    mov     [rdi+14], cx
    add     rdi, 16
    inc     ecx
    cmp     ecx, 64
    jb      .w
    ; виньетка
    lea     rdi, [vign]
    xor     esi, esi
.vy:
    mov     eax, esi
    sub     eax, 190
    cvtsi2ss xmm1, eax
    mulss   xmm1, [f_inv230]
    mulss   xmm1, xmm1
    xor     ebx, ebx
.vx:
    mov     eax, ebx
    sub     eax, 320
    cvtsi2ss xmm0, eax
    mulss   xmm0, [f_inv340]
    mulss   xmm0, xmm0
    addss   xmm0, xmm1
    mulss   xmm0, [f_0_45]
    movss   xmm2, [f_1]
    subss   xmm2, xmm0
    CLAMP01 xmm2
    mulss   xmm2, [f_255]
    cvtss2si eax, xmm2
    mov     [rdi], al
    inc     rdi
    inc     ebx
    cmp     ebx, SCR_W
    jb      .vx
    inc     esi
    cmp     esi, SCR_H
    jb      .vy
    ; бревенчатая таблица: h -> (h%6) | (h/6)<<4
    lea     rdi, [logtab]
    xor     esi, esi
.lt:
    mov     eax, esi
    xor     edx, edx
    mov     ebx, 6
    div     ebx
    shl     eax, 4
    or      eax, edx
    mov     [rdi + rsi], al
    inc     esi
    cmp     esi, 64
    jb      .lt
    ; плёночное зерно
    lea     rdi, [grain]
    xor     ebx, ebx
.g:
    call    rand
    and     eax, 7
    sub     eax, 3
    mov     [rdi + rbx], al
    inc     ebx
    cmp     ebx, 65536
    jb      .g
    ; нормали G-буфера: x100 -> единичные
    lea     rsi, [nraw]
    lea     rdi, [nvec]
    mov     ebx, NN
.nv:
    movsx   eax, word [rsi]
    cvtsi2ss xmm0, eax
    movsx   eax, word [rsi + 2]
    cvtsi2ss xmm1, eax
    movsx   eax, word [rsi + 4]
    cvtsi2ss xmm2, eax
    movaps  xmm3, xmm0
    mulss   xmm3, xmm0
    movaps  xmm4, xmm1
    mulss   xmm4, xmm1
    addss   xmm3, xmm4
    movaps  xmm4, xmm2
    mulss   xmm4, xmm2
    addss   xmm3, xmm4
    sqrtss  xmm3, xmm3
    movss   xmm4, [f_1]
    divss   xmm4, xmm3
    mulss   xmm0, xmm4
    mulss   xmm1, xmm4
    mulss   xmm2, xmm4
    movss   [rdi], xmm0
    movss   [rdi + 4], xmm1
    movss   [rdi + 8], xmm2
    mov     dword [rdi + 12], 0
    add     rsi, 6
    add     rdi, 16
    dec     ebx
    jnz     .nv
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- спрайты
; rdi = картинка, r10d = ширина, ecx = x0, edx = y0, r8d = x1, r9d = y1 (вкл.), eax = цвет
img_rect:
    push    rbx
    push    rsi
    mov     esi, edx
.y:
    cmp     esi, r9d
    jg      .done
    mov     ebx, ecx
.x:
    cmp     ebx, r8d
    jg      .ny
    mov     r11d, esi
    imul    r11d, r10d
    add     r11d, ebx
    mov     [rdi + r11*4], eax
    inc     ebx
    jmp     .x
.ny:
    inc     esi
    jmp     .y
.done:
    pop     rsi
    pop     rbx
    ret

%macro IRECT 5          ; x0, y0, x1, y1, цвет
    mov     ecx, %1
    mov     edx, %2
    mov     r8d, %3
    mov     r9d, %4
    mov     eax, %5
    call    img_rect
%endmacro

gen_sprites:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40

    ; ================= ель 32x64
    lea     rdi, [spr_spruce]
    mov     r10d, 32
    IRECT   14, 55, 16, 63, 0x013A2A1E
    IRECT   17, 55, 17, 63, 0x01281C14
    xor     r12d, r12d                  ; ярус k
.tier:
    imul    eax, r12d, 10
    mov     ebx, 58
    sub     ebx, eax                    ; yb
    mov     esi, ebx
    sub     esi, 18
    add     esi, r12d                   ; yt
    imul    eax, r12d, 2
    mov     ebp, 15
    sub     ebp, eax                    ; hw
    mov     r13d, esi                   ; y
.ty:
    cmp     r13d, ebx
    jg      .tnext
    ; w = hw*(y-yt)/(yb-yt) + дрожание
    mov     eax, r13d
    sub     eax, esi
    imul    eax, ebp
    mov     ecx, ebx
    sub     ecx, esi
    xor     edx, edx
    div     ecx
    mov     r15d, eax
    mov     eax, r13d
    mov     ecx, r12d
    HASH    eax, ecx
    and     eax, 1
    add     r15d, eax
    cmp     r15d, 1
    jge     .wok
    mov     r15d, 1
.wok:
    mov     r14d, 16
    sub     r14d, r15d                  ; x от 16-w
.tx:
    mov     eax, 15
    add     eax, r15d
    cmp     r14d, eax
    jg      .tyn
    cmp     r14d, 31
    ja      .txn
    ; dist, e, bot
    mov     eax, r14d
    sub     eax, 16
    jge     .dpos
    mov     eax, 15
    sub     eax, r14d
.dpos:
    mov     ecx, r15d
    dec     ecx
    sub     ecx, eax                    ; e
    mov     edx, ebx
    sub     edx, r13d                   ; bot
    push    rcx
    push    rdx
    imul    eax, r12d, 37
    add     eax, r14d
    mov     ecx, r13d
    HASH    eax, ecx
    pop     rdx
    pop     rcx
    ; снег на кромке и на нижнем краю лап
    cmp     ecx, 0
    jle     .snow
    cmp     ecx, 1
    jg      .nedge
    test    eax, 3
    jnz     .snow
.nedge:
    cmp     edx, 1
    jg      .green
    test    eax, 1
    jnz     .snow
.green:
    mov     edx, 0x012B4A3A
    cmp     r14d, 16
    jl      .gl
    mov     edx, 0x011E3629
.gl:
    mov     ecx, eax
    and     ecx, 7
    jnz     .g2
    mov     edx, 0x0116281E
.g2:
    and     eax, 15
    cmp     eax, 1
    jne     .put
    cmp     r14d, 16
    jge     .put
    mov     edx, 0x01365A46
    jmp     .put
.snow:
    mov     edx, 0x01DCE4EE
    test    eax, 4
    jz      .put
    mov     edx, 0x01B8C4D4
.put:
    mov     eax, r13d
    shl     eax, 5
    add     eax, r14d
    mov     [rdi + rax*4], edx
.txn:
    inc     r14d
    jmp     .tx
.tyn:
    inc     r13d
    jmp     .ty
.tnext:
    inc     r12d
    cmp     r12d, 5
    jb      .tier
    mov     r10d, 32
    IRECT   15, 3, 16, 3, 0x01E4EAF2

    ; ================= берёза 20x56
    lea     rdi, [spr_birch]
    mov     r10d, 20
    ; крона — редкие веточки
    xor     r13d, r13d
.cy:
    xor     r14d, r14d
.cx:
    mov     eax, r14d
    mov     ecx, r13d
    add     ecx, 50
    HASH    eax, ecx
    and     eax, 7
    jnz     .cxn
    mov     eax, r14d
    sub     eax, 10
    imul    eax, eax
    imul    eax, 196
    mov     ecx, r13d
    sub     ecx, 14
    imul    ecx, ecx
    imul    ecx, 100
    add     eax, ecx
    cmp     eax, 19600
    jge     .cxn
    imul    eax, r13d, 20
    add     eax, r14d
    mov     dword [rdi + rax*4], 0x015A4852
.cxn:
    inc     r14d
    cmp     r14d, 20
    jb      .cx
    inc     r13d
    cmp     r13d, 31
    jb      .cy
    ; ветви
    xor     r12d, r12d
.br:
    lea     esi, [r12d*4 + 6]           ; y0
    mov     ebp, 1
    test    r12d, 1
    jnz     .bdir
    mov     ebp, -1
.bdir:
    mov     eax, r12d
    mov     ecx, 7
    HASH    eax, ecx
    and     eax, 3
    lea     r15d, [eax + 5]             ; длина
    mov     r14d, 1
.bi:
    cmp     r14d, r15d
    jg      .bnext
    mov     eax, ebp
    imul    eax, r14d
    add     eax, 10                     ; x
    cmp     eax, 19
    ja      .bin
    mov     ecx, r14d
    shr     ecx, 1
    mov     edx, esi
    sub     edx, ecx                    ; y
    imul    edx, 20
    add     edx, eax
    mov     dword [rdi + rdx*4], 0x014A3A44
    cmp     r14d, r15d
    jne     .bin
    sub     edx, 20
    js      .bin
    mov     dword [rdi + rdx*4], 0x014A3A44
.bin:
    inc     r14d
    jmp     .bi
.bnext:
    inc     r12d
    cmp     r12d, 8
    jb      .br
    ; ствол
    mov     r10d, 20
    IRECT   10, 4, 10, 13, 0x01C8C8BE
    mov     r13d, 14
.ty2:
    mov     r14d, 9
.tx2:
    mov     edx, 0x01E2E2D8
    cmp     r14d, 11
    jne     .tc
    mov     edx, 0x01A8A89E
.tc:
    mov     eax, r14d
    mov     ecx, r13d
    HASH    eax, ecx
    and     eax, 7
    jnz     .tc2
    mov     edx, 0x0124201E
.tc2:
    mov     eax, r13d
    mov     ecx, 99
    HASH    eax, ecx
    and     eax, 15
    jnz     .tc3
    mov     edx, 0x012A2622
.tc3:
    imul    eax, r13d, 20
    add     eax, r14d
    mov     [rdi + rax*4], edx
    inc     r14d
    cmp     r14d, 11
    jbe     .tx2
    inc     r13d
    cmp     r13d, 55
    jbe     .ty2

    ; ================= фонарный столб 12x50
    lea     rdi, [spr_lamp]
    mov     r10d, 12
    IRECT   3, 6, 3, 49, 0x0134363C
    IRECT   4, 6, 4, 49, 0x0124262A
    IRECT   3, 6, 9, 7, 0x0134363C
    IRECT   3, 5, 9, 5, 0x01E0E6EE
    IRECT   7, 8, 10, 8, 0x01202226
    IRECT   8, 9, 9, 9, 0xC1FFE0A0          ; «горячая» лампа — сильный блум
    IRECT   2, 45, 5, 49, 0x012A2C30

    ; ================= православный крест 10x20
    lea     rdi, [spr_cross]
    mov     r10d, 10
    IRECT   4, 0, 4, 17, 0x013C3026
    IRECT   5, 0, 5, 17, 0x012A2018
    IRECT   3, 3, 6, 3, 0x013C3026
    IRECT   1, 6, 8, 7, 0x013C3026
    IRECT   1, 12, 2, 12, 0x013C3026
    IRECT   3, 11, 6, 11, 0x013C3026
    IRECT   7, 10, 8, 10, 0x013C3026
    IRECT   3, 2, 6, 2, 0x01E6ECF2
    IRECT   1, 5, 8, 5, 0x01E6ECF2
    IRECT   4, 0, 5, 0, 0x01E6ECF2
    IRECT   3, 17, 6, 17, 0x01D8E0EA
    IRECT   1, 18, 8, 19, 0x01D8E0EA

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

; ---------------------------------------------------------------- карта
; tile(x,y) = t   (ecx = x, edx = y, eax = t) — с проверкой границ
set_tile:
    cmp     ecx, MAPW
    jae     .no
    cmp     edx, MAPW
    jae     .no
    push    rdx
    shl     edx, 7
    add     edx, ecx
    lea     r11, [tmap]
    mov     [r11 + rdx], al
    pop     rdx
.no:
    ret

; ecx = x, edx = y -> eax = тайл (за картой — T_WALL)
get_tile:
    mov     eax, T_WALL
    cmp     ecx, MAPW
    jae     .no
    cmp     edx, MAPW
    jae     .no
    mov     eax, edx
    shl     eax, 7
    add     eax, ecx
    lea     r11, [tmap]
    movzx   eax, byte [r11 + rax]
.no:
    ret

; xmm0 = x, xmm1 = y, xmm2 = радиус, rdx -> цвет (4 x f32) -> rax = запись (0 — полно)
add_light:
    mov     eax, [nlights]
    cmp     eax, MAXL
    jae     .full
    imul    eax, eax, L_SZ
    lea     r11, [lights]
    add     r11, rax
    xorps   xmm3, xmm3
    mov     eax, 32
.z:
    movups  [r11 + rax], xmm3           ; поля света попиксельно — с нуля
    add     eax, 16
    cmp     eax, L_SZ
    jb      .z
    movss   [r11 + L_X], xmm0
    movss   [r11 + L_Y], xmm1
    movss   [r11 + L_R], xmm2
    mov     eax, [nl_kind]
    mov     [r11 + L_KIND], eax
    movups  xmm3, [rdx]
    movups  [r11 + L_COL], xmm3
    inc     dword [nlights]
    mov     rax, r11
    ret
.full:
    xor     eax, eax
    ret

gen_map:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    lea     rdi, [tmap]
    xor     eax, eax
    mov     ecx, MAPW*MAPW
    rep     stosb
    lea     rdi, [hflags]
    mov     ecx, MAPW*MAPW
    rep     stosb
    lea     rdi, [rmap]
    mov     ecx, MAPW*MAPW
    rep     stosb
    lea     rdi, [wdeco]                ; нутро изб (interior.inc)
    mov     ecx, MAPW*MAPW
    rep     stosb
    lea     rdi, [fmap]
    mov     ecx, MAPW*MAPW*2
    rep     stosb
    mov     dword [nloot], 0
    mov     dword [nhouses], 0
    mov     dword [nlights], 0
    mov     dword [nsmk], 0
    ; главная улица
    lea     rdi, [tmap + 60*MAPW]
    mov     al, T_ROAD
    mov     ecx, 3*MAPW
    rep     stosb
    ; две поперечные
    lea     rdi, [tmap]
    xor     ecx, ecx
.vr:
    mov     byte [rdi+36], T_ROAD
    mov     byte [rdi+37], T_ROAD
    mov     byte [rdi+38], T_ROAD
    mov     byte [rdi+90], T_ROAD
    mov     byte [rdi+91], T_ROAD
    mov     byte [rdi+92], T_ROAD
    add     rdi, MAPW
    inc     ecx
    cmp     ecx, MAPW
    jb      .vr
    ; участки с избами
    lea     rsi, [lot_table]
.lot:
    cmp     byte [rsi], 0xFF
    je      .lotsdone
    call    rand
    cmp     eax, 0xD8000000
    jae     .skip
    movzx   eax, byte [rsi]
    mov     [L_x0], eax
    movzx   eax, byte [rsi+1]
    mov     [L_y0], eax
    movzx   eax, byte [rsi+2]
    mov     [L_w], eax
    movzx   eax, byte [rsi+3]
    mov     [L_h], eax
    movzx   eax, byte [rsi+4]
    mov     [L_side], eax
    call    make_lot
.skip:
    add     rsi, 5
    jmp     .lot
.lotsdone:
    call    make_cemetery
    call    place_lamps
    call    place_wells
    call    gen_wires
    call    place_trees
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

; забор по периметру L_x0,L_y0,L_w,L_h; внутри снег
fence_rect:
    push    rbx
    push    rsi
    mov     esi, [L_y0]
.y:
    mov     eax, [L_y0]
    add     eax, [L_h]
    cmp     esi, eax
    jge     .done
    mov     ebx, [L_x0]
.x:
    mov     eax, [L_x0]
    add     eax, [L_w]
    cmp     ebx, eax
    jge     .ny
    mov     eax, T_FENCE
    cmp     esi, [L_y0]
    je      .set
    cmp     ebx, [L_x0]
    je      .set
    mov     ecx, [L_y0]
    add     ecx, [L_h]
    dec     ecx
    cmp     esi, ecx
    je      .set
    mov     ecx, [L_x0]
    add     ecx, [L_w]
    dec     ecx
    cmp     ebx, ecx
    je      .set
    mov     eax, T_SNOW
.set:
    mov     ecx, ebx
    mov     edx, esi
    call    set_tile
    inc     ebx
    jmp     .x
.ny:
    inc     esi
    jmp     .y
.done:
    pop     rsi
    pop     rbx
    ret

; ecx = x, edx = y, r8d = nx, r9d = ny
try_window:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 32
    mov     ebx, ecx
    mov     esi, edx
    mov     r12d, r8d
    mov     r13d, r9d
    call    get_tile
    cmp     eax, T_WALL
    jne     .out
    ; соседи вдоль стены не должны быть дверью
    mov     ecx, ebx
    mov     edx, esi
    test    r12d, r12d
    jnz     .alongy
    dec     ecx
    call    get_tile
    cmp     eax, T_DOOR
    je      .out
    lea     ecx, [ebx+1]
    mov     edx, esi
    call    get_tile
    cmp     eax, T_DOOR
    je      .out
    jmp     .ok
.alongy:
    dec     edx
    call    get_tile
    cmp     eax, T_DOOR
    je      .out
    mov     ecx, ebx
    lea     edx, [esi+1]
    call    get_tile
    cmp     eax, T_DOOR
    je      .out
.ok:
    mov     eax, T_WIN
    cmp     dword [H_lit], 0
    je      .setw
    mov     eax, T_WINL
.setw:
    mov     ecx, ebx
    mov     edx, esi
    call    set_tile
    cmp     dword [H_lit], 0
    je      .out
    cvtsi2ss xmm0, ebx
    cvtsi2ss xmm3, r12d
    mulss   xmm3, [f_1_1]
    addss   xmm0, xmm3
    addss   xmm0, [f_0_5]
    cvtsi2ss xmm1, esi
    cvtsi2ss xmm3, r13d
    mulss   xmm3, [f_1_1]
    addss   xmm1, xmm3
    addss   xmm1, [f_0_5]
    movss   xmm2, [f_3_4]
    lea     rdx, [col_window]
    mov     dword [nl_kind], LK_WIN
    call    add_light
    test    rax, rax
    jz      .out
    mov     [rax + L_TX], ebx           ; для света сквозь раму: тайл и куда наружу
    mov     [rax + L_TY], esi
    mov     [rax + L_DX], r12d
    mov     [rax + L_DY], r13d
.out:
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

make_lot:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    call    fence_rect
    ; калитка — 2 тайла посередине стороны к дороге
    mov     eax, [L_side]
    mov     ebx, [L_x0]
    mov     esi, [L_y0]
    cmp     eax, 0
    je      .gs
    cmp     eax, 1
    je      .gn
    cmp     eax, 2
    je      .ge
    ; W
    mov     eax, [L_h]
    shr     eax, 1
    lea     esi, [esi + eax - 1]
    jmp     .gv
.ge:
    mov     eax, [L_w]
    lea     ebx, [ebx + eax - 1]
    mov     eax, [L_h]
    shr     eax, 1
    lea     esi, [esi + eax - 1]
.gv:
    mov     ecx, ebx
    mov     edx, esi
    mov     eax, T_SNOW
    call    set_tile
    mov     ecx, ebx
    lea     edx, [esi+1]
    mov     eax, T_SNOW
    call    set_tile
    jmp     .house
.gs:
    mov     eax, [L_h]
    lea     esi, [esi + eax - 1]
.gn:
    mov     eax, [L_w]
    shr     eax, 1
    lea     ebx, [ebx + eax - 1]
    mov     ecx, ebx
    mov     edx, esi
    mov     eax, T_SNOW
    call    set_tile
    lea     ecx, [ebx+1]
    mov     edx, esi
    mov     eax, T_SNOW
    call    set_tile

.house:
    ; размеры избы
    mov     ecx, 3
    call    randn
    add     eax, 5
    mov     [H_w], eax
    mov     ecx, 2
    call    randn
    add     eax, 5
    mov     [H_h], eax
    ; hx = x0 + (w-hw)/2 + rnd(-1..1), зажать в [x0+2, x0+w-2-hw]
    mov     ecx, 3
    call    randn
    dec     eax
    mov     ebx, [L_w]
    sub     ebx, [H_w]
    shr     ebx, 1
    add     ebx, [L_x0]
    add     ebx, eax
    mov     eax, [L_x0]
    add     eax, 2
    cmp     ebx, eax
    cmovl   ebx, eax
    mov     eax, [L_x0]
    add     eax, [L_w]
    sub     eax, 2
    sub     eax, [H_w]
    cmp     ebx, eax
    cmovg   ebx, eax
    mov     [H_x], ebx
    mov     ecx, 3
    call    randn
    dec     eax
    mov     ebx, [L_h]
    sub     ebx, [H_h]
    shr     ebx, 1
    add     ebx, [L_y0]
    add     ebx, eax
    mov     eax, [L_y0]
    add     eax, 2
    cmp     ebx, eax
    cmovl   ebx, eax
    mov     eax, [L_y0]
    add     eax, [L_h]
    sub     eax, 2
    sub     eax, [H_h]
    cmp     ebx, eax
    cmovg   ebx, eax
    mov     [H_y], ebx
    ; стены и пол
    xor     esi, esi                    ; j
.hy:
    cmp     esi, [H_h]
    jge     .hdone
    xor     edi, edi                    ; i
.hx:
    cmp     edi, [H_w]
    jge     .hny
    mov     eax, T_WALL
    test    esi, esi
    jz      .hset
    test    edi, edi
    jz      .hset
    mov     ecx, [H_h]
    dec     ecx
    cmp     esi, ecx
    je      .hset
    mov     ecx, [H_w]
    dec     ecx
    cmp     edi, ecx
    je      .hset
    mov     eax, T_WOOD
.hset:
    mov     ecx, [H_x]
    add     ecx, edi
    mov     edx, [H_y]
    add     edx, esi
    call    set_tile
    inc     edi
    jmp     .hx
.hny:
    inc     esi
    jmp     .hy
.hdone:
    ; горит ли свет
    mov     dword [H_lit], 0
    call    rand
    cmp     eax, 0x58000000
    jae     .dark
    mov     dword [H_lit], 1
.dark:
    ; в каждом третьем-втором светлом доме работает телевизор
    mov     dword [H_tv], 0
    cmp     dword [H_lit], 0
    je      .flags
    call    rand
    cmp     eax, 0x66000000
    jae     .flags
    mov     dword [H_tv], 1
.flags:
    ; флаги дома на его тайлы: бит0 свет, бит1 телевизор
    mov     r8d, [H_tv]
    add     r8d, r8d
    or      r8d, [H_lit]
    xor     esi, esi
.fy2:
    cmp     esi, [H_h]
    jge     .fdone
    xor     edi, edi
.fx2:
    cmp     edi, [H_w]
    jge     .fny
    mov     eax, [H_y]
    add     eax, esi
    shl     eax, 7
    add     eax, [H_x]
    add     eax, edi
    lea     rdx, [hflags]
    mov     [rdx + rax], r8b
    inc     edi
    jmp     .fx2
.fny:
    inc     esi
    jmp     .fy2
.fdone:
    ; дверь
    mov     eax, [L_side]
    mov     ebx, [H_x]
    mov     esi, [H_y]
    cmp     eax, 0
    je      .ds
    cmp     eax, 1
    je      .dn
    cmp     eax, 2
    je      .de
    mov     eax, [H_h]                  ; W
    shr     eax, 1
    add     esi, eax
    jmp     .dset
.de:
    mov     eax, [H_w]
    lea     ebx, [ebx + eax - 1]
    mov     eax, [H_h]
    shr     eax, 1
    add     esi, eax
    jmp     .dset
.ds:
    mov     eax, [H_h]
    lea     esi, [esi + eax - 1]
.dn:
    mov     eax, [H_w]
    shr     eax, 1
    add     ebx, eax
.dset:
    mov     ecx, ebx
    mov     edx, esi
    mov     eax, T_DOOR
    call    set_tile
    ; окна: север/юг
    mov     r12d, 1
.wns:
    mov     eax, [H_w]
    dec     eax
    cmp     r12d, eax
    jge     .wwe0
    test    r12d, 1
    jz      .wnsn
    mov     ecx, [H_x]
    add     ecx, r12d
    mov     edx, [H_y]
    xor     r8d, r8d
    mov     r9d, -1
    call    try_window
    mov     ecx, [H_x]
    add     ecx, r12d
    mov     edx, [H_y]
    add     edx, [H_h]
    dec     edx
    xor     r8d, r8d
    mov     r9d, 1
    call    try_window
.wnsn:
    inc     r12d
    jmp     .wns
.wwe0:
    mov     r12d, 1
.wwe:
    mov     eax, [H_h]
    dec     eax
    cmp     r12d, eax
    jge     .furn
    test    r12d, 1
    jz      .wwen
    mov     ecx, [H_x]
    mov     edx, [H_y]
    add     edx, r12d
    mov     r8d, -1
    xor     r9d, r9d
    call    try_window
    mov     ecx, [H_x]
    add     ecx, [H_w]
    dec     ecx
    mov     edx, [H_y]
    add     edx, r12d
    mov     r8d, 1
    xor     r9d, r9d
    call    try_window
.wwen:
    inc     r12d
    jmp     .wwe
.furn:
    ; печь и стол в дальнем от двери углу
    mov     eax, [L_side]
    mov     ebx, [H_x]
    inc     ebx                         ; stove x
    mov     esi, [H_y]
    inc     esi                         ; stove y
    mov     edi, [H_x]
    add     edi, [H_w]
    sub     edi, 2                      ; table x
    mov     ebp, esi                    ; table y
    cmp     eax, 1
    jne     .f2
    mov     esi, [H_y]
    add     esi, [H_h]
    sub     esi, 2
    mov     ebp, esi
    jmp     .fset
.f2:
    cmp     eax, 2
    jne     .f3
    mov     edi, ebx
    mov     ebp, [H_y]
    add     ebp, [H_h]
    sub     ebp, 2
    jmp     .fset
.f3:
    cmp     eax, 3
    jne     .fset
    mov     ebx, [H_x]
    add     ebx, [H_w]
    sub     ebx, 2
    mov     edi, ebx
    mov     ebp, [H_y]
    add     ebp, [H_h]
    sub     ebp, 2
.fset:
    mov     eax, T_STOVE
    cmp     dword [H_lit], 0
    je      .st
    mov     eax, T_STOVEL
.st:
    mov     ecx, ebx
    mov     edx, esi
    call    set_tile
    mov     ecx, edi
    mov     edx, ebp
    mov     eax, T_TABLE
    call    set_tile
    ; крыша: запись дома (труба над печью)
    mov     ecx, ebx
    mov     edx, esi
    call    add_house
    call    furnish                     ; мебель, отделка, половики, лут (interior.inc)
    ; свет в избе
    cmp     dword [H_lit], 0
    je      .pile
    ; топится печь — дым из трубы
    mov     eax, [nsmk]
    cmp     eax, MAXSMK
    jae     .nosm
    lea     rdx, [smk]
    mov     [rdx + rax*2], bl
    mov     [rdx + rax*2 + 1], sil
    lea     rdx, [smkz]
    mov     ecx, [H_zt]
    mov     [rdx + rax], cl
    inc     dword [nsmk]
.nosm:
    ; телевизор у стола
    cmp     dword [H_tv], 0
    je      .room
    cvtsi2ss xmm0, edi
    addss   xmm0, [f_0_5]
    cvtsi2ss xmm1, ebp
    addss   xmm1, [f_0_5]
    movss   xmm2, [f_4_5]
    lea     rdx, [col_tv]
    mov     dword [nl_kind], LK_TV
    call    add_light
    test    rax, rax
    jz      .room
    mov     [rax + L_TX], edi
    mov     [rax + L_TY], ebp
.room:
    ; огонь в топке (дверца — на грани +y печи)
    cvtsi2ss xmm0, ebx
    addss   xmm0, [f_0_5]
    cvtsi2ss xmm1, esi
    addss   xmm1, [f_1]
    movss   xmm2, [f_3_2]
    lea     rdx, [col_fire]
    mov     dword [nl_kind], LK_STOVE
    call    add_light
    test    rax, rax
    jz      .room2
    mov     [rax + L_TX], ebx
    mov     [rax + L_TY], esi
.room2:
    mov     dword [nl_kind], LK_ROOM
    cvtsi2ss xmm0, dword [H_w]
    mulss   xmm0, [f_0_5]
    cvtsi2ss xmm3, dword [H_x]
    addss   xmm0, xmm3
    cvtsi2ss xmm1, dword [H_h]
    mulss   xmm1, [f_0_5]
    cvtsi2ss xmm3, dword [H_y]
    addss   xmm1, xmm3
    cvtsi2ss xmm2, dword [H_w]
    mulss   xmm2, [f_0_8]
    lea     rdx, [col_room]
    call    add_light
.pile:
    ; поленница у дальнего забора
    mov     eax, [L_side]
    cmp     eax, 2
    jae     .pv
    mov     ecx, [L_w]
    sub     ecx, 6
    call    randn
    mov     ebx, [L_x0]
    lea     ebx, [ebx + eax + 2]
    mov     esi, [L_y0]
    inc     esi
    cmp     dword [L_side], 0
    je      .ph
    mov     esi, [L_y0]
    add     esi, [L_h]
    sub     esi, 2
.ph:
    mov     ecx, ebx
    mov     edx, esi
    call    pile_if_snow
    lea     ecx, [ebx+1]
    mov     edx, esi
    call    pile_if_snow
    jmp     .ytree
.pv:
    mov     ecx, [L_h]
    sub     ecx, 6
    call    randn
    mov     esi, [L_y0]
    lea     esi, [esi + eax + 2]
    mov     ebx, [L_x0]
    inc     ebx
    cmp     dword [L_side], 2
    je      .pv2
    mov     ebx, [L_x0]
    add     ebx, [L_w]
    sub     ebx, 2
.pv2:
    mov     ecx, ebx
    mov     edx, esi
    call    pile_if_snow
    mov     ecx, ebx
    lea     edx, [esi+1]
    call    pile_if_snow
.ytree:
    ; дерево во дворе
    mov     r12d, 12
.yt:
    mov     ecx, [L_w]
    sub     ecx, 2
    call    randn
    mov     ebx, [L_x0]
    lea     ebx, [ebx + eax + 1]
    mov     ecx, [L_h]
    sub     ecx, 2
    call    randn
    mov     esi, [L_y0]
    lea     esi, [esi + eax + 1]
    mov     ecx, ebx
    mov     edx, esi
    call    snow_cross4
    test    eax, eax
    jz      .ytn
    call    rand
    mov     edx, eax
    mov     eax, T_SPRUCE
    test    edx, 0x100
    jz      .yts
    mov     eax, T_BIRCH
.yts:
    mov     ecx, ebx
    mov     edx, esi
    call    set_tile
    jmp     .done
.ytn:
    dec     r12d
    jnz     .yt
.done:
    call    lot_props
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

; поленница, если там снег
pile_if_snow:
    push    rbx
    push    rsi
    sub     rsp, 40
    mov     ebx, ecx
    mov     esi, edx
    call    get_tile
    cmp     eax, T_SNOW
    jne     .no
    mov     ecx, ebx
    mov     edx, esi
    mov     eax, T_PILE
    call    set_tile
.no:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret

; eax = 1, если тайл и 4 соседа — снег
snow_cross4:
    push    rbx
    push    rsi
    sub     rsp, 40
    mov     ebx, ecx
    mov     esi, edx
    call    get_tile
    test    eax, eax
    jnz     .no
    lea     ecx, [ebx-1]
    mov     edx, esi
    call    get_tile
    test    eax, eax
    jnz     .no
    lea     ecx, [ebx+1]
    mov     edx, esi
    call    get_tile
    test    eax, eax
    jnz     .no
    mov     ecx, ebx
    lea     edx, [esi-1]
    call    get_tile
    test    eax, eax
    jnz     .no
    mov     ecx, ebx
    lea     edx, [esi+1]
    call    get_tile
    test    eax, eax
    jnz     .no
    mov     eax, 1
    jmp     .out
.no:
    xor     eax, eax
.out:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret

CEM_X0 equ 106
CEM_Y0 equ 86
CEM_W  equ 13
CEM_H  equ 15

make_cemetery:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32
    mov     dword [L_x0], CEM_X0
    mov     dword [L_y0], CEM_Y0
    mov     dword [L_w], CEM_W
    mov     dword [L_h], CEM_H
    call    fence_rect
    mov     ecx, CEM_X0+5
    mov     edx, CEM_Y0
    mov     eax, T_SNOW
    call    set_tile
    mov     ecx, CEM_X0+6
    mov     edx, CEM_Y0
    mov     eax, T_SNOW
    call    set_tile
    mov     esi, CEM_Y0+2
.cy:
    mov     ebx, CEM_X0+2
.cx:
    call    rand
    cmp     eax, 0xC0000000
    jae     .cn
    mov     ecx, ebx
    mov     edx, esi
    mov     eax, T_CROSS
    call    set_tile
.cn:
    add     ebx, 2
    cmp     ebx, CEM_X0+CEM_W-2
    jl      .cx
    add     esi, 2
    cmp     esi, CEM_Y0+CEM_H-2
    jl      .cy
    ; пара берёз
    mov     edi, 5
.b:
    mov     ecx, CEM_W-2
    call    randn
    lea     ebx, [eax + CEM_X0 + 1]
    mov     ecx, CEM_H-2
    call    randn
    lea     esi, [eax + CEM_Y0 + 1]
    mov     ecx, ebx
    mov     edx, esi
    call    get_tile
    test    eax, eax
    jnz     .bn
    mov     ecx, ebx
    mov     edx, esi
    mov     eax, T_BIRCH
    call    set_tile
.bn:
    dec     edi
    jnz     .b
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ecx = x, edx = y
try_lamp:
    push    rbx
    push    rsi
    sub     rsp, 40
    mov     ebx, ecx
    mov     esi, edx
    call    get_tile
    test    eax, eax
    jnz     .no
    mov     ecx, ebx
    mov     edx, esi
    mov     eax, T_LAMP
    call    set_tile
    ; фонарь у околицы старта барахлит
    mov     dword [nl_kind], LK_LAMP
    cmp     ebx, 60
    jne     .nb
    cmp     esi, 63
    jne     .nb
    mov     dword [nl_kind], LK_BROKEN
    mov     [brk_x], ebx
    mov     [brk_y], esi
.nb:
    ; запомнить для проводов
    mov     eax, [nlamps]
    cmp     eax, MAXLAMP
    jae     .nl
    lea     rdx, [lamps]
    mov     [rdx + rax*4], bl
    mov     [rdx + rax*4 + 1], sil
    mov     ecx, [lamp_line]
    mov     [rdx + rax*4 + 2], cl
    inc     dword [nlamps]
.nl:
    cvtsi2ss xmm0, ebx
    addss   xmm0, [f_0_5]
    cvtsi2ss xmm1, esi
    addss   xmm1, [f_0_5]
    movss   xmm2, [f_7_5]
    lea     rdx, [col_lamp]
    call    add_light
.no:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret

place_lamps:
    push    rbx
    sub     rsp, 32
    mov     dword [nlamps], 0
    mov     dword [brk_x], -1
    mov     ebx, 4
.r1:
    mov     dword [lamp_line], 1
    mov     ecx, ebx
    mov     edx, 59
    call    try_lamp
    mov     dword [lamp_line], 2
    lea     ecx, [ebx+8]
    mov     edx, 63
    call    try_lamp
    add     ebx, 16
    cmp     ebx, 124
    jb      .r1
    mov     ebx, 6
.r2:
    mov     dword [lamp_line], 3
    mov     ecx, 35
    mov     edx, ebx
    call    try_lamp
    mov     dword [lamp_line], 4
    mov     ecx, 39
    lea     edx, [ebx+8]
    call    try_lamp
    mov     dword [lamp_line], 5
    mov     ecx, 89
    mov     edx, ebx
    call    try_lamp
    mov     dword [lamp_line], 6
    mov     ecx, 93
    lea     edx, [ebx+8]
    call    try_lamp
    add     ebx, 16
    cmp     ebx, 120
    jb      .r2
    add     rsp, 32
    pop     rbx
    ret

place_trees:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40
    xor     esi, esi                    ; y
.y:
    xor     ebx, ebx                    ; x
.x:
    mov     ecx, ebx
    mov     edx, esi
    call    get_tile
    test    eax, eax
    jnz     .n
    ; расстояние до края
    mov     edi, ebx
    cmp     esi, edi
    cmovl   edi, esi
    mov     eax, 127
    sub     eax, ebx
    cmp     eax, edi
    cmovl   edi, eax
    mov     eax, 127
    sub     eax, esi
    cmp     eax, edi
    cmovl   edi, eax
    cmp     edi, 2
    jl      .spruce
    cmp     edi, 8
    jge     .inner
    ; кайма леса: p = (8-edge)*0.055
    mov     eax, 8
    sub     eax, edi
    cvtsi2ss xmm1, eax
    mulss   xmm1, [f_0_055]
    call    randf
    comiss  xmm0, xmm1
    jae     .n
    call    rand
    test    eax, 7
    jnz     .spruce
    jmp     .birch
.inner:
    ; не у старта
    mov     eax, ebx
    sub     eax, 64
    add     eax, 4
    cmp     eax, 8
    jae     .far
    mov     eax, esi
    sub     eax, 61
    add     eax, 4
    cmp     eax, 8
    jb      .n
.far:
    call    randf
    comiss  xmm0, [f_0_03]
    jae     .n
    mov     ecx, ebx
    mov     edx, esi
    call    snow_cross4
    test    eax, eax
    jz      .n
    call    rand
    test    eax, 0x80
    jz      .spruce
.birch:
    mov     eax, T_BIRCH
    jmp     .set
.spruce:
    mov     eax, T_SPRUCE
.set:
    mov     ecx, ebx
    mov     edx, esi
    call    set_tile
.n:
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

build_opq:
    lea     r8, [tmap]
    lea     r9, [opq]
    lea     r10, [tprop]
    xor     ecx, ecx
.l:
    movzx   eax, byte [r8 + rcx]
    movzx   eax, byte [r10 + rax]
    shr     eax, 1
    and     eax, 1
    mov     [r9 + rcx], al
    ; для света комнаты окна тоже глухие (наружу светят огни у окон)
    movzx   edx, byte [r8 + rcx]
    cmp     edx, T_WIN
    je      .w
    cmp     edx, T_WINL
    jne     .nw
.w:
    mov     eax, 1
.nw:
    lea     rdx, [opqw]
    mov     [rdx + rcx], al
    inc     ecx
    cmp     ecx, MAPW*MAPW
    jb      .l
    ret

; opq_df: чебышёвское расстояние до ближайшего непрозрачного тайла opq (0 — сам
; непрозрачный), не больше 60 — по нему los_ray перешагивает открытое. Два прохода
; (прямой и обратный) с 8 соседями дают точное расстояние
build_opq_df:
    push    rbx
    ; ⌈2^32 / n⌉ для шагов луча, n = 1..LOSN−1
    mov     ecx, 1
.rc:
    mov     rax, 1 << 32
    lea     rax, [rax + rcx - 1]
    xor     edx, edx
    div     rcx
    lea     rdx, [los_rcp]
    mov     [rdx + rcx*8], rax
    inc     ecx
    cmp     ecx, LOSN
    jb      .rc
    lea     r8, [opq]
    lea     r9, [opq_df]
    xor     ecx, ecx
.i:
    cmp     byte [r8 + rcx], 0
    mov     eax, 60
    mov     edx, 0
    cmovne  eax, edx
    mov     [r9 + rcx], al
    inc     ecx
    cmp     ecx, MAPW*MAPW
    jb      .i
    ; прямой: соседи (x−1, y), (x−1, y−1), (x, y−1), (x+1, y−1)
    xor     r10d, r10d                  ; y
.fy:
    xor     r11d, r11d                  ; x
.fx:
    mov     ecx, r10d
    shl     ecx, 7
    add     ecx, r11d
    movzx   eax, byte [r9 + rcx]
    test    r11d, r11d
    jz      .f1
    movzx   edx, byte [r9 + rcx - 1]
    inc     edx
    cmp     edx, eax
    cmovb   eax, edx
.f1:
    test    r10d, r10d
    jz      .f4
    movzx   edx, byte [r9 + rcx - MAPW]
    inc     edx
    cmp     edx, eax
    cmovb   eax, edx
    test    r11d, r11d
    jz      .f3
    movzx   edx, byte [r9 + rcx - MAPW - 1]
    inc     edx
    cmp     edx, eax
    cmovb   eax, edx
.f3:
    cmp     r11d, MAPW-1
    je      .f4
    movzx   edx, byte [r9 + rcx - MAPW + 1]
    inc     edx
    cmp     edx, eax
    cmovb   eax, edx
.f4:
    mov     [r9 + rcx], al
    inc     r11d
    cmp     r11d, MAPW
    jb      .fx
    inc     r10d
    cmp     r10d, MAPW
    jb      .fy
    ; обратный: соседи (x+1, y), (x+1, y+1), (x, y+1), (x−1, y+1)
    mov     r10d, MAPW-1
.by:
    mov     r11d, MAPW-1
.bx:
    mov     ecx, r10d
    shl     ecx, 7
    add     ecx, r11d
    movzx   eax, byte [r9 + rcx]
    cmp     r11d, MAPW-1
    je      .b1
    movzx   edx, byte [r9 + rcx + 1]
    inc     edx
    cmp     edx, eax
    cmovb   eax, edx
.b1:
    cmp     r10d, MAPW-1
    je      .b4
    movzx   edx, byte [r9 + rcx + MAPW]
    inc     edx
    cmp     edx, eax
    cmovb   eax, edx
    cmp     r11d, MAPW-1
    je      .b3
    movzx   edx, byte [r9 + rcx + MAPW + 1]
    inc     edx
    cmp     edx, eax
    cmovb   eax, edx
.b3:
    test    r11d, r11d
    jz      .b4
    movzx   edx, byte [r9 + rcx + MAPW - 1]
    inc     edx
    cmp     edx, eax
    cmovb   eax, edx
.b4:
    mov     [r9 + rcx], al
    dec     r11d
    jns     .bx
    dec     r10d
    jns     .by
    pop     rbx
    ret

; ---------------------------------------------------------------- луч видимости
; ecx,edx = откуда, r8d,r9d = куда (мир 8.8) -> eax = 1, если не заслонено
; тайл-цель сам себя не заслоняет (грани стен освещаются снаружи)
los_ray:
    push    rbx
    push    rsi
    mov     ebx, ecx
    mov     esi, edx
    mov     eax, r9d
    sar     eax, 8
    shl     eax, 7
    mov     r10d, r8d
    sar     r10d, 8
    add     r10d, eax                   ; тайл-цель
    sub     r8d, ebx                    ; dx
    sub     r9d, esi                    ; dy
    mov     eax, r8d
    cdq
    xor     eax, edx
    sub     eax, edx
    mov     ecx, r9d
    mov     r11d, ecx
    sar     r11d, 31
    xor     ecx, r11d
    sub     ecx, r11d
    cmp     eax, ecx
    cmovl   eax, ecx
    shr     eax, 6                      ; шаг 1/4 тайла
    test    eax, eax
    jz      .clear
    mov     ecx, eax
    ; шаг = (d << 8) / n к нулю — умножением на ⌈2^32/n⌉: при |d << 8|·n < 2^32
    ; (здесь всегда) частное то же, что у idiv
    lea     r11, [los_rcp]
    mov     r11, [r11 + rcx*8]
    mov     eax, r8d
    shl     eax, 8
    cdq
    xor     eax, edx
    sub     eax, edx
    imul    rax, r11
    shr     rax, 32
    xor     eax, edx
    sub     eax, edx
    mov     r8d, eax
    mov     eax, r9d
    shl     eax, 8
    cdq
    xor     eax, edx
    sub     eax, edx
    imul    rax, r11
    shr     rax, 32
    xor     eax, edx
    sub     eax, edx
    mov     r9d, eax
    shl     ebx, 8
    shl     esi, 8
    mov     r11, [los_map]
    lea     rax, [opq]
    cmp     r11, rax
    jne     .step                       ; другие карты — шаг за шагом
    ; по opq — с перешагиванием открытого: шаг луча меньше ½ тайла, и если до
    ; ближайшего непрозрачного df тайлов, следующие 2(df − 1) шагов заведомо идут по
    ; открытому — туда же, куда пришёл бы шаг за шагом (pos + k·step), без проверок
    push    rdi
    lea     rdi, [opq_df]
.fast:
    mov     eax, esi
    sar     eax, 16
    shl     eax, 7
    mov     edx, ebx
    sar     edx, 16
    add     eax, edx
    movzx   edx, byte [rdi + rax]
    lea     edx, [rdx*2 - 2]
    cmp     edx, 2
    jl      .f1
    cmp     edx, ecx
    cmova   edx, ecx
    mov     eax, r8d
    imul    eax, edx
    add     ebx, eax
    mov     eax, r9d
    imul    eax, edx
    add     esi, eax
    sub     ecx, edx
    jnz     .fast
    pop     rdi
    jmp     .clear
.f1:
    add     ebx, r8d
    add     esi, r9d
    mov     eax, esi
    sar     eax, 16
    shl     eax, 7
    mov     edx, ebx
    sar     edx, 16
    add     eax, edx
    cmp     eax, r10d
    je      .f2
    cmp     byte [r11 + rax], 0
    jne     .fblk
.f2:
    dec     ecx
    jnz     .fast
    pop     rdi
    jmp     .clear
.fblk:
    pop     rdi
    jmp     .blocked
.step:
    add     ebx, r8d
    add     esi, r9d
    mov     eax, esi
    sar     eax, 16
    shl     eax, 7
    mov     edx, ebx
    sar     edx, 16
    add     eax, edx
    cmp     eax, r10d
    je      .next
    cmp     byte [r11 + rax], 0
    jne     .blocked
.next:
    dec     ecx
    jnz     .step
.clear:
    mov     eax, 1
    pop     rsi
    pop     rbx
    ret
.blocked:
    xor     eax, eax
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- луч над картой высот
; ecx,edx = огонь, r8d,r9d = цель (мир 8.8), [lt_h] = высота огня, px -> eax = 1, если
; луч от огня к снегу цели нигде не ниже заслонителей hmap8 (шаг 1/8 тайла). Первые
; 3 шага — столб и голова самого фонаря, последний — сама цель; стены и крыша избы,
; в которой цель, её не заслоняют (грани стен освещаются снаружи)
hf_ray:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    mov     eax, r9d
    sar     eax, 8
    shl     eax, 7
    mov     r12d, r8d
    sar     r12d, 8
    add     eax, r12d
    lea     r12, [rmap]
    movzx   r12d, byte [r12 + rax]      ; изба цели (0 — нет)
    mov     r13d, r8d
    sar     r13d, 8
    shl     r13d, 16
    mov     eax, r9d
    sar     eax, 8
    or      r13d, eax                   ; тайл цели: tx<<16 | ty
    mov     esi, ecx
    mov     edi, edx
    sub     r8d, ecx                    ; dx
    sub     r9d, edx                    ; dy
    mov     eax, r8d
    cdq
    xor     eax, edx
    sub     eax, edx
    mov     r10d, r9d
    mov     r11d, r10d
    sar     r11d, 31
    xor     r10d, r11d
    sub     r10d, r11d
    cmp     eax, r10d
    cmovl   eax, r10d
    shr     eax, 5                      ; шагов по 1/8 тайла
    cmp     eax, 4
    jb      .clear
    mov     r10d, eax
    shl     esi, 11                     ; ячейки 16.16
    shl     edi, 11
    mov     eax, r8d
    shl     eax, 11
    cdq
    idiv    r10d
    mov     r8d, eax                    ; шаг x
    mov     eax, r9d
    shl     eax, 11
    cdq
    idiv    r10d
    mov     r9d, eax                    ; шаг y
    mov     ebx, [lt_h]
    shl     ebx, 16                     ; высота луча 16.16 px
    mov     eax, ebx
    cdq
    idiv    r10d
    mov     r11d, eax                   ; спуск за шаг
    xor     ecx, ecx
.st:
    add     esi, r8d
    add     edi, r9d
    sub     ebx, r11d
    inc     ecx
    cmp     ecx, r10d
    jae     .clear
    cmp     ecx, 3
    jb      .st
    mov     eax, edi
    sar     eax, 16
    imul    eax, eax, HW8
    mov     edx, esi
    sar     edx, 16
    add     eax, edx
    lea     rdx, [hmap8 + HPAD*HW8 + HPAD]
    movzx   eax, byte [rdx + rax]
    and     eax, 0x7F
    shl     eax, 16
    cmp     eax, ebx
    jle     .st
    test    r12d, r12d
    jz      .blk
    mov     eax, esi                    ; своя изба не заслоняет
    sar     eax, 19
    shl     eax, 16
    mov     edx, edi
    sar     edx, 19
    or      eax, edx
    cmp     eax, r13d
    je      .st
.blk:
    xor     eax, eax
    jmp     .out
.clear:
    mov     eax, 1
.out:
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; esi, edi = сэмпл (4 на тайл), [lt_x], [lt_y] = огонь 8.8 -> xmm0 = доля дошедших лучей:
; 4 луча — точки 2x2 внутри сэмпла к 4 точкам огня (лампа не точка — полутени), по
; высотам (фонари, окна) или по тайлам (комнаты, телевизоры: внутри избы крыша сплошная)
lt_vis:
    push    rbx
    push    r13
    sub     rsp, 40
    xor     r13d, r13d
    xor     ebx, ebx
.r:
    lea     rax, [lv_tab]
    mov     r8d, esi
    shl     r8d, 6
    add     r8d, [rax + rbx*8]
    mov     r9d, edi
    shl     r9d, 6
    add     r9d, [rax + rbx*8 + 4]
    mov     ecx, [lt_x]
    add     ecx, [rax + rbx*8 + 32]
    mov     edx, [lt_y]
    add     edx, [rax + rbx*8 + 36]
    cmp     dword [lt_hf], 0
    jne     .hf
    call    los_ray
    jmp     .acc
.hf:
    call    hf_ray
.acc:
    add     r13d, eax
    inc     ebx
    cmp     ebx, 4
    jb      .r
    cvtsi2ss xmm0, r13d
    mulss   xmm0, [f_0_25]
    add     rsp, 40
    pop     r13
    pop     rbx
    ret

; ---------------------------------------------------------------- статический свет
calc_static_light:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    xor     eax, eax
    lea     rdi, [slight]
    mov     ecx, SLW*SLW
    rep     stosd
    lea     rdi, [slightL]
    mov     ecx, SLW*SLW
    rep     stosd
    lea     rdi, [slightB]
    mov     ecx, SLW*SLW
    rep     stosd
    lea     rdi, [slightT]
    mov     ecx, SLW*SLW
    rep     stosd
    lea     r12, [lights]
    mov     r13d, [nlights]
    pxor    xmm15, xmm15
.L:
    test    r13d, r13d
    jz      .done
    ; свой буфер по виду: окна и комнаты, фонари, сломанный, телевизоры
    lea     rbx, [slight]
    mov     eax, [r12 + L_KIND]
    cmp     eax, LK_STOVE
    je      .Lnext                      ; топка светит только попиксельно
    cmp     eax, LK_LAMP
    jne     .k1
    lea     rbx, [slightL]
.k1:
    cmp     eax, LK_BROKEN
    jne     .k2
    lea     rbx, [slightB]
.k2:
    cmp     eax, LK_TV
    jne     .k3
    lea     rbx, [slightT]
.k3:
    lea     rdx, [opq]
    cmp     eax, LK_ROOM
    jne     .k4
    lea     rdx, [opqw]
.k4:
    mov     [los_map], rdx
    ; фонари и окна — лучом над картой высот (тени заборов, деревьев, лавок),
    ; огонь в комнате и телевизор — по тайлам
    mov     dword [lt_hf], 0
    mov     ecx, 42                     ; голова фонаря, px
    cmp     eax, LK_LAMP
    je      .k5
    cmp     eax, LK_BROKEN
    je      .k5
    mov     ecx, 18                     ; середина окна
    cmp     eax, LK_WIN
    jne     .k6
.k5:
    mov     [lt_h], ecx
    mov     dword [lt_hf], 1
.k6:
    movss   xmm8, [r12 + L_X]
    movss   xmm9, [r12 + L_Y]
    movss   xmm10, [r12 + L_R]
    movups  xmm11, [r12 + L_COL]
    movss   xmm12, xmm10
    mulss   xmm12, xmm10                ; r^2
    movss   xmm0, xmm8
    mulss   xmm0, [f_256]
    cvtss2si eax, xmm0
    mov     [lt_x], eax
    movss   xmm0, xmm9
    mulss   xmm0, [f_256]
    cvtss2si eax, xmm0
    mov     [lt_y], eax
    ; диапазон сэмплов
    movss   xmm0, xmm8
    subss   xmm0, xmm10
    mulss   xmm0, [f_4]
    cvttss2si r14d, xmm0
    xor     eax, eax
    cmp     r14d, eax
    cmovl   r14d, eax                   ; i0
    movss   xmm0, xmm8
    addss   xmm0, xmm10
    mulss   xmm0, [f_4]
    cvttss2si r15d, xmm0
    inc     r15d
    mov     eax, SLW-1
    cmp     r15d, eax
    cmovg   r15d, eax                   ; i1
    movss   xmm0, xmm9
    subss   xmm0, xmm10
    mulss   xmm0, [f_4]
    cvttss2si edi, xmm0
    xor     eax, eax
    cmp     edi, eax
    cmovl   edi, eax                    ; j
    movss   xmm0, xmm9
    addss   xmm0, xmm10
    mulss   xmm0, [f_4]
    cvttss2si ebp, xmm0
    inc     ebp
    mov     eax, SLW-1
    cmp     ebp, eax
    cmovg   ebp, eax                    ; j1
.jy:
    cmp     edi, ebp
    jg      .Lnext
    mov     esi, r14d
.ix:
    cmp     esi, r15d
    jg      .jn
    cvtsi2ss xmm0, esi
    mulss   xmm0, [f_0_25]
    subss   xmm0, xmm8
    mulss   xmm0, xmm0
    cvtsi2ss xmm1, edi
    mulss   xmm1, [f_0_25]
    subss   xmm1, xmm9
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    comiss  xmm0, xmm12
    jae     .in
    divss   xmm0, xmm12
    movss   xmm13, [f_1]
    subss   xmm13, xmm0
    mulss   xmm13, xmm13                ; att
    call    lt_vis
    comiss  xmm0, [f_0]
    jbe     .in
    mulss   xmm13, xmm0                 ; доля лучей, дошедших до сэмпла
    shufps  xmm13, xmm13, 0
    mulps   xmm13, xmm11
    cvtps2dq xmm13, xmm13
    packssdw xmm13, xmm13
    packuswb xmm13, xmm13
    mov     eax, edi
    shl     eax, 9
    add     eax, esi
    movd    xmm0, [rbx + rax*4]
    paddusb xmm0, xmm13
    movd    [rbx + rax*4], xmm0
.in:
    inc     esi
    jmp     .ix
.jn:
    inc     edi
    jmp     .jy
.Lnext:
    add     r12, L_SZ
    dec     r13d
    jmp     .L
.done:
    lea     rax, [opq]
    mov     [los_map], rax
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
