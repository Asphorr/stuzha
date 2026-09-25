; ----------------------------------------------------------------------------
;  asynth.inc — голоса сложнее слоёв audio.inc (тот же поток звука, те же уши и микс).
;  Зов (VK_CALL): связки — толчки потока раскрытия с дрожью периода, мерцанием силы и
;  подгармоникой (каждый второй период тише и длиннее), шум дыхания по раскрытию и
;  всплеском на согласной; четыре форманты пасти ползут между «закрыто» и «открыто»
;  по слогам; горло перегружается. Лай, вой, рык, петух.
;  Зёрна (VK_GRAIN): россыпь щелчков — частота и сила по нагрузке (пятка, перекат,
;  носок), каждый — всплеск шума в трёх резонансах со своим разбросом; и трение
;  рывками (скрип снега, половиц, звон цепи). Шаги, треск печи.
;  Всё случайное в таком голосе — от его зерна (EV_SEED): эхо — точная копия звука.
; ----------------------------------------------------------------------------
section .text

VK_CALL     equ 1
VK_GRAIN    equ 2

; зов: состояние голоса (поверх полей слоёв начиная с V_F1)
C_TAB       equ 128                     ; qword: описание
C_TS        equ 136                     ; прошло, с
C_END       equ 140
C_PS        equ 144                     ; высота (событие)
C_FS        equ 148                     ; форманты (за высотой)
C_RG        equ 152                     ; хрипота (событие)
C_PH        equ 156                     ; фаза связок
C_DF0       equ 160                     ; циклов за сэмпл по контуру
C_DF        equ 164                     ; ...в этом периоде
C_DN        equ 168                     ; поток -> звук: мерцание / шаг фазы
C_GP        equ 172                     ; поток прошлого сэмпла
C_PAR       equ 176                     ; чётность периода
C_SUB       equ 180                     ; подгармоника: глубина
C_SUBP      equ 184                     ;   сдвиг длины периода
C_NA        equ 188                     ; шум ровный
C_NG        equ 192                     ; шум по раскрытию
C_LP        equ 196                     ; ФНЧ источника
C_TC        equ 200
C_DRV       equ 204                     ; перегруз горла
C_DRF       equ 208                     ; блуждание высоты
C_AMP       equ 212
C_AT        equ 216                     ; громкость на конец куска
C_IOQ       equ 220                     ; 1 / доля раскрытия
C_TSC       equ 224                     ; темп этого голоса
C_PV        equ 228                     ; размах высоты этого голоса
C_F         equ 256                     ; 4 форманты по 64 байта (поля F_*)

; зёрна: состояние голоса
G_TAB       equ 128
G_TS        equ 136
G_A         equ 140                     ; сила (событие)
G_FS        equ 144                     ; высота (событие)
G_TK        equ 148                     ; темп
G_P         equ 152                     ; шанс зерна за сэмпл
G_GA        equ 156                     ; сила зерна
G_LVL       equ 160
G_DK        equ 164
G_SP        equ 168                     ; фаза трения
G_SD        equ 172
G_SD0       equ 176
G_SA        equ 180                     ; сила рывка
G_W1        equ 184                     ; веса резонансов (у каждого зерна свои)
G_W2        equ 188
G_W3        equ 192
G_XS        equ 196
G_DIR       equ 200
G_END       equ 204
G_LPC       equ 208                     ; ФНЧ щелчков: коэффициент, состояние
G_LPS       equ 212
G_F         equ 256                     ; 3 резонанса по 64 байта

; описание зова
CD_NSYL     equ 0                       ; int
CD_OQ       equ 4                       ; доля периода, что связки открыты
CD_TILT     equ 8                       ; ФНЧ источника, Гц (0 — нет)
CD_NOISE    equ 12                      ; шум дыхания
CD_NGATE    equ 16                      ; ...доля, что идёт по раскрытию
CD_JIT      equ 20                      ; дрожь периода
CD_SHIM     equ 24                      ; мерцание силы
CD_SUB      equ 28                      ; подгармоника
CD_DRV      equ 32                      ; перегруз
CD_VIBF     equ 36                      ; вибрато, Гц
CD_VIBD     equ 40
CD_DRIFT    equ 44                      ; блуждание высоты
CD_RUF      equ 48                      ; хрип: АМ, Гц
CD_RUD      equ 52
CD_FEXP     equ 56                      ; форманты за высотой: доля
CD_GAIN     equ 60
CD_F        equ 64                      ; 4 x (закрыто Гц, открыто Гц, Q, громкость)
CD_VART     equ 128                     ; разброс по голосам: длины слогов, ±доля
CD_VARP     equ 132                     ;   размаха высоты, ±доля
CD_SYL      equ 136
; слог
SY_T0       equ 0
SY_DUR      equ 4
SY_F        equ 8                       ; высота: начало, вершина, конец, где вершина
SY_AMP      equ 24
SY_ATK      equ 28
SY_REL      equ 32
SY_DEC      equ 36                      ; спад силы к концу слога, доля
SY_M        equ 40                      ; рот: начало, вершина, конец, где
SY_BST      equ 56                      ; всплеск шума на согласной
SY_BTAU     equ 60
SY_NZ       equ 64                      ; шум слога ×
SY_SB       equ 68                      ; подгармоника слога ×
SYSZ        equ 72

; описание зёрен
GD_DUR      equ 0
GD_HT       equ 4                       ; пятка: где, ширина, зёрен/с, сила
GD_HW       equ 8
GD_HR       equ 12
GD_HA       equ 16
GD_TT       equ 20                      ; носок
GD_TW       equ 24
GD_TR       equ 28
GD_TA       equ 32
GD_BR       equ 36                      ; перекат между ними
GD_BA       equ 40
GD_KP       equ 44                      ; int: сила зерна u^KP — мелких много, крупных мало
GD_GD       equ 48                      ; зерно гаснет, τ с
GD_SS       equ 52                      ; трение: сила рывка (0 — нет)
GD_SF       equ 56                      ; ...рывков в секунду: начало, вершина, конец, где
GD_SJ       equ 72                      ; ...дрожь
GD_WR       equ 76                      ; разброс весов резонансов по зёрнам
GD_DIR      equ 80                      ; щелчок мимо резонансов
GD_GAIN     equ 84
GD_ISP      equ 88                      ; сила ускоряет: темп 1 + ISP·(сила − 1)
GD_R        equ 92                      ; 3 x (Гц, Q, громкость, разброс)
GD_LP       equ 140                     ; ФНЧ на щелчки, Гц (0 — нет)
GDSZ        equ 144

; xorshift в r8d -> %1 = [−1, 1); портит eax
%macro KRND 1
    mov     eax, r8d
    shl     eax, 13
    xor     r8d, eax
    mov     eax, r8d
    shr     eax, 17
    xor     r8d, eax
    mov     eax, r8d
    shl     eax, 5
    xor     r8d, eax
    cvtsi2ss %1, r8d
    mulss   %1, [ac_2m31]
%endmacro

