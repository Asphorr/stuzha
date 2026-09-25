; ----------------------------------------------------------------------------
;  audio.inc — звук. Всё синтезируется на лету, звуковых файлов нет.
;  Поток на waveOut: 44.1 кГц, стерео, блоки по 512 кадров (~12 мс), 5 в очереди.
;  Игра раз в кадр пишет параметры (ap: ветер у игрока и по бокам, над крышами, в избе
;  ли, позёмка, ели/берёзы/провода/фонари/печь/телевизор рядом, сердце) и кладёт события
;  (шаг, взмах, удар, голос зомби, лай...) в кольцо; поток звука их разбирает.
;  Фоны — пять четвёрок SVF-фильтров на шуме (SSE: 4 фильтра разом) и две четвёрки
;  синусов (провода, гул фонарей). События — голоса: шум/щелчки + осциллятор -> два
;  SVF, огибающая, глиссандо, дрожь, АМ; панорама и глушение по расстоянию, стенам и
;  ветру (по ветру слышно дальше). Голоса сложнее — зов (лай, петух) и зёрна (шаги,
;  треск) — в asynth.inc. Реверб — сеть из 8 задержек, мягкий предел.
;  --wav в режиме снимка пишет то же самое в shotN.wav (проверка без ушей),
;  --sndtest — все звуки событий подряд в sndtest.wav.
;  Внутри потока звука: вызываемые сохраняют rbx, rsi, rdi, rbp, r12-r15; xmm — любые
;  (aud_block сохраняет xmm6-15 для внешнего вызова).
; ----------------------------------------------------------------------------
section .text

AFS         equ 44100
ABLK        equ 512                     ; кадров в блоке
ANBUF       equ 5                       ; блоков в очереди устройства
ASUB        equ 32                      ; кусок голоса: огибающая и фильтры — по кускам
NVOICE      equ 64
VSZ         equ 512
NEV         equ 128                     ; кольцо событий
EVSZ        equ 64
NQUAD       equ 5
QSZ         equ 352
NPQ         equ 2
PQSZ        equ 128
NAP         equ 32
WAVMAX      equ AFS*125                 ; до ~2 мин в --wav
ACC_R       equ ABLK*16                 ; суммы четвёрок: L, R, реверб подряд
ACC_V       equ ABLK*32
MAXKN       equ 16                      ; будки

; параметры фона (игра -> звук), float
AP_WN       equ 0                       ; ветер у игрока (с затишьем и порывами)
AP_WL       equ 4                       ; ...в 2 тайлах левее на экране
AP_WR       equ 8                       ; ...правее
AP_WPAN     equ 12                      ; откуда дует: −1 слева .. +1 справа
AP_WF       equ 16                      ; над крышами
AP_IN       equ 20                      ; в избе
AP_HISS     equ 24                      ; позёмка/метель у игрока
AP_EDGE     equ 28                      ; заслоны рядом (избы, заборы) — вой
AP_SPR      equ 32
AP_SPRP     equ 36
AP_BIR      equ 40
AP_BIRP     equ 44
AP_WIRE     equ 48
AP_WIRP     equ 52
AP_WIRF     equ 56                      ; тон проводов, Гц
AP_HUM      equ 60
AP_HUMP     equ 64
AP_BRK      equ 68
AP_BRKP     equ 72
AP_FIRE     equ 76
AP_FIRP     equ 80
AP_TV       equ 84
AP_TVP      equ 88
AP_TVL      equ 92
AP_HB       equ 96                      ; сердце: громкость
AP_HBR      equ 100                     ; ...ударов в секунду
AP_DEAD     equ 104
AP_VOL      equ 108
AP_GX       equ 112                     ; порыв у игрока сверх среднего, 0..1
AP_TVIN     equ 116                     ; в избе с телевизором (иначе — через стену, глухо)

; событие
EV_SND      equ 0
EV_GL       equ 4
EV_GR       equ 8
EV_CL       equ 12                      ; ФНЧ по уху (коэф. однополюсника)
EV_CR       equ 16
EV_RV       equ 20                      ; в реверб
EV_P        equ 24                      ; высота
EV_VOL      equ 28
EV_DEL      equ 32                      ; задержка, с
EV_SEED     equ 36                      ; зерно голоса (0 — любое): эхо — та же копия
EV_A        equ 40                      ; сила шага / хрипота голоса (0 — обычная)

; голос
V_ON        equ 0                       ; звук + 1; 0 — свободен
V_T         equ 4                       ; прошло сэмплов
V_DEL       equ 8                       ; до начала, сэмплы
V_STG       equ 12                      ; 0 атака, 1 держим, 2 спад
V_ENV       equ 16
V_ATK       equ 20                      ; прирост за сэмпл
V_HOLD      equ 24                      ; сэмплов держать
V_KR        equ 28                      ; спад за кусок (32)
V_NW        equ 32                      ; шум -> фильтры
V_OFW       equ 36                      ; осциллятор -> фильтры
V_ODW       equ 40                      ; осциллятор напрямую
V_THR       equ 44                      ; щелчки: проходит |шум| > порога (0 — сплошной)
V_OTY       equ 48                      ; 0 синус, 1 пила
V_PH        equ 52                      ; фаза пилы / Re синуса
V_IM        equ 56                      ; Im синуса
V_F         equ 60                      ; частота, Гц
V_FE        equ 64                      ; куда скользит
V_FK        equ 68                      ; скольжение за кусок
V_JIT       equ 72                      ; дрожь высоты, доля
V_JV        equ 76
V_AMI       equ 80                      ; АМ: циклов за кусок
V_AMD       equ 84
V_AMP       equ 88
V_GL        equ 92
V_GR        equ 96
V_GV        equ 100
V_CL        equ 104
V_CR        equ 108
V_SL        equ 112
V_SR        equ 116
V_NS        equ 120
V_DUR       equ 124
V_F1        equ 128
V_F2        equ 192
V_KIND      equ 248                     ; 0 — слои; зов, зёрна — asynth.inc (свои поля с 128)
; фильтр голоса (от V_F1/V_F2): TPT SVF — v1 = a1·i1 + a2·(x − i2), v2 = i2 + g·v1
F_MD        equ 0                       ; 0 нет, 1 ФНЧ, 2 полоса, 3 ФВЧ
F_FC        equ 4
F_FE        equ 8
F_FK        equ 12
F_K         equ 16                      ; 1/Q
F_G         equ 20
F_A1        equ 24
F_A2        equ 28
F_TG        equ 32
F_M0        equ 36                      ; выход = m0·x + m1·v1 + m2·v2 (с громкостью)
F_M1        equ 40
F_M2        equ 44
F_I1        equ 48
F_I2        equ 52

; четвёрка фильтров фона (все поля — по 4 float)
Q_RNG       equ 0                       ; 4 ГСЧ xorshift
Q_Y         equ 16                      ; окраска: однополюсный ФНЧ шума
Q_C         equ 32
Q_WR        equ 48                      ; вход = белый·wr + окрашенный·wl + щелчки·wt
Q_WL        equ 64
Q_THR       equ 80
Q_WT        equ 96
Q_I1        equ 112
Q_I2        equ 128
Q_A1        equ 144
Q_A2        equ 160
Q_TG        equ 176
Q_M0        equ 192
Q_M1        equ 208
Q_M2        equ 224
Q_GL        equ 240
Q_GR        equ 256
Q_GV        equ 272
Q_DL        equ 288
Q_DR        equ 304
Q_DV        equ 320
Q_E         equ 336                     ; энергия (для --wav)

; четвёрка синусов
PQ_RE       equ 0
PQ_IM       equ 16
PQ_CW       equ 32
PQ_SW       equ 48
PQ_GL       equ 64
PQ_GR       equ 80
PQ_DL       equ 96
PQ_DR       equ 112

; слой звука (заготовка голоса), см. lay_tab
LY_CH       equ 0                       ; шанс
LY_DEL      equ 4                       ; задержка, с (+ случайно до LY_DELR)
LY_DELR     equ 8
LY_G        equ 12                      ; громкость (± доля LY_GR)
LY_GR       equ 16
LY_ATK      equ 20                      ; атака, с
LY_HOLD     equ 24                      ; держать, с (± доля)
LY_HOLR     equ 28
LY_REL      equ 32                      ; спад τ, с
LY_NW       equ 36
LY_OFW      equ 40
LY_ODW      equ 44
LY_THR      equ 48
LY_OTY      equ 52                      ; int
LY_F0       equ 56                      ; Гц (± доля)
LY_F0R      equ 60
LY_FER      equ 64                      ; конец глиссандо / начало
LY_FTAU     equ 68
LY_JIT      equ 72
LY_AMF      equ 76                      ; Гц
LY_AMD      equ 80
LY_DUR      equ 84                      ; предел, с
LY_PT       equ 88                      ; фильтры за высотой события (0..1)
LY_F1       equ 92
LY_F2       equ 120
LYSZ        equ 160
LF_MD       equ 0                       ; int
LF_FC       equ 4
LF_FCR      equ 8
LF_FER      equ 12
LF_FTAU     equ 16
LF_Q        equ 20
LF_G        equ 24

; звуки
SN_STEP_SNOW equ 0
SN_STEP_ROAD equ 1
SN_STEP_WOOD equ 2
SN_STEP_Z   equ 3
SN_SWING    equ 4
SN_HIT      equ 5
SN_KILL     equ 6
SN_ZATK     equ 7
SN_SNARL    equ 8
SN_GROAN    equ 9
SN_HMM      equ 10
SN_AGGRO    equ 11
SN_GROWL    equ 12
SN_SNIFF    equ 13
SN_BREATH   equ 14
SN_CLICK    equ 15
SN_KNOCK    equ 16
SN_CREAK    equ 17
SN_BARK     equ 18
SN_HOWL     equ 19
SN_HB       equ 20
SN_DEATH    equ 21
SN_POP      equ 22
SN_BRK      equ 23
SN_FLUMP    equ 24
SN_ROOSTER  equ 25
SN_INHALE   equ 26
SN_FCREAK   equ 27                      ; половица скрипит
SN_GROWLD   equ 28                      ; цепной пёс рычит
SN_CHAIN    equ 29                      ; цепь звякает
SN_SIZZLE   equ 30                      ; сырое полено шипит
NSND        equ 31

; ---------------------------------------------------------------- устройство
; открыть waveOut и запустить поток; нет устройства — игра молча идёт без звука
audio_init:
    push    rbx
    sub     rsp, 64
    call    aud_reset
    xor     ecx, ecx
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    call    [CreateEventW]
    test    rax, rax
    jz      .out
    mov     [aud_hev], rax
    lea     rcx, [aud_hwo]
    mov     edx, 0xFFFFFFFF             ; WAVE_MAPPER
    lea     r8, [aud_wfx]
    mov     r9, rax                     ; событие: буфер отыгран
    mov     qword [rsp + 32], 0
    mov     qword [rsp + 40], 0x50000   ; CALLBACK_EVENT
    call    [waveOutOpen]
    mov     [aud_oerr], eax
    test    eax, eax
    jnz     .out
    xor     ebx, ebx
.h:
    imul    eax, ebx, 48
    lea     rdx, [aud_hdr]
    add     rdx, rax
    imul    ecx, ebx, ABLK*4
    lea     r8, [aud_pcm]
    add     r8, rcx
    mov     [rdx], r8                   ; lpData
    mov     dword [rdx + 8], ABLK*4     ; dwBufferLength
    mov     rcx, [aud_hwo]
    mov     r8d, 48
    call    [waveOutPrepareHeader]
    inc     ebx
    cmp     ebx, ANBUF
    jb      .h
    mov     byte [aud_live], 1
    mov     byte [aud_on], 1
    xor     ecx, ecx
    xor     edx, edx
    lea     r8, [aud_thread]
    xor     r9d, r9d
    mov     qword [rsp + 32], 0
    mov     qword [rsp + 40], 0
    call    [CreateThread]
    mov     [aud_hthr], rax
.out:
    add     rsp, 64
    pop     rbx
    ret

; поток: ждёт, пока устройство вернёт буфер, досчитывает его и отдаёт обратно
aud_thread:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    call    [GetCurrentThread]
    mov     rcx, rax
    mov     edx, 2                      ; THREAD_PRIORITY_HIGHEST
    call    [SetThreadPriority]
    xor     ebx, ebx
.prime:
    mov     ecx, ebx
    call    aud_fill
    inc     ebx
    cmp     ebx, ANBUF
    jb      .prime
    xor     ebx, ebx                    ; какой вернётся следующим
.wait:
    mov     rcx, [aud_hev]
    mov     edx, 100
    call    [WaitForSingleObject]
    cmp     byte [aud_quit], 0
    jne     .out
    ; все разом отыграны — поток не поспел (счётчик провалов)
    xor     esi, esi
    xor     ecx, ecx
    lea     rdx, [aud_hdr]
.cnt:
    test    dword [rdx + 24], 1         ; WHDR_DONE
    jz      .cn
    inc     esi
.cn:
    add     rdx, 48
    inc     ecx
    cmp     ecx, ANBUF
    jb      .cnt
    cmp     esi, ANBUF
    jb      .drain
    inc     dword [aud_under]
.drain:
    imul    eax, ebx, 48
    lea     rdx, [aud_hdr]
    test    dword [rdx + rax + 24], 1
    jz      .wait
    mov     ecx, ebx
    call    aud_fill
    inc     ebx
    cmp     ebx, ANBUF
    jb      .drain
    xor     ebx, ebx
    jmp     .drain
.out:
    add     rsp, 40
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    xor     eax, eax
    ret

; ecx = номер буфера: досчитать и отдать устройству
aud_fill:
    push    rbx
    sub     rsp, 32
    mov     ebx, ecx
    imul    eax, ebx, ABLK*4
    lea     rcx, [aud_pcm]
    add     rcx, rax
    call    aud_block
    inc     dword [aud_nblk]
    imul    eax, ebx, 48
    lea     rdx, [aud_hdr]
    add     rdx, rax
    mov     rcx, [aud_hwo]
    mov     r8d, 48
    call    [waveOutWrite]
    add     rsp, 32
    pop     rbx
    ret

; выход: остановить поток, вернуть буферы, закрыть устройство (--alog: счётчики)
audio_shutdown:
    cmp     byte [aud_live], 0
    je      .no
    push    rbx
    sub     rsp, 48
    mov     byte [aud_quit], 1
    mov     rcx, [aud_hev]
    call    [SetEvent]
    mov     rcx, [aud_hthr]
    mov     edx, 500
    call    [WaitForSingleObject]
    mov     rcx, [aud_hwo]
    call    [waveOutReset]
    xor     ebx, ebx
.u:
    imul    eax, ebx, 48
    lea     rdx, [aud_hdr]
    add     rdx, rax
    mov     rcx, [aud_hwo]
    mov     r8d, 48
    call    [waveOutUnprepareHeader]
    inc     ebx
    cmp     ebx, ANBUF
    jb      .u
    mov     rcx, [aud_hwo]
    call    [waveOutClose]
    mov     byte [aud_live], 0
    cmp     dword [dbg_alog], 0
    je      .nolog
    lea     rcx, [s_alogname]
    lea     rdx, [aud_nblk]
    mov     r8d, 12
    call    write_small
.nolog:
    add     rsp, 48
    pop     rbx
.no:
    ret

; rcx = имя (UTF-16), rdx = данные, r8d = байт: записать файл целиком
write_small:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 64
    mov     rsi, rdx
    mov     edi, r8d
    mov     edx, 0x40000000
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword [rsp + 32], 2
    mov     qword [rsp + 40], 0x80
    mov     qword [rsp + 48], 0
    call    [CreateFileW]
    mov     rbx, rax
    mov     rcx, rbx
    mov     rdx, rsi
    mov     r8d, edi
    lea     r9, [written]
    mov     qword [rsp + 32], 0
    call    [WriteFile]
    mov     rcx, rbx
    call    [CloseHandle]
    add     rsp, 64
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- без устройства: в память
; режим снимка: звук считается в том же потоке и копится для shotN.wav
wav_begin:
    sub     rsp, 40
    call    aud_reset
    mov     byte [aud_on], 1
    mov     byte [aud_wav], 1
    mov     dword [wav_n], 0
    mov     dword [wav_owe], 0
    add     rsp, 40
    ret

; после кадра симуляции: досчитать звук на dt
wav_pump:
    cmp     byte [aud_wav], 0
    je      .no
    sub     rsp, 40
    movss   xmm0, [dt]
    mulss   xmm0, [ac_fs]
    cvtss2si eax, xmm0
    add     [wav_owe], eax
.l:
    cmp     dword [wav_owe], ABLK
    jl      .out
    mov     eax, [wav_n]
    cmp     eax, WAVMAX - ABLK
    ja      .out
    lea     rcx, [wav_q0]
    call    [QueryPerformanceCounter]
    mov     eax, [wav_n]
    lea     rcx, [wav_buf]
    lea     rcx, [rcx + rax*4]
    call    aud_block
    lea     rcx, [wav_q1]
    call    [QueryPerformanceCounter]
    mov     rax, [wav_q1]
    sub     rax, [wav_q0]
    add     [wav_qs], rax
    add     dword [wav_n], ABLK
    sub     dword [wav_owe], ABLK
    jmp     .l
.out:
    add     rsp, 40
.no:
    ret

; rcx = имя: RIFF-заголовок + накопленное; рядом alevels.bin — энергия фонов и пики
wav_save:
    push    rbx
    push    rsi
    sub     rsp, 56
    mov     rsi, rcx
    mov     eax, [wav_n]
    shl     eax, 2
    mov     [wav_hdr + 40], eax
    add     eax, 36
    mov     [wav_hdr + 4], eax
    mov     rcx, rsi
    mov     edx, 0x40000000
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword [rsp + 32], 2
    mov     qword [rsp + 40], 0x80
    mov     qword [rsp + 48], 0
    call    [CreateFileW]
    mov     rbx, rax
    mov     rcx, rbx
    lea     rdx, [wav_hdr]
    mov     r8d, 44
    lea     r9, [written]
    mov     qword [rsp + 32], 0
    call    [WriteFile]
    mov     rcx, rbx
    lea     rdx, [wav_buf]
    mov     r8d, [wav_n]
    shl     r8d, 2
    lea     r9, [written]
    mov     qword [rsp + 32], 0
    call    [WriteFile]
    mov     rcx, rbx
    call    [CloseHandle]
    mov     eax, [wav_n]
    mov     [st_frames], eax
    ; мкс на блок (в поле после st_knee)
    shr     eax, 9
    cvtsi2ss xmm1, eax
    cvtsi2ss xmm0, qword [wav_qs]
    mulss   xmm0, [ac_1e6]
    cvtsi2ss xmm2, qword [qpf]
    divss   xmm0, xmm2
    maxss   xmm1, [ac_1]
    divss   xmm0, xmm1
    movss   [st_us], xmm0
    lea     rcx, [s_alevname]
    lea     rdx, [st_lane]
    mov     r8d, ST_BYTES
    call    write_small
    lea     rcx, [s_atrname]
    lea     rdx, [trbuf]
    mov     r8d, [tr_n]
    shl     r8d, 7
    call    write_small
    add     rsp, 56
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- блок
; rcx = куда: ABLK кадров стерео int16. События -> голоса, фоны, голоса, реверб, предел
aud_block:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 200
    movaps  [rsp + 32], xmm6
    movaps  [rsp + 48], xmm7
    movaps  [rsp + 64], xmm8
    movaps  [rsp + 80], xmm9
    movaps  [rsp + 96], xmm10
    movaps  [rsp + 112], xmm11
    movaps  [rsp + 128], xmm12
    movaps  [rsp + 144], xmm13
    movaps  [rsp + 160], xmm14
    movaps  [rsp + 176], xmm15
    stmxcsr [rsp + 192]
    mov     eax, [rsp + 192]
    or      eax, 0x8040                 ; денормалы — в ноль (хвосты фильтров)
    mov     [rsp + 196], eax
    ldmxcsr [rsp + 196]
    mov     [ab_out], rcx
    lea     rdi, [mixL]
    xor     eax, eax
    mov     ecx, (ABLK*12 + ABLK*48) / 8
    rep     stosq
    call    ev_drain
    call    bed_ctl
    lea     rbx, [quads]
    mov     r12d, NQUAD
.q:
    call    quad_run
    add     rbx, QSZ
    dec     r12d
    jnz     .q
    lea     rbx, [pquads]
    mov     r12d, NPQ
.p:
    call    ph_run
    add     rbx, PQSZ
    dec     r12d
    jnz     .p
    call    acc_fold
    lea     rbx, [voices]
    mov     r12d, NVOICE
.v:
    cmp     dword [rbx + V_ON], 0
    je      .vn
    call    voice_run
.vn:
    add     rbx, VSZ
    dec     r12d
    jnz     .v
    call    rev_run
    call    master_out
    ldmxcsr [rsp + 192]
    movaps  xmm6, [rsp + 32]
    movaps  xmm7, [rsp + 48]
    movaps  xmm8, [rsp + 64]
    movaps  xmm9, [rsp + 80]
    movaps  xmm10, [rsp + 96]
    movaps  xmm11, [rsp + 112]
    movaps  xmm12, [rsp + 128]
    movaps  xmm13, [rsp + 144]
    movaps  xmm14, [rsp + 160]
    movaps  xmm15, [rsp + 176]
    add     rsp, 200
    pop     rbp
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- математика потока звука
; xmm0 = x -> e^x (x87); портит at_t0
a_exp:
    movss   [at_t0], xmm0
    fld     dword [at_t0]
    fldl2e
    fmulp   st1, st0                    ; y = x·log2 e
    fld     st0
    frndint                             ; n, y
    fxch    st1
    fsub    st0, st1                    ; y − n, n
    f2xm1
    fld1
    faddp   st1, st0                    ; 2^(y−n), n
    fscale
    fstp    st1
    fstp    dword [at_t0]
    movss   xmm0, [at_t0]
    ret

; xmm0 = частота среза, Гц -> g = tg(π·fc/fs)
a_tan:
    maxss   xmm0, [ac_10]
    minss   xmm0, [ac_fcmax]
    mulss   xmm0, [ac_pi_fs]
    movss   [at_t0], xmm0
    fld     dword [at_t0]
    fptan
    fstp    st0
    fstp    dword [at_t0]
    movss   xmm0, [at_t0]
    ret

; ГСЧ потока звука: eax; a_rndf -> xmm0 [0,1); a_rnds -> xmm0 [−1,1). Портят eax, r11
a_rnd:
    mov     eax, [at_rng]
    mov     r11d, eax
    shl     r11d, 13
    xor     eax, r11d
    mov     r11d, eax
    shr     r11d, 17
    xor     eax, r11d
    mov     r11d, eax
    shl     r11d, 5
    xor     eax, r11d
    mov     [at_rng], eax
    ret
a_rndf:
    call    a_rnd
    shr     eax, 8
    cvtsi2ss xmm0, eax
    mulss   xmm0, [ac_2m24]
    ret
a_rnds:
    call    a_rnd
    cvtsi2ss xmm0, eax
    mulss   xmm0, [ac_2m31]
    ret

; xmm0 = панорама −1..1, xmm1 = громкость -> xmm0 = левое, xmm1 = правое (равная мощность)
a_pan:
    maxss   xmm0, [ac_m1]
    minss   xmm0, [ac_1]
    movss   xmm2, [ac_1]
    subss   xmm2, xmm0
    mulss   xmm2, [ac_0_5]
    sqrtss  xmm2, xmm2
    addss   xmm0, [ac_1]
    mulss   xmm0, [ac_0_5]
    sqrtss  xmm0, xmm0
    mulss   xmm0, xmm1                  ; правое
    mulss   xmm1, xmm2                  ; левое
    movaps  xmm2, xmm0
    movaps  xmm0, xmm1
    movaps  xmm1, xmm2
    ret

; xmm0 = сила ветра s -> громкость ветра s² / (0.25 + 0.75 s + 0.25 s²): в тишь быстро
; стихает, в метели дальше растёт медленнее (иначе шаги и голоса тонут совсем)
a_loud:
    maxss   xmm0, [ac_0]
    movaps  xmm1, xmm0
    mulss   xmm1, [ac_0_25]
    addss   xmm1, [ac_0_75]
    mulss   xmm1, xmm0
    addss   xmm1, [ac_0_25]
    mulss   xmm0, xmm0
    divss   xmm0, xmm1
    ret

; ---------------------------------------------------------------- события -> голоса
ev_drain:
    push    r12
    push    rsi
    sub     rsp, 40
    mov     r12d, [ev_tail]
.l:
    cmp     r12d, [ev_head]
    je      .done
    mov     eax, r12d
    and     eax, NEV - 1
    shl     eax, 6
    lea     rsi, [evq]
    add     rsi, rax
    call    ev_start
    inc     r12d
    jmp     .l
.done:
    mov     [ev_tail], r12d
    add     rsp, 40
    pop     rsi
    pop     r12
    ret

; rsi = событие: по слою звука — голос (слой может не выпасть — шанс)
ev_start:
    push    rbx
    push    rdi
    push    r13
    push    r14
    sub     rsp, 40
    mov     eax, [rsi + EV_SND]
    cmp     eax, NSND
    jae     .out
    lea     rdx, [snd_tab]
    movzx   r13d, byte [rdx + rax*2]
    movzx   r14d, byte [rdx + rax*2 + 1]
.lay:
    test    r14d, r14d
    jz      .xk
    imul    edi, r13d, LYSZ
    lea     rax, [lay_tab]
    add     rdi, rax
    call    a_rndf
    comiss  xmm0, [rdi + LY_CH]
    jae     .next
    call    voice_alloc
    call    voice_fill
.next:
    inc     r13d
    dec     r14d
    jmp     .lay
.xk:
    ; голос сложнее слоёв (asynth.inc): зов или зёрна
    mov     eax, [rsi + EV_SND]
    lea     rdx, [snd_xk]
    movzx   ecx, byte [rdx + rax*2]
    movzx   r13d, byte [rdx + rax*2 + 1]
    test    ecx, ecx
    jz      .out
    cmp     ecx, VK_CALL
    jne     .gr
    lea     rax, [cd_tab]
    mov     rdi, [rax + r13*8]
    call    voice_alloc
    call    call_fill
    jmp     .out
.gr:
    imul    edi, r13d, GDSZ
    lea     rax, [gd_snow]
    add     rdi, rax
    call    voice_alloc
    call    grain_fill
.out:
    add     rsp, 40
    pop     r14
    pop     r13
    pop     rdi
    pop     rbx
    ret

