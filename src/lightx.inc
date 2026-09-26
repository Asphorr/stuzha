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
lx_d3f:   times 8 dd 0x7F
lx_dc0:   times 8 dd 0x80
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
lx_dm1:   times 8 dd -1
lx_d16:   times 8 dd 16
lx_daolim: times 8 dd AOLIM
lx_bw01:  times 2 db 0,-1,1,-1, 4,-1,5,-1, 8,-1,9,-1, 12,-1,13,-1   ; байты 0, 1 дворда -> слова
lx_hmask: times 8 dd 0                  ; dl_row8: дорожки четвёрок по битам 1, 2
          times 4 dd -1
          times 4 dd 0
          times 4 dd 0
          times 4 dd -1
          times 8 dd -1

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
LX_AO    equ 1056                       ; AO у земли: множитель 0..256
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
    vmovdqa xmm8, [rsp + LX_AO + 16*%1]
    LXBC    ymm8, xmm8
    vpblendw ymm8, ymm8, [lt_w256], 0x88
    vpmullw ymm10, ymm10, ymm8
    vpsrlw  ymm10, ymm10, 8             ; и низ у стен, заборов, стволов — AO
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
    ; пересвет белеет: каждому каналу — доля сильнейшего сверх порога (grade.inc)
    vpshuflw ymm3, ymm2, 0xC9
    vpshufhw ymm3, ymm3, 0xC9           ; G R B A
    vpmaxsw ymm3, ymm3, ymm2
    vpshuflw ymm4, ymm2, 0xD2
    vpshufhw ymm4, ymm4, 0xD2           ; R B G A
    vpmaxsw ymm3, ymm3, ymm4
    vpsubusw ymm3, ymm3, [hl_k2]
    vpmulhuw ymm3, ymm3, [hl_kk]
    vpaddw  ymm2, ymm2, ymm3
    ; тоновая кривая: 16 слов -> два сбора по 8 двордов из tm_lut; до колена (≤ TM_K)
    ; она — тождество (если глаз привык: E = 1), и если все 16 там, сборы не нужны
    vpminsw ymm2, ymm2, [lx_w2047]
    vpcmpgtw ymm3, ymm2, [tm_thr]
    vptest  ymm3, ymm3
    jz      %%tmid
    vpmovzxwd ymm3, xmm2
    vextracti128 xmm4, ymm2, 1
    vpmovzxwd ymm4, xmm4
    vpcmpeqd ymm5, ymm5, ymm5
    vpgatherdd ymm6, [rsi + ymm3*4 + (tm_lut - albedo)], ymm5
    vpcmpeqd ymm5, ymm5, ymm5
    vpgatherdd ymm7, [rsi + ymm4*4 + (tm_lut - albedo)], ymm5
    vpackusdw ymm2, ymm6, ymm7
    vpermq  ymm2, ymm2, 0xD8
%%tmid:
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
    ; свет огней строки — попиксельно, в dlb (dl_row8 ниже: dl_row по 8 пикселей)
    mov     [rsp + LX_PIX], ecx
    mov     ecx, CY0
    sub     ecx, ebx
    call    dl_row8
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
    vpmovzxbd ymm0, [rsi + rcx + (mfsh - albedo)]
    vpsubd  ymm0, ymm0, [lx_dm1]
    vpmulld ymm15, ymm15, ymm0
    vpsrld  ymm15, ymm15, 8             ; и мимо фигур (dl_row8)
.nomoon:
    vmovdqa [rsp + LX_MV], ymm15
    ; ----- AO у земли (как в light_rows): множитель 0..256 (ymm2–4 не трогает)
    vmovdqa ymm12, [lx_d256]
    vmovdqa ymm5, [lx_d32]
    vpcmpgtd ymm5, ymm5, ymm4           ; z < AO_ZF
    vptest  ymm5, ymm5
    jz      .aod                        ; все выше — AO нет
    vpsubd  ymm8, ymm2, [lx_d16]
    vpsubd  ymm9, ymm3, [lx_d16]
    vpminud ymm8, ymm8, [lx_daolim]
    vpminud ymm9, ymm9, [lx_daolim]
    vpand   ymm10, ymm8, [lx_d31]       ; fx
    vpand   ymm11, ymm9, [lx_d31]       ; fy
    vpsrld  ymm8, ymm8, 5
    vpsrld  ymm9, ymm9, 5
    vpslld  ymm9, ymm9, 10              ; · AOW
    vpaddd  ymm8, ymm8, ymm9
    vpcmpeqd ymm7, ymm7, ymm7
    vpgatherdd ymm12, [rsi + ymm8*1 + (ao16 - albedo)], ymm7
    vpcmpeqd ymm7, ymm7, ymm7
    vpgatherdd ymm13, [rsi + ymm8*1 + (ao16 - albedo) + AOW], ymm7
    vpshufb ymm12, ymm12, [lx_bw01]     ; a00 | a01 << 16
    vpshufb ymm13, ymm13, [lx_bw01]     ; a10 | a11 << 16
    vmovdqa ymm14, [lx_d32]
    vpsubd  ymm14, ymm14, ymm10
    vpslld  ymm15, ymm10, 16
    vpor    ymm14, ymm14, ymm15         ; 32 − fx | fx << 16
    vpmaddwd ymm12, ymm12, ymm14
    vpmaddwd ymm13, ymm13, ymm14
    vpslld  ymm13, ymm13, 16
    vpor    ymm12, ymm12, ymm13
    vmovdqa ymm14, [lx_d32]
    vpsubd  ymm14, ymm14, ymm11
    vpslld  ymm15, ymm11, 16
    vpor    ymm14, ymm14, ymm15         ; 32 − fy | fy << 16
    vpmaddwd ymm12, ymm12, ymm14
    vpsrld  ymm12, ymm12, 10            ; AO 0..255
    vpsrld  ymm13, ymm12, 7
    vpaddd  ymm12, ymm12, ymm13         ; 0..256
    vmovdqa ymm13, [lx_d256]
    vpsubd  ymm12, ymm13, ymm12
    vmovdqa ymm14, [lx_d32]
    vpsubd  ymm14, ymm14, ymm4
    vpmaxsd ymm14, ymm14, [lx_d0]
    vpminsd ymm14, ymm14, [lx_d32]      ; к AO_ZF px — на нет
    vpmaddwd ymm12, ymm12, ymm14
    vpsrld  ymm12, ymm12, 5
    vpsubd  ymm12, ymm13, ymm12
.aod:
    vmovdqa [rsp + LX_AO], ymm12
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
    ; нормаль: у своего света — без флага (& 0x7F), у остальных — байт как есть
    vmovdqa ymm5, [rsp + LX_NB]
    vpslld  ymm6, ymm5, 24
    vpsrad  ymm6, ymm6, 31              ; свой свет (NOWN)
    vmovdqa [rsp + LX_OWN], ymm6
    vpand   ymm7, ymm6, [lx_dc0]
    vpandn  ymm7, ymm7, ymm5
    vmovdqa [rsp + LX_N2], ymm7
    ; ----- позёмка: у земли (z < 12), где метёт — доля 0..60
    mov     dword [rsp + LX_DRF], 0
    cmp     dword [wg_on], 0
    je      .nodrift                    ; тихо — маска нулевая
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
    ; ----- виньетка и зерно: байты (c + k + grain_off) & 0xFFFF — подряд, кроме стыка
    vpmovzxbd ymm3, [rsi + rcx + (vign - albedo)]
    vmovdqa [rsp + LX_VG], ymm3
    mov     eax, [rsp + LX_GOFF]
    add     eax, ecx
    and     eax, 0xFFFF
    cmp     eax, 0x10000 - 8
    ja      .grw
    vpmovsxbd ymm5, [rsi + rax + (grain - albedo)]
    jmp     .grd
.grw:
    vmovd   xmm3, ecx
    vpbroadcastd ymm3, xmm3
    vpaddd  ymm3, ymm3, [lx_d0_7]
    vpaddd  ymm3, ymm3, [rsp + LX_GOFF]
    vpand   ymm3, ymm3, [lx_dffff]
    vpcmpeqd ymm4, ymm4, ymm4
    vpgatherdd ymm5, [rsi + ymm3*1 + (grain - albedo)], ymm4
    vpslld  ymm5, ymm5, 24
    vpsrad  ymm5, ymm5, 24
.grd:
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

; ---------------------------------------------------------------- огни по 8 пикселей
; dl_row8: то же, что dl_row (lights.inc), по 8 пикселей. Четвёрки dl_row лежат в
; половинах ymm (дорожка = пиксель), и каждая операция — та же, что у dl_row на
; своей дорожке: над f32 — поэлементно, над словами и двордами VEX-256 работает по
; 128-битным половинам, как SSE над каждой четвёркой (порядок операндов тот же) —
; поэтому dlb совпадает до байта. Решения «на группу» — как у dl_row, по четвёркам:
; рамка тени фигуры сверяется с рамкой каждой четвёрки. Где dl_row пропускает огонь
; для всей четвёрки, здесь её дорожки считаются под маской: к свету прибавляется +0,
; видимость множится на 1 — то же, что пропуск. Сборы из таблиц — vpgatherdd.
; ymm8 = 1/(1 + обёртка), ymm9 = обёртка, ymm10–12 = нормаль, ymm13–15 = свет B, G, R
DX_GX       equ 0                       ; x, y (8.8), z (px) — f32
DX_GY       equ 32
DX_GZ       equ 64
DX_ZI       equ 96                      ; z, целые
DX_VAL      equ 128                     ; дорожка со светом
DX_OWN      equ 160                     ; крыша (свой свет)
DX_HID      equ 192                     ; изба под пикселем + 1
DX_MSD      equ 224                     ; сдвиг выборки тени к огню: msd·256
DX_BNC      equ 256                     ; отсвет снега
DX_DX       equ 288                     ; до огня, px-единицы
DX_DY       equ 320
DX_DZ       equ 352
DX_ID       equ 384                     ; 1/d
DX_F        equ 416                     ; сила огня до тени
DX_FP       equ 448                     ; окно: сила пятна до тени
DX_M        equ 480                     ; дорожки, куда огонь достаёт
DX_SIDX     equ 512                     ; сэмпл сетки
DX_WX       equ 544                     ; веса билинейки по нему словами: 64 − f | f << 16
DX_WY       equ 576
DX_NID      equ 608                     ; нормаль
DX_T        equ 640                     ; временное (окно: t)
DX_A        equ 672
DX_B        equ 704
DX_TMP      equ 736
DX_BB       equ 768                     ; рамки четвёрок: x0, x1, y0, y1 — первой, второй
DX_BBOK     equ 800
DX_ROWZ     equ 804
DX_CAM      equ 808
DX_GOX      equ 812
DX_GOY      equ 816
DX_RSP      equ 824                     ; rsp до выравнивания
DX_TM       equ 832                     ; маски плиток строки (адрес)
DX_TF       equ 840                     ; флаги плиток строки (адрес)
DX_TP       equ 848                     ; пятна окон по плиткам строки (адрес)
DX_TH       equ 856                     ; тени огней по плиткам строки (адрес)
DX_F0       equ 864                     ; тень фонарика: r (слоты), доли, углы
DX_F1       equ 896
DX_F2       equ 928
DX_F3       equ 960
DX_F4       equ 992
DX_F5       equ 1024
DX_F6       equ 1056
DX_PX       equ 1088                    ; пиксель, px-единицы (x, y)
DX_PY       equ 1120
DX_AA       equ 1152                    ; тени фигур: |D|², 1/|D|², размер огня
DX_IA       equ 1184
DX_SZL      equ 1216
DX_CB       equ 1248                    ; D·E
DX_CF       equ 1280                    ; D·r0
DX_NO       equ 1312                    ; не своя капсула
DX_OM       equ 1344                    ; наибольшая доля тени
DX_CM       equ 1376                    ; дорожки огня (и четвёрок, чью рамку тень задевает)
DX_CM2      equ 1408                    ; дорожки фигуры
DX_TG       equ 1440                    ; тени зомби от фонарика по плиткам строки (адрес)
DX_TMF      equ 1448                    ; фигуры под луной по плиткам строки (адрес)
DX_WM       equ 1456                    ; отсев капсул: запас на полутень огня
DX_CMB      equ 1472                    ; тени фигур: дорожки, где тень что-то решает
DX_SZ       equ 1504