; сэмпл в xmm1 -> уши (ФНЧ: левое xmm5, правое xmm4) -> mixL/mixR/mixV; портит xmm1, xmm2
%macro KTAIL 0
    movaps  xmm2, xmm1
    subss   xmm2, xmm5
    mulss   xmm2, [rbx + V_CL]
    addss   xmm5, xmm2
    movaps  xmm2, xmm1
    subss   xmm2, xmm4
    mulss   xmm2, [rbx + V_CR]
    addss   xmm4, xmm2
    movaps  xmm2, xmm5
    mulss   xmm2, [rbx + V_GL]
    addss   xmm2, [r9]
    movss   [r9], xmm2
    movaps  xmm2, xmm4
    mulss   xmm2, [rbx + V_GR]
    addss   xmm2, [r10]
    movss   [r10], xmm2
    mulss   xmm1, [rbx + V_GV]
    addss   xmm1, [r11]
    movss   [r11], xmm1
    add     r9, 4
    add     r10, 4
    add     r11, 4
%endmacro

; ---------------------------------------------------------------- общее
; rbx = голос, rsi = событие, xmm0 = громкость звука: зерно, задержка, уши
kv_common:
    sub     rsp, 40
    movss   [rsp + 32], xmm0
    mov     eax, [rsi + EV_SEED]
    test    eax, eax
    jnz     .sd
    call    a_rnd
.sd:
    or      eax, 1
    mov     [rbx + V_NS], eax
    movss   xmm0, [rsi + EV_DEL]
    mulss   xmm0, [ac_fs]
    cvttss2si eax, xmm0
    mov     [rbx + V_DEL], eax
    movss   xmm0, [rsp + 32]
    mulss   xmm0, [rsi + EV_VOL]
    movss   xmm1, xmm0
    mulss   xmm1, [rsi + EV_GL]
    movss   [rbx + V_GL], xmm1
    movss   xmm1, xmm0
    mulss   xmm1, [rsi + EV_GR]
    movss   [rbx + V_GR], xmm1
    mulss   xmm0, [rsi + EV_RV]
    movss   [rbx + V_GV], xmm0
    mov     eax, [rsi + EV_CL]
    mov     [rbx + V_CL], eax
    mov     eax, [rsi + EV_CR]
    mov     [rbx + V_CR], eax
    ; сила/хрипота события: 0 — не задана
    movss   xmm0, [rsi + EV_A]
    comiss  xmm0, [ac_0]
    ja      .a
    movss   xmm0, [ac_1]
.a:
    movss   [rsp + 36], xmm0
    mov     eax, [rsp + 36]
    add     rsp, 40
    ret                                 ; eax = сила (float)

; rbx = голос, rdi = описание, ecx = вид: голос чист, вид и описание записаны
kv_clear:
    push    rdi
    push    rcx
    mov     rdi, rbx
    xor     eax, eax
    mov     ecx, VSZ/8
    rep     stosq
    pop     rcx
    pop     rdi
    mov     eax, [rsi + EV_SND]
    inc     eax
    mov     [rbx + V_ON], eax
    mov     [rbx + V_KIND], ecx
    mov     dword [rbx + V_ENV], __float32__(1.0)   ; для voice_alloc: звучит
    mov     [rbx + C_TAB], rdi
    ret

; rcx -> (a, b, c, p), xmm0 = τ 0..1 -> xmm0: a -> b за [0, p], b -> c за [p, 1], плавно
; (u²(3 − 2u)). Портит xmm1-3
k_contour:
    movss   xmm1, [rcx + 12]
    comiss  xmm0, xmm1
    jae     .hi
    divss   xmm0, xmm1
    movss   xmm2, [rcx]
    movss   xmm3, [rcx + 4]
    jmp     .s
.hi:
    subss   xmm0, xmm1
    movss   xmm2, [ac_1]
    subss   xmm2, xmm1
    maxss   xmm2, [ac_1em4]
    divss   xmm0, xmm2
    movss   xmm2, [rcx + 4]
    movss   xmm3, [rcx + 8]
.s:
    maxss   xmm0, [ac_0]
    minss   xmm0, [ac_1]
    movaps  xmm1, xmm0
    addss   xmm1, xmm1
    subss   xmm1, [ac_3]
    mulss   xmm1, xmm0
    mulss   xmm0, xmm1
    xorps   xmm0, [ac_sign]
    subss   xmm3, xmm2
    mulss   xmm0, xmm3
    addss   xmm0, xmm2
    ret

; xmm0 = t, rcx -> (центр, ширина) -> xmm0 = горб: u²(3 − 2u), u = 1 − |t − центр| / ширина.
; Портит xmm1
k_bump:
    subss   xmm0, [rcx]
    andps   xmm0, [ac_abs]
    divss   xmm0, [rcx + 4]
    movss   xmm1, [ac_1]
    subss   xmm1, xmm0
    maxss   xmm1, [ac_0]
    movaps  xmm0, xmm1
    addss   xmm1, xmm1
    subss   xmm1, [ac_3]
    mulss   xmm1, xmm0
    mulss   xmm0, xmm1
    xorps   xmm0, [ac_sign]
    ret

; ---------------------------------------------------------------- зов
; rbx = голос, rsi = событие, rdi = описание зова
call_fill:
    push    r12
    sub     rsp, 32
    mov     r12, rdi
    mov     ecx, VK_CALL
    call    kv_clear
    movss   xmm0, [r12 + CD_GAIN]
    call    kv_common
    mov     [rbx + C_RG], eax
    movss   xmm0, [rsi + EV_P]
    movss   [rbx + C_PS], xmm0
    subss   xmm0, [ac_1]
    mulss   xmm0, [r12 + CD_FEXP]
    addss   xmm0, [ac_1]
    movss   [rbx + C_FS], xmm0
    movss   xmm0, [ac_1]
    divss   xmm0, [r12 + CD_OQ]
    movss   [rbx + C_IOQ], xmm0
    ; ФНЧ источника: 1 − e^(−2π fc/fs)
    mov     dword [rbx + C_TC], __float32__(1.0)
    movss   xmm0, [r12 + CD_TILT]
    comiss  xmm0, [ac_0]
    jbe     .nt
    mulss   xmm0, [ac_m2pi_fs]
    call    a_exp
    movss   xmm1, [ac_1]
    subss   xmm1, xmm0
    movss   [rbx + C_TC], xmm1
.nt:
    mov     eax, [r12 + CD_DRV]
    mov     [rbx + C_DRV], eax
    ; конец: последний слог и отзвук формант
    mov     eax, [r12 + CD_NSYL]
    dec     eax
    imul    eax, eax, SYSZ
    movss   xmm0, [r12 + CD_SYL + rax + SY_T0]
    addss   xmm0, [r12 + CD_SYL + rax + SY_DUR]
    addss   xmm0, [ac_0_15]
    movss   [rbx + C_END], xmm0
    ; форманты: полосы с единичной вершиной
    xor     ecx, ecx
