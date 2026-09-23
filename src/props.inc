; ----------------------------------------------------------------------------
;  props.inc — дворовые мелочи: снеговик, лавочка, бельё, будка, скворечник,
;              колодец-журавль, печные трубы; расстановка по участкам
; ----------------------------------------------------------------------------
section .text

; rdi = картинка, r10d = ширина; ecx = x, edx = y, eax = цвет
img_px:
    push    rdx
    imul    edx, r10d
    add     edx, ecx
    mov     [rdi + rdx*4], eax
    pop     rdx
    ret

; линия ecx,edx -> r8d,r9d цветом eax
img_line:
    push    rbx
    push    rsi
    push    r12
    push    r13
    push    r14
    push    r15
    mov     r12d, ecx
    mov     r13d, edx
    mov     r14d, r8d
    sub     r14d, ecx                   ; dx
    mov     r15d, r9d
    sub     r15d, edx                   ; dy
    mov     ebx, eax
    mov     eax, r14d
    cdq
    xor     eax, edx
    sub     eax, edx
    mov     esi, eax
    mov     eax, r15d
    cdq
    xor     eax, edx
    sub     eax, edx
    cmp     eax, esi
    cmovg   esi, eax                    ; n
    test    esi, esi
    jnz     .go
    mov     ecx, r12d
    mov     edx, r13d
    mov     eax, ebx
    call    img_px
    jmp     .done
.go:
    cvtsi2ss xmm2, esi
    xor     r8d, r8d
.l:
    cmp     r8d, esi
    jg      .done
    cvtsi2ss xmm1, r8d
    cvtsi2ss xmm0, r14d
    mulss   xmm0, xmm1
    divss   xmm0, xmm2
    cvtss2si ecx, xmm0
    add     ecx, r12d
    cvtsi2ss xmm0, r15d
    mulss   xmm0, xmm1
    divss   xmm0, xmm2
    cvtss2si edx, xmm0
    add     edx, r13d
    mov     eax, ebx
    call    img_px
    inc     r8d
    jmp     .l
.done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rsi
    pop     rbx
    ret

; круг: ecx,edx = центр, r8d = радиус, eax = цвет, r9d = цвет тени (справа-снизу)
img_disc:
    push    rbx
    push    rsi
    push    r12
    push    r13
    push    r14
    push    r15
    mov     r12d, ecx
    mov     r13d, edx
    mov     r14d, r8d
    mov     ebx, eax
    mov     r15d, r9d
    mov     esi, r14d
    neg     esi
.y:
    cmp     esi, r14d
    jg      .done
    mov     r11d, r14d
    neg     r11d
.x:
    cmp     r11d, r14d
    jg      .ny
    mov     eax, r11d
    imul    eax, eax
    mov     ecx, esi
    imul    ecx, ecx
    add     eax, ecx
    mov     ecx, r14d
    imul    ecx, ecx
    add     ecx, r14d
    cmp     eax, ecx
    jg      .nx
    mov     eax, ebx
    lea     ecx, [r11d*2]
    add     ecx, esi
    imul    ecx, ecx, 3
    lea     edx, [r14d*2]
    cmp     ecx, edx
    jle     .base
    mov     eax, r15d
.base:
    lea     ecx, [r12d + r11d]
    lea     edx, [r13d + esi]
    call    img_px
.nx:
    inc     r11d
    jmp     .x
.ny:
    inc     esi
    jmp     .y
.done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rsi
    pop     rbx
    ret

