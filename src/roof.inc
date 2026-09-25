; ----------------------------------------------------------------------------
;  roof.inc — избы: запись дома, высота крыши, снятие крыши (проход — roofdraw.inc).
;  Крыша рисуется после объектов: каждый кусок — выпуклый четырёхугольник в своей
;  мировой плоскости; на пиксель восстанавливается точка крыши (мир + высота),
;  сравнивается глубина с G-буфером (x+y по одной экранной точке), шейдер даёт цвет
;  и нормаль, свет — свой (небо по нормали и видимость дома), луна — в light_pass.
;  Локальные оси: u — вдоль конька, v — поперёк; +v и +u смотрят к камере.
; ----------------------------------------------------------------------------
section .text

; ---------------------------------------------------------------- запись дома
; ecx, edx = печь (тайл); размеры и флаги — H_x, H_y, H_w, H_h, H_lit, H_tv
add_house:
    push    rbx
    push    rsi
    push    rdi
    mov     eax, [nhouses]
    cmp     eax, MAXH
    jae     .out
    imul    edi, eax, HS_SZ
    lea     rbx, [houses]
    add     rbx, rdi
    inc     eax
    mov     [nhouses], eax
    mov     r8d, eax                    ; номер + 1 — в rmap
    mov     [rbx + HS_SX], ecx
    mov     [rbx + HS_SY], edx
    mov     eax, [H_x]
    mov     [rbx + HS_X0], eax
    mov     eax, [H_y]
    mov     [rbx + HS_Y0], eax
    mov     eax, [H_w]
    mov     [rbx + HS_W], eax
    mov     eax, [H_h]
    mov     [rbx + HS_H], eax
    mov     dword [rbx + HS_FADE], __float32__(1.0)
    mov     dword [rbx + HS_OCC], 0
    ; облик — от хеша места, а не от ГСЧ: село не перетасуется
    mov     eax, [H_x]
    imul    eax, eax, 7
    add     eax, 3
    mov     ecx, [H_y]
    imul    ecx, ecx, 13
    add     ecx, 5
    HASH    eax, ecx
    mov     [rbx + HS_SEED], eax
    mov     esi, eax
    ; конёк — вдоль длинной стороны
    xor     ecx, ecx
    mov     eax, [H_w]
    cmp     eax, [H_h]
    ja      .ax
    mov     ecx, 1
    jb      .ax
    mov     ecx, esi
    and     ecx, 1
.ax:
    mov     [rbx + HS_AX], ecx
    mov     eax, esi
    shr     eax, 4
    and     eax, 3
    cmp     eax, 3
    jb      .p
    mov     eax, 1
.p:
    add     eax, 11
    mov     [rbx + HS_P], eax
    mov     eax, [H_tv]
    add     eax, eax
    or      eax, [H_lit]
    test    esi, 0x300
    jnz     .f1
    or      eax, HF_SLIDE
.f1:
    test    esi, 0xC00
    jnz     .f2
    or      eax, HF_CROW
.f2:
    test    esi, 0x1000
    jz      .f3
    or      eax, HF_WASH
.f3:
    test    esi, 0x2000
    jz      .f4
    or      eax, HF_SUN
.f4:
    test    esi, 0x4000
    jz      .f5
    or      eax, HF_BARGE
.f5:
    mov     [rbx + HS_FL], eax
    mov     eax, esi
    shr     eax, 16
    and     eax, 3
    mov     [rbx + HS_MAT], eax
    mov     eax, esi
    shr     eax, 20
    and     eax, 7
    cmp     eax, 6
    jb      .g
    sub     eax, 6
.g:
    lea     rdx, [gab_pal]
    mov     eax, [rdx + rax*4]
    mov     [rbx + HS_GAB], eax
    ; снег на коньке и верх трубы
    mov     eax, [H_h]
    cmp     dword [rbx + HS_AX], 0
    je      .zr
    mov     eax, [H_w]
