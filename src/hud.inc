; ----------------------------------------------------------------------------
;  hud.inc — HUD в разрешении окна
;
;  Кадр 640x360 растягивается ближайшим пикселем в буфер размером с окно (ob), и
;  HUD рисуется уже в нём. Буквы растеризует GDI (сглаженно, под текущий масштаб)
;  по одной в кэш глифов: покрытие и рядом мягкая тень. Полосы, значки, стрелка
;  ветра и подложки — фигуры по расстоянию до контура (прямоугольник со
;  скруглением, отрезок с толщиной, круг, ромб; можно контуром и с растушёвкой),
;  по 4 пикселя за шаг.
;
;  Главный поток в конце render пишет команды HUD (текст и фигуры) в список
;  своего буфера кадра — в единицах кадра 640x360, так что раскладка от окна не
;  зависит; ширины букв для неё — из тех же шрифтов кеглем 256 (fn_adv). Поток
;  вывода растягивает кадр, исполняет список в масштабе окна (U = высота / 360) и
;  выводит. В снимках то же — сразу: кадр без HUD пишется в shotN.bmp (его сверяет
;  cmp.py), с HUD в 1920x1080 — в viewN.bmp.
; ----------------------------------------------------------------------------
section .text

HQMAX   equ 192                         ; команд HUD на кадр
HQSZ    equ 160
HQ_T    equ 0                           ; 1 — текст, 2 — фигура
HQ_F    equ 4                           ; текст: шрифт (| FT_NOSH — без тени)
HQ_X    equ 8                           ;   левый край и верх строки, единицы кадра
HQ_Y    equ 12
HQ_C    equ 16                          ;   цвет ARGB, A — непрозрачность
HQ_TR   equ 20                          ;   разрядка
HQ_N    equ 24                          ;   число знаков
HQ_S    equ 32                          ;   знаки UTF-16
HQ_CH   equ (HQSZ - HQ_S) / 2
HQ_K    equ 4                           ; фигура: вид
HQ_P    equ 8                           ;   ax, ay, bx, by, r, rw, sf — как hs_p
HQ_SC   equ 36                          ;   цвет ARGB

SH_BOX  equ 0                           ; центр a, полуразмеры b, скругление r
SH_CAP  equ 1                           ; отрезок a—b толщиной 2r
SH_CIRC equ 2                           ; круг: центр a, радиус r
SH_DIAM equ 3                           ; ромб: центр a, радиус r
SH_SOFT equ 4                           ; мягкая подложка: центр a, полуразмеры b, спад
                                        ;   sf — по x и y отдельно (плавный, без корня)
                                        ; rw > 0 — только контур полутолщины rw;
                                        ; sf — ширина растушёвки края (иначе 1 px)
MAXO    equ 8192                        ; буфер вывода — не шире и не выше
FT_BIG  equ 0                           ; шрифты: часы, счёт
FT_TXT  equ 1                           ; подписи
FT_KEY  equ 2                           ; клавиши
FT_TINY equ 3                           ; время кадра
FT_TTL  equ 4                           ; название, итог ночи
FT_SUB  equ 5                           ; подзаголовок (у Georgia цифры старого стиля —
FT_MID  equ 6                           ;   поэтому строка итога с часами — этим)
NFT     equ 7
FT_NOSH equ 0x100
FN_SZ   equ 24                          ; fn_def: кегль (единицы), жирность, курсив, имя
NCH     equ 167                         ; знаков: ASCII, А..я, Ё ё и gc_ext
GC_SZ   equ 8*1024*1024                 ; кэш глифов
GR_W    equ 512                         ; холст растеризации глифа (квадрат)

C_WHITE equ 0xFFE8EEF4
C_DIM   equ 0xFFA0AEBE
C_FAINT equ 0xFF6E7A8A

; фигура из чисел: ax, ay, bx, by, r, rw, sf
%macro HSP 7
    mov     dword [hs_ax], __float32__(%1)
    mov     dword [hs_ay], __float32__(%2)
    mov     dword [hs_bx], __float32__(%3)
    mov     dword [hs_by], __float32__(%4)
    mov     dword [hs_r], __float32__(%5)
    mov     dword [hs_rw], __float32__(%6)
    mov     dword [hs_sf], __float32__(%7)
%endmacro
; вид, цвет
%macro SHP 2
    mov     ecx, %1
    mov     edx, %2
    call    hq_shape
%endmacro
; текст в точке hq_tx, hq_ty: шрифт, выравнивание (0 — влево, 1 — по центру, 2 — вправо), строка, цвет
%macro TXTP 4
    mov     ecx, %1
    mov     edx, %2
    lea     r8, [%3]
    mov     r9d, %4
    call    hq_text
%endmacro
; то же с точкой из чисел: шрифт, выравнивание, x, y, строка, цвет
%macro TXT 6
    mov     dword [hq_tx], __float32__(%3)
    mov     dword [hq_ty], __float32__(%4)
    TXTP    %1, %2, %5, %6
%endmacro
; A цвета (reg32) · hq_fade / 256; портит eax
%macro FADE 1
    mov     eax, %1
    shr     eax, 24
    imul    eax, [hq_fade]
    shr     eax, 8
    shl     eax, 24
    and     %1, 0x00FFFFFF
    or      %1, eax
%endmacro
; скаляр xmm -> все 4 места вектора
%macro BCAST 2
    movss   [%1], %2
    movss   [%1 + 4], %2
    movss   [%1 + 8], %2
    movss   [%1 + 12], %2
%endmacro

; ================================================================ знаки
; eax = знак UTF-16 -> eax = номер в наборе (чего нет — «?»); портит r10
hud_slot:
    lea     r10d, [rax - 0x20]
    cmp     r10d, 0x7E - 0x20
    jbe     .asc
    lea     r10d, [rax - 0x410]
    cmp     r10d, 0x44F - 0x410
    jbe     .cyr
    lea     r10, [gc_ext]
.x:
    cmp     word [r10], 0
    je      .q
    cmp     ax, [r10]
    je      .f
    add     r10, 2
    jmp     .x
.f:
    lea     rax, [gc_ext]
    sub     r10, rax
    shr     r10d, 1
    lea     eax, [r10 + 159]
    ret
.q:
    mov     eax, '?' - 0x20
    ret
.asc:
    mov     eax, r10d
    ret
.cyr:
    lea     eax, [r10 + 95]
    ret

; eax = номер в наборе -> eax = знак; портит r10
hud_cp:
    cmp     eax, 95
    jae     .c
    add     eax, 0x20
    ret
.c:
    cmp     eax, 159
    jae     .e
    add     eax, 0x410 - 95
    ret
.e:
    lea     r10, [gc_ext]
    movzx   eax, word [r10 + rax*2 - 159*2]
    ret

; ================================================================ запись команд (главный поток)
; -> rax = место под команду в списке текущего буфера (0 — список полон); портит ecx, edx, r8
hq_slot:
    mov     eax, [fb_cur]
    lea     rdx, [hq_n]
    mov     ecx, [rdx + rax*4]
    cmp     ecx, HQMAX
    jae     .full
    lea     r8d, [rcx + 1]
    mov     [rdx + rax*4], r8d
    imul    eax, eax, HQMAX*HQSZ
    imul    ecx, ecx, HQSZ
    add     eax, ecx
    lea     rdx, [hq_buf]
    add     rax, rdx
    ret
.full:
    xor     eax, eax
    ret

; ecx = шрифт, rdx = строка -> xmm0 = ширина в единицах кадра (с разрядкой hq_trk);
; портит eax, ecx, rdx, r10, r11
hq_measure:
    and     ecx, 0xFF
    imul    ecx, ecx, NCH*4
    lea     r11, [fn_adv]
    add     r11, rcx
    xorps   xmm0, xmm0
    xor     ecx, ecx
.c:
    movzx   eax, word [rdx]
    test    eax, eax
    jz      .e
    call    hud_slot
    addss   xmm0, [r11 + rax*4]
    addss   xmm0, [hq_trk]
    add     rdx, 2
    inc     ecx
    jmp     .c
.e:
    test    ecx, ecx
    jz      .z
    subss   xmm0, [hq_trk]
.z:
    ret

; ecx = шрифт (| FT_NOSH), edx = выравнивание, r8 = строка, r9d = цвет; точка — hq_tx,
; hq_ty -> xmm0 = ширина. Разрядка hq_trk действует на эту строку и сбрасывается
hq_text:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    sub     rsp, 40
    mov     ebx, ecx
    mov     r12d, edx
    mov     rsi, r8
    mov     edi, r9d
    mov     rdx, rsi
    call    hq_measure
    movss   [rsp + 32], xmm0
    movss   xmm1, [hq_tx]
    cmp     r12d, 1
    jb      .al
    movss   xmm2, xmm0
    ja      .r
    mulss   xmm2, [hc_half]
.r:
    subss   xmm1, xmm2
.al:
    movss   [rsp + 36], xmm1
    FADE    edi
    call    hq_slot
    test    rax, rax
    jz      .out
    mov     dword [rax + HQ_T], 1
    mov     [rax + HQ_F], ebx
    mov     ecx, [rsp + 36]
    mov     [rax + HQ_X], ecx
    mov     ecx, [hq_ty]
    mov     [rax + HQ_Y], ecx
    mov     [rax + HQ_C], edi
    mov     ecx, [hq_trk]
    mov     [rax + HQ_TR], ecx
    xor     ecx, ecx
.cp:
    movzx   edx, word [rsi + rcx*2]
    test    edx, edx
    jz      .end
    mov     [rax + HQ_S + rcx*2], dx
    inc     ecx
    cmp     ecx, HQ_CH
    jb      .cp
.end:
    mov     [rax + HQ_N], ecx
.out:
    mov     dword [hq_trk], 0
    movss   xmm0, [rsp + 32]
    add     rsp, 40
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ecx = вид, edx = цвет; фигура — hs_p (rw и sf после записи сбрасываются)
hq_shape:
    push    rbx
    push    rsi
    sub     rsp, 8
    mov     ebx, ecx
    mov     esi, edx
    FADE    esi
    call    hq_slot
    test    rax, rax
    jz      .out
    mov     dword [rax + HQ_T], 2
    mov     [rax + HQ_K], ebx
    movups  xmm0, [hs_p]
    movups  [rax + HQ_P], xmm0
    movsd   xmm0, [hs_p + 16]
    movsd   [rax + HQ_P + 16], xmm0
    mov     ecx, [hs_p + 24]
    mov     [rax + HQ_P + 24], ecx
    mov     [rax + HQ_SC], esi
.out:
    mov     dword [hs_rw], 0
    mov     dword [hs_sf], 0
    add     rsp, 8
    pop     rsi
    pop     rbx
    ret

; ================================================================ шрифты, буфер вывода
; ebx = номер шрифта, ecx = высота (−px) -> rax = HFONT (сглаженный, без ClearType)
fn_create:
    sub     rsp, 0x78
    imul    eax, ebx, FN_SZ
    lea     rdx, [fn_def]
    add     rax, rdx
    mov     rdx, [rax + 16]
    mov     [rsp + 104], rdx            ; имя
    mov     edx, [rax + 4]
    mov     [rsp + 32], rdx             ; жирность
    mov     edx, [rax + 8]
    mov     [rsp + 40], rdx             ; курсив
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword [rsp + 48], 0
    mov     qword [rsp + 56], 0
    mov     qword [rsp + 64], 1         ; DEFAULT_CHARSET
    mov     qword [rsp + 72], 0
    mov     qword [rsp + 80], 0
    mov     qword [rsp + 88], 4         ; ANTIALIASED_QUALITY
    mov     qword [rsp + 96], 0
    call    [CreateFontW]
    add     rsp, 0x78
    ret

