; ----------------------------------------------------------------------------
;  bands.inc — объекты и крыши полосами экрана, на всех ядрах
; ----------------------------------------------------------------------------
; Объекты и крыши рисуются художником: пиксель — последняя запись из
; последовательности отрисовок, тест глубины сверяет его же gpos. Кадр делится на
; NBANDS горизонтальных полос; каждая проходит ту же последовательность (те же
; тайлы, фигуры, куски крыш в том же порядке), но пишет только свои строки, так что
; пиксель получает ровно те же записи, что и за один проход, — кадр совпадает до
; байта. У объектов десятки переменных-«регистров» (bk_*, img_*, rq_*, ri_* ...):
; код objects.inc и roofdraw.inc собирается по копии на полосу, имя вида foo
; раскрывается в foo_bN (N — номер копии), и у каждой копии свои переменные

NBANDS      equ 4

; имена копий: foo -> foo_b<BI> (раскрывается при использовании — BI текущей копии)
%macro BAND_NAMES 1-*
%rep %0
    %define %1 %1 %+ _b %+ BI
    %rotate 1
%endrep
%endmacro

BAND_NAMES draw_objects, draw_tile, draw_block, shade_top, shade_side, draw_img, \
    draw_cmap, blob, draw_entity, \
    rq_plane, rq_planel, rq_corner, rq_cornl, rq_fill, rq_line, draw_roofs, \
    roof_setup, roof_draw, roof_antenna, col_mul, roof_mat, sh_slope, sh_gable, \
    sh_verge, sh_towel, sh_fascia, sh_chim, sh_horse, band_draw, \
    band_y0, band_y1, band_h, band_occ, band_t0, band_t1, di_r1, \
    bk_kind, bk_H, bk_tx, bk_ty, bk_sx, bk_sy, bk_seed, bk_cut, bk_faces, bk_notop, \
    bk_dith, col_dh, col_gpos, pix_lx, pix_ly, sh_n, di_t1, di_t2, \
    cm_gpos, cm_dlim, cm_n0, cm_t1, cm_t2, cm_snow, cm_seed, \
    img_sway, img_mode, img_anchor, img_nb, sw_g, sw_vs, tmp_d, cmx, pal, dlist, \
    rq_x, rq_y, rq_s0, rq_sa, rq_sb, rq_n0, rq_n, rq_gp, rq_vis, rq_fade, rq_dpl, \
    rq_r1, rq_c1, rq_face, rq_ax, rq_occ, rq_occn, rq_sh, \
    rf_u0, rf_u1, rf_ug, rf_uv, rf_v0, rf_v1, rf_vm, rf_k, rf_zr, rf_ze, \
    ri_U0, ri_U1, ri_V0, ri_V1, ri_Vm, ri_hs, ri_P, ri_uc, ri_vc, ri_zt, ri_zr, \
    ri_ze, ri_fl, ri_seed, ri_mat, ri_gab, ri_barge, rn_f, rn_b, rn_g, rn_a, \
    rt_a, rt_b, rt_c, rt_d, rt_e, rt_f, la_u0, la_v0, la_z0, la_u1, la_v1, la_z1, \
    la_col, ra_g, ra_s, ra_u, ra_t

%assign BI 0
%rep NBANDS
section .text
%include "objects.inc"
%include "roofdraw.inc"

; ecx = первая строка полосы, edx = за последней: объекты, потом крыши над ними
band_draw:
    sub     rsp, 40
    mov     [band_y0], ecx
    mov     [band_y1], edx
    sub     edx, ecx
    mov     [band_h], edx
    lea     rcx, [band_t0]
    call    [QueryPerformanceCounter]
    call    draw_objects
    call    draw_roofs
    lea     rcx, [band_t1]
    call    [QueryPerformanceCounter]
    add     rsp, 40
    ret