.fm:
    mov     eax, ecx
    shl     eax, 6
    lea     rdx, [rbx + C_F + rax]
    mov     dword [rdx + F_MD], 2
    mov     eax, ecx
    shl     eax, 4
    movss   xmm0, [ac_1]
    divss   xmm0, [r12 + CD_F + rax + 8]
    movss   [rdx + F_K], xmm0
    mov     eax, [r12 + CD_F + rax + 12]
    mov     [rdx + F_G], eax
    inc     ecx
    cmp     ecx, 4
    jb      .fm
    ; фаза связок — случайная; свой темп и размах высоты (два «гав» подряд не близнецы);
    ; цели первого куска
    mov     r8d, [rbx + V_NS]
    KRND    xmm0
    andps   xmm0, [ac_abs]
    movss   [rbx + C_PH], xmm0
    KRND    xmm0
    mulss   xmm0, [r12 + CD_VART]
    addss   xmm0, [ac_1]
    movss   xmm1, [ac_1]
    divss   xmm1, xmm0
    movss   [rbx + C_TSC], xmm1
    KRND    xmm0
    mulss   xmm0, [r12 + CD_VARP]
    addss   xmm0, [ac_1]
    movss   [rbx + C_PV], xmm0
    ; высота до первого слога (он может начаться не сразу) — его начальная
    movss   xmm0, [r12 + CD_SYL + SY_F]
    mulss   xmm0, [rbx + C_PS]
    mulss   xmm0, [ac_inv_fs]
    minss   xmm0, [ac_0_45]
    maxss   xmm0, [ak_dfmin]
    movss   [rbx + C_DF0], xmm0
    call    call_chunk
    movss   xmm0, [rbx + C_DF0]
    movss   [rbx + C_DF], xmm0
    movss   xmm1, [ak_dnk]
    divss   xmm1, xmm0
    movss   [rbx + C_DN], xmm1
    mov     [rbx + V_NS], r8d
    add     rsp, 32
    pop     r12
    ret

; rbx = голос-зов, r8d — его ГСЧ: по времени C_TS — цели куска: высота C_DF0, громкость
; C_AT, шумы, подгармоника, форманты. Портит rax, rcx, rdx, xmm0-3
call_chunk:
    push    r12
    push    r13
    sub     rsp, 56
    mov     r12, [rbx + C_TAB]
    ; слог, где сейчас время (слоги по порядку)
    lea     r13, [r12 + CD_SYL]
    mov     ecx, [r12 + CD_NSYL]
    movss   xmm0, [rbx + C_TS]
.f:
    movaps  xmm1, xmm0
    subss   xmm1, [r13 + SY_T0]
    comiss  xmm1, [ac_0]
    jb      .gap
    comiss  xmm1, [r13 + SY_DUR]
    jb      .syl
    add     r13, SYSZ
    dec     ecx
    jnz     .f
.gap:
    ; между слогами — тихо (форманты дозвенят), высота держится
    xorps   xmm0, xmm0
    movss   [rbx + C_AT], xmm0
    movss   [rbx + C_NA], xmm0
    movss   [rbx + C_NG], xmm0
    jmp     .out
.syl:
    movss   [rsp + 32], xmm1            ; с начала слога, с
    divss   xmm1, [r13 + SY_DUR]
    movss   [rsp + 36], xmm1            ; τ
    ; --- высота: контур · событие · (1 + вибрато + блуждание)
    movss   xmm0, [rbx + C_TS]
    mulss   xmm0, [r12 + CD_VIBF]
    mulss   xmm0, [ac_2pi]
    call    fsin
    mulss   xmm0, [r12 + CD_VIBD]
    movss   [rsp + 40], xmm0
    KRND    xmm0
    mulss   xmm0, [r12 + CD_DRIFT]
    subss   xmm0, [rbx + C_DRF]
    mulss   xmm0, [ac_0_05]
    addss   xmm0, [rbx + C_DRF]
    movss   [rbx + C_DRF], xmm0
    addss   xmm0, [rsp + 40]
    addss   xmm0, [ac_1]
    movss   [rsp + 40], xmm0
    lea     rcx, [r13 + SY_F]
    movss   xmm0, [rsp + 36]
    call    k_contour
    subss   xmm0, [r13 + SY_F]
    mulss   xmm0, [rbx + C_PV]
    addss   xmm0, [r13 + SY_F]
    mulss   xmm0, [rsp + 40]
    mulss   xmm0, [rbx + C_PS]
    mulss   xmm0, [ac_inv_fs]
    minss   xmm0, [ac_0_45]
    maxss   xmm0, [ak_dfmin]
    movss   [rbx + C_DF0], xmm0
    ; --- громкость: подъём · спад · (1 − DEC·τ) · хрип
    movss   xmm0, [rsp + 32]
    divss   xmm0, [r13 + SY_ATK]
    call    sstep
    movss   [rsp + 44], xmm0
    movss   xmm0, [r13 + SY_DUR]
    subss   xmm0, [rsp + 32]
    divss   xmm0, [r13 + SY_REL]
    call    sstep
    mulss   xmm0, [rsp + 44]
    movss   xmm1, [rsp + 36]
    mulss   xmm1, [r13 + SY_DEC]
    movss   xmm2, [ac_1]
    subss   xmm2, xmm1
    mulss   xmm0, xmm2
    mulss   xmm0, [r13 + SY_AMP]
    movss   [rsp + 44], xmm0
    movss   xmm0, [rbx + C_TS]
    mulss   xmm0, [r12 + CD_RUF]
    mulss   xmm0, [ac_2pi]
    call    fsin
    mulss   xmm0, [ac_0_5]
    addss   xmm0, [ac_0_5]
    mulss   xmm0, [r12 + CD_RUD]
    mulss   xmm0, [rbx + C_RG]
    minss   xmm0, [ac_1]
    movss   xmm1, [ac_1]
    subss   xmm1, xmm0
    mulss   xmm1, [rsp + 44]
    movss   [rbx + C_AT], xmm1
    ; --- шум: ровный + по раскрытию; всплеск на согласной в начале слога
    movss   xmm0, [rsp + 32]
    xorps   xmm0, [ac_sign]
    divss   xmm0, [r13 + SY_BTAU]
    call    a_exp
    mulss   xmm0, [r13 + SY_BST]
    movss   [rsp + 48], xmm0
    movss   xmm0, [r12 + CD_NOISE]
    mulss   xmm0, [r13 + SY_NZ]
    mulss   xmm0, [rbx + C_RG]
    movss   xmm1, xmm0
    mulss   xmm1, [r12 + CD_NGATE]
    addss   xmm1, xmm1
    movss   [rbx + C_NG], xmm1
    movss   xmm1, [ac_1]
    subss   xmm1, [r12 + CD_NGATE]
    mulss   xmm0, xmm1
    addss   xmm0, [rsp + 48]
    movss   [rbx + C_NA], xmm0
    ; --- подгармоника
    movss   xmm0, [r12 + CD_SUB]
    mulss   xmm0, [r13 + SY_SB]
    mulss   xmm0, [rbx + C_RG]
    minss   xmm0, [ac_0_9]
    movss   [rbx + C_SUB], xmm0
    mulss   xmm0, [ac_0_08]
    movss   [rbx + C_SUBP], xmm0
    ; --- рот -> форманты
    lea     rcx, [r13 + SY_M]
    movss   xmm0, [rsp + 36]
    call    k_contour
    movss   [rsp + 48], xmm0
    xor     ecx, ecx
.fm:
    mov     eax, ecx
    shl     eax, 4
    movss   xmm0, [r12 + CD_F + rax + 4]
    subss   xmm0, [r12 + CD_F + rax]
    mulss   xmm0, [rsp + 48]
    addss   xmm0, [r12 + CD_F + rax]
    mulss   xmm0, [rbx + C_FS]
    mov     eax, ecx
    shl     eax, 6
    lea     rdx, [rbx + C_F + rax]
    movss   [rdx + F_FC], xmm0
    call    vf_coef                     ; rcx не трогает
    inc     ecx
    cmp     ecx, 4
    jb      .fm
.out:
    add     rsp, 56
    pop     r13
    pop     r12
    ret