; 16 байт на пиксель из [rsi + (%1 − albedo) + nid·16]: пиксели 0..3 — в нижние
; половины ymm4..7, 4..7 — в верхние (для TR4Y)
%macro LD1 3                            ; таблица, ymm%2, пиксель %3 (и %3 + 4)
    mov     eax, [rbp + DX_NID + 4*%3]
    shl     eax, 4
    vmovups xmm%2, [rsi + rax + (%1 - albedo)]
    mov     eax, [rbp + DX_NID + 16 + 4*%3]
    shl     eax, 4
    vinsertf128 ymm%2, ymm%2, [rsi + rax + (%1 - albedo)], 1
%endmacro
%macro LD8 1
    LD1     %1, 4, 0
    LD1     %1, 5, 1
    LD1     %1, 6, 2
    LD1     %1, 7, 3
%endmacro

; TR4 из lights.inc по половинам: ymm4..7 -> ymm2 = 0-е, ymm1 = 1-е, ymm3 = 2-е,
; ymm6 = 3-е (портит ymm0, ymm4)
%macro TR4Y 0
    vunpcklps ymm0, ymm4, ymm5          ; a0 b0 a1 b1
    vunpcklps ymm1, ymm6, ymm7          ; c0 d0 c1 d1
    vunpckhps ymm4, ymm4, ymm5          ; a2 b2 a3 b3
    vunpckhps ymm6, ymm6, ymm7          ; c2 d2 c3 d3
    vunpcklpd ymm2, ymm0, ymm1          ; a0 b0 c0 d0
    vunpckhpd ymm1, ymm0, ymm1          ; a1 b1 c1 d1
    vunpcklpd ymm3, ymm4, ymm6          ; a2 b2 c2 d2
    vunpckhpd ymm6, ymm4, ymm6          ; a3 b3 c3 d3
%endmacro

; HZT из lights.inc: угол %1, ymm4 = (z + 64) << 16 (портит ymm0, ymm7)
%macro HZT8 1
    vpslld  ymm0, %1, 16
    vpsubd  ymm7, ymm4, ymm0
    vpmaddwd %1, ymm7, %1
    vpsrad  %1, %1, 8
    vpaddd  %1, %1, [lt_d128]
%endmacro

; BNC1 из lights.inc: канал отсвета (байт со сдвигом %1) из углов ymm2 = c00,
; ymm1 = c10, ymm3 = c01, ymm6 = c11 -> ymm0 (портит ymm4, ymm5)
%macro BNC8 1
%if %1
    vpsrld  ymm0, ymm2, %1
    vpand   ymm0, ymm0, ymm11
%else
    vpand   ymm0, ymm2, ymm11
%endif
%if %1 < 16
    vpslld  ymm4, ymm1, 16 - %1
    vpand   ymm4, ymm4, ymm12
%else
    vpand   ymm4, ymm1, ymm12
%endif
    vpor    ymm0, ymm0, ymm4
    vpmaddwd ymm0, ymm0, ymm9
    vpsrld  ymm0, ymm0, 6               ; ряд 0
%if %1
    vpsrld  ymm5, ymm3, %1
    vpand   ymm5, ymm5, ymm11
%else
    vpand   ymm5, ymm3, ymm11
%endif
%if %1 < 16
    vpslld  ymm4, ymm6, 16 - %1
    vpand   ymm4, ymm4, ymm12
%else
    vpand   ymm4, ymm6, ymm12
%endif
    vpor    ymm5, ymm5, ymm4
    vpmaddwd ymm5, ymm5, ymm9
    vpslld  ymm5, ymm5, 10
    vpand   ymm5, ymm5, ymm12           ; (ряд 1 >> 6) << 16
    vpor    ymm0, ymm0, ymm5
    vpmaddwd ymm0, ymm0, ymm10
    vpsrld  ymm0, ymm0, 6
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, ymm8
%endmacro

; ecx = строка -> dlb (как dl_row); сохраняет rbx, rsi, rdi, rbp, r12–r15, ymm портит все
dl_row8:
    push    rbx
    push    rsi
    push    rdi
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    mov     rax, rsp
    sub     rsp, DX_SZ
    and     rsp, -32
    mov     rbp, rsp
    mov     [rbp + DX_RSP], rax
    lea     rsi, [albedo]
    mov     eax, CY0
    sub     eax, ecx
    mov     [rbp + DX_ROWZ], eax
    mov     eax, ecx
    shr     eax, 4
    imul    edx, eax, DT_W
    lea     r8, [dl_tf]
    add     r8, rdx
    mov     [rbp + DX_TF], r8
    lea     r8, [dl_tm]
    lea     r8, [r8 + rdx*8]
    mov     [rbp + DX_TM], r8
    lea     r8, [dl_tp]
    lea     r8, [r8 + rdx*8]
    mov     [rbp + DX_TP], r8
    lea     r8, [dl_th]
    lea     r8, [r8 + rdx*8]
    mov     [rbp + DX_TH], r8
    lea     r8, [dl_tfg]
    lea     r8, [r8 + rdx*2]
    mov     [rbp + DX_TG], r8
    lea     r8, [dl_tmf]
    lea     r8, [r8 + rdx*2]
    mov     [rbp + DX_TMF], r8
    mov     eax, [sc_wmax]
    mov     [rbp + DX_WM], eax
    mov     eax, [camx]
    add     eax, [camy]
    mov     [rbp + DX_CAM], eax
    mov     eax, [gox]
    shl     eax, 6
    mov     [rbp + DX_GOX], eax
    mov     eax, [goy]
    shl     eax, 6
    mov     [rbp + DX_GOY], eax
    imul    r12d, ecx, SCR_W            ; пиксель
    xor     r13d, r13d                  ; x
.grp:
    ; плитка без огней и без фонарика — темно (все 8 — в одной плитке)
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DX_TM]
    cmp     qword [rdx + rax*8], 0
    jne     .g0
    mov     rdx, [rbp + DX_TF]
    cmp     byte [rdx + rax], 0
    je      .gzero
.g0:
    vmovdqu ymm0, [rsi + r12*4]
    vpxor   ymm1, ymm1, ymm1
    vpcmpgtd ymm0, ymm0, ymm1           ; есть свет: не пустота, не эмиссия
    vmovmskps eax, ymm0
    test    eax, eax
    jz      .gzero
    vmovdqa [rbp + DX_VAL], ymm0
    ; ----- мир: x, y (8.8), z (px)
    vmovdqu ymm1, [rsi + r12*4 + (gposb - albedo)]
    vpslld  ymm2, ymm1, 16
    vpsrld  ymm2, ymm2, 16              ; x
    vpsrld  ymm1, ymm1, 16              ; y
    vpaddd  ymm3, ymm2, ymm1
    vpbroadcastd ymm4, [rbp + DX_CAM]
    vpsubd  ymm3, ymm3, ymm4
    vpsrad  ymm3, ymm3, 5
    vpbroadcastd ymm4, [rbp + DX_ROWZ]
    vpaddd  ymm3, ymm3, ymm4            ; z
    vmovdqa [rbp + DX_ZI], ymm3
    vcvtdq2ps ymm4, ymm2
    vmovaps [rbp + DX_GX], ymm4
    vmulps  ymm4, ymm4, [lt_pxu4]
    vmovaps [rbp + DX_PX], ymm4
    vcvtdq2ps ymm4, ymm1
    vmovaps [rbp + DX_GY], ymm4
    vmulps  ymm4, ymm4, [lt_pxu4]
    vmovaps [rbp + DX_PY], ymm4
    vcvtdq2ps ymm4, ymm3
    vmovaps [rbp + DX_GZ], ymm4
    mov     dword [rbp + DX_BBOK], 0
    ; плитка только с тенями фигур под луной — их и всё (огней нет: dlb — 0)
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DX_TF]
    movzx   ecx, byte [rdx + rax]
    test    ecx, 1
    jnz     .g1
    mov     rdx, [rbp + DX_TM]
    cmp     qword [rdx + rax*8], 0
    jne     .g1
    call    .mf
    jmp     .gz1
.g1:
    ; изба под пикселем: rmap[(y >> 8)·128 + (x >> 8)]
    vpsrld  ymm4, ymm1, 8
    vpslld  ymm4, ymm4, 7
    vpsrld  ymm5, ymm2, 8
    vpaddd  ymm4, ymm4, ymm5
    vpcmpeqd ymm5, ymm5, ymm5
    vpxor   ymm6, ymm6, ymm6
    vpgatherdd ymm6, [rsi + ymm4*1 + (rmap - albedo)], ymm5
    vpand   ymm6, ymm6, [lt_dff]
    vmovdqa [rbp + DX_HID], ymm6
    ; сэмпл сетки света — как в light_rows: от окна сетки, прижато к [0, GLIM]
    vpbroadcastd ymm4, [rbp + DX_GOX]
    vpsubd  ymm2, ymm2, ymm4
    vpbroadcastd ymm4, [rbp + DX_GOY]
    vpsubd  ymm1, ymm1, ymm4
    vpxor   ymm7, ymm7, ymm7
    vpmaxsd ymm5, ymm2, ymm7            ; x ≥ 0
    vpminsd ymm5, ymm5, [lt_dglim]      ; x ≤ GLIM
    vpmaxsd ymm6, ymm1, ymm7
    vpminsd ymm6, ymm6, [lt_dglim]      ; y
    vpand   ymm4, ymm5, [lt_d63]        ; fx
    vpslld  ymm7, ymm4, 16
    vpsubd  ymm7, ymm7, ymm4
    vpaddd  ymm7, ymm7, [lt_d64]
    vmovdqa [rbp + DX_WX], ymm7
    vpand   ymm4, ymm6, [lt_d63]        ; fy
    vpslld  ymm7, ymm4, 16
    vpsubd  ymm7, ymm7, ymm4
    vpaddd  ymm7, ymm7, [lt_d64]
    vmovdqa [rbp + DX_WY], ymm7
    vpsrld  ymm5, ymm5, 6
    vpsrld  ymm6, ymm6, 6
    vpmaddwd ymm6, ymm6, [lt_dgw]
    vpaddd  ymm5, ymm5, ymm6
    vmovdqa [rbp + DX_SIDX], ymm5
    ; ----- нормаль, её обёртка, отсвет, сдвиг выборки тени — по индексу нормали
    vpmovzxbd ymm4, [rsi + r12 + (nbuf - albedo)]
    vpand   ymm5, ymm4, [lt_d80]
    vpcmpeqd ymm5, ymm5, [lt_d80]
    vmovdqa [rbp + DX_OWN], ymm5
    vpand   ymm4, ymm4, [lt_d3f]
    vmovdqa [rbp + DX_NID], ymm4
    LD8     nvec
    TR4Y
    vmovaps ymm10, ymm2                 ; x
    vmovaps ymm11, ymm1                 ; y
    vmovaps ymm12, ymm3                 ; z
    LD8     dl_nw
    TR4Y
    vmovaps ymm9, ymm2                  ; обёртка
    vmovaps ymm8, ymm1                  ; 1 / (1 + обёртка)
    vmovaps [rbp + DX_BNC], ymm3
    vmovaps [rbp + DX_MSD], ymm6
    ; ----- тени фигур под луной (ymm8–12 — нормаль — не трогает)
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DX_TF]
    test    byte [rdx + rax], 4
    jz      .g2
    call    .mf
    jmp     .g3