; -> rbx = свободный голос, иначе самый тихий из звучащих (ждущие задержку не трогаем)
voice_alloc:
    lea     rbx, [voices]
    mov     ecx, NVOICE
.f:
    cmp     dword [rbx + V_ON], 0
    je      .got
    add     rbx, VSZ
    dec     ecx
    jnz     .f
    lea     rdx, [voices]
    mov     rbx, rdx
    mov     ecx, NVOICE
    movss   xmm1, [ac_big]
.s:
    cmp     dword [rdx + V_DEL], 0
    jne     .sn
    movss   xmm0, [rdx + V_GL]
    addss   xmm0, [rdx + V_GR]
    mulss   xmm0, [rdx + V_ENV]
    comiss  xmm0, xmm1
    jae     .sn
    movaps  xmm1, xmm0
    mov     rbx, rdx
.sn:
    add     rdx, VSZ
    dec     ecx
    jnz     .s
.got:
    ret

; rbx = голос, rdi = слой, rsi = событие: разброс по слою, место — из события
voice_fill:
    push    r12
    sub     rsp, 32
    mov     r12, rdi
    mov     rdi, rbx
    xor     eax, eax
    mov     ecx, VSZ/8
    rep     stosq
    mov     rdi, r12
    mov     eax, [rsi + EV_SND]
    inc     eax
    mov     [rbx + V_ON], eax
    call    a_rnd
    or      eax, 1
    mov     [rbx + V_NS], eax
    ; задержка
    call    a_rndf
    mulss   xmm0, [rdi + LY_DELR]
    addss   xmm0, [rdi + LY_DEL]
    addss   xmm0, [rsi + EV_DEL]
    mulss   xmm0, [ac_fs]
    cvttss2si eax, xmm0
    mov     [rbx + V_DEL], eax
    ; громкость: слой · разброс · событие -> уши и реверб
    call    a_rnds
    mulss   xmm0, [rdi + LY_GR]
    addss   xmm0, [ac_1]
    mulss   xmm0, [rdi + LY_G]
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
    ; огибающая
    movss   xmm0, [rdi + LY_ATK]
    mulss   xmm0, [ac_fs]
    comiss  xmm0, [ac_1]
    jae     .atk
    mov     dword [rbx + V_ENV], __float32__(1.0)
    mov     dword [rbx + V_STG], 1
    jmp     .hold
.atk:
    movss   xmm1, [ac_1]
    divss   xmm1, xmm0
    movss   [rbx + V_ATK], xmm1
.hold:
    call    a_rnds
    mulss   xmm0, [rdi + LY_HOLR]
    addss   xmm0, [ac_1]
    mulss   xmm0, [rdi + LY_HOLD]
    mulss   xmm0, [ac_fs]
    cvttss2si eax, xmm0
    mov     [rbx + V_HOLD], eax
    movss   xmm0, [rdi + LY_REL]
    mulss   xmm0, [ac_fs]
    maxss   xmm0, [ac_1]
    movss   xmm1, [ac_m32]
    divss   xmm1, xmm0
    movaps  xmm0, xmm1
    call    a_exp
    movss   [rbx + V_KR], xmm0
    ; источник
    mov     eax, [rdi + LY_NW]
    mov     [rbx + V_NW], eax
    mov     eax, [rdi + LY_OFW]
    mov     [rbx + V_OFW], eax
    mov     eax, [rdi + LY_ODW]
    mov     [rbx + V_ODW], eax
    mov     eax, [rdi + LY_THR]
    mov     [rbx + V_THR], eax
    mov     eax, [rdi + LY_OTY]
    mov     [rbx + V_OTY], eax
    ; высота и глиссандо
    call    a_rnds
    mulss   xmm0, [rdi + LY_F0R]
    addss   xmm0, [ac_1]
    mulss   xmm0, [rdi + LY_F0]
    mulss   xmm0, [rsi + EV_P]
    movss   [rbx + V_F], xmm0
    movss   xmm1, [rdi + LY_FTAU]
    comiss  xmm1, [ac_0]
    ja      .gl
    movss   [rbx + V_FE], xmm0
    mov     dword [rbx + V_FK], __float32__(1.0)
    jmp     .gd
.gl:
    mulss   xmm0, [rdi + LY_FER]
    movss   [rbx + V_FE], xmm0
    mulss   xmm1, [ac_fs]
    movss   xmm0, [ac_m32]
    divss   xmm0, xmm1
    call    a_exp
    movss   [rbx + V_FK], xmm0
.gd:
    mov     eax, [rdi + LY_JIT]
    mov     [rbx + V_JIT], eax
    movss   xmm0, [rdi + LY_AMF]
    mulss   xmm0, [ac_32_fs]
    movss   [rbx + V_AMI], xmm0
    mov     eax, [rdi + LY_AMD]
    mov     [rbx + V_AMD], eax
    call    a_rndf
    movss   [rbx + V_AMP], xmm0
    ; синус — с нуля (без щелчка), пила — со случайной фазы
    mov     dword [rbx + V_PH], __float32__(1.0)
    cmp     dword [rbx + V_OTY], 0
    je      .ph
    call    a_rndf
    movss   [rbx + V_PH], xmm0
.ph:
    movss   xmm0, [rdi + LY_DUR]
    mulss   xmm0, [ac_fs]
    cvttss2si eax, xmm0
    mov     [rbx + V_DUR], eax
    ; фильтры: за высотой события — на долю LY_PT
    movss   xmm0, [rsi + EV_P]
    subss   xmm0, [ac_1]
    mulss   xmm0, [rdi + LY_PT]
    addss   xmm0, [ac_1]
    movss   [at_pm], xmm0
    lea     rdx, [rbx + V_F1]
    lea     r9, [rdi + LY_F1]
    call    vf_init
    lea     rdx, [rbx + V_F2]
    lea     r9, [rdi + LY_F2]
    call    vf_init
    add     rsp, 32
    pop     r12
    ret

; rdx = фильтр голоса, r9 = фильтр слоя
vf_init:
    mov     eax, [r9 + LF_MD]
    mov     [rdx + F_MD], eax
    test    eax, eax
    jz      .off
    call    a_rnds
    mulss   xmm0, [r9 + LF_FCR]
    addss   xmm0, [ac_1]
    mulss   xmm0, [r9 + LF_FC]
    mulss   xmm0, [at_pm]
    movss   [rdx + F_FC], xmm0
    movss   xmm1, [r9 + LF_FTAU]
    comiss  xmm1, [ac_0]
    ja      .g
    movss   [rdx + F_FE], xmm0
    mov     dword [rdx + F_FK], __float32__(1.0)
    jmp     .q
.g:
    mulss   xmm0, [r9 + LF_FER]
    movss   [rdx + F_FE], xmm0
    mulss   xmm1, [ac_fs]
    movss   xmm0, [ac_m32]
    divss   xmm0, xmm1
    call    a_exp
    movss   [rdx + F_FK], xmm0
.q:
    movss   xmm0, [ac_1]
    divss   xmm0, [r9 + LF_Q]
    movss   [rdx + F_K], xmm0
    mov     eax, [r9 + LF_G]
    mov     [rdx + F_G], eax
    jmp     vf_coef
.off:
    ret

; rdx = фильтр: коэффициенты TPT SVF по fc, 1/Q, виду и громкости (портит xmm0-3, eax)
vf_coef:
    movss   xmm0, [rdx + F_FC]
    call    a_tan
    movss   [rdx + F_TG], xmm0
    movaps  xmm1, xmm0
    addss   xmm1, [rdx + F_K]
    mulss   xmm1, xmm0
    addss   xmm1, [ac_1]
    movss   xmm2, [ac_1]
    divss   xmm2, xmm1
    movss   [rdx + F_A1], xmm2
    mulss   xmm2, xmm0
    movss   [rdx + F_A2], xmm2
    movss   xmm3, [rdx + F_G]
    xorps   xmm0, xmm0
    xorps   xmm1, xmm1
    xorps   xmm2, xmm2
    mov     eax, [rdx + F_MD]
    cmp     eax, 1
    jne     .nlp
    movaps  xmm2, xmm3                  ; ФНЧ: v2
    jmp     .st
.nlp:
    cmp     eax, 2
    jne     .nbp
    movss   xmm1, [rdx + F_K]           ; полоса с единичной вершиной: k·v1
    mulss   xmm1, xmm3
    jmp     .st
.nbp:
    cmp     eax, 3
    jne     .st
    movaps  xmm0, xmm3                  ; ФВЧ: x − k·v1 − v2
    movss   xmm1, [rdx + F_K]
    mulss   xmm1, xmm3
    xorps   xmm1, [ac_sign]
    movaps  xmm2, xmm3
    xorps   xmm2, [ac_sign]
.st:
    movss   [rdx + F_M0], xmm0
    movss   [rdx + F_M1], xmm1
    movss   [rdx + F_M2], xmm2
    ret

; ---------------------------------------------------------------- голос
; шум (xorshift в r8d) с порогом щелчков · nw -> xmm0
%macro VNOISE 0
    mov     eax, r8d
    shl     eax, 13
    xor     r8d, eax
    mov     eax, r8d
    shr     eax, 17
    xor     r8d, eax
    mov     eax, r8d
    shl     eax, 5
    xor     r8d, eax
    cvtsi2ss xmm0, r8d
    mulss   xmm0, [ac_2m31]
    movaps  xmm1, xmm0
    andps   xmm1, [ac_abs]
    movss   xmm2, [rbx + V_THR]
    cmpltss xmm2, xmm1
    andps   xmm0, xmm2
    mulss   xmm0, [rbx + V_NW]
%endmacro

; один фильтр голоса: вход xmm0, выход прибавляется к xmm1; состояния %2 (i1), %3 (i2)
%macro VSVF 3
    movaps  xmm2, xmm0
    subss   xmm2, %3
    mulss   xmm2, [rbx + %1 + F_A2]
    movaps  xmm3, %2
    mulss   xmm3, [rbx + %1 + F_A1]
    addss   xmm3, xmm2                  ; v1
    movaps  xmm2, xmm3
    mulss   xmm2, [rbx + %1 + F_TG]
    addss   xmm2, %3                    ; v2
    subss   %2, xmm3
    subss   %2, xmm3
    xorps   %2, [ac_sign]               ; i1 = 2·v1 − i1
    subss   %3, xmm2
    subss   %3, xmm2
    xorps   %3, [ac_sign]               ; i2 = 2·v2 − i2
    mulss   xmm3, [rbx + %1 + F_M1]
    mulss   xmm2, [rbx + %1 + F_M2]
    addss   xmm1, xmm3
    addss   xmm1, xmm2
    movaps  xmm2, xmm0
    mulss   xmm2, [rbx + %1 + F_M0]
    addss   xmm1, xmm2
%endmacro

; осциллятор в xmm1, шум в xmm0: огибающая, фильтры, уши, в микс
%macro VTAIL 0
    movaps  xmm2, xmm1
    mulss   xmm2, [rbx + V_OFW]
    addss   xmm0, xmm2
    mulss   xmm0, xmm15                 ; огибающая — на источник: резонансы звенят дальше
    mulss   xmm1, [rbx + V_ODW]
    mulss   xmm1, xmm15
    VSVF    V_F1, xmm9, xmm8
    VSVF    V_F2, xmm7, xmm6
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
    addss   xmm15, xmm14
    add     r9, 4
    add     r10, 4
    add     r11, 4
%endmacro

; rbx = голос: прибавить блок к mixL/mixR/mixV. Куски по 32: огибающая (с АМ) линейно
; внутри куска, высота и срезы фильтров — на кусок
voice_run:
    push    r13
    push    r14
    sub     rsp, 40
    mov     eax, [rbx + V_DEL]
    cmp     eax, ABLK
    jb      .go
    sub     dword [rbx + V_DEL], ABLK
    jmp     .ret
.go:
    mov     dword [rbx + V_DEL], 0
    mov     r13d, eax
    mov     eax, [rbx + V_KIND]
    test    eax, eax
    jz      .lay
    cmp     eax, VK_CALL
    jne     .gr
    call    call_run
    jmp     .ret
.gr:
    call    grain_run
    jmp     .ret
.lay:
    mov     r8d, [rbx + V_NS]
    movss   xmm13, [rbx + V_PH]
    movss   xmm12, [rbx + V_IM]
    movss   xmm9, [rbx + V_F1 + F_I1]
    movss   xmm8, [rbx + V_F1 + F_I2]
    movss   xmm7, [rbx + V_F2 + F_I1]
    movss   xmm6, [rbx + V_F2 + F_I2]
    movss   xmm5, [rbx + V_SL]
    movss   xmm4, [rbx + V_SR]
.sub:
    cmp     r13d, ABLK
    jae     .end
    mov     r14d, ABLK
    sub     r14d, r13d
    mov     eax, ASUB
    cmp     r14d, eax
    cmova   r14d, eax
    ; --- огибающая на конец куска
    movss   xmm15, [rbx + V_ENV]
    movss   xmm0, xmm15
    mov     eax, [rbx + V_STG]
    test    eax, eax
    jnz     .nat
    cvtsi2ss xmm1, r14d
    mulss   xmm1, [rbx + V_ATK]
    addss   xmm0, xmm1
    comiss  xmm0, [ac_1]
    jb      .eset
    movss   xmm0, [ac_1]
    mov     dword [rbx + V_STG], 1
    jmp     .eset
.nat:
    cmp     eax, 1
    jne     .rel
    sub     [rbx + V_HOLD], r14d
    jg      .eset
    mov     dword [rbx + V_STG], 2
    jmp     .eset
.rel:
    mulss   xmm0, [rbx + V_KR]
.eset:
    movss   [rbx + V_ENV], xmm0
    ; АМ (рык, дрожь голоса): e·(1 − d·(½ + ½ sin 2πφ)) на краях куска
    movss   xmm1, [rbx + V_AMD]
    comiss  xmm1, [ac_0]
    jbe     .noam
    movss   [at_e1], xmm0
    movss   xmm0, [rbx + V_AMP]
    mulss   xmm0, [ac_2pi]
    call    fsin
    mulss   xmm0, [ac_0_5]
    addss   xmm0, [ac_0_5]
    mulss   xmm0, [rbx + V_AMD]
    movss   xmm1, [ac_1]
    subss   xmm1, xmm0
    mulss   xmm15, xmm1
    movss   xmm0, [rbx + V_AMP]
    addss   xmm0, [rbx + V_AMI]
    comiss  xmm0, [ac_1]
    jb      .amw
    subss   xmm0, [ac_1]
.amw:
    movss   [rbx + V_AMP], xmm0
    mulss   xmm0, [ac_2pi]
    call    fsin
    mulss   xmm0, [ac_0_5]
    addss   xmm0, [ac_0_5]
    mulss   xmm0, [rbx + V_AMD]
    movss   xmm1, [ac_1]
    subss   xmm1, xmm0
    movss   xmm0, [at_e1]
    mulss   xmm0, xmm1
.noam:
    movaps  xmm14, xmm0
    subss   xmm14, xmm15
    cvtsi2ss xmm1, r14d
    divss   xmm14, xmm1                 ; приращение огибающей за сэмпл
    ; --- высота: глиссандо и дрожь
    movss   xmm0, [rbx + V_F]
    subss   xmm0, [rbx + V_FE]
    mulss   xmm0, [rbx + V_FK]
    addss   xmm0, [rbx + V_FE]
    movss   [rbx + V_F], xmm0
    movss   xmm1, [rbx + V_JIT]
    comiss  xmm1, [ac_0]
    jbe     .nj
    movss   [at_e1], xmm0
    call    a_rnds
    mulss   xmm0, [rbx + V_JIT]
    subss   xmm0, [rbx + V_JV]
    mulss   xmm0, [ac_0_3]
    addss   xmm0, [rbx + V_JV]
    movss   [rbx + V_JV], xmm0
    addss   xmm0, [ac_1]
    mulss   xmm0, [at_e1]
.nj:
    mulss   xmm0, [ac_inv_fs]           ; циклов за сэмпл
    cmp     dword [rbx + V_OTY], 0
    jne     .sawk
    ; синус: поворот фазора на θ = 2π·f/fs (ряды до θ⁵), фазор подправляется к |1|
    mulss   xmm0, [ac_2pi]
    movaps  xmm1, xmm0
    mulss   xmm1, xmm0
    movaps  xmm2, xmm1
    mulss   xmm2, [ac_i24]
    subss   xmm2, [ac_0_5]
    mulss   xmm2, xmm1
    addss   xmm2, [ac_1]
    movaps  xmm3, xmm1
    mulss   xmm3, [ac_i120]
    subss   xmm3, [ac_i6]
    mulss   xmm3, xmm1
    addss   xmm3, [ac_1]
    mulss   xmm3, xmm0
    movaps  xmm11, xmm2
    movaps  xmm10, xmm3
    movaps  xmm0, xmm13
    mulss   xmm0, xmm13
    movaps  xmm1, xmm12
    mulss   xmm1, xmm12
    addss   xmm0, xmm1
    mulss   xmm0, [ac_m0_5]
    addss   xmm0, [ac_1_5]
    mulss   xmm13, xmm0
    mulss   xmm12, xmm0
    jmp     .flt
.sawk:
    minss   xmm0, [ac_0_45]
    movaps  xmm11, xmm0
.flt:
    ; --- срезы фильтров скользят
    cmp     dword [rbx + V_F1 + F_MD], 0
    je      .f2
    cmp     dword [rbx + V_F1 + F_FK], __float32__(1.0)
    je      .f2
    movss   xmm0, [rbx + V_F1 + F_FC]
    subss   xmm0, [rbx + V_F1 + F_FE]
    mulss   xmm0, [rbx + V_F1 + F_FK]
    addss   xmm0, [rbx + V_F1 + F_FE]
    movss   [rbx + V_F1 + F_FC], xmm0
    lea     rdx, [rbx + V_F1]
    call    vf_coef
.f2:
    cmp     dword [rbx + V_F2 + F_MD], 0
    je      .fd
    cmp     dword [rbx + V_F2 + F_FK], __float32__(1.0)
    je      .fd
    movss   xmm0, [rbx + V_F2 + F_FC]
    subss   xmm0, [rbx + V_F2 + F_FE]
    mulss   xmm0, [rbx + V_F2 + F_FK]
    addss   xmm0, [rbx + V_F2 + F_FE]
    movss   [rbx + V_F2 + F_FC], xmm0
    lea     rdx, [rbx + V_F2]
    call    vf_coef
.fd:
    ; --- сэмплы куска
    lea     rax, [mixL]
    lea     r9, [rax + r13*4]
    lea     r10, [rax + r13*4 + ABLK*4]
    lea     r11, [rax + r13*4 + ABLK*8]
    mov     ecx, r14d
    cmp     dword [rbx + V_OTY], 0
    jne     .lsaw
.lsin:
    VNOISE
    movaps  xmm1, xmm13
    mulss   xmm1, xmm11
    movaps  xmm2, xmm12
    mulss   xmm2, xmm10
    subss   xmm1, xmm2                  ; re·c − im·s
    mulss   xmm13, xmm10
    mulss   xmm12, xmm11
    addss   xmm12, xmm13                ; re·s + im·c
    movaps  xmm13, xmm1
    movaps  xmm1, xmm12
    VTAIL
    dec     ecx
    jnz     .lsin
    jmp     .snext
.lsaw:
    VNOISE
    ; пила с polyBLEP: у скачка — сглаженная ступенька, без наложения частот
    addss   xmm13, xmm11
    comiss  xmm13, [ac_1]
    jb      .sw
    subss   xmm13, [ac_1]
.sw:
    movaps  xmm1, xmm13
    addss   xmm1, xmm1
    subss   xmm1, [ac_1]
    comiss  xmm13, xmm11
    jae     .b2
    movaps  xmm2, xmm13
    divss   xmm2, xmm11                 ; t = φ/dt
    movaps  xmm3, xmm2
    mulss   xmm3, xmm2
    addss   xmm2, xmm2
    subss   xmm2, xmm3
    subss   xmm2, [ac_1]                ; 2t − t² − 1
    subss   xmm1, xmm2
    jmp     .bd
.b2:
    movss   xmm2, [ac_1]
    subss   xmm2, xmm11
    comiss  xmm13, xmm2
    jbe     .bd
    movaps  xmm2, xmm13
    subss   xmm2, [ac_1]
    divss   xmm2, xmm11                 ; t = (φ − 1)/dt
    movaps  xmm3, xmm2
    mulss   xmm3, xmm2
    addss   xmm3, xmm2
    addss   xmm3, xmm2
    addss   xmm3, [ac_1]                ; t² + 2t + 1
    subss   xmm1, xmm3
.bd:
    VTAIL
    dec     ecx
    jnz     .lsaw
.snext:
    add     r13d, r14d
    jmp     .sub
.end:
    mov     [rbx + V_NS], r8d
    movss   [rbx + V_PH], xmm13
    movss   [rbx + V_IM], xmm12
    movss   [rbx + V_F1 + F_I1], xmm9
    movss   [rbx + V_F1 + F_I2], xmm8
    movss   [rbx + V_F2 + F_I1], xmm7
    movss   [rbx + V_F2 + F_I2], xmm6
    movss   [rbx + V_SL], xmm5
    movss   [rbx + V_SR], xmm4
    add     dword [rbx + V_T], ABLK
    mov     eax, [rbx + V_T]
    cmp     eax, [rbx + V_DUR]
    jae     .kill
    cmp     dword [rbx + V_STG], 2
    jne     .ret
    movss   xmm0, [rbx + V_ENV]
    comiss  xmm0, [ac_1em4]
    jae     .ret
    ; огибающая стихла — ждём, пока отзвенят фильтры
    andps   xmm9, [ac_abs]
    andps   xmm8, [ac_abs]
    andps   xmm7, [ac_abs]
    andps   xmm6, [ac_abs]
    andps   xmm5, [ac_abs]
    andps   xmm4, [ac_abs]
    addss   xmm9, xmm8
    addss   xmm9, xmm7
    addss   xmm9, xmm6
    addss   xmm9, xmm5
    addss   xmm9, xmm4
    comiss  xmm9, [ac_1em5]
    jae     .ret
.kill:
    mov     dword [rbx + V_ON], 0
.ret:
    add     rsp, 40
    pop     r14
    pop     r13
    ret

; ---------------------------------------------------------------- фоны: четвёрки фильтров
; rbx = четвёрка: шум (свой ГСЧ на полосу) -> окраска -> SVF -> в суммы acc4 (L, R, реверб);
; громкости ползут к целям линейно за блок
quad_run:
    movdqa  xmm0, [rbx + Q_RNG]
    movaps  xmm1, [rbx + Q_Y]
    movaps  xmm2, [rbx + Q_I1]
    movaps  xmm3, [rbx + Q_I2]
    movaps  xmm4, [rbx + Q_GL]
    movaps  xmm5, [rbx + Q_GR]
    movaps  xmm6, [rbx + Q_GV]
    movaps  xmm7, [rbx + Q_E]
    lea     rdx, [acc4]
    xor     ecx, ecx
.l:
    movdqa  xmm8, xmm0
    pslld   xmm8, 13
    pxor    xmm0, xmm8
    movdqa  xmm8, xmm0
    psrld   xmm8, 17
    pxor    xmm0, xmm8
    movdqa  xmm8, xmm0
    pslld   xmm8, 5
    pxor    xmm0, xmm8
    cvtdq2ps xmm8, xmm0
    mulps   xmm8, [ac4_2m31]            ; белый шум −1..1
    movaps  xmm9, xmm8
    subps   xmm9, xmm1
    mulps   xmm9, [rbx + Q_C]
    addps   xmm1, xmm9                  ; окрашенный
    movaps  xmm9, xmm8
    andps   xmm9, [ac_abs]
    movaps  xmm10, [rbx + Q_THR]
    cmpltps xmm10, xmm9
    andps   xmm10, xmm8
    mulps   xmm10, [rbx + Q_WT]         ; щелчки
    movaps  xmm9, xmm8
    mulps   xmm9, [rbx + Q_WR]
    addps   xmm9, xmm10
    movaps  xmm10, xmm1
    mulps   xmm10, [rbx + Q_WL]
    addps   xmm9, xmm10                 ; x
    movaps  xmm10, xmm9
    subps   xmm10, xmm3
    mulps   xmm10, [rbx + Q_A2]
    movaps  xmm11, xmm2
    mulps   xmm11, [rbx + Q_A1]
    addps   xmm11, xmm10                ; v1
    movaps  xmm12, xmm11
    mulps   xmm12, [rbx + Q_TG]
    addps   xmm12, xmm3                 ; v2
    movaps  xmm13, xmm11
    addps   xmm13, xmm11
    subps   xmm13, xmm2
    movaps  xmm2, xmm13
    movaps  xmm13, xmm12
    addps   xmm13, xmm12
    subps   xmm13, xmm3
    movaps  xmm3, xmm13
    mulps   xmm9, [rbx + Q_M0]
    mulps   xmm11, [rbx + Q_M1]
    mulps   xmm12, [rbx + Q_M2]
    addps   xmm9, xmm11
    addps   xmm9, xmm12                 ; выход полос
    movaps  xmm10, xmm9
    mulps   xmm10, xmm4
    movaps  xmm11, xmm10
    addps   xmm10, [rdx + rcx]
    movaps  [rdx + rcx], xmm10
    movaps  xmm12, xmm9
    mulps   xmm12, xmm5
    movaps  xmm13, xmm12
    addps   xmm12, [rdx + rcx + ACC_R]
    movaps  [rdx + rcx + ACC_R], xmm12
    mulps   xmm11, xmm11
    mulps   xmm13, xmm13
    addps   xmm7, xmm11
    addps   xmm7, xmm13
    mulps   xmm9, xmm6
    addps   xmm9, [rdx + rcx + ACC_V]
    movaps  [rdx + rcx + ACC_V], xmm9
    addps   xmm4, [rbx + Q_DL]
    addps   xmm5, [rbx + Q_DR]
    addps   xmm6, [rbx + Q_DV]
    add     ecx, 16
    cmp     ecx, ABLK*16
    jb      .l
    movdqa  [rbx + Q_RNG], xmm0
    movaps  [rbx + Q_Y], xmm1
    movaps  [rbx + Q_I1], xmm2
    movaps  [rbx + Q_I2], xmm3
    movaps  [rbx + Q_GL], xmm4
    movaps  [rbx + Q_GR], xmm5
    movaps  [rbx + Q_GV], xmm6
    movaps  [rbx + Q_E], xmm7
    ret

; rbx = четвёрка, ecx = её первая полоса: коэффициенты по целям lt_*, шаги громкостей
quad_prep:
    push    r12
    push    r13
    sub     rsp, 40
    mov     r12d, ecx
    xor     r13d, r13d