gen_props:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40

    ; ================= снеговик 14x22
    lea     rdi, [spr_snowman]
    mov     r10d, 14
    mov     ecx, 7
    mov     edx, 17
    mov     r8d, 4
    mov     eax, 0x01E8EEF4
    mov     r9d, 0x01B8C2D0
    call    img_disc
    mov     ecx, 7
    mov     edx, 10
    mov     r8d, 3
    mov     eax, 0x01ECF0F6
    mov     r9d, 0x01BCC6D4
    call    img_disc
    mov     ecx, 7
    mov     edx, 5
    mov     r8d, 2
    mov     eax, 0x01F0F4F8
    mov     r9d, 0x01C0CAD8
    call    img_disc
    IRECT   6, 0, 8, 0, 0x01587090      ; ведро
    IRECT   5, 1, 9, 1, 0x01486080
    IRECT   5, 2, 9, 2, 0x01344658
    IRECT   6, 4, 6, 4, 0x01141414      ; угольки
    IRECT   8, 4, 8, 4, 0x01141414
    IRECT   9, 5, 10, 5, 0x01E07830     ; морковка
    IRECT   11, 5, 11, 5, 0x01B05820
    IRECT   7, 9, 7, 9, 0x01181818
    IRECT   7, 11, 7, 11, 0x01181818
    IRECT   7, 15, 7, 15, 0x01181818
    mov     ecx, 4
    mov     edx, 10
    mov     r8d, 1
    mov     r9d, 7
    mov     eax, 0x01402C1C
    call    img_line
    mov     ecx, 10
    mov     edx, 10
    mov     r8d, 13
    mov     r9d, 7
    mov     eax, 0x01402C1C
    call    img_line
    IRECT   0, 6, 0, 6, 0x01402C1C
    IRECT   13, 6, 13, 6, 0x01402C1C

    ; ================= лавочка 20x12
    lea     rdi, [spr_bench]
    mov     r10d, 20
    IRECT   2, 0, 17, 0, 0x01E4EAF0
    IRECT   2, 1, 17, 1, 0x01604428
    IRECT   2, 2, 17, 2, 0x014A3420
    IRECT   3, 3, 3, 5, 0x01402C1C
    IRECT   16, 3, 16, 5, 0x01402C1C
    IRECT   1, 5, 18, 5, 0x01E8EEF2
    IRECT   1, 6, 18, 6, 0x016C4C2E
    IRECT   1, 7, 18, 7, 0x01503820
    IRECT   2, 8, 3, 11, 0x01382818
    IRECT   16, 8, 17, 11, 0x01382818

    ; ================= бельевая верёвка 32x26: столбы и верёвка
    lea     rdi, [spr_line]
    mov     r10d, 32
    IRECT   1, 2, 1, 25, 0x01504A40
    IRECT   30, 2, 30, 25, 0x01504A40
    IRECT   0, 2, 2, 2, 0x01403A32
    IRECT   29, 2, 31, 2, 0x01403A32
    IRECT   0, 1, 2, 1, 0x01E4EAF0
    IRECT   29, 1, 31, 1, 0x01E4EAF0
    mov     r12d, 2
.lx:
    lea     eax, [r12d - 1]
    mov     ecx, 30
    sub     ecx, r12d
    imul    eax, ecx
    imul    eax, eax, 10
    add     eax, 420
    xor     edx, edx
    mov     ecx, 841
    div     ecx
    lea     edx, [eax + 3]
    mov     ecx, r12d
    mov     eax, 0x01686C74
    call    img_px
    inc     r12d
    cmp     r12d, 29
    jbe     .lx
    ; бельё (отдельный слой — качается)
    lea     rdi, [spr_linei]
    mov     r10d, 32
    IRECT   5, 6, 13, 7, 0x018A3C34     ; рубаха
    IRECT   5, 8, 6, 9, 0x01702E28
    IRECT   12, 8, 13, 9, 0x01702E28
    IRECT   7, 8, 11, 14, 0x018A3C34
    IRECT   9, 8, 9, 13, 0x01602620
    IRECT   7, 14, 11, 14, 0x01702E28
    IRECT   6, 6, 12, 6, 0x01C8D0DA     ; иней
    IRECT   7, 5, 7, 5, 0x01A08058      ; прищепки
    IRECT   11, 5, 11, 5, 0x01A08058
    IRECT   15, 6, 25, 17, 0x01C4C8CC   ; простыня
    IRECT   15, 10, 25, 10, 0x0190A0B8
    IRECT   15, 6, 25, 6, 0x01DDE2E8
    IRECT   18, 18, 19, 18, 0x01B4B8BC
    IRECT   23, 18, 23, 18, 0x01B4B8BC
    IRECT   16, 5, 16, 5, 0x01A08058
    IRECT   24, 5, 24, 5, 0x01A08058
    IRECT   26, 5, 27, 9, 0x01405070    ; носки
    IRECT   26, 9, 28, 10, 0x01405070

    ; ================= собачья будка 22x18 (пустая, цепь лежит в снегу)
    lea     rdi, [spr_kennel]
    mov     r10d, 22
    xor     r12d, r12d