.g2:
    mov     qword [rsi + r12 + (mfsh - albedo)], -1
.g3:
    vxorps  ymm13, ymm13, ymm13
    vxorps  ymm14, ymm14, ymm14
    vxorps  ymm15, ymm15, ymm15
    ; ----- огни плитки
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DX_TM]
    mov     r15, [rdx + rax*8]
.al:
    test    r15, r15
    jz      .fl
    bsf     rax, r15
    lea     rcx, [r15 - 1]
    and     r15, rcx
    mov     r14d, eax                   ; номер огня (пятна окон, тени по плиткам)
    imul    eax, eax, AL_SZ
    lea     rdi, [al_buf]
    add     rdi, rax
    vmovaps ymm0, [rdi + AL_XV]
    vsubps  ymm0, ymm0, [rbp + DX_GX]
    vmulps  ymm0, ymm0, [lt_pxu4]       ; до огня, px-единицы
    vmovaps ymm1, [rdi + AL_YV]
    vsubps  ymm1, ymm1, [rbp + DX_GY]
    vmulps  ymm1, ymm1, [lt_pxu4]
    vmovaps ymm2, [rdi + AL_ZV]
    vsubps  ymm2, ymm2, [rbp + DX_GZ]
    vmulps  ymm3, ymm0, ymm0
    vmulps  ymm4, ymm1, ymm1
    vaddps  ymm3, ymm3, ymm4
    vmulps  ymm4, ymm2, ymm2
    vaddps  ymm3, ymm3, ymm4            ; d²
    vmulps  ymm5, ymm3, [rdi + AL_IR2V]
    vcmpltps ymm5, ymm5, [lt_one4]      ; достаёт
    vandps  ymm5, ymm5, [rbp + DX_VAL]
    cmp     dword [rdi + AL_HID], 0
    je      .a1
    vmovdqa ymm4, [rbp + DX_HID]
    vpcmpeqd ymm4, ymm4, [rdi + AL_HIDV]
    vandps  ymm5, ymm5, ymm4            ; огонь в избе — только её
.a1:
    vmovmskps eax, ymm5
    test    eax, eax
    jz      .al
    vmovaps [rbp + DX_M], ymm5
    vmovaps [rbp + DX_DX], ymm0
    vmovaps [rbp + DX_DY], ymm1
    vmovaps [rbp + DX_DZ], ymm2
    ; затухание (1 − d²/R²)²
    vmulps  ymm4, ymm3, [rdi + AL_IRAV]
    vmovaps ymm6, [lt_one4]
    vsubps  ymm6, ymm6, ymm4
    vmaxps  ymm6, ymm6, [lt_zero4]
    vmulps  ymm6, ymm6, ymm6
    vandps  ymm6, ymm6, ymm5
    vmaxps  ymm3, ymm3, [lt_one4]
    vrsqrtps ymm7, ymm3
    vmovaps [rbp + DX_ID], ymm7         ; 1/d
    ; N·L с обёрткой
    vmulps  ymm4, ymm0, ymm10
    vmulps  ymm3, ymm1, ymm11
    vaddps  ymm4, ymm4, ymm3
    vmulps  ymm3, ymm2, ymm12
    vaddps  ymm4, ymm4, ymm3
    vmulps  ymm4, ymm4, ymm7
    vaddps  ymm4, ymm4, ymm9
    vmulps  ymm4, ymm4, ymm8
    vmaxps  ymm4, ymm4, [lt_zero4]
    vmulps  ymm6, ymm6, ymm4
    ; лепесток огня
    vmulps  ymm0, ymm0, [rdi + AL_LDX]
    vmulps  ymm1, ymm1, [rdi + AL_LDY]
    vaddps  ymm0, ymm0, ymm1
    vmulps  ymm2, ymm2, [rdi + AL_LDZ]
    vaddps  ymm0, ymm0, ymm2
    vmulps  ymm0, ymm0, ymm7
    vmulps  ymm0, ymm0, [rdi + AL_LK]
    vaddps  ymm0, ymm0, [rdi + AL_LB]
    vmaxps  ymm0, ymm0, [lt_zero4]
    vminps  ymm0, ymm0, [lt_one4]
    vmulps  ymm6, ymm6, ymm0
    vmovaps [rbp + DX_F], ymm6
    vxorps  ymm0, ymm0, ymm0
    vmovaps [rbp + DX_FP], ymm0
    cmp     dword [rdi + AL_KIND], LK_WIN
    jne     .a2
    call    .win                        ; свечение — только снаружи; пятно сквозь раму
.a2:
    vmovaps ymm0, [rbp + DX_F]
    vorps   ymm0, ymm0, [rbp + DX_FP]
    vpxor   ymm1, ymm1, ymm1
    vpcmpeqd ymm0, ymm0, ymm1
    vmovmskps eax, ymm0
    cmp     eax, 255
    je      .al                         ; ничего не дошло
    vmovaps ymm5, [lt_one4]             ; видимость огня
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DX_TH]
    mov     rdx, [rdx + rax*8]
    bt      rdx, r14
    jnc     .a3                         ; в плитке огню ничто не заслоняет (или карты нет)
    call    .hz
.a3:
    cmp     dword [rdi + AL_NF], 0
    je      .a4
    call    .figs
.a4:
    vmovaps ymm4, [rbp + DX_F]
    vmulps  ymm4, ymm4, ymm5
    vmulps  ymm0, ymm4, [rdi + AL_CB]
    vaddps  ymm13, ymm13, ymm0
    vmulps  ymm0, ymm4, [rdi + AL_CG]
    vaddps  ymm14, ymm14, ymm0
    vmulps  ymm4, ymm4, [rdi + AL_CR]
    vaddps  ymm15, ymm15, ymm4
    cmp     dword [rdi + AL_KIND], LK_WIN
    jne     .al
    vmovaps ymm4, [rbp + DX_FP]
    vmulps  ymm4, ymm4, ymm5
    vbroadcastss ymm0, [rdi + AL_PCOL]
    vmulps  ymm0, ymm0, ymm4
    vaddps  ymm13, ymm13, ymm0
    vbroadcastss ymm0, [rdi + AL_PCOL + 4]
    vmulps  ymm0, ymm0, ymm4
    vaddps  ymm14, ymm14, ymm0
    vbroadcastss ymm0, [rdi + AL_PCOL + 8]
    vmulps  ymm0, ymm0, ymm4
    vaddps  ymm15, ymm15, ymm0
    jmp     .al

    ; ----- фонарик: прожектор из руки
.fl:
    cmp     dword [dl_fon], 0
    je      .bn
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DX_TF]
    test    byte [rdx + rax], 1
    jz      .bn                         ; до плитки луч не достаёт
    vbroadcastss ymm0, [dl_sx]
    vmovaps ymm1, [rbp + DX_GX]
    vsubps  ymm1, ymm1, ymm0
    vmulps  ymm1, ymm1, [lt_pxu4]       ; от руки
    vbroadcastss ymm0, [dl_sy]
    vmovaps ymm2, [rbp + DX_GY]
    vsubps  ymm2, ymm2, ymm0
    vmulps  ymm2, ymm2, [lt_pxu4]
    vmovaps ymm3, [rbp + DX_GZ]
    vsubps  ymm3, ymm3, [lt_handz4]
    vmulps  ymm4, ymm1, ymm1
    vmulps  ymm5, ymm2, ymm2
    vaddps  ymm4, ymm4, ymm5
    vmulps  ymm5, ymm3, ymm3
    vaddps  ymm4, ymm4, ymm5            ; d²
    vbroadcastss ymm5, [dl_fr2]
    vcmpltps ymm5, ymm5, ymm4
    vandnps ymm5, ymm5, [rbp + DX_VAL]  ; в пределах луча
    vmaxps  ymm4, ymm4, [lt_one4]
    vmovaps [rbp + DX_A], ymm4          ; d²
    vrsqrtps ymm6, ymm4                 ; 1/d
    vmovaps [rbp + DX_ID], ymm6
    vbroadcastss ymm0, [dl_ax]
    vmulps  ymm0, ymm0, ymm1
    vbroadcastss ymm7, [dl_ay]
    vmulps  ymm7, ymm7, ymm2
    vaddps  ymm0, ymm0, ymm7
    vbroadcastss ymm7, [dl_az]
    vmulps  ymm7, ymm7, ymm3
    vaddps  ymm0, ymm0, ymm7
    vmulps  ymm0, ymm0, ymm6            ; c — косинус от оси луча
    vmovaps ymm7, [lt_0_5v]
    vcmpltps ymm7, ymm7, ymm0
    vandps  ymm5, ymm5, ymm7
    vmovmskps eax, ymm5
    test    eax, eax
    jz      .bn
    vmovaps [rbp + DX_M], ymm5
    ; N·L: свет от руки; вскользь по снегу — обёртка шире
    vmulps  ymm1, ymm1, ymm10
    vmulps  ymm2, ymm2, ymm11
    vaddps  ymm1, ymm1, ymm2
    vmulps  ymm3, ymm3, ymm12
    vaddps  ymm1, ymm1, ymm3
    vmulps  ymm1, ymm1, ymm6
    vmovaps ymm2, [lt_fwrap4]
    vsubps  ymm2, ymm2, ymm1
    vmulps  ymm2, ymm2, [lt_fwk4]
    vmaxps  ymm2, ymm2, [lt_zero4]
    vandps  ymm2, ymm2, ymm5
    vmovaps [rbp + DX_B], ymm2          ; N·L
    ; профиль луча: пятно, горячая середина, тёмное кольцо рефлектора, ореол
    vsubps  ymm1, ymm0, [lt_0_86v]
    vmulps  ymm1, ymm1, [lt_14v]
    vmaxps  ymm1, ymm1, [lt_zero4]
    vminps  ymm1, ymm1, [lt_one4]
    vsubps  ymm2, ymm0, [lt_0_965v]
    vmulps  ymm2, ymm2, [lt_40v]
    vmaxps  ymm2, ymm2, [lt_zero4]
    vminps  ymm2, ymm2, [lt_one4]
    vmulps  ymm2, ymm2, [lt_0_35v]
    vaddps  ymm1, ymm1, ymm2
    vsubps  ymm2, ymm0, [lt_0_915v]
    vandps  ymm2, ymm2, [lt_absm]
    vmulps  ymm2, ymm2, [lt_50v]
    vmovaps ymm3, [lt_one4]
    vsubps  ymm3, ymm3, ymm2
    vmaxps  ymm3, ymm3, [lt_zero4]
    vmulps  ymm3, ymm3, [lt_0_12v]
    vsubps  ymm1, ymm1, ymm3
    vmaxps  ymm1, ymm1, [lt_zero4]
    vsubps  ymm0, ymm0, [lt_0_5v]
    vmulps  ymm0, ymm0, [lt_3v]
    vmaxps  ymm0, ymm0, [lt_zero4]
    vminps  ymm0, ymm0, [lt_one4]
    vmulps  ymm0, ymm0, [lt_0_07v]
    vaddps  ymm1, ymm1, ymm0            ; профиль
    ; затухание: 1/(1 + 0.022·d²) (тайлы), к краю дальности — на нет
    vmovaps ymm0, [rbp + DX_A]
    vmovaps ymm2, ymm0
    vmulps  ymm0, ymm0, [lt_fk4]
    vaddps  ymm0, ymm0, [lt_one4]
    vmovss  xmm3, [flash_i]
    vmulss  xmm3, xmm3, [lt_fmul]
    vbroadcastss ymm3, xmm3
    vdivps  ymm3, ymm3, ymm0
    vmulps  ymm2, ymm2, [rbp + DX_ID]   ; d
    vmulps  ymm2, ymm2, [lt_i196v]
    vbroadcastss ymm0, [fl_reach]
    vsubps  ymm0, ymm0, ymm2
    vmulps  ymm0, ymm0, [lt_0_25v]
    vmaxps  ymm0, ymm0, [lt_zero4]
    vminps  ymm0, ymm0, [lt_one4]
    vmulps  ymm3, ymm3, ymm0
    vmulps  ymm1, ymm1, ymm3
    vmulps  ymm1, ymm1, [rbp + DX_B]    ; · N·L (0 вне луча)
    vmovaps [rbp + DX_F], ymm1
    ; тень: лучи от руки по карте высот (fl_buf), если в плитке есть что заслонять;
    ; крыше — её видимость (lbuf)
    vmovaps ymm2, [lt_one4]
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DX_TF]
    test    byte [rdx + rax], 2
    jz      .fl1
    call    .fls