; rbx = голос-зов, r13d = с какого сэмпла блока
call_run:
    push    r12
    push    r13
    push    r14
    sub     rsp, 32
    mov     r12, [rbx + C_TAB]
    mov     r8d, [rbx + V_NS]
    movss   xmm13, [rbx + C_F + F_I1]
    movss   xmm12, [rbx + C_F + F_I2]
    movss   xmm11, [rbx + C_F + 64 + F_I1]
    movss   xmm10, [rbx + C_F + 64 + F_I2]
    movss   xmm9, [rbx + C_F + 128 + F_I1]
    movss   xmm8, [rbx + C_F + 128 + F_I2]
    movss   xmm7, [rbx + C_F + 192 + F_I1]
    movss   xmm6, [rbx + C_F + 192 + F_I2]
    movss   xmm5, [rbx + V_SL]
    movss   xmm4, [rbx + V_SR]
    movss   xmm15, [rbx + C_AMP]
.sub:
    cmp     r13d, ABLK
    jae     .end
    mov     r14d, ABLK
    sub     r14d, r13d
    mov     eax, ASUB
    cmp     r14d, eax
    cmova   r14d, eax
    call    call_chunk                  ; xmm4-15 не трогает
    movss   xmm14, [rbx + C_AT]
    subss   xmm14, xmm15
    cvtsi2ss xmm0, r14d
    divss   xmm14, xmm0
    lea     rax, [mixL]
    lea     r9, [rax + r13*4]
    lea     r10, [rax + r13*4 + ABLK*4]
    lea     r11, [rax + r13*4 + ABLK*8]
    mov     ecx, r14d
.s:
    ; связки: фаза; новый период — своя дрожь, мерцание, подгармоника
    movss   xmm1, [rbx + C_PH]
    addss   xmm1, [rbx + C_DF]
    comiss  xmm1, [ac_1]
    jb      .ph
    subss   xmm1, [ac_1]
    movss   [rbx + C_PH], xmm1
    KRND    xmm0
    mulss   xmm0, [r12 + CD_JIT]
    addss   xmm0, [ac_1]
    movss   xmm3, [ac_1]
    movss   xmm2, [rbx + C_SUBP]
    xor     dword [rbx + C_PAR], 1
    jz      .ev
    subss   xmm3, [rbx + C_SUB]         ; нечётный — тише и длиннее
    xorps   xmm2, [ac_sign]
.ev:
    addss   xmm2, [ac_1]
    mulss   xmm0, xmm2
    mulss   xmm0, [rbx + C_DF0]
    movss   [rbx + C_DF], xmm0
    KRND    xmm2
    mulss   xmm2, [r12 + CD_SHIM]
    addss   xmm2, [ac_1]
    mulss   xmm3, xmm2
    mulss   xmm3, [ak_dnk]
    divss   xmm3, xmm0
    movss   [rbx + C_DN], xmm3
    movss   xmm1, [rbx + C_PH]
.ph:
    movss   [rbx + C_PH], xmm1
    ; поток раскрытия g(φ): подъём u²(3 − 2u) до 0.66 раскрытия, спад cos(πv/2)
    mulss   xmm1, [rbx + C_IOQ]
    xorps   xmm2, xmm2
    comiss  xmm1, [ac_1]
    jae     .gd
    comiss  xmm1, [ak_0_66]
    jae     .fall
    mulss   xmm1, [ak_i066]
    movaps  xmm2, xmm1
    addss   xmm2, xmm2
    movss   xmm3, [ac_3]
    subss   xmm3, xmm2
    mulss   xmm3, xmm1
    mulss   xmm3, xmm1
    movaps  xmm2, xmm3
    jmp     .gd
.fall:
    subss   xmm1, [ak_0_66]
    mulss   xmm1, [ak_i034]
    mulss   xmm1, xmm1                  ; v²
    movss   xmm2, [ak_c6]
    mulss   xmm2, xmm1
    addss   xmm2, [ak_c4]
    mulss   xmm2, xmm1
    addss   xmm2, [ak_c2]
    mulss   xmm2, xmm1
    addss   xmm2, [ac_1]
    maxss   xmm2, [ac_0]
.gd:
    ; толчок = приращение потока (излучение ртом), шум дыхания — по раскрытию
    movaps  xmm3, xmm2
    subss   xmm3, [rbx + C_GP]
    movss   [rbx + C_GP], xmm2
    mulss   xmm3, [rbx + C_DN]
    KRND    xmm0
    mulss   xmm2, [rbx + C_NG]
    addss   xmm2, [rbx + C_NA]
    mulss   xmm0, xmm2
    addss   xmm0, xmm3
    mulss   xmm0, xmm15
    movss   xmm1, [rbx + C_LP]
    subss   xmm0, xmm1
    mulss   xmm0, [rbx + C_TC]
    addss   xmm0, xmm1
    movss   [rbx + C_LP], xmm0
    xorps   xmm1, xmm1
    VSVF    C_F, xmm13, xmm12
    VSVF    C_F + 64, xmm11, xmm10
    VSVF    C_F + 128, xmm9, xmm8
    VSVF    C_F + 192, xmm7, xmm6
    ; горло перегружено: y / (1 + d·|y|)
    movaps  xmm2, xmm1
    andps   xmm2, [ac_abs]
    mulss   xmm2, [rbx + C_DRV]
    addss   xmm2, [ac_1]
    divss   xmm1, xmm2
    KTAIL
    addss   xmm15, xmm14
    dec     ecx
    jnz     .s
    cvtsi2ss xmm0, r14d
    mulss   xmm0, [ac_inv_fs]
    mulss   xmm0, [rbx + C_TSC]
    addss   xmm0, [rbx + C_TS]
    movss   [rbx + C_TS], xmm0
    add     r13d, r14d
    jmp     .sub
.end:
    movss   [rbx + C_F + F_I1], xmm13
    movss   [rbx + C_F + F_I2], xmm12
    movss   [rbx + C_F + 64 + F_I1], xmm11
    movss   [rbx + C_F + 64 + F_I2], xmm10
    movss   [rbx + C_F + 128 + F_I1], xmm9
    movss   [rbx + C_F + 128 + F_I2], xmm8
    movss   [rbx + C_F + 192 + F_I1], xmm7
    movss   [rbx + C_F + 192 + F_I2], xmm6
    movss   [rbx + V_SL], xmm5
    movss   [rbx + V_SR], xmm4
    movss   [rbx + C_AMP], xmm15
    mov     [rbx + V_NS], r8d
    ucomiss xmm5, xmm4                  ; NaN в ушах — голос долой (реверб чинится сам)
    jp      .kill
    movss   xmm0, [rbx + C_TS]
    comiss  xmm0, [rbx + C_END]
    jb      .ret
.kill:
    mov     dword [rbx + V_ON], 0
.ret:
    add     rsp, 32
    pop     r14
    pop     r13
    pop     r12
    ret