.zr:
    imul    eax, [rbx + HS_P]
    shr     eax, 1
    add     eax, RZW + RTS
    mov     [rbx + HS_ZR], eax
    mov     r10, rbx
    mov     ecx, [rbx + HS_SX]
    shl     ecx, 8
    add     ecx, 128
    mov     edx, [rbx + HS_SY]
    shl     edx, 8
    add     edx, 128
    call    roof_zs
    add     eax, RCHH
    mov     [rbx + HS_ZT], eax
    mov     [H_zt], eax
    ; тайлы под крышей
    mov     esi, [H_y]
.ry:
    mov     eax, [H_y]
    add     eax, [H_h]
    cmp     esi, eax
    jge     .out
    mov     edi, [H_x]
.rx:
    mov     eax, [H_x]
    add     eax, [H_w]
    cmp     edi, eax
    jge     .rn
    mov     eax, esi
    shl     eax, 7
    add     eax, edi
    lea     rdx, [rmap]
    mov     [rdx + rax], r8b
    inc     edi
    jmp     .rx
.rn:
    inc     esi
    jmp     .ry
.out:
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; r10 = дом, ecx = wx, edx = wy (8.8) -> eax = высота снега на крыше, px (портит ecx, edx)
roof_zs:
    cmp     dword [r10 + HS_AX], 0
    jne     .ay
    mov     eax, [r10 + HS_Y0]
    mov     ecx, [r10 + HS_H]
    jmp     .c
.ay:
    mov     edx, ecx
    mov     eax, [r10 + HS_X0]
    mov     ecx, [r10 + HS_W]
.c:
    shl     eax, 8
    shl     ecx, 7                      ; полупролёт
    add     eax, ecx                    ; конёк
    sub     edx, eax
    mov     eax, edx
    sar     eax, 31
    xor     edx, eax
    sub     edx, eax                    ; от конька
    sub     ecx, edx
    imul    ecx, [r10 + HS_P]
    sar     ecx, 8
    lea     eax, [ecx + RZW + RTS]
    ret

; ---------------------------------------------------------------- снятие крыши
; игрок в избе — крыша тает за ~0.3 с (как в PZ), вышел — возвращается
update_roofs:
    movss   xmm2, [dt]
    mulss   xmm2, [f_7]
    minss   xmm2, [f_1]
    lea     rdx, [houses]
    mov     ecx, [nhouses]
.l:
    test    ecx, ecx
    jz      .out
    movss   xmm3, [f_1]
    cvtsi2ss xmm0, dword [rdx + HS_X0]
    movss   xmm4, [pl + E_X]
    comiss  xmm0, xmm4
    ja      .t
    cvtsi2ss xmm1, dword [rdx + HS_W]
    addss   xmm0, xmm1
    comiss  xmm0, xmm4
    jbe     .t
    cvtsi2ss xmm0, dword [rdx + HS_Y0]
    movss   xmm4, [pl + E_Y]
    comiss  xmm0, xmm4
    ja      .t
    cvtsi2ss xmm1, dword [rdx + HS_H]
    addss   xmm0, xmm1
    comiss  xmm0, xmm4
    jbe     .t
    xorps   xmm3, xmm3
.t:
    movss   xmm0, [rdx + HS_FADE]
    subss   xmm3, xmm0
    mulss   xmm3, xmm2
    addss   xmm0, xmm3
    movss   [rdx + HS_FADE], xmm0
    add     rdx, HS_SZ
    dec     ecx
    jmp     .l
.out:
    ret

; ---------------------------------------------------------------- для roofdraw.inc
; (растеризатор и шейдеры собираются по копии на полосу экрана — bands.inc)
; угол из памяти: u, v, z (f32), номер
%macro RC 4
    movss   xmm0, %1
    movss   xmm1, %2
    movss   xmm2, %3
    mov     ecx, %4
    call    rq_cornl
%endmacro

; шейдер, нормаль по умолчанию, заливка
%macro RFILL 2
    lea     rax, [%1]
    mov     [rq_sh], rax
    mov     eax, %2
    mov     [rq_n0], eax
    call    rq_fill
%endmacro