.l:
    lea     eax, [r12d + r13d]
    lea     rdx, [lt]
    movss   xmm0, [rdx + rax*4 + LT_FC]
    call    a_tan
    lea     eax, [r12d + r13d]
    lea     rdx, [lt]
    movss   xmm1, [ac_1]
    divss   xmm1, [rdx + rax*4 + LT_Q]  ; k
    movaps  xmm2, xmm0
    addss   xmm2, xmm1
    mulss   xmm2, xmm0
    addss   xmm2, [ac_1]
    movss   xmm3, [ac_1]
    divss   xmm3, xmm2
    movss   [rbx + r13*4 + Q_A1], xmm3
    mulss   xmm3, xmm0
    movss   [rbx + r13*4 + Q_A2], xmm3
    movss   [rbx + r13*4 + Q_TG], xmm0
    lea     r8, [ls_md]
    mov     r8d, [r8 + rax*4]
    xorps   xmm4, xmm4
    xorps   xmm5, xmm5
    xorps   xmm6, xmm6
    cmp     r8d, 1
    jne     .nlp
    movss   xmm6, [ac_1]
    jmp     .st
.nlp:
    cmp     r8d, 2
    jne     .nbp
    movaps  xmm5, xmm1
    jmp     .st
.nbp:
    movss   xmm4, [ac_1]
    movaps  xmm5, xmm1
    xorps   xmm5, [ac_sign]
    movss   xmm6, [ac_m1]
.st:
    movss   [rbx + r13*4 + Q_M0], xmm4
    movss   [rbx + r13*4 + Q_M1], xmm5
    movss   [rbx + r13*4 + Q_M2], xmm6
    movss   xmm0, [rdx + rax*4 + LT_GL]
    subss   xmm0, [rbx + r13*4 + Q_GL]
    mulss   xmm0, [ac_inv_blk]
    movss   [rbx + r13*4 + Q_DL], xmm0
    movss   xmm0, [rdx + rax*4 + LT_GR]
    subss   xmm0, [rbx + r13*4 + Q_GR]
    mulss   xmm0, [ac_inv_blk]
    movss   [rbx + r13*4 + Q_DR], xmm0
    movss   xmm0, [rdx + rax*4 + LT_GV]
    subss   xmm0, [rbx + r13*4 + Q_GV]
    mulss   xmm0, [ac_inv_blk]
    movss   [rbx + r13*4 + Q_DV], xmm0
    inc     r13d
    cmp     r13d, 4
    jb      .l
    add     rsp, 40
    pop     r13
    pop     r12
    ret

; ---------------------------------------------------------------- фоны: четвёрки синусов
; rbx = четвёрка, ecx = первая полоса: поворот фазоров по pt_f, шаги громкостей
ph_prep:
    push    r12
    push    r13
    sub     rsp, 40
    mov     r12d, ecx
    xor     r13d, r13d
.l:
    lea     eax, [r12d + r13d]
    lea     rdx, [ptq]
    movss   xmm0, [rdx + rax*4 + PT_F]
    mulss   xmm0, [ac_2pi_fs]
    movss   [at_t0], xmm0
    fld     dword [at_t0]
    fsincos
    fstp    dword [at_t0]               ; cos
    fstp    dword [at_t1]               ; sin
    mov     r8d, [at_t0]
    mov     [rbx + r13*4 + PQ_CW], r8d
    mov     r8d, [at_t1]
    mov     [rbx + r13*4 + PQ_SW], r8d
    movss   xmm0, [rdx + rax*4 + PT_GL]
    subss   xmm0, [rbx + r13*4 + PQ_GL]
    mulss   xmm0, [ac_inv_blk]
    movss   [rbx + r13*4 + PQ_DL], xmm0
    movss   xmm0, [rdx + rax*4 + PT_GR]
    subss   xmm0, [rbx + r13*4 + PQ_GR]
    mulss   xmm0, [ac_inv_blk]
    movss   [rbx + r13*4 + PQ_DR], xmm0
    inc     r13d
    cmp     r13d, 4
    jb      .l
    ; фазоры — обратно на единичный круг
    movaps  xmm0, [rbx + PQ_RE]
    mulps   xmm0, xmm0
    movaps  xmm1, [rbx + PQ_IM]
    mulps   xmm1, xmm1
    addps   xmm0, xmm1
    mulps   xmm0, [ac4_m0_5]
    addps   xmm0, [ac4_1_5]
    movaps  xmm1, [rbx + PQ_RE]
    mulps   xmm1, xmm0
    movaps  [rbx + PQ_RE], xmm1
    movaps  xmm1, [rbx + PQ_IM]
    mulps   xmm1, xmm0
    movaps  [rbx + PQ_IM], xmm1
    add     rsp, 40
    pop     r13
    pop     r12
    ret

ph_run:
    movaps  xmm0, [rbx + PQ_RE]
    movaps  xmm1, [rbx + PQ_IM]
    movaps  xmm2, [rbx + PQ_CW]
    movaps  xmm3, [rbx + PQ_SW]
    movaps  xmm4, [rbx + PQ_GL]
    movaps  xmm5, [rbx + PQ_GR]
    movaps  xmm8, [rbx + PQ_DL]
    movaps  xmm9, [rbx + PQ_DR]
    lea     rdx, [acc4]
    xor     ecx, ecx
.l:
    movaps  xmm6, xmm0
    mulps   xmm6, xmm2
    movaps  xmm7, xmm1
    mulps   xmm7, xmm3
    subps   xmm6, xmm7
    mulps   xmm0, xmm3
    mulps   xmm1, xmm2
    addps   xmm1, xmm0
    movaps  xmm0, xmm6
    movaps  xmm6, xmm1
    mulps   xmm6, xmm4
    addps   xmm6, [rdx + rcx]
    movaps  [rdx + rcx], xmm6
    movaps  xmm7, xmm1
    mulps   xmm7, xmm5
    addps   xmm7, [rdx + rcx + ACC_R]
    movaps  [rdx + rcx + ACC_R], xmm7
    addps   xmm4, xmm8
    addps   xmm5, xmm9
    add     ecx, 16
    cmp     ecx, ABLK*16
    jb      .l
    movaps  [rbx + PQ_RE], xmm0
    movaps  [rbx + PQ_IM], xmm1
    movaps  [rbx + PQ_GL], xmm4
    movaps  [rbx + PQ_GR], xmm5
    ret

; суммы четвёрок -> моно-дорожки: 4 сэмпла разом (транспонирование 4x4)
%macro FOLD4 2
    movaps  xmm0, [%1]
    movaps  xmm1, [%1 + 16]
    movaps  xmm2, [%1 + 32]
    movaps  xmm3, [%1 + 48]
    movaps  xmm4, xmm0
    unpcklps xmm0, xmm1
    unpckhps xmm4, xmm1
    movaps  xmm5, xmm2
    unpcklps xmm2, xmm3
    unpckhps xmm5, xmm3
    addps   xmm0, xmm4
    addps   xmm2, xmm5
    movaps  xmm1, xmm0
    shufps  xmm0, xmm2, 0x44
    shufps  xmm1, xmm2, 0xEE
    addps   xmm0, xmm1
    addps   xmm0, [%2]
    movaps  [%2], xmm0
%endmacro

acc_fold:
    lea     rdx, [acc4]
    lea     r8, [mixL]
    mov     ecx, ABLK/4
.l:
    FOLD4   rdx, r8
    FOLD4   rdx + ACC_R, r8 + ABLK*4
    FOLD4   rdx + ACC_V, r8 + ABLK*8
    add     rdx, 64
    add     r8, 16
    dec     ecx
    jnz     .l
    ret

; ---------------------------------------------------------------- реверб
; сеть из 8 задержек 25–52 мс (FDN): выходы линий через ФНЧ глушения, смешаны матрицей
; Хаусхолдера (x − ¼·Σx — без потерь, каждая линия кормит все), обратно — с затуханием
; по длине линии (хвост гаснет ровно, T60 как у прежних гребёнок); вход — в линии с
; разными знаками, уши — разными суммами линий, по 2 фазовращателя на ухо. Не звенит
; нотой, как 4 гребёнки. В избе — короче и глуше
RVN         equ 8
RVM         equ 4095                    ; длина буфера линии − 1
; линия %1 (длина %2): y -> ФНЧ -> xmm1..; в xmm4 копится Σ
%macro FDNRD 2
    lea     eax, [r8 - %2]
    and     eax, RVM
    movss   xmm0, [r9 + rax*4 + %1*(RVM+1)*4]
    movss   xmm1, [rv_f + %1*4]
    subss   xmm1, xmm0
    mulss   xmm1, xmm13
    addss   xmm1, xmm0                  ; f = y + (f − y)·глушь
    movss   [rv_f + %1*4], xmm1
    addss   xmm4, xmm1
%endmacro
; линия %1: (f − ¼Σ)·g + вход·знак -> в буфер; f — в уши со знаками %2 (L), %3 (R)
%macro FDNWR 3
    movss   xmm1, [rv_f + %1*4]
    movaps  xmm0, xmm1
    subss   xmm0, xmm4
    mulss   xmm0, [rv_g + %1*4]
%if %2 > 0
    addss   xmm5, xmm1
%else
    subss   xmm5, xmm1
%endif
%if %3 > 0
    addss   xmm6, xmm1
%else
    subss   xmm6, xmm1
%endif
    movss   xmm1, [rv_in + %1*4]
    mulss   xmm1, xmm3
    addss   xmm0, xmm1
    mov     eax, r8d
    and     eax, RVM
    movss   [r9 + rax*4 + %1*(RVM+1)*4], xmm0
%endmacro

%macro ALLP 3
    lea     eax, [r8 - %2]
    and     eax, 1023
    movss   xmm7, [r10 + rax*4 + %1*4096]
    movaps  xmm14, %3
    mulss   xmm14, [ac_0_5]
    subss   xmm7, xmm14                 ; y = b − x/2
    movaps  xmm14, xmm7
    mulss   xmm14, [ac_0_5]
    addss   xmm14, %3                   ; в линию: x + y/2
    mov     eax, r8d
    and     eax, 1023
    movss   [r10 + rax*4 + %1*4096], xmm14
    movaps  %3, xmm7
%endmacro

rev_run:
    push    rbx
    sub     rsp, 32
    ; затухание за проход линии: g = fb^(длина / 1400) — хвост гаснет одинаково во всех
    ; линиях; fb = 0.74 на улице, 0.58 в избе (T60 ≈ 0.7 и 0.4 с, как у прежних гребёнок)
    movss   xmm0, [bs + AP_IN]
    mulss   xmm0, [ac_m0_16]
    addss   xmm0, [ac_0_74]
    movss   [at_t1], xmm0
    fld1
    fld     dword [at_t1]
    fyl2x                               ; log2 fb
    fstp    dword [at_t1]
    xor     ebx, ebx
.g:
    lea     rdx, [rv_len]
    cvtsi2ss xmm0, dword [rdx + rbx*4]
    mulss   xmm0, [ac_i1400]
    mulss   xmm0, [at_t1]               ; log2 g
    mulss   xmm0, [ac_ln2]
    call    a_exp
    lea     rdx, [rv_g]
    movss   [rdx + rbx*4], xmm0
    inc     ebx
    cmp     ebx, RVN
    jb      .g
    movss   xmm13, [bs + AP_IN]
    mulss   xmm13, [ac_0_3]
    addss   xmm13, [ac_0_3]             ; глушь
    mov     r8d, [rv_p]
    lea     r9, [rv_c]
    lea     r10, [rv_a]
    lea     rdx, [mixL]
    xor     r11d, r11d
.l:
    movss   xmm3, [rdx + r11*4 + ABLK*8]
    xorps   xmm4, xmm4
    FDNRD   0, 1116
    FDNRD   1, 1277
    FDNRD   2, 1422
    FDNRD   3, 1617
    FDNRD   4, 1787
    FDNRD   5, 1949
    FDNRD   6, 2111
    FDNRD   7, 2293
    mulss   xmm4, [ac_0_25]             ; ¼·Σ
    xorps   xmm5, xmm5
    xorps   xmm6, xmm6
    FDNWR   0, 1, 1
    FDNWR   1, -1, 1
    FDNWR   2, 1, -1
    FDNWR   3, -1, -1
    FDNWR   4, 1, 1
    FDNWR   5, -1, 1
    FDNWR   6, 1, -1
    FDNWR   7, -1, -1
    mulss   xmm5, [ac_rv_out]
    mulss   xmm6, [ac_rv_out]
    ALLP    0, 556, xmm5
    ALLP    1, 341, xmm5
    ALLP    2, 579, xmm6
    ALLP    3, 367, xmm6
    addss   xmm5, [rdx + r11*4]
    movss   [rdx + r11*4], xmm5
    addss   xmm6, [rdx + r11*4 + ABLK*4]
    movss   [rdx + r11*4 + ABLK*4], xmm6
    inc     r8d
    inc     r11d
    cmp     r11d, ABLK
    jb      .l
    and     r8d, 0xFFFFF
    mov     [rv_p], r8d
    ; NaN в линиях (сломался какой-то голос) — реверб с нуля, а не тишина до конца игры
    movss   xmm0, [rv_f]
    addss   xmm0, [rv_f + 28]
    addss   xmm0, xmm5
    addss   xmm0, xmm6
    ucomiss xmm0, xmm0
    jnp     .ok
    push    rdi
    lea     rdi, [rv_c]
    xor     eax, eax
    mov     ecx, RVN*(RVM+1) + 4*1024
    rep     stosd                       ; rv_c, rv_a — подряд
    pop     rdi
    xorps   xmm0, xmm0
    movups  [rv_f], xmm0
    movups  [rv_f + 16], xmm0
.ok:
    add     rsp, 32
    pop     rbx
    ret

; ---------------------------------------------------------------- выход
; глушь при смерти (ФНЧ), громкость (M — выкл), мягкий предел выше 0.6 -> int16
%macro SOFTCLIP 1
    movaps  xmm1, xmm0
    andps   xmm1, [ac_abs]
    maxss   xmm15, xmm1
    comiss  xmm1, [ac_knee]
    jbe     %%lin
    inc     dword [st_knee]
    movaps  xmm2, xmm0
    andps   xmm2, [ac_sign]
    subss   xmm1, [ac_knee]
    mulss   xmm1, [ac_2_5]
    minss   xmm1, [ac_3]
    movaps  xmm3, xmm1
    mulss   xmm3, xmm1
    movaps  xmm14, xmm3
    addss   xmm3, [ac_27]
    mulss   xmm3, xmm1
    mulss   xmm14, [ac_9]
    addss   xmm14, [ac_27]
    divss   xmm3, xmm14                 ; th z ≈ z(27 + z²)/(27 + 9z²)
    mulss   xmm3, [ac_0_4]
    addss   xmm3, [ac_knee]
    orps    xmm3, xmm2
    movaps  xmm0, xmm3
%%lin:
    mulss   xmm0, [ac_32767]
    cvtss2si eax, xmm0
    mov     [%1], ax
%endmacro

master_out:
    push    rdi
    movss   xmm9, [bs + AP_DEAD]
    mulss   xmm9, [ac_m0_93]
    addss   xmm9, [ac_1]
    movss   xmm10, [mo_g]
    movss   xmm11, [bs + AP_VOL]
    mulss   xmm11, [ac_master]
    movss   [mo_g], xmm11
    subss   xmm11, xmm10
    mulss   xmm11, [ac_inv_blk]
    movss   xmm12, [mo_l]
    movss   xmm13, [mo_r]
    movss   xmm15, [st_peak]
    lea     rdx, [mixL]
    mov     rdi, [ab_out]
    mov     ecx, ABLK
.l:
    movss   xmm0, [rdx]
    subss   xmm0, xmm12
    mulss   xmm0, xmm9
    addss   xmm12, xmm0
    movaps  xmm0, xmm12
    mulss   xmm0, xmm10
    SOFTCLIP rdi
    movss   xmm0, [rdx + ABLK*4]
    subss   xmm0, xmm13
    mulss   xmm0, xmm9
    addss   xmm13, xmm0
    movaps  xmm0, xmm13
    mulss   xmm0, xmm10
    SOFTCLIP rdi + 2
    addss   xmm10, xmm11
    add     rdx, 4
    add     rdi, 4
    dec     ecx
    jnz     .l
    ; NaN в выходном ФНЧ — с нуля (иначе тишина до конца игры)
    movaps  xmm0, xmm12
    addss   xmm0, xmm13
    ucomiss xmm0, xmm0
    jnp     .fin
    xorps   xmm12, xmm12
    xorps   xmm13, xmm13
.fin:
    movss   [mo_l], xmm12
    movss   [mo_r], xmm13
    movss   [st_peak], xmm15
    pop     rdi
    ret

; ---------------------------------------------------------------- сброс
aud_reset:
    push    rdi
    sub     rsp, 32
    lea     rdi, [aud_z0]
    lea     rcx, [aud_z1]
    sub     rcx, rdi
    shr     rcx, 3
    xor     eax, eax
    rep     stosq
    mov     dword [at_rng], 0x9E3779B9
    mov     dword [ev_head], 0
    mov     dword [ev_tail], 0
    lea     rdx, [quads]
    lea     r8, [ls_col]
    xor     ecx, ecx
.q:
    mov     eax, ecx
    shr     eax, 2
    imul    eax, eax, QSZ
    lea     r9, [rdx + rax]
    mov     eax, ecx
    and     eax, 3
    lea     r9, [r9 + rax*4]
    mov     eax, [r8]
    mov     [r9 + Q_C], eax
    mov     eax, [r8 + 4]
    mov     [r9 + Q_WR], eax
    mov     eax, [r8 + 8]
    mov     [r9 + Q_WL], eax
    mov     eax, [r8 + 12]
    mov     [r9 + Q_THR], eax
    mov     eax, [r8 + 16]
    mov     [r9 + Q_WT], eax
    imul    eax, ecx, 0x2545F491
    add     eax, 0x6B43A9B5
    or      eax, 1
    mov     [r9 + Q_RNG], eax
    add     r8, 20
    inc     ecx
    cmp     ecx, NQUAD*4
    jb      .q
    lea     rdx, [pquads]
    mov     ecx, NPQ
    movaps  xmm0, [ac4_1]
.p:
    movaps  [rdx + PQ_RE], xmm0
    add     rdx, PQSZ
    dec     ecx
    jnz     .p
    add     rsp, 32
    pop     rdi
    ret

; ---------------------------------------------------------------- управление фонами
; ecx = полоса, xmm0 = срез, xmm1 = Q, xmm2 = левое, xmm3 = правое, xmm4 = в реверб
lane_set:
    lea     rdx, [lt]
    movss   [rdx + rcx*4 + LT_FC], xmm0
    movss   [rdx + rcx*4 + LT_Q], xmm1
    movss   [rdx + rcx*4 + LT_GL], xmm2
    movss   [rdx + rcx*4 + LT_GR], xmm3
    movss   [rdx + rcx*4 + LT_GV], xmm4
    ret

; то же с панорамой: xmm2 = громкость, xmm3 = панорама
lane_pan:
    movss   [bt_fc], xmm0
    movss   [bt_q], xmm1
    movss   [bt_gv], xmm4
    movaps  xmm0, xmm3
    movaps  xmm1, xmm2
    call    a_pan
    movaps  xmm2, xmm0
    movaps  xmm3, xmm1
    movss   xmm0, [bt_fc]
    movss   xmm1, [bt_q]
    movss   xmm4, [bt_gv]
    jmp     lane_set

; xmm0 = сила ветра -> срез шороха 150 + 900 s + 500 s² (до 7 кГц)
wind_fc:
    maxss   xmm0, [ac_0]
    movaps  xmm1, xmm0
    mulss   xmm1, [ac_500]
    addss   xmm1, [ac_900]
    mulss   xmm1, xmm0
    addss   xmm1, [ac_150]
    minss   xmm1, [ac_7000]
    movaps  xmm0, xmm1
    ret

; xmm0 = s -> xmm0 = max(0, s)² (для буханья и берёз)
a_sq0:
    maxss   xmm0, [ac_0]
    mulss   xmm0, xmm0
    ret

bed_ctl:
    push    rbx
    push    r12
    sub     rsp, 40
    ; --- энергия полос за прошлый блок (только --wav)
    cmp     byte [aud_wav], 0
    je      .nost
    lea     rbx, [quads]
    lea     rdx, [st_lane]
    mov     ecx, NQUAD
.st:
    movaps  xmm0, [rbx + Q_E]
    cvtps2pd xmm1, xmm0
    addpd   xmm1, [rdx]
    movapd  [rdx], xmm1
    movhlps xmm0, xmm0
    cvtps2pd xmm1, xmm0
    addpd   xmm1, [rdx + 16]
    movapd  [rdx + 16], xmm1
    xorps   xmm0, xmm0
    movaps  [rbx + Q_E], xmm0
    add     rbx, QSZ
    add     rdx, 32
    dec     ecx
    jnz     .st
    ; след параметров по блокам -> atrace.bin (32 float на блок)
    mov     eax, [tr_n]
    cmp     eax, TRMAX
    jae     .nost
    inc     dword [tr_n]
    shl     eax, 7
    lea     rdx, [trbuf]
    add     rdx, rax
    lea     r8, [ap]
    xor     ecx, ecx
.tr:
    movaps  xmm0, [r8 + rcx]
    movaps  [rdx + rcx], xmm0
    add     ecx, 16
    cmp     ecx, NAP*4
    jb      .tr
    lea     r8, [voices]                ; последнее поле — сколько голосов звучит
    xor     eax, eax
    mov     ecx, NVOICE
.tv:
    cmp     dword [r8 + V_ON], 0
    je      .tvn
    inc     eax
.tvn:
    add     r8, VSZ
    dec     ecx
    jnz     .tv
    cvtsi2ss xmm0, eax
    movss   [rdx + NAP*4 - 4], xmm0
.nost:
    ; --- параметры игры сглаживаются (у каждого своя скорость)
    lea     rdx, [ap]
    lea     r8, [bs]
    lea     r9, [bs_a]
    xor     ecx, ecx
.sm:
    movss   xmm0, [rdx + rcx*4]
    subss   xmm0, [r8 + rcx*4]
    mulss   xmm0, [r9 + rcx*4]
    addss   xmm0, [r8 + rcx*4]
    movss   [r8 + rcx*4], xmm0
    inc     ecx
    cmp     ecx, NAP
    jb      .sm
    ; --- блуждания: буханье, провода, высота воя
    call    a_rndf
    comiss  xmm0, [ac_0_15]
    jae     .bf1
    call    a_rndf
    movss   [bf_t], xmm0
.bf1:
    movss   xmm0, [bf_t]
    subss   xmm0, [bf]
    mulss   xmm0, [ac_0_3]
    addss   xmm0, [bf]
    movss   [bf], xmm0
    call    a_rndf
    comiss  xmm0, [ac_0_05]
    jae     .wf1
    call    a_rndf
    movss   [wfl_t], xmm0
.wf1:
    movss   xmm0, [wfl_t]
    subss   xmm0, [wfl]
    mulss   xmm0, [ac_0_1]
    addss   xmm0, [wfl]
    movss   [wfl], xmm0
    xor     r12d, r12d