.fl1:
    ; зомби в луче, чьи тени задевают плитку, — капсулами от руки
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DX_TG]
    movzx   r11d, word [rdx + rax*2]
    test    r11d, r11d
    jz      .fl2
    ; дорожки, где тень что-то решает: луч дошёл, не крыша, рельеф не затенил
    vmovaps ymm0, [rbp + DX_F]
    vpxor   ymm1, ymm1, ymm1
    vpcmpeqd ymm0, ymm0, ymm1
    vcmpltps ymm1, ymm1, ymm2
    vandnps ymm0, ymm0, ymm1
    vandps  ymm0, ymm0, [rbp + DX_M]
    vmovaps ymm1, [rbp + DX_OWN]
    vandnps ymm1, ymm1, ymm0
    vmovaps [rbp + DX_CMB], ymm1
    vmovmskps eax, ymm1
    test    eax, eax
    jz      .fl2
    vmovaps ymm5, ymm2
    call    .gbb
    vbroadcastss ymm0, [dl_sx]
    vsubps  ymm0, ymm0, [rbp + DX_GX]
    vmulps  ymm0, ymm0, [lt_pxu4]
    vmovaps [rbp + DX_DX], ymm0         ; D = рука − пиксель, px-единицы
    vbroadcastss ymm1, [dl_sy]
    vsubps  ymm1, ymm1, [rbp + DX_GY]
    vmulps  ymm1, ymm1, [lt_pxu4]
    vmovaps [rbp + DX_DY], ymm1
    vmovaps ymm2, [lt_handz4]
    vsubps  ymm2, ymm2, [rbp + DX_GZ]
    vmovaps [rbp + DX_DZ], ymm2
    vmulps  ymm0, ymm0, ymm0
    vmulps  ymm1, ymm1, ymm1
    vaddps  ymm0, ymm0, ymm1
    vmulps  ymm2, ymm2, ymm2
    vaddps  ymm0, ymm0, ymm2
    vmaxps  ymm0, ymm0, [lt_one4]
    vmovaps [rbp + DX_AA], ymm0
    vmovaps ymm1, [lt_one4]
    vdivps  ymm1, ymm1, ymm0
    vmovaps [rbp + DX_IA], ymm1
    vmovaps ymm0, [fl_szlv]
    vmovaps [rbp + DX_SZL], ymm0
.fl3:
    bsf     eax, r11d
    btr     r11d, eax
    imul    eax, eax, AL_FSZ
    lea     r9, [fl_fgl]
    add     r9, rax
    mov     r10d, 1
    call    .figl
    test    r11d, r11d
    jnz     .fl3
    vmovaps ymm2, ymm5
.fl2:
    vmovdqu ymm0, [rsi + r12*4 + (lbuf - albedo)]
    vpsrld  ymm0, ymm0, 24
    vcvtdq2ps ymm0, ymm0
    vmulps  ymm0, ymm0, [lt_i255v]
    vmovaps ymm1, [rbp + DX_OWN]
    vandps  ymm0, ymm0, ymm1
    vandnps ymm1, ymm1, ymm2
    vorps   ymm0, ymm0, ymm1
    vandps  ymm0, ymm0, [rbp + DX_M]
    vmulps  ymm0, ymm0, [rbp + DX_F]
    vmulps  ymm1, ymm0, [lt_flb]
    vaddps  ymm13, ymm13, ymm1
    vmulps  ymm1, ymm0, [lt_flg]
    vaddps  ymm14, ymm14, ymm1
    vmulps  ymm0, ymm0, [lt_flr]
    vaddps  ymm15, ymm15, ymm0

    ; ----- отсвет снега снизу: свет огней на снегу вокруг (lgst), по грани и высоте
.bn:
    vmovaps ymm0, [rbp + DX_GZ]
    vmulps  ymm0, ymm0, [lt_i72v]
    vmovaps ymm1, [lt_one4]
    vsubps  ymm1, ymm1, ymm0
    vmaxps  ymm1, ymm1, [lt_zero4]
    vmulps  ymm1, ymm1, [rbp + DX_BNC]
    vandps  ymm1, ymm1, [rbp + DX_VAL]
    vmovaps [rbp + DX_A], ymm1
    vxorps  ymm0, ymm0, ymm0
    vcmpltps ymm0, ymm0, ymm1
    vmovmskps eax, ymm0
    test    eax, eax
    jz      .gl
    vmovaps ymm8, ymm1                  ; доля отсвета (0 — дорожка не берёт)
    ; углы lgst сборами: ymm2 = c00, ymm1 = c10, ymm3 = c01, ymm6 = c11
    vmovdqa ymm7, [rbp + DX_SIDX]
    vpcmpeqd ymm5, ymm5, ymm5
    vpxor   ymm2, ymm2, ymm2
    vpgatherdd ymm2, [rsi + ymm7*4 + (lgst - albedo)], ymm5
    vpcmpeqd ymm5, ymm5, ymm5
    vpxor   ymm1, ymm1, ymm1
    vpgatherdd ymm1, [rsi + ymm7*4 + (lgst - albedo) + 4], ymm5
    vpcmpeqd ymm5, ymm5, ymm5
    vpxor   ymm3, ymm3, ymm3
    vpgatherdd ymm3, [rsi + ymm7*4 + (lgst - albedo) + GW*4], ymm5
    vpcmpeqd ymm5, ymm5, ymm5
    vpxor   ymm6, ymm6, ymm6
    vpgatherdd ymm6, [rsi + ymm7*4 + (lgst - albedo) + GW*4 + 4], ymm5
    vmovdqa ymm9, [rbp + DX_WX]
    vmovdqa ymm10, [rbp + DX_WY]
    vmovdqa ymm11, [lt_dff]
    vmovdqa ymm12, [lt_dff0000]
    BNC8    0
    vaddps  ymm13, ymm13, ymm0
    BNC8    8
    vaddps  ymm14, ymm14, ymm0
    BNC8    16
    vaddps  ymm15, ymm15, ymm0

    ; ----- искры: в свете огней и фонарика снег блестит гуще — кристаллы ловят луч
.gl:
    vaddps  ymm0, ymm13, ymm15
    vxorps  ymm1, ymm1, ymm1
    vcmpltps ymm1, ymm1, ymm0
    vmovmskps eax, ymm1                 ; дорожки со светом огней
    test    eax, eax
    jz      .st
    ; снег: нормаль 0 или N_DY..N_SM − 1, и светлый (R ≥ 200)
    vmovdqa ymm2, [rbp + DX_NID]
    vpxor   ymm3, ymm3, ymm3
    vpcmpeqd ymm3, ymm3, ymm2
    vpcmpgtd ymm4, ymm2, [lt_dndy1]
    vmovdqa ymm5, [lt_dnsm]
    vpcmpgtd ymm5, ymm5, ymm2
    vpand   ymm4, ymm4, ymm5
    vpor    ymm3, ymm3, ymm4
    vpand   ymm1, ymm1, ymm3
    vmovdqu ymm2, [rsi + r12*4]
    vpsrld  ymm2, ymm2, 16
    vpand   ymm2, ymm2, [lt_dff]
    vpcmpgtd ymm2, ymm2, [lt_d199]
    vpand   ymm1, ymm1, ymm2
    vmovmskps eax, ymm1
    test    eax, eax
    jz      .st
    ; кристалл: HASH((x >> 3) ^ glint, y >> 3) & 255 < 5 (vpmulld — младшие 32 бита, как imul)
    vcvttps2dq ymm2, [rbp + DX_GX]
    vpsrld  ymm2, ymm2, 3
    vpbroadcastd ymm0, [glint]
    vpxor   ymm2, ymm2, ymm0
    vcvttps2dq ymm3, [rbp + DX_GY]
    vpsrld  ymm3, ymm3, 3
    vpmulld ymm2, ymm2, [lt_hx]
    vpmulld ymm3, ymm3, [lt_hy]
    vpxor   ymm2, ymm2, ymm3
    vpsrld  ymm3, ymm2, 15
    vpxor   ymm2, ymm2, ymm3
    vpmulld ymm2, ymm2, [lt_hm]
    vpsrld  ymm3, ymm2, 13
    vpxor   ymm2, ymm2, ymm3
    vpand   ymm2, ymm2, [lt_dff]
    vmovdqa ymm3, [lt_d5]
    vpcmpgtd ymm3, ymm3, ymm2
    vpand   ymm1, ymm1, ymm3
    vmovmskps eax, ymm1
    test    eax, eax
    jz      .st
    ; искра — свет ×2.1
    vandps  ymm0, ymm1, [lt_2_1v]
    vandnps ymm1, ymm1, [lt_one4]
    vorps   ymm0, ymm0, ymm1
    vmulps  ymm13, ymm13, ymm0
    vmulps  ymm14, ymm14, ymm0
    vmulps  ymm15, ymm15, ymm0