; DC буфера вывода и холст глифов; ширины знаков всех шрифтов в единицах кадра
hud_init:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 0x40
    xor     ecx, ecx
    call    [CreateCompatibleDC]
    mov     [ob_dc], rax
    mov     rcx, rax
    mov     edx, 3                      ; COLORONCOLOR — ближайший пиксель, как раньше
    call    [SetStretchBltMode]
    xor     ecx, ecx
    call    [CreateCompatibleDC]
    mov     [gr_dc], rax
    mov     rcx, rax
    lea     rdx, [gr_bmi]
    xor     r8d, r8d
    lea     r9, [gr_bits]
    mov     qword [rsp + 32], 0
    mov     qword [rsp + 40], 0
    call    [CreateDIBSection]
    mov     rcx, [gr_dc]
    mov     rdx, rax
    call    [SelectObject]
    mov     rcx, [gr_dc]
    mov     edx, 1                      ; TRANSPARENT
    call    [SetBkMode]
    mov     rcx, [gr_dc]
    mov     edx, 0x00FFFFFF
    call    [SetTextColor]
    ; ширины: шрифт кеглем 256 px, ширина знака · кегль / 256
    xor     ebx, ebx
.f:
    mov     ecx, -256
    call    fn_create
    mov     rsi, rax
    mov     rcx, [gr_dc]
    mov     rdx, rsi
    call    [SelectObject]
    cmp     qword [gr_font0], 0
    jne     .f0
    mov     [gr_font0], rax             ; родной шрифт холста — чтобы было что выбрать
.f0:
    xor     edi, edi
.s:
    mov     eax, edi
    call    hud_cp
    mov     rcx, [gr_dc]
    mov     edx, eax
    mov     r8d, eax
    lea     r9, [rsp + 48]
    call    [GetCharWidth32W]
    cvtsi2ss xmm0, dword [rsp + 48]
    imul    eax, ebx, FN_SZ
    lea     rdx, [fn_def]
    mulss   xmm0, [rdx + rax]
    mulss   xmm0, [hc_1_256]
    imul    eax, ebx, NCH
    add     eax, edi
    lea     rdx, [fn_adv]
    movss   [rdx + rax*4], xmm0
    inc     edi
    cmp     edi, NCH
    jb      .s
    mov     rcx, [gr_dc]
    mov     rdx, [gr_font0]
    call    [SelectObject]
    mov     rcx, rsi
    call    [DeleteObject]
    inc     ebx
    cmp     ebx, NFT
    jb      .f
    mov     dword [fn_cur], -1
    add     rsp, 0x40
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ecx = W, edx = H: буфер вывода этого размера (строкой больше — запас на запись по
; 4 пикселя), масштаб, шрифты под него, кэш глифов — заново. Тот же размер — ничего
hud_resize:
    cmp     ecx, [ob_w]
    jne     .go
    cmp     edx, [ob_h]
    jne     .go
    cmp     qword [ob_bits], 0
    jne     .same
.go:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 0x40
    mov     [ob_w], ecx
    mov     [ob_h], edx
    mov     [ob_bmi + 4], ecx
    lea     eax, [rdx + 1]
    neg     eax
    mov     [ob_bmi + 8], eax
    mov     rcx, [ob_dc]
    lea     rdx, [ob_bmi]
    xor     r8d, r8d
    lea     r9, [ob_bits]
    mov     qword [rsp + 32], 0
    mov     qword [rsp + 40], 0
    call    [CreateDIBSection]
    mov     rbx, rax
    mov     rcx, [ob_dc]
    mov     rdx, rbx
    call    [SelectObject]
    mov     rcx, [ob_bmp]
    test    rcx, rcx
    jz      .nb
    call    [DeleteObject]
.nb:
    mov     [ob_bmp], rbx
    ; карты растяжки: у каждого столбца и строки вывода — ближайшие кадра (в байтах)
    lea     rdi, [hu_xmap]
    mov     r8d, [ob_w]
    add     r8d, r8d
    xor     ecx, ecx
.xm:
    lea     eax, [rcx*2 + 1]
    imul    eax, eax, SCR_W
    xor     edx, edx
    div     r8d
    shl     eax, 2
    mov     [rdi + rcx*4], eax
    inc     ecx
    cmp     ecx, [ob_w]
    jb      .xm
    lea     rdi, [hu_ymap]
    mov     r8d, [ob_h]
    add     r8d, r8d
    xor     ecx, ecx
.ym:
    lea     eax, [rcx*2 + 1]
    imul    eax, eax, SCR_H
    xor     edx, edx
    div     r8d
    imul    eax, eax, SCR_W*4
    mov     [rdi + rcx*4], eax
    inc     ecx
    cmp     ecx, [ob_h]
    jb      .ym
    ; масштаб; буквы крупнее, чем при U = 8, не растим (холст глифа)
    cvtsi2ss xmm0, dword [ob_h]
    mulss   xmm0, [hc_1_360]
    movss   [hu_u], xmm0
    minss   xmm0, [hc_8]
    movss   [hu_uf], xmm0
    ; тень букв: сдвиг 0.5 U, размытие 0.6 U (не меньше пикселя), поле под неё
    movss   xmm0, [hu_u]
    mulss   xmm0, [hc_half]
    cvtss2si eax, xmm0
    mov     ecx, 1
    cmp     eax, ecx
    cmovl   eax, ecx
    mov     [g_sho], eax
    movss   xmm0, [hu_u]
    mulss   xmm0, [hc_0_6]
    cvtss2si edx, xmm0
    cmp     edx, ecx
    cmovl   edx, ecx
    mov     [g_rb], edx
    lea     eax, [rax + rdx + 1]
    mov     [g_m], eax
    ; шрифты под масштаб
    mov     rcx, [gr_dc]
    mov     rdx, [gr_font0]
    call    [SelectObject]
    mov     dword [fn_cur], -1
    xor     ebx, ebx
.f:
    lea     rsi, [fn_h]
    mov     rcx, [rsi + rbx*8]
    test    rcx, rcx
    jz      .nf
    call    [DeleteObject]
.nf:
    imul    eax, ebx, FN_SZ
    lea     rdx, [fn_def]
    movss   xmm0, [rdx + rax]
    mulss   xmm0, [hu_uf]
    cvtss2si ecx, xmm0
    mov     eax, 6
    cmp     ecx, eax
    cmovl   ecx, eax
    neg     ecx
    call    fn_create
    lea     rsi, [fn_h]
    mov     [rsi + rbx*8], rax
    inc     ebx
    cmp     ebx, NFT
    jb      .f
    lea     rdi, [gc_tab]
    xor     eax, eax
    mov     ecx, NFT*NCH*16/8
    rep     stosq
    mov     dword [gc_pos], 0
    add     rsp, 0x40
    pop     rdi
    pop     rsi
    pop     rbx
.same:
    ret

; ================================================================ кэш глифов
; ecx = шрифт, edx = номер знака -> rax = запись gc_tab; при первом обращении
; знак растеризуется. Запись: dd смещение в gc_arena + 1 (0 — ещё нет), dw w, h,
; dw ox, oy — клетка от пера и верха строки (со знаком). В клетке w·h байт
; покрытия, за ними w·h байт тени (сдвинута и размыта)
hud_glyph:
    imul    eax, ecx, NCH
    add     eax, edx
    shl     eax, 4
    lea     r8, [gc_tab]
    add     rax, r8
    cmp     dword [rax], 0
    je      .make
    ret
.make:
    push    rbx
    push    rsi
    push    rdi
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 72
    mov     r12, rax
    mov     r14d, edx
    cmp     ecx, [fn_cur]
    je      .sel
    mov     [fn_cur], ecx
    lea     rax, [fn_h]
    mov     rdx, [rax + rcx*8]
    mov     rcx, [gr_dc]
    call    [SelectObject]
.sel:
    mov     eax, r14d
    call    hud_cp
    mov     [rsp + 64], rax             ; знак и ноль
    mov     rcx, [gr_dc]
    lea     rdx, [rsp + 64]
    mov     r8d, 1
    lea     r9, [rsp + 48]              ; SIZE
    call    [GetTextExtentPoint32W]
    ; поле: вокруг знака — поля тени и полстроки на свесы
    mov     eax, [rsp + 52]
    shr     eax, 1
    add     eax, [g_m]
    mov     ebp, eax                    ; отступ
    mov     eax, [rsp + 48]
    lea     eax, [rax + rbp*2]
    mov     ecx, [rsp + 52]
    lea     ecx, [rcx + rbp*2]
    mov     edx, GR_W
    cmp     eax, edx
    cmova   eax, edx
    cmp     ecx, edx
    cmova   ecx, edx
    mov     [rsp + 56], eax             ; ширина поля
    mov     [rsp + 60], ecx             ; высота
    xor     ebx, ebx
.clr:
    mov     rdi, [gr_bits]
    imul    eax, ebx, GR_W*4
    add     rdi, rax
    mov     ecx, [rsp + 56]
    xor     eax, eax
    rep     stosd
    inc     ebx
    cmp     ebx, [rsp + 60]
    jb      .clr
    mov     rcx, [gr_dc]
    mov     edx, ebp
    mov     r8d, ebp
    lea     r9, [rsp + 64]
    mov     qword [rsp + 32], 1
    call    [TextOutW]
    call    [GdiFlush]
    ; рамка покрытия (канал G)
    mov     r8d, [rsp + 56]             ; x0
    mov     r9d, -1                     ; x1
    mov     r10d, [rsp + 60]            ; y0
    mov     r11d, -1                    ; y1
    xor     ebx, ebx
.by:
    mov     rsi, [gr_bits]
    imul    eax, ebx, GR_W*4
    lea     rsi, [rsi + rax + 1]
    xor     ecx, ecx
.bx:
    cmp     byte [rsi + rcx*4], 0
    je      .bn
    cmp     ecx, r8d
    cmovb   r8d, ecx
    cmp     ecx, r9d
    cmovg   r9d, ecx
    cmp     ebx, r10d
    cmovb   r10d, ebx
    mov     r11d, ebx
.bn:
    inc     ecx
    cmp     ecx, [rsp + 56]
    jb      .bx
    inc     ebx
    cmp     ebx, [rsp + 60]
    jb      .by
    test    r9d, r9d
    jns     .ink
    mov     dword [r12], 1              ; пустой знак (пробел)
    mov     dword [r12 + 4], 0
    jmp     .ret
.ink:
    ; клетка: рамка с полями тени, в пределах поля
    mov     eax, [g_m]
    sub     r8d, eax
    jns     .x0
    xor     r8d, r8d
.x0:
    sub     r10d, eax
    jns     .y0
    xor     r10d, r10d
.y0:
    lea     r9d, [r9 + rax + 1]
    cmp     r9d, [rsp + 56]
    cmova   r9d, [rsp + 56]
    lea     r11d, [r11 + rax + 1]
    cmp     r11d, [rsp + 60]
    cmova   r11d, [rsp + 60]
    sub     r9d, r8d
    sub     r11d, r10d
    mov     [rsp + 40], r8d             ; левый верх клетки на холсте
    mov     [rsp + 44], r10d
    mov     r13d, r9d                   ; w
    mov     r14d, r11d                  ; h
    ; место в кэше; не влезло — кэш заново (прочие знаки растеризуются снова)
    mov     eax, r13d
    imul    eax, r14d
    lea     eax, [rax*2 + 3]
    and     eax, -4
    mov     ebx, eax
    mov     ecx, [gc_pos]
    add     ecx, eax
    cmp     ecx, GC_SZ
    jbe     .fit
    lea     rdi, [gc_tab]
    xor     eax, eax
    mov     ecx, NFT*NCH*16/8
    rep     stosq
    mov     dword [gc_pos], 0
    mov     ecx, ebx
.fit:
    mov     edx, [gc_pos]
    mov     [gc_pos], ecx
    lea     r15, [gc_arena]
    add     r15, rdx
    inc     edx
    mov     [r12], edx
    mov     [r12 + 4], r13w
    mov     [r12 + 6], r14w
    mov     eax, [rsp + 40]
    sub     eax, ebp
    mov     [r12 + 8], ax
    mov     eax, [rsp + 44]
    sub     eax, ebp
    mov     [r12 + 10], ax
    ; покрытие
    xor     ebx, ebx