.hw:
    call    a_rnds
    lea     rdx, [hw]
    subss   xmm0, [rdx + r12*4]
    mulss   xmm0, [ac_0_02]
    addss   xmm0, [rdx + r12*4]
    movss   [rdx + r12*4], xmm0
    inc     r12d
    cmp     r12d, 4
    jb      .hw
    movss   xmm0, [ac_1]
    subss   xmm0, [bs + AP_IN]
    movss   [bt_out], xmm0              ; на улице
    ; === 0: ветер в ушах — шорох, со стороны, откуда дует, громче; слева и справа — свой
    ;        ветер (порыв приходит в одно ухо раньше)
    movss   xmm0, [bs + AP_WPAN]
    mulss   xmm0, [ac_0_35]
    movss   xmm1, [ac_1]
    subss   xmm1, xmm0
    movss   [bt_sl], xmm1
    addss   xmm0, [ac_1]
    movss   [bt_sr], xmm0
    movss   xmm0, [bs + AP_WL]
    call    a_loud
    mulss   xmm0, [bt_out]
    mulss   xmm0, [bt_sl]
    movss   [bt_al], xmm0
    movss   xmm0, [bs + AP_WR]
    call    a_loud
    mulss   xmm0, [bt_out]
    mulss   xmm0, [bt_sr]
    movss   [bt_ar], xmm0
    movss   xmm0, [bs + AP_WL]
    call    wind_fc
    movss   [bt_fl], xmm0
    movss   xmm0, [bs + AP_WR]
    call    wind_fc
    movss   [bt_fr], xmm0
    mov     ecx, 0
    movss   xmm0, [bt_fl]
    movss   xmm1, [ac_0_7]
    movss   xmm2, [bt_al]
    mulss   xmm2, [ac_kn]
    movaps  xmm3, xmm2
    mulss   xmm3, [ac_0_2]
    xorps   xmm4, xmm4
    call    lane_set
    mov     ecx, 1
    movss   xmm0, [bt_fr]
    movss   xmm1, [ac_0_7]
    movss   xmm3, [bt_ar]
    mulss   xmm3, [ac_kn]
    movaps  xmm2, xmm3
    mulss   xmm2, [ac_0_2]
    xorps   xmm4, xmm4
    call    lane_set
    mov     ecx, 2
    movss   xmm0, [bt_fl]
    mulss   xmm0, [ac_0_55]
    movss   xmm1, [ac_1_6]
    movss   xmm2, [bt_al]
    mulss   xmm2, [ac_knb]
    movaps  xmm3, xmm2
    mulss   xmm3, [ac_0_2]
    xorps   xmm4, xmm4
    call    lane_set
    mov     ecx, 3
    movss   xmm0, [bt_fr]
    mulss   xmm0, [ac_0_55]
    movss   xmm1, [ac_1_6]
    movss   xmm3, [bt_ar]
    mulss   xmm3, [ac_knb]
    movaps  xmm2, xmm3
    mulss   xmm2, [ac_0_2]
    xorps   xmm4, xmm4
    call    lane_set
    ; === 1: гул над селом (в избе — глухо), буханье порывов в уши, вой в трубе
    movss   xmm0, [bs + AP_WF]
    call    a_loud
    movss   [bt_af], xmm0
    movss   xmm0, [bs + AP_WF]
    mulss   xmm0, [ac_380]
    addss   xmm0, [ac_200]
    movss   xmm1, [ac_160]
    subss   xmm1, xmm0
    mulss   xmm1, [bs + AP_IN]
    addss   xmm0, xmm1
    movss   [bt_ff], xmm0
    movss   xmm0, [bs + AP_IN]
    mulss   xmm0, [ac_m0_65]
    addss   xmm0, [ac_1]
    mulss   xmm0, [bt_af]
    mulss   xmm0, [ac_kf]
    movss   [bt_g], xmm0
    mov     ecx, 4
    movss   xmm0, [bt_ff]
    movss   xmm1, [ac_0_6]
    movss   xmm2, [bt_g]
    movaps  xmm3, xmm2
    mulss   xmm3, [ac_0_35]
    xorps   xmm4, xmm4
    call    lane_set
    mov     ecx, 5
    movss   xmm0, [bt_ff]
    mulss   xmm0, [ac_1_1]
    movss   xmm1, [ac_0_6]
    movss   xmm3, [bt_g]
    movaps  xmm2, xmm3
    mulss   xmm2, [ac_0_35]
    xorps   xmm4, xmm4
    call    lane_set
    ; буханье: (s − 0.35)², дрожит
    movss   xmm0, [bs + AP_WN]
    subss   xmm0, [ac_0_35]
    call    a_sq0
    mulss   xmm0, [bt_out]
    movss   xmm1, [bf]
    mulss   xmm1, [ac_0_65]
    addss   xmm1, [ac_0_35]
    mulss   xmm0, xmm1
    mulss   xmm0, [ac_kb]
    movaps  xmm2, xmm0
    movaps  xmm3, xmm0
    mov     ecx, 6
    movss   xmm0, [ac_75]
    movss   xmm1, [ac_0_8]
    xorps   xmm4, xmm4
    call    lane_set
    ; труба: 95 + 55 s, гуляет; подвывает только в порыв (GX²) — ровный гул в избе
    ; слышался как «ветер, да не ветер»
    movss   xmm0, [bs + AP_WF]
    mulss   xmm0, [ac_55]
    addss   xmm0, [ac_95]
    movss   xmm1, [hw + 12]
    mulss   xmm1, [ac_0_8]
    addss   xmm1, [ac_1]
    mulss   xmm0, xmm1
    movss   [bt_fch], xmm0
    movss   xmm0, [bs + AP_GX]
    mulss   xmm0, xmm0
    mulss   xmm0, [bt_af]
    mulss   xmm0, [bs + AP_IN]
    mulss   xmm0, [ac_kc]
    movss   [bt_gch], xmm0
    mov     ecx, 7
    movss   xmm0, [bt_fch]
    movss   xmm1, [ac_7]
    movss   xmm2, [bt_gch]
    movaps  xmm3, xmm2
    xorps   xmm4, xmm4
    call    lane_set
    ; === 2: вой на углах изб и заборов — выше и громче в порыв; вторая труба
    movss   xmm0, [bs + AP_WF]
    subss   xmm0, [ac_0_5]
    mulss   xmm0, [ac_1_25]
    CLAMP01 xmm0
    movaps  xmm1, xmm0
    sqrtss  xmm1, xmm1
    mulss   xmm0, xmm1
    movss   xmm1, [bs + AP_EDGE]
    mulss   xmm1, [ac_0_7]
    addss   xmm1, [ac_0_3]
    mulss   xmm0, xmm1
    movss   xmm1, [bs + AP_GX]
    mulss   xmm1, [ac_0_55]
    addss   xmm1, [ac_0_45]
    mulss   xmm0, xmm1
    movss   xmm1, [bs + AP_IN]
    mulss   xmm1, [ac_m0_75]
    addss   xmm1, [ac_1]
    mulss   xmm0, xmm1
    mulss   xmm0, [ac_kh]
    movss   [bt_h], xmm0
    movss   xmm0, [bs + AP_WF]
    mulss   xmm0, [ac_0_35]
    addss   xmm0, [ac_0_55]
    movss   xmm1, [bs + AP_GX]
    mulss   xmm1, [ac_0_25]
    addss   xmm0, xmm1
    movss   [bt_pf], xmm0
    xor     r12d, r12d
.howl:
    lea     rdx, [howl_b]
    movss   xmm0, [rdx + r12*4]
    mulss   xmm0, [bt_pf]
    lea     rdx, [hw]
    movss   xmm1, [rdx + r12*4]
    mulss   xmm1, [ac_0_8]
    addss   xmm1, [ac_1]
    mulss   xmm0, xmm1
    lea     rdx, [howl_q]
    movss   xmm1, [rdx + r12*4]
    lea     rdx, [howl_w]
    movss   xmm2, [rdx + r12*4]
    mulss   xmm2, [bt_h]
    movss   xmm3, [bs + AP_WPAN]
    mulss   xmm3, [ac_0_5]
    movaps  xmm4, xmm2
    mulss   xmm4, [ac_0_3]
    lea     ecx, [r12d + 8]
    call    lane_pan
    inc     r12d
    cmp     r12d, 3
    jb      .howl
    mov     ecx, 11
    movss   xmm0, [bt_fch]
    mulss   xmm0, [ac_1_93]
    movss   xmm1, [ac_9]
    movss   xmm2, [bt_gch]
    mulss   xmm2, [ac_0_55]
    movaps  xmm3, xmm2
    xorps   xmm4, xmm4
    call    lane_set
    ; === 3: ели шумят, берёзы свистят ветками (по кронам — ветер над крышами), позёмка шуршит
    mov     ecx, 12
    movss   xmm0, [bs + AP_WF]
    mulss   xmm0, [ac_260]
    addss   xmm0, [ac_520]
    movss   xmm1, [ac_0_9]
    movss   xmm2, [bs + AP_IN]
    mulss   xmm2, [ac_m0_8]
    addss   xmm2, [ac_1]
    mulss   xmm2, [bt_af]
    mulss   xmm2, [bs + AP_SPR]
    mulss   xmm2, [ac_ks]
    movss   xmm3, [bs + AP_SPRP]
    movaps  xmm4, xmm2
    mulss   xmm4, [ac_0_2]
    call    lane_pan
    movss   xmm0, [bs + AP_WF]
    subss   xmm0, [ac_0_35]
    call    a_sq0
    movss   xmm1, [bs + AP_IN]
    mulss   xmm1, [ac_m0_9]
    addss   xmm1, [ac_1]
    mulss   xmm0, xmm1
    mulss   xmm0, [bs + AP_BIR]
    mulss   xmm0, [ac_kbi]
    movaps  xmm2, xmm0
    mov     ecx, 13
    movss   xmm0, [bs + AP_WF]
    mulss   xmm0, [ac_800]
    addss   xmm0, [ac_2300]
    movss   xmm1, [ac_3_5]
    movss   xmm3, [bs + AP_BIRP]
    xorps   xmm4, xmm4
    call    lane_pan
    movss   xmm0, [bs + AP_HISS]
    mulss   xmm0, [bt_out]
    mulss   xmm0, [ac_khs]
    movss   [bt_g], xmm0
    mov     ecx, 14
    movss   xmm0, [ac_5500]
    movss   xmm1, [ac_0_55]
    movss   xmm2, [bt_g]
    mulss   xmm2, [bt_sl]
    movaps  xmm3, xmm2
    mulss   xmm3, [ac_0_25]
    xorps   xmm4, xmm4
    call    lane_set
    mov     ecx, 15
    movss   xmm0, [ac_6200]
    movss   xmm1, [ac_0_55]
    movss   xmm3, [bt_g]
    mulss   xmm3, [bt_sr]
    movaps  xmm2, xmm3
    mulss   xmm2, [ac_0_25]
    xorps   xmm4, xmm4
    call    lane_set
    ; === 4: печь гудит и трещит, телевизор бубнит за стеной. Гул — не ровный шорох (тот —
    ;   ветер), а пламя: трепещет (новая цель в среднем раз в 60 мс, догоняется за ~30),
    ;   и тихо. Треск — вспышками: всплеск силы гаснет за ~0.4 с, между — редкие щелчки
    call    a_rndf
    comiss  xmm0, [ac_0_2]
    jae     .fl1
    call    a_rndf
    movss   [st_flt], xmm0
.fl1:
    movss   xmm0, [st_flt]
    subss   xmm0, [st_fl]
    mulss   xmm0, [ac_0_35]
    addss   xmm0, [st_fl]
    movss   [st_fl], xmm0
    movss   xmm0, [st_cr]
    mulss   xmm0, [ac_0_97]
    movss   [st_cr], xmm0
    call    a_rndf
    comiss  xmm0, [ac_0_025]
    jae     .cr1
    call    a_rndf
    mulss   xmm0, [ac_0_8]
    addss   xmm0, [st_cr]
    minss   xmm0, [ac_1_5]
    movss   [st_cr], xmm0
.cr1:
    mov     ecx, 16
    movss   xmm0, [ac_140]
    movss   xmm1, [ac_0_8]
    movss   xmm2, [st_fl]
    mulss   xmm2, [ac_0_65]
    addss   xmm2, [ac_0_35]
    mulss   xmm2, [bs + AP_FIRE]
    mulss   xmm2, [ac_kfi]
    movss   xmm3, [bs + AP_FIRP]
    movaps  xmm4, xmm2
    mulss   xmm4, [ac_0_3]
    call    lane_pan
    mov     ecx, 17
    movss   xmm0, [st_fl]
    mulss   xmm0, [ac_0_4]
    addss   xmm0, [ac_0_8]
    mulss   xmm0, [ac_2400]
    movss   xmm1, [ac_1_1]
    movss   xmm2, [st_cr]
    addss   xmm2, [ac_0_12]
    mulss   xmm2, [bs + AP_FIRE]
    mulss   xmm2, [ac_kcr]
    movss   xmm3, [bs + AP_FIRP]
    movaps  xmm4, xmm2
    mulss   xmm4, [ac_0_3]
    call    lane_pan
    ; слоги: новые форманты и громкость каждые 0.1–0.3 с, иногда пауза
    movss   xmm0, [tv_st]
    subss   xmm0, [ac_blk_s]
    movss   [tv_st], xmm0
    comiss  xmm0, [ac_0]
    ja      .tvs
    call    a_rndf
    comiss  xmm0, [ac_0_18]
    jae     .syl
    mov     dword [tv_sg], 0
    call    a_rndf
    mulss   xmm0, [ac_0_5]
    addss   xmm0, [ac_0_2]
    movss   [tv_st], xmm0
    jmp     .tvs
.syl:
    call    a_rndf
    mulss   xmm0, [ac_0_65]
    addss   xmm0, [ac_0_35]
    movss   [tv_sg], xmm0
    call    a_rndf
    mulss   xmm0, [ac_500]
    addss   xmm0, [ac_350]
    movss   [tv_f1], xmm0
    call    a_rndf
    mulss   xmm0, [ac_1300]
    addss   xmm0, [ac_900]
    movss   [tv_f2], xmm0
    call    a_rndf
    mulss   xmm0, [ac_0_2]
    addss   xmm0, [ac_0_09]
    movss   [tv_st], xmm0
.tvs:
    movss   xmm0, [tv_sg]
    subss   xmm0, [tv_sc]
    mulss   xmm0, [ac_0_5]
    addss   xmm0, [tv_sc]
    movss   [tv_sc], xmm0
    mulss   xmm0, [bs + AP_TV]
    mulss   xmm0, [bs + AP_TVL]
    mulss   xmm0, [ac_ktv]
    movss   [bt_g], xmm0
    mov     ecx, 18
    movss   xmm0, [tv_f1]
    movss   xmm1, [ac_4]
    movss   xmm2, [bt_g]
    movss   xmm3, [bs + AP_TVP]
    movaps  xmm4, xmm2
    mulss   xmm4, [ac_0_3]
    call    lane_pan
    mov     ecx, 19
    movss   xmm0, [tv_f2]
    movss   xmm1, [ac_6]
    movss   xmm2, [bs + AP_TVIN]        ; верхняя форманта — только в той же избе
    mulss   xmm2, [ac_0_85]
    addss   xmm2, [ac_0_15]
    mulss   xmm2, [bt_g]
    mulss   xmm2, [ac_0_6]
    movss   xmm3, [bs + AP_TVP]
    movaps  xmm4, xmm2
    mulss   xmm4, [ac_0_3]
    call    lane_pan
    ; === синусы 0..3: провода поют (тон — от ветра над крышами), 4..7: гул фонарей
    movss   xmm0, [bs + AP_WF]
    subss   xmm0, [ac_0_3]
    mulss   xmm0, [ac_1_6]
    CLAMP01 xmm0
    movss   xmm1, [wfl]
    mulss   xmm1, [ac_0_5]
    addss   xmm1, [ac_0_5]
    mulss   xmm0, xmm1
    mulss   xmm0, [bt_out]
    mulss   xmm0, [bs + AP_WIRE]
    mulss   xmm0, [ac_kw]
    movaps  xmm1, xmm0
    movss   xmm0, [bs + AP_WIRP]
    call    a_pan
    movss   [bt_al], xmm0
    movss   [bt_ar], xmm1
    movss   xmm0, [wfl]
    subss   xmm0, [ac_0_5]
    mulss   xmm0, [ac_0_03]
    addss   xmm0, [ac_1]
    mulss   xmm0, [bs + AP_WIRF]
    movss   [bt_g], xmm0
    lea     rdx, [ptq]
    lea     r8, [wire_m]
    lea     r9, [wire_w]
    xor     ecx, ecx
.wl:
    movss   xmm0, [bt_g]
    mulss   xmm0, [r8 + rcx*4]
    movss   [rdx + rcx*4 + PT_F], xmm0
    movss   xmm0, [bt_al]
    mulss   xmm0, [r9 + rcx*4]
    movss   [rdx + rcx*4 + PT_GL], xmm0
    movss   xmm0, [bt_ar]
    mulss   xmm0, [r9 + rcx*4]
    movss   [rdx + rcx*4 + PT_GR], xmm0
    inc     ecx
    cmp     ecx, 4
    jb      .wl
    movss   xmm0, [bs + AP_IN]
    mulss   xmm0, [ac_m0_85]
    addss   xmm0, [ac_1]
    movss   [bt_g], xmm0
    movss   xmm1, [bs + AP_HUM]
    mulss   xmm1, [ac_khu]
    mulss   xmm1, [bt_g]
    movss   xmm0, [bs + AP_HUMP]
    call    a_pan
    movss   [bt_al], xmm0
    movss   [bt_ar], xmm1
    movss   xmm1, [bs + AP_BRK]
    mulss   xmm1, [ac_kbr]
    mulss   xmm1, [bt_g]
    movss   xmm0, [bs + AP_BRKP]
    call    a_pan
    movss   [bt_fl], xmm0
    movss   [bt_fr], xmm1
    lea     rdx, [ptq]
    lea     r8, [hum_w]
    lea     r9, [brk_w]
    xor     ecx, ecx
.hl:
    lea     r10, [hum_f]
    mov     eax, [r10 + rcx*4]
    mov     [rdx + rcx*4 + PT_F + 16], eax
    movss   xmm0, [bt_al]
    mulss   xmm0, [r8 + rcx*4]
    movss   xmm1, [bt_fl]
    mulss   xmm1, [r9 + rcx*4]
    addss   xmm0, xmm1
    movss   [rdx + rcx*4 + PT_GL + 16], xmm0
    movss   xmm0, [bt_ar]
    mulss   xmm0, [r8 + rcx*4]
    movss   xmm1, [bt_fr]
    mulss   xmm1, [r9 + rcx*4]
    addss   xmm0, xmm1
    movss   [rdx + rcx*4 + PT_GR + 16], xmm0
    inc     ecx
    cmp     ecx, 4
    jb      .hl
    ; --- коэффициенты четвёрок
    lea     rbx, [quads]
    xor     r12d, r12d
.qp:
    mov     ecx, r12d
    call    quad_prep
    add     rbx, QSZ
    add     r12d, 4
    cmp     r12d, NQUAD*4
    jb      .qp
    lea     rbx, [pquads]
    xor     ecx, ecx
    call    ph_prep
    add     rbx, PQSZ
    mov     ecx, 4
    call    ph_prep
    ; --- сердце: тук-тук с частотой из игры
    movss   xmm0, [bs + AP_HB]
    comiss  xmm0, [ac_0_03]
    ja      .hb
    mov     dword [hb_ph], __float32__(0.85)
    jmp     .hbd
.hb:
    movss   xmm0, [bs + AP_HBR]
    mulss   xmm0, [ac_blk_s]
    addss   xmm0, [hb_ph]
    movss   [hb_ph], xmm0
    comiss  xmm0, [ac_1]
    jb      .hbd
    subss   xmm0, [ac_1]
    movss   [hb_ph], xmm0
    mov     ecx, SN_HB
    movss   xmm0, [bs + AP_HB]
    xorps   xmm1, xmm1
    call    play_int
.hbd:
    ; --- печь: поленья стреляют — чаще во вспышку треска; изредка шипит сырое
    movss   xmm0, [bs + AP_FIRE]
    comiss  xmm0, [ac_0_02]
    jbe     .npop
    call    a_rndf
    movss   xmm1, [st_cr]
    mulss   xmm1, [ac_1_5]
    addss   xmm1, [ac_0_3]
    mulss   xmm1, [bs + AP_FIRE]
    mulss   xmm1, [ac_pop_p]
    comiss  xmm0, xmm1
    jae     .nsz
    mov     ecx, SN_POP
    movss   xmm0, [bs + AP_FIRE]
    movss   xmm1, [bs + AP_FIRP]
    call    play_int
.nsz:
    call    a_rndf
    movss   xmm1, [bs + AP_FIRE]
    mulss   xmm1, [ac_sz_p]
    comiss  xmm0, xmm1
    jae     .npop
    mov     ecx, SN_SIZZLE
    movss   xmm0, [bs + AP_FIRE]
    movss   xmm1, [bs + AP_FIRP]
    call    play_int
.npop:
    add     rsp, 40
    pop     r12
    pop     rbx
    ret

; ecx = звук, xmm0 = громкость, xmm1 = панорама: голос из самого потока звука
play_int:
    push    rsi
    sub     rsp, 32
    lea     rsi, [ai_ev]
    mov     [rsi + EV_SND], ecx
    movss   [rsi + EV_VOL], xmm0
    movaps  xmm0, xmm1
    movss   xmm1, [ac_1]
    call    a_pan
    movss   [rsi + EV_GL], xmm0
    movss   [rsi + EV_GR], xmm1
    mov     dword [rsi + EV_CL], __float32__(1.0)
    mov     dword [rsi + EV_CR], __float32__(1.0)
    mov     dword [rsi + EV_RV], __float32__(0.08)
    mov     dword [rsi + EV_P], __float32__(1.0)
    mov     dword [rsi + EV_DEL], 0
    mov     dword [rsi + EV_SEED], 0
    mov     dword [rsi + EV_A], 0
    call    ev_start
    add     rsp, 32
    pop     rsi
    ret

; ================================================================ сторона игры
; ГСЧ игры для звука (своё зерно: село, зомби и картинка от звука не меняются)
g_rnd:
    mov     eax, [gt_rng]
    mov     r11d, eax
    shl     r11d, 13
    xor     eax, r11d
    mov     r11d, eax
    shr     r11d, 17
    xor     eax, r11d
    mov     r11d, eax
    shl     r11d, 5
    xor     eax, r11d
    mov     [gt_rng], eax
    ret
g_rndf:
    call    g_rnd
    shr     eax, 8
    cvtsi2ss xmm0, eax
    mulss   xmm0, [ac_2m24]
    ret

; xmm0 = срез, Гц -> коэффициент однополюсного ФНЧ 1 − e^(−2π fc/fs)
g_lpc:
    minss   xmm0, [ac_fcmax]
    mulss   xmm0, [ac_m2pi_fs]
    movss   [gt_t0], xmm0
    fld     dword [gt_t0]
    fldl2e
    fmulp   st1, st0
    fld     st0
    frndint
    fxch    st1
    fsub    st0, st1
    f2xm1
    fld1
    faddp   st1, st0
    fscale
    fstp    st1
    fstp    dword [gt_t0]
    movss   xmm0, [ac_1]
    subss   xmm0, [gt_t0]
    ret

; xmm0 = x, xmm1 = y -> eax = номер избы + 1 (0 — улица, за краем — улица)
g_room:
    cvttss2si eax, xmm0
    cvttss2si ecx, xmm1
    cmp     eax, MAPW
    jae     .out
    cmp     ecx, MAPW
    jae     .out
    shl     ecx, 7
    add     eax, ecx
    lea     rcx, [rmap]
    movzx   eax, byte [rcx + rax]
    ret
.out:
    xor     eax, eax
    ret

; кладёт gt_ev в кольцо (полное — событие пропадает)
ev_push:
    mov     eax, [ev_head]
    mov     ecx, eax
    sub     ecx, [ev_tail]
    cmp     ecx, NEV - 1
    jae     .drop
    mov     ecx, eax
    and     ecx, NEV - 1
    shl     ecx, 6
    lea     rdx, [evq]
    add     rdx, rcx
    movdqa  xmm0, [gt_ev]
    movdqa  [rdx], xmm0
    movdqa  xmm0, [gt_ev + 16]
    movdqa  [rdx + 16], xmm0
    movdqa  xmm0, [gt_ev + 32]
    movdqa  [rdx + 32], xmm0
    inc     eax
    mov     [ev_head], eax              ; x86: запись индекса не обгонит запись события
    mov     ecx, [gt_ev + EV_SND]       ; счёт по звукам — в alevels.bin
    lea     rdx, [st_ev]
    inc     dword [rdx + rcx*4]
.drop:
    ret