; ---------------------------------------------------------------- зёрна
; rbx = голос, rsi = событие, rdi = описание зёрен
grain_fill:
    push    r12
    sub     rsp, 32
    mov     r12, rdi
    mov     ecx, VK_GRAIN
    call    kv_clear
    movss   xmm0, [r12 + GD_GAIN]
    call    kv_common
    mov     [rbx + G_A], eax
    movd    xmm0, eax
    subss   xmm0, [ac_1]
    mulss   xmm0, [r12 + GD_ISP]
    addss   xmm0, [ac_1]
    maxss   xmm0, [ac_0_3]
    movss   [rbx + G_TK], xmm0
    mov     eax, [rsi + EV_P]
    mov     [rbx + G_FS], eax
    ; зерно гаснет: e^(−1/(τ·fs)) за сэмпл
    movss   xmm0, [r12 + GD_GD]
    mulss   xmm0, [ac_fs]
    maxss   xmm0, [ac_1]
    movss   xmm1, [ac_m1]
    divss   xmm1, xmm0
    movaps  xmm0, xmm1
    call    a_exp
    movss   [rbx + G_DK], xmm0
    mov     eax, [r12 + GD_DIR]
    mov     [rbx + G_DIR], eax
    mov     dword [rbx + G_LPC], __float32__(1.0)
    movss   xmm0, [r12 + GD_LP]
    comiss  xmm0, [ac_0]
    jbe     .nlp
    mulss   xmm0, [ac_m2pi_fs]
    call    a_exp
    movss   xmm1, [ac_1]
    subss   xmm1, xmm0
    movss   [rbx + G_LPC], xmm1
.nlp:
    movss   xmm0, [r12 + GD_DUR]
    addss   xmm0, [ac_0_1]
    movss   [rbx + G_END], xmm0
    mov     eax, __float32__(1.0)
    mov     [rbx + G_W1], eax
    mov     [rbx + G_W2], eax
    mov     [rbx + G_W3], eax
    xor     ecx, ecx
.r:
    mov     eax, ecx
    shl     eax, 6
    lea     rdx, [rbx + G_F + rax]
    mov     dword [rdx + F_MD], 2
    mov     eax, ecx
    shl     eax, 4
    movss   xmm0, [ac_1]
    divss   xmm0, [r12 + GD_R + rax + 4]
    movss   [rdx + F_K], xmm0
    mov     eax, [r12 + GD_R + rax + 8]
    mov     [rdx + F_G], eax
    inc     ecx
    cmp     ecx, 3
    jb      .r
    mov     r8d, [rbx + V_NS]
    KRND    xmm0
    andps   xmm0, [ac_abs]
    movss   [rbx + G_SP], xmm0
    call    grain_chunk
    mov     eax, [rbx + G_SD0]
    mov     [rbx + G_SD], eax
    mov     [rbx + V_NS], r8d
    add     rsp, 32
    pop     r12
    ret

; rbx = голос-зёрна, r8d — его ГСЧ: по времени — шанс и сила зёрен, трение, резонансы.
; Портит rax, rcx, rdx, xmm0-3
grain_chunk:
    push    r12
    sub     rsp, 48
    mov     r12, [rbx + G_TAB]
    movss   xmm0, [rbx + G_TS]
    mulss   xmm0, [rbx + G_TK]
    movss   [rsp + 32], xmm0            ; t в своём темпе
    lea     rcx, [r12 + GD_HT]
    call    k_bump
    movss   [rsp + 36], xmm0            ; пятка
    movss   xmm0, [rsp + 32]
    lea     rcx, [r12 + GD_TT]
    call    k_bump
    movss   [rsp + 40], xmm0            ; носок
    xorps   xmm2, xmm2
    movss   xmm0, [rsp + 32]
    comiss  xmm0, [r12 + GD_HT]
    jb      .nb
    comiss  xmm0, [r12 + GD_TT]
    ja      .nb
    movss   xmm2, [ac_1]
.nb:
    movss   [rsp + 44], xmm2            ; перекат
    ; зёрен в секунду -> шанс за сэмпл
    movss   xmm0, [rsp + 36]
    mulss   xmm0, [r12 + GD_HR]
    movss   xmm1, [rsp + 40]
    mulss   xmm1, [r12 + GD_TR]
    addss   xmm0, xmm1
    movss   xmm1, [rsp + 44]
    mulss   xmm1, [r12 + GD_BR]
    addss   xmm0, xmm1
    mulss   xmm0, [rbx + G_A]
    mulss   xmm0, [rbx + G_TK]
    mulss   xmm0, [ac_inv_fs]
    minss   xmm0, [ac_0_3]
    movss   [rbx + G_P], xmm0
    ; нагрузка -> сила зерна и рывка трения
    movss   xmm0, [rsp + 36]
    mulss   xmm0, [r12 + GD_HA]
    movss   xmm1, [rsp + 40]
    mulss   xmm1, [r12 + GD_TA]
    addss   xmm0, xmm1
    movss   xmm1, [rsp + 44]
    mulss   xmm1, [r12 + GD_BA]
    addss   xmm0, xmm1
    mulss   xmm0, [rbx + G_A]
    movss   [rbx + G_GA], xmm0
    mulss   xmm0, [r12 + GD_SS]
    movss   [rbx + G_SA], xmm0
    movss   xmm0, [rsp + 32]
    divss   xmm0, [r12 + GD_DUR]
    minss   xmm0, [ac_1]
    lea     rcx, [r12 + GD_SF]
    call    k_contour
    mulss   xmm0, [rbx + G_FS]
    mulss   xmm0, [ac_inv_fs]
    minss   xmm0, [ac_0_45]
    movss   [rbx + G_SD0], xmm0
    movss   xmm0, [rsp + 32]
    comiss  xmm0, [r12 + GD_DUR]
    jb      .res
    xorps   xmm0, xmm0
    movss   [rbx + G_P], xmm0
    movss   [rbx + G_SA], xmm0
.res:
    ; резонансы: свой разброс на каждый кусок
    xor     ecx, ecx
.r:
    KRND    xmm0
    mov     eax, ecx
    shl     eax, 4
    mulss   xmm0, [r12 + GD_R + rax + 12]
    addss   xmm0, [ac_1]
    mulss   xmm0, [r12 + GD_R + rax]
    mulss   xmm0, [rbx + G_FS]
    mov     eax, ecx
    shl     eax, 6
    lea     rdx, [rbx + G_F + rax]
    movss   [rdx + F_FC], xmm0
    call    vf_coef
    inc     ecx
    cmp     ecx, 3
    jb      .r
    add     rsp, 48
    pop     r12
    ret

; rbx = голос-зёрна, r13d = с какого сэмпла блока
grain_run:
    push    r12
    push    r13
    push    r14
    sub     rsp, 32
    mov     r12, [rbx + G_TAB]
    mov     r8d, [rbx + V_NS]
    movss   xmm13, [rbx + G_F + F_I1]
    movss   xmm12, [rbx + G_F + F_I2]
    movss   xmm11, [rbx + G_F + 64 + F_I1]
    movss   xmm10, [rbx + G_F + 64 + F_I2]
    movss   xmm9, [rbx + G_F + 128 + F_I1]
    movss   xmm8, [rbx + G_F + 128 + F_I2]
    movss   xmm5, [rbx + V_SL]
    movss   xmm4, [rbx + V_SR]
    movss   xmm15, [rbx + G_LVL]
    movss   xmm14, [rbx + G_DK]
    movss   xmm6, [rbx + G_SP]
    movss   xmm7, [rbx + G_SD]
.sub:
    cmp     r13d, ABLK
    jae     .end
    mov     r14d, ABLK
    sub     r14d, r13d
    mov     eax, ASUB
    cmp     r14d, eax
    cmova   r14d, eax
    call    grain_chunk
    lea     rax, [mixL]
    lea     r9, [rax + r13*4]
    lea     r10, [rax + r13*4 + ABLK*4]
    lea     r11, [rax + r13*4 + ABLK*8]
    mov     ecx, r14d