.cy:
    mov     eax, [rsp + 44]
    add     eax, ebx
    imul    eax, eax, GR_W
    add     eax, [rsp + 40]
    mov     rsi, [gr_bits]
    lea     rsi, [rsi + rax*4 + 1]
    mov     eax, ebx
    imul    eax, r13d
    lea     rdi, [r15 + rax]
    xor     ecx, ecx
.cx:
    mov     al, [rsi + rcx*4]
    mov     [rdi + rcx], al
    inc     ecx
    cmp     ecx, r13d
    jb      .cx
    inc     ebx
    cmp     ebx, r14d
    jb      .cy
    ; тень: покрытие со сдвигом (sho, sho), размытое квадратом (2rb+1)², ×1.75.
    ; Суммы по строкам — в холст (он уже не нужен), потом по столбцам
    mov     r8d, [g_sho]
    mov     r9d, [g_rb]
    xor     ebx, ebx
.hy:
    mov     eax, ebx
    imul    eax, r13d
    lea     rsi, [r15 + rax]
    mov     rdi, [gr_bits]
    lea     rdi, [rdi + rax*4]
    xor     ecx, ecx
.hx:
    xor     edx, edx
    mov     r10d, ecx
    sub     r10d, r8d
    sub     r10d, r9d
    lea     r11d, [r10 + r9*2]
.hk:
    cmp     r10d, r13d
    jae     .hs
    movzx   eax, byte [rsi + r10]
    add     edx, eax
.hs:
    inc     r10d
    cmp     r10d, r11d
    jle     .hk
    mov     [rdi + rcx*4], edx
    inc     ecx
    cmp     ecx, r13d
    jb      .hx
    inc     ebx
    cmp     ebx, r14d
    jb      .hy
    lea     eax, [r9*2 + 1]
    imul    eax, eax
    mov     ecx, eax
    mov     eax, 114688                 ; 1.75 · 65536
    xor     edx, edx
    div     ecx
    mov     [rsp + 48], eax
    mov     eax, r13d
    imul    eax, r14d
    lea     rax, [r15 + rax]
    mov     [rsp + 56], rax             ; карта тени
    xor     ebx, ebx
.vy:
    xor     ecx, ecx
.vx:
    xor     edx, edx
    mov     r10d, ebx
    sub     r10d, r8d
    sub     r10d, r9d
    lea     r11d, [r10 + r9*2]
.vk:
    cmp     r10d, r14d
    jae     .vs
    mov     eax, r10d
    imul    eax, r13d
    add     eax, ecx
    mov     rsi, [gr_bits]
    add     edx, [rsi + rax*4]
.vs:
    inc     r10d
    cmp     r10d, r11d
    jle     .vk
    imul    edx, [rsp + 48]
    shr     edx, 16
    mov     eax, 255
    cmp     edx, eax
    cmova   edx, eax
    mov     eax, ebx
    imul    eax, r13d
    add     eax, ecx
    mov     rsi, [rsp + 56]
    mov     [rsi + rax], dl
    inc     ecx
    cmp     ecx, r13d
    jb      .vx
    inc     ebx
    cmp     ebx, r14d
    jb      .vy
.ret:
    mov     rax, r12
    add     rsp, 72
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ================================================================ исполнение (поток вывода)
; rcx = запись глифа, edx = X, r8d = Y (px, левый верх клетки), r9 = карта w·h;
; цвет — gl_col (A — непрозрачность)
hud_blit:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 32
    movdqa  [rsp], xmm6
    movdqa  [rsp + 16], xmm7
    movzx   r10d, word [rcx + 4]
    movzx   r11d, word [rcx + 6]
    mov     eax, [gl_col]
    movd    xmm4, eax
    pxor    xmm5, xmm5
    punpcklbw xmm4, xmm5
    punpcklqdq xmm4, xmm4               ; цвет двух пикселей словами
    shr     eax, 24
    mov     ebx, eax
    movd    xmm3, eax
    pshuflw xmm3, xmm3, 0               ; A словами
    mov     rsi, [ob_bits]
    xor     r12d, r12d
.row:
    cmp     r12d, r11d
    jae     .done
    lea     eax, [r8 + r12]
    cmp     eax, [ob_h]
    jae     .nrow
    imul    eax, [ob_w]
    lea     r13, [rsi + rax*4]
    mov     eax, r12d
    imul    eax, r10d
    lea     rdi, [r9 + rax]
    xor     r14d, r14d
    ; по 4 пикселя: пустые четвёрки — мимо; у края буфера и в хвосте клетки — по одному
.px4:
    lea     eax, [r14 + 4]
    cmp     eax, r10d
    ja      .px
    mov     eax, [rdi + r14]
    test    eax, eax
    jz      .n4
    lea     r15d, [rdx + r14]
    test    r15d, r15d
    js      .px
    lea     ecx, [r15 + 4]
    cmp     ecx, [ob_w]
    ja      .px
    movd    xmm0, eax
    punpcklbw xmm0, xmm5
    pmullw  xmm0, xmm3
    psrlw   xmm0, 9                     ; 0..127
    punpcklwd xmm0, xmm0
    movdqa  xmm1, xmm0
    punpckldq xmm0, xmm0
    punpckhdq xmm1, xmm1
    movdqu  xmm2, [r13 + r15*4]
    movdqa  xmm6, xmm2
    punpcklbw xmm2, xmm5
    punpckhbw xmm6, xmm5
    movdqa  xmm7, xmm4
    psubw   xmm7, xmm2
    pmullw  xmm7, xmm0
    psraw   xmm7, 7
    paddw   xmm2, xmm7
    movdqa  xmm7, xmm4
    psubw   xmm7, xmm6
    pmullw  xmm7, xmm1
    psraw   xmm7, 7
    paddw   xmm6, xmm7
    packuswb xmm2, xmm6
    movdqu  [r13 + r15*4], xmm2
.n4:
    add     r14d, 4
    jmp     .px4
.px:
    cmp     r14d, r10d
    jae     .nrow
    movzx   eax, byte [rdi + r14]
    test    eax, eax
    jz      .npx
    lea     r15d, [rdx + r14]
    cmp     r15d, [ob_w]
    jae     .npx
    imul    eax, ebx
    shr     eax, 9                      ; 0..127
    movd    xmm0, [r13 + r15*4]
    punpcklbw xmm0, xmm5
    movdqa  xmm1, xmm4
    psubw   xmm1, xmm0
    movd    xmm2, eax
    pshuflw xmm2, xmm2, 0
    pmullw  xmm1, xmm2
    psraw   xmm1, 7
    paddw   xmm0, xmm1
    packuswb xmm0, xmm0
    movd    [r13 + r15*4], xmm0
.npx:
    inc     r14d
    jmp     .px
.nrow:
    inc     r12d
    jmp     .row
.done:
    movdqa  xmm6, [rsp]
    movdqa  xmm7, [rsp + 16]
    add     rsp, 32
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; rsi = команда-текст: тень (если есть), потом буквы
hud_do_text:
    push    rbx
    push    rdi
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 48
    mov     r12d, [rsi + HQ_F]
    and     r12d, 0xFF
    movss   xmm0, [rsi + HQ_Y]
    mulss   xmm0, [hu_u]
    cvtss2si eax, xmm0
    mov     [rsp + 32], eax
    xor     ebp, ebp                    ; 0 — тень, 1 — буквы
    test    dword [rsi + HQ_F], FT_NOSH
    jz      .pass
    inc     ebp
.pass:
    mov     eax, [rsi + HQ_C]
    test    ebp, ebp
    jnz     .col
    shr     eax, 24                     ; тень: почти чёрная, 0.85 непрозрачности букв
    imul    eax, eax, 218
    shr     eax, 8
    shl     eax, 24
    or      eax, 0x00080A10
.col:
    mov     [gl_col], eax
    mov     eax, [rsi + HQ_X]
    mov     [rsp + 36], eax             ; перо, единицы кадра
    xor     ebx, ebx
.ch:
    cmp     ebx, [rsi + HQ_N]
    jae     .pend
    movzx   eax, word [rsi + HQ_S + rbx*2]
    call    hud_slot
    mov     r15d, eax
    mov     ecx, r12d
    mov     edx, eax
    call    hud_glyph
    movzx   ecx, word [rax + 4]
    test    ecx, ecx
    jz      .adv
    movzx   r9d, word [rax + 6]
    imul    r9d, ecx
    mov     r10d, [rax]
    dec     r10d
    lea     r11, [gc_arena]
    add     r10, r11
    test    ebp, ebp
    jnz     .map
    add     r10, r9
.map:
    movss   xmm0, [rsp + 36]
    mulss   xmm0, [hu_u]
    cvtss2si edx, xmm0
    movsx   ecx, word [rax + 8]
    add     edx, ecx
    movsx   r8d, word [rax + 10]
    add     r8d, [rsp + 32]
    mov     r9, r10
    mov     rcx, rax
    call    hud_blit
.adv:
    imul    eax, r12d, NCH
    add     eax, r15d
    lea     rcx, [fn_adv]
    movss   xmm0, [rsp + 36]
    addss   xmm0, [rcx + rax*4]
    addss   xmm0, [rsi + HQ_TR]
    movss   [rsp + 36], xmm0
    inc     ebx
    jmp     .ch
.pend:
    inc     ebp
    cmp     ebp, 2
    jb      .pass
    add     rsp, 48
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rdi
    pop     rbx
    ret

; rsi = команда-фигура: покрытие clamp(0.5 − d / s) по расстоянию d до контура,
; s — растушёвка (не меньше пикселя); по 4 пикселя строки за шаг
hud_do_shape:
    push    rbx
    push    rdi
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 128
    movdqa  [rsp + 32], xmm6
    movdqa  [rsp + 48], xmm7
    movdqa  [rsp + 64], xmm8
    movdqa  [rsp + 80], xmm9
    movdqa  [rsp + 96], xmm10
    movss   xmm7, [hu_u]
    movss   xmm0, [rsi + HQ_P + 24]
    mulss   xmm0, xmm7
    maxss   xmm0, [hc_1]
    movss   xmm1, [hc_1]
    divss   xmm1, xmm0
    BCAST   sd_invs, xmm1
    mulss   xmm0, [hc_half]
    addss   xmm0, [hc_1]                ; поле рамки: s/2 + 1
    xor     ebp, ebp
    movss   xmm1, [rsi + HQ_P + 20]
    comiss  xmm1, [hc_0]
    jbe     .nr
    inc     ebp                         ; контур
.nr:
    mulss   xmm1, xmm7
    BCAST   sd_rw, xmm1
    addss   xmm0, xmm1
    movss   xmm1, [rsi + HQ_P + 16]
    mulss   xmm1, xmm7
    BCAST   sd_r, xmm1
    movss   xmm2, [rsi + HQ_P]
    mulss   xmm2, xmm7
    movss   xmm3, [rsi + HQ_P + 4]
    mulss   xmm3, xmm7
    movss   xmm4, [rsi + HQ_P + 8]
    mulss   xmm4, xmm7
    movss   xmm5, [rsi + HQ_P + 12]
    mulss   xmm5, xmm7
    BCAST   sd_ax, xmm2
    BCAST   sd_ay, xmm3
    mov     ebx, [rsi + HQ_K]
    cmp     ebx, SH_BOX
    je      .isbox
    cmp     ebx, SH_SOFT
    jne     .nbox
.isbox:
    ; прямоугольник: q = |p − a| − (b − r); рамка a ± b (подложка — так же, r = 0)
    movss   xmm8, xmm4
    subss   xmm8, xmm1
    BCAST   sd_bx, xmm8
    movss   xmm8, xmm5
    subss   xmm8, xmm1
    BCAST   sd_by, xmm8
    movss   xmm8, xmm2
    subss   xmm8, xmm4
    subss   xmm8, xmm0
    movss   xmm9, xmm2
    addss   xmm9, xmm4
    addss   xmm9, xmm0
    movss   xmm10, xmm3
    subss   xmm10, xmm5
    subss   xmm10, xmm0
    movss   xmm6, xmm3
    addss   xmm6, xmm5
    addss   xmm6, xmm0
    jmp     .bb