section .data
align 8, db 0
band_t0:    dq 0                        ; сколько шла (для --bench)
band_t1:    dq 0
band_y0:    dd 0                        ; строки полосы: [band_y0, band_y1)
band_y1:    dd SCR_H
band_h:     dd SCR_H
di_r1:      dd 0                        ; draw_img: за последней строкой картинки в полосе
; блок тайла (draw_tile -> draw_block, shade_*)
bk_kind:    dd 0
bk_H:       dd 0
bk_tx:      dd 0
bk_ty:      dd 0
bk_sx:      dd 0
bk_sy:      dd 0
bk_seed:    dd 0
bk_cut:     dd 0
bk_faces:   dd 0
bk_notop:   dd 0
bk_dith:    dd 0
col_dh:     dd 0                        ; сугроб у грани в этой колонке, px
col_gpos:   dd 0
pix_lx:     dd 0
pix_ly:     dd 0
sh_n:       dd 0                        ; нормаль пикселя грани (shade_side)
di_t1:      dd 0
di_t2:      dd 0
; фигуры и картинки
cm_gpos:    dd 0
cm_dlim:    dd 0x7FFFFFFF               ; фигура не рисуется поверх того, что ближе к глазу
cm_n0:      dd 0
cm_t1:      dd 0
cm_t2:      dd 0
cm_snow:    dd 0
cm_seed:    dd 0
img_sway:   dd 0
img_mode:   dd 0
img_anchor: dd 0
img_nb:     dd N_BL                     ; нормали картинки: N_BL / N_SM / N_SB
sw_g:       dd 0.0
sw_vs:      dd 0.0                      ; дерево: ветер вбок по экрану
tmp_d:      dd 0.0
cmx:        dd 0
; проход крыш
rq_x:       dd 0.0, 0.0, 0.0, 0.0      ; углы на экране (за rq_x сразу rq_y)
rq_y:       dd 0.0, 0.0, 0.0, 0.0
rq_s0:      dd 0.0
rq_sa:      dd 0.0
rq_sb:      dd 0.0
rq_n0:      dd 0
rq_n:       dd 0
rq_gp:      dd 0
rq_vis:     dd 0
rq_fade:    dd 16
rq_dpl:     dd 0
rq_r1:      dd 0
rq_c1:      dd 0
rq_face:    dd 0
rq_ax:      dd 0
rf_u0:      dd 0.0
rf_u1:      dd 0.0
rf_ug:      dd 0.0
rf_uv:      dd 0.0
rf_v0:      dd 0.0
rf_v1:      dd 0.0
rf_vm:      dd 0.0
rf_k:       dd 0.0
rf_zr:      dd 0.0
rf_ze:      dd 0.0
ri_U0:      dd 0
ri_U1:      dd 0
ri_V0:      dd 0
ri_V1:      dd 0
ri_Vm:      dd 0
ri_hs:      dd 0
ri_P:       dd 0
ri_uc:      dd 0
ri_vc:      dd 0
ri_zt:      dd 0
ri_zr:      dd 0
ri_ze:      dd 0
ri_fl:      dd 0
ri_seed:    dd 0
ri_mat:     dd 0
ri_gab:     dd 0
ri_barge:   dd 0
rn_f:       dd 0
rn_b:       dd 0
rn_g:       dd 0
rn_a:       dd 0
rt_a:       dd 0.0
rt_b:       dd 0.0
rt_c:       dd 0.0
rt_d:       dd 0.0
rt_e:       dd 0.0
rt_f:       dd 0.0
la_u0:      dd 0.0
la_v0:      dd 0.0
la_z0:      dd 0.0
la_u1:      dd 0.0
la_v1:      dd 0.0
la_z1:      dd 0.0
la_col:     dd 0
ra_g:       dd 0.0
ra_s:       dd 0.0
ra_u:       dd 0.0
ra_t:       dd 0.0
rq_occ:     dd 0
rq_occn:    dd 0
align 8, db 0
rq_sh:      dq 0
; палитра символов персонажей (индекс = ASCII); фигуры перекрашивают свои цвета
pal:
%assign i 0
%rep 128
  %if i == 'h'
    dd 0x016B4A2E
  %elif i == 'H'
    dd 0x01463020
  %elif i == 's'
    dd 0x01E0AE8C
  %elif i == 'S'
    dd 0x01B08066
  %elif i == 'e'
    dd 0x01201818
  %elif i == 'a'
    dd 0x01282220
  %elif i == 'g'
    dd 0x019AA88E
  %elif i == 'G'
    dd 0x016A7862
  %elif i == 'x'
    dd 0x01601010
  %elif i == 'r'
    dd 0x01801414
  %elif i == 'R'
    dd 0x014A0A0A
  %elif i == 'w'
    dd 0x018A6A40
  %elif i == 'm'
    dd 0x01A8B0B8
  %elif i == 'M'
    dd 0x01687078
  %elif i == 'k'
    dd 0x0118181C
  %elif i == 'L'
    dd 0x01303036
  %else
    dd 0x01FF00FF
  %endif
  %assign i i+1
%endrep

section .bss
alignb 8
dlist:      resq 304                    ; фигуры кадра: ключ x+y, индекс
band_occ:   resb MAXH                   ; крыша закрыла фигуру игрока (в этой полосе)

%assign BI BI + 1
%endrep

section .rdata
align 8, db 0
band_tab:                               ; копии: band_draw, band_occ, band_t0
%assign BI 0
%rep NBANDS
    dq band_draw, band_occ, band_t0
%assign BI BI + 1
%endrep
BT_SZ       equ 24