.st:
    ; B, G, R по дорожкам -> 8 пикселей по 4 слова (половины — как у dl_row)
    vcvtps2dq ymm13, ymm13
    vcvtps2dq ymm14, ymm14
    vcvtps2dq ymm15, ymm15
    vpackssdw ymm13, ymm13, ymm13
    vpackssdw ymm14, ymm14, ymm14
    vpackssdw ymm15, ymm15, ymm15
    vpunpcklwd ymm13, ymm13, ymm14      ; B0 G0 B1 G1 B2 G2 B3 G3 | то же 4..7
    vpxor   ymm0, ymm0, ymm0
    vpunpcklwd ymm15, ymm15, ymm0       ; R0 0 R1 0 ...
    vpunpckhdq ymm1, ymm13, ymm15       ; пиксели 2, 3 | 6, 7
    vpunpckldq ymm13, ymm13, ymm15      ; 0, 1 | 4, 5
    vperm2i128 ymm0, ymm13, ymm1, 0x20  ; 0..3
    vperm2i128 ymm1, ymm13, ymm1, 0x31  ; 4..7
    vmovdqu [rsi + r12*8 + (dlb - albedo)], ymm0
    vmovdqu [rsi + r12*8 + (dlb - albedo) + 32], ymm1
    jmp     .next
.gzero:
    mov     qword [rsi + r12 + (mfsh - albedo)], -1
.gz1:
    vpxor   ymm0, ymm0, ymm0
    vmovdqu [rsi + r12*8 + (dlb - albedo)], ymm0
    vmovdqu [rsi + r12*8 + (dlb - albedo) + 32], ymm0
.next:
    add     r12d, 8
    add     r13d, 8
    cmp     r13d, SCR_W
    jb      .grp
    mov     rsp, [rbp + DX_RSP]
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rbx
    ret

    ; ----- окно (rdi): свечение стекла — только снаружи стены; пятно сквозь раму
.win:
    vmovaps ymm0, [rbp + DX_GY]         ; поперёк стены
    vmovaps ymm1, [rbp + DX_GX]         ; вдоль
    vbroadcastss ymm2, [rdi + AL_BY]
    vbroadcastss ymm3, [rdi + AL_BX]
    cmp     dword [rdi + AL_WAX], 0
    je      .w1
    vmovaps ymm0, [rbp + DX_GX]
    vmovaps ymm1, [rbp + DX_GY]
    vbroadcastss ymm2, [rdi + AL_BX]
    vbroadcastss ymm3, [rdi + AL_BY]
.w1:
    vbroadcastss ymm4, [rdi + AL_WPC]
    vsubps  ymm5, ymm0, ymm4
    vbroadcastss ymm6, [rdi + AL_WSG]
    vmulps  ymm5, ymm5, ymm6
    vxorps  ymm6, ymm6, ymm6
    vcmpltps ymm6, ymm6, ymm5           ; снаружи
    vpbroadcastd ymm7, [rdi + AL_WHID]
    vpcmpeqd ymm7, ymm7, [rbp + DX_HID]
    vpandn  ymm7, ymm7, ymm6            ; и не в своей избе
    vandps  ymm7, ymm7, [rbp + DX_M]
    vmovaps ymm6, [rbp + DX_F]
    vandps  ymm6, ymm6, ymm7
    vmovaps [rbp + DX_F], ymm6
    vmovmskps eax, ymm7
    test    eax, eax
    jz      .wr
    ; пятно в эту плитку не ложится (клин от лампочки сквозь стекло — мимо)
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DX_TP]
    mov     rdx, [rdx + rax*8]
    bt      rdx, r14
    jnc     .wr
    ; t = (WPC − Bn) / (n − Bn); W = B + t·(P − B)
    vsubps  ymm5, ymm4, ymm2
    vsubps  ymm6, ymm0, ymm2
    vdivps  ymm5, ymm5, ymm6            ; t
    vandps  ymm5, ymm5, ymm7            ; вне — 0 (не NaN)
    vmovaps [rbp + DX_T], ymm5
    vsubps  ymm6, ymm1, ymm3
    vmulps  ymm6, ymm6, ymm5
    vaddps  ymm6, ymm6, ymm3
    vbroadcastss ymm0, [rdi + AL_WU0]
    vsubps  ymm6, ymm6, ymm0
    vbroadcastss ymm0, [rdi + AL_WUD]
    vmulps  ymm6, ymm6, ymm0
    vaddps  ymm6, ymm6, [lt_one4]       ; a = wu + 1
    vbroadcastss ymm0, [rdi + AL_BZ]
    vmovaps ymm1, [rbp + DX_GZ]
    vsubps  ymm1, ymm1, ymm0
    vmulps  ymm1, ymm1, ymm5
    vaddps  ymm1, ymm1, ymm0
    vsubps  ymm1, ymm1, [lt_6v]         ; b = wh + 1
    vxorps  ymm0, ymm0, ymm0
    vcmpltps ymm0, ymm0, ymm6
    vandps  ymm7, ymm7, ymm0
    vcmpltps ymm0, ymm6, [lt_wmaxav]
    vandps  ymm7, ymm7, ymm0
    vxorps  ymm0, ymm0, ymm0
    vcmpltps ymm0, ymm0, ymm1
    vandps  ymm7, ymm7, ymm0
    vcmpltps ymm0, ymm1, [lt_wmaxbv]
    vandps  ymm7, ymm7, ymm0            ; в раме
    vmovmskps eax, ymm7
    test    eax, eax
    jz      .wr
    vandps  ymm6, ymm6, ymm7
    vandps  ymm1, ymm1, ymm7
    vmovaps [rbp + DX_A], ymm6
    vmovaps [rbp + DX_B], ymm1
    ; до лампочки: затухание 1/d², N·L
    vbroadcastss ymm0, [rdi + AL_BX]
    vsubps  ymm0, ymm0, [rbp + DX_GX]
    vmulps  ymm0, ymm0, [lt_pxu4]
    vbroadcastss ymm1, [rdi + AL_BY]
    vsubps  ymm1, ymm1, [rbp + DX_GY]
    vmulps  ymm1, ymm1, [lt_pxu4]
    vbroadcastss ymm2, [rdi + AL_BZ]
    vsubps  ymm2, ymm2, [rbp + DX_GZ]
    vmulps  ymm3, ymm0, ymm10
    vmulps  ymm4, ymm1, ymm11
    vaddps  ymm3, ymm3, ymm4
    vmulps  ymm4, ymm2, ymm12
    vaddps  ymm3, ymm3, ymm4            ; N·(B − P)
    vmulps  ymm0, ymm0, ymm0
    vmulps  ymm1, ymm1, ymm1
    vmulps  ymm2, ymm2, ymm2
    vaddps  ymm0, ymm0, ymm1
    vaddps  ymm0, ymm0, ymm2            ; dB²
    vmaxps  ymm0, ymm0, [lt_one4]
    vrsqrtps ymm1, ymm0
    vmulps  ymm3, ymm3, ymm1
    vaddps  ymm3, ymm3, ymm9
    vmulps  ymm3, ymm3, ymm8
    vmaxps  ymm3, ymm3, [lt_zero4]
    vbroadcastss ymm1, [rdi + AL_IRB]
    vmulps  ymm1, ymm1, ymm0
    vmovaps ymm2, [lt_one4]
    vsubps  ymm2, ymm2, ymm1
    vmaxps  ymm2, ymm2, [lt_zero4]
    vmulps  ymm2, ymm2, ymm2
    vmulps  ymm3, ymm3, ymm2
    vbroadcastss ymm1, [rdi + AL_BD2]
    vdivps  ymm1, ymm1, ymm0
    vminps  ymm1, ymm1, [lt_one4]
    vmulps  ymm3, ymm3, ymm1
    vandps  ymm3, ymm3, ymm7
    vmovaps [rbp + DX_FP], ymm3
    ; маска рамы по дорожкам: чёткая и размытая, билинейно; дальше от рамы — размытее
    vmovmskps ebx, ymm7                 ; (r14 — номер огня, нужен после)
    xor     ecx, ecx
.wl:
    bt      ebx, ecx
    jnc     .wln
    vmovss  xmm1, [rbp + DX_A + rcx*4]
    vmovss  xmm2, [rbp + DX_B + rcx*4]
    vcvttss2si eax, xmm1
    vcvtsi2ss xmm3, xmm3, eax
    vsubss  xmm1, xmm1, xmm3            ; fa
    vcvttss2si edx, xmm2
    vcvtsi2ss xmm3, xmm3, edx
    vsubss  xmm2, xmm2, xmm3            ; fb
    imul    edx, edx, WM_W
    add     edx, eax
    mov     rax, [rdi + AL_WMS]
    add     rdx, rax
    call    .mask
    vmovaps xmm6, xmm3                  ; чёткая
    add     rdx, 256
    call    .mask                       ; размытая
    vmovss  xmm0, [f_1]
    vmovss  xmm4, [rbp + DX_T + rcx*4]
    vmaxss  xmm4, xmm4, [f_0_05]
    vdivss  xmm0, xmm0, xmm4
    vsubss  xmm0, xmm0, [f_1]
    vmulss  xmm0, xmm0, [lt_blur]
    vminss  xmm0, xmm0, [f_1]
    vsubss  xmm3, xmm3, xmm6
    vmulss  xmm3, xmm3, xmm0
    vaddss  xmm3, xmm3, xmm6
    vmulss  xmm3, xmm3, [f_inv255]
    vmulss  xmm3, xmm3, [rbp + DX_FP + rcx*4]
    vmovss  [rbp + DX_FP + rcx*4], xmm3
.wln:
    inc     ecx
    cmp     ecx, 8
    jb      .wl
.wr:
    ret
; rdx -> маска (левый верхний из 2x2), xmm1 = fa, xmm2 = fb -> xmm3 (0..255); портит xmm4, xmm5, eax
.mask:
    movzx   eax, byte [rdx]
    vcvtsi2ss xmm3, xmm3, eax
    movzx   eax, byte [rdx + 1]
    vcvtsi2ss xmm4, xmm4, eax
    vsubss  xmm4, xmm4, xmm3
    vmulss  xmm4, xmm4, xmm1
    vaddss  xmm3, xmm3, xmm4
    movzx   eax, byte [rdx + WM_W]
    vcvtsi2ss xmm4, xmm4, eax
    movzx   eax, byte [rdx + WM_W + 1]
    vcvtsi2ss xmm5, xmm5, eax
    vsubss  xmm5, xmm5, xmm4
    vmulss  xmm5, xmm5, xmm1
    vaddss  xmm4, xmm4, xmm5
    vsubss  xmm4, xmm4, xmm3
    vmulss  xmm4, xmm4, xmm2
    vaddss  xmm3, xmm3, xmm4
    ret

    ; ----- тень по карте горизонта огня (rdi) -> ymm5: 2x2 сэмпла у точки, сдвинутой к
    ; огню на msd тайлов; крыша, вне карты — 1
.hz:
    vmovaps ymm0, [rbp + DX_DX]
    vmovaps ymm1, [rbp + DX_DY]
    vmulps  ymm2, ymm0, ymm0
    vmulps  ymm3, ymm1, ymm1
    vaddps  ymm2, ymm2, ymm3
    vmaxps  ymm2, ymm2, [lt_1e4v]
    vrsqrtps ymm2, ymm2
    vmulps  ymm2, ymm2, [rbp + DX_MSD]
    vmulps  ymm0, ymm0, ymm2
    vmulps  ymm1, ymm1, ymm2
    vaddps  ymm0, ymm0, [rbp + DX_GX]
    vaddps  ymm1, ymm1, [rbp + DX_GY]
    vbroadcastss ymm2, [rdi + AL_HSC]
    vmulps  ymm0, ymm0, ymm2
    vmulps  ymm1, ymm1, ymm2
    vcvttps2dq ymm0, ymm0               ; точка выборки, 64 на сэмпл карты
    vcvttps2dq ymm1, ymm1
    vpand   ymm2, ymm0, [lt_d63]
    vmovdqa [rbp + DX_A], ymm2          ; доли
    vpand   ymm3, ymm1, [lt_d63]
    vmovdqa [rbp + DX_B], ymm3
    vpsrad  ymm0, ymm0, 6
    vpsrad  ymm1, ymm1, 6
    vpbroadcastd ymm4, [rdi + AL_HMX]
    vpsubd  ymm0, ymm0, ymm4            ; сэмпл карты
    vpbroadcastd ymm4, [rdi + AL_HMY]
    vpsubd  ymm1, ymm1, ymm4
    mov     eax, [rdi + AL_HMW]
    dec     eax
    vmovd   xmm4, eax
    vpbroadcastd ymm4, xmm4             ; W − 1
    vpcmpgtd ymm6, ymm4, ymm0
    vpcmpgtd ymm7, ymm4, ymm1
    vpand   ymm6, ymm6, ymm7            ; i, j < W − 1
    vpxor   ymm7, ymm7, ymm7
    vpcmpgtd ymm7, ymm7, ymm0
    vpandn  ymm7, ymm7, ymm6            ; i ≥ 0
    vpxor   ymm6, ymm6, ymm6
    vpcmpgtd ymm6, ymm6, ymm1
    vpandn  ymm6, ymm6, ymm7            ; j ≥ 0
    vpand   ymm6, ymm6, [rbp + DX_M]
    vmovdqa ymm7, [rbp + DX_OWN]
    vpandn  ymm7, ymm7, ymm6            ; тенить: в карте, огонь достаёт, не крыша
    vmovaps ymm5, [lt_one4]
    vmovmskps eax, ymm7
    test    eax, eax
    jz      .hzr
    vmovdqa [rbp + DX_TMP], ymm7
    vpand   ymm0, ymm0, ymm7
    vpand   ymm1, ymm1, ymm7
    vpbroadcastd ymm4, [rdi + AL_HMW]
    vpmaddwd ymm1, ymm1, ymm4
    vpaddd  ymm7, ymm1, ymm0            ; индекс j·W + i
    mov     rdx, [rdi + AL_HM]
    vpcmpeqd ymm5, ymm5, ymm5
    vpxor   ymm2, ymm2, ymm2
    vpgatherdd ymm2, [rdx + ymm7*4], ymm5       ; c00
    vpcmpeqd ymm5, ymm5, ymm5
    vpxor   ymm1, ymm1, ymm1
    vpgatherdd ymm1, [rdx + ymm7*4 + 4], ymm5   ; c10
    vpaddd  ymm7, ymm7, ymm4                    ; строкой ниже
    vpcmpeqd ymm5, ymm5, ymm5
    vpxor   ymm3, ymm3, ymm3
    vpgatherdd ymm3, [rdx + ymm7*4], ymm5       ; c01
    vpcmpeqd ymm5, ymm5, ymm5
    vpxor   ymm6, ymm6, ymm6
    vpgatherdd ymm6, [rdx + ymm7*4 + 4], ymm5   ; c11
    vmovdqa ymm4, [rbp + DX_ZI]
    vpaddd  ymm4, ymm4, [lt_d64]
    vpslld  ymm4, ymm4, 16              ; (z + 64) << 16
    HZT8    ymm2
    HZT8    ymm1
    HZT8    ymm3
    HZT8    ymm6
    ; углы словами, прижаты к 0..256: [t00 | t10], [t01 | t11]
    vpackssdw ymm2, ymm2, ymm1
    vpackssdw ymm3, ymm3, ymm6
    vpxor   ymm0, ymm0, ymm0
    vpmaxsw ymm2, ymm2, ymm0
    vpmaxsw ymm3, ymm3, ymm0
    vpminsw ymm2, ymm2, [lt_w256]
    vpminsw ymm3, ymm3, [lt_w256]
    ; билинейно (сумма весов 4096): по x — ряды, по y — их
    vmovdqa ymm0, [rbp + DX_A]          ; fx
    vpslld  ymm5, ymm0, 16
    vpsubd  ymm5, ymm5, ymm0
    vpaddd  ymm5, ymm5, [lt_d64]        ; 64 − fx | fx << 16
    vpshufd ymm0, ymm2, 0xEE
    vpunpcklwd ymm2, ymm2, ymm0         ; t00 t10 по дорожкам
    vpmaddwd ymm2, ymm2, ymm5
    vpshufd ymm0, ymm3, 0xEE
    vpunpcklwd ymm3, ymm3, ymm0
    vpmaddwd ymm3, ymm3, ymm5
    vpackssdw ymm2, ymm2, ymm3          ; ряды 0 | 1 (≤ 16384)
    vmovdqa ymm0, [rbp + DX_B]          ; fy
    vpslld  ymm5, ymm0, 16
    vpsubd  ymm5, ymm5, ymm0
    vpaddd  ymm5, ymm5, [lt_d64]
    vpshufd ymm0, ymm2, 0xEE
    vpunpcklwd ymm2, ymm2, ymm0
    vpmaddwd ymm2, ymm2, ymm5
    vcvtdq2ps ymm2, ymm2
    vmulps  ymm2, ymm2, [lt_i1mv]       ; 0..1
    vmovaps ymm0, [rbp + DX_TMP]
    vandps  ymm2, ymm2, ymm0
    vandnps ymm0, ymm0, [lt_one4]
    vorps   ymm2, ymm2, ymm0            ; где не тенили — 1
    vmovaps ymm5, ymm2
.hzr:
    ret

    ; ----- тень фонарика -> ymm2 (как .fls у dl_row; углы — сбором)
.fls:
    vbroadcastss ymm0, [dl_sx]
    vmovaps ymm1, [rbp + DX_GX]
    vsubps  ymm1, ymm1, ymm0            ; от руки, 8.8
    vbroadcastss ymm0, [dl_sy]
    vmovaps ymm2, [rbp + DX_GY]
    vsubps  ymm2, ymm2, ymm0
    vmulps  ymm3, ymm1, ymm1
    vmulps  ymm4, ymm2, ymm2
    vaddps  ymm3, ymm3, ymm4
    vmaxps  ymm3, ymm3, [lt_1e4v]
    vrsqrtps ymm4, ymm3
    vmulps  ymm3, ymm3, ymm4            ; r
    vsubps  ymm3, ymm3, [rbp + DX_MSD]
    vmaxps  ymm3, ymm3, [lt_zero4]
    vmulps  ymm5, ymm3, ymm4
    vmulps  ymm1, ymm1, ymm5
    vmulps  ymm2, ymm2, ymm5            ; точка выборки от руки
    vmulps  ymm3, ymm3, [fl_idsv]
    vmaxps  ymm3, ymm3, [lt_0_5v]       ; у самой руки — не в нулевом слоте
    vminps  ymm3, ymm3, [fl_rmaxv]
    vmovaps [rbp + DX_F0], ymm3         ; r, слоты
    vbroadcastss ymm5, [fl_ax]
    vbroadcastss ymm6, [fl_ay]
    vmulps  ymm0, ymm1, ymm5
    vmulps  ymm7, ymm2, ymm6
    vaddps  ymm0, ymm0, ymm7            ; вдоль оси
    vmulps  ymm2, ymm2, ymm5
    vmulps  ymm1, ymm1, ymm6
    vsubps  ymm2, ymm2, ymm1            ; поперёк
    vmaxps  ymm0, ymm0, [lt_zero4]
    vandps  ymm1, ymm2, [lt_absm]
    vaddps  ymm0, ymm0, ymm1
    vaddps  ymm0, ymm0, [lt_1e4v]
    vdivps  ymm2, ymm2, ymm0            ; ромбовый угол
    vmulps  ymm2, ymm2, [fl_kuv]
    vaddps  ymm2, ymm2, [fl_u0v]        ; луч, дробный
    vmaxps  ymm2, ymm2, [lt_zero4]
    vminps  ymm2, ymm2, [fl_umaxv]
    vcvttps2dq ymm0, ymm2
    vcvtdq2ps ymm1, ymm0
    vsubps  ymm2, ymm2, ymm1
    vmovaps [rbp + DX_F1], ymm2         ; доля луча
    vcvttps2dq ymm1, ymm3
    vcvtdq2ps ymm4, ymm1
    vsubps  ymm3, ymm3, ymm4
    vmovaps [rbp + DX_F2], ymm3         ; доля слота
    vpmaddwd ymm0, ymm0, [fl_nw]
    vpaddd  ymm0, ymm0, ymm1
    vpaddd  ymm0, ymm0, ymm0            ; (луч, слот) угла 00, дворды
    vmovdqa [rbp + DX_T], ymm0
    lea     rdx, [fl_buf]
    call    .flr                        ; луч i
    vmovaps [rbp + DX_F6], ymm2
    lea     rdx, [fl_buf + FL_N*8]
    call    .flr                        ; луч i + 1
    vsubps  ymm2, ymm2, [rbp + DX_F6]
    vmulps  ymm2, ymm2, [rbp + DX_F1]
    vaddps  ymm2, ymm2, [rbp + DX_F6]
    ret
; луч (rdx -> fl_buf луча угла 00) -> ymm2: два слота, между ними — по доле слота
.flr:
    vmovdqa ymm7, [rbp + DX_T]
    vpcmpeqd ymm0, ymm0, ymm0
    vpxor   ymm4, ymm4, ymm4
    vpgatherdd ymm4, [rdx + ymm7*4 + 8], ymm0   ; m, слот j + 1
    vmovaps [rbp + DX_F3], ymm4
    vpcmpeqd ymm0, ymm0, ymm0
    vpxor   ymm4, ymm4, ymm4
    vpgatherdd ymm4, [rdx + ymm7*4 + 12], ymm0  ; 1/k*
    vmovaps [rbp + DX_F4], ymm4
    vpcmpeqd ymm0, ymm0, ymm0
    vpxor   ymm2, ymm2, ymm2
    vpgatherdd ymm2, [rdx + ymm7*4], ymm0       ; m, слот j
    vpcmpeqd ymm0, ymm0, ymm0
    vpxor   ymm1, ymm1, ymm1
    vpgatherdd ymm1, [rdx + ymm7*4 + 4], ymm0   ; 1/k*
    call    .flt
    vmovaps [rbp + DX_F5], ymm2
    vmovaps ymm2, [rbp + DX_F3]
    vmovaps ymm1, [rbp + DX_F4]
    call    .flt
    vsubps  ymm2, ymm2, [rbp + DX_F5]
    vmulps  ymm2, ymm2, [rbp + DX_F2]
    vaddps  ymm2, ymm2, [rbp + DX_F5]
    ret
; ymm2 = m, ymm1 = 1/k* -> ymm2 = доля света 0..1 (портит ymm0, ymm1, ymm4)
.flt:
    vmovaps ymm0, [rbp + DX_F0]
    vmulps  ymm2, ymm2, ymm0
    vaddps  ymm2, ymm2, [fl_hv]         ; горизонт, px
    vmulps  ymm1, ymm1, ymm0
    vmulps  ymm1, ymm1, [fl_szv]
    vsubps  ymm1, ymm1, [fl_szv]        ; полутень: размер·(r/k* − 1)
    vmaxps  ymm1, ymm1, [fl_pen0v]
    vmovaps ymm4, [rbp + DX_GZ]
    vsubps  ymm4, ymm4, ymm2
    vdivps  ymm4, ymm4, ymm1
    vaddps  ymm4, ymm4, [lt_0_5v]
    vmaxps  ymm4, ymm4, [lt_zero4]
    vminps  ymm2, ymm4, [lt_one4]
    ret

    ; ----- фигуры у огня (rdi): их капсулы (figsh.inc) на отрезке пиксель — огонь;
    ; ymm5 *= тень (как .figs у dl_row; фигура, чья рамка тени мимо четвёрки, для её
    ; дорожек под маской — у dl_row четвёрка её пропускает)
.figs:
    vmovaps ymm0, [rbp + DX_F]
    vorps   ymm0, ymm0, [rbp + DX_FP]
    vpxor   ymm1, ymm1, ymm1
    vpcmpeqd ymm0, ymm0, ymm1
    vcmpltps ymm1, ymm1, ymm5
    vandnps ymm0, ymm0, ymm1
    vandps  ymm0, ymm0, [rbp + DX_M]
    vmovaps [rbp + DX_CMB], ymm0
    vmovmskps eax, ymm0
    test    eax, eax
    jz      .fb1
    call    .gbb
    vmovaps ymm0, [rbp + DX_DX]
    vmulps  ymm0, ymm0, ymm0
    vmovaps ymm1, [rbp + DX_DY]
    vmulps  ymm1, ymm1, ymm1
    vaddps  ymm0, ymm0, ymm1
    vmovaps ymm1, [rbp + DX_DZ]
    vmulps  ymm1, ymm1, ymm1
    vaddps  ymm0, ymm0, ymm1
    vmaxps  ymm0, ymm0, [lt_one4]
    vmovaps [rbp + DX_AA], ymm0         ; |D|²
    vmovaps ymm1, [lt_one4]
    vdivps  ymm1, ymm1, ymm0
    vmovaps [rbp + DX_IA], ymm1
    vbroadcastss ymm0, [lt_hmsz]        ; размер огня: фонарь, окно
    cmp     dword [rdi + AL_KIND], LK_WIN
    jne     .fs0
    vbroadcastss ymm0, [lt_hmsz + 4]
.fs0:
    vmovaps [rbp + DX_SZL], ymm0
    mov     r10d, [rdi + AL_NF]
    lea     r9, [rdi + AL_FIG]
; фигуры r9 (как AL_FIG: рамка тени, номер записи), r10d > 0 — сколько; дорожки — DX_CMB
.figl:
.fg:
    xor     eax, eax
    vmovss  xmm0, [rbp + DX_BB + 4]
    vcomiss xmm0, [r9 + 8]
    jb      .fh0
    vmovss  xmm0, [rbp + DX_BB]
    vcomiss xmm0, [r9 + 12]
    ja      .fh0
    vmovss  xmm0, [rbp + DX_BB + 12]
    vcomiss xmm0, [r9 + 16]
    jb      .fh0
    vmovss  xmm0, [rbp + DX_BB + 8]
    vcomiss xmm0, [r9 + 20]
    ja      .fh0
    or      eax, 1                      ; первая четвёрка
.fh0:
    vmovss  xmm0, [rbp + DX_BB + 16 + 4]
    vcomiss xmm0, [r9 + 8]
    jb      .fh1
    vmovss  xmm0, [rbp + DX_BB + 16]
    vcomiss xmm0, [r9 + 12]
    ja      .fh1
    vmovss  xmm0, [rbp + DX_BB + 16 + 12]
    vcomiss xmm0, [r9 + 16]
    jb      .fh1
    vmovss  xmm0, [rbp + DX_BB + 16 + 8]
    vcomiss xmm0, [r9 + 20]
    ja      .fh1
    or      eax, 2                      ; вторая
.fh1:
    test    eax, eax
    jz      .fgn
    shl     eax, 5
    lea     r8, [lx_hmask]
    vmovaps ymm0, [r8 + rax]            ; дорожки четвёрок, чью рамку тень задевает
    vandps  ymm0, ymm0, [rbp + DX_CMB]
    vmovaps [rbp + DX_CM], ymm0
    mov     eax, [r9 + 24]
    imul    eax, eax, SC_SZ
    lea     rdx, [sc_buf]
    add     rdx, rax
    cmp     dword [rdx + SC_N], 0
    je      .fgn
    call    .cap
.fgn:
    add     r9, AL_FSZ
    dec     r10d
    jnz     .fg
    ret

    ; ----- тень фигуры (rdx -> запись sc_buf): .cap у dl_row по 8 дорожкам
.cap:
    vbroadcastss ymm0, [rdx + SC_FX]
    vsubps  ymm0, ymm0, [rbp + DX_PX]   ; к фигуре
    vbroadcastss ymm1, [rdx + SC_FY]
    vsubps  ymm1, ymm1, [rbp + DX_PY]
    vmulps  ymm2, ymm0, [rbp + DX_DX]
    vmulps  ymm3, ymm1, [rbp + DX_DY]
    vaddps  ymm2, ymm2, ymm3
    vmovaps ymm3, [rbp + DX_DX]
    vmulps  ymm3, ymm3, ymm3
    vmovaps ymm4, [rbp + DX_DY]
    vmulps  ymm4, ymm4, ymm4
    vaddps  ymm3, ymm3, ymm4
    vmaxps  ymm3, ymm3, [lt_1e4v]
    vdivps  ymm2, ymm2, ymm3            ; t ближайшей точки
    vxorps  ymm6, ymm6, ymm6
    vcmpltps ymm6, ymm6, ymm2
    vcmpleps ymm7, ymm2, [lt_one4]
    vandps  ymm6, ymm6, ymm7
    vandps  ymm6, ymm6, [rbp + DX_CM]
    vmulps  ymm7, ymm2, [rbp + DX_DZ]
    vaddps  ymm7, ymm7, [rbp + DX_GZ]   ; луч там
    vmovss  xmm4, [rdx + SC_TOP]
    vaddss  xmm4, xmm4, [rdx + SC_RB]
    vbroadcastss ymm4, xmm4
    vmulps  ymm3, ymm2, [rbp + DX_SZL]
    vaddps  ymm4, ymm4, ymm3            ; + полутень
    vcmpltps ymm7, ymm7, ymm4
    vandps  ymm6, ymm6, ymm7
    vmulps  ymm7, ymm2, [rbp + DX_DX]
    vsubps  ymm0, ymm0, ymm7
    vmulps  ymm0, ymm0, ymm0
    vmulps  ymm7, ymm2, [rbp + DX_DY]
    vsubps  ymm1, ymm1, ymm7
    vmulps  ymm1, ymm1, ymm1
    vaddps  ymm0, ymm0, ymm1            ; сбоку²
    vmulps  ymm1, ymm2, [rbp + DX_SZL]
    vmovss  xmm4, [rdx + SC_RB]
    vaddss  xmm4, xmm4, [f_1]
    vbroadcastss ymm4, xmm4
    vaddps  ymm1, ymm1, ymm4
    vmulps  ymm1, ymm1, ymm1
    vcmpltps ymm0, ymm0, ymm1
    vandps  ymm6, ymm6, ymm0
    vmovmskps eax, ymm6
    test    eax, eax
    jz      .cpr
    vmovaps [rbp + DX_CM2], ymm6
    vxorps  ymm0, ymm0, ymm0
    vmovaps [rbp + DX_OM], ymm0
    mov     ecx, [rdx + SC_N]
    lea     r8, [rdx + SC_HSZ]
.cp1:
    ; отсев: прямая пиксель — огонь дальше шара капсулы у всех дорожек — доля 0
    vbroadcastss ymm0, [r8 + SC_CX]
    vsubps  ymm0, ymm0, [rbp + DX_PX]
    vbroadcastss ymm1, [r8 + SC_CY]
    vsubps  ymm1, ymm1, [rbp + DX_PY]
    vbroadcastss ymm2, [r8 + SC_CZ]
    vsubps  ymm2, ymm2, [rbp + DX_GZ]
    vmulps  ymm3, ymm0, [rbp + DX_DX]
    vmulps  ymm4, ymm1, [rbp + DX_DY]
    vaddps  ymm3, ymm3, ymm4
    vmulps  ymm4, ymm2, [rbp + DX_DZ]
    vaddps  ymm3, ymm3, ymm4
    vmulps  ymm0, ymm0, ymm0
    vmulps  ymm1, ymm1, ymm1
    vaddps  ymm0, ymm0, ymm1
    vmulps  ymm2, ymm2, ymm2
    vaddps  ymm0, ymm0, ymm2
    vmulps  ymm3, ymm3, ymm3
    vmulps  ymm3, ymm3, [rbp + DX_IA]
    vsubps  ymm0, ymm0, ymm3
    vmovss  xmm1, [r8 + SC_CR]
    vaddss  xmm1, xmm1, [rbp + DX_WM]
    vmulss  xmm1, xmm1, xmm1
    vbroadcastss ymm1, xmm1
    vcmpltps ymm0, ymm0, ymm1
    vandps  ymm0, ymm0, [rbp + DX_CM2]
    vmovmskps eax, ymm0
    test    eax, eax
    jz      .cp2
    call    .cap1
    vmaxps  ymm0, ymm0, [rbp + DX_OM]
    vmovaps [rbp + DX_OM], ymm0
