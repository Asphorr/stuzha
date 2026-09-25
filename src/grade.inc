; ----------------------------------------------------------------------------
;  grade.inc — цвет кадра: палитры по времени и погоде, ночное зрение, глаз
; ----------------------------------------------------------------------------
; Последний проход по готовому кадру (после блума, частиц, снега и инея, до HUD):
; у каждого пикселя яркость Y (0..255) и цветность d = c − Y; выход
;   c' = Y + O(Y) + S(Y)·d
; где таблица по яркости (256 × 4 слова: O_B, O_G, O_R, S·64) собирается на кадр из
; палитры: кривая яркости (сдвиг, S-кривая, подъём чёрного), ночное зрение (в темноте
; цвет уходит и всё синеет — палочки глаза; яркие огни остаются цветными), насыщенность
; теней и светов, оттенок теней и светов. Палитр пять — ночь, метель, мороз, сумерки,
; рассвет, — смешиваются по часу и погоде; продрогшему — бледнее и синее.
; Глаз привыкает: яркость кадра (середина — втрое) меряется тем же проходом, и
; экспозиция E тянется к ключу палитры — к свету быстро, к темноте медленно (вышел из
; избы — пару секунд почти не видно). E живёт в тоновой кривой tm_lut (до неё свет в
; словах до 2047 — пересвет не теряется), а пересвет ещё и белеет: сверх порога
; каждому каналу прибавляется доля от сильнейшего (как у плёнки: натриевое пятно у
; лампы — к жёлто-белому, а не плоско-оранжевое).
section .text

GP_PK       equ 0                       ; ночное зрение: сила
GP_YP       equ 4                       ;                ниже какой яркости
GP_NB       equ 8                       ;                оттенок серого (B, G, R)
GP_NG       equ 12
GP_NR       equ 16
GP_SAT      equ 20                      ; насыщенность
GP_HSAT     equ 24                      ;              в светах
GP_SHB      equ 28                      ; оттенок теней (прибавка у чёрного, B, G, R)
GP_SHG      equ 32
GP_SHR      equ 36
GP_YSH      equ 40                      ;              до какой яркости
GP_HIB      equ 44                      ; оттенок светов
GP_HIG      equ 48
GP_HIR      equ 52
GP_GAM      equ 56                      ; сдвиг кривой: 0.5 — как есть, меньше — темнее
GP_CON      equ 60                      ; S-кривая
GP_BLK      equ 64                      ; подъём чёрного
GP_L0       equ 68                      ; ключ глаза: яркость кадра, к которой он привык
GP_HLB      equ 72                      ; блум (с халацией — красноватый ореол), ·8
GP_HLG      equ 76
GP_HLR      equ 80
GP_HLK      equ 84                      ; пересвет белеет: доля сильнейшего канала
GP_PQ       equ 88                      ; ночное зрение: потеря цвета (в тех же тенях)
GP_N        equ 24                      ; чисел в палитре
GP_SZ       equ GP_N*4

; палитра rsi -> gd_par с весом xmm0: P += (палитра − P)·w
gp_blend:
    shufps  xmm0, xmm0, 0
    lea     rdx, [gd_par]
    xor     ecx, ecx
.l:
    movups  xmm1, [rsi + rcx]
    movups  xmm2, [rdx + rcx]
    subps   xmm1, xmm2
    mulps   xmm1, xmm0
    addps   xmm2, xmm1
    movups  [rdx + rcx], xmm2
    add     ecx, 16
    cmp     ecx, GP_SZ
    jb      .l
    ret

; xmm0 = экспозиция E -> tm_lut: tm[v] = кривая(v·E) — до колена как есть, выше
; K + S·(1 − e^(−(x − K)/S)), не больше 255 (при E = 1 — ровно прежняя кривая)
tm_build:
    push    rbx
    movss   [tm_e], xmm0
    movaps  xmm6, xmm0
    lea     rdx, [tm_lut]
    xor     ebx, ebx