section .text
; задание пула: полосы ecx .. edx−1
obj_job:
    push    rbx
    push    rsi
    sub     rsp, 40
    mov     ebx, ecx
    mov     esi, edx
.l:
    cmp     ebx, esi
    jae     .out
    lea     rax, [band_y]               ; полоса k: строки band_y[k] .. band_y[k+1]
    mov     ecx, [rax + rbx*4]
    mov     edx, [rax + rbx*4 + 4]
    lea     rax, [band_tab]
    imul    r8d, ebx, BT_SZ
    call    [rax + r8]
    inc     ebx
    jmp     .l
.out:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret

; после полос: закрыла ли крыша фигуру игрока — в какой-нибудь полосе (для окна
; в крыше со следующего кадра, HS_OCC); самая долгая полоса -> band_tmax; границы
; полос на следующий кадр — поровну времени по замеру этого (кадр от них не зависит)
BAND_MINH   equ 8                       ; полоса не тоньше, строк
band_merge:
    lea     rdx, [houses]
    xor     ecx, ecx
.h:
    cmp     ecx, [nhouses]
    jae     .t
    xor     eax, eax
    lea     r8, [band_tab + 8]
    mov     r9d, NBANDS
.b:
    mov     r10, [r8]
    or      al, [r10 + rcx]
    add     r8, BT_SZ
    dec     r9d
    jnz     .b
    mov     [rdx + HS_OCC], eax
    add     rdx, HS_SZ
    inc     ecx
    jmp     .h
.t:
    xor     eax, eax
    xorps   xmm5, xmm5                  ; сумма
    lea     r8, [band_tab + 16]
    lea     r11, [band_tk]
    xor     r9d, r9d
.tb:
    mov     r10, [r8]
    mov     rcx, [r10 + 8]
    sub     rcx, [r10]
    cmp     rcx, rax
    cmova   rax, rcx
    cvtsi2ss xmm0, rcx
    movss   [r11 + r9*4], xmm0
    addss   xmm5, xmm0
    add     r8, BT_SZ
    inc     r9d
    cmp     r9d, NBANDS
    jb      .tb
    mov     [band_tmax], rax
    comiss  xmm5, [f_1]
    jb      .out
    ; стоимость строки в полосе — её время / высота; граница j — где набралось
    ; T·j/NBANDS, и к ней — полпути от нынешней (без раскачки)
    mov     eax, NBANDS
    cvtsi2ss xmm7, eax
    lea     rdx, [band_y]
    lea     r10, [band_ny]
    mov     r9d, 1
.j:
    cvtsi2ss xmm6, r9d
    mulss   xmm6, xmm5
    divss   xmm6, xmm7                  ; сколько времени — до границы
    xor     ecx, ecx
.k:
    movss   xmm0, [r11 + rcx*4]
    cmp     ecx, NBANDS - 1
    je      .in
    comiss  xmm6, xmm0
    jbe     .in
    subss   xmm6, xmm0
    inc     ecx
    jmp     .k
.in:
    maxss   xmm0, [f_1]
    divss   xmm6, xmm0
    minss   xmm6, [f_1]                 ; доля полосы k
    mov     eax, [rdx + rcx*4 + 4]
    sub     eax, [rdx + rcx*4]
    cvtsi2ss xmm0, eax
    mulss   xmm6, xmm0
    cvtsi2ss xmm0, dword [rdx + rcx*4]
    addss   xmm6, xmm0
    cvtsi2ss xmm0, dword [rdx + r9*4]
    addss   xmm6, xmm0
    mulss   xmm6, [f_0_5]
    cvtss2si eax, xmm6
    mov     [r10 + r9*4], eax
    inc     r9d
    cmp     r9d, NBANDS
    jb      .j
    ; по порядку и не тоньше BAND_MINH
    mov     r9d, 1
.c:
    mov     eax, [r10 + r9*4]
    mov     ecx, [rdx + r9*4 - 4]
    add     ecx, BAND_MINH
    cmp     eax, ecx
    cmovl   eax, ecx
    mov     ecx, NBANDS
    sub     ecx, r9d
    imul    ecx, ecx, -BAND_MINH
    add     ecx, SCR_H
    cmp     eax, ecx
    cmovg   eax, ecx
    mov     [rdx + r9*4], eax
    inc     r9d
    cmp     r9d, NBANDS
    jb      .c
.out:
    ret

section .data
align 8, db 0
band_tmax:  dq 0                        ; тики самой долгой полосы кадра (--bench)
band_y:                                 ; границы полос, строки (первая 0, последняя SCR_H)
%assign i 0
%rep NBANDS + 1
    dd SCR_H * i / NBANDS
%assign i i + 1
%endrep
section .bss
band_tk:    resd NBANDS                 ; время полос, f32
band_ny:    resd NBANDS + 1
section .text