.nbox:
    addss   xmm0, xmm1                  ; + r
    cmp     ebx, SH_CAP
    jne     .ncap
    ; отрезок: ba = b − a, 1 / |ba|²; рамка по концам
    movss   xmm8, xmm4
    subss   xmm8, xmm2
    BCAST   sd_bx, xmm8
    movss   xmm9, xmm5
    subss   xmm9, xmm3
    BCAST   sd_by, xmm9
    mulss   xmm8, xmm8
    mulss   xmm9, xmm9
    addss   xmm8, xmm9
    maxss   xmm8, [hc_eps]
    movss   xmm9, [hc_1]
    divss   xmm9, xmm8
    BCAST   sd_idd, xmm9
    movss   xmm8, xmm2
    minss   xmm8, xmm4
    subss   xmm8, xmm0
    movss   xmm9, xmm2
    maxss   xmm9, xmm4
    addss   xmm9, xmm0
    movss   xmm10, xmm3
    minss   xmm10, xmm5
    subss   xmm10, xmm0
    movss   xmm6, xmm3
    maxss   xmm6, xmm5
    addss   xmm6, xmm0
    jmp     .bb
.ncap:
    ; круг, ромб: рамка a ± r
    movss   xmm8, xmm2
    subss   xmm8, xmm0
    movss   xmm9, xmm2
    addss   xmm9, xmm0
    movss   xmm10, xmm3
    subss   xmm10, xmm0
    movss   xmm6, xmm3
    addss   xmm6, xmm0
.bb:
    ; рамка в пикселях, в пределах буфера
    xor     ecx, ecx
    cvttss2si r12d, xmm8                ; x0
    cmp     r12d, ecx
    cmovl   r12d, ecx
    cvttss2si r13d, xmm9                ; x1 (не включая)
    inc     r13d
    cmp     r13d, [ob_w]
    cmovg   r13d, [ob_w]
    cvttss2si r14d, xmm10               ; y0
    cmp     r14d, ecx
    cmovl   r14d, ecx
    cvttss2si eax, xmm6                 ; y1
    inc     eax
    cmp     eax, [ob_h]
    cmovg   eax, [ob_h]
    mov     [rsp + 112], eax
    cmp     r12d, r13d
    jge     .out
    cmp     r14d, eax
    jge     .out
    ; цвет: слова двух пикселей; A·128/255 — множитель покрытия
    mov     eax, [rsi + HQ_SC]
    movd    xmm0, eax
    pxor    xmm1, xmm1
    punpcklbw xmm0, xmm1
    punpcklqdq xmm0, xmm0
    movdqa  [sd_col], xmm0
    shr     eax, 24
    cvtsi2ss xmm0, eax
    mulss   xmm0, [hc_128_255]
    BCAST   sd_ak, xmm0
    cvtsi2ss xmm0, r13d
    BCAST   sd_x1, xmm0
    pxor    xmm10, xmm10
.y:
    cvtsi2ss xmm9, r14d
    addss   xmm9, [hc_half]
    shufps  xmm9, xmm9, 0               ; py
    mov     eax, r14d
    imul    eax, [ob_w]
    add     eax, r12d
    mov     rdi, [ob_bits]
    lea     rdi, [rdi + rax*4]
    cvtsi2ss xmm8, r12d
    shufps  xmm8, xmm8, 0
    addps   xmm8, [hc_lane]             ; px
    mov     r15d, r12d
.x:
    cmp     ebx, SH_CAP
    je      .dcap
    ja      .dcd
    ; прямоугольник со скруглением
    movaps  xmm0, xmm8
    subps   xmm0, [sd_ax]
    andps   xmm0, [hc_absm]
    subps   xmm0, [sd_bx]
    movaps  xmm1, xmm9
    subps   xmm1, [sd_ay]
    andps   xmm1, [hc_absm]
    subps   xmm1, [sd_by]
    movaps  xmm2, xmm0
    maxps   xmm2, xmm1
    minps   xmm2, xmm10
    maxps   xmm0, xmm10
    maxps   xmm1, xmm10
    mulps   xmm0, xmm0
    mulps   xmm1, xmm1
    addps   xmm0, xmm1
    sqrtps  xmm0, xmm0
    addps   xmm0, xmm2
    subps   xmm0, [sd_r]
    jmp     .cov
.dcap:
    movaps  xmm0, xmm8
    subps   xmm0, [sd_ax]
    movaps  xmm1, xmm9
    subps   xmm1, [sd_ay]
    movaps  xmm2, xmm0
    mulps   xmm2, [sd_bx]
    movaps  xmm3, xmm1
    mulps   xmm3, [sd_by]
    addps   xmm2, xmm3
    mulps   xmm2, [sd_idd]
    maxps   xmm2, xmm10
    minps   xmm2, [hc_one4]
    movaps  xmm3, [sd_bx]
    mulps   xmm3, xmm2
    subps   xmm0, xmm3
    movaps  xmm3, [sd_by]
    mulps   xmm3, xmm2
    subps   xmm1, xmm3
    mulps   xmm0, xmm0
    mulps   xmm1, xmm1
    addps   xmm0, xmm1
    sqrtps  xmm0, xmm0
    subps   xmm0, [sd_r]
    jmp     .cov
.dcd:
    cmp     ebx, SH_SOFT
    je      .dsoft
    movaps  xmm0, xmm8
    subps   xmm0, [sd_ax]
    movaps  xmm1, xmm9
    subps   xmm1, [sd_ay]
    cmp     ebx, SH_DIAM
    je      .ddia
    mulps   xmm0, xmm0
    mulps   xmm1, xmm1
    addps   xmm0, xmm1
    sqrtps  xmm0, xmm0
    subps   xmm0, [sd_r]
    jmp     .cov
.ddia:
    andps   xmm0, [hc_absm]
    andps   xmm1, [hc_absm]
    addps   xmm0, xmm1
    subps   xmm0, [sd_r]
    mulps   xmm0, [hc_rs2]
    jmp     .cov
.dsoft:
    ; подложка: t = tx·ty, tx = clamp((bx − |px − ax|) / s + 0.5), покрытие — t²(3 − 2t)
    movaps  xmm0, xmm8
    subps   xmm0, [sd_ax]
    andps   xmm0, [hc_absm]
    movaps  xmm1, [sd_bx]
    subps   xmm1, xmm0
    mulps   xmm1, [sd_invs]
    addps   xmm1, [hc_half4]
    maxps   xmm1, xmm10
    minps   xmm1, [hc_one4]
    movaps  xmm0, xmm9
    subps   xmm0, [sd_ay]
    andps   xmm0, [hc_absm]
    movaps  xmm2, [sd_by]
    subps   xmm2, xmm0
    mulps   xmm2, [sd_invs]
    addps   xmm2, [hc_half4]
    maxps   xmm2, xmm10
    minps   xmm2, [hc_one4]
    mulps   xmm1, xmm2
    movaps  xmm2, xmm1
    mulps   xmm2, xmm1
    addps   xmm1, xmm1
    movaps  xmm0, [hc_three4]
    subps   xmm0, xmm1
    mulps   xmm0, xmm2
    movaps  xmm1, xmm0
    jmp     .lanes
.cov:
    test    ebp, ebp
    jz      .fill
    andps   xmm0, [hc_absm]             ; контур: |d| − rw
    subps   xmm0, [sd_rw]
.fill:
    mulps   xmm0, [sd_invs]
    movaps  xmm1, [hc_half4]
    subps   xmm1, xmm0
    maxps   xmm1, xmm10
    minps   xmm1, [hc_one4]
.lanes:
    movaps  xmm2, xmm8
    cmpltps xmm2, [sd_x1]               ; за правым краем рамки — ноль
    andps   xmm1, xmm2
    mulps   xmm1, [sd_ak]
    cvttps2dq xmm0, xmm1                ; 0..128
    movdqa  xmm2, xmm10
    pcmpeqd xmm2, xmm0
    pmovmskb eax, xmm2
    cmp     eax, 0xFFFF
    je      .nx
    packssdw xmm0, xmm0
    punpcklwd xmm0, xmm0
    movdqa  xmm1, xmm0
    punpckldq xmm0, xmm0                ; a0 ×4, a1 ×4
    punpckhdq xmm1, xmm1                ; a2 ×4, a3 ×4
    movdqu  xmm2, [rdi]
    movdqa  xmm3, xmm2
    punpcklbw xmm2, xmm10
    punpckhbw xmm3, xmm10
    movdqa  xmm4, [sd_col]
    psubw   xmm4, xmm2
    pmullw  xmm4, xmm0
    psraw   xmm4, 7
    paddw   xmm2, xmm4
    movdqa  xmm4, [sd_col]
    psubw   xmm4, xmm3
    pmullw  xmm4, xmm1
    psraw   xmm4, 7
    paddw   xmm3, xmm4
    packuswb xmm2, xmm3
    movdqu  [rdi], xmm2
.nx:
    addps   xmm8, [hc_four4]
    add     rdi, 16
    add     r15d, 4
    cmp     r15d, r13d
    jl      .x
    inc     r14d
    cmp     r14d, [rsp + 112]
    jl      .y
.out:
    movdqa  xmm6, [rsp + 32]
    movdqa  xmm7, [rsp + 48]
    movdqa  xmm8, [rsp + 64]
    movdqa  xmm9, [rsp + 80]
    movdqa  xmm10, [rsp + 96]
    add     rsp, 128
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rdi
    pop     rbx
    ret

; ecx = буфер кадра: его список HUD — в ob
hud_exec:
    push    rbx
    push    rsi
    sub     rsp, 40
    lea     rax, [hq_n]
    mov     ebx, [rax + rcx*4]
    imul    eax, ecx, HQMAX*HQSZ
    lea     rsi, [hq_buf]
    add     rsi, rax
.c:
    test    ebx, ebx
    jz      .done
    cmp     dword [rsi + HQ_T], 1
    jne     .s
    call    hud_do_text
    jmp     .n
.s:
    call    hud_do_shape
.n:
    add     rsi, HQSZ
    dec     ebx
    jmp     .c
.done:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret

; ecx = буфер кадра: его строки в ob ближайшим пикселем (по hu_xmap, hu_ymap); строка
; вывода с той же строкой кадра, что у предыдущей, — копия предыдущей
hud_scale:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    lea     rax, [fb_bits]
    mov     r12, [rax + rcx*8]
    lea     r13, [hu_xmap]
    lea     r14, [hu_ymap]
    mov     r15d, [ob_w]
    mov     rdi, [ob_bits]
    mov     ebx, -1
    xor     r11d, r11d
.y:
    cmp     r11d, [ob_h]
    jae     .done
    mov     eax, [r14 + r11*4]
    cmp     eax, ebx
    jne     .build
    mov     rsi, rdi
    mov     rax, r15
    shl     rax, 2
    sub     rsi, rax
    mov     rcx, rax
    rep     movsb
    jmp     .next
.build:
    mov     ebx, eax
    lea     rsi, [r12 + rax]
    xor     ecx, ecx
    mov     r10d, r15d
    and     r10d, -4
.x4:
    cmp     ecx, r10d
    jae     .x1
    mov     eax, [r13 + rcx*4]
    mov     edx, [r13 + rcx*4 + 4]
    mov     eax, [rsi + rax]
    mov     edx, [rsi + rdx]
    mov     [rdi + rcx*4], eax
    mov     [rdi + rcx*4 + 4], edx
    mov     eax, [r13 + rcx*4 + 8]
    mov     edx, [r13 + rcx*4 + 12]
    mov     eax, [rsi + rax]
    mov     edx, [rsi + rdx]
    mov     [rdi + rcx*4 + 8], eax
    mov     [rdi + rcx*4 + 12], edx
    add     ecx, 4
    jmp     .x4
.x1:
    cmp     ecx, r15d
    jae     .xe
    mov     eax, [r13 + rcx*4]
    mov     eax, [rsi + rax]
    mov     [rdi + rcx*4], eax
    inc     ecx
    jmp     .x1
.xe:
    lea     rdi, [rdi + r15*4]