.tm:
    cvtsi2ss xmm0, ebx
    mulss   xmm0, xmm6
    comiss  xmm0, [lt_tmk]
    ja      .sh
    cvtss2si eax, xmm0
    jmp     .put
.sh:
    subss   xmm0, [lt_tmk]
    mulss   xmm0, [lt_tmis]             ; t
    movss   xmm2, [f_1]
    addss   xmm2, xmm0                  ; e^t ≈ 1 + t + t²/2 + t³/6 + t⁴/24
    movaps  xmm3, xmm0
    mulss   xmm3, xmm0
    movaps  xmm4, xmm3
    mulss   xmm4, [f_0_5]
    addss   xmm2, xmm4
    mulss   xmm3, xmm0
    movaps  xmm4, xmm3
    mulss   xmm4, [lt_inv6]
    addss   xmm2, xmm4
    mulss   xmm3, xmm0
    mulss   xmm3, [lt_inv24]
    addss   xmm2, xmm3
    movss   xmm1, [f_1]
    divss   xmm1, xmm2
    movss   xmm0, [f_1]
    subss   xmm0, xmm1
    mulss   xmm0, [lt_tms]
    addss   xmm0, [lt_tmk]
    cvtss2si eax, xmm0
    mov     ecx, 255
    cmp     eax, ecx
    cmova   eax, ecx
.put:
    mov     [rdx + rbx*4], eax
    inc     ebx
    cmp     ebx, TM_N
    jb      .tm
    ; AVX2-путь не собирает из кривой, пока все ≤ колена, — только если там тождество
    mov     eax, TM_K
    movss   xmm0, [tm_e]
    comiss  xmm0, [f_1]
    je      .thr
    mov     eax, 0xFFFF                 ; −1: собирать всегда
.thr:
    lea     rdx, [tm_thr]
    mov     ecx, 16
.t16:
    mov     [rdx], ax
    add     rdx, 2
    dec     ecx
    jnz     .t16
    pop     rbx
    ret

; новая игра: глаз привык к ночи
grade_reset:
    mov     dword [ex_e], __float32__(1.0)
    mov     dword [ex_meter], 0
    ret

; ---------------------------------------------------------------- на кадр (до света)
; палитра по часу и погоде -> тоновая кривая с экспозицией, порог и доля пересвета,
; таблица по яркости, блум
grade_frame:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32
    ; ----- палитра: ночь, к ней метель, мороз, сумерки, рассвет
    lea     rsi, [gp_night]
    lea     rdi, [gd_par]
    mov     ecx, GP_N
    rep     movsd
    movss   xmm0, [wnd_snow]
    subss   xmm0, [gd_bl0]
    mulss   xmm0, [gd_bl1]
    call    sstep
    lea     rsi, [gp_bliz]
    call    gp_blend
    movss   xmm0, [frost_k]
    lea     rsi, [gp_frost]
    call    gp_blend
    movss   xmm0, [gtime]
    mulss   xmm0, [f_inv60]
    movss   [gd_h], xmm0
    subss   xmm0, [gd_dk0]
    mulss   xmm0, [gd_dk1]
    call    sstep
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    movaps  xmm0, xmm1
    lea     rsi, [gp_dusk]
    call    gp_blend
    movss   xmm0, [gd_h]
    subss   xmm0, [gd_dw0]
    mulss   xmm0, [gd_dw1]
    call    sstep
    lea     rsi, [gp_dawn]
    call    gp_blend
    ; продрог: бледнее, ночное зрение сильнее
    movss   xmm0, [frost_lvl]
    movaps  xmm1, xmm0
    mulss   xmm1, [gd_cold_s]
    movss   xmm2, [f_1]
    subss   xmm2, xmm1
    movss   xmm1, [gd_par + GP_SAT]
    mulss   xmm1, xmm2
    movss   [gd_par + GP_SAT], xmm1
    movss   xmm1, [gd_par + GP_HSAT]
    mulss   xmm1, xmm2
    movss   [gd_par + GP_HSAT], xmm1
    mulss   xmm0, [gd_cold_p]
    addss   xmm0, [gd_par + GP_PK]
    minss   xmm0, [f_0_9]
    movss   [gd_par + GP_PK], xmm0
    ; ----- тоновая кривая — под экспозицию глаза
    movss   xmm0, [ex_e]
    comiss  xmm0, [tm_e]
    je      .tmok
    call    tm_build