.kr:
    lea     eax, [r12d + 2]             ; полуширина ската
    mov     ecx, 9
    sub     ecx, eax
    mov     r13d, ecx
    lea     r8d, [rax + 9]
    mov     r14d, r8d
    lea     edx, [r12d + 1]
    mov     r9d, edx
    mov     eax, 0x01402A1A
    call    img_rect
    mov     ecx, r13d
    lea     edx, [r12d + 1]
    mov     eax, 0x01E4EAF0
    call    img_px
    mov     ecx, r14d
    lea     edx, [r12d + 1]
    mov     eax, 0x01D8E0EA
    call    img_px
    inc     r12d
    cmp     r12d, 7
    jb      .kr
    IRECT   8, 0, 10, 0, 0x01E8EEF2
    IRECT   7, 1, 11, 1, 0x01E8EEF2
    IRECT   3, 8, 15, 16, 0x01604028
    IRECT   5, 8, 5, 16, 0x014A3020
    IRECT   8, 8, 8, 16, 0x014A3020
    IRECT   11, 8, 11, 16, 0x014A3020
    IRECT   14, 8, 14, 16, 0x014A3020
    IRECT   16, 8, 18, 16, 0x01482E1C
    IRECT   16, 7, 18, 7, 0x01E4EAF0
    IRECT   7, 11, 11, 16, 0x01100C0A
    IRECT   8, 10, 10, 10, 0x01100C0A
    IRECT   3, 17, 18, 17, 0x01D8E0EA
    IRECT   12, 16, 12, 16, 0x01788088  ; цепь
    IRECT   13, 17, 13, 17, 0x01788088
    IRECT   15, 17, 15, 17, 0x01788088
    IRECT   17, 16, 17, 16, 0x01788088
    IRECT   18, 15, 20, 16, 0x01506478  ; миска
    IRECT   19, 15, 19, 15, 0x01E8EEF2

    ; ================= скворечник на шесте 8x46
    lea     rdi, [spr_birdh]
    mov     r10d, 8
    IRECT   3, 10, 3, 45, 0x01585048
    IRECT   4, 10, 4, 45, 0x01403830
    IRECT   1, 4, 6, 10, 0x01705030
    IRECT   5, 4, 6, 10, 0x01583C24
    IRECT   3, 6, 4, 7, 0x01100C0A
    IRECT   0, 3, 7, 3, 0x01402818
    IRECT   1, 2, 6, 2, 0x01E4EAF0
    IRECT   2, 1, 5, 1, 0x01ECF0F4

    ; ================= колодец-журавль 40x58
    lea     rdi, [spr_well]
    mov     r10d, 40
    mov     r12d, 46
.wl:
    mov     eax, r12d
    sub     eax, 46
    xor     edx, edx
    mov     ecx, 3
    div     ecx
    mov     eax, 0x01704C30
    test    edx, edx
    jnz     .wl1
    mov     eax, 0x014A3420
.wl1:
    mov     ecx, 4
    mov     edx, r12d
    mov     r8d, 17
    mov     r9d, r12d
    call    img_rect
    mov     ecx, 4
    mov     edx, r12d
    mov     eax, 0x01A07C58
    call    img_px
    mov     ecx, 17
    mov     edx, r12d
    mov     eax, 0x01A07C58
    call    img_px
    inc     r12d
    cmp     r12d, 57
    jbe     .wl
    IRECT   5, 43, 16, 43, 0x01E8EEF2
    IRECT   5, 44, 16, 45, 0x015A3C28
    IRECT   7, 44, 14, 45, 0x01080808
    IRECT   27, 19, 27, 57, 0x01503824
    IRECT   28, 19, 28, 57, 0x01382818
    IRECT   26, 17, 26, 19, 0x01503824
    IRECT   29, 17, 29, 19, 0x01503824
    mov     ecx, 38
    mov     edx, 28
    mov     r8d, 8
    mov     r9d, 2
    mov     eax, 0x01584030
    call    img_line
    mov     ecx, 38
    mov     edx, 29
    mov     r8d, 8
    mov     r9d, 3
    mov     eax, 0x01402C20
    call    img_line
    ; снег по жерди через пиксель
    mov     r12d, 10
.ws:
    lea     eax, [r12d - 8]
    imul    eax, eax, 26
    xor     edx, edx
    mov     ecx, 30
    div     ecx
    lea     edx, [eax + 1]
    mov     ecx, r12d
    mov     eax, 0x01E4EAF0
    call    img_px
    add     r12d, 3
    cmp     r12d, 36
    jbe     .ws
    IRECT   35, 27, 39, 31, 0x01403028  ; противовес
    IRECT   35, 26, 39, 26, 0x01E4EAF0
    mov     r12d, 3