.next:
    inc     r11d
    jmp     .y
.done:
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ecx = буфер кадра, edx = W, r8d = H: кадр растянут в ob этого размера, поверх — HUD
hud_compose:
    push    rbx
    sub     rsp, 0x60
    mov     ebx, ecx
    mov     ecx, MAXO
    cmp     edx, ecx
    cmova   edx, ecx
    cmp     r8d, ecx
    cmova   r8d, ecx
    mov     ecx, edx
    mov     edx, r8d
    call    hud_resize
    lea     rcx, [hu_t0]
    call    [QueryPerformanceCounter]
    mov     ecx, ebx
    call    hud_scale
    lea     rcx, [hu_t1]
    call    [QueryPerformanceCounter]
    mov     ecx, ebx
    call    hud_exec
    lea     rcx, [hu_t2]
    call    [QueryPerformanceCounter]
    mov     rax, [hu_t1]                ; --bench: растяжка и HUD отдельно
    sub     rax, [hu_t0]
    mov     [hu_tscale], rax
    mov     rax, [hu_t2]
    sub     rax, [hu_t1]
    mov     [hu_thud], rax
    add     rsp, 0x60
    pop     rbx
    ret

; снимок с HUD: кадр fb_cur в view_w x view_h (--view, иначе 1920x1080) с HUD ->
; viewN.bmp (буква — как у shotN)
hud_shot:
    push    rbx
    push    rsi
    sub     rsp, 0x48
    mov     ecx, [fb_cur]
    mov     edx, [view_w]
    mov     r8d, [view_h]
    call    hud_compose
    mov     eax, [ob_w]                 ; заголовок BMP (снизу вверх)
    mov     [view_hdr + 18], eax
    mov     ecx, [ob_h]
    mov     [view_hdr + 22], ecx
    imul    eax, ecx
    shl     eax, 2
    mov     [view_hdr + 34], eax
    add     eax, 54
    mov     [view_hdr + 2], eax
    mov     ax, [s_shotname + 8]
    mov     [s_viewname + 8], ax
    lea     rcx, [s_viewname]
    mov     edx, 0x40000000             ; GENERIC_WRITE
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword [rsp + 32], 2         ; CREATE_ALWAYS
    mov     qword [rsp + 40], 0x80
    mov     qword [rsp + 48], 0
    call    [CreateFileW]
    mov     rbx, rax
    mov     rcx, rbx
    lea     rdx, [view_hdr]
    mov     r8d, 54
    lea     r9, [written]
    mov     qword [rsp + 32], 0
    call    [WriteFile]
    mov     esi, [ob_h]
    dec     esi
.row:
    mov     rdx, [ob_bits]
    mov     eax, [ob_w]
    imul    eax, esi
    shl     eax, 2
    add     rdx, rax
    mov     rcx, rbx
    mov     r8d, [ob_w]
    shl     r8d, 2
    lea     r9, [written]
    mov     qword [rsp + 32], 0
    call    [WriteFile]
    dec     esi
    jns     .row
    mov     rcx, rbx
    call    [CloseHandle]
    add     rsp, 0x48
    pop     rsi
    pop     rbx
    ret

; ================================================================ раскладка (главный поток)
; конец render: список HUD этого кадра — по состоянию игры
hud_build:
    push    rbx
    sub     rsp, 32
    mov     eax, [fb_cur]
    lea     rdx, [hq_n]
    mov     dword [rdx + rax*4], 0
    mov     dword [hq_fade], 256
    mov     dword [hq_trk], 0
    mov     dword [hs_rw], 0
    mov     dword [hs_sf], 0
    movss   xmm0, [hud_t]               ; часы HUD — для мигания
    addss   xmm0, [dt]
    movss   [hud_t], xmm0
    xorps   xmm0, xmm0                  ; сколько идёт конец ночи (смерть, рассвет)
    cmp     dword [gstate], 0
    je      .ov
    movss   xmm0, [ov_t]
    addss   xmm0, [dt]
.ov:
    movss   [ov_t], xmm0
    call    hd_back
    call    hd_clock
    call    hd_wind
    call    hd_score
    call    hd_vitals
    call    hd_loot
    cmp     dword [gstate], 1
    je      .dead
    cmp     dword [gstate], 2
    je      .won
    ; заставка первые 7 с: проявляется за 0.8 с, гаснет за последние 1.2
    movss   xmm0, [elapsed]
    comiss  xmm0, [hc_7]
    jae     .out
    movss   xmm1, xmm0
    mulss   xmm1, [hc_1_0_8]
    minss   xmm1, [hc_1]
    movss   xmm2, [hc_7]
    subss   xmm2, xmm0
    mulss   xmm2, [hc_1_1_2]
    minss   xmm2, [hc_1]
    mulss   xmm1, xmm2
    mulss   xmm1, [hc_256]
    cvttss2si eax, xmm1
    mov     [hq_fade], eax
    call    hd_intro
    jmp     .out
.dead:
    lea     rcx, [ts_dead]
    mov     edx, 0xFFD03030
    lea     r8, [ts_lasted]
    call    hd_end
    jmp     .out
.won:
    lea     rcx, [ts_dawn]
    mov     edx, 0xFFF8D080
    lea     r8, [ts_survived]
    call    hd_end
.out:
    mov     dword [hq_fade], 256
    add     rsp, 32
    pop     rbx
    ret

; мягкие подложки по углам — чтобы надписи читались и на свету
hd_back:
    sub     rsp, 40
    HSP     40.0, 26.0, 44.0, 18.0, 0.0, 0.0, 28.0
    SHP     SH_SOFT, 0x8C04060A
    HSP     60.0, 324.0, 60.0, 24.0, 0.0, 0.0, 28.0
    SHP     SH_SOFT, 0x8C04060A
    HSP     600.0, 20.0, 40.0, 14.0, 0.0, 0.0, 28.0
    SHP     SH_SOFT, 0x7304060A
    add     rsp, 40
    ret

; часы, месяц, сколько до рассвета; полоса ночи 17:20 → 08:00 с луной
hd_clock:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 48
    lea     rdi, [tbuf]
    call    wput_clock
    TXT     FT_BIG, 0, 12.0, 7.0, tbuf, C_WHITE
    addss   xmm0, [hc_17]               ; справа от часов, 5 единиц отступа
    movss   [rsp + 32], xmm0
    movss   [hq_tx], xmm0
    mov     dword [hq_ty], __float32__(10.0)
    TXTP    FT_TXT, 0, ts_month, C_DIM
    lea     rdi, [tbuf]
    lea     rsi, [ts_todawn]
    call    wput_str
    movss   xmm0, [hc_1920]
    subss   xmm0, [gtime]
    maxss   xmm0, [hc_0]
    cvttss2si eax, xmm0
    xor     edx, edx
    mov     ecx, 60
    div     ecx
    mov     ebx, edx
    call    wput_num
    mov     word [rdi], ':'
    mov     eax, ebx
    xor     edx, edx
    mov     ecx, 10
    div     ecx
    add     eax, '0'
    add     edx, '0'
    mov     [rdi + 2], ax
    mov     [rdi + 4], dx
    mov     word [rdi + 6], 0
    movss   xmm0, [rsp + 32]
    movss   [hq_tx], xmm0
    mov     dword [hq_ty], __float32__(18.5)
    TXTP    FT_TXT, 0, tbuf, C_DIM
    ; полоса ночи
    HSP     13.0, 31.0, 97.0, 31.0, 0.6, 0.0, 0.0
    SHP     SH_CAP, 0x40C8D2E1
    movss   xmm0, [gtime]
    subss   xmm0, [hc_1040]
    mulss   xmm0, [hc_1_880]
    maxss   xmm0, [hc_0]
    minss   xmm0, [hc_1]
    mulss   xmm0, [hc_84]
    addss   xmm0, [hc_13]
    movss   [rsp + 36], xmm0            ; где луна
    movss   [hs_bx], xmm0
    SHP     SH_CAP, 0xD9C8D7EB
    movss   xmm0, [rsp + 36]
    movss   [hs_ax], xmm0
    mov     dword [hs_r], __float32__(2.4)
    mov     dword [hs_sf], __float32__(1.5)
    SHP     SH_CIRC, 0x59C8DCFF
    mov     dword [hs_r], __float32__(1.6)
    SHP     SH_CIRC, 0xFFECF2FA
    movss   xmm0, [rsp + 36]
    addss   xmm0, [hc_0_8]
    movss   [hs_ax], xmm0
    mov     dword [hs_ay], __float32__(30.5)
    mov     dword [hs_r], __float32__(1.3)
    SHP     SH_CIRC, 0xFF1E2430         ; серп
    HSP     97.0, 31.0, 0.0, 0.0, 1.3, 0.0, 0.0
    SHP     SH_CIRC, 0xE6F0BE6E         ; рассвет
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; компас ветра: кольцо, стрелка — куда дует у игрока (длина по силе), строка справа
hd_wind:
    sub     rsp, 40
    HSP     18.0, 43.0, 0.0, 0.0, 5.2, 0.35, 0.0
    SHP     SH_CIRC, 0xCCA0AEBE
    ; экранное направление ветра у игрока: (2(vx − vy), vx + vy); в штиль — общее
    movss   xmm0, [pl_wvx]
    subss   xmm0, [pl_wvy]
    addss   xmm0, xmm0
    movss   xmm1, [pl_wvx]
    addss   xmm1, [pl_wvy]
    movaps  xmm2, xmm0
    mulss   xmm2, xmm0
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    addss   xmm2, xmm3
    sqrtss  xmm2, xmm2
    comiss  xmm2, [f_0_01]
    ja      .ok
    movss   xmm0, [wind_dx]
    subss   xmm0, [wind_dy]
    addss   xmm0, xmm0
    movss   xmm1, [wind_dx]
    addss   xmm1, [wind_dy]
    movaps  xmm2, xmm0
    mulss   xmm2, xmm0
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    addss   xmm2, xmm3
    sqrtss  xmm2, xmm2
    maxss   xmm2, [hc_eps]
.ok:
    movss   xmm3, [hc_1]
    divss   xmm3, xmm2
    mulss   xmm0, xmm3
    mulss   xmm1, xmm3
    movss   [hw_ux], xmm0
    movss   [hw_uy], xmm1
    ; полудлина 2.4..3.6; в затишье — бледнее
    movss   xmm2, [pl_wsm]
    minss   xmm2, [hc_1]
    mulss   xmm2, [hc_1_2]
    addss   xmm2, [hc_2_4]
    mov     dword [hw_c], C_WHITE
    movss   xmm3, [pl_wsm]
    mulss   xmm3, [f_12]
    comiss  xmm3, [hc_half]
    jae     .col
    mov     dword [hw_c], 0x80E8EEF4
.col:
    ; древко
    mulss   xmm0, xmm2
    mulss   xmm1, xmm2
    movss   xmm2, [hc_18]
    subss   xmm2, xmm0
    movss   [hs_ax], xmm2
    movss   xmm2, [hc_43]
    subss   xmm2, xmm1
    movss   [hs_ay], xmm2
    movss   xmm2, [hc_18]
    addss   xmm2, xmm0
    movss   [hs_bx], xmm2
    movss   [hw_tx], xmm2
    movss   xmm2, [hc_43]
    addss   xmm2, xmm1
    movss   [hs_by], xmm2
    movss   [hw_ty], xmm2
    mov     dword [hs_r], __float32__(0.55)
    SHP     SH_CAP, [hw_c]
    ; наконечник: от острия назад 2.4 и вбок ±1.8
    movss   xmm0, [hw_tx]
    movss   [hs_ax], xmm0
    movss   xmm0, [hw_ty]
    movss   [hs_ay], xmm0
    movss   xmm0, [hw_ux]
    mulss   xmm0, [hc_2_4]
    movss   xmm1, [hw_tx]
    subss   xmm1, xmm0                  ; назад по x
    movss   xmm0, [hw_uy]
    mulss   xmm0, [hc_2_4]
    movss   xmm2, [hw_ty]
    subss   xmm2, xmm0                  ; назад по y
    movss   xmm3, [hw_uy]
    mulss   xmm3, [hc_1_8]              ; вбок: (−uy, ux)·1.8
    movss   xmm4, [hw_ux]
    mulss   xmm4, [hc_1_8]
    movss   xmm0, xmm1
    subss   xmm0, xmm3
    movss   [hs_bx], xmm0
    movss   xmm0, xmm2
    addss   xmm0, xmm4
    movss   [hs_by], xmm0
    movss   [hw_bx], xmm1
    movss   [hw_by], xmm2
    movss   [hw_sx], xmm3
    movss   [hw_sy], xmm4
    SHP     SH_CAP, [hw_c]
    movss   xmm0, [hw_bx]
    addss   xmm0, [hw_sx]
    movss   [hs_bx], xmm0
    movss   xmm0, [hw_by]
    subss   xmm0, [hw_sy]
    movss   [hs_by], xmm0
    SHP     SH_CAP, [hw_c]
    call    wind_text
    TXT     FT_TXT, 0, 27.0, 38.5, tbuf, C_WHITE
    add     rsp, 40
    ret