.tmok:
    ; ----- пересвет белеет: порог 230/E, доля — pmulhuw (·65536)
    movss   xmm0, [gd_k2]
    divss   xmm0, [ex_e]
    minss   xmm0, [gd_2047]
    cvtss2si eax, xmm0
    movss   xmm0, [gd_par + GP_HLK]
    mulss   xmm0, [gd_65536]
    minss   xmm0, [gd_65535]
    cvtss2si ecx, xmm0
    lea     rdx, [hl_k2]
    xor     r8d, r8d
.hk:
    mov     r9d, eax
    mov     r10d, ecx
    mov     r11d, r8d
    and     r11d, 3
    cmp     r11d, 3
    jne     .hk1
    mov     r9d, 0xFFFF                 ; четвёртое слово — не трогать
    xor     r10d, r10d
.hk1:
    mov     [rdx + r8*2], r9w
    mov     [rdx + r8*2 + (hl_kk - hl_k2)], r10w
    inc     r8d
    cmp     r8d, 16
    jb      .hk
    ; ----- блум с халацией
    lea     rdx, [bl_gainv]
    movss   xmm0, [gd_par + GP_HLB]
    cvtss2si eax, xmm0
    mov     [rdx], ax
    mov     [rdx + 8], ax
    movss   xmm0, [gd_par + GP_HLG]
    cvtss2si eax, xmm0
    mov     [rdx + 2], ax
    mov     [rdx + 10], ax
    movss   xmm0, [gd_par + GP_HLR]
    cvtss2si eax, xmm0
    mov     [rdx + 4], ax
    mov     [rdx + 12], ax
    ; ----- таблица по яркости
    call    gd_table
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; gd_par -> gd_tab[Y] = O_B, O_G, O_R (со знаком), S·64
gd_table:
    push    rbx
    push    rsi
    sub     rsp, 48
    ; оттенок ночного зрения — с яркостью 1
    movss   xmm0, [gd_par + GP_NB]
    mulss   xmm0, [gd_wb]
    movss   xmm1, [gd_par + GP_NG]
    mulss   xmm1, [gd_wg]
    addss   xmm0, xmm1
    movss   xmm1, [gd_par + GP_NR]
    mulss   xmm1, [gd_wr]
    addss   xmm0, xmm1
    movss   xmm1, [f_1]
    divss   xmm1, xmm0
    movss   xmm0, [gd_par + GP_NB]
    mulss   xmm0, xmm1
    subss   xmm0, [f_1]
    movss   [gd_n + 0], xmm0            ; N − 1
    movss   xmm0, [gd_par + GP_NG]
    mulss   xmm0, xmm1
    subss   xmm0, [f_1]
    movss   [gd_n + 4], xmm0
    movss   xmm0, [gd_par + GP_NR]
    mulss   xmm0, xmm1
    subss   xmm0, [f_1]
    movss   [gd_n + 8], xmm0
    ; сдвиг кривой: f = y / (k·(1 − y) + 1), k = 1/GAM − 2
    movss   xmm0, [f_1]
    divss   xmm0, [gd_par + GP_GAM]
    subss   xmm0, [f_2]
    movss   [gd_bk], xmm0
    movss   xmm0, [gd_par + GP_BLK]
    mulss   xmm0, [f_inv255]
    movss   [gd_blk], xmm0
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    movss   [gd_blk1], xmm1
    lea     rsi, [gd_tab]
    xor     ebx, ebx