.wc:
    mov     ecx, 8
    mov     edx, r12d
    mov     eax, 0x01606468
    call    img_px
    add     r12d, 2
    cmp     r12d, 37
    jbe     .wc
    IRECT   6, 38, 10, 41, 0x015A6478   ; ведро
    IRECT   6, 38, 10, 38, 0x01404858

    ; ================= печные трубы 8x24: топится / холодная (в снегу)
    lea     rdi, [spr_chim]
    mov     r10d, 8
    call    .chimney
    lea     rdi, [spr_chims]
    call    .chimney
    IRECT   0, 0, 7, 1, 0x01EEF2F6
    IRECT   0, 2, 0, 3, 0x01D8E4F0
    IRECT   7, 2, 7, 2, 0x01D8E4F0

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
.chimney:
    IRECT   1, 2, 6, 23, 0x01D4DAE0
    IRECT   5, 2, 6, 23, 0x01AEB4BC
    mov     r12d, 4
.cb:
    mov     ecx, 1
    mov     edx, r12d
    mov     r8d, 6
    mov     r9d, r12d
    mov     eax, 0x01B4BAC4
    call    img_rect
    add     r12d, 3
    cmp     r12d, 23
    jbe     .cb
    IRECT   0, 0, 7, 1, 0x01C0C6CE
    IRECT   2, 0, 5, 0, 0x01202020
    ret

; ---------------------------------------------------------------- расстановка
; мелочи на участке L_x0.. (вызывается из make_lot)
lot_props:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40
    call    randf
    comiss  xmm0, [f_0_35]
    jae     .nsm
    mov     edi, T_SNOWMAN
    call    .place
.nsm:
    call    randf
    comiss  xmm0, [f_0_4]
    jae     .nln
    mov     edi, T_LINE
    call    .place
.nln:
    call    randf
    comiss  xmm0, [f_0_33]
    jae     .nkn
    mov     edi, T_KENNEL
    call    .place
.nkn:
    call    randf
    comiss  xmm0, [f_0_3]
    jae     .nbh
    mov     edi, T_BIRDH
    call    .place
.nbh:
    ; лавочка у калитки, снаружи
    call    randf
    comiss  xmm0, [f_0_6]
    jae     .out
    mov     eax, [L_side]
    cmp     eax, 1
    jb      .bs
    je      .bn
    cmp     eax, 2
    je      .be
    mov     ebx, [L_x0]                 ; запад
    dec     ebx
    mov     esi, [L_h]
    shr     esi, 1
    add     esi, [L_y0]
    sub     esi, 3
    jmp     .bset
.be:
    mov     ebx, [L_x0]
    add     ebx, [L_w]
    mov     esi, [L_h]
    shr     esi, 1
    add     esi, [L_y0]
    sub     esi, 3
    jmp     .bset
.bs:
    mov     esi, [L_y0]
    add     esi, [L_h]
    jmp     .bx
.bn:
    mov     esi, [L_y0]
    dec     esi
.bx:
    mov     ebx, [L_w]
    shr     ebx, 1
    add     ebx, [L_x0]
    sub     ebx, 3
.bset:
    mov     ecx, ebx
    mov     edx, esi
    call    get_tile
    test    eax, eax
    jnz     .out
    mov     ecx, ebx
    mov     edx, esi
    mov     eax, T_BENCH
    call    set_tile
.out:
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
; edi = тайл: до 12 попыток найти клетку с чистым крестом
.place:
    mov     r12d, 12
.pt:
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
    jz      .pn
    mov     ecx, ebx
    mov     edx, esi
    mov     eax, edi
    call    set_tile
    ret
.pn:
    dec     r12d
    jnz     .pt
    ret

; колодцы у перекрёстков: нужен свободный квадрат 3x3
place_wells:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32
    lea     rdi, [well_spots]
.l:
    movzx   ebx, byte [rdi]
    cmp     ebx, 0xFF
    je      .out
    movzx   esi, byte [rdi + 1]
    mov     ecx, ebx
    mov     edx, esi
    call    snow_cross4
    test    eax, eax
    jz      .n
    lea     ecx, [ebx - 1]
    lea     edx, [esi - 1]
    call    get_tile
    test    eax, eax
    jnz     .n
    lea     ecx, [ebx + 1]
    lea     edx, [esi + 1]
    call    get_tile
    test    eax, eax
    jnz     .n
    lea     ecx, [ebx + 1]
    lea     edx, [esi - 1]
    call    get_tile
    test    eax, eax
    jnz     .n
    lea     ecx, [ebx - 1]
    lea     edx, [esi + 1]
    call    get_tile
    test    eax, eax
    jnz     .n
    mov     ecx, ebx
    mov     edx, esi
    mov     eax, T_WELL
    call    set_tile
.n:
    add     rdi, 2
    jmp     .l
.out:
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret
