; ----------------------------------------------------------------------------
;  ao16.inc — AO у земли попиксельно: карта 8 на тайл, сколько неба видит низ
;  ячейки. Горизонт по hmf16 (в избах — стены и мебель) по 8 направлениям на тайл
;  вокруг: по направлению — наибольшее sinθ·(1 − d/R), AO = 1 − среднее. Занятая
;  ячейка (выше 3 px) берёт самую тёмную из соседей-земли: у стены и забора тень до
;  самого края, и грань стены, ствол, низ фигуры берут её же. light_rows — билинейно
;  у точки пикселя, к 32 px высоты на нет. Раз на карту
; ----------------------------------------------------------------------------
AOW         equ MAPW*8                  ; сторона ao16
AO_R        equ 16                      ; строк таблицы на вид (шаги по hmf16, 1/16 тайла)
AO_RS       equ 12                      ; шагов: 3/4 тайла вокруг
AO_ZF       equ 32                      ; к этой высоте, px, AO на нет
AOLIM       equ (AOW - 1)*32 - 1        ; предел точки выборки 2x2 (8.8 − 16)
AO_OCC      equ 3                       ; ячейка выше — занята
AO_MG       equ 9                       ; поле у края карты: там без AO
AO_K        equ 192                     ; сила: 255 − Σ·AO_K/2048 (256 — среднее)

section .text

calc_ao16:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32
    ; таблица: [прямо / наискось][шаг][высота] -> 255·sinθ·(1 − d/R)
    lea     rdi, [ao_lut]
    xor     ebx, ebx                    ; вид·AO_R + шаг − 1
.l1:
    mov     eax, ebx
    and     eax, AO_R - 1
    cvtsi2ss xmm1, eax
    addss   xmm1, [f_0_5]
    mulss   xmm1, [ao_cell]             ; d, px-единицы: от середины ячейки
    cmp     ebx, AO_R
    jb      .l2
    mulss   xmm1, [ao_sqrt2]
.l2:
    movaps  xmm3, xmm1
    mulss   xmm3, [ao_ird]
    movss   xmm2, [f_1]
    subss   xmm2, xmm3
    maxss   xmm2, [f_0]
    mulss   xmm2, [f_255]               ; 255·(1 − d/R)
    mulss   xmm1, xmm1                  ; d²
    xor     esi, esi                    ; h
.l3:
    cvtsi2ss xmm0, esi
    movaps  xmm3, xmm0
    mulss   xmm3, xmm0
    addss   xmm3, xmm1
    sqrtss  xmm3, xmm3
    divss   xmm0, xmm3                  ; sinθ
    mulss   xmm0, xmm2
    cvttss2si eax, xmm0
    mov     [rdi], al
    inc     rdi
    inc     esi
    cmp     esi, 128
    jb      .l3
    inc     ebx
    cmp     ebx, 2*AO_R
    jb      .l1
    lea     rcx, [ao16_rows]
    mov     edx, AOW
    mov     r8d, 16
    call    par_for
    lea     rcx, [ao16_dil]
    mov     edx, AOW
    mov     r8d, 16
    call    par_for
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; строки ecx .. edx−1 -> ao16t: земля 0..254, занятая — 255
ao16_rows:
    push    rbx
    push    rsi
    push    rdi
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    mov     r12d, ecx                   ; cy
    mov     r13d, edx
    lea     rsi, [hmf16]
.y:
    imul    edi, r12d, AOW
    lea     rax, [ao16t]
    add     rdi, rax                    ; строка ao16t
    xor     r14d, r14d                  ; cx
.x:
    mov     byte [rdi + r14], 255
    cmp     r12d, AO_MG
    jb      .xn
    cmp     r12d, AOW - AO_MG
    jae     .xn
    cmp     r14d, AO_MG
    jb      .xn
    cmp     r14d, AOW - AO_MG
    jae     .xn
    ; своя высота: 2x2 hmf16, есть ли выше AO_OCC
    imul    r15d, r12d, 2*HM16W
    lea     r15d, [r15d + r14d*2]       ; (2cx, 2cy)
    movzx   eax, word [rsi + r15]
    movzx   ecx, word [rsi + r15 + HM16W]
    shl     ecx, 16
    or      eax, ecx
    and     eax, 0x7F7F7F7F
    add     eax, 0x80808080 - (AO_OCC + 1)*0x01010101
    test    eax, 0x80808080
    jnz     .xn                         ; занята
    ; 8 направлений: наибольшее по шагам, в сумму
    xor     r10d, r10d                  ; Σ
    lea     r11, [ao_dirs]
    mov     ebx, 8
.d:
    movsxd  rbp, dword [r11]            ; шаг по hmf16
    movsxd  r8, dword [r11 + 4]         ; начало от (2cx, 2cy)
    add     r8, r15
    mov     r9d, [r11 + 8]
    lea     rax, [ao_lut]
    add     r9, rax                     ; строка таблицы: шаг 1
    xor     edx, edx
    mov     ecx, AO_RS
.s:
    add     r8, rbp
    movzx   eax, byte [rsi + r8]
    and     eax, 0x7F
    movzx   eax, byte [r9 + rax]
    cmp     eax, edx
    cmova   edx, eax
    add     r9, 128
    dec     ecx
    jnz     .s
    add     r10d, edx
    add     r11, 12
    dec     ebx
    jnz     .d
    imul    r10d, r10d, AO_K
    shr     r10d, 11
    mov     eax, 254
    cmp     r10d, eax
    cmova   r10d, eax
    sub     eax, r10d
    mov     [rdi + r14], al
.xn:
    inc     r14d
    cmp     r14d, AOW
    jb      .x
    inc     r12d
    cmp     r12d, r13d
    jb      .y
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; строки ecx .. edx−1: ao16t -> ao16. Земля 254 — открыто (255); занятая — самая
; тёмная из 8 соседей-земли (нет таких — 255)
ao16_dil:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    mov     r12d, ecx
    mov     r13d, edx
.y:
    imul    esi, r12d, AOW
    lea     rax, [ao16t]
    add     rsi, rax
    imul    edi, r12d, AOW
    lea     rax, [ao16]
    add     rdi, rax
    xor     ecx, ecx
.x:
    movzx   eax, byte [rsi + rcx]
    cmp     eax, 255
    jne     .put
    test    r12d, r12d
    jz      .put
    cmp     r12d, AOW - 1
    jae     .put
    test    ecx, ecx
    jz      .put
    cmp     ecx, AOW - 1
    jae     .put
    lea     rdx, [rsi + rcx]
    movzx   ebx, byte [rdx - AOW - 1]
    cmp     ebx, eax
    cmovb   eax, ebx
    movzx   ebx, byte [rdx - AOW]
    cmp     ebx, eax
    cmovb   eax, ebx
    movzx   ebx, byte [rdx - AOW + 1]
    cmp     ebx, eax
    cmovb   eax, ebx
    movzx   ebx, byte [rdx - 1]
    cmp     ebx, eax
    cmovb   eax, ebx
    movzx   ebx, byte [rdx + 1]
    cmp     ebx, eax
    cmovb   eax, ebx
    movzx   ebx, byte [rdx + AOW - 1]
    cmp     ebx, eax
    cmovb   eax, ebx
    movzx   ebx, byte [rdx + AOW]
    cmp     ebx, eax
    cmovb   eax, ebx
    movzx   ebx, byte [rdx + AOW + 1]
    cmp     ebx, eax
    cmovb   eax, ebx
.put:
    cmp     eax, 254
    jne     .p1
    inc     eax
.p1:
    mov     [rdi + rcx], al
    inc     ecx
    cmp     ecx, AOW
    jb      .x
    inc     r12d
    cmp     r12d, r13d
    jb      .y
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

section .rdata
align 4, db 0
; направления: шаг по hmf16, начало от (2cx, 2cy) (отрицательным — соседняя ячейка:
; середина ячейки ao16 — на углу четырёх), строка таблицы (прямо / наискось)
ao_dirs:
    dd 1, 0, 0
    dd -1, 1, 0
    dd HM16W, 0, 0
    dd -HM16W, HM16W, 0
    dd HM16W + 1, 0, AO_R*128
    dd HM16W - 1, 1, AO_R*128
    dd -HM16W + 1, HM16W, AO_R*128
    dd -HM16W - 1, HM16W + 1, AO_R*128
ao_cell:    dd 1.2256                   ; шаг hmf16, px-единицы
ao_sqrt2:   dd 1.4142136
ao_ird:     dd 0.0679949                ; 1 / (AO_RS·шаг)
ao_lim:     dd AOLIM
ao_zf:      dd AO_ZF

section .bss
alignb 16
ao_lut:     resb 2*AO_R*128
ao16t:      resb AOW*AOW
ao16:       resb AOW*AOW + 16           ; + поле: dl_row8 берёт дворды

section .text