.s:
    KRND    xmm0                        ; шум
    mov     eax, r8d                    ; зерно? — старшие биты того же числа
    shr     eax, 8
    cvtsi2ss xmm1, eax
    mulss   xmm1, [ac_2m24]
    comiss  xmm1, [rbx + G_P]
    jae     .ng
    ; зерно: сила u^KP, свои веса резонансов
    KRND    xmm1
    andps   xmm1, [ac_abs]
    movaps  xmm2, xmm1
    mov     eax, [r12 + GD_KP]
.kp:
    dec     eax
    jle     .kd
    mulss   xmm1, xmm2
    jmp     .kp
.kd:
    mulss   xmm1, [rbx + G_GA]
    addss   xmm15, xmm1
    KRND    xmm1
    andps   xmm1, [ac_abs]
    mulss   xmm1, [r12 + GD_WR]
    movss   xmm2, [ac_1]
    subss   xmm2, xmm1
    movss   [rbx + G_W1], xmm2
    KRND    xmm1
    andps   xmm1, [ac_abs]
    mulss   xmm1, [r12 + GD_WR]
    movss   xmm2, [ac_1]
    subss   xmm2, xmm1
    movss   [rbx + G_W2], xmm2
    KRND    xmm1
    andps   xmm1, [ac_abs]
    mulss   xmm1, [r12 + GD_WR]
    movss   xmm2, [ac_1]
    subss   xmm2, xmm1
    movss   [rbx + G_W3], xmm2
.ng:
    mulss   xmm0, xmm15
    mulss   xmm15, xmm14
    ; трение: рывок на обороте фазы, следующий — с дрожью
    addss   xmm6, xmm7
    comiss  xmm6, [ac_1]
    jb      .nss
    subss   xmm6, [ac_1]
    KRND    xmm1
    movaps  xmm2, xmm1
    mulss   xmm1, [r12 + GD_SJ]
    addss   xmm1, [ac_1]
    movss   xmm7, [rbx + G_SD0]
    mulss   xmm7, xmm1
    mulss   xmm2, [ac_0_3]
    addss   xmm2, [ac_1]
    mulss   xmm2, [rbx + G_SA]
    addss   xmm0, xmm2
.nss:
    movss   xmm1, [rbx + G_LPS]
    subss   xmm0, xmm1
    mulss   xmm0, [rbx + G_LPC]
    addss   xmm0, xmm1
    movss   [rbx + G_LPS], xmm0
    movss   [rbx + G_XS], xmm0
    xorps   xmm1, xmm1
    mulss   xmm0, [rbx + G_W1]
    VSVF    G_F, xmm13, xmm12
    movss   xmm0, [rbx + G_XS]
    mulss   xmm0, [rbx + G_W2]
    VSVF    G_F + 64, xmm11, xmm10
    movss   xmm0, [rbx + G_XS]
    mulss   xmm0, [rbx + G_W3]
    VSVF    G_F + 128, xmm9, xmm8
    movss   xmm2, [rbx + G_XS]
    mulss   xmm2, [rbx + G_DIR]
    addss   xmm1, xmm2
    KTAIL
    dec     ecx
    jnz     .s
    cvtsi2ss xmm0, r14d
    mulss   xmm0, [ac_inv_fs]
    addss   xmm0, [rbx + G_TS]
    movss   [rbx + G_TS], xmm0
    add     r13d, r14d
    jmp     .sub
.end:
    movss   [rbx + G_F + F_I1], xmm13
    movss   [rbx + G_F + F_I2], xmm12
    movss   [rbx + G_F + 64 + F_I1], xmm11
    movss   [rbx + G_F + 64 + F_I2], xmm10
    movss   [rbx + G_F + 128 + F_I1], xmm9
    movss   [rbx + G_F + 128 + F_I2], xmm8
    movss   [rbx + V_SL], xmm5
    movss   [rbx + V_SR], xmm4
    movss   [rbx + G_LVL], xmm15
    movss   [rbx + G_SP], xmm6
    movss   [rbx + G_SD], xmm7
    mov     [rbx + V_NS], r8d
    ucomiss xmm5, xmm4
    jp      .kill
    movss   xmm0, [rbx + G_TS]
    mulss   xmm0, [rbx + G_TK]
    comiss  xmm0, [rbx + G_END]
    jb      .ret
.kill:
    mov     dword [rbx + V_ON], 0
.ret:
    add     rsp, 32
    pop     r14
    pop     r13
    pop     r12
    ret

; ================================================================ описания
%macro CALLD 16
    dd %1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14, %15, %16
%endmacro
%macro FORM 4
    dd %1, %2, %3, %4
%endmacro
; начало, длина; высота: начало, вершина, конец, где; сила, подъём, спад, убыль; рот: начало,
; вершина, конец, где; всплеск шума, его τ; шум ×, подгармоника ×
%macro SYL 18
    dd %1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14, %15, %16, %17, %18
%endmacro
; длина; пятка: где, ширина, зёрен/с, сила; носок: то же; перекат: зёрен/с, сила; u^KP;
; τ зерна; трение: сила, рывков/с (начало, вершина, конец, где), дрожь; разброс весов,
; напрямую, громкость, темп от силы
%macro GRAIN 23
    dd %1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14, %15, %16, %17, %18
    dd %19, %20, %21, %22, %23
%endmacro
; резонанс: Гц, Q, громкость, разброс
%macro GRES 4
    dd %1, %2, %3, %4
%endmacro

section .rdata
align 16, db 0
ak_dnk:     dd 0.12                     ; толчок потока: 0.12 / шаг фазы
ak_dfmin:   dd 0.0005                   ; шаг фазы не меньше (22 Гц): без деления на 0
ak_0_66:    dd 0.66
ak_i066:    dd 1.5151515
ak_i034:    dd 2.9411765
ak_c2:      dd -1.2337006               ; cos(πv/2) = 1 + v²(c2 + v²(c4 + v²·c6))
ak_c4:      dd 0.25366950
ak_c6:      dd -0.020863481

; --- лай: «гав» — высота дугой (с 0.6 до 1 и вниз за 0.16 с), пасть раскрывается на «а»
cd_bark:
    CALLD 1, 0.55, 0, 0.42, 0.45, 0.05, 0.25, 0.3, 2.0, 0, 0, 0.02, 0, 0, 0.7, 1.0
    FORM  470.0, 760.0, 3.2, 1.0
    FORM  1100.0, 1400.0, 4.5, 0.55
    FORM  2300.0, 2600.0, 5.0, 0.2
    FORM  3600.0, 3900.0, 4.0, 0.1
    dd    0.15, 0.3
    SYL   0, 0.17, 330.0, 640.0, 350.0, 0.45, 1.0, 0.03, 0.07, 0.3, 0.1, 1.0, 0.25, 0.35, 0.35, 0.015, 1.0, 1.0