; справа вверху: упокоено (новый — вспыхивает золотым и за 0.8 с остывает), время кадра
hd_score:
    push    rsi
    push    rdi
    sub     rsp, 40
    mov     eax, [kills]
    cmp     eax, [hs_lastk]
    mov     [hs_lastk], eax
    jbe     .kt
    mov     dword [hs_kt], __float32__(1.0)
.kt:
    movss   xmm1, [dt]
    mulss   xmm1, [hc_1_0_8]
    movss   xmm0, [hs_kt]
    subss   xmm0, xmm1
    maxss   xmm0, [hc_0]
    movss   [hs_kt], xmm0
    mulss   xmm0, [f_256]
    cvttss2si r8d, xmm0
    mov     ecx, C_WHITE
    mov     edx, 0xFFF8D080
    call    hd_lerpc
    mov     esi, eax
    lea     rdi, [tbuf]
    mov     eax, [kills]
    call    wput_num
    TXT     FT_BIG, 2, 628.0, 5.0, tbuf, esi
    TXT     FT_TXT, 2, 628.0, 24.0, ts_killed, C_DIM
    ; «4.2 мс» — update + render, скользящее среднее
    lea     rdi, [tbuf]
    movss   xmm0, [frame_ms]
    mulss   xmm0, [hc_10]
    cvtss2si eax, xmm0
    xor     edx, edx
    mov     ecx, 10
    div     ecx
    mov     esi, edx
    call    wput_num
    mov     word [rdi], '.'
    add     rdi, 2
    mov     eax, esi
    call    wput_num
    lea     rsi, [ts_ms]
    call    wput_str
    TXT     FT_TINY | FT_NOSH, 2, 628.0, 33.0, tbuf, C_FAINT
    add     rsp, 40
    pop     rdi
    pop     rsi
    ret

; ecx, edx = цвета ARGB, r8d = доля второго 0..256 -> eax; портит r9, r10, r11
hd_lerpc:
    xor     eax, eax
    mov     r11d, 4
.c:
    rol     eax, 8
    rol     ecx, 8
    rol     edx, 8
    movzx   r9d, cl
    movzx   r10d, dl
    sub     r10d, r9d
    imul    r10d, r8d
    sar     r10d, 8
    add     r9d, r10d
    mov     al, r9b
    dec     r11d
    jnz     .c
    ret

; слева внизу: значок, полоса, подпись — здоровье, тепло, силы, фонарь
hd_vitals:
    push    rbx
    sub     rsp, 32
    ; мигание тревог: A = 0.55 + 0.45·|2·frac(1.6 t) − 1|
    movss   xmm0, [hud_t]
    mulss   xmm0, [hc_1_6]
    cvttss2si eax, xmm0
    cvtsi2ss xmm1, eax
    subss   xmm0, xmm1
    addss   xmm0, xmm0
    subss   xmm0, [hc_1]
    andps   xmm0, [hc_absm]
    mulss   xmm0, [hc_0_45]
    addss   xmm0, [hc_0_55]
    mulss   xmm0, [hc_255]
    cvttss2si eax, xmm0
    shl     eax, 24
    mov     [hv_pulse], eax
    ; --- здоровье: сердце из двух отрезков
    HSP     17.0, 307.2, 15.0, 304.4, 1.9, 0.0, 0.0
    SHP     SH_CAP, 0xFFD6403A
    HSP     17.0, 307.2, 19.0, 304.4, 1.9, 0.0, 0.0
    SHP     SH_CAP, 0xFFD6403A
    mov     eax, 0xFFD6403A
    movss   xmm0, [pl_hp]
    comiss  xmm0, [hc_30]
    jae     .hp
    and     eax, 0x00FFFFFF
    or      eax, [hv_pulse]
.hp:
    mov     [hv_c], eax
    mulss   xmm0, [f_0_01]
    movss   [hv_v], xmm0
    mov     dword [hv_y], __float32__(305.0)
    lea     r8, [ts_hp]
    mov     r9d, C_DIM
    call    hd_bar
    ; --- тепло: градусник; остывая, полоса из рыжей голубеет
    movss   xmm0, [pl_warm]
    mulss   xmm0, [hc_1_60]
    maxss   xmm0, [hc_0]
    minss   xmm0, [hc_1]
    mulss   xmm0, [f_256]
    cvttss2si ecx, xmm0                 ; t·256
    mov     eax, 90                     ; R 150 -> 240
    imul    eax, ecx
    sar     eax, 8
    add     eax, 150
    shl     eax, 16
    mov     edx, -40                    ; G 190 -> 150
    imul    edx, ecx
    sar     edx, 8
    add     edx, 190
    shl     edx, 8
    or      eax, edx
    mov     edx, -170                   ; B 240 -> 70
    imul    edx, ecx
    sar     edx, 8
    add     edx, 240
    or      eax, edx
    or      eax, 0xFF000000
    mov     ebx, eax
    HSP     17.0, 313.0, 17.0, 317.0, 1.1, 0.0, 0.0
    SHP     SH_CAP, C_DIM
    HSP     17.0, 318.0, 0.0, 0.0, 2.0, 0.0, 0.0
    SHP     SH_CIRC, ebx
    HSP     17.0, 315.5, 17.0, 317.0, 0.5, 0.0, 0.0
    SHP     SH_CAP, ebx
    mov     [hv_c], ebx
    movss   xmm0, [pl_warm]
    mulss   xmm0, [f_0_01]
    movss   [hv_v], xmm0
    mov     dword [hv_y], __float32__(316.0)
    lea     r8, [ts_warm]
    mov     r9d, C_DIM
    movss   xmm0, [pl_warm]
    comiss  xmm0, [hc_20]
    jae     .wm
    lea     r8, [ts_freeze]
    mov     r9d, 0x00AACDF5
    or      r9d, [hv_pulse]
.wm:
    call    hd_bar
    ; --- силы: молния
    HSP     18.6, 323.6, 15.8, 327.4, 0.55, 0.0, 0.0
    SHP     SH_CAP, 0xFFDEB850
    HSP     15.8, 327.4, 18.2, 327.4, 0.55, 0.0, 0.0
    SHP     SH_CAP, 0xFFDEB850
    HSP     18.2, 327.4, 15.6, 330.8, 0.55, 0.0, 0.0
    SHP     SH_CAP, 0xFFDEB850
    mov     dword [hv_c], 0xFFDEB850
    movss   xmm0, [pl_sta]
    mulss   xmm0, [f_0_01]
    movss   [hv_v], xmm0
    mov     dword [hv_y], __float32__(327.0)
    lea     r8, [ts_sta]
    mov     r9d, C_DIM
    call    hd_bar
    ; --- фонарь: батарейка с зарядом; включён — ярче, садится — мигает
    mov     ebx, 0xFF6080A0
    cmp     dword [pl_flash], 0
    je      .fo
    mov     ebx, 0xFF78BEF5
.fo:
    movss   xmm0, [pl_bat]
    comiss  xmm0, [hc_15]
    jae     .bl
    comiss  xmm0, [hc_0]
    jbe     .bl
    and     ebx, 0x00FFFFFF
    or      ebx, [hv_pulse]
.bl:
    HSP     16.6, 338.0, 3.0, 1.9, 0.6, 0.3, 0.0
    SHP     SH_BOX, C_DIM
    HSP     20.1, 338.0, 0.5, 0.9, 0.3, 0.0, 0.0
    SHP     SH_BOX, C_DIM
    movss   xmm0, [pl_bat]
    mulss   xmm0, [f_0_01]
    maxss   xmm0, [hc_0]
    minss   xmm0, [hc_1]
    movss   [hv_v], xmm0
    comiss  xmm0, [f_0_01]
    jb      .be
    movss   xmm1, [hc_1]                ; заряд: слева направо, полуширина 2v + 0.2
    subss   xmm1, xmm0
    addss   xmm1, xmm1
    movss   xmm2, [hc_16_6]
    subss   xmm2, xmm1
    movss   [hs_ax], xmm2
    mov     dword [hs_ay], __float32__(338.0)
    addss   xmm0, xmm0
    addss   xmm0, [hc_0_2]
    movss   [hs_bx], xmm0
    mov     dword [hs_by], __float32__(1.1)
    mov     dword [hs_r], __float32__(0.2)
    SHP     SH_BOX, ebx
.be:
    mov     [hv_c], ebx
    mov     dword [hv_y], __float32__(338.0)
    lea     r8, [ts_bat]
    mov     r9d, C_DIM
    movss   xmm0, [pl_bat]
    comiss  xmm0, [hc_0]
    ja      .bt
    lea     r8, [ts_batdead]
    mov     r9d, 0xFFE65046
.bt:
    call    hd_bar
    add     rsp, 32
    pop     rbx
    ret

; полоса: hv_y — середина, hv_v — доля 0..1, hv_c — цвет; r8 — подпись, r9d — её цвет
hd_bar:
    push    rbx
    push    rsi
    sub     rsp, 40
    mov     rsi, r8
    mov     ebx, r9d
    movss   xmm0, [hv_v]
    maxss   xmm0, [hc_0]
    minss   xmm0, [hc_1]
    movss   [hv_v], xmm0
    ; тёмная подложка и светлая дорожка во всю длину
    mov     dword [hs_ax], __float32__(25.0)
    mov     dword [hs_bx], __float32__(105.0)
    mov     eax, [hv_y]
    mov     [hs_ay], eax
    mov     [hs_by], eax
    mov     dword [hs_r], __float32__(1.9)
    SHP     SH_CAP, 0x6E080A10
    mov     dword [hs_r], __float32__(1.2)
    SHP     SH_CAP, 0x26C8D2E1
    movss   xmm0, [hv_v]
    comiss  xmm0, [f_0_01]
    jb      .lab
    mulss   xmm0, [hc_80]
    addss   xmm0, [hc_25]
    movss   [hs_bx], xmm0
    SHP     SH_CAP, [hv_c]
    ; блик по верху
    movss   xmm0, [hs_bx]
    subss   xmm0, [hc_0_6]
    movss   [hs_bx], xmm0
    mov     dword [hs_ax], __float32__(25.6)
    movss   xmm0, [hv_y]
    subss   xmm0, [hc_0_55]
    movss   [hs_ay], xmm0
    movss   [hs_by], xmm0
    mov     dword [hs_r], __float32__(0.35)
    SHP     SH_CAP, 0x40FFFFFF
.lab:
    mov     dword [hq_tx], __float32__(109.0)
    movss   xmm0, [hv_y]
    subss   xmm0, [hc_4_2]
    movss   [hq_ty], xmm0
    mov     ecx, FT_TXT
    xor     edx, edx
    mov     r8, rsi
    mov     r9d, ebx
    call    hq_text
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret

; значок запаса: фигура со сдвигом от hl_cx — dx, ay, bx, by, r, вид, цвет
%macro HLI 7
    movss   xmm0, [hl_cx]
    mov     dword [hs_ax], __float32__(%1)
    addss   xmm0, [hs_ax]
    movss   [hs_ax], xmm0
    mov     dword [hs_ay], __float32__(%2)
    mov     dword [hs_bx], __float32__(%3)
    mov     dword [hs_by], __float32__(%4)
    mov     dword [hs_r], __float32__(%5)
    SHP     %6, %7
%endmacro
; то же отрезком: оба конца — сдвиги от hl_cx (dx1, y1, dx2, y2, r, цвет)
%macro HLC 6
    movss   xmm0, [hl_cx]
    mov     dword [hs_ax], __float32__(%1)
    addss   xmm0, [hs_ax]
    movss   [hs_ax], xmm0
    movss   xmm0, [hl_cx]
    mov     dword [hs_bx], __float32__(%3)
    addss   xmm0, [hs_bx]
    movss   [hs_bx], xmm0
    mov     dword [hs_ay], __float32__(%2)
    mov     dword [hs_by], __float32__(%4)
    mov     dword [hs_r], __float32__(%5)
    SHP     SH_CAP, %6
%endmacro

; лут: запас справа внизу (клавиша 1..4, значок, «×N»), по центру — сообщение о
; находке и подсказка «E взять …» / «E обыскать …»
hd_loot:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 48
    cmp     dword [gstate], 0
    jne     .out
    ; ----- запас
    xor     ecx, ecx
    xor     eax, eax
    lea     rdx, [inv]
.cnt:
    cmp     dword [rdx + rax*4], 0
    je      .c1
    inc     ecx
.c1:
    inc     eax
    cmp     eax, NLT
    jb      .cnt
    test    ecx, ecx
    jz      .msg
    ; подложка под ряд: слот — 34 единицы, ряд прижат к 628
    imul    ecx, ecx, 34
    cvtsi2ss xmm0, ecx
    mulss   xmm0, [hc_half]
    movss   xmm1, [hc_628]
    subss   xmm1, xmm0
    movss   [hs_ax], xmm1
    addss   xmm0, [hc_6]
    movss   [hs_bx], xmm0
    mov     dword [hs_ay], __float32__(338.0)
    mov     dword [hs_by], __float32__(8.0)
    mov     dword [hs_r], 0
    mov     dword [hs_sf], __float32__(22.0)
    SHP     SH_SOFT, 0x8C04060A
    mov     dword [hl_x], __float32__(594.0)
    mov     ebx, NLT
.slot:
    dec     ebx
    js      .msg
    lea     rdx, [inv]
    mov     esi, [rdx + rbx*4]
    test    esi, esi
    jz      .slot
    ; клавиша
    movss   xmm0, [hl_x]
    addss   xmm0, [hc_4]
    movss   [hs_ax], xmm0
    movss   [hq_tx], xmm0
    mov     dword [hs_ay], __float32__(338.0)
    mov     dword [hs_bx], __float32__(3.4)
    mov     dword [hs_by], __float32__(5.0)
    mov     dword [hs_r], __float32__(1.4)
    SHP     SH_BOX, 0xCC0E121A
    mov     dword [hs_rw], __float32__(0.3)
    SHP     SH_BOX, 0xE696A6BA
    mov     dword [hq_ty], __float32__(333.8)
    lea     rdx, [hl_keys]
    mov     r8, [rdx + rbx*8]
    mov     ecx, FT_KEY | FT_NOSH
    mov     edx, 1
    mov     r9d, C_WHITE
    call    hq_text
    ; значок
    movss   xmm0, [hl_x]
    addss   xmm0, [hc_13]
    movss   [hl_cx], xmm0
    cmp     ebx, 1
    jb      .ibat
    je      .iband
    cmp     ebx, 2
    je      .itea
    ; тушёнка: банка с полосой и крышкой
    HLI     0.0, 338.7, 3.2, 2.8, 0.7, SH_BOX, 0xFFC89838
    HLI     0.0, 339.1, 3.2, 0.8, 0.0, SH_BOX, 0xFF8A2A20
    HLI     0.0, 335.9, 3.2, 0.55, 0.3, SH_BOX, 0xFFD0D4D8
    jmp     .num
.ibat:
    ; плоская батарейка: половины красная и синяя, латунные язычки
    HLI     -1.7, 339.0, 1.7, 2.6, 0.4, SH_BOX, 0xFFB83028
    HLI     1.7, 339.0, 1.7, 2.6, 0.4, SH_BOX, 0xFF2A4A9A
    HLI     -1.5, 335.6, 0.5, 0.9, 0.0, SH_BOX, 0xFFD8B050
    HLI     1.5, 335.6, 0.5, 0.9, 0.0, SH_BOX, 0xFFD8B050
    jmp     .num
.iband:
    ; бинт: рулон с дыркой
    HLI     0.0, 338.2, 0.0, 0.0, 3.4, SH_CIRC, 0xFFE8E6DE
    HLI     0.0, 338.2, 0.0, 0.0, 1.2, SH_CIRC, 0xFF8E8A80
    jmp     .num
.itea:
    ; термос: красная колба, серая крышка
    HLC     0.0, 336.0, 0.0, 340.8, 2.0, 0xFFB83A2E
    HLI     0.0, 333.5, 1.6, 1.0, 0.4, SH_BOX, 0xFFC8D0D8
.num:
    lea     rdi, [tbuf]
    mov     word [rdi], 0xD7
    add     rdi, 2
    mov     eax, esi
    call    wput_num
    movss   xmm0, [hl_x]
    addss   xmm0, [hc_18]
    movss   [hq_tx], xmm0
    mov     dword [hq_ty], __float32__(333.8)
    TXTP    FT_TXT, 0, tbuf, C_WHITE
    movss   xmm0, [hl_x]
    subss   xmm0, [hc_34]
    movss   [hl_x], xmm0
    jmp     .slot
.msg:
    ; ----- сообщение: гаснет за последние полсекунды
    movss   xmm0, [lm_t]
    comiss  xmm0, [hc_0]
    jbe     .prompt
    addss   xmm0, xmm0
    minss   xmm0, [hc_1]
    mulss   xmm0, [hc_256]
    cvttss2si eax, xmm0
    mov     [hq_fade], eax
    HSP     320.0, 297.5, 64.0, 5.5, 0.0, 0.0, 22.0
    SHP     SH_SOFT, 0x8C04060A
    TXT     FT_TXT, 1, 320.0, 292.5, lm_buf, C_WHITE
    mov     dword [hq_fade], 256
.prompt:
    ; ----- подсказка
    cmp     dword [lp_kind], 0
    je      .out
    lea     rdi, [tbuf]
    cmp     dword [lp_kind], 1
    jne     .pc
    lea     rsi, [ts_take]
    call    wput_str
    mov     eax, [lp_type]
    lea     rdx, [lo_names]
    mov     rsi, [rdx + rax*8 - 8]
    call    wput_str
    jmp     .pk
.pc:
    mov     eax, [lp_type]
    lea     rdx, [lo_cask]
    mov     rsi, [rdx + rax*8 - 8]
    call    wput_str
.pk:
    HSP     320.0, 311.0, 56.0, 6.0, 0.0, 0.0, 22.0
    SHP     SH_SOFT, 0x8C04060A
    mov     dword [hk_lc], C_WHITE
    mov     dword [hk_y], __float32__(305.5)
    lea     rcx, [hk_prompt]
    call    hd_keys
    mov     dword [hk_lc], C_DIM
.out:
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; тёмная полоса поперёк экрана и росчерк с ромбом (xmm0 — его y)
hd_band:
    sub     rsp, 56
    movss   [rsp + 32], xmm0
    HSP     320.0, 0.0, 360.0, 58.0, 0.0, 0.0, 40.0
    movss   xmm0, [rsp + 32]
    addss   xmm0, [hc_10]               ; середина полосы — на 10 ниже росчерка
    movss   [hs_ay], xmm0
    SHP     SH_SOFT, 0xB304060A
    HSP     262.0, 0.0, 312.0, 0.0, 0.3, 0.0, 0.0
    movss   xmm0, [rsp + 32]
    movss   [hs_ay], xmm0
    movss   [hs_by], xmm0
    SHP     SH_CAP, 0xCCA0AEBE
    mov     dword [hs_ax], __float32__(328.0)
    mov     dword [hs_bx], __float32__(378.0)
    SHP     SH_CAP, 0xCCA0AEBE
    mov     dword [hs_ax], __float32__(320.0)
    mov     dword [hs_r], __float32__(2.6)
    SHP     SH_DIAM, C_DIM
    add     rsp, 56
    ret

; заставка: название, подзаголовок, клавиши
hd_intro:
    sub     rsp, 40
    movss   xmm0, [hc_150]
    call    hd_band
    mov     dword [hq_trk], __float32__(7.0)
    TXT     FT_TTL, 1, 320.0, 92.0, ts_title, C_WHITE
    TXT     FT_SUB, 1, 320.0, 157.0, ts_sub, 0xFFD2DCE8
    mov     dword [hk_y], __float32__(186.0)
    lea     rcx, [hk_intro]
    call    hd_keys
    mov     dword [hk_y], __float32__(200.0)
    lea     rcx, [hk_intro2]
    call    hd_keys
    add     rsp, 40
    ret

; конец ночи: rcx — заголовок, edx — его цвет, r8 — начало итога
hd_end:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 48
    mov     rsi, rcx
    mov     ebx, edx
    mov     [rsp + 32], r8
    ; проявляется за 1.2 с
    movss   xmm0, [ov_t]
    mulss   xmm0, [hc_1_1_2]
    minss   xmm0, [hc_1]
    mulss   xmm0, [hc_256]
    cvttss2si eax, xmm0
    mov     [hq_fade], eax
    movss   xmm0, [hc_146]
    call    hd_band
    mov     dword [hq_trk], __float32__(5.0)
    mov     dword [hq_tx], __float32__(320.0)
    mov     dword [hq_ty], __float32__(90.0)
    mov     ecx, FT_TTL
    mov     edx, 1
    mov     r8, rsi
    mov     r9d, ebx
    call    hq_text
    ; «Продержался до 23:11 · упокоено: 3»
    lea     rdi, [tbuf]
    mov     rsi, [rsp + 32]
    call    wput_str
    call    wput_clock
    lea     rsi, [ts_sepk]
    call    wput_str
    mov     eax, [kills]
    call    wput_num
    TXT     FT_MID, 1, 320.0, 153.0, tbuf, 0xFFD2DCE8
    mov     dword [hk_y], __float32__(182.0)
    lea     rcx, [hk_end]
    call    hd_keys
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ряд клавиш по центру: rcx — пары (клавиша, подпись; подпись 0 — следующая клавиша
; к той же подписи), в конце 0; hk_y — верх ряда
hd_keys:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 48
    mov     rsi, rcx
    xorps   xmm0, xmm0
    movss   [rsp + 32], xmm0
    mov     rbx, rsi
.w:
    mov     rdx, [rbx]
    test    rdx, rdx
    jz      .wend
    mov     ecx, FT_KEY
    call    hq_measure
    addss   xmm0, [hc_5]                ; клавиша: текст и поля
    addss   xmm0, [rsp + 32]
    movss   [rsp + 32], xmm0
    mov     rdx, [rbx + 8]
    test    rdx, rdx
    jnz     .wl
    addss   xmm0, [hc_2]
    movss   [rsp + 32], xmm0
    jmp     .wn
.wl:
    mov     ecx, FT_TXT
    call    hq_measure
    addss   xmm0, [hc_13]               ; 3 до подписи, 10 после
    addss   xmm0, [rsp + 32]
    movss   [rsp + 32], xmm0
.wn:
    add     rbx, 16
    jmp     .w
.wend:
    movss   xmm0, [rsp + 32]
    subss   xmm0, [hc_10]
    mulss   xmm0, [hc_half]
    movss   xmm1, [hc_320]
    subss   xmm1, xmm0
    movss   [rsp + 36], xmm1            ; x
    mov     rbx, rsi