; ecx = звук, xmm0 = x, xmm1 = y (тайлы), xmm2 = громкость, xmm3 = высота.
; Громкость 1/(1 + 0.45·d'), где d' — расстояние, укороченное ветром от источника к
; слушателю (и удлинённое против); верха съедают воздух и снегопад; стены: из избы
; на улицу — глухо, за углом — приглушённо; панорама по экрану, дальнее ухо — глуше.
; gt_del — задержка (сбрасывается), gt_nocc = 1 — без проверки «за углом»
snd_emit:
    cmp     byte [aud_on], 0
    jne     .go
    ret
.go:
    push    rbx
    sub     rsp, 48
    lea     rbx, [gt_ev]
    mov     [rbx + EV_SND], ecx
    movss   [rbx + EV_VOL], xmm2
    movss   [rbx + EV_P], xmm3
    mov     eax, [gt_seed]
    mov     [rbx + EV_SEED], eax
    mov     eax, [gt_a]
    mov     [rbx + EV_A], eax
    movss   [gt_sx], xmm0
    movss   [gt_sy], xmm1
    subss   xmm0, [pl + E_X]
    subss   xmm1, [pl + E_Y]
    movss   [gt_dx], xmm0
    movss   [gt_dy], xmm1
    mulss   xmm0, xmm0
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    addss   xmm0, [ac_0_01]
    sqrtss  xmm0, xmm0
    movss   [gt_d], xmm0
    ; ветер: cos(источник от слушателя, ветер) < 0 — дует от источника к нам
    movss   xmm1, [gt_dx]
    mulss   xmm1, [wind_dx]
    movss   xmm2, [gt_dy]
    mulss   xmm2, [wind_dy]
    addss   xmm1, xmm2
    divss   xmm1, xmm0
    movss   xmm2, [wind_g]
    minss   xmm2, [ac_1_5]
    mulss   xmm1, xmm2
    mulss   xmm1, [ac_m0_35]
    addss   xmm1, [ac_1]
    maxss   xmm1, [ac_0_45]
    divss   xmm0, xmm1                  ; d'
    movaps  xmm1, xmm0
    mulss   xmm1, [ac_0_45]
    addss   xmm1, [ac_1]
    movss   xmm2, [ac_1]
    divss   xmm2, xmm1
    movss   [gt_g], xmm2
    movss   xmm1, [wnd_snow]
    mulss   xmm1, [ac_0_2]
    addss   xmm1, [ac_0_12]
    mulss   xmm1, xmm0
    addss   xmm1, [ac_1]
    movss   xmm2, [ac_17000]
    divss   xmm2, xmm1
    movss   [gt_fc], xmm2
    ; стены
    movss   xmm0, [gt_sx]
    movss   xmm1, [gt_sy]
    call    g_room
    mov     [gt_rs], eax
    movss   xmm0, [pl + E_X]
    movss   xmm1, [pl + E_Y]
    call    g_room
    mov     [gt_rp], eax
    cmp     eax, [gt_rs]
    je      .same
    movss   xmm0, [gt_g]
    mulss   xmm0, [ac_0_4]
    movss   [gt_g], xmm0
    movss   xmm0, [gt_fc]
    minss   xmm0, [ac_650]
    movss   [gt_fc], xmm0
    jmp     .walls
.same:
    test    eax, eax
    jnz     .walls
    cmp     byte [gt_nocc], 0
    jne     .walls
    movss   xmm0, [gt_d]
    comiss  xmm0, [ac_14]
    jae     .walls
    comiss  xmm0, [ac_1_5]
    jb      .walls
    ; за углом: по сетке прямой видимости прошлого кадра
    movss   xmm0, [gt_sx]
    mulss   xmm0, [f_4]
    cvttss2si eax, xmm0
    sub     eax, [gox]
    cmp     eax, GW
    jae     .walls
    movss   xmm0, [gt_sy]
    mulss   xmm0, [f_4]
    cvttss2si ecx, xmm0
    sub     ecx, [goy]
    cmp     ecx, GW
    jae     .walls
    imul    ecx, ecx, GW
    add     eax, ecx
    lea     rcx, [losg]
    cmp     byte [rcx + rax], 0
    jne     .walls
    movss   xmm0, [gt_g]
    mulss   xmm0, [ac_0_7]
    movss   [gt_g], xmm0
    movss   xmm0, [gt_fc]
    minss   xmm0, [ac_2400]
    movss   [gt_fc], xmm0
.walls:
    ; тихо — не шлём
    movss   xmm0, [gt_g]
    mulss   xmm0, [rbx + EV_VOL]
    comiss  xmm0, [ac_0_01]
    jb      .out
    ; реверб: ~√g (далёкое — больше отражений относительно прямого), в избе гуще
    movss   xmm0, [gt_g]
    sqrtss  xmm0, xmm0
    mulss   xmm0, [ac_0_22]
    cmp     dword [gt_rp], 0
    je      .rv
    mulss   xmm0, [ac_1_6]
.rv:
    movss   [rbx + EV_RV], xmm0
    ; панорама: сдвиг по экрану, близкое — к центру
    movss   xmm0, [gt_dx]
    subss   xmm0, [gt_dy]
    mulss   xmm0, [ac_0_7071]
    movss   xmm1, [gt_d]
    maxss   xmm1, [ac_2]
    divss   xmm0, xmm1
    maxss   xmm0, [ac_m1]
    minss   xmm0, [ac_1]
    movss   [gt_p], xmm0
    movss   xmm1, [gt_g]
    call    a_pan
    movss   [rbx + EV_GL], xmm0
    movss   [rbx + EV_GR], xmm1
    ; дальнее ухо глуше
    movss   xmm0, [gt_p]
    maxss   xmm0, [ac_0]
    mulss   xmm0, [ac_m0_5]
    addss   xmm0, [ac_1]
    mulss   xmm0, [gt_fc]
    call    g_lpc
    movss   [rbx + EV_CL], xmm0
    movss   xmm0, [gt_p]
    minss   xmm0, [ac_0]
    mulss   xmm0, [ac_0_5]
    addss   xmm0, [ac_1]
    mulss   xmm0, [gt_fc]
    call    g_lpc
    movss   [rbx + EV_CR], xmm0
    mov     eax, [gt_del]
    mov     [rbx + EV_DEL], eax
    call    ev_push
.out:
    mov     dword [gt_del], 0
    mov     dword [gt_seed], 0
    mov     dword [gt_a], 0
    mov     byte [gt_nocc], 0
    add     rsp, 48
    pop     rbx
    ret

; ecx = звук, xmm0 = панорама, xmm2 = громкость, xmm3 = высота: у самого игрока
snd_emit_c:
    cmp     byte [aud_on], 0
    jne     .go
    ret
.go:
    push    rbx
    sub     rsp, 32
    lea     rbx, [gt_ev]
    mov     [rbx + EV_SND], ecx
    movss   [rbx + EV_VOL], xmm2
    movss   [rbx + EV_P], xmm3
    mov     eax, [gt_seed]
    mov     [rbx + EV_SEED], eax
    mov     eax, [gt_a]
    mov     [rbx + EV_A], eax
    movss   xmm1, [ac_1]
    call    a_pan
    movss   [rbx + EV_GL], xmm0
    movss   [rbx + EV_GR], xmm1
    mov     dword [rbx + EV_CL], __float32__(1.0)
    mov     dword [rbx + EV_CR], __float32__(1.0)
    mov     dword [rbx + EV_RV], __float32__(0.12)
    movss   xmm0, [pl + E_X]
    movss   xmm1, [pl + E_Y]
    call    g_room
    test    eax, eax
    jz      .o
    mov     dword [rbx + EV_RV], __float32__(0.3)
.o:
    mov     eax, [gt_del]
    mov     [rbx + EV_DEL], eax
    call    ev_push
    mov     dword [gt_del], 0
    mov     dword [gt_seed], 0
    mov     dword [gt_a], 0
    add     rsp, 32
    pop     rbx
    ret

; rbx = зомби -> xmm3 = высота его голоса (у каждого своя, по номеру). Портит eax-edx, xmm4
zpitch:
    lea     rax, [ents]
    mov     rcx, rbx
    sub     rcx, rax
    mov     eax, ecx
    xor     edx, edx
    mov     ecx, ESZ
    div     ecx
    cvtsi2ss xmm3, eax
    mulss   xmm3, [ac_phi]
    cvttss2si eax, xmm3
    cvtsi2ss xmm4, eax
    subss   xmm3, xmm4
    mulss   xmm3, [ac_0_36]
    addss   xmm3, [ac_0_84]
    ret

; ---------------------------------------------------------------- крючки в игре
; xmm0 = x, xmm1 = y — куда ляжет след, пока он не оттиснут: есть ли тут уже следы
; (max в 3x3 ячейках) -> gt_ftr, высота снега (рельеф) -> gt_fdp. xmm0, xmm1 не трогает
snd_foot:
    movss   xmm2, xmm0
    mulss   xmm2, [ac_8]
    cvttss2si eax, xmm2
    movss   xmm2, xmm1
    mulss   xmm2, [ac_8]
    cvttss2si ecx, xmm2
    mov     dword [gt_ftr], 0
    mov     dword [gt_fdp], 0
    dec     eax
    dec     ecx
    cmp     eax, DECW - 3
    ja      .no
    cmp     ecx, DECW - 3
    ja      .no
    shl     ecx, 10
    add     ecx, eax                    ; левый верхний угол 3x3
    lea     rdx, [tracks]
    xor     eax, eax
    mov     r8d, 3
.r:
    movzx   r9d, byte [rdx + rcx]
    cmp     r9d, eax
    cmova   eax, r9d
    movzx   r9d, byte [rdx + rcx + 1]
    cmp     r9d, eax
    cmova   eax, r9d
    movzx   r9d, byte [rdx + rcx + 2]
    cmp     r9d, eax
    cmova   eax, r9d
    add     ecx, DECW
    dec     r8d
    jnz     .r
    mov     [gt_ftr], eax
    sub     ecx, DECW*2 - 1             ; середина
    lea     rdx, [drift]
    movzx   eax, byte [rdx + rcx*4]
    mov     [gt_fdp], eax
.no:
    ret

; шаг игрока: снег — хруст, дорога (укатано, мороз) — скрип, в избе — половицы. По
; нахоженному — глуше и тише, по сугробу — глубже, ниже и гуще (snd_foot)
snd_step_pl:
    cmp     byte [aud_on], 0
    je      .no
    sub     rsp, 56
    movss   xmm0, [pl + E_X]
    movss   xmm1, [pl + E_Y]
    call    g_room
    mov     ecx, SN_STEP_WOOD
    test    eax, eax
    jnz     .s
    cvttss2si eax, [pl + E_X]
    cvttss2si ecx, [pl + E_Y]
    and     eax, MAPW - 1
    and     ecx, MAPW - 1
    shl     ecx, 7
    add     eax, ecx
    lea     rdx, [tmap]
    movzx   edx, byte [rdx + rax]
    mov     ecx, SN_STEP_SNOW
    cmp     edx, T_ROAD
    jne     .s
    mov     ecx, SN_STEP_ROAD
.s:
    mov     [rsp + 32], ecx
    movss   xmm2, [ac_0_85]
    mov     dword [gt_a], __float32__(1.0)
    cmp     dword [pl_sprint], 0
    je      .w
    movss   xmm2, [ac_1_25]
    mov     dword [gt_a], __float32__(1.5)  ; бегом — чаще и крепче
.w:
    movss   xmm3, [ac_1]
    cmp     dword [rsp + 32], SN_STEP_WOOD
    je      .pan
    ; нахожено (tr = след / 215): тише и глуше, реже хрустит; сугроб (глубина
    ; dp = (высота − 2 px) / 10 px): громче, ниже, гуще
    cvtsi2ss xmm4, dword [gt_ftr]
    mulss   xmm4, [ac_i215]
    minss   xmm4, [ac_1]
    cvtsi2ss xmm5, dword [gt_fdp]
    mulss   xmm5, [ac_0_0125]
    subss   xmm5, [ac_0_2]
    maxss   xmm5, [ac_0]
    minss   xmm5, [ac_1]
    movss   xmm0, [ac_1]
    movaps  xmm1, xmm4
    mulss   xmm1, [ac_0_35]
    subss   xmm0, xmm1
    movaps  xmm1, xmm5
    mulss   xmm1, [ac_0_35]
    addss   xmm1, [ac_1]
    mulss   xmm0, xmm1
    mulss   xmm2, xmm0                  ; громкость
    movss   xmm0, [ac_1]
    movaps  xmm1, xmm4
    mulss   xmm1, [ac_0_3]
    subss   xmm0, xmm1
    movaps  xmm1, xmm5
    mulss   xmm1, [ac_0_4]
    addss   xmm1, [ac_1]
    mulss   xmm0, xmm1
    mulss   xmm0, [gt_a]
    movss   [gt_a], xmm0                ; сила: зёрен больше/меньше
    movaps  xmm3, xmm4
    mulss   xmm3, [ac_0_08]
    addss   xmm3, [ac_1]
    movaps  xmm1, xmm5
    mulss   xmm1, [ac_0_2]
    movss   xmm0, [ac_1]
    subss   xmm0, xmm1
    mulss   xmm3, xmm0                  ; высота резонансов
.pan:
    movss   xmm0, [ac_0_08]             ; левая/правая нога — чуть в стороны
    test    dword [pl + E_FOOT], 1
    jz      .p
    xorps   xmm0, [ac_sign]
.p:
    movss   [rsp + 36], xmm0
    call    snd_emit_c
    ; половица: у каждой доски свой нрав (хеш тайла) — каждая третья скрипит, когда на
    ; неё ступишь, и всегда своим голосом
    cmp     dword [rsp + 32], SN_STEP_WOOD
    jne     .out
    cvttss2si eax, [pl + E_X]
    cvttss2si ecx, [pl + E_Y]
    imul    eax, eax, 73856093
    imul    ecx, ecx, 19349663
    xor     eax, ecx
    mov     ecx, eax
    shr     ecx, 13
    xor     eax, ecx
    imul    eax, eax, 0x5BD1E995
    mov     ecx, eax
    shr     ecx, 15
    xor     eax, ecx
    mov     ecx, eax
    and     ecx, 7
    cmp     ecx, 3
    jae     .out
    shr     eax, 8
    movzx   eax, al
    cvtsi2ss xmm3, eax
    mulss   xmm3, [ac_i255]
    mulss   xmm3, [ac_0_45]
    addss   xmm3, [ac_0_8]
    movss   [rsp + 32], xmm3
    call    g_rndf
    comiss  xmm0, [ac_0_75]
    jae     .out
    mov     dword [gt_del], __float32__(0.03)
    mov     ecx, SN_FCREAK
    movss   xmm0, [rsp + 36]
    movss   xmm2, [ac_0_9]
    movss   xmm3, [rsp + 32]
    call    snd_emit_c
.out:
    add     rsp, 56
.no:
    ret

; rbx = зомби: шаг (волочит ноги) — раз на 0.4 тайла пути (следы-волоки ставятся чаще);
; не больше 6 за кадр, ближе 18 тайлов
snd_step_z:
    cmp     byte [aud_on], 0
    je      .no
    movss   xmm0, [rbx + E_AN]
    mulss   xmm0, [ac_2_5]
    cvttss2si eax, xmm0
    lea     rcx, [ents]
    mov     rdx, rbx
    sub     rdx, rcx
    shr     edx, 4                      ; ESZ = 80 = 16·5: номер·5
    imul    edx, edx, 0xCCCD
    shr     edx, 18                     ; /5 — номер зомби
    lea     rcx, [zsi]
    cmp     [rcx + rdx*4], eax
    je      .no
    mov     [rcx + rdx*4], eax
    cmp     dword [gt_zsteps], 6
    jae     .no
    movss   xmm0, [rbx + E_X]
    subss   xmm0, [pl + E_X]
    mulss   xmm0, xmm0
    movss   xmm1, [rbx + E_Y]
    subss   xmm1, [pl + E_Y]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    comiss  xmm0, [ac_324]
    jae     .no
    inc     dword [gt_zsteps]
    sub     rsp, 40
    movss   xmm2, [ac_0_9]
    mov     dword [gt_a], __float32__(1.0)
    cmp     dword [rbx + E_ST], ST_CHASE
    jne     .v
    movss   xmm2, [ac_1_2]
    mov     dword [gt_a], __float32__(1.35)
.v:
    mov     ecx, SN_STEP_Z
    movss   xmm0, [rbx + E_X]
    movss   xmm1, [rbx + E_Y]
    movss   xmm3, [ac_1]
    call    snd_emit
    add     rsp, 40
.no:
    ret

; взмах лопатой: свист в сторону взгляда
snd_swing:
    cmp     byte [aud_on], 0
    je      .no
    sub     rsp, 40
    movss   xmm0, [pl + E_FX]
    subss   xmm0, [pl + E_FY]
    mulss   xmm0, [ac_0_35]
    movss   xmm2, [ac_1]
    movss   xmm3, [ac_1]
    mov     ecx, SN_SWING
    call    snd_emit_c
    add     rsp, 40
.no:
    ret

; rbx = задетый зомби: удар (лопата звенит); добит — падает в снег со стоном
snd_hit:
    cmp     byte [aud_on], 0
    je      .no
    sub     rsp, 40
    call    g_rndf
    mulss   xmm0, [ac_0_12]
    addss   xmm0, [ac_0_94]
    movaps  xmm3, xmm0
    mov     dword [gt_del], __float32__(0.06)   ; лопата долетает
    mov     byte [gt_nocc], 1
    mov     ecx, SN_HIT
    movss   xmm0, [rbx + E_X]
    movss   xmm1, [rbx + E_Y]
    movss   xmm2, [ac_1]
    call    snd_emit
    cmp     dword [rbx + E_HP], 0
    jg      .out
    call    zpitch
    mov     dword [gt_del], __float32__(0.06)
    mov     byte [gt_nocc], 1
    mov     ecx, SN_KILL
    movss   xmm0, [rbx + E_X]
    movss   xmm1, [rbx + E_Y]
    movss   xmm2, [ac_1]
    call    snd_emit
.out:
    add     rsp, 40
.no:
    ret

; rbx = зомби дотянулся: взмах и удар по игроку, рык
snd_zatk:
    cmp     byte [aud_on], 0
    je      .no
    sub     rsp, 40
    mov     ecx, SN_ZATK
    xorps   xmm0, xmm0
    movss   xmm2, [ac_1]
    movss   xmm3, [ac_1]
    call    snd_emit_c
    call    zpitch
    mov     byte [gt_nocc], 1
    mov     ecx, SN_SNARL
    movss   xmm0, [rbx + E_X]
    movss   xmm1, [rbx + E_Y]
    movss   xmm2, [ac_1]
    call    snd_emit
    add     rsp, 40
.no:
    ret

; rbx = зомби учуял по ветру: принюхивается
snd_sniff:
    cmp     byte [aud_on], 0
    je      .no
    sub     rsp, 40
    call    zpitch
    mov     ecx, SN_SNIFF
    movss   xmm0, [rbx + E_X]
    movss   xmm1, [rbx + E_Y]
    movss   xmm2, [ac_1]
    call    snd_emit
    add     rsp, 40
.no:
    ret

; щелчок фонарика
snd_click:
    cmp     byte [aud_on], 0
    je      .no
    sub     rsp, 40
    mov     ecx, SN_CLICK
    movss   xmm0, [ac_0_2]
    movss   xmm2, [ac_1]
    movss   xmm3, [ac_1]
    call    snd_emit_c
    add     rsp, 40
.no:
    ret

; выдох вместе с паром изо рта (br_t — до следующего), вдох — посередине; слышно,
; когда устал, бежишь или продрог
snd_breath:
    cmp     byte [aud_on], 0
    je      .no
    sub     rsp, 40
    movss   xmm0, [ac_75]
    subss   xmm0, [pl_sta]
    mulss   xmm0, [ac_i60]
    CLAMP01 xmm0
    cmp     dword [pl_sprint], 0
    je      .ns
    maxss   xmm0, [ac_0_6]
.ns:
    movss   xmm1, [ac_25]
    subss   xmm1, [pl_warm]
    mulss   xmm1, [ac_0_012]
    CLAMP01 xmm1
    maxss   xmm0, xmm1                  ; продрог — дыхание рваное
    comiss  xmm0, [ac_0_08]
    jb      .out
    mulss   xmm0, [ac_0_65]
    addss   xmm0, [ac_0_35]
    movss   [gt_bv], xmm0
    mov     ecx, SN_BREATH
    xorps   xmm0, xmm0
    movss   xmm2, [gt_bv]
    movss   xmm3, [ac_1]
    call    snd_emit_c
    movss   xmm0, [br_t]
    mulss   xmm0, [ac_0_5]
    movss   [gt_del], xmm0
    mov     ecx, SN_INHALE
    xorps   xmm0, xmm0
    movss   xmm2, [gt_bv]
    mulss   xmm2, [ac_0_8]
    movss   xmm3, [ac_1]
    call    snd_emit_c
.out:
    add     rsp, 40
.no:
    ret

; ebx, esi = тайл дерева: ком снега с веток (слышно в 9 тайлах — дальше их слишком много)
snd_flump:
    cmp     byte [aud_on], 0
    je      .no
    sub     rsp, 40
    cvtsi2ss xmm0, ebx
    addss   xmm0, [f_0_5]
    cvtsi2ss xmm1, esi
    addss   xmm1, [f_0_5]
    movss   xmm2, xmm0
    subss   xmm2, [pl + E_X]
    mulss   xmm2, xmm2
    movss   xmm3, xmm1
    subss   xmm3, [pl + E_Y]
    mulss   xmm3, xmm3
    addss   xmm2, xmm3
    comiss  xmm2, [ac_81]
    jae     .out
    movss   xmm2, [ac_0_7]
    movss   xmm3, [ac_1]
    mov     ecx, SN_FLUMP
    call    snd_emit
.out:
    add     rsp, 40
.no:
    ret

; ---------------------------------------------------------------- кадр игры -> звук
; параметры фона и события по таймерам (голоса зомби, собаки, ставни, петухи)
audio_frame:
    cmp     byte [aud_on], 0
    jne     .go
    ret
.go:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    sub     rsp, 48
    lea     rbx, [ap]
    ; M — звук вкл/выкл
    cmp     byte [k_m_edge], 0
    je      .nm
    xor     byte [aud_mute], 1
.nm:
    mov     dword [rbx + AP_VOL], __float32__(1.0)
    cmp     byte [aud_mute], 0
    je      .vol
    mov     dword [rbx + AP_VOL], 0
.vol:
    ; --- ветер: у игрока, в 2.2 тайла левее и правее по экрану, над крышами
    mov     eax, [pl_ws]
    mov     [rbx + AP_WN], eax
    movss   xmm0, [pl + E_X]
    subss   xmm0, [ac_side]
    movss   xmm1, [pl + E_Y]
    addss   xmm1, [ac_side]
    movss   xmm2, [f_10]
    call    wind_vec
    movss   [rbx + AP_WL], xmm2
    movss   xmm0, [pl + E_X]
    addss   xmm0, [ac_side]
    movss   xmm1, [pl + E_Y]
    subss   xmm1, [ac_side]
    movss   xmm2, [f_10]
    call    wind_vec
    movss   [rbx + AP_WR], xmm2
    movss   xmm0, [pl + E_X]
    movss   xmm1, [pl + E_Y]
    call    wind_top
    movss   [rbx + AP_WF], xmm2
    movss   [gt_wf], xmm2
    ; порыв сверх среднего (0.5 — средний)
    movss   xmm0, [wv_gf]
    subss   xmm0, [ac_1]
    movss   xmm1, [wnd_gust]
    maxss   xmm1, [ac_0_05]
    divss   xmm0, xmm1
    mulss   xmm0, [ac_0_5]
    addss   xmm0, [ac_0_5]
    CLAMP01 xmm0
    movss   [rbx + AP_GX], xmm0
    ; откуда дует — по экрану (+ справа)
    movss   xmm0, [wind_dy]
    subss   xmm0, [wind_dx]
    mulss   xmm0, [ac_0_7071]
    movss   [rbx + AP_WPAN], xmm0
    ; в избе
    movss   xmm0, [pl + E_X]
    movss   xmm1, [pl + E_Y]
    call    g_room
    mov     [gt_rp], eax
    xorps   xmm0, xmm0
    test    eax, eax
    jz      .out1
    movss   xmm0, [ac_1]
.out1:
    movss   [rbx + AP_IN], xmm0
    ; позёмка у ног (та же маска, что рисует полосы) и метель
    movss   xmm0, [pl_ws]
    subss   xmm0, [ac_0_45]
    mulss   xmm0, [ac_1_6]
    CLAMP01 xmm0
    mulss   xmm0, xmm0
    mulss   xmm0, [ac_0_8]
    movss   xmm1, [gt_wf]
    minss   xmm1, [ac_1_2]
    mulss   xmm1, [wnd_snow]
    mulss   xmm1, [ac_0_6]
    addss   xmm0, xmm1
    CLAMP01 xmm0
    movss   [rbx + AP_HISS], xmm0
    ; заслоны рядом: ячейки выше 14 px в квадрате 13x13 (2 на тайл)
    movss   xmm0, [pl + E_X]
    addss   xmm0, xmm0
    cvttss2si r12d, xmm0
    movss   xmm0, [pl + E_Y]
    addss   xmm0, xmm0
    cvttss2si r13d, xmm0
    xor     r14d, r14d
    lea     r8, [wob]
    mov     esi, -6
.ey:
    mov     edi, -6
.ex:
    lea     eax, [r12d + edi]
    lea     ecx, [r13d + esi]
    cmp     eax, WSW
    jae     .en
    cmp     ecx, WSW
    jae     .en
    shl     ecx, 8
    add     eax, ecx
    cmp     byte [r8 + rax], 14
    jb      .en
    inc     r14d
.en:
    inc     edi
    cmp     edi, 6
    jle     .ex
    inc     esi
    cmp     esi, 6
    jle     .ey
    cvtsi2ss xmm0, r14d
    mulss   xmm0, [ac_i45]
    minss   xmm0, [ac_1]
    movss   [rbx + AP_EDGE], xmm0
    ; деревья в 6 тайлах: ели и берёзы, вес 1/(1 + 0.18 d²), панорама по весу
    xorps   xmm0, xmm0
    movss   [gt_spw], xmm0
    movss   [gt_spp], xmm0
    movss   [gt_biw], xmm0
    movss   [gt_bip], xmm0
    cvttss2si r12d, [pl + E_X]
    cvttss2si r13d, [pl + E_Y]
    mov     esi, -6
.ty:
    mov     edi, -6
.tx:
    lea     eax, [r12d + edi]
    lea     ecx, [r13d + esi]
    cmp     eax, MAPW
    jae     .tn
    cmp     ecx, MAPW
    jae     .tn
    mov     edx, ecx
    shl     edx, 7
    add     edx, eax
    lea     r8, [tmap]
    movzx   edx, byte [r8 + rdx]
    cmp     edx, T_SPRUCE
    je      .tr
    cmp     edx, T_BIRCH
    jne     .tn
.tr:
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    subss   xmm0, [pl + E_X]
    cvtsi2ss xmm1, ecx
    addss   xmm1, [f_0_5]
    subss   xmm1, [pl + E_Y]
    call    g_wpan                      ; xmm0 = вес, xmm1 = вес·панорама
    cmp     edx, T_SPRUCE
    jne     .bi
    addss   xmm0, [gt_spw]
    movss   [gt_spw], xmm0
    addss   xmm1, [gt_spp]
    movss   [gt_spp], xmm1
    jmp     .tn
.bi:
    addss   xmm0, [gt_biw]
    movss   [gt_biw], xmm0
    addss   xmm1, [gt_bip]
    movss   [gt_bip], xmm1
.tn:
    inc     edi
    cmp     edi, 6
    jle     .tx
    inc     esi
    cmp     esi, 6
    jle     .ty
    movss   xmm0, [gt_spw]
    mulss   xmm0, [ac_0_4]
    minss   xmm0, [ac_1]
    movss   [rbx + AP_SPR], xmm0
    movss   xmm0, [gt_spp]
    movss   xmm1, [gt_spw]
    maxss   xmm1, [ac_0_01]
    divss   xmm0, xmm1
    movss   [rbx + AP_SPRP], xmm0
    movss   xmm0, [gt_biw]
    mulss   xmm0, [ac_0_5]
    minss   xmm0, [ac_1]
    movss   [rbx + AP_BIR], xmm0
    movss   xmm0, [gt_bip]
    movss   xmm1, [gt_biw]
    maxss   xmm1, [ac_0_01]
    divss   xmm0, xmm1
    movss   [rbx + AP_BIRP], xmm0
    ; провода: ближайший пролёт; поют тоном ~ скорости ветра над крышами (Эол)
    mov     dword [gt_ww], 0
    mov     dword [gt_wp], 0
    lea     rsi, [wires]
    mov     r12d, [nwires]
.wi:
    test    r12d, r12d
    jz      .wd
    movzx   eax, byte [rsi]
    cvtsi2ss xmm2, eax
    addss   xmm2, [f_0_5]               ; a
    movzx   eax, byte [rsi + 1]
    cvtsi2ss xmm3, eax
    addss   xmm3, [f_0_5]
    movzx   eax, byte [rsi + 2]
    cvtsi2ss xmm4, eax
    addss   xmm4, [f_0_5]
    subss   xmm4, xmm2                  ; b − a
    movzx   eax, byte [rsi + 3]
    cvtsi2ss xmm5, eax
    addss   xmm5, [f_0_5]
    subss   xmm5, xmm3
    movss   xmm0, [pl + E_X]
    subss   xmm0, xmm2                  ; p − a
    movss   xmm1, [pl + E_Y]
    subss   xmm1, xmm3
    movaps  xmm2, xmm0
    mulss   xmm2, xmm4
    movaps  xmm3, xmm1
    mulss   xmm3, xmm5
    addss   xmm2, xmm3                  ; (p−a)·(b−a)
    movaps  xmm3, xmm4
    mulss   xmm3, xmm4
    movss   [gt_t1], xmm5
    mulss   xmm5, xmm5
    addss   xmm3, xmm5
    maxss   xmm3, [ac_0_01]
    divss   xmm2, xmm3
    CLAMP01 xmm2                        ; t
    movss   xmm5, [gt_t1]
    mulss   xmm4, xmm2
    mulss   xmm5, xmm2
    subss   xmm0, xmm4                  ; p − ближняя точка
    subss   xmm1, xmm5
    xorps   xmm0, [ac_sign]
    xorps   xmm1, [ac_sign]             ; ближняя точка − p
    call    g_wpan
    comiss  xmm0, [gt_ww]
    jbe     .wn
    movss   [gt_ww], xmm0
    divss   xmm1, xmm0
    movss   [gt_wp], xmm1
.wn:
    add     rsi, 8
    dec     r12d
    jmp     .wi
.wd:
    movss   xmm0, [gt_ww]
    mulss   xmm0, [ac_1_5]
    minss   xmm0, [ac_1]
    movss   [rbx + AP_WIRE], xmm0
    mov     eax, [gt_wp]
    mov     [rbx + AP_WIRP], eax
    movss   xmm0, [gt_wf]
    mulss   xmm0, [ac_216]              ; 18 Гц на м/с, 12 м/с на единицу
    addss   xmm0, [ac_40]
    maxss   xmm0, [ac_60]
    minss   xmm0, [ac_520]
    movss   [rbx + AP_WIRF], xmm0
    ; фонари гудят (натрий, дроссель на 50 Гц — гул 100); сломанный — трещит в такт миганию
    xorps   xmm0, xmm0
    movss   [gt_spw], xmm0
    movss   [gt_spp], xmm0
    lea     rsi, [lamps]
    mov     r12d, [nlamps]
.la:
    test    r12d, r12d
    jz      .ld
    movzx   eax, byte [rsi]
    movzx   ecx, byte [rsi + 1]
    cmp     eax, [brk_x]
    jne     .lok
    cmp     ecx, [brk_y]
    je      .ln
.lok:
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    subss   xmm0, [pl + E_X]
    cvtsi2ss xmm1, ecx
    addss   xmm1, [f_0_5]
    subss   xmm1, [pl + E_Y]
    movss   [gt_t1], xmm0
    movss   [gt_t2], xmm1
    mulss   xmm0, xmm0
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    comiss  xmm0, [ac_100]
    jae     .ln
    movss   xmm0, [gt_t1]
    movss   xmm1, [gt_t2]
    call    g_wpan4
    addss   xmm0, [gt_spw]
    movss   [gt_spw], xmm0
    addss   xmm1, [gt_spp]
    movss   [gt_spp], xmm1
.ln:
    add     rsi, 4
    dec     r12d
    jmp     .la
.ld:
    movss   xmm0, [gt_spw]
    mulss   xmm0, [lamp_t]
    minss   xmm0, [ac_1]
    movss   [rbx + AP_HUM], xmm0
    movss   xmm0, [gt_spp]
    movss   xmm1, [gt_spw]
    maxss   xmm1, [ac_0_01]
    divss   xmm0, xmm1
    movss   [rbx + AP_HUMP], xmm0
    mov     dword [rbx + AP_BRK], 0
    mov     eax, [brk_x]
    test    eax, eax
    js      .nbrk
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    movss   [gt_sx], xmm0
    subss   xmm0, [pl + E_X]
    cvtsi2ss xmm1, dword [brk_y]
    addss   xmm1, [f_0_5]
    movss   [gt_sy], xmm1
    subss   xmm1, [pl + E_Y]
    call    g_wpan4
    divss   xmm1, xmm0                  ; панорама (вес > 0 всегда)
    movss   [rbx + AP_BRKP], xmm1
    mulss   xmm0, [lamp_t]
    movss   [gt_t1], xmm0
    xorps   xmm1, xmm1
    mov     eax, [brk_st]
    cmp     eax, 2
    je      .bl
    movss   xmm1, [ac_0_3]              ; горит — ровно зудит
    test    eax, eax
    jz      .bl
    movss   xmm1, [brk_lvl]             ; дребезжит — вспышками
.bl:
    mulss   xmm1, [gt_t1]
    movss   [rbx + AP_BRK], xmm1
    ; щелчки на переключениях мигания
    xor     ecx, ecx
    movss   xmm0, [brk_lvl]
    comiss  xmm0, [ac_0]
    jbe     .bc
    inc     ecx
.bc:
    cmp     dword [brk_st], 1
    jne     .bset
    movss   xmm0, [gt_t1]
    comiss  xmm0, [ac_0_02]
    jb      .bset
    cmp     ecx, [gt_bprev]
    je      .bset
    mov     [gt_bprev], ecx
    call    g_rndf
    comiss  xmm0, [ac_0_35]
    jae     .nbrk
    call    g_rndf
    mulss   xmm0, [ac_0_5]
    addss   xmm0, [ac_0_5]
    movaps  xmm2, xmm0
    mov     byte [gt_nocc], 1
    mov     ecx, SN_BRK
    movss   xmm0, [gt_sx]
    movss   xmm1, [gt_sy]
    movss   xmm3, [ac_1]
    call    snd_emit
    jmp     .nbrk
.bset:
    mov     [gt_bprev], ecx
.nbrk:
    ; печь топится (ближайшая) и телевизор (ближайший) — через стену глуше
    mov     dword [gt_fw], 0
    mov     dword [gt_fp], 0
    mov     dword [gt_tw], 0
    mov     dword [gt_tp], 0
    mov     dword [gt_tin], 0
    lea     rsi, [houses]
    xor     r12d, r12d
.hs:
    cmp     r12d, [nhouses]
    jae     .hd
    ; печь
    mov     eax, [rsi + HS_SX]
    mov     ecx, [rsi + HS_SY]
    cmp     eax, MAPW
    jae     .tv
    cmp     ecx, MAPW
    jae     .tv
    mov     edx, ecx
    shl     edx, 7
    add     edx, eax
    lea     r8, [tmap]
    cmp     byte [r8 + rdx], T_STOVEL
    jne     .tv
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    subss   xmm0, [pl + E_X]
    cvtsi2ss xmm1, ecx
    addss   xmm1, [f_0_5]
    subss   xmm1, [pl + E_Y]
    call    g_wpan3
    lea     eax, [r12d + 1]
    cmp     eax, [gt_rp]
    je      .fin
    mulss   xmm0, [ac_0_1]
    mulss   xmm1, [ac_0_1]
.fin:
    comiss  xmm0, [gt_fw]
    jbe     .tv
    movss   [gt_fw], xmm0
    divss   xmm1, xmm0
    movss   [gt_fp], xmm1
.tv:
    test    dword [rsi + HS_FL], HF_TV
    jz      .hn
    test    dword [rsi + HS_FL], HF_LIT
    jz      .hn
    cvtsi2ss xmm0, dword [rsi + HS_W]
    mulss   xmm0, [f_0_5]
    cvtsi2ss xmm1, dword [rsi + HS_X0]
    addss   xmm0, xmm1
    subss   xmm0, [pl + E_X]
    cvtsi2ss xmm1, dword [rsi + HS_H]
    mulss   xmm1, [f_0_5]
    cvtsi2ss xmm2, dword [rsi + HS_Y0]
    addss   xmm1, xmm2
    subss   xmm1, [pl + E_Y]
    call    g_wpan2
    mov     ecx, __float32__(1.0)       ; в той же избе — ясно
    lea     eax, [r12d + 1]
    cmp     eax, [gt_rp]
    je      .tin
    xor     ecx, ecx                    ; через стену — глухо и тише
    mulss   xmm0, [ac_0_35]
    mulss   xmm1, [ac_0_35]
.tin:
    comiss  xmm0, [gt_tw]
    jbe     .hn
    movss   [gt_tw], xmm0
    divss   xmm1, xmm0
    movss   [gt_tp], xmm1
    mov     [gt_tin], ecx
.hn:
    add     rsi, HS_SZ
    inc     r12d
    jmp     .hs
.hd:
    mov     eax, [gt_fw]
    mov     [rbx + AP_FIRE], eax
    mov     eax, [gt_fp]
    mov     [rbx + AP_FIRP], eax
    movss   xmm0, [gt_tw]
    mulss   xmm0, [lamp_t]
    movss   [rbx + AP_TV], xmm0
    mov     eax, [gt_tp]
    mov     [rbx + AP_TVP], eax
    mov     eax, [tv_lvl]
    mov     [rbx + AP_TVL], eax
    mov     eax, [gt_tin]
    mov     [rbx + AP_TVIN], eax
    ; сердце: ранен, замерзаешь, погоня рядом
    movss   xmm0, [ac_45]
    subss   xmm0, [pl_hp]
    mulss   xmm0, [ac_i35]
    CLAMP01 xmm0
    movss   xmm1, [ac_18]
    subss   xmm1, [pl_warm]
    mulss   xmm1, [ac_0_039]
    CLAMP01 xmm1
    maxss   xmm0, xmm1
    cmp     byte [gt_chnear], 0
    je      .hb1
    maxss   xmm0, [ac_0_35]
.hb1:
    cmp     dword [gstate], 1
    jne     .hb2
    xorps   xmm0, xmm0
.hb2:
    movss   [rbx + AP_HB], xmm0
    mulss   xmm0, [ac_0_9]
    addss   xmm0, [ac_1_05]
    movss   [rbx + AP_HBR], xmm0
    xorps   xmm0, xmm0
    cmp     dword [gstate], 1
    jne     .dd
    movss   xmm0, [ac_1]
.dd:
    movss   [rbx + AP_DEAD], xmm0
    ; --- события
    call    az_voices
    call    az_dogs
    call    az_kennels
    call    az_knocks
    call    az_misc
    mov     dword [gt_zsteps], 0
    add     rsp, 48
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; xmm0 = dx, xmm1 = dy (от игрока) -> xmm0 = вес 1/(1 + k·d²), xmm1 = вес·панорама.
; g_wpan: k = 0.18 (деревья), g_wpan2: 0.06 (телевизор), g_wpan3: 0.4 (печь),
; g_wpan4: 0.8 (фонари); проводам — 0.35. Портит xmm2, xmm3
g_wpan:
    movss   xmm3, [ac_0_18]
    jmp     g_wpk
g_wpan2:
    movss   xmm3, [ac_0_06]
    jmp     g_wpk
g_wpan3:
    movss   xmm3, [ac_0_4]
    jmp     g_wpk
g_wpan4:
    movss   xmm3, [ac_0_8]
g_wpk:
    movaps  xmm2, xmm0
    subss   xmm2, xmm1
    mulss   xmm2, [ac_0_7071]           ; сдвиг по экрану
    mulss   xmm0, xmm0
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    movaps  xmm1, xmm0
    addss   xmm1, [ac_1]
    sqrtss  xmm1, xmm1
    divss   xmm2, xmm1                  ; панорама ≈ сдвиг / √(d² + 1)
    mulss   xmm0, xmm3
    addss   xmm0, [ac_1]
    movss   xmm1, [ac_1]
    divss   xmm1, xmm0
    movaps  xmm0, xmm1
    mulss   xmm1, xmm2
    ret

; --- зомби: стон бродячего, «хм?» проверяющего, рык в погоне; заметил — вскрик.
; У каждого свой таймер и свой голос (высота — по номеру)
az_voices:
    push    rbx
    push    r12
    sub     rsp, 40
    mov     byte [gt_chnear], 0
    lea     rbx, [ents]
    xor     r12d, r12d
.l:
    mov     eax, [rbx + E_ST]
    lea     rdx, [zst]
    movzx   ecx, byte [rdx + r12]
    mov     [rdx + r12], al
    cmp     eax, ST_WANDER
    jb      .n
    cmp     eax, ST_CORPSE
    jae     .n
    movss   xmm0, [rbx + E_X]
    subss   xmm0, [pl + E_X]
    mulss   xmm0, xmm0
    movss   xmm1, [rbx + E_Y]
    subss   xmm1, [pl + E_Y]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    comiss  xmm0, [ac_576]
    jae     .n
    cmp     eax, ST_CHASE
    jne     .nc
    comiss  xmm0, [ac_25]
    jae     .nc
    mov     byte [gt_chnear], 1
.nc:
    cmp     eax, ecx
    je      .timer
    cmp     ecx, ST_WANDER
    jb      .fresh
    cmp     ecx, ST_CORPSE
    jae     .fresh
    cmp     eax, ST_CHASE
    jne     .ninv
    call    zpitch
    mov     ecx, SN_AGGRO
    movss   xmm2, [ac_1]
    call    .say
    call    g_rndf
    mulss   xmm0, [ac_2]
    addss   xmm0, [ac_1_5]
    jmp     .settm
.ninv:
    cmp     eax, ST_INVEST
    jne     .timer
    cmp     ecx, ST_WANDER
    jne     .timer
    movss   xmm0, [rbx + E_STUN]        ; принюхивается — звук уже был
    comiss  xmm0, [ac_0]
    ja      .inv2
    call    zpitch
    mov     ecx, SN_HMM
    movss   xmm2, [ac_0_9]
    call    .say
.inv2:
    call    g_rndf
    mulss   xmm0, [ac_2]
    addss   xmm0, [ac_2_5]
    jmp     .settm
.fresh:
    call    g_rndf
    mulss   xmm0, [ac_9]
    addss   xmm0, [ac_1]
    jmp     .settm
.timer:
    lea     rdx, [zvt]
    movss   xmm0, [rdx + r12*4]
    subss   xmm0, [dt]
    movss   [rdx + r12*4], xmm0
    comiss  xmm0, [ac_0]
    ja      .n
    movss   xmm0, [rbx + E_STUN]
    comiss  xmm0, [ac_0]
    jbe     .talk
    movss   xmm0, [ac_0_4]
    jmp     .settm
.talk:
    call    zpitch
    mov     eax, [rbx + E_ST]
    cmp     eax, ST_CHASE
    je      .tch
    cmp     eax, ST_INVEST
    je      .tin
    call    g_rndf
    mulss   xmm0, [ac_0_4]
    addss   xmm0, [ac_0_8]
    movaps  xmm2, xmm0
    mov     ecx, SN_GROAN
    call    .say
    call    g_rndf
    mulss   xmm0, [ac_11]
    addss   xmm0, [ac_7]
    jmp     .settm
.tin:
    mov     ecx, SN_HMM
    movss   xmm2, [ac_0_8]
    call    .say
    call    g_rndf
    mulss   xmm0, [ac_3]
    addss   xmm0, [ac_3]
    jmp     .settm
.tch:
    mov     ecx, SN_GROWL
    movss   xmm2, [ac_1]
    call    .say
    call    g_rndf
    mulss   xmm0, [ac_2]
    addss   xmm0, [ac_1_7]
.settm:
    lea     rdx, [zvt]
    movss   [rdx + r12*4], xmm0
.n:
    add     rbx, ESZ
    inc     r12d
    cmp     r12d, MAXE
    jb      .l
    add     rsp, 40
    pop     r12
    pop     rbx
    ret
; ecx = звук, xmm2 = громкость, xmm3 = высота: из места зомби rbx; хрипота — у каждого
; своя (по номеру)
.say:
    sub     rsp, 8
    lea     rax, [ents]
    mov     rdx, rbx
    sub     rdx, rax
    shr     edx, 4                      ; ESZ = 80 = 16·5
    imul    edx, edx, 0xCCCD
    shr     edx, 18
    cvtsi2ss xmm0, edx
    mulss   xmm0, [ac_sqrt5]
    cvttss2si eax, xmm0
    cvtsi2ss xmm1, eax
    subss   xmm0, xmm1
    mulss   xmm0, [ac_0_8]
    addss   xmm0, [ac_0_7]
    movss   [gt_a], xmm0
    movss   xmm0, [rbx + E_X]
    movss   xmm1, [rbx + E_Y]
    call    snd_emit
    add     rsp, 8
    ret

; --- собаки по селу: у каждой свой двор за околицей и свой голос (audio_newgame); лают
; сериями — неровно, иногда «гав-гав» подряд, последний тише; иногда воют (чаще глубокой
; ночью), иногда отвечает соседка, а там и третья; лай отдаётся эхом от леса. В бурю
; прячутся. По ветру слышно дальше — это уже в snd_emit
az_dogs:
    push    rbx
    push    r12
    push    r13
    push    r14
    sub     rsp, 40
    movss   xmm0, [dog_t]
    subss   xmm0, [dt]
    movss   [dog_t], xmm0
    comiss  xmm0, [ac_0]
    ja      .out
    call    g_rndf
    mulss   xmm0, [ac_36]
    addss   xmm0, [ac_14]
    movss   [dog_t], xmm0
    movss   xmm0, [wind_g]
    comiss  xmm0, [ac_1_25]
    ja      .out
    mov     dword [gt_dd], 0            ; задержка от «сейчас», копится по сериям
    call    g_rndf
    mulss   xmm0, [ac_nfd]
    cvttss2si r12d, xmm0                ; кто начнёт
    mov     r14d, 1
    call    g_rndf
    comiss  xmm0, [ac_0_35]
    jae     .dog
    inc     r14d                        ; ответит соседка
    call    g_rndf
    comiss  xmm0, [ac_0_3]
    jae     .dog
    inc     r14d                        ; ...и ещё одна
.dog:
    lea     rdx, [fd_x]
    mov     eax, [rdx + r12*4]
    mov     [gt_sx], eax
    lea     rdx, [fd_y]
    mov     eax, [rdx + r12*4]
    mov     [gt_sy], eax
    lea     rdx, [fd_p]
    mov     eax, [rdx + r12*4]
    mov     [gt_dp], eax
    ; воет — реже, чаще глубокой ночью (с 23 до 4)
    movss   xmm1, [ac_0_08]
    movss   xmm0, [gtime]
    comiss  xmm0, [ac_1380]
    jb      .hw
    comiss  xmm0, [ac_1680]
    ja      .hw
    movss   xmm1, [ac_0_25]
.hw:
    movss   [gt_t1], xmm1
    call    g_rndf
    comiss  xmm0, [gt_t1]
    jae     .bark
    mov     eax, [gt_dd]
    mov     [gt_del], eax
    mov     byte [gt_nocc], 1
    mov     ecx, SN_HOWL
    movss   xmm0, [gt_sx]
    movss   xmm1, [gt_sy]
    movss   xmm2, [ac_1]
    movss   xmm3, [gt_dp]
    mulss   xmm3, [ac_0_85]
    call    snd_emit_e
    movss   xmm0, [gt_dd]
    addss   xmm0, [ac_1_5]
    movss   [gt_dd], xmm0
    jmp     .next
.bark:
    call    g_rndf
    mulss   xmm0, [ac_6]
    cvttss2si ebx, xmm0
    add     ebx, 2                      ; 2..7 раз
    call    g_rndf
    mulss   xmm0, [ac_0_3]
    addss   xmm0, [ac_0_85]
    lea     rdx, [fd_t]
    mulss   xmm0, [rdx + r12*4]
    movss   [gt_t2], xmm0               ; шаг лая этой серии — от нрава собаки
.b:
    call    g_rndf
    comiss  xmm0, [ac_0_18]
    jb      .dbl
    call    g_rndf
    mulss   xmm0, [ac_0_5]
    addss   xmm0, [ac_0_75]
    mulss   xmm0, [gt_t2]
    jmp     .bd
.dbl:
    movss   xmm0, [ac_0_18]             ; «гав-гав»
.bd:
    addss   xmm0, [gt_dd]
    movss   [gt_dd], xmm0
    movss   [gt_del], xmm0
    lea     rdx, [fd_r]
    mov     eax, [rdx + r12*4]
    mov     [gt_a], eax
    mov     byte [gt_nocc], 1
    call    g_rndf
    mulss   xmm0, [ac_0_3]
    addss   xmm0, [ac_0_85]
    cmp     ebx, 1
    jne     .v
    mulss   xmm0, [ac_0_75]             ; последний — тише
.v:
    movss   [gt_t3], xmm0
    call    g_rndf
    subss   xmm0, [ac_0_5]
    mulss   xmm0, [ac_0_1]
    addss   xmm0, [ac_1]
    mulss   xmm0, [gt_dp]
    movaps  xmm3, xmm0
    movss   xmm2, [gt_t3]
    mov     ecx, SN_BARK
    movss   xmm0, [gt_sx]
    movss   xmm1, [gt_sy]
    call    snd_emit_e
    dec     ebx
    jnz     .b
.next:
    dec     r14d
    jz      .out
    ; отвечает другая, через 1.2–2.7 с после
    call    g_rndf
    mulss   xmm0, [ac_nfd1]
    cvttss2si eax, xmm0
    lea     eax, [r12 + rax + 1]
    xor     edx, edx
    mov     ecx, NFD
    div     ecx
    mov     r12d, edx
    call    g_rndf
    mulss   xmm0, [ac_1_5]
    addss   xmm0, [ac_1_2]
    addss   xmm0, [gt_dd]
    movss   [gt_dd], xmm0
    jmp     .dog
.out:
    add     rsp, 40
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret

; как snd_emit (ecx = звук, xmm0 = x, xmm1 = y, xmm2 = громкость, xmm3 = высота; gt_del,
; gt_a, gt_nocc), но голос со своим зерном и с шансом 0.7 — эхо от леса: та же копия
; звука на 0.2–0.55 с позже, тише и глуше, с другой стороны (источник повёрнут вокруг
; слушателя на 60–140° и отодвинут)
snd_emit_e:
    push    rbx
    sub     rsp, 80
    mov     [rsp + 32], ecx
    movss   [rsp + 36], xmm0
    movss   [rsp + 40], xmm1
    movss   [rsp + 44], xmm2
    movss   [rsp + 48], xmm3
    call    g_rnd
    or      eax, 1
    mov     [rsp + 52], eax
    mov     [gt_seed], eax
    mov     eax, [gt_del]
    mov     [rsp + 56], eax
    mov     eax, [gt_a]
    mov     [rsp + 60], eax
    mov     ecx, [rsp + 32]
    movss   xmm0, [rsp + 36]
    movss   xmm1, [rsp + 40]
    movss   xmm2, [rsp + 44]
    movss   xmm3, [rsp + 48]
    call    snd_emit
    call    g_rndf
    comiss  xmm0, [ac_0_7]
    jae     .out
    call    g_rndf
    mulss   xmm0, [ac_1_4]
    addss   xmm0, [ac_1_05]             ; 60°..140°
    movss   [rsp + 64], xmm0
    call    g_rnd
    test    eax, 0x100
    jz      .l
    movss   xmm0, [rsp + 64]
    xorps   xmm0, [ac_sign]
    movss   [rsp + 64], xmm0
.l:
    movss   xmm0, [rsp + 64]
    call    fsin
    movss   [rsp + 68], xmm0            ; sin
    movss   xmm0, [rsp + 64]
    addss   xmm0, [f_pi_2]
    call    fsin                        ; cos
    movss   xmm2, [rsp + 36]
    subss   xmm2, [pl + E_X]            ; dx
    movss   xmm3, [rsp + 40]
    subss   xmm3, [pl + E_Y]            ; dy
    movaps  xmm4, xmm2
    mulss   xmm4, xmm0                  ; dx·c
    movaps  xmm5, xmm3
    mulss   xmm5, [rsp + 68]            ; dy·s
    subss   xmm4, xmm5
    mulss   xmm2, [rsp + 68]            ; dx·s
    mulss   xmm3, xmm0                  ; dy·c
    addss   xmm2, xmm3
    mulss   xmm4, [ac_1_3]
    mulss   xmm2, [ac_1_3]
    addss   xmm4, [pl + E_X]
    addss   xmm2, [pl + E_Y]
    movss   [rsp + 36], xmm4
    movss   [rsp + 40], xmm2
    call    g_rndf
    mulss   xmm0, [ac_0_35]
    addss   xmm0, [ac_0_2]
    addss   xmm0, [rsp + 56]
    movss   [gt_del], xmm0
    mov     eax, [rsp + 52]
    mov     [gt_seed], eax
    mov     eax, [rsp + 60]
    mov     [gt_a], eax
    mov     byte [gt_nocc], 1
    mov     ecx, [rsp + 32]
    movss   xmm0, [rsp + 36]
    movss   xmm1, [rsp + 40]
    movss   xmm2, [rsp + 44]
    mulss   xmm2, [ac_0_4]
    movss   xmm3, [rsp + 48]
    call    snd_emit
.out:
    add     rsp, 80
    pop     rbx
    ret

; --- собака в будке: лает, когда рядом зомби (5.5 тайла) или игрок (3) — сторож; кидается
; на цепи (цепь звенит); игрока в 3–5 тайлах сперва облаивает рыком. Голос и хрипота — у
; каждой будки свои
az_kennels:
    push    rbx
    push    r12
    push    r13
    sub     rsp, 48
    xor     r12d, r12d
.k:
    cmp     r12d, [kn_n]
    jae     .out
    lea     rdx, [kn_t]
    movss   xmm0, [rdx + r12*4]
    subss   xmm0, [dt]
    movss   [rdx + r12*4], xmm0
    comiss  xmm0, [ac_0]
    ja      .kn
    mov     dword [rdx + r12*4], __float32__(0.5)
    lea     rdx, [kn_xy]
    movzx   eax, byte [rdx + r12*2]
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    movss   [gt_sx], xmm0
    movzx   eax, byte [rdx + r12*2 + 1]
    cvtsi2ss xmm1, eax
    addss   xmm1, [f_0_5]
    movss   [gt_sy], xmm1
    ; голос и хрипота этой будки
    cvtsi2ss xmm2, r12d
    mulss   xmm2, [ac_phi]
    cvttss2si eax, xmm2
    cvtsi2ss xmm3, eax
    subss   xmm2, xmm3
    movaps  xmm3, xmm2
    mulss   xmm2, [ac_0_4]
    addss   xmm2, [ac_0_8]
    movss   [gt_dp], xmm2
    mulss   xmm3, [ac_2_5]
    cvttss2si eax, xmm3
    cvtsi2ss xmm2, eax
    subss   xmm3, xmm2
    mulss   xmm3, [ac_0_8]
    addss   xmm3, [ac_0_9]
    movss   [rsp + 36], xmm3
    ; игрок
    subss   xmm0, [pl + E_X]
    subss   xmm1, [pl + E_Y]
    mulss   xmm0, xmm0
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    movss   [rsp + 32], xmm0
    comiss  xmm0, [ac_900]
    jae     .kn                         ; дальше 30 — не слышно
    comiss  xmm0, [ac_10]
    jb      .bark
    ; зомби
    lea     rbx, [ents]
    mov     ecx, MAXE
.z:
    mov     eax, [rbx + E_ST]
    cmp     eax, ST_WANDER
    jb      .zn
    cmp     eax, ST_CORPSE
    jae     .zn
    movss   xmm0, [rbx + E_X]
    subss   xmm0, [gt_sx]
    mulss   xmm0, xmm0
    movss   xmm1, [rbx + E_Y]
    subss   xmm1, [gt_sy]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    comiss  xmm0, [ac_30]
    jb      .bark
.zn:
    add     rbx, ESZ
    dec     ecx
    jnz     .z
    ; игрок в 3–5 тайлах: рычит
    movss   xmm0, [rsp + 32]
    comiss  xmm0, [ac_25]
    jae     .kn
    call    g_rndf
    mulss   xmm0, [ac_2]
    addss   xmm0, [ac_2_5]
    lea     rdx, [kn_t]
    movss   [rdx + r12*4], xmm0
    mov     eax, [rsp + 36]
    mov     [gt_a], eax
    mov     byte [gt_nocc], 1
    call    g_rndf
    mulss   xmm0, [ac_0_2]
    addss   xmm0, [ac_0_9]
    mulss   xmm0, [gt_dp]
    movaps  xmm3, xmm0
    mov     ecx, SN_GROWLD
    movss   xmm0, [gt_sx]
    movss   xmm1, [gt_sy]
    movss   xmm2, [ac_1]
    call    snd_emit
    jmp     .kn
.bark:
    call    g_rndf
    mulss   xmm0, [ac_3]
    addss   xmm0, [ac_3]
    lea     rdx, [kn_t]
    movss   [rdx + r12*4], xmm0
    ; кидается — цепь звенит
    mov     dword [gt_del], __float32__(0.05)
    mov     byte [gt_nocc], 1
    mov     ecx, SN_CHAIN
    movss   xmm0, [gt_sx]
    movss   xmm1, [gt_sy]
    movss   xmm2, [ac_0_8]
    movss   xmm3, [ac_1]
    call    snd_emit
    call    g_rndf
    mulss   xmm0, [ac_4]
    cvttss2si r13d, xmm0
    add     r13d, 3
    mov     dword [gt_dd], 0
.b:
    call    g_rndf
    mulss   xmm0, [ac_0_12]
    addss   xmm0, [ac_0_28]
    addss   xmm0, [gt_dd]
    movss   [gt_dd], xmm0
    movss   [gt_del], xmm0
    mov     eax, [rsp + 36]
    mov     [gt_a], eax
    mov     byte [gt_nocc], 1
    call    g_rndf
    subss   xmm0, [ac_0_5]
    mulss   xmm0, [ac_0_08]
    addss   xmm0, [ac_1]
    mulss   xmm0, [gt_dp]
    movaps  xmm3, xmm0
    mov     ecx, SN_BARK
    movss   xmm0, [gt_sx]
    movss   xmm1, [gt_sy]
    movss   xmm2, [ac_1_2]
    call    snd_emit
    dec     r13d
    jnz     .b
.kn:
    inc     r12d
    jmp     .k
.out:
    add     rsp, 48
    pop     r13
    pop     r12
    pop     rbx
    ret

; --- в порывы хлопают ставни и калитки у ближних изб; иногда сперва скрипит петля
az_knocks:
    push    rbx
    sub     rsp, 32
    movss   xmm0, [gt_wf]
    subss   xmm0, [ac_0_5]
    mulss   xmm0, [ac_1_4]
    CLAMP01 xmm0
    comiss  xmm0, [ac_0]
    jbe     .out
    mulss   xmm0, [dt]
    movss   xmm1, [kn_gt]
    subss   xmm1, xmm0
    movss   [kn_gt], xmm1
    comiss  xmm1, [ac_0]
    ja      .out
    call    g_rndf
    mulss   xmm0, [ac_3_5]
    addss   xmm0, [ac_1_2]
    movss   [kn_gt], xmm0
    cmp     dword [nhouses], 0
    je      .out
    mov     ebx, 6
.try:
    call    g_rndf
    cvtsi2ss xmm1, dword [nhouses]
    mulss   xmm0, xmm1
    cvttss2si eax, xmm0
    imul    eax, eax, HS_SZ
    lea     rdx, [houses]
    add     rdx, rax
    ; точка на стене: сторона x0 или x0 + w, случайно вдоль
    call    g_rndf
    cvtsi2ss xmm1, dword [rdx + HS_H]
    mulss   xmm0, xmm1
    cvtsi2ss xmm1, dword [rdx + HS_Y0]
    addss   xmm0, xmm1
    movss   [gt_sy], xmm0
    cvtsi2ss xmm0, dword [rdx + HS_X0]
    mov     eax, [gt_rng]
    test    eax, 0x100
    jz      .side
    cvtsi2ss xmm1, dword [rdx + HS_W]
    addss   xmm0, xmm1
.side:
    movss   [gt_sx], xmm0
    subss   xmm0, [pl + E_X]
    mulss   xmm0, xmm0
    movss   xmm1, [gt_sy]
    subss   xmm1, [pl + E_Y]
    mulss   xmm1, xmm1
    addss   xmm0, xmm1
    comiss  xmm0, [ac_256]
    jb      .got
    dec     ebx
    jnz     .try
    jmp     .out
.got:
    call    g_rndf
    comiss  xmm0, [ac_0_25]
    jae     .nocr
    mov     byte [gt_nocc], 1
    mov     ecx, SN_CREAK
    movss   xmm0, [gt_sx]
    movss   xmm1, [gt_sy]
    movss   xmm2, [ac_1]
    movss   xmm3, [ac_1]
    call    snd_emit
    mov     dword [gt_dd], __float32__(0.4)
    jmp     .kn
.nocr:
    mov     dword [gt_dd], 0
.kn:
    call    g_rndf
    mulss   xmm0, [ac_0_4]
    addss   xmm0, [ac_0_6]
    movss   [gt_t2], xmm0
    mov     eax, [gt_dd]
    mov     [gt_del], eax
    mov     byte [gt_nocc], 1
    mov     ecx, SN_KNOCK
    movss   xmm0, [gt_sx]
    movss   xmm1, [gt_sy]
    movss   xmm2, [gt_t2]
    movss   xmm3, [ac_1]
    call    snd_emit
    call    g_rndf
    comiss  xmm0, [ac_0_4]
    jae     .out
    ; ещё раз, тише: ставня отскочила
    call    g_rndf
    mulss   xmm0, [ac_0_18]
    addss   xmm0, [ac_0_12]
    addss   xmm0, [gt_dd]
    movss   [gt_del], xmm0
    movss   xmm2, [gt_t2]
    mulss   xmm2, [ac_0_65]
    mov     byte [gt_nocc], 1
    mov     ecx, SN_KNOCK
    movss   xmm0, [gt_sx]
    movss   xmm1, [gt_sy]
    movss   xmm3, [ac_1_1]
    call    snd_emit
.out:
    add     rsp, 32
    pop     rbx
    ret

; --- исход ночи и рассвет: смерть — глухой удар. Петухи: первые — в полночь, вторые — в
; два, третьи — перед рассветом (6:40), ещё в 7:20 и на рассвете
az_misc:
    push    rbx
    sub     rsp, 32
    mov     eax, [gstate]
    cmp     eax, [gt_gs]
    je      .roo
    mov     [gt_gs], eax
    cmp     eax, 1
    jne     .nd
    mov     ecx, SN_DEATH
    xorps   xmm0, xmm0
    movss   xmm2, [ac_1]
    movss   xmm3, [ac_1]
    call    snd_emit_c
    jmp     .roo
.nd:
    cmp     eax, 2
    jne     .roo
    call    roo_chorus
.roo:
    xor     ebx, ebx
.rt:
    lea     rdx, [roo_at]
    movss   xmm0, [gtime]
    comiss  xmm0, [rdx + rbx*4]
    jb      .rn
    movzx   eax, byte [gt_roo]
    bts     eax, ebx
    jc      .rn
    mov     [gt_roo], al
    subss   xmm0, [rdx + rbx*4]
    comiss  xmm0, [ac_10]
    jae     .rn                         ; давно прошло (ночь начата позже) — молчат
    call    roo_chorus
.rn:
    inc     ebx
    cmp     ebx, 4
    jb      .rt
    add     rsp, 32
    pop     rbx
    ret

; петухи перекликаются: начинает один (2–3 раза через 4.5–8.5 с), другие отвечают со
; своих дворов по 1–2 раза — всё задержками вперёд, с эхом
roo_chorus:
    push    rbx
    push    r12
    push    r13
    push    r14
    sub     rsp, 40
    call    g_rndf
    mulss   xmm0, [ac_nro]
    cvttss2si r12d, xmm0                ; кто начнёт
    xor     r13d, r13d                  ; который по счёту
.bird:
    ; свой двор, голос и хрипота
    lea     rdx, [ro_x]
    mov     eax, [rdx + r12*4]
    mov     [gt_sx], eax
    lea     rdx, [ro_y]
    mov     eax, [rdx + r12*4]
    mov     [gt_sy], eax
    ; когда начнёт: первый — сразу, другие — через 1.5–6 с после предыдущего
    xorps   xmm0, xmm0
    test    r13d, r13d
    jz      .t0
    call    g_rndf
    mulss   xmm0, [ac_4_5]
    addss   xmm0, [ac_1_5]
    addss   xmm0, [gt_t3]
.t0:
    movss   [gt_t3], xmm0
    movss   [gt_dd], xmm0
    ; сколько раз: первый 2–3, другие 1–2
    call    g_rndf
    xor     ebx, ebx
    comiss  xmm0, [ac_0_5]
    setb    bl
    movzx   ebx, bl
    inc     ebx
    test    r13d, r13d
    jnz     .c
    inc     ebx
.c:
    lea     rdx, [ro_r]
    mov     eax, [rdx + r12*4]
    mov     [gt_a], eax
    mov     eax, [gt_dd]
    mov     [gt_del], eax
    mov     byte [gt_nocc], 1
    call    g_rndf
    subss   xmm0, [ac_0_5]
    mulss   xmm0, [ac_0_03]
    addss   xmm0, [ac_1]
    lea     rdx, [ro_p]
    mulss   xmm0, [rdx + r12*4]
    movaps  xmm3, xmm0
    mov     ecx, SN_ROOSTER
    movss   xmm0, [gt_sx]
    movss   xmm1, [gt_sy]
    movss   xmm2, [ac_1_5]
    call    snd_emit_e
    call    g_rndf
    mulss   xmm0, [ac_4]
    addss   xmm0, [ac_4_5]
    addss   xmm0, [gt_dd]
    movss   [gt_dd], xmm0
    dec     ebx
    jnz     .c
    ; следующий двор
    inc     r12d
    cmp     r12d, NRO
    jb      .nx
    xor     r12d, r12d
.nx:
    inc     r13d
    cmp     r13d, NRO
    jb      .bird
    add     rsp, 40
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    ret

; новая игра: таймеры зомби, будки (по карте), собаки, петухи
audio_newgame:
    push    rdi
    push    rbx
    sub     rsp, 40
    lea     rdi, [zst]
    xor     eax, eax
    mov     ecx, MAXE
    rep     stosb
    lea     rdi, [zvt]
    mov     ecx, MAXE
    rep     stosd
    mov     dword [dog_t], __float32__(9.0)
    mov     dword [kn_gt], __float32__(2.0)
    mov     byte [gt_roo], 0
    ; собаки — по дворам за околицей (60–90 тайлов от середины), петухи — по селу (30–70):
    ; у каждой своё место, голос и хрипота на всю ночь
    xor     ebx, ebx
.fd:
    movss   xmm0, [ac_60]
    movss   xmm1, [ac_30]
    call    place_ring
    mov     eax, [gt_sx]
    lea     rdx, [fd_x]
    mov     [rdx + rbx*4], eax
    mov     eax, [gt_sy]
    lea     rdx, [fd_y]
    mov     [rdx + rbx*4], eax
    call    g_rndf
    mulss   xmm0, [ac_0_45]
    addss   xmm0, [ac_0_8]
    lea     rdx, [fd_p]
    movss   [rdx + rbx*4], xmm0
    ; нрав: мелкая (голос выше) тявкает часто, крупная басит реже — 0.35..0.95 с
    movss   xmm1, [ac_1_25]
    subss   xmm1, xmm0
    mulss   xmm1, [ac_1_1]
    addss   xmm1, [ac_0_3]
    movss   [rsp + 32], xmm1
    call    g_rndf
    mulss   xmm0, [ac_0_15]
    addss   xmm0, [rsp + 32]
    lea     rdx, [fd_t]
    movss   [rdx + rbx*4], xmm0
    call    g_rndf
    addss   xmm0, [ac_0_7]
    lea     rdx, [fd_r]
    movss   [rdx + rbx*4], xmm0
    inc     ebx
    cmp     ebx, NFD
    jb      .fd
    xor     ebx, ebx
.ro:
    movss   xmm0, [ac_30]
    movss   xmm1, [ac_40]
    call    place_ring
    mov     eax, [gt_sx]
    lea     rdx, [ro_x]
    mov     [rdx + rbx*4], eax
    mov     eax, [gt_sy]
    lea     rdx, [ro_y]
    mov     [rdx + rbx*4], eax
    call    g_rndf
    mulss   xmm0, [ac_0_3]
    addss   xmm0, [ac_0_72]
    lea     rdx, [ro_p]
    movss   [rdx + rbx*4], xmm0
    call    g_rndf
    mulss   xmm0, [ac_1_3]
    addss   xmm0, [ac_0_7]
    lea     rdx, [ro_r]
    movss   [rdx + rbx*4], xmm0
    inc     ebx
    cmp     ebx, NRO
    jb      .ro
    mov     dword [gt_gs], 0
    mov     dword [kn_n], 0
    lea     rdx, [tmap]
    xor     eax, eax
.f:
    cmp     byte [rdx + rax], T_KENNEL
    jne     .fn
    mov     ecx, [kn_n]
    cmp     ecx, MAXKN
    jae     .fn
    lea     r8, [kn_xy]
    mov     r9d, eax
    and     r9d, MAPW - 1
    mov     [r8 + rcx*2], r9b
    mov     r9d, eax
    shr     r9d, 7
    mov     [r8 + rcx*2 + 1], r9b
    lea     r8, [kn_t]
    mov     dword [r8 + rcx*4], __float32__(1.0)
    inc     dword [kn_n]
.fn:
    inc     eax
    cmp     eax, MAPW*MAPW
    jb      .f
    add     rsp, 40
    pop     rbx
    pop     rdi
    ret

; xmm0 = от, xmm1 = разброс: случайная точка на кольце вокруг середины карты -> gt_sx, gt_sy
place_ring:
    sub     rsp, 40
    movss   [rsp + 32], xmm0
    movss   [rsp + 36], xmm1
    call    g_rndf
    mulss   xmm0, [rsp + 36]
    addss   xmm0, [rsp + 32]
    movss   [rsp + 32], xmm0
    call    g_rndf
    mulss   xmm0, [ac_2pi]
    movss   [rsp + 36], xmm0
    call    fsin
    mulss   xmm0, [rsp + 32]
    addss   xmm0, [ac_mid]
    movss   [gt_sy], xmm0
    movss   xmm0, [rsp + 36]
    addss   xmm0, [f_pi_2]
    call    fsin
    mulss   xmm0, [rsp + 32]
    addss   xmm0, [ac_mid]
    movss   [gt_sx], xmm0
    add     rsp, 40
    ret

; --sndtest: каждый звук события по очереди (по 216 блоков), потом фоны по одному
; (по 258 блоков) -> sndtest.wav
aud_sndtest:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32
    call    wav_begin
    mov     dword [ap + AP_VOL], __float32__(1.0)
    mov     dword [bs + AP_VOL], __float32__(1.0)
    mov     dword [mo_g], __float32__(1.0)
    xor     ebx, ebx
.s:
    ; шаги — чередой, лай — серией (sndt_rep, sndt_gap); ноги — чуть в стороны
    lea     rdx, [sndt_rep]
    movzx   esi, byte [rdx + rbx]
    xor     edi, edi
.rep:
    cvtsi2ss xmm0, edi
    lea     rdx, [sndt_gap]
    mulss   xmm0, [rdx + rbx*4]
    movss   [gt_del], xmm0
    movss   xmm0, [ac_0_08]
    test    edi, 1
    jz      .rp
    xorps   xmm0, [ac_sign]
.rp:
    mov     ecx, ebx
    movss   xmm2, [ac_1]
    movss   xmm3, [ac_1]
    call    snd_emit_c
    inc     edi
    cmp     edi, esi
    jb      .rep
    mov     dword [wav_owe], 216*ABLK   ; ровно 216 блоков (~2.5 с) на звук
    mov     dword [dt], 0
    call    wav_pump
    inc     ebx
    cmp     ebx, NSND
    jb      .s
    ; фоны по одному, по 3 с: параметры из bedt (смещение, значение; −1 — конец куска)
    lea     rsi, [bedt]
.bed:
    cmp     dword [rsi], -2
    je      .bd
    lea     rdi, [ap]
    xor     eax, eax
    mov     ecx, NAP
    rep     stosd
    mov     dword [ap + AP_VOL], __float32__(1.0)
.bp:
    mov     eax, [rsi]
    add     rsi, 4
    cmp     eax, -1
    je      .bgo
    mov     ecx, [rsi]
    add     rsi, 4
    lea     rdx, [ap]
    mov     [rdx + rax], ecx
    jmp     .bp
.bgo:
    mov     dword [wav_owe], 258*ABLK
    mov     dword [dt], 0
    call    wav_pump
    jmp     .bed
.bd:
    ; издалека, в тишине и безветрии: собака в ~52 тайлах — серия лая с эхом, потом петух
    ; в ~39 (тот же путь, что в игре: расстояние, воздух, эхо)
    lea     rdi, [ap]
    xor     eax, eax
    mov     ecx, NAP
    rep     stosd
    mov     dword [ap + AP_VOL], __float32__(1.0)
    mov     dword [wind_g], 0
    xor     ebx, ebx
.fb:
    cvtsi2ss xmm0, ebx
    mulss   xmm0, [ac_0_4]
    addss   xmm0, [ac_0_2]
    movss   [gt_del], xmm0
    mov     eax, [fd_r]
    mov     [gt_a], eax
    mov     byte [gt_nocc], 1
    mov     ecx, SN_BARK
    movss   xmm0, [pl + E_X]
    addss   xmm0, [ac_48]
    movss   xmm1, [pl + E_Y]
    subss   xmm1, [ac_20]
    movss   xmm2, [ac_1]
    movss   xmm3, [fd_p]
    call    snd_emit_e
    inc     ebx
    cmp     ebx, 5
    jb      .fb
    mov     dword [gt_del], __float32__(2.6)
    mov     eax, [ro_r]
    mov     [gt_a], eax
    mov     byte [gt_nocc], 1
    mov     ecx, SN_ROOSTER
    movss   xmm0, [pl + E_X]
    subss   xmm0, [ac_30]
    movss   xmm1, [pl + E_Y]
    subss   xmm1, [ac_25]
    movss   xmm2, [ac_1_5]
    movss   xmm3, [ro_p]
    call    snd_emit_e
    mov     dword [wav_owe], 480*ABLK   ; ~5.6 с
    mov     dword [dt], 0
    call    wav_pump
    lea     rcx, [s_sndtname]
    call    wav_save
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ================================================================ данные звука
FNO         equ 0
FLP         equ 1
FBP         equ 2
FHP         equ 3
OSIN        equ 0
OSAW        equ 1
; слой: шанс, задержка с, +случайно до, громкость, ±доля, атака с, держать с, ±доля, спад τ с
%macro LAYER 9
    dd %1, %2, %3, %4, %5, %6, %7, %8, %9
%endmacro
; источник: шум, осц.->фильтры, осц. напрямую, порог щелчков, осц. (OSIN/OSAW), f0 Гц,
; ±доля, глиссандо (конец/начало), τ с, дрожь, АМ Гц, АМ глубина, предел с, фильтры за высотой
%macro SRC 14
    dd %1, %2, %3, %4, %5, %6, %7, %8, %9, %10, %11, %12, %13, %14
%endmacro
; фильтр: вид, срез Гц, ±доля, конец/начало, τ с, Q, громкость
%macro FLT 7
    dd %1, %2, %3, %4, %5, %6, %7
%endmacro
%macro NOFLT 0
    dd FNO, 0, 0, 0, 0, 1.0, 0
%endmacro
%macro LEND 0
    dd 0, 0, 0
%endmacro

section .rdata
align 16, db 0
lay_tab:
; --- шаг по снегу: хруст — зёрнами (asynth.inc, gd_snow), здесь — мягкий удар веса:
;     валенок уминает снег, глухо (100–300 Гц)
L_ssnow:
    LAYER 1.0, 0, 0.004, 0.3, 0.25, 0.006, 0.025, 0.3, 0.04
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.3, 0
    FLT   FLP, 240.0, 0.2, 0.7, 0.05, 0.9, 1.0
    FLT   FBP, 110.0, 0.15, 1.0, 0, 1.5, 0.8
    LEND
; --- укатанная дорога: скрип — зёрнами (gd_road), удар — твёрже и короче
L_sroad:
    LAYER 1.0, 0, 0.004, 0.3, 0.25, 0.004, 0.015, 0.3, 0.03
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.3, 0
    FLT   FLP, 300.0, 0.2, 0.7, 0.04, 0.9, 1.0
    FLT   FBP, 130.0, 0.15, 1.0, 0, 1.5, 0.7
    LEND
; --- по половицам в валенках: глухой стук, доска отзывается; скрипят свои половицы
;     (SN_FCREAK из snd_step_pl)
L_swood:
    LAYER 1.0, 0, 0.004, 1.2, 0.2, 0.002, 0.012, 0.3, 0.03
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.4, 0
    FLT   FBP, 170.0, 0.12, 1.0, 0, 4.0, 1.4
    FLT   FLP, 400.0, 0.1, 1.0, 0, 0.8, 0.5
    LEND
    LAYER 1.0, 0, 0.004, 0.35, 0.3, 0.001, 0.004, 0.3, 0.012
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.2, 0
    FLT   FBP, 720.0, 0.15, 1.0, 0, 3.0, 1.0
    FLT   FBP, 1500.0, 0.15, 1.0, 0, 3.0, 0.4
    LEND
; --- зомби волочит ноги: скребёт — зёрнами (gd_zdrag), здесь — тяжёлый шаг
L_sz:
    LAYER 1.0, 0.02, 0.02, 0.4, 0.25, 0.008, 0.03, 0.3, 0.05
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.4, 0
    FLT   FLP, 220.0, 0.2, 0.7, 0.05, 0.9, 1.0
    FLT   FBP, 95.0, 0.15, 1.0, 0, 1.5, 0.8
    LEND
; --- взмах лопатой: полоса шума съезжает вниз, пока громкость растёт — «вжух»
L_swing:
    LAYER 1.0, 0, 0, 0.84, 0.15, 0.07, 0.02, 0.2, 0.06
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.8, 0
    FLT   FBP, 1900.0, 0.1, 0.3, 0.1, 2.2, 1.0
    FLT   FLP, 700.0, 0.1, 1.0, 0, 0.7, 0.35
    LEND
; --- удар: глухой удар в тело, лопата звенит (два узких резонанса железа), хлюп
L_hit:
    LAYER 1.0, 0, 0, 0.55, 0.1, 0.001, 0.01, 0.2, 0.07
    SRC   0.5, 0, 1.0, 0, OSIN, 105.0, 0.1, 0.5, 0.05, 0, 0, 0, 0.8, 0
    FLT   FLP, 900.0, 0.1, 1.0, 0, 0.7, 0.8
    NOFLT
    LEND
    LAYER 1.0, 0, 0, 0.25, 0.2, 0.0005, 0.002, 0, 0.002
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0
    FLT   FBP, 1180.0, 0.06, 1.0, 0, 400.0, 350.0
    FLT   FBP, 2790.0, 0.06, 1.0, 0, 500.0, 260.0
    LEND
    LAYER 1.0, 0, 0, 0.3, 0.2, 0.001, 0.008, 0.2, 0.03
    SRC   1.0, 0, 0, 0.7, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.5, 0
    FLT   FBP, 2600.0, 0.15, 1.0, 0, 1.0, 1.0
    FLT   FBP, 700.0, 0.15, 1.0, 0, 1.5, 0.5
    LEND
; --- добит: падает в снег (удар + хруст), последний стон
L_kill:
    LAYER 1.0, 0.28, 0.04, 0.6, 0.1, 0.004, 0.03, 0.2, 0.12
    SRC   0.6, 0, 1.0, 0, OSIN, 62.0, 0.1, 0.65, 0.08, 0, 0, 0, 1.2, 0
    FLT   FLP, 500.0, 0.1, 1.0, 0, 0.7, 1.0
    NOFLT
    LEND
    LAYER 1.0, 0.27, 0.04, 0.4, 0.2, 0.01, 0.06, 0.3, 0.12
    SRC   1.0, 0, 0, 0.85, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 1.2, 0
    FLT   FBP, 900.0, 0.15, 1.0, 0, 1.0, 1.0
    FLT   FBP, 2600.0, 0.15, 1.0, 0, 1.3, 0.5
    LEND
    ; последний стон — зов (asynth.inc, cd_dmoan)
; --- зомби достал: взмах и удар по телогрейке
L_zatk:
    LAYER 1.0, 0, 0, 0.3, 0.2, 0.04, 0, 0, 0.04
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.5, 0
    FLT   FBP, 1400.0, 0.1, 0.4, 0.06, 2.0, 1.0
    NOFLT
    LEND
    LAYER 1.0, 0.05, 0.01, 0.75, 0.1, 0.001, 0.015, 0.2, 0.1
    SRC   0.7, 0, 1.0, 0, OSIN, 80.0, 0.1, 0.55, 0.06, 0, 0, 0, 1.0, 0
    FLT   FLP, 400.0, 0.1, 1.0, 0, 0.8, 1.0
    FLT   FBP, 1800.0, 0.1, 1.0, 0, 1.2, 0.25
    LEND
; --- рык, стон, «хм?», вскрик, рык на бегу — зов (asynth.inc: cd_snarl, cd_groan, cd_hmm,
;     cd_aggro, cd_zgrowl)
; --- учуял по ветру: три вдоха носом, потом «хм» (cd_shmm)
L_sniff:
    LAYER 1.0, 0, 0, 0.42, 0.2, 0.025, 0.04, 0.3, 0.035
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.4, 0
    FLT   FBP, 2100.0, 0.1, 1.0, 0, 1.3, 1.0
    FLT   FHP, 4500.0, 0.1, 1.0, 0, 0.7, 0.35
    LEND
    LAYER 1.0, 0.17, 0.03, 0.46, 0.2, 0.025, 0.04, 0.3, 0.035
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.4, 0
    FLT   FBP, 2100.0, 0.1, 1.0, 0, 1.3, 1.0
    FLT   FHP, 4500.0, 0.1, 1.0, 0, 0.7, 0.35
    LEND
    LAYER 1.0, 0.34, 0.04, 0.42, 0.2, 0.025, 0.04, 0.3, 0.035
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.4, 0
    FLT   FBP, 2100.0, 0.1, 1.0, 0, 1.3, 1.0
    FLT   FHP, 4500.0, 0.1, 1.0, 0, 0.7, 0.35
    LEND
; --- выдох (вместе с паром изо рта)
L_breath:
    LAYER 1.0, 0, 0, 0.21, 0.2, 0.06, 0.08, 0.3, 0.12
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.8, 0
    FLT   FBP, 1050.0, 0.1, 0.8, 0.2, 1.4, 1.0
    FLT   FLP, 450.0, 0.1, 1.0, 0, 0.7, 0.4
    LEND
; --- щелчок выключателя фонарика (два щелчка механизма)
L_click:
    LAYER 1.0, 0, 0, 0.18, 0.1, 0.0003, 0.0005, 0, 0.002
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.2, 0
    FLT   FBP, 3400.0, 0.05, 1.0, 0, 3.0, 1.5
    FLT   FBP, 1300.0, 0.05, 1.0, 0, 5.0, 1.0
    LEND
    LAYER 1.0, 0.035, 0.005, 0.1, 0.1, 0.0003, 0.0005, 0, 0.002
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.2, 0
    FLT   FBP, 3000.0, 0.05, 1.0, 0, 3.0, 1.5
    FLT   FBP, 1200.0, 0.05, 1.0, 0, 5.0, 1.0
    LEND
; --- ставня/калитка хлопнула: доска звенит, глухой удар
L_knock:
    LAYER 1.0, 0, 0, 0.8, 0.25, 0.001, 0.004, 0.3, 0.02
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.8, 0
    FLT   FBP, 310.0, 0.12, 1.0, 0, 20.0, 2.5
    FLT   FBP, 1150.0, 0.15, 1.0, 0, 6.0, 0.8
    LEND
    LAYER 1.0, 0, 0, 0.5, 0.2, 0.001, 0.015, 0.2, 0.07
    SRC   0, 0, 1.0, 0, OSIN, 95.0, 0.12, 0.7, 0.06, 0, 0, 0, 0.6, 0
    NOFLT
    NOFLT
    LEND
; --- петля ставни визжит — зёрна (asynth.inc, gd_hinge)
; --- сердце: тук-тук
L_hb:
    LAYER 1.0, 0, 0, 0.35, 0.1, 0.004, 0.01, 0, 0.06
    SRC   0.3, 0, 1.0, 0, OSIN, 58.0, 0.05, 0.72, 0.05, 0, 0, 0, 0.5, 0
    FLT   FLP, 140.0, 0, 1.0, 0, 0.7, 1.0
    NOFLT
    LEND
    LAYER 1.0, 0.16, 0, 0.22, 0.1, 0.004, 0.01, 0, 0.06
    SRC   0.3, 0, 1.0, 0, OSIN, 50.0, 0.05, 0.72, 0.05, 0, 0, 0, 0.5, 0
    FLT   FLP, 140.0, 0, 1.0, 0, 0.7, 1.0
    NOFLT
    LEND
; --- смерть: глухой удар, всё проваливается в вату
L_death:
    LAYER 1.0, 0, 0, 0.68, 0, 0.005, 0.05, 0, 0.9
    SRC   0.5, 0, 1.0, 0, OSIN, 52.0, 0, 0.55, 0.5, 0, 0, 0, 5.0, 0
    FLT   FLP, 220.0, 0, 1.0, 0, 0.7, 1.0
    NOFLT
    LEND
; --- сломанный фонарь: треск дросселя на вспышке
L_brk:
    LAYER 1.0, 0, 0, 0.6, 0.3, 0.0003, 0.002, 0.5, 0.006
    SRC   1.0, 0, 0, 0.6, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.2, 0
    FLT   FBP, 2600.0, 0.2, 1.0, 0, 1.5, 1.0
    FLT   FBP, 7000.0, 0.2, 1.0, 0, 1.2, 0.6
    LEND
; --- снег с веток: мягкий ком и осыпь
L_flump:
    LAYER 1.0, 0, 0.05, 1.0, 0.25, 0.02, 0.08, 0.3, 0.15
    SRC   1.0, 0, 0, 0.8, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0
    FLT   FLP, 450.0, 0.2, 1.0, 0, 0.8, 1.0
    FLT   FBP, 1500.0, 0.2, 1.0, 0, 1.2, 0.35
    LEND
; --- вдох
L_inhale:
    LAYER 1.0, 0, 0, 0.11, 0.2, 0.12, 0.04, 0.3, 0.06
    SRC   1.0, 0, 0, 0, OSIN, 0, 0, 1.0, 0, 0, 0, 0, 0.6, 0
    FLT   FBP, 1700.0, 0.1, 1.2, 0.15, 1.6, 1.0
    FLT   FHP, 3000.0, 0.1, 1.0, 0, 0.7, 0.3
    LEND
; --- сырое полено шипит: пар рвётся рывками — шершавый шум высоко, дрожит
L_sizzle:
    LAYER 1.0, 0, 0, 0.06, 0.3, 0.25, 0.8, 0.5, 0.4
    SRC   1.0, 0, 0, 0.3, OSIN, 0, 0, 1.0, 0, 0, 7.0, 0.6, 2.5, 0
    FLT   FBP, 5200.0, 0.2, 0.8, 1.0, 1.4, 1.0
    FLT   FHP, 7500.0, 0.1, 1.0, 0, 0.7, 0.3
    LEND
lay_end:

%define LAYIX(l) ((l - lay_tab) / LYSZ)
; звук -> первый слой, сколько слоёв (в порядке SN_*)
snd_tab:
    db LAYIX(L_ssnow), 1
    db LAYIX(L_sroad), 1
    db LAYIX(L_swood), 2
    db LAYIX(L_sz), 1
    db LAYIX(L_swing), 1
    db LAYIX(L_hit), 3
    db LAYIX(L_kill), 2                 ; + последний стон — зов
    db LAYIX(L_zatk), 2
    db 0, 0                             ; рык при ударе — зов
    db 0, 0                             ; стон — зов
    db 0, 0                             ; «хм?» — зов
    db 0, 0                             ; вскрик — зов
    db 0, 0                             ; рык на бегу — зов
    db LAYIX(L_sniff), 3                ; + «хм» — зов
    db LAYIX(L_breath), 1
    db LAYIX(L_click), 2
    db LAYIX(L_knock), 2
    db 0, 0                             ; петля — зёрна
    db 0, 0                             ; лай — зов
    db 0, 0                             ; вой — зов
    db LAYIX(L_hb), 2
    db LAYIX(L_death), 1
    db 0, 0                             ; полено — зёрна
    db LAYIX(L_brk), 1
    db LAYIX(L_flump), 1
    db 0, 0                             ; петух — зов
    db LAYIX(L_inhale), 1
    db 0, 0                             ; половица — зёрна
    db 0, 0                             ; рык — зов
    db 0, 0                             ; цепь — зёрна
    db LAYIX(L_sizzle), 1
snd_tab_end:
; звук -> голос из asynth.inc: вид (0 — нет), номер описания
snd_xk:
    db VK_GRAIN, GD_SNOW
    db VK_GRAIN, GD_ROAD
    db 0, 0
    db VK_GRAIN, GD_ZDRAG
    db 0, 0, 0, 0                       ; взмах, удар
    db VK_CALL, CD_DMOAN                ; добит
    db 0, 0                             ; достал
    db VK_CALL, CD_SNARL
    db VK_CALL, CD_GROAN
    db VK_CALL, CD_HMM
    db VK_CALL, CD_AGGRO
    db VK_CALL, CD_ZGROWL
    db VK_CALL, CD_SHMM                 ; учуял
    db 0, 0, 0, 0, 0, 0                 ; выдох, фонарик, ставня
    db VK_GRAIN, GD_HINGE
    db VK_CALL, CD_BARK
    db VK_CALL, CD_HOWL
    db 0, 0, 0, 0                       ; сердце, смерть
    db VK_GRAIN, GD_POP
    db 0, 0                             ; фонарь
    db VK_GRAIN, GD_SPRINKLE            ; ком снега: осыпь
    db VK_CALL, CD_ROO
    db 0, 0                             ; вдох
    db VK_GRAIN, GD_FCREAK
    db VK_CALL, CD_GROWL
    db VK_GRAIN, GD_CHAIN
    db 0, 0                             ; шипит
%if $ - snd_xk != NSND*2
%error "snd_xk: не по числу звуков"
%endif
; --sndtest: сколько раз подряд и через сколько секунд (шаги — ходьбой, лай — серией)
sndt_rep:
    db 6, 6, 6, 4                       ; шаги
    times SN_BARK - SN_SWING db 1
    db 4, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1
%if $ - sndt_rep != NSND
%error "sndt_rep: не по числу звуков"
%endif
align 4, db 0
sndt_gap:
    dd 0.165, 0.165, 0.165, 0.4
    times SN_BARK - SN_SWING dd 0.5
    dd 0.42, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.7, 1.0, 1.0, 1.0

align 4, db 0
; --sndtest, фоны по одному: (смещение AP_*, значение)..., −1; в конце −2
bedt:
    dd AP_WN, 0.75, AP_WL, 0.75, AP_WR, 0.75, AP_WF, 0.75, AP_GX, 0.5, -1     ; ветрено, чисто
    dd AP_WN, 1.5, AP_WL, 1.5, AP_WR, 1.3, AP_WF, 1.5, AP_GX, 0.8, AP_EDGE, 1.0, AP_HISS, 0.8, -1
    dd AP_IN, 1.0, AP_WF, 1.3, AP_GX, 0.7, AP_FIRE, 1.0, -1                     ; в избе в метель
    dd AP_WF, 1.0, AP_SPR, 1.0, AP_BIR, 1.0, AP_BIRP, 0.6, -1                   ; лес
    dd AP_WF, 1.0, AP_WIRE, 1.0, AP_WIRF, 200.0, -1                             ; провода
    dd AP_HUM, 1.0, AP_BRK, 0.6, AP_BRKP, 0.5, -1                               ; фонари
    dd AP_TV, 1.0, AP_TVL, 1.0, -1                                              ; телевизор
    dd AP_HB, 0.8, AP_HBR, 1.8, -1                                              ; сердце
    dd -2
; полосы фонов: окраска шума c, белый, окрашенный, порог щелчков (2 — нет), щелчки
ls_col:
    dd 0.35, 0.25, 1.0, 2.0, 0          ; 0 ветер у игрока: слева ФНЧ
    dd 0.35, 0.25, 1.0, 2.0, 0          ; 1   справа ФНЧ
    dd 0.2, 0.3, 1.0, 2.0, 0            ; 2   слева полоса
    dd 0.2, 0.3, 1.0, 2.0, 0            ; 3   справа полоса
    dd 0.08, 0.05, 1.0, 2.0, 0          ; 4 гул над селом слева
    dd 0.08, 0.05, 1.0, 2.0, 0          ; 5   справа
    dd 0.02, 0, 1.0, 2.0, 0             ; 6 буханье
    dd 0.05, 0, 1.0, 2.0, 0             ; 7 труба
    dd 0.5, 1.0, 0, 2.0, 0              ; 8 вой
    dd 0.5, 1.0, 0, 2.0, 0              ; 9
    dd 0.5, 1.0, 0, 2.0, 0              ; 10
    dd 0.08, 0, 1.0, 2.0, 0             ; 11 труба, второй резонанс
    dd 0.3, 0.3, 1.0, 2.0, 0            ; 12 ели
    dd 0.5, 1.0, 0, 2.0, 0              ; 13 берёзы
    dd 0.5, 0.6, 0, 0.992, 1.5          ; 14 позёмка слева: шорох + уколы льдинок
    dd 0.5, 0.6, 0, 0.992, 1.5          ; 15   справа
    dd 0.03, 0, 1.0, 2.0, 0             ; 16 печь гудит
    dd 0.5, 0, 0, 0.9985, 1.0           ; 17   трещит
    dd 0.5, 1.0, 0, 2.0, 0              ; 18 телевизор, форманта 1
    dd 0.5, 1.0, 0, 2.0, 0              ; 19   форманта 2
ls_md:
    dd FLP, FLP, FBP, FBP,  FLP, FLP, FLP, FBP,  FBP, FBP, FBP, FBP
    dd FBP, FBP, FBP, FBP,  FLP, FBP, FBP, FBP
howl_b:     dd 430.0, 610.0, 870.0
howl_q:     dd 16.0, 22.0, 28.0
howl_w:     dd 1.0, 0.7, 0.45
wire_m:     dd 1.0, 1.0045, 2.003, 3.0
wire_w:     dd 1.0, 0.8, 0.28, 0.16
hum_f:      dd 100.0, 200.0, 300.0, 400.0
hum_w:      dd 1.0, 0.5, 0.25, 0.12
brk_w:      dd 0.7, 0.9, 0.8, 0.7
; сглаживание параметров за блок (порядок AP_*)
bs_a:       dd 0.35, 0.35, 0.35, 0.05, 0.3, 0.06, 0.2, 0.05
            dd 0.05, 0.05, 0.05, 0.05, 0.05, 0.05, 0.08, 0.05
            dd 0.05, 0.6, 0.1, 0.05, 0.05, 0.05, 0.05, 0.3
            dd 0.05, 0.05, 0.02, 0.2, 0.3, 0.1, 0.1, 0.1

; громкости фонов (подбор по --wav: энергия полос в alevels.bin)
ac_kn:      dd 0.38                     ; шорох в ушах, ФНЧ
ac_knb:     dd 0.25                     ;   полоса
ac_kf:      dd 0.46                     ; гул над селом
ac_kb:      dd 0.9                      ; буханье
ac_kc:      dd 1.0                      ; труба (в порыв)
ac_kh:      dd 3.0                      ; вой
ac_ks:      dd 0.5                      ; ели
ac_kbi:     dd 0.4                      ; берёзы
ac_khs:     dd 0.14                     ; позёмка
ac_kfi:     dd 0.22                     ; печь гудит
ac_kcr:     dd 1.0                      ;   трещит
ac_ktv:     dd 1.0                      ; телевизор
ac_kw:      dd 0.05                     ; провода
ac_khu:     dd 0.008                    ; фонари
ac_kbr:     dd 0.02                     ; сломанный фонарь
ac_master:  dd 0.85

align 16, db 0
ac_abs:     times 4 dd 0x7FFFFFFF
ac_sign:    times 4 dd 0x80000000
ac4_1:      times 4 dd 1.0
ac4_1_5:    times 4 dd 1.5
ac4_m0_5:   times 4 dd -0.5
ac4_2m31:   times 4 dd 4.656612873e-10
ac_fs:      dd 44100.0
ac_inv_fs:  dd 2.2675737e-5
ac_2pi:     dd 6.2831853
ac_pi_fs:   dd 7.1237928e-5
ac_2pi_fs:  dd 1.4247586e-4
ac_m2pi_fs: dd -1.4247586e-4
ac_32_fs:   dd 7.2562358e-4
ac_inv_blk: dd 0.001953125
ac_blk_s:   dd 0.011609977
ac_fcmax:   dd 20000.0
ac_2m24:    dd 5.9604645e-8
ac_2m31:    dd 4.656612873e-10
ac_big:     dd 1.0e30
ac_1em4:    dd 1.0e-4
ac_1em5:    dd 1.0e-5
ac_m32:     dd -32.0
ac_knee:    dd 0.6
ac_32767:   dd 32767.0
ac_i6:      dd 0.16666667
ac_i24:     dd 0.041666668
ac_i120:    dd 0.008333334
ac_i35:     dd 0.028571429
ac_i45:     dd 0.022222222
ac_i60:     dd 0.016666668
ac_phi:     dd 0.618034
ac_side:    dd 1.5556                   ; 2.2 тайла вбок по экрану: (1, −1)·2.2/√2
ac_pop_p:   dd 0.0406                   ; 3.5 щелчка в секунду на блок
ac_0:       dd 0.0
ac_0_01:    dd 0.01
ac_0_012:   dd 0.012
ac_0_02:    dd 0.02
ac_0_03:    dd 0.03
ac_0_039:   dd 0.039
ac_0_05:    dd 0.05
ac_0_06:    dd 0.06
ac_0_08:    dd 0.08
ac_0_09:    dd 0.09
ac_0_1:     dd 0.1
ac_0_12:    dd 0.12
ac_0_15:    dd 0.15
ac_0_18:    dd 0.18
ac_0_2:     dd 0.2
ac_0_22:    dd 0.22
ac_0_25:    dd 0.25
ac_0_28:    dd 0.28
ac_0_3:     dd 0.3
ac_0_35:    dd 0.35
ac_0_36:    dd 0.36
ac_0_4:     dd 0.4
ac_0_45:    dd 0.45
ac_0_5:     dd 0.5
ac_0_55:    dd 0.55
ac_0_6:     dd 0.6
ac_0_65:    dd 0.65
ac_0_7:     dd 0.7
ac_0_7071:  dd 0.70710678
ac_0_74:    dd 0.74
ac_0_75:    dd 0.75
ac_0_8:     dd 0.8
ac_0_84:    dd 0.84
ac_0_85:    dd 0.85
ac_0_9:     dd 0.9
ac_0_94:    dd 0.94
ac_1:       dd 1.0
ac_1_05:    dd 1.05
ac_1_1:     dd 1.1
ac_1_2:     dd 1.2
ac_1_25:    dd 1.25
ac_1_4:     dd 1.4
ac_1_5:     dd 1.5
ac_1_6:     dd 1.6
ac_1_7:     dd 1.7
ac_1_93:    dd 1.93
ac_2:       dd 2.0
ac_2_5:     dd 2.5
ac_3:       dd 3.0
ac_3_5:     dd 3.5
ac_4:       dd 4.0
ac_6:       dd 6.0
ac_7:       dd 7.0
ac_9:       dd 9.0
ac_10:      dd 10.0
ac_11:      dd 11.0
ac_14:      dd 14.0
ac_18:      dd 18.0
ac_25:      dd 25.0
ac_27:      dd 27.0
ac_30:      dd 30.0
ac_36:      dd 36.0
ac_40:      dd 40.0
ac_45:      dd 45.0
ac_55:      dd 55.0
ac_60:      dd 60.0
ac_75:      dd 75.0
ac_81:      dd 81.0
ac_95:      dd 95.0
ac_100:     dd 100.0
ac_150:     dd 150.0
ac_160:     dd 160.0
ac_200:     dd 200.0
ac_216:     dd 216.0
ac_220:     dd 220.0
ac_256:     dd 256.0
ac_260:     dd 260.0
ac_324:     dd 324.0
ac_350:     dd 350.0
ac_380:     dd 380.0
ac_500:     dd 500.0
ac_520:     dd 520.0
ac_576:     dd 576.0
ac_650:     dd 650.0
ac_800:     dd 800.0
ac_900:     dd 900.0
ac_1300:    dd 1300.0
ac_1380:    dd 1380.0
ac_1680:    dd 1680.0
ac_1840:    dd 1840.0
ac_1880:    dd 1880.0
ac_2300:    dd 2300.0
ac_2400:    dd 2400.0
ac_5500:    dd 5500.0
ac_6200:    dd 6200.0
ac_7000:    dd 7000.0
ac_17000:   dd 17000.0
ac_1e6:     dd 1000000.0
ac_i255:    dd 0.003921569
ac_i215:    dd 0.004651163
ac_0_0125:  dd 0.0125
ac_8:       dd 8.0
ac_sqrt5:   dd 2.2360680
ac_i1400:   dd 0.000714286
ac_ln2:     dd 0.69314718
ac_rv_out:  dd 0.4                      ; реверб: линии -> уши
rv_len:     dd 1116, 1277, 1422, 1617, 1787, 1949, 2111, 2293   ; = длинам в rev_run
rv_in:      dd 0.5, -0.5, 0.5, 0.5, -0.5, 0.5, -0.5, -0.5       ; вход в линии: знаки
ac_0_025:   dd 0.025
ac_0_97:    dd 0.97
ac_140:     dd 140.0
ac_sz_p:    dd 0.001                    ; полено шипит: раз в ~12 с на блок
ac_0_72:    dd 0.72
ac_20:      dd 20.0
ac_48:      dd 48.0
ac_1_3:     dd 1.3
ac_4_5:     dd 4.5
ac_5:       dd 5.0
ac_nfd:     dd 5.0                      ; NFD
ac_nfd1:    dd 4.0                      ; NFD − 1
ac_nro:     dd 3.0                      ; NRO
ac_mid:     dd 64.0                     ; середина карты (MAPW/2)
roo_at:     dd 1440.0, 1560.0, 1840.0, 1880.0   ; петухи: 0:00, 2:00, 6:40, 7:20
ac_m1:      dd -1.0
ac_m0_5:    dd -0.5
ac_m0_16:   dd -0.16
ac_m0_35:   dd -0.35
ac_m0_65:   dd -0.65
ac_m0_75:   dd -0.75
ac_m0_8:    dd -0.8
ac_m0_85:   dd -0.85
ac_m0_9:    dd -0.9
ac_m0_93:   dd -0.93

section .data
align 16, db 0
aud_wfx:    dw 1, 2                     ; PCM, стерео
            dd AFS, AFS*4
            dw 4, 16, 0
align 4, db 0
wav_hdr:    db 'RIFF'
            dd 0
            db 'WAVE', 'fmt '
            dd 16
            dw 1, 2
            dd AFS, AFS*4
            dw 4, 16
            db 'data'
            dd 0
s_alogname: dw __utf16__('alog.bin'), 0
s_alevname: dw __utf16__('alevels.bin'), 0
s_sndtname: dw __utf16__('sndtest.wav'), 0
s_atrname:  dw __utf16__('atrace.bin'), 0
s_wavname:  dw __utf16__('shot?.wav'), 0
align 4, db 0
gt_rng:     dd 0x51F15EED
dbg_wav:    dd 0
dbg_sndt:   dd 0
dbg_alog:   dd 0
aud_nblk:   dd 0                        ; alog.bin: блоков, провалов, ошибка waveOutOpen
aud_under:  dd 0
aud_oerr:   dd -1

section .bss
alignb 16
aud_z0:                                 ; обнуляется в aud_reset
voices:     resb NVOICE*VSZ
quads:      resb NQUAD*QSZ
pquads:     resb NPQ*PQSZ
rv_c:       resd RVN*(RVM+1)                ; линии реверба
rv_a:       resd 4*1024
st_lane:    resq NQUAD*4
st_frames:  resd 1
st_peak:    resd 1
st_knee:    resd 1
st_us:      resd 1
st_ev:      resd NSND
ST_BYTES    equ NQUAD*4*8 + 16 + NSND*4
bs:         resd NAP
lt:         resd NQUAD*4*5
LT_FC       equ 0
LT_Q        equ NQUAD*4*4
LT_GL       equ NQUAD*4*8
LT_GR       equ NQUAD*4*12
LT_GV       equ NQUAD*4*16
ptq:        resd 8*3
PT_F        equ 0
PT_GL       equ 32
PT_GR       equ 64
alignb 16
ai_ev:      resb EVSZ
at_t0:      resd 1
at_t1:      resd 1
at_e1:      resd 1
at_pm:      resd 1
at_rng:     resd 1
bt_out:     resd 1
bt_sl:      resd 1
bt_sr:      resd 1
bt_al:      resd 1
bt_ar:      resd 1
bt_fl:      resd 1
bt_fr:      resd 1
bt_af:      resd 1
bt_ff:      resd 1
bt_g:       resd 1
bt_fch:     resd 1
bt_gch:     resd 1
bt_h:       resd 1
bt_pf:      resd 1
bt_fc:      resd 1
bt_q:       resd 1
bt_gv:      resd 1
bf:         resd 1
bf_t:       resd 1
wfl:        resd 1
wfl_t:      resd 1
hw:         resd 4
tv_st:      resd 1
tv_sg:      resd 1
tv_sc:      resd 1
tv_f1:      resd 1
tv_f2:      resd 1
hb_ph:      resd 1
st_fl:      resd 1                      ; печь: трепет пламени, его цель
st_flt:     resd 1
st_cr:      resd 1                      ;   сила треска (вспышки)
mo_g:       resd 1
mo_l:       resd 1
mo_r:       resd 1
rv_f:       resd RVN                    ; ФНЧ глушения линий
rv_g:       resd RVN                    ; затухание за проход
rv_p:       resd 1
ab_out:     resq 1
alignb 16
aud_z1:
mixL:       resd ABLK                   ; mixL, mixR, mixV, acc4 — подряд (чистятся блоком)
mixR:       resd ABLK
mixV:       resd ABLK
acc4:       resb ABLK*48
evq:        resb NEV*EVSZ
gt_ev:      resb EVSZ
ap:         resd NAP
aud_pcm:    resb ANBUF*ABLK*4
aud_hdr:    resb ANBUF*48
alignb 64
ev_head:    resd 16                     ; пишет игра
ev_tail:    resd 16                     ; пишет поток звука
aud_hwo:    resq 1
aud_hev:    resq 1
aud_hthr:   resq 1
wav_n:      resd 1
wav_owe:    resd 1
wav_q0:     resq 1
wav_q1:     resq 1
wav_qs:     resq 1
aud_on:     resb 1
aud_live:   resb 1
aud_wav:    resb 1
aud_quit:   resb 1
aud_mute:   resb 1
gt_nocc:    resb 1
gt_chnear:  resb 1
gt_roo:     resb 1
alignb 4
gt_t0:      resd 1
gt_t1:      resd 1
gt_t2:      resd 1
gt_t3:      resd 1
gt_sx:      resd 1
gt_sy:      resd 1
gt_dx:      resd 1
gt_dy:      resd 1
gt_d:       resd 1
gt_g:       resd 1
gt_fc:      resd 1
gt_p:       resd 1
gt_rs:      resd 1
gt_rp:      resd 1
gt_del:     resd 1
gt_zsteps:  resd 1
gt_bv:      resd 1
gt_wf:      resd 1
gt_spw:     resd 1
gt_spp:     resd 1
gt_biw:     resd 1
gt_bip:     resd 1
gt_ww:      resd 1
gt_wp:      resd 1
gt_fw:      resd 1
gt_fp:      resd 1
gt_tw:      resd 1
gt_tp:      resd 1
gt_tin:     resd 1
gt_dd:      resd 1
gt_dp:      resd 1
gt_bprev:   resd 1
gt_gs:      resd 1
gt_ftr:     resd 1                      ; snd_foot: след уже есть (0..255), высота снега
gt_fdp:     resd 1
gt_seed:    resd 1                      ; для snd_emit: зерно голоса (сбрасывается)
gt_a:       resd 1                      ;   сила/хрипота (сбрасывается)
dog_t:      resd 1
kn_gt:      resd 1
kn_n:       resd 1
NFD         equ 5                       ; собаки по селу
NRO         equ 3                       ; петухи
fd_x:       resd NFD                    ; двор, голос (высота), хрипота — на всю ночь
fd_y:       resd NFD
fd_p:       resd NFD
fd_r:       resd NFD
fd_t:       resd NFD                    ; шаг лая
ro_x:       resd NRO
ro_y:       resd NRO
ro_p:       resd NRO
ro_r:       resd NRO
zvt:        resd MAXE
zsi:        resd MAXE                   ; шаг зомби: номер по пройденному пути
kn_t:       resd MAXKN
zst:        resb MAXE
kn_xy:      resb MAXKN*2
alignb 16
wav_buf:    resd WAVMAX
TRMAX       equ 12000
trbuf:      resd TRMAX*NAP
tr_n:       resd 1

section .text
