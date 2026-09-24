; ----------------------------------------------------------------------------
;  lightx.inc — отложенное освещение на AVX2: то же, что light_rows, по 8 пикселей
; ----------------------------------------------------------------------------
; Два слоя. Попиксельные числа (z, луна, индексы сеток, позёмка, выборки) — по
; дворду на пиксель, 8 пикселей в ymm; диапазоны малы, и dword-арифметика точна,
; а где в light_rows был pmaddwd — тот же vpmaddwd. Цвет — как в light_rows:
; пиксель — 4 слова (B, G, R, четвёртое), в ymm четыре пикселя, и каждая операция
; над словами та же (vpmullw — это pmullw, vpsrlw — psrlw, vpsubusw, vpminsw,
; vpaddsw, vpackuswb...), поэтому переполнения и насыщения те же и кадр совпадает
; до байта; этот слой идёт двумя половинами по 4 пикселя. Развилки (пустота, свой
; свет, искра, позёмка, память, эмиссия) — масками: считается всё, берётся
; нужное. Число из xmm (дворд на пиксель) раздаётся во все 4 слова пикселя:
; vpmovzxdq + vpshufb (lx_bw0)

section .rdata
align 32, db 0
lx_bw0:   db 0,1,0,1,0,1,0,1, 8,9,8,9,8,9,8,9, 0,1,0,1,0,1,0,1, 8,9,8,9,8,9,8,9
lx_bw3:   db 6,7,6,7,6,7,6,7, 14,15,14,15,14,15,14,15, 6,7,6,7,6,7,6,7, 14,15,14,15,14,15,14,15
lx_w255:  times 16 dw 255
lx_wlmax: times 4 dw 1023, 1023, 1023, 255
lx_w2047: times 16 dw 2047
lx_w64:   times 16 dw 64
lx_w190:  times 16 dw 190
lx_gray:  times 4 dw 1, 2, 1, 0
lx_d0_7:  dd 0, 1, 2, 3, 4, 5, 6, 7
lx_d0:    times 8 dd 0
lx_dffff: times 8 dd 0xFFFF
lx_d3f:   times 8 dd 0x3F
lx_dc0:   times 8 dd 0xC0
lx_dff:   times 8 dd 0xFF
lx_d31:   times 8 dd 31
lx_d63:   times 8 dd 63
lx_d32:   times 8 dd 32
lx_d64:   times 8 dd 64
lx_d128:  times 8 dd 128
lx_d256:  times 8 dd 256
lx_dmlim: times 8 dd MLIM
lx_dglim: times 8 dd GLIM
lx_dsw8:  times 8 dd SW8
lx_dgw:   times 8 dd GW
lx_d12:   times 8 dd 12
lx_d16k:  times 8 dd 16384
lx_d29:   times 8 dd 29
lx_d50:   times 8 dd 50
lx_d21:   times 8 dd 21
lx_d60:   times 8 dd 60

section .text

; своё на стеке (выровнен на 32): кадр — ymm по 8 двордов (или 4 пикселя словами)
LX_CAM   equ 0                          ; camx + camy
LX_GOX   equ 32                         ; gox·64
LX_GOY   equ 64
LX_MOX   equ 96                         ; начало развёртки теней, 8.8
LX_MOY   equ 128
LX_PDX   equ 160                        ; оси позёмки
LX_PDY   equ 192
LX_PS1   equ 224
LX_PS2   equ 256
LX_ROWZ  equ 288                        ; CY0 − строка
LX_GOFF  equ 320                        ; сдвиг зерна
LX_TINT  equ 352                        ; оттенок кадра, «память», пелена — словами
LX_MEMT  equ 384
LX_HAZE  equ 416
LX_END   equ 448                        ; конец диапазона, пиксель
LX_MOON  equ 452                        ; есть ли луна
LX_DRF   equ 456                        ; в группе есть позёмка
LX_PIX   equ 460                        ; пиксель начала строки (на время dl_row)
; группа из 8 пикселей: по дворду на пиксель
LX_A     equ 480                        ; альбедо
LX_GX    equ 512
LX_GY    equ 544
LX_Z     equ 576
LX_NB    equ 608
LX_FX    equ 640                        ; доли внутри сэмпла сетки
LX_FY    equ 672
LX_SIDX  equ 704
LX_MV    equ 736                        ; луна дошла, 0..255
LX_OWN   equ 768                        ; свой свет (маска)
LX_N2    equ 800                        ; индекс нормали для amb_d, moon_n
LX_D     equ 832                        ; доля позёмки, 0..60
LX_VG    equ 864                        ; виньетка
LX_GR    equ 896                        ; зерно (со знаком)
LX_S00   equ 928                        ; 4 сэмпла сетки света
LX_S01   equ 960
LX_S10   equ 992
LX_S11   equ 1024
LX_SZ    equ 1088

; число в xmm (дворд на пиксель) -> ymm: в каждое слово пикселя
%macro LXBC 2
    vpmovzxdq %1, %2
    vpshufb %1, %1, [lx_bw0]
%endmacro

; одна ячейка развёртки: ymm8 — индекс, %1 — смещение, %2 — вес ячейки;
; ymm14 = z + 64, сумма — в ymm15
%macro LXCELL 2
    vpcmpeqd ymm7, ymm7, ymm7
    vpgatherdd ymm12, [rsi + ymm8*4 + (mz8 - albedo) + %1], ymm7
    vpand   ymm13, ymm12, [lx_dff]              ; горизонт + 64
    vpsubd  ymm13, ymm14, ymm13
    vpsrld  ymm7, ymm12, 16                     ; 4096 / полутень
    vpmaddwd ymm13, ymm13, ymm7                 ; (z − hz)·ip — как pmaddwd в light_rows
    vpsrad  ymm13, ymm13, 4
    vpaddd  ymm13, ymm13, [lx_d128]
    vpmaxsd ymm13, ymm13, [lx_d0]
    vpminsd ymm13, ymm13, [lx_d256]             ; 0..256
    vpsrld  ymm12, ymm12, 8
    vpand   ymm12, ymm12, [lx_dff]              ; облака
    vpmaddwd ymm13, ymm13, ymm12
    vpsrld  ymm13, ymm13, 8
    vpmaddwd ymm13, ymm13, %2
    vpaddd  ymm15, ymm15, ymm13
%endmacro

; цвет 4 пикселей группы: %1 = 0 — первые, 1 — вторые
%macro LXHALF 1
    ; сетка света: билинейно по 4 сэмплам
    vpmovzxbw ymm10, [rsp + LX_S00 + 16*%1]
    vpmovzxbw ymm11, [rsp + LX_S01 + 16*%1]
    vpmovzxbw ymm12, [rsp + LX_S10 + 16*%1]
    vpmovzxbw ymm13, [rsp + LX_S11 + 16*%1]
    vmovdqa xmm8, [rsp + LX_FX + 16*%1]
    LXBC    ymm8, xmm8                  ; fx
    vmovdqa ymm14, [lx_w64]
    vpsubw  ymm14, ymm14, ymm8          ; 64 − fx
    vpmullw ymm10, ymm10, ymm14
    vpmullw ymm11, ymm11, ymm8
    vpaddw  ymm10, ymm10, ymm11
    vpsrlw  ymm10, ymm10, 6             ; верхний ряд
    vpmullw ymm12, ymm12, ymm14
    vpmullw ymm13, ymm13, ymm8
    vpaddw  ymm12, ymm12, ymm13
    vpsrlw  ymm12, ymm12, 6             ; нижний
    vmovdqa xmm9, [rsp + LX_FY + 16*%1]
    LXBC    ymm9, xmm9                  ; fy
    vmovdqa ymm14, [lx_w64]
    vpsubw  ymm14, ymm14, ymm9
    vpmullw ymm10, ymm10, ymm14
    vpmullw ymm12, ymm12, ymm9
    vpaddw  ymm10, ymm10, ymm12
    vpsrlw  ymm10, ymm10, 6             ; L: B, G, R, видимость
    vmovdqa xmm7, [rsp + LX_N2 + 16*%1]
    vpcmpeqd ymm8, ymm8, ymm8
    vpgatherdq ymm9, [rsi + xmm7*8 + (amb_d - albedo)], ymm8
    vpsubusw ymm10, ymm10, ymm9         ; грань видит меньше неба
    vpmovzxbw ymm11, [rsi + rcx*4 + (lbuf - albedo) + 16*%1]
    vmovdqa xmm6, [rsp + LX_OWN + 16*%1]
    LXBC    ymm6, xmm6
    vpblendvb ymm10, ymm10, ymm11, ymm6 ; свой свет — из lbuf
    vpcmpeqd ymm8, ymm8, ymm8
    vpgatherdq ymm9, [rsi + xmm7*8 + (moon_n - albedo)], ymm8
    vmovdqa xmm12, [rsp + LX_MV + 16*%1]
    LXBC    ymm12, xmm12
    vpmullw ymm9, ymm9, ymm12           ; луна по нормали
    vpsrlw  ymm9, ymm9, 8
    vpaddw  ymm10, ymm10, ymm9
    vpaddw  ymm10, ymm10, [rsi + rcx*8 + (dlb - albedo) + 32*%1]   ; огни (dl_row)
    vpminsw ymm10, ymm10, [lx_wlmax]    ; L: свет до 1023, видимость до 255
    ; альбедо · L / 128 — как в light_rows: (a << 7)·(L << 2) >> 16
    vmovdqa xmm0, [rsp + LX_A + 16*%1]
    vpmovzxbw ymm1, xmm0
    vpsllw  ymm2, ymm1, 7
    vpsllw  ymm3, ymm10, 2
    vpmulhuw ymm2, ymm2, ymm3           ; X
    vpslld  xmm3, xmm0, 1
    vpsrad  xmm3, xmm3, 31              ; искра на снегу (бит 30) — вдвое
    LXBC    ymm3, xmm3
    vpand   ymm3, ymm3, ymm2
    vpaddw  ymm2, ymm2, ymm3
    ; тоновая кривая: 16 слов -> два сбора по 8 двордов из tm_lut
    vpminsw ymm2, ymm2, [lx_w2047]
    vpmovzxwd ymm3, xmm2
    vextracti128 xmm4, ymm2, 1
    vpmovzxwd ymm4, xmm4
    vpcmpeqd ymm5, ymm5, ymm5
    vpgatherdd ymm6, [rsi + ymm3*4 + (tm_lut - albedo)], ymm5
    vpcmpeqd ymm5, ymm5, ymm5
    vpgatherdd ymm7, [rsi + ymm4*4 + (tm_lut - albedo)], ymm5
    vpackusdw ymm2, ymm6, ymm7
    vpermq  ymm2, ymm2, 0xD8
    ; позёмка: X += (летящий снег − X)·доля/64 (доля 0 — без изменений)
    cmp     dword [rsp + LX_DRF], 0
    je      %%nd
    vmovdqa xmm3, [rsp + LX_D + 16*%1]
    LXBC    ymm3, xmm3
    vpminsw ymm4, ymm10, [lx_w255]
    vpmullw ymm4, ymm4, [lx_w190]       ; свет·190/64, не ярче 255
    vpsrlw  ymm4, ymm4, 6
    vpminsw ymm4, ymm4, [lx_w255]
    vpsubw  ymm4, ymm4, ymm2
    vpmullw ymm4, ymm4, ymm3
    vpsraw  ymm4, ymm4, 6
    vpaddw  ymm2, ymm2, ymm4
%%nd:
    vpxor   ymm3, ymm3, ymm3
    vpmaxsw ymm2, ymm2, ymm3
    vpminsw ymm2, ymm2, [lx_w255]       ; X, насыщено до 255
    ; «память» вне обзора (видимость V < 255): тусклый серо-синий
    vpshufb ymm3, ymm10, [lx_bw3]       ; V в каждое слово пикселя
    vpmaddwd ymm4, ymm2, [lx_gray]
    vpshufd ymm5, ymm4, 0xB1
    vpaddd  ymm4, ymm4, ymm5
    vpsrld  ymm4, ymm4, 2
    vpshufb ymm4, ymm4, [lx_bw0]        ; яркость
    vpmullw ymm4, ymm4, [rsp + LX_MEMT]
    vpsrlw  ymm4, ymm4, 7
    vpaddw  ymm4, ymm4, [rsp + LX_HAZE]
    vpmullw ymm5, ymm2, ymm3
    vmovdqa ymm6, [lx_w255]
    vpsubw  ymm6, ymm6, ymm3
    vpmullw ymm4, ymm4, ymm6
    vpaddw  ymm5, ymm5, ymm4
    vpsrlw  ymm5, ymm5, 8
    vmovdqa ymm6, [lx_w255]
    vpcmpgtw ymm6, ymm6, ymm3
    vpblendvb ymm2, ymm2, ymm5, ymm6
    ; оттенок кадра
    vpmullw ymm2, ymm2, [rsp + LX_TINT]
    vpsrlw  ymm2, ymm2, 8
    ; эмиссивные — своим цветом, без света
    vpsrad  xmm3, xmm0, 31
    LXBC    ymm3, xmm3
    vpblendvb ymm2, ymm2, ymm1, ymm3
    ; виньетка и зерно
    vmovdqa xmm3, [rsp + LX_VG + 16*%1]
    LXBC    ymm3, xmm3
    vpmullw ymm2, ymm2, ymm3
    vpsrlw  ymm2, ymm2, 8
    vmovdqa xmm5, [rsp + LX_GR + 16*%1]
    LXBC    ymm5, xmm5
    vpaddsw ymm2, ymm2, ymm5
    vpackuswb ymm2, ymm2, ymm2
    vpermq  ymm2, ymm2, 0x08            ; 4 пикселя — в низ
    vpxor   xmm3, xmm3, xmm3
    vpcmpeqd xmm3, xmm3, xmm0           ; пустота — ноль
    vpandn  xmm2, xmm3, xmm2
    vmovdqu [r8 + rcx*4 + 16*%1], xmm2
%endmacro

; строки ecx .. edx−1 — как light_rows (с любого потока пула)
light_rows_x:
    push    rbx
    push    rsi
    push    rdi
    push    rbp
    mov     rbp, rsp
    sub     rsp, LX_SZ
    and     rsp, -32
    imul    eax, edx, SCR_W
    mov     [rsp + LX_END], eax
    mov     ebx, CY0
    sub     ebx, ecx                    ; CY0 − строка
    imul    ecx, ecx, SCR_W
    lea     rsi, [albedo]
    mov     r8, [fbbits]
    mov     eax, [camx]
    add     eax, [camy]
    vmovd   xmm0, eax
    vpbroadcastd ymm0, xmm0
    vmovdqa [rsp + LX_CAM], ymm0
    mov     eax, [gox]
    shl     eax, 6
    vmovd   xmm0, eax
    vpbroadcastd ymm0, xmm0
    vmovdqa [rsp + LX_GOX], ymm0
    mov     eax, [goy]
    shl     eax, 6
    vmovd   xmm0, eax
    vpbroadcastd ymm0, xmm0
    vmovdqa [rsp + LX_GOY], ymm0
    vpbroadcastd ymm0, [mo_x]
    vmovdqa [rsp + LX_MOX], ymm0
    vpbroadcastd ymm0, [mo_y]
    vmovdqa [rsp + LX_MOY], ymm0
    vpbroadcastd ymm0, [pz_dx]
    vmovdqa [rsp + LX_PDX], ymm0
    vpbroadcastd ymm0, [pz_dy]
    vmovdqa [rsp + LX_PDY], ymm0
    vpbroadcastd ymm0, [pz_s1]
    vmovdqa [rsp + LX_PS1], ymm0
    vpbroadcastd ymm0, [pz_s2]
    vmovdqa [rsp + LX_PS2], ymm0
    vpbroadcastd ymm0, [grain_off]
    vmovdqa [rsp + LX_GOFF], ymm0
    vpbroadcastq ymm0, [tint_vec]
    vmovdqa [rsp + LX_TINT], ymm0
    vpbroadcastq ymm0, [memtint_v]
    vmovdqa [rsp + LX_MEMT], ymm0
    vpbroadcastq ymm0, [haze_v]
    vmovdqa [rsp + LX_HAZE], ymm0
    mov     eax, [moon_on]
    mov     [rsp + LX_MOON], eax
.row:
    ; свет огней строки — попиксельно, в dlb (lights.inc: SSE, тот же код, что у light_rows)
    vzeroupper
    mov     [rsp + LX_PIX], ecx
    mov     ecx, CY0
    sub     ecx, ebx
    call    dl_row
    mov     ecx, [rsp + LX_PIX]
    mov     r8, [fbbits]
    vmovd   xmm0, ebx
    vpbroadcastd ymm0, xmm0
    vmovdqa [rsp + LX_ROWZ], ymm0
    mov     edi, SCR_W/8
.grp:
    vmovdqu ymm0, [rsi + rcx*4]         ; альбедо
    vptest  ymm0, ymm0
    jz      .void                       ; все восемь — пустота
    vmovdqa [rsp + LX_A], ymm0
    vmovdqu ymm1, [rsi + rcx*4 + (gposb - albedo)]
    vpand   ymm2, ymm1, [lx_dffff]      ; gx
    vpsrld  ymm3, ymm1, 16              ; gy
    vmovdqa [rsp + LX_GX], ymm2
    vmovdqa [rsp + LX_GY], ymm3
    vpaddd  ymm4, ymm2, ymm3
    vpsubd  ymm4, ymm4, [rsp + LX_CAM]
    vpsrad  ymm4, ymm4, 5
    vpaddd  ymm4, ymm4, [rsp + LX_ROWZ] ; z пикселя, px
    vmovdqa [rsp + LX_Z], ymm4
    vpmovzxbd ymm5, [rsi + rcx + (nbuf - albedo)]
    vmovdqa [rsp + LX_NB], ymm5
    ; ----- луна: 2x2 ячейки развёртки у точки, сдвинутой к луне по нормали
    vpxor   ymm15, ymm15, ymm15
    cmp     dword [rsp + LX_MOON], 0
    je      .nomoon
    vpand   ymm6, ymm5, [lx_d3f]
    vpcmpeqd ymm7, ymm7, ymm7
    vpgatherdd ymm8, [rsi + ymm6*8 + (msh - albedo)], ymm7
    vpcmpeqd ymm7, ymm7, ymm7
    vpgatherdd ymm9, [rsi + ymm6*8 + (msh - albedo) + 4], ymm7
    vpaddd  ymm8, ymm8, ymm2
    vpsubd  ymm8, ymm8, [rsp + LX_MOX]
    vpminud ymm8, ymm8, [lx_dmlim]      ; mx (и отрицательные — к краю)
    vpaddd  ymm9, ymm9, ymm3
    vpsubd  ymm9, ymm9, [rsp + LX_MOY]
    vpminud ymm9, ymm9, [lx_dmlim]      ; my
    vpand   ymm10, ymm8, [lx_d31]       ; fx
    vpand   ymm11, ymm9, [lx_d31]       ; fy
    vpsrld  ymm8, ymm8, 5
    vpsrld  ymm9, ymm9, 5
    vpmaddwd ymm9, ymm9, [lx_dsw8]
    vpaddd  ymm8, ymm8, ymm9            ; ячейка
    vmovdqa ymm12, [lx_d32]             ; веса 2x2 (в сумме 1024)
    vpsubd  ymm13, ymm12, ymm10         ; 32 − fx
    vpsubd  ymm12, ymm12, ymm11         ; 32 − fy
    vpmaddwd ymm0, ymm12, ymm13
    vpmaddwd ymm1, ymm12, ymm10
    vpmaddwd ymm5, ymm11, ymm13
    vpmaddwd ymm6, ymm11, ymm10
    vpaddd  ymm14, ymm4, [lx_d64]       ; z + 64
    LXCELL  0, ymm0
    LXCELL  4, ymm1
    LXCELL  SW8*4, ymm5
    LXCELL  SW8*4 + 4, ymm6
    vpsrld  ymm15, ymm15, 10            ; дошло луны 0..255
.nomoon:
    vmovdqa [rsp + LX_MV], ymm15
    ; ----- сетка света: сэмпл, доли, 4 выборки
    vpsubd  ymm6, ymm2, [rsp + LX_GOX]
    vpsubd  ymm7, ymm3, [rsp + LX_GOY]
    vpmaxsd ymm6, ymm6, [lx_d0]
    vpmaxsd ymm7, ymm7, [lx_d0]
    vpminsd ymm6, ymm6, [lx_dglim]
    vpminsd ymm7, ymm7, [lx_dglim]
    vpand   ymm8, ymm6, [lx_d63]
    vpand   ymm9, ymm7, [lx_d63]
    vmovdqa [rsp + LX_FX], ymm8
    vmovdqa [rsp + LX_FY], ymm9
    vpsrld  ymm6, ymm6, 6
    vpsrld  ymm7, ymm7, 6
    vpmaddwd ymm7, ymm7, [lx_dgw]
    vpaddd  ymm6, ymm6, ymm7            ; сэмпл
    vmovdqa [rsp + LX_SIDX], ymm6
    vpcmpeqd ymm7, ymm7, ymm7
    vpgatherdd ymm10, [rsi + ymm6*4 + (lgs - albedo)], ymm7
    vpcmpeqd ymm7, ymm7, ymm7
    vpgatherdd ymm11, [rsi + ymm6*4 + (lgs - albedo) + 4], ymm7
    vpcmpeqd ymm7, ymm7, ymm7
    vpgatherdd ymm12, [rsi + ymm6*4 + (lgs - albedo) + GW*4], ymm7
    vpcmpeqd ymm7, ymm7, ymm7
    vpgatherdd ymm13, [rsi + ymm6*4 + (lgs - albedo) + GW*4 + 4], ymm7
    vmovdqa [rsp + LX_S00], ymm10
    vmovdqa [rsp + LX_S01], ymm11
    vmovdqa [rsp + LX_S10], ymm12
    vmovdqa [rsp + LX_S11], ymm13
    ; нормаль: у своего света — без флагов (& 0x3F), у остальных — байт как есть
    vmovdqa ymm5, [rsp + LX_NB]
    vpslld  ymm6, ymm5, 24
    vpsrad  ymm6, ymm6, 31              ; свой свет (NOWN)
    vmovdqa [rsp + LX_OWN], ymm6
    vpand   ymm7, ymm6, [lx_dc0]
    vpandn  ymm7, ymm7, ymm5
    vmovdqa [rsp + LX_N2], ymm7
    ; ----- позёмка: у земли (z < 12), где метёт — доля 0..60
    mov     dword [rsp + LX_DRF], 0
    vmovdqa ymm5, [lx_d12]
    vpcmpgtd ymm5, ymm5, ymm4           ; z < 12
    vmovdqa ymm6, [rsp + LX_SIDX]
    vpcmpeqd ymm7, ymm7, ymm7
    vpgatherdd ymm8, [rsi + ymm6*1 + (wgrid - albedo)], ymm7
    vpcmpeqd ymm7, ymm7, ymm7
    vpgatherdd ymm9, [rsi + ymm6*1 + (wgrid - albedo) + GW], ymm7
    vpand   ymm8, ymm8, [lx_dffff]      ; маска ветра: сэмпл и сосед справа
    vpand   ymm9, ymm9, [lx_dffff]      ; строкой ниже
    vpor    ymm7, ymm8, ymm9
    vptest  ymm7, ymm5
    jz      .nodrift
    mov     dword [rsp + LX_DRF], 1
    vmovdqa ymm11, [rsp + LX_FX]
    vmovdqa ymm12, [rsp + LX_FY]
    vpand   ymm13, ymm8, [lx_dff]
    vpsrld  ymm8, ymm8, 8
    vpsubd  ymm8, ymm8, ymm13
    vpmaddwd ymm8, ymm8, ymm11
    vpslld  ymm13, ymm13, 6
    vpaddd  ymm8, ymm8, ymm13           ; верхняя строка ·64
    vpand   ymm13, ymm9, [lx_dff]
    vpsrld  ymm9, ymm9, 8
    vpsubd  ymm9, ymm9, ymm13
    vpmaddwd ymm9, ymm9, ymm11
    vpslld  ymm13, ymm13, 6
    vpaddd  ymm9, ymm9, ymm13           ; нижняя
    vpsubd  ymm9, ymm9, ymm8
    vpmaddwd ymm9, ymm9, ymm12
    vpsrad  ymm9, ymm9, 6
    vpaddd  ymm8, ymm8, ymm9
    vpsrld  ymm8, ymm8, 6               ; M 0..255
    vpsubd  ymm13, ymm2, [lx_d16k]      ; от середины карты
    vpsubd  ymm14, ymm3, [lx_d16k]
    vpmulld ymm15, ymm13, [rsp + LX_PDX]
    vpmulld ymm0, ymm14, [rsp + LX_PDY]
    vpaddd  ymm15, ymm15, ymm0          ; вдоль, тайлы·65536
    vpmulld ymm14, ymm14, [rsp + LX_PDX]
    vpmulld ymm13, ymm13, [rsp + LX_PDY]
    vpsubd  ymm14, ymm14, ymm13
    vpsrad  ymm14, ymm14, 11            ; поперёк, 1/32 тайла
    vpsrad  ymm0, ymm15, 12             ; слой 1
    vpsubd  ymm0, ymm0, [rsp + LX_PS1]
    vpand   ymm0, ymm0, [lx_dff]
    vpand   ymm1, ymm14, [lx_d63]
    vpslld  ymm1, ymm1, 8
    vpaddd  ymm0, ymm0, ymm1
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm10, [rsi + ymm0*1 + (strk - albedo)], ymm1
    vpand   ymm10, ymm10, [lx_dff]
    vpslld  ymm0, ymm15, 1              ; слой 2: вдоль·3
    vpaddd  ymm0, ymm0, ymm15
    vpsrad  ymm0, ymm0, 13
    vpsubd  ymm0, ymm0, [rsp + LX_PS2]
    vpand   ymm0, ymm0, [lx_dff]
    vmovdqa ymm1, [lx_d29]
    vpsubd  ymm1, ymm1, ymm14           ; поперёк зеркально
    vpand   ymm1, ymm1, [lx_d63]
    vpslld  ymm1, ymm1, 8
    vpaddd  ymm0, ymm0, ymm1
    vpcmpeqd ymm1, ymm1, ymm1
    vpgatherdd ymm6, [rsi + ymm0*1 + (strk - albedo)], ymm1
    vpand   ymm6, ymm6, [lx_dff]
    vpaddd  ymm10, ymm10, ymm6
    vpaddd  ymm10, ymm10, [lx_d50]
    vpmaddwd ymm10, ymm10, ymm8
    vpsrld  ymm10, ymm10, 11
    vpmaxsd ymm4, ymm4, [lx_d0]
    vmovdqa ymm1, [lx_d12]
    vpsubd  ymm1, ymm1, ymm4
    vpmaddwd ymm10, ymm10, ymm1
    vpmaddwd ymm10, ymm10, [lx_d21]
    vpsrld  ymm10, ymm10, 8
    vpminud ymm10, ymm10, [lx_d60]
    vpand   ymm10, ymm10, ymm5          ; только где z < 12
    vmovdqa [rsp + LX_D], ymm10
.nodrift:
    ; ----- виньетка и зерно: байты (c + k + grain_off) & 0xFFFF
    vpmovzxbd ymm3, [rsi + rcx + (vign - albedo)]
    vmovdqa [rsp + LX_VG], ymm3
    vmovd   xmm3, ecx
    vpbroadcastd ymm3, xmm3
    vpaddd  ymm3, ymm3, [lx_d0_7]
    vpaddd  ymm3, ymm3, [rsp + LX_GOFF]
    vpand   ymm3, ymm3, [lx_dffff]
    vpcmpeqd ymm4, ymm4, ymm4
    vpgatherdd ymm5, [rsi + ymm3*1 + (grain - albedo)], ymm4
    vpslld  ymm5, ymm5, 24
    vpsrad  ymm5, ymm5, 24
    vmovdqa [rsp + LX_GR], ymm5
    ; ----- цвет: две половины по 4 пикселя
    LXHALF  0
    LXHALF  1
.next:
    add     ecx, 8
    dec     edi
    jnz     .grp
    dec     ebx
    cmp     ecx, [rsp + LX_END]
    jb      .row
    vzeroupper
    mov     rsp, rbp
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret
.void:
    vmovdqu [r8 + rcx*4], ymm0
    jmp     .next

; AVX2 есть и включён у ОС (XCR0: XMM и YMM) -> have_avx2 = 1; --noavx — не брать
cpu_detect:
    push    rbx
    cmp     dword [no_avx], 0
    jne     .no
    mov     eax, 1
    cpuid
    and     ecx, (1 << 27) | (1 << 28)  ; OSXSAVE, AVX
    cmp     ecx, (1 << 27) | (1 << 28)
    jne     .no
    xor     ecx, ecx
    xgetbv
    and     eax, 6
    cmp     eax, 6
    jne     .no
    mov     eax, 7
    xor     ecx, ecx
    cpuid
    test    ebx, 1 << 5                 ; AVX2
    jz      .no
    mov     dword [have_avx2], 1
.no:
    pop     rbx
    ret

section .data
have_avx2:  dd 0
no_avx:     dd 0

section .text