.k:
    mov     rdx, [rbx]
    test    rdx, rdx
    jz      .done
    mov     ecx, FT_KEY
    call    hq_measure
    addss   xmm0, [hc_5]
    movss   [rsp + 40], xmm0            ; ширина клавиши
    mulss   xmm0, [hc_half]
    movss   [hs_bx], xmm0
    addss   xmm0, [rsp + 36]
    movss   [hs_ax], xmm0
    movss   [hq_tx], xmm0
    movss   xmm0, [hk_y]
    addss   xmm0, [hc_5]
    movss   [hs_ay], xmm0
    mov     dword [hs_by], __float32__(5.5)
    mov     dword [hs_r], __float32__(1.6)
    SHP     SH_BOX, 0xCC0E121A
    mov     dword [hs_rw], __float32__(0.3)
    SHP     SH_BOX, 0xE696A6BA
    movss   xmm0, [hk_y]
    addss   xmm0, [hc_0_8]
    movss   [hq_ty], xmm0
    mov     ecx, FT_KEY | FT_NOSH
    mov     edx, 1
    mov     r8, [rbx]
    mov     r9d, C_WHITE
    call    hq_text
    movss   xmm0, [rsp + 36]
    addss   xmm0, [rsp + 40]
    cmp     qword [rbx + 8], 0
    jne     .lab
    addss   xmm0, [hc_2]
    movss   [rsp + 36], xmm0
    jmp     .kn
.lab:
    addss   xmm0, [hc_3]
    movss   [hq_tx], xmm0
    movss   [rsp + 36], xmm0
    mov     ecx, FT_TXT
    xor     edx, edx
    mov     r8, [rbx + 8]
    mov     r9d, [hk_lc]
    call    hq_text
    addss   xmm0, [hc_10]
    addss   xmm0, [rsp + 36]
    movss   [rsp + 36], xmm0
.kn:
    add     rbx, 16
    jmp     .k
.done:
    add     rsp, 48
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ================================================================ данные HUD
section .data
align 16, db 0
hc_absm:    times 4 dd 0x7FFFFFFF
hc_one4:    times 4 dd 1.0
hc_half4:   times 4 dd 0.5
hc_four4:   times 4 dd 4.0
hc_rs2:     times 4 dd 0.70710678
hc_three4:  times 4 dd 3.0
hc_lane:    dd 0.5, 1.5, 2.5, 3.5
hc_0:       dd 0.0
hc_1:       dd 1.0
hc_half:    dd 0.5
hc_eps:     dd 1.0e-6
hc_128_255: dd 0.50196078
hc_1_360:   dd 0.0027777778
hc_1_256:   dd 0.00390625
hc_8:       dd 8.0
hc_0_2:     dd 0.2
hc_0_45:    dd 0.45
hc_0_55:    dd 0.55
hc_0_6:     dd 0.6
hc_0_8:     dd 0.8
hc_1_0_8:   dd 1.25                     ; 1 / 0.8
hc_1_1_2:   dd 0.8333333                ; 1 / 1.2
hc_1_2:     dd 1.2
hc_1_6:     dd 1.6
hc_1_8:     dd 1.8
hc_2:       dd 2.0
hc_2_4:     dd 2.4
hc_3:       dd 3.0
hc_4_2:     dd 4.2
hc_5:       dd 5.0
hc_7:       dd 7.0
hc_10:      dd 10.0
hc_13:      dd 13.0
hc_15:      dd 15.0
hc_17:      dd 17.0
hc_16_6:    dd 16.6
hc_18:      dd 18.0
hc_20:      dd 20.0
hc_25:      dd 25.0
hc_30:      dd 30.0
hc_43:      dd 43.0
hc_80:      dd 80.0
hc_84:      dd 84.0
hc_146:     dd 146.0
hc_150:     dd 150.0
hc_255:     dd 255.0
hc_256:     dd 256.0
hc_320:     dd 320.0
hc_1040:    dd 1040.0                   ; 17:20
hc_1920:    dd 1920.0                   ; 08:00 следующего дня
hc_4:       dd 4.0
hc_6:       dd 6.0
hc_34:      dd 34.0
hc_628:     dd 628.0
hl_cx:      dd 0.0                      ; середина значка запаса
hc_1_60:    dd 0.016666668
hc_1_880:   dd 0.0011363636             ; 1 / (08:00 − 17:20)

; шрифты HUD: кегль в единицах кадра, жирность, курсив, имя
align 8, db 0
fn_def:     dd 18.0, 400, 0, 0
            dq fnn_bsb
            dd 7.5, 400, 0, 0
            dq fnn_bsl
            dd 7.0, 400, 0, 0
            dq fnn_bsb
            dd 6.0, 400, 0, 0
            dq fnn_bl
            dd 44.0, 700, 0, 0
            dq fnn_geo
            dd 11.0, 400, 1, 0
            dq fnn_geo
            dd 10.0, 400, 0, 0
            dq fnn_bsl
fn_cur:     dd -1                       ; какой выбран в холст глифов
hq_fade:    dd 256                      ; непрозрачность записываемого (заставка, итог)
hq_trk:     dd 0.0                      ; разрядка следующей строки
hq_tx:      dd 0.0                      ; точка следующей строки
hq_ty:      dd 0.0
hs_p:                                   ; фигура: a, b, r, rw, sf
hs_ax:      dd 0.0
hs_ay:      dd 0.0
hs_bx:      dd 0.0
hs_by:      dd 0.0
hs_r:       dd 0.0
hs_rw:      dd 0.0
hs_sf:      dd 0.0
hud_t:      dd 0.0
ov_t:       dd 0.0
hv_y:       dd 0.0
hv_v:       dd 0.0
hv_c:       dd 0
hv_pulse:   dd 0
hw_ux:      dd 0.0
hw_uy:      dd 0.0
hw_tx:      dd 0.0
hw_ty:      dd 0.0
hw_bx:      dd 0.0
hw_by:      dd 0.0
hw_sx:      dd 0.0
hw_sy:      dd 0.0
hw_c:       dd 0
hk_y:       dd 0.0
hk_lc:      dd C_DIM                    ; цвет подписей у клавиш
hl_x:       dd 0.0                      ; запас: левый край слота
hs_lastk:   dd 0                        ; счёт прошлого кадра и вспышка нового упокоенного
hs_kt:      dd 0.0
gl_col:     dd 0
gc_pos:     dd 0
g_sho:      dd 1
g_rb:       dd 1
g_m:        dd 3
hu_u:       dd 1.0
hu_uf:      dd 1.0
ob_w:       dd 0
ob_h:       dd 0
align 8, db 0
hu_t0:      dq 0
hu_t1:      dq 0
hu_t2:      dq 0
hu_tscale:  dq 0                        ; последний кадр: растяжка, HUD (тики QPC)
hu_thud:    dq 0
ob_dc:      dq 0
ob_bmp:     dq 0
ob_bits:    dq 0
gr_dc:      dq 0
gr_bits:    dq 0
gr_font0:   dq 0
fn_h:       times NFT dq 0
ob_bmi:     dd 40, 0, 0
            dw 1, 32
            dd 0, 0, 0, 0, 0, 0
gr_bmi:     dd 40, GR_W, -GR_W
            dw 1, 32
            dd 0, 0, 0, 0, 0, 0
view_w:     dd 1920                     ; снимок с HUD (--view)
view_h:     dd 1080
view_hdr:   db 'BM'                     ; размеры — в hud_shot
            dd 0, 0, 54
            dd 40, 0, 0
            dw 1, 32
            dd 0, 0, 2835, 2835, 0, 0
hk_intro:   dq ts_kwasd, ts_go, ts_kshift, ts_run, ts_klmb, 0, ts_kspace, ts_shovel
            dq ts_kf, ts_flash, ts_km, ts_sound, ts_kf11, ts_screen, 0
hk_intro2:  dq ts_ke, ts_takei, ts_k14, ts_usei, 0
hk_end:     dq ts_kr, ts_again, ts_kesc, ts_quit, 0
hk_prompt:  dq ts_ke, tbuf, 0            ; «E взять …» — подпись собирается в tbuf
hl_keys:    dq ts_k1, ts_k2, ts_k3, ts_k4
align 2, db 0
gc_ext:     dw 0x401, 0x451, 0xB7, 0x2014, 0x2013, 0x2192, 0xB0, 0xD7, 0
fnn_bsb:    dw __utf16__('Bahnschrift SemiBold'), 0
fnn_bsl:    dw __utf16__('Bahnschrift SemiLight'), 0
fnn_bl:     dw __utf16__('Bahnschrift Light'), 0
fnn_geo:    dw __utf16__('Georgia'), 0
s_viewname: dw __utf16__('view?.bmp'), 0
ts_month:   dw __utf16__('январь'), 0
ts_todawn:  dw __utf16__('до рассвета '), 0
ts_killed:  dw __utf16__('упокоено'), 0
ts_ms:      dw __utf16__(' мс'), 0
ts_hp:      dw __utf16__('здоровье'), 0
ts_warm:    dw __utf16__('тепло'), 0
ts_freeze:  dw __utf16__('замерзаешь'), 0
ts_sta:     dw __utf16__('силы'), 0
ts_bat:     dw __utf16__('фонарь'), 0
ts_batdead: dw __utf16__('фонарь сел'), 0
ts_title:   dw __utf16__('СТУЖА'), 0
ts_sub:     dw __utf16__('Зима. Мёртвые бродят по селу. Доживи до рассвета.'), 0
ts_dead:    dw __utf16__('ТЫ ПОГИБ'), 0
ts_lasted:  dw __utf16__('Продержался до '), 0
ts_dawn:    dw __utf16__('РАССВЕТ'), 0
ts_survived: dw __utf16__('Дожил до утра: '), 0
ts_sepk:    dw __utf16__(' · упокоено: '), 0
ts_kwasd:   dw __utf16__('WASD'), 0
ts_go:      dw __utf16__('идти'), 0
ts_kshift:  dw __utf16__('Shift'), 0
ts_run:     dw __utf16__('бежать'), 0
ts_klmb:    dw __utf16__('ЛКМ'), 0
ts_kspace:  dw __utf16__('Пробел'), 0
ts_shovel:  dw __utf16__('лопата'), 0
ts_kf:      dw __utf16__('F'), 0
ts_flash:   dw __utf16__('фонарь'), 0
ts_km:      dw __utf16__('M'), 0
ts_sound:   dw __utf16__('звук'), 0
ts_kf11:    dw __utf16__('F11'), 0
ts_screen:  dw __utf16__('экран'), 0
ts_kr:      dw __utf16__('R'), 0
ts_again:   dw __utf16__('заново'), 0
ts_kesc:    dw __utf16__('Esc'), 0
ts_quit:    dw __utf16__('выход'), 0
ts_ke:      dw __utf16__('E'), 0
ts_takei:   dw __utf16__('взять, обыскать'), 0
ts_k14:     dw __utf16__('1–4'), 0
ts_usei:    dw __utf16__('вещь из запаса'), 0
ts_k1:      dw __utf16__('1'), 0
ts_k2:      dw __utf16__('2'), 0
ts_k3:      dw __utf16__('3'), 0
ts_k4:      dw __utf16__('4'), 0
ts_times:   dw 0xD7, 0

section .bss
alignb 16
sd_ax:      resd 4                      ; фигура в пикселях, по 4 места
sd_ay:      resd 4
sd_bx:      resd 4
sd_by:      resd 4
sd_r:       resd 4
sd_rw:      resd 4
sd_invs:    resd 4
sd_idd:     resd 4
sd_ak:      resd 4
sd_x1:      resd 4
sd_col:     resw 8
hu_xmap:    resd MAXO                   ; растяжка: столбец кадра ·4 у столбца вывода
hu_ymap:    resd MAXO                   ;   строка кадра ·640·4 у строки вывода
hq_n:       resd 2                      ; команд в списке буфера
hq_buf:     resb 2*HQMAX*HQSZ           ; списки HUD двух буферов кадра
fn_adv:     resd NFT*NCH                ; ширины знаков, единицы кадра
gc_tab:     resb NFT*NCH*16             ; кэш глифов: записи
gc_arena:   resb GC_SZ                  ;   и клетки

section .text