.y:
    cvtsi2ss xmm7, ebx                  ; Y
    movaps  xmm0, xmm7
    mulss   xmm0, [f_inv255]            ; y
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    mulss   xmm1, [gd_bk]
    addss   xmm1, [f_1]
    divss   xmm0, xmm1                  ; f
    ; S-кривая: f + CON·f(1 − f)(2f − 1)
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    mulss   xmm1, xmm0
    movaps  xmm2, xmm0
    addss   xmm2, xmm2
    subss   xmm2, [f_1]
    mulss   xmm1, xmm2
    mulss   xmm1, [gd_par + GP_CON]
    addss   xmm0, xmm1
    CLAMP01 xmm0
    mulss   xmm0, [gd_blk1]
    addss   xmm0, [gd_blk]
    mulss   xmm0, [f_255]
    movss   [rsp], xmm0                 ; Cy
    ; ночное зрение: синеет p = PK·(1 − Y/YP)², бледнеет q = PQ·(1 − Y/YP)²
    movaps  xmm1, xmm7
    divss   xmm1, [gd_par + GP_YP]
    movss   xmm2, [f_1]
    subss   xmm2, xmm1
    CLAMP01 xmm2
    mulss   xmm2, xmm2
    movaps  xmm1, xmm2
    mulss   xmm1, [gd_par + GP_PQ]
    movss   [rsp + 20], xmm1            ; q
    mulss   xmm2, [gd_par + GP_PK]
    movss   [rsp + 4], xmm2             ; p
    ; насыщенность: от SAT к HSAT на Y 150..255
    movaps  xmm0, xmm7
    subss   xmm0, [gd_150]
    mulss   xmm0, [gd_i105]
    call    sstep
    movss   xmm1, [gd_par + GP_HSAT]
    subss   xmm1, [gd_par + GP_SAT]
    mulss   xmm1, xmm0
    addss   xmm1, [gd_par + GP_SAT]
    movss   [rsp + 8], xmm1             ; s
    ; веса оттенков: тени 1 − Y/YSH, света sstep((Y − 140)/115)
    movaps  xmm0, xmm7
    divss   xmm0, [gd_par + GP_YSH]
    movss   xmm1, [f_1]
    subss   xmm1, xmm0
    CLAMP01 xmm1
    movss   [rsp + 12], xmm1
    movaps  xmm0, xmm7
    subss   xmm0, [gd_140]
    mulss   xmm0, [gd_i115]
    call    sstep
    movss   [rsp + 16], xmm0
    ; O_c = Cy − Y + p·Cy·(N_c − 1) + SH_c·тени + HI_c·света
    xor     ecx, ecx
.ch:
    movss   xmm0, [rsp]
    movaps  xmm1, xmm0
    subss   xmm0, xmm7
    mulss   xmm1, [rsp + 4]
    lea     rdx, [gd_n]
    mulss   xmm1, [rdx + rcx*4]
    addss   xmm0, xmm1
    lea     rdx, [gd_par + GP_SHB]
    movss   xmm1, [rdx + rcx*4]
    mulss   xmm1, [rsp + 12]
    addss   xmm0, xmm1
    lea     rdx, [gd_par + GP_HIB]
    movss   xmm1, [rdx + rcx*4]
    mulss   xmm1, [rsp + 16]
    addss   xmm0, xmm1
    maxss   xmm0, [gd_m255]
    minss   xmm0, [f_255]
    cvtss2si eax, xmm0
    mov     [rsi + rcx*2], ax
    inc     ecx
    cmp     ecx, 3
    jb      .ch
    ; S = s·(1 − q)·(Cy + 1)/(Y + 1), ·64, 0..127
    movss   xmm0, [rsp]
    addss   xmm0, [f_1]
    movaps  xmm1, xmm7
    addss   xmm1, [f_1]
    divss   xmm0, xmm1
    minss   xmm0, [gd_1_5]
    mulss   xmm0, [rsp + 8]
    movss   xmm1, [f_1]
    subss   xmm1, [rsp + 20]
    mulss   xmm0, xmm1
    mulss   xmm0, [gd_64]
    maxss   xmm0, [f_0]
    minss   xmm0, [gd_127]
    cvtss2si eax, xmm0
    mov     [rsi + 6], ax
    add     rsi, 8
    inc     ebx
    cmp     ebx, 256
    jb      .y
    add     rsp, 48
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- проход по кадру
grade_pass:
    sub     rsp, 40
    lea     rcx, [grade_rows]
    mov     edx, SCR_H
    mov     r8d, 8
    call    par_for
    ; замер для глаза: весь кадр + середина (x 160..479, y 90..269) втрое
    lea     rdx, [gd_rows]
    xor     ecx, ecx
    xor     r8, r8                      ; весь кадр
    xor     r9, r9                      ; середина
.s:
    mov     eax, [rdx + rcx*8]
    add     r8, rax
    cmp     ecx, 90
    jb      .sn
    cmp     ecx, 270
    jae     .sn
    mov     eax, [rdx + rcx*8 + 4]
    add     r9, rax
.sn:
    inc     ecx
    cmp     ecx, SCR_H
    jb      .s
    cvtsi2ss xmm0, r8
    mulss   xmm0, [gd_ifull]
    cvtsi2ss xmm1, r9
    mulss   xmm1, [gd_imid]
    mulss   xmm1, [f_3]
    addss   xmm0, xmm1
    mulss   xmm0, [f_0_25]
    movss   [ex_meter], xmm0
    add     rsp, 40
    ret

; строки кадра ecx .. edx−1 (с любого потока пула): по два пикселя в xmm словами;
; сумма яркости строки и её середины -> gd_rows
grade_rows:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    mov     r12d, ecx
    mov     r13d, edx
    mov     rsi, [fbbits]
    lea     rdi, [gd_tab]
    pxor    xmm15, xmm15
    movdqa  xmm14, [gd_yw]
    movdqa  xmm13, [gd_m3]
.row:
    imul    eax, r12d, SCR_W
    lea     rbx, [rsi + rax*4]
    xor     r8d, r8d                    ; яркость строки
    xor     r9d, r9d                    ; её середины
    xor     r10d, r10d                  ; пара
.px:
    movq    xmm0, [rbx + r10*8]
    punpcklbw xmm0, xmm15               ; B0 G0 R0 A0 B1 G1 R1 A1
    movdqa  xmm1, xmm0
    pmaddwd xmm1, xmm14
    pshufd  xmm2, xmm1, 0xB1
    paddd   xmm1, xmm2
    psrld   xmm1, 8                     ; Y0, Y0, Y1, Y1
    movd    eax, xmm1
    pshufd  xmm2, xmm1, 2
    movd    edx, xmm2
    add     r8d, eax
    add     r8d, edx
    lea     ecx, [r10d - 80]
    cmp     ecx, 160
    jae     .nc
    add     r9d, eax
    add     r9d, edx
.nc:
    movq    xmm3, [rdi + rax*8]
    movhps  xmm3, [rdi + rdx*8]         ; O_B O_G O_R S — у обоих
    packssdw xmm1, xmm1
    punpcklwd xmm1, xmm1                ; Y0 ×4, Y1 ×4
    psubw   xmm0, xmm1                  ; d
    pshuflw xmm2, xmm3, 0xFF
    pshufhw xmm2, xmm2, 0xFF            ; S0 ×4, S1 ×4
    pmullw  xmm0, xmm2
    psraw   xmm0, 6
    paddw   xmm0, xmm1
    pand    xmm3, xmm13
    paddw   xmm0, xmm3
    packuswb xmm0, xmm0
    movq    [rbx + r10*8], xmm0
    inc     r10d
    cmp     r10d, SCR_W/2
    jb      .px
    lea     rax, [gd_rows]
    mov     [rax + r12*8], r8d
    mov     [rax + r12*8 + 4], r9d
    inc     r12d
    cmp     r12d, r13d
    jb      .row
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- глаз (в update)
; сцена Ls = замер / E; к ключу L0 палитры: в пределах ×1.35 — E = 1 (привык), ярче —
; E = √(L0·1.35/Ls), темнее — (L0/1.35/Ls)^¼, в 0.6..1.3; к свету — быстро, к темноте —
; медленно
grade_update:
    ; в избе, где горит свет, глаз привыкает к нему: E ≤ 1 − 0.38·(свет в избах)
    movss   xmm4, [f_1]
    cvttss2si eax, [pl + E_X]
    cvttss2si ecx, [pl + E_Y]
    and     eax, MAPW - 1
    and     ecx, MAPW - 1
    shl     ecx, 7
    add     eax, ecx
    lea     rdx, [rmap]
    movzx   eax, byte [rdx + rax]
    test    eax, eax
    jz      .env
    lea     rdx, [lights]
    mov     ecx, [nlights]
.lt:
    test    ecx, ecx
    jz      .env
    cmp     dword [rdx + L_KIND], LK_ROOM
    jne     .ltn
    cmp     [rdx + L_HID], eax
    je      .lit
.ltn:
    add     rdx, L_SZ
    dec     ecx
    jmp     .lt
.lit:
    movss   xmm1, [lamp_t]
    mulss   xmm1, [gd_eroom]
    subss   xmm4, xmm1
.env:
    movss   xmm0, [ex_meter]
    comiss  xmm0, [f_0]
    jbe     .out
    divss   xmm0, [ex_e]                ; Ls
    movss   xmm1, [gd_par + GP_L0]
    mulss   xmm1, [gd_1_35]
    divss   xmm1, xmm0
    movss   xmm2, [f_1]
    comiss  xmm1, xmm2
    jae     .dark
    sqrtss  xmm2, xmm1                  ; ярче ключа
    jmp     .tgt
.dark:
    movss   xmm1, [gd_par + GP_L0]
    divss   xmm1, [gd_1_35]
    divss   xmm1, xmm0
    comiss  xmm1, xmm2
    jbe     .tgt                        ; привык: 1
    sqrtss  xmm1, xmm1
    sqrtss  xmm2, xmm1
.tgt:
    minss   xmm2, xmm4                  ; и не выше, чем позволяет свет избы
    maxss   xmm2, [gd_emin]
    minss   xmm2, [gd_emax]
    movss   xmm0, [ex_e]
    movss   xmm3, [gd_rdark]
    comiss  xmm2, xmm0
    jae     .r
    movss   xmm3, [gd_rlight]
.r:
    mulss   xmm3, [dt]
    minss   xmm3, [f_1]
    movaps  xmm1, xmm2
    subss   xmm1, xmm0
    mulss   xmm1, xmm3
    addss   xmm0, xmm1
    ; привык — ровно 1 (тоновая кривая до колена — тождество, AVX2 её не собирает)
    comiss  xmm2, [f_1]
    jne     .put
    movaps  xmm1, xmm0
    subss   xmm1, [f_1]
    andps   xmm1, [f_absmask]
    comiss  xmm1, [gd_snap]
    jae     .put
    movss   xmm0, [f_1]
.put:
    movss   [ex_e], xmm0
.out:
    ret

; ============================================================================
section .rdata
align 16, db 0
gd_yw:      dw 29, 150, 77, 0, 29, 150, 77, 0      ; яркость ·256: B, G, R
gd_m3:      dw -1, -1, -1, 0, -1, -1, -1, 0
align 4, db 0
gd_wb:      dd 0.11328125               ; 29/256
gd_wg:      dd 0.5859375                ; 150/256
gd_wr:      dd 0.30078125               ; 77/256
gd_bl0:     dd 0.3                      ; метель: снег 0.3 .. 0.7
gd_bl1:     dd 2.5
gd_dk0:     dd 17.7                     ; сумерки гаснут 17:42 .. 18:48
gd_dk1:     dd 0.9091
gd_dw0:     dd 30.3                     ; рассвет 06:18 .. 07:42
gd_dw1:     dd 0.7143
gd_cold_s:  dd 0.35
gd_cold_p:  dd 0.25
gd_k2:      dd 420.0
gd_2047:    dd 2047.0
gd_65536:   dd 65536.0
gd_65535:   dd 65535.0
gd_150:     dd 150.0
gd_i105:    dd 0.0095238
gd_140:     dd 140.0
gd_i115:    dd 0.0086957
gd_m255:    dd -255.0
gd_1_5:     dd 1.5
gd_64:      dd 64.0
gd_127:     dd 127.0
gd_ifull:   dd 4.3402778e-6             ; 1/(640·360)
gd_imid:    dd 1.7361111e-5             ; 1/(320·180)
gd_1_35:    dd 1.35
gd_emin:    dd 0.6
gd_emax:    dd 1.3
gd_rlight:  dd 3.0                      ; к свету, 1/с
gd_rdark:   dd 0.45                     ; к темноте
gd_snap:    dd 0.004
gd_eroom:   dd 0.38