; --- вой: долгий, высота поднимается и медленно сходит, вибрато, почти без шума
cd_howl:
    CALLD 1, 0.5, 2600.0, 0.05, 0.5, 0.008, 0.04, 0.03, 0, 5.2, 0.012, 0.01, 0, 0, 0.5, 1.0
    FORM  420.0, 650.0, 6.0, 1.0
    FORM  900.0, 1150.0, 7.0, 0.45
    FORM  2300.0, 2500.0, 6.0, 0.12
    FORM  3400.0, 3400.0, 5.0, 0.05
    dd    0.2, 0.25
    SYL   0, 2.4, 400.0, 560.0, 410.0, 0.3, 1.0, 0.3, 0.7, 0.25, 0.2, 0.8, 0.3, 0.25, 0.05, 0.05, 1.0, 1.0
; --- рык цепного пса: низко, с подгармоникой и хрипом, пасть почти закрыта
cd_growl:
    CALLD 1, 0.6, 1400.0, 0.2, 0.7, 0.06, 0.25, 0.5, 0.5, 0, 0, 0.03, 23.0, 0.35, 0.6, 0.7
    FORM  230.0, 330.0, 2.5, 1.0
    FORM  650.0, 750.0, 4.0, 0.2
    FORM  1700.0, 1800.0, 5.0, 0.05
    FORM  2400.0, 2400.0, 5.0, 0.02
    dd    0.25, 0.2
    SYL   0, 0.9, 105.0, 125.0, 95.0, 0.5, 1.0, 0.08, 0.2, 0.1, 0.1, 0.35, 0.15, 0.5, 0, 0.01, 1.0, 1.0
; --- петух: ку-ка-ре-ку-у (слоги и высоты сняты с записи: 780–890 Гц, последний долгий,
;     с подгармоникой), горло хрипит
cd_roo:
    CALLD 4, 0.45, 0, 0.16, 0.5, 0.025, 0.15, 0.12, 0.8, 0, 0, 0.012, 55.0, 0.18, 0.5, 1.0
    FORM  800.0, 1000.0, 3.0, 1.0
    FORM  1650.0, 1800.0, 4.0, 0.9
    FORM  2600.0, 2800.0, 4.0, 0.35
    FORM  4400.0, 4800.0, 3.0, 0.4
    dd    0.05, 0.12
    SYL   0, 0.21, 760.0, 875.0, 740.0, 0.45, 0.85, 0.02, 0.03, 0.1, 0.3, 1.0, 0.5, 0.3, 0.6, 0.015, 1.6, 1.5
    SYL   0.25, 0.2, 800.0, 890.0, 830.0, 0.35, 0.9, 0.015, 0.025, 0.1, 0.3, 1.0, 0.6, 0.3, 0.5, 0.012, 1.3, 1.2
    SYL   0.465, 0.315, 740.0, 890.0, 800.0, 0.4, 0.95, 0.015, 0.025, 0.05, 0.4, 1.0, 0.8, 0.35, 0.4, 0.012, 1.0, 0.8
    SYL   0.795, 0.78, 700.0, 860.0, 720.0, 0.2, 1.0, 0.03, 0.12, 0.15, 0.5, 1.0, 0.7, 0.3, 0.4, 0.015, 0.8, 1.6

; --- зомби. Связки срываются (подгармоника, дрожь — «скрип» голоса), дышат шумом;
;     гласные — форманты человека пониже. Высота — у каждого своя (EV_P), хрипота — тоже
;     (EV_A). Стон: «ы-а-о», высота плывёт вниз
cd_groan:
    CALLD 1, 0.5, 2200.0, 0.3, 0.6, 0.07, 0.3, 0.45, 1.5, 5.0, 0.01, 0.03, 0, 0, 0.3, 1.5
    FORM  330.0, 720.0, 5.0, 1.0
    FORM  1300.0, 1100.0, 6.0, 0.5
    FORM  2400.0, 2500.0, 6.0, 0.2
    FORM  3300.0, 3400.0, 5.0, 0.08
    dd    0.2, 0.25
    SYL   0, 1.3, 88.0, 96.0, 70.0, 0.35, 1.0, 0.25, 0.45, 0.3, 0.1, 1.0, 0.4, 0.4, 0.15, 0.05, 1.0, 1.0
; --- «хм?» — носом, с выдохом; к концу вверх, как вопрос
cd_hmm:
    CALLD 1, 0.55, 1600.0, 0.15, 0.5, 0.03, 0.12, 0.2, 0.5, 0, 0, 0.02, 0, 0, 0.3, 0.57
    FORM  260.0, 300.0, 4.0, 1.0
    FORM  1000.0, 1100.0, 6.0, 0.15
    FORM  2300.0, 2400.0, 6.0, 0.05
    FORM  3300.0, 3300.0, 5.0, 0.02
    dd    0.15, 0.2
    SYL   0, 0.38, 105.0, 96.0, 128.0, 0.5, 1.0, 0.04, 0.1, 0, 0, 0.2, 0.1, 0.5, 0.6, 0.05, 1.0, 1.0
; --- «хм» после вдохов (учуял): то же, без вопроса, после трёх вдохов
cd_shmm:
    CALLD 1, 0.55, 1600.0, 0.15, 0.5, 0.03, 0.12, 0.2, 0.5, 0, 0, 0.02, 0, 0, 0.3, 0.5
    FORM  260.0, 300.0, 4.0, 1.0
    FORM  1000.0, 1100.0, 6.0, 0.15
    FORM  2300.0, 2400.0, 6.0, 0.05
    FORM  3300.0, 3300.0, 5.0, 0.02
    dd    0.15, 0.2
    SYL   0.62, 0.34, 102.0, 100.0, 92.0, 0.4, 1.0, 0.04, 0.1, 0, 0, 0.2, 0.1, 0.5, 0.4, 0.05, 1.0, 1.0
; --- заметил: вскрик-рык на «а», срывается вниз, горло рвёт
cd_aggro:
    CALLD 1, 0.5, 0, 0.5, 0.4, 0.08, 0.3, 0.5, 3.0, 0, 0, 0.04, 0, 0, 0.4, 1.7
    FORM  600.0, 820.0, 3.5, 1.0
    FORM  1200.0, 1350.0, 4.5, 0.6
    FORM  2500.0, 2700.0, 5.0, 0.3
    FORM  3500.0, 3700.0, 4.0, 0.12
    dd    0.12, 0.2
    SYL   0, 0.6, 140.0, 195.0, 105.0, 0.25, 1.0, 0.03, 0.25, 0.35, 0.3, 1.0, 0.6, 0.2, 0.6, 0.02, 1.0, 1.0
; --- рычит на бегу: низко, трепещет (~27 Гц), с хрипом
cd_zgrowl:
    CALLD 1, 0.55, 1800.0, 0.35, 0.6, 0.06, 0.25, 0.5, 1.5, 0, 0, 0.03, 27.0, 0.55, 0.3, 1.5
    FORM  450.0, 560.0, 3.5, 1.0
    FORM  950.0, 1100.0, 4.5, 0.4
    FORM  2300.0, 2400.0, 5.0, 0.12
    FORM  3300.0, 3300.0, 5.0, 0.05
    dd    0.2, 0.2
    SYL   0, 0.45, 88.0, 100.0, 80.0, 0.4, 1.0, 0.06, 0.15, 0.2, 0.2, 0.7, 0.4, 0.4, 0.3, 0.03, 1.0, 1.0