.cp2:
    add     r8, SC_CSZ
    dec     ecx
    jnz     .cp1
    vmovaps ymm0, [rbp + DX_OM]
    vandps  ymm0, ymm0, [rbp + DX_CM2]
    vmovaps ymm1, [lt_one4]
    vsubps  ymm1, ymm1, ymm0
    vmulps  ymm5, ymm5, ymm1
.cpr:
    ret
; капсула r8 -> ymm0 = доля тени (.cap1 у dl_row); r0 — в ymm1..3
.cap1:
    vbroadcastss ymm0, [r8 + SC_AX]
    vmovaps ymm1, [rbp + DX_PX]
    vsubps  ymm1, ymm1, ymm0
    vbroadcastss ymm0, [r8 + SC_AY]
    vmovaps ymm2, [rbp + DX_PY]
    vsubps  ymm2, ymm2, ymm0
    vbroadcastss ymm0, [r8 + SC_AZ]
    vmovaps ymm3, [rbp + DX_GZ]
    vsubps  ymm3, ymm3, ymm0            ; r0 = пиксель − A
    vbroadcastss ymm0, [r8 + SC_EX]
    vmulps  ymm0, ymm0, [rbp + DX_DX]
    vbroadcastss ymm4, [r8 + SC_EY]
    vmulps  ymm4, ymm4, [rbp + DX_DY]
    vaddps  ymm0, ymm0, ymm4
    vbroadcastss ymm4, [r8 + SC_EZ]
    vmulps  ymm4, ymm4, [rbp + DX_DZ]
    vaddps  ymm0, ymm0, ymm4
    vmovaps [rbp + DX_CB], ymm0         ; b = D·E
    vmulps  ymm0, ymm1, [rbp + DX_DX]
    vmulps  ymm4, ymm2, [rbp + DX_DY]
    vaddps  ymm0, ymm0, ymm4
    vmulps  ymm4, ymm3, [rbp + DX_DZ]
    vaddps  ymm0, ymm0, ymm4
    vmovaps [rbp + DX_CF], ymm0         ; c = D·r0
    vbroadcastss ymm0, [r8 + SC_EX]
    vmulps  ymm0, ymm0, ymm1
    vbroadcastss ymm4, [r8 + SC_EY]
    vmulps  ymm4, ymm4, ymm2
    vaddps  ymm0, ymm0, ymm4
    vbroadcastss ymm4, [r8 + SC_EZ]
    vmulps  ymm4, ymm4, ymm3
    vaddps  ymm0, ymm0, ymm4            ; f = E·r0
    vbroadcastss ymm4, [r8 + SC_IE]
    vmulps  ymm4, ymm4, ymm0
    vmaxps  ymm4, ymm4, [lt_zero4]
    vminps  ymm4, ymm4, [lt_one4]       ; s0
    vbroadcastss ymm6, [r8 + SC_EX]
    vmulps  ymm6, ymm6, ymm4
    vsubps  ymm6, ymm6, ymm1
    vmulps  ymm6, ymm6, ymm6
    vbroadcastss ymm7, [r8 + SC_EY]
    vmulps  ymm7, ymm7, ymm4
    vsubps  ymm7, ymm7, ymm2
    vmulps  ymm7, ymm7, ymm7
    vaddps  ymm6, ymm6, ymm7
    vbroadcastss ymm7, [r8 + SC_EZ]
    vmulps  ymm7, ymm7, ymm4
    vsubps  ymm7, ymm7, ymm3
    vmulps  ymm7, ymm7, ymm7
    vaddps  ymm6, ymm6, ymm7
    vbroadcastss ymm7, [r8 + SC_RS2]
    vcmpnltps ymm6, ymm6, ymm7
    vmovaps [rbp + DX_NO], ymm6         ; не своя
    vmulps  ymm4, ymm0, [rbp + DX_CB]
    vbroadcastss ymm6, [r8 + SC_EE]
    vmulps  ymm7, ymm6, [rbp + DX_CF]
    vsubps  ymm4, ymm4, ymm7
    vmulps  ymm6, ymm6, [rbp + DX_AA]
    vmovaps ymm7, [rbp + DX_CB]
    vmulps  ymm7, ymm7, ymm7
    vsubps  ymm6, ymm6, ymm7
    vmaxps  ymm6, ymm6, [lt_1e4v]
    vdivps  ymm4, ymm4, ymm6
    vmaxps  ymm4, ymm4, [lt_zero4]
    vminps  ymm4, ymm4, [lt_one4]
    vmulps  ymm6, ymm4, [rbp + DX_CB]
    vaddps  ymm6, ymm6, ymm0
    vbroadcastss ymm7, [r8 + SC_IE]
    vmulps  ymm6, ymm6, ymm7
    vmaxps  ymm6, ymm6, [lt_zero4]
    vminps  ymm6, ymm6, [lt_one4]       ; s
    vmulps  ymm4, ymm6, [rbp + DX_CB]
    vsubps  ymm4, ymm4, [rbp + DX_CF]
    vmulps  ymm4, ymm4, [rbp + DX_IA]
    vmaxps  ymm4, ymm4, [lt_zero4]
    vminps  ymm4, ymm4, [lt_one4]       ; t
    vmulps  ymm0, ymm4, [rbp + DX_DX]
    vaddps  ymm0, ymm0, ymm1
    vbroadcastss ymm7, [r8 + SC_EX]
    vmulps  ymm7, ymm7, ymm6
    vsubps  ymm0, ymm0, ymm7
    vmulps  ymm0, ymm0, ymm0
    vmulps  ymm1, ymm4, [rbp + DX_DY]
    vaddps  ymm1, ymm1, ymm2
    vbroadcastss ymm7, [r8 + SC_EY]
    vmulps  ymm7, ymm7, ymm6
    vsubps  ymm1, ymm1, ymm7
    vmulps  ymm1, ymm1, ymm1
    vaddps  ymm0, ymm0, ymm1
    vmulps  ymm1, ymm4, [rbp + DX_DZ]
    vaddps  ymm1, ymm1, ymm3
    vbroadcastss ymm7, [r8 + SC_EZ]
    vmulps  ymm7, ymm7, ymm6
    vsubps  ymm1, ymm1, ymm7
    vmulps  ymm1, ymm1, ymm1
    vaddps  ymm0, ymm0, ymm1
    vsqrtps ymm0, ymm0
    vbroadcastss ymm1, [r8 + SC_RHO]
    vsubps  ymm1, ymm1, ymm0            ; ρ − d
    vmulps  ymm2, ymm4, [rbp + DX_SZL]
    vaddps  ymm2, ymm2, [sc_w0v]        ; полутень у капсулы
    vdivps  ymm1, ymm1, ymm2
    vaddps  ymm1, ymm1, [lt_0_5v]
    vmaxps  ymm1, ymm1, [lt_zero4]
    vminps  ymm1, ymm1, [lt_one4]
    vandps  ymm0, ymm1, [rbp + DX_NO]
    ret

    ; ----- луна: тени фигур mn_fgl -> mfsh (.mf у dl_row; дорожки — все со светом)
.mf:
    vmovaps ymm5, [lt_one4]
    call    .gbb
    vmovaps ymm0, [mn_v]
    vmovaps [rbp + DX_DX], ymm0
    vmovaps ymm0, [mn_v + 32]
    vmovaps [rbp + DX_DY], ymm0
    vmovaps ymm0, [mn_v + 64]
    vmovaps [rbp + DX_DZ], ymm0
    vmovaps ymm0, [mn_v + 96]
    vmovaps [rbp + DX_AA], ymm0
    vmovaps ymm0, [mn_v + 128]
    vmovaps [rbp + DX_IA], ymm0
    vmovaps ymm0, [mn_v + 160]
    vmovaps [rbp + DX_SZL], ymm0
    vmovaps ymm0, [rbp + DX_VAL]
    vmovaps [rbp + DX_CMB], ymm0
    mov     eax, [mn_wm]
    mov     [rbp + DX_WM], eax
    mov     eax, r13d
    shr     eax, 4
    mov     rdx, [rbp + DX_TMF]
    movzx   r11d, word [rdx + rax*2]
.mf1:
    bsf     eax, r11d
    btr     r11d, eax
    imul    eax, eax, MN_FSZ
    lea     r9, [mn_fgl]
    add     r9, rax
    mov     r10d, 1
    call    .figl
    test    r11d, r11d
    jnz     .mf1
    mov     eax, [sc_wmax]
    mov     [rbp + DX_WM], eax
    vmulps  ymm5, ymm5, [lt_255v]
    vcvtps2dq ymm5, ymm5
    vextracti128 xmm0, ymm5, 1
    vpackssdw xmm5, xmm5, xmm0
    vpackuswb xmm5, xmm5, xmm5
    vmovq   [rsi + r12 + (mfsh - albedo)], xmm5
    ret

; рамки четвёрок по их дорожкам со светом (DX_BB, один раз на группу)
.gbb:
    cmp     dword [rbp + DX_BBOK], 0
    jne     .fb1
    ; рамка каждой четвёрки по её дорожкам со светом (как у dl_row)
    vmovaps ymm0, [rbp + DX_VAL]
    vandnps ymm1, ymm0, [lt_big4]
    vandnps ymm2, ymm0, [lt_mbig4]
    vmovaps ymm3, [rbp + DX_GX]
    vandps  ymm3, ymm3, ymm0
    vorps   ymm4, ymm3, ymm2
    vorps   ymm3, ymm3, ymm1
    vshufps ymm6, ymm3, ymm3, 0xEE
    vminps  ymm3, ymm3, ymm6
    vpshufd ymm6, ymm3, 1
    vminps  ymm3, ymm3, ymm6            ; x0 — в 0-й дорожке половины
    vshufps ymm6, ymm4, ymm4, 0xEE
    vmaxps  ymm4, ymm4, ymm6
    vpshufd ymm6, ymm4, 1
    vmaxps  ymm4, ymm4, ymm6            ; x1
    vunpcklps ymm3, ymm3, ymm4          ; x0 x1 . .
    vmovaps ymm7, [rbp + DX_GY]
    vandps  ymm7, ymm7, ymm0
    vorps   ymm4, ymm7, ymm2
    vorps   ymm7, ymm7, ymm1
    vshufps ymm6, ymm7, ymm7, 0xEE
    vminps  ymm7, ymm7, ymm6
    vpshufd ymm6, ymm7, 1
    vminps  ymm7, ymm7, ymm6            ; y0
    vshufps ymm6, ymm4, ymm4, 0xEE
    vmaxps  ymm4, ymm4, ymm6
    vpshufd ymm6, ymm4, 1
    vmaxps  ymm4, ymm4, ymm6            ; y1
    vunpcklps ymm7, ymm7, ymm4          ; y0 y1 . .
    vunpcklpd ymm3, ymm3, ymm7          ; x0 x1 y0 y1 — у каждой половины
    vmovaps [rbp + DX_BB], ymm3
    mov     dword [rbp + DX_BBOK], 1
.fb1:
    ret

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