; палитры: PK YP  NB NG NR  SAT HSAT  SHB SHG SHR YSH  HIB HIG HIR  GAM CON BLK L0  HLB HLG HLR HLK  PQ —
gp_night:   dd 0.40, 170.0,  1.60, 1.08, 0.62,  1.22, 1.12,  3.0, 0.0, -3.0, 90.0,  -3.0, 0.0, 3.0
            dd 0.52, 0.14, 0.0, 62.0,  12.0, 14.0, 17.0, 0.20,  0.30, 0.0
gp_bliz:    dd 0.35, 170.0,  1.30, 1.04, 0.80,  0.80, 0.75,  5.0, 4.0, 2.0, 130.0,  0.0, 0.0, 0.0
            dd 0.53, -0.12, 6.0, 80.0,  13.0, 14.0, 15.0, 0.15,  0.35, 0.0
gp_frost:   dd 0.55, 180.0,  1.75, 1.10, 0.55,  1.30, 1.15,  5.0, 1.0, -4.0, 100.0,  -5.0, 0.0, 6.0
            dd 0.50, 0.20, 0.0, 62.0,  13.0, 16.0, 22.0, 0.25,  0.25, 0.0
gp_dusk:    dd 0.15, 120.0,  1.40, 1.00, 0.80,  1.02, 0.98,  6.0, 0.0, 4.0, 100.0,  -5.0, 0.0, 6.0
            dd 0.50, 0.10, 1.0, 60.0,  12.0, 14.0, 17.0, 0.18,  0.10, 0.0
gp_dawn:    dd 0.25, 150.0,  1.45, 1.10, 0.70,  1.10, 1.05,  6.0, 4.0, 0.0, 110.0,  -2.0, 2.0, 8.0
            dd 0.50, 0.12, 1.0, 70.0,  12.0, 14.0, 17.0, 0.18,  0.20, 0.0
%if (gp_bliz - gp_night) != GP_SZ
  %error "палитра: не GP_N чисел"
%endif

section .data
align 32, db 0
tm_thr:     times 16 dw TM_K            ; AVX2: до какого слова кривая — тождество
hl_k2:      times 16 dw 0xFFFF          ; пересвет: порог (четвёртое слово — 0xFFFF)
hl_kk:      times 16 dw 0               ;           доля ·65536
bl_gainv:   dw 14, 14, 14, 0, 14, 14, 14, 0
align 4, db 0
tm_e:       dd 1.0                      ; экспозиция, под которую собрана tm_lut
ex_e:       dd 1.0                      ; экспозиция глаза
ex_meter:   dd 0.0                      ; яркость прошлого кадра (середина — втрое)
gd_h:       dd 0.0
gd_n:       dd 0.0, 0.0, 0.0, 0.0       ; оттенок ночного зрения − 1
gd_bk:      dd 0.0
gd_blk:     dd 0.0
gd_blk1:    dd 1.0
gd_par:     times GP_N dd 0.0           ; палитра кадра
gd_dump:    dd 0.0, 0.0, 0.0, 0.0       ; --shotN: E, замер, мороз, иней -> grade.bin
s_gradename: dw __utf16__('grade.bin'), 0

section .bss
alignb 16
gd_tab:     resq 256                    ; по яркости: O_B, O_G, O_R, S·64
gd_rows:    resq SCR_H                  ; яркость строк: вся, середина