; --- рык при ударе: коротко, пасть настежь
cd_snarl:
    CALLD 1, 0.5, 0, 0.45, 0.4, 0.07, 0.3, 0.45, 2.5, 0, 0, 0.03, 31.0, 0.5, 0.4, 1.9
    FORM  650.0, 800.0, 3.5, 1.0
    FORM  1300.0, 1450.0, 4.5, 0.55
    FORM  2500.0, 2700.0, 5.0, 0.25
    FORM  3500.0, 3600.0, 4.0, 0.1
    dd    0.15, 0.2
    SYL   0, 0.3, 125.0, 150.0, 105.0, 0.3, 1.0, 0.02, 0.12, 0.3, 0.3, 1.0, 0.5, 0.3, 0.5, 0.02, 1.0, 1.0
; --- добит: последний стон, проваливается вниз
cd_dmoan:
    CALLD 1, 0.5, 2000.0, 0.35, 0.6, 0.06, 0.3, 0.45, 1.2, 0, 0, 0.03, 18.0, 0.3, 0.3, 1.0
    FORM  450.0, 700.0, 5.0, 1.0
    FORM  900.0, 1100.0, 6.0, 0.45
    FORM  2400.0, 2500.0, 6.0, 0.15
    FORM  3300.0, 3300.0, 5.0, 0.05
    dd    0.15, 0.2
    SYL   0.02, 1.0, 115.0, 120.0, 60.0, 0.2, 1.0, 0.06, 0.35, 0.4, 0.6, 1.0, 0.3, 0.2, 0.2, 0.03, 1.0, 1.2

; --- шаг по рыхлому снегу в валенках: плотный хруст под пяткой, реже на перекате, ещё
;     горсть под носком; щелчки звенят низко (400–1000 Гц), выше быстро стихают
gd_snow:
    GRAIN 0.15, 0.02, 0.045, 5000.0, 1.0, 0.1, 0.04, 2500.0, 0.55, 900.0, 0.35, 2, 0.0007, 0, 1.0, 1.0, 1.0, 0.5, 0, 0.85, 0.05, 0.65, 0.4
    GRES  480.0, 1.6, 1.0, 0.35
    GRES  1000.0, 1.6, 0.55, 0.3
    GRES  2400.0, 1.3, 0.22, 0.25
    dd    1800.0
; --- укатанная дорога в мороз: снег скрипит — трение рывками 220–380 в секунду звенит
;     в полосах 1.2 и 2.6 кГц, под ним редкий хруст
gd_road:
    GRAIN 0.15, 0.015, 0.035, 1200.0, 1.0, 0.1, 0.04, 700.0, 0.8, 350.0, 0.6, 2, 0.0005, 0.35, 220.0, 380.0, 260.0, 0.4, 0.12, 0.4, 0.03, 2.2, 0.4
    GRES  1250.0, 5.0, 1.0, 0.04
    GRES  520.0, 2.5, 0.6, 0.2
    GRES  2600.0, 6.0, 0.45, 0.04
    dd    3800.0
; --- половица скрипит под ногой: трение 260–520 рывков/с — визг, что плывёт вверх и
;     обратно, звенят моды доски; под ним потрескивает
gd_fcreak:
    GRAIN 0.32, 0.03, 0.06, 200.0, 0.8, 0.2, 0.12, 300.0, 1.0, 150.0, 0.9, 2, 0.0003, 0.9, 260.0, 520.0, 330.0, 0.55, 0.1, 0.5, 0.01, 1.0, 0
    GRES  700.0, 4.5, 1.0, 0.03
    GRES  1500.0, 5.0, 0.7, 0.03
    GRES  2800.0, 4.0, 0.3, 0.03
    dd    5000.0
; --- зомби волочит ноги: скребёт по снегу долго и ровно, потом проваливается
gd_zdrag:
    GRAIN 0.3, 0.03, 0.06, 2500.0, 0.7, 0.22, 0.08, 1200.0, 0.5, 1800.0, 0.45, 2, 0.0008, 0, 1.0, 1.0, 1.0, 0.5, 0, 0.8, 0.03, 1.05, 0.3
    GRES  380.0, 1.3, 1.0, 0.3
    GRES  800.0, 1.5, 0.6, 0.3
    GRES  1700.0, 1.2, 0.15, 0.25
    dd    1400.0
; --- цепь звякает: острые щелчки в узких резонансах звеньев
gd_chain:
    GRAIN 0.5, 0.02, 0.05, 2000.0, 1.0, 0.25, 0.2, 300.0, 0.5, 150.0, 0.3, 2, 0.00008, 0, 1.0, 1.0, 1.0, 0.5, 0, 0.9, 0.02, 1.0, 0
    GRES  2900.0, 35.0, 6.0, 0.15
    GRES  4300.0, 45.0, 5.0, 0.15
    GRES  6100.0, 40.0, 3.0, 0.15
    dd    0
; --- полено стрельнуло: пачка щелчков за 50 мс
gd_pop:
    GRAIN 0.05, 0.004, 0.01, 6000.0, 1.0, 0.03, 0.02, 1500.0, 0.4, 400.0, 0.3, 3, 0.00015, 0, 1.0, 1.0, 1.0, 0.5, 0, 0.8, 0.08, 1.0, 0
    GRES  1500.0, 2.0, 2.0, 0.4
    GRES  3200.0, 2.0, 1.4, 0.3
    GRES  700.0, 1.5, 0.8, 0.3
    dd    0

; --- петля ставни визжит: трение 140–420 рывков/с, звенит железо
gd_hinge:
    GRAIN 0.5, 0.05, 0.1, 0, 0.8, 0.35, 0.2, 0, 1.0, 0, 0.9, 1, 0.0002, 0.9, 140.0, 420.0, 220.0, 0.55, 0.12, 0, 0.01, 2.0, 0
    GRES  1100.0, 7.0, 1.0, 0.02
    GRES  2300.0, 8.0, 0.7, 0.02
    GRES  3600.0, 7.0, 0.35, 0.03
    dd    6000.0

; --- снег с веток: ком развалился — и ещё с полсекунды сыплется снежная пыль, мелко и
;     высоко (сам ком — слоем L_flump)
gd_sprinkle:
    GRAIN 0.7, 0.03, 0.05, 800.0, 0.6, 0.35, 0.3, 250.0, 0.3, 150.0, 0.25, 2, 0.0003, 0, 1.0, 1.0, 1.0, 0.5, 0, 0.7, 0.02, 0.6, 0
    GRES  2600.0, 1.5, 1.0, 0.3
    GRES  4500.0, 1.5, 0.6, 0.3
    GRES  1200.0, 1.2, 0.4, 0.3
    dd    0

%if gd_road - gd_snow != GDSZ
%error "gd_*: размер описания зёрен не GDSZ"
%endif

align 8, db 0
cd_tab:     dq cd_bark, cd_howl, cd_growl, cd_roo, cd_groan, cd_hmm, cd_shmm, cd_aggro
            dq cd_zgrowl, cd_snarl, cd_dmoan
CD_BARK     equ 0
CD_HOWL     equ 1
CD_GROWL    equ 2
CD_ROO      equ 3
CD_GROAN    equ 4
CD_HMM      equ 5
CD_SHMM     equ 6
CD_AGGRO    equ 7
CD_ZGROWL   equ 8
CD_SNARL    equ 9
CD_DMOAN    equ 10
GD_SNOW     equ 0
GD_ROAD     equ 1
GD_FCREAK   equ 2
GD_ZDRAG    equ 3
GD_CHAIN    equ 4
GD_POP      equ 5
GD_HINGE    equ 6
GD_SPRINKLE equ 7

section .text
