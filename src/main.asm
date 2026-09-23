; ============================================================================
;  СТУЖА — изометрический survival на чистом NASM (Win64)
;  софтверный рендер: G-буфер + отложенное освещение на SSE2
;
;  сборка:  nasm -f win64 main.asm -o main.obj
;           x86_64-w64-mingw32-ld main.obj -o stuzha.exe --subsystem windows
;               -e start -lkernel32 -luser32 -lgdi32 -ldwmapi -lwinmm
;           (или просто sh build.sh)
; ============================================================================
BITS 64
DEFAULT REL

%include "const.inc"

section .text
global start

; ---------------------------------------------------------------- точка входа
start:
    sub     rsp, 0x78
    call    [SetProcessDPIAware]
    xor     ecx, ecx
    call    [GetModuleHandleW]
    mov     [hinst], rax
    lea     rcx, [qpf]
    call    [QueryPerformanceFrequency]
    lea     rcx, [qpc_last]
    call    [QueryPerformanceCounter]
    call    parse_cmdline
    mov     eax, [qpc_last]
    or      eax, 1
    cmp     dword [shotmode], 0
    je      .seed
    mov     eax, 0x2545F491
.seed:
    mov     [rng], eax
    call    init_tables
    call    gen_clouds
    call    gen_gust                    ; шум порывов, полосы позёмки, иней — своим ГСЧ,
    call    gen_streaks                 ; село от них не меняется
    call    gen_frost
    call    gen_sprites
    call    gen_props
    call    init_gdi
    call    new_game
    cmp     dword [dbg_sndt], 0
    je      .nst
    call    aud_sndtest                 ; --sndtest: все звуки событий -> sndtest.wav
    jmp     quit
.nst:
    cmp     dword [shotmode], 0
    jne     run_shot
    call    create_window
    call    audio_init                  ; нет устройства — молча без звука

main_loop:
.pump:
    lea     rcx, [msg]
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    mov     dword [rsp+32], 1           ; PM_REMOVE
    call    [PeekMessageW]
    test    eax, eax
    jz      .frame
    cmp     dword [msg+8], 0x12         ; WM_QUIT
    je      quit
    lea     rcx, [msg]
    call    [TranslateMessage]
    lea     rcx, [msg]
    call    [DispatchMessageW]
    jmp     .pump
.frame:
    lea     rcx, [qpc_now]
    call    [QueryPerformanceCounter]
    mov     rax, [qpc_now]
    mov     rcx, rax
    sub     rax, [qpc_last]
    mov     [qpc_last], rcx
    cvtsi2ss xmm0, rax
    cvtsi2ss xmm1, qword [qpf]
    divss   xmm0, xmm1
    minss   xmm0, [f_0_05]
    movss   [dt], xmm0
    call    read_input
    cmp     byte [k_esc_edge], 0
    je      .noesc
    cmp     dword [fullscr], 0
    je      quit
    call    toggle_fs                   ; из полного экрана Esc сперва возвращает окно
.noesc:
    call    update
    call    render
    ; время кадра (update + render), скользящее среднее
    call    ms_since_frame
    subss   xmm0, [frame_ms]
    mulss   xmm0, [f_0_05]
    addss   xmm0, [frame_ms]
    movss   [frame_ms], xmm0
    call    present
    call    [DwmFlush]
    jmp     main_loop

; -> xmm0 = мс с момента qpc_now
ms_since_frame:
    sub     rsp, 40
    lea     rcx, [qpc_end]
    call    [QueryPerformanceCounter]
    mov     rax, [qpc_end]
    sub     rax, [qpc_now]
    cvtsi2ss xmm0, rax
    mulss   xmm0, [f_1000]
    cvtsi2ss xmm1, qword [qpf]
    divss   xmm0, xmm1
    add     rsp, 40
    ret

quit:
    call    audio_shutdown
    xor     ecx, ecx
    call    [ExitProcess]

; ---------------------------------------------------------------- режим снимка
; --shotN: фиксированный сид, сценарий N, 90 кадров симуляции, дамп shotN.bmp
run_shot:
    mov     dword [wind_t], __float32__(37.0)
    mov     eax, [shotmode]
    cmp     eax, 10
    je      .s10
    cmp     eax, 11
    je      .s11
    cmp     eax, 12
    je      .s12
    cmp     eax, 13
    je      .s13
    cmp     eax, 7
    je      .s7
    cmp     eax, 8
    je      .s8
    cmp     eax, 9
    je      .s9
    cmp     eax, 2
    je      .s2
    cmp     eax, 3
    je      .s3
    cmp     eax, 4
    je      .s4
    cmp     eax, 5
    je      .s5
    cmp     eax, 6
    je      .s6
    ; 1: ночь, фонарь, взгляд вверх-вправо на дома
    mov     dword [gtime], __float32__(1390.0)
    mov     dword [pl_flash], 1
    mov     dword [mx], 470
    mov     dword [my], 110
    jmp     .zomb
.s2:
    ; 2: сумерки, без фонаря
    mov     dword [gtime], __float32__(1052.0)
    mov     dword [mx], 180
    mov     dword [my], 150
    jmp     .zomb
.s3:
    ; 3: ночь, внутри освещённого дома
    mov     dword [gtime], __float32__(1500.0)
    mov     dword [pl_flash], 1
    mov     dword [mx], 200
    mov     dword [my], 250
    call    shot_put_inside
    jmp     .zomb
.s4:
    ; 4: ночь, фонарь; идём вниз по снегу, потом машем лопатой
    mov     dword [gtime], __float32__(1400.0)
    mov     dword [pl_flash], 1
    mov     dword [mx], 430
    mov     dword [my], 250
    jmp     .zomb
.s5:
    ; 5: зомби вплотную спереди, лопата зажата
    mov     dword [gtime], __float32__(1400.0)
    mov     dword [pl_flash], 1
    mov     dword [mx], 430
    mov     dword [my], 250
    lea     rax, [shot_zoff5]
    mov     [shot_ztab], rax
    jmp     .zomb
.s6:
    ; 6: смерть — 1 HP, зомби вплотную
    mov     dword [gtime], __float32__(1400.0)
    mov     dword [pl_flash], 1
    mov     dword [pl_hp], __float32__(1.0)
    lea     rax, [shot_zoff5]
    mov     [shot_ztab], rax
    jmp     .zomb
.s7:
    ; 7: 21:00, луна низко — длинные тени, двор с мелочами, без фонаря
    mov     dword [gtime], __float32__(1260.0)
    mov     dword [mx], 330
    mov     dword [my], 120
    call    shot_put_yard
    jmp     .zomb
.s8:
    ; 8: 23:30, фонарь бьёт в идущих зомби — тени от луча
    mov     dword [gtime], __float32__(1410.0)
    mov     dword [pl_flash], 1
    mov     dword [mx], 520
    mov     dword [my], 170
    lea     rax, [shot_zoff8]
    mov     [shot_ztab], rax
    jmp     .zomb
.s9:
    ; 9: 07:05 — луна садится за спиной, светает
    mov     dword [gtime], __float32__(1865.0)
    mov     dword [mx], 250
    mov     dword [my], 140
    jmp     .zomb
.s10:
    ; 0 (10): 23:10, игрок за избой — окно в крыше, луна на скате
    mov     dword [gtime], __float32__(1390.0)
    mov     dword [pl_flash], 1
    mov     dword [mx], 470
    mov     dword [my], 110
    mov     dword [pl + E_X], __float32__(62.5)
    mov     dword [pl + E_Y], __float32__(47.6)
    mov     dword [cam_xf], __float32__(62.5)
    mov     dword [cam_yf], __float32__(47.6)
    jmp     .zomb
.s11:
    ; w (11): 21:40, ясно и ветрено — позёмка при луне, с коньков пылит, деревья гнёт
    mov     dword [gtime], __float32__(1300.0)
    mov     dword [mx], 470
    mov     dword [my], 150
    jmp     .zomb
.s12:
    ; b (12): 00:40, метель; фонарик, игрок продрог — иней по краям
    mov     dword [gtime], __float32__(1480.0)
    mov     dword [pl_flash], 1
    mov     dword [pl_warm], __float32__(22.0)
    mov     dword [mx], 520
    mov     dword [my], 170
    lea     rax, [shot_zoff8]
    mov     [shot_ztab], rax
    jmp     .zomb
.s13:
    ; z (13): 21:40, ветрено; игрок стоит, зомби по ветру и против — кто учует
    mov     dword [gtime], __float32__(1300.0)
    mov     dword [mx], 470
    mov     dword [my], 150
    lea     rax, [shot_zoffz]
    mov     [shot_ztab], rax
.zomb:
    ; --wav: вплотную зомби — только где они и нужны (бой, смерть, нюх); в остальных
    ; снимках слушаем место, а не драку за 15 с
    cmp     dword [dbg_wav], 0
    je      .plz
    mov     eax, [shotmode]
    cmp     eax, 4
    jb      .noz
    cmp     eax, 6
    jbe     .plz
    cmp     eax, 8
    je      .plz
    cmp     eax, 13
    je      .plz
.noz:
    jmp     .placed
.plz:
    call    shot_place_zombies
.placed:
    mov     dword [elapsed], __float32__(10.0)
    ; прогрев частиц: дым, пар, снег с веток
    mov     ebx, 150
.warm:
    mov     dword [dt], 0x3D088889
    call    update_wind
    call    player_wind
    call    update_flakes
    call    update_env
    call    update_fx
    dec     ebx
    jnz     .warm
    mov     dword [shot_frames], 20
    mov     eax, [shotmode]
    cmp     eax, 13
    jne     .s46
    mov     dword [shot_frames], 600    ; 20 с — успеть учуять и подойти
    jmp     .sim
.s46:
    cmp     eax, 4
    jb      .sim
    cmp     eax, 6
    ja      .sim
    mov     dword [shot_frames], 110
.sim:
    cmp     dword [dbg_soak], 0
    je      .simw
    mov     dword [shot_frames], 3000
    mov     dword [dbg_soak], 0
.simw:
    ; --wav: звук той же симуляцией, не короче 15 с
    cmp     dword [dbg_wav], 0
    je      .sim0
    call    wav_begin
    mov     dword [dbg_wav], 0
    cmp     dword [shot_frames], 450
    jae     .sim0
    mov     dword [shot_frames], 450
.sim0:
    cmp     dword [shotmode], 5
    jne     .nk5
    mov     byte [k_lmb], 1
.nk5:
    cmp     dword [shotmode], 4
    jne     .nokeys
    mov     byte [k_s], 0
    mov     byte [k_lmb], 0
    cmp     dword [shot_frames], 56     ; последний взмах застаём на середине — виден след
    jbe     .atk
    mov     byte [k_s], 1
    jmp     .nokeys
.atk:
    mov     byte [k_lmb], 1
.nokeys:
    mov     dword [dt], 0x3D088889      ; 1/30
    call    update
    call    render                      ; как в живом цикле: зомби смотрят в сетку видимости
    call    wav_pump
    dec     dword [shot_frames]
    jnz     .sim
    ; прогрев, замер 10 рендеров, финальный кадр с цифрой
    call    render
    lea     rdi, [prof_acc]
    xor     eax, eax
    mov     ecx, 16*8
    rep     stosb
    lea     rcx, [qpc_now]
    call    [QueryPerformanceCounter]
    mov     dword [shot_frames], 10
.bench:
    call    render
    dec     dword [shot_frames]
    jnz     .bench
    call    ms_since_frame
    mulss   xmm0, [f_0_1]
    movss   [frame_ms], xmm0
    mov     rax, [qpf]                  ; prof.bin: 8 проходов за 10 кадров + частота QPC
    mov     [prof_acc + 15*8], rax
    lea     rcx, [s_profname]
    mov     edx, 0x40000000
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword [rsp+32], 2
    mov     qword [rsp+40], 0x80
    mov     qword [rsp+48], 0
    call    [CreateFileW]
    mov     rbx, rax
    mov     rcx, rbx
    lea     rdx, [prof_acc]
    mov     r8d, 128
    lea     r9, [written]
    mov     qword [rsp+32], 0
    call    [WriteFile]
    mov     rcx, rbx
    call    [CloseHandle]
    ; проверка нюха: сущности, игрок и ветер -> zlog.bin
    cmp     dword [shotmode], 13
    jne     .nozl
    lea     rcx, [s_zlogname]
    mov     edx, 0x40000000
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword [rsp+32], 2
    mov     qword [rsp+40], 0x80
    mov     qword [rsp+48], 0
    call    [CreateFileW]
    mov     rbx, rax
    mov     rcx, rbx
    lea     rdx, [ents]
    mov     r8d, MAXE*ESZ
    lea     r9, [written]
    mov     qword [rsp+32], 0
    call    [WriteFile]
    mov     rcx, rbx
    lea     rdx, [pl]
    mov     r8d, ESZ
    lea     r9, [written]
    mov     qword [rsp+32], 0
    call    [WriteFile]
    mov     rcx, rbx
    lea     rdx, [wind_dx]
    mov     r8d, 8
    lea     r9, [written]
    mov     qword [rsp+32], 0
    call    [WriteFile]
    mov     rcx, rbx
    call    [CloseHandle]
.nozl:
    call    render
    cmp     dword [dbg_lmap], 0
    je      .svw
    call    dump_lmap
.svw:
    cmp     dword [dbg_wind], 0
    je      .svb
    call    dump_wind
.svb:
    call    save_bmp
    cmp     byte [aud_wav], 0
    je      quit
    mov     ax, [s_shotname + 8]        ; shotN.wav рядом с shotN.bmp
    mov     [s_wavname + 8], ax
    lea     rcx, [s_wavname]
    call    wav_save
    jmp     quit

; игрок — рядом с первой горящей печью
shot_put_inside:
    push    rbx
    sub     rsp, 32
    lea     rbx, [tmap]
    xor     eax, eax
.find:
    cmp     byte [rbx + rax], T_STOVEL
    je      .got
    inc     eax
    cmp     eax, MAPW*MAPW
    jb      .find
    jmp     .out
.got:
    ; ищем соседа-пол
    mov     ecx, eax
    lea     rdx, [nb4]
    xor     r8d, r8d
.nb:
    movsx   r9d, word [rdx + r8*2]
    lea     r10d, [ecx + r9d]
    cmp     byte [rbx + r10], T_WOOD
    je      .place
    inc     r8d
    cmp     r8d, 4
    jb      .nb
    jmp     .out
.place:
    mov     eax, r10d
    and     eax, MAPW-1
    cvtsi2ss xmm0, eax
    addss   xmm0, [f_0_5]
    movss   [pl + E_X], xmm0
    movss   [cam_xf], xmm0
    shr     r10d, 7
    cvtsi2ss xmm0, r10d
    addss   xmm0, [f_0_5]
    movss   [pl + E_Y], xmm0
    movss   [cam_yf], xmm0
.out:
    add     rsp, 32
    pop     rbx
    ret

; игрок во дворе с бельевой верёвкой (или снеговиком), чуть ниже по экрану
shot_put_yard:
    lea     rdx, [tmap]
    xor     eax, eax
.f:
    cmp     byte [rdx + rax], T_LINE
    je      .got
    inc     eax
    cmp     eax, MAPW*MAPW
    jb      .f
    xor     eax, eax
.f2:
    cmp     byte [rdx + rax], T_SNOWMAN
    je      .got
    inc     eax
    cmp     eax, MAPW*MAPW
    jb      .f2
    ret
.got:
    ; ближайшая свободная клетка снега ниже по экрану
    lea     r8, [yard_offs]
.o:
    movsx   ecx, byte [r8]
    cmp     ecx, 99
    je      .none
    movsx   r9d, byte [r8 + 1]
    mov     r10d, eax
    and     r10d, MAPW-1
    add     r10d, ecx                   ; x
    mov     r11d, eax
    shr     r11d, 7
    add     r11d, r9d                   ; y
    cmp     r10d, MAPW
    jae     .on
    cmp     r11d, MAPW
    jae     .on
    mov     ecx, r11d
    shl     ecx, 7
    add     ecx, r10d
    cmp     byte [rdx + rcx], T_SNOW
    je      .put
.on:
    add     r8, 2
    jmp     .o
.none:
    mov     r10d, eax
    and     r10d, MAPW-1
    mov     r11d, eax
    shr     r11d, 7
.put:
    cvtsi2ss xmm0, r10d
    addss   xmm0, [f_0_5]
    movss   [pl + E_X], xmm0
    movss   [cam_xf], xmm0
    cvtsi2ss xmm0, r11d
    addss   xmm0, [f_0_5]
    movss   [pl + E_Y], xmm0
    movss   [cam_yf], xmm0
    ret

; несколько зомби перед игроком — чтобы на снимке были спрайты
shot_place_zombies:
    push    rbx
    push    rsi
    sub     rsp, 40
    lea     rbx, [ents]
    mov     rsi, [shot_ztab]
.lp:
    movss   xmm0, [rsi]
    comiss  xmm0, [f_neg99]
    je      .done
    ; ищем свободный слот
.slot:
    cmp     dword [rbx + E_ST], ST_FREE
    je      .use
    add     rbx, ESZ
    jmp     .slot
.use:
    call    init_zombie_fields
    movss   xmm0, [pl + E_X]
    addss   xmm0, [rsi]
    movss   [rbx + E_X], xmm0
    movss   xmm0, [pl + E_Y]
    addss   xmm0, [rsi+4]
    movss   [rbx + E_Y], xmm0
    mov     eax, [rsi+8]
    mov     [rbx + E_ST], eax
    add     rsi, 12
    add     rbx, ESZ
    jmp     .lp
.done:
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret

save_bmp:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 0x40
    call    [GdiFlush]
    mov     eax, [shotmode]
    add     eax, '0'
    cmp     eax, '9'
    jbe     .nm
    mov     ecx, eax
    mov     eax, '0'                    ; сценарий 10 -> shot0.bmp
    cmp     ecx, '0' + 11
    jne     .nm1
    mov     eax, 'w'
.nm1:
    cmp     ecx, '0' + 12
    jne     .nm2
    mov     eax, 'b'
.nm2:
    cmp     ecx, '0' + 13
    jne     .nm
    mov     eax, 'z'
.nm:
    mov     [s_shotname+8], ax
    lea     rcx, [s_shotname]
    mov     edx, 0x40000000             ; GENERIC_WRITE
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword [rsp+32], 2           ; CREATE_ALWAYS
    mov     qword [rsp+40], 0x80
    mov     qword [rsp+48], 0
    call    [CreateFileW]
    mov     rbx, rax
    mov     rcx, rbx
    lea     rdx, [bmp_hdr]
    mov     r8d, 54
    lea     r9, [written]
    mov     qword [rsp+32], 0
    call    [WriteFile]
    mov     esi, SCR_H-1
.row:
    mov     rdx, [fbbits]
    imul    eax, esi, SCR_W*4
    add     rdx, rax
    mov     rcx, rbx
    mov     r8d, SCR_W*4
    lea     r9, [written]
    mov     qword [rsp+32], 0
    call    [WriteFile]
    dec     esi
    jns     .row
    mov     rcx, rbx
    call    [CloseHandle]
    add     rsp, 0x40
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- командная строка
parse_cmdline:
    sub     rsp, 40
    call    [GetCommandLineW]
.scan:
    movzx   ecx, word [rax]
    test    ecx, ecx
    jz      .done
    cmp     ecx, '-'
    jne     .next
    cmp     word [rax+2], '-'
    jne     .next
    cmp     word [rax+4], 'f'           ; --fs: сразу во весь экран
    jne     .nfs
    cmp     word [rax+6], 's'
    jne     .nfs
    mov     dword [start_fs], 1
    jmp     .next
.nfs:
    cmp     word [rax+4], 'h'           ; --hb: хитбоксы (и в снимках)
    jne     .nhb
    cmp     word [rax+6], 'b'
    jne     .nhb
    mov     dword [dbg_hit], 1
    jmp     .next
.nhb:
    cmp     word [rax+4], 'l'           ; --lm: снимок — запечённый свет огней сверху
    jne     .nlm
    cmp     word [rax+6], 'm'
    jne     .nlm
    mov     dword [dbg_lmap], 1
    jmp     .next
.nlm:
    cmp     word [rax+4], 'w'           ; --wind: снимок — поле ветра сверху
    jne     .nwd
    cmp     word [rax+6], 'i'
    jne     .nwd
    mov     dword [dbg_wind], 1
    jmp     .next
.nwd:
    cmp     word [rax+4], 's'           ; --soak: в снимке 3000 кадров игры (проверка на долгом ходу)
    jne     .nsk
    cmp     word [rax+6], 'o'
    jne     .nsk
    mov     dword [dbg_soak], 1
    jmp     .next
.nsk:
    cmp     word [rax+4], 'w'           ; --wav: в снимке ещё и звук -> shotN.wav
    jne     .nwv
    cmp     word [rax+6], 'a'
    jne     .nwv
    mov     dword [dbg_wav], 1
    jmp     .next
.nwv:
    cmp     word [rax+4], 's'           ; --sndtest: звуки событий подряд -> sndtest.wav
    jne     .nsn
    cmp     word [rax+6], 'n'
    jne     .nsn
    mov     dword [dbg_sndt], 1
    jmp     .next
.nsn:
    cmp     word [rax+4], 'a'           ; --alog: при выходе alog.bin (блоки, провалы)
    jne     .nal
    cmp     word [rax+6], 'l'
    jne     .nal
    mov     dword [dbg_alog], 1
    jmp     .next
.nal:
    cmp     word [rax+4], 's'
    jne     .next
    cmp     word [rax+6], 'h'
    jne     .next
    cmp     word [rax+8], 'o'
    jne     .next
    cmp     word [rax+10], 't'
    jne     .next
    movzx   ecx, word [rax+12]
    cmp     ecx, '0'
    jne     .nw
    mov     ecx, 9                      ; --shot0 = сценарий 10
    jmp     .dig
.nw:
    cmp     ecx, 'w'
    jne     .nb
    mov     ecx, 10                     ; --shotw = 11: ветер, позёмка при луне
    jmp     .dig
.nb:
    cmp     ecx, 'b'
    jne     .nz
    mov     ecx, 11                     ; --shotb = 12: метель
    jmp     .dig
.nz:
    cmp     ecx, 'z'
    jne     .n0
    mov     ecx, 12                     ; --shotz = 13: проверка нюха (+ zlog.bin)
    jmp     .dig
.n0:
    sub     ecx, '1'
    cmp     ecx, 8
    jbe     .dig
    xor     ecx, ecx
.dig:
    inc     ecx
    mov     [shotmode], ecx
.next:
    add     rax, 2
    jmp     .scan
.done:
    add     rsp, 40
    ret

; ---------------------------------------------------------------- GDI
init_gdi:
    sub     rsp, 0x78
    xor     ecx, ecx
    call    [CreateCompatibleDC]
    mov     [memdc], rax
    mov     rcx, rax
    lea     rdx, [bmi]
    xor     r8d, r8d
    lea     r9, [fbbits]
    mov     qword [rsp+32], 0
    mov     qword [rsp+40], 0
    call    [CreateDIBSection]
    mov     rcx, [memdc]
    mov     rdx, rax
    call    [SelectObject]
    mov     rcx, [memdc]
    mov     edx, 1                      ; TRANSPARENT
    call    [SetBkMode]
    mov     ecx, -12
    mov     edx, 700
    lea     r8, [s_face_mono]
    call    make_font
    mov     [font_s], rax
    mov     ecx, -15
    mov     edx, 700
    lea     r8, [s_face_serif]
    call    make_font
    mov     [font_m], rax
    mov     ecx, -34
    mov     edx, 700
    lea     r8, [s_face_serif]
    call    make_font
    mov     [font_b], rax
    add     rsp, 0x78
    ret

; ecx = высота, edx = жирность, r8 = имя шрифта -> rax
make_font:
    sub     rsp, 0x78
    mov     [rsp+104], r8
    mov     [rsp+32], rdx
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword [rsp+40], 0
    mov     qword [rsp+48], 0
    mov     qword [rsp+56], 0
    mov     qword [rsp+64], 1           ; DEFAULT_CHARSET
    mov     qword [rsp+72], 0
    mov     qword [rsp+80], 0
    mov     qword [rsp+88], 3           ; NONANTIALIASED_QUALITY
    mov     qword [rsp+96], 0
    call    [CreateFontW]
    add     rsp, 0x78
    ret

; ---------------------------------------------------------------- окно
WSTYLE equ 0x00CF0000                   ; OVERLAPPEDWINDOW: тянется и разворачивается

create_window:
    push    rbx
    sub     rsp, 0x60
    xor     ecx, ecx
    mov     edx, 32515                  ; IDC_CROSS
    call    [LoadCursorW]
    mov     [wc+40], rax
    mov     rax, [hinst]
    mov     [wc+24], rax
    lea     rax, [WndProc]
    mov     [wc+8], rax
    lea     rax, [s_class]
    mov     [wc+64], rax
    lea     rcx, [wc]
    call    [RegisterClassExW]
    lea     rcx, [wrect]
    mov     edx, WSTYLE
    xor     r8d, r8d
    call    [AdjustWindowRect]
    mov     eax, [wrect+8]
    sub     eax, [wrect]
    mov     ebx, [wrect+12]
    sub     ebx, [wrect+4]
    xor     ecx, ecx
    lea     rdx, [s_class]
    lea     r8, [s_wintitle]
    mov     r9d, WSTYLE | 0x10000000    ; + WS_VISIBLE
    mov     dword [rsp+32], 0x80000000  ; CW_USEDEFAULT
    mov     dword [rsp+40], 0x80000000
    mov     [rsp+48], rax
    mov     [rsp+56], rbx
    mov     qword [rsp+64], 0
    mov     qword [rsp+72], 0
    mov     rax, [hinst]
    mov     [rsp+80], rax
    mov     qword [rsp+88], 0
    call    [CreateWindowExW]
    mov     [hwnd], rax
    mov     rcx, rax
    call    [GetDC]
    mov     [hdc], rax
    mov     rcx, rax
    mov     edx, 3                      ; COLORONCOLOR
    call    [SetStretchBltMode]
    cmp     dword [start_fs], 0
    je      .win
    call    toggle_fs
.win:
    add     rsp, 0x60
    pop     rbx
    ret

WndProc:
    cmp     edx, 2                      ; WM_DESTROY
    je      .destroy
    cmp     edx, 5                      ; WM_SIZE
    je      .size
    cmp     edx, 0x100                  ; WM_KEYDOWN
    je      .key
    cmp     edx, 0x104                  ; WM_SYSKEYDOWN
    je      .syskey
    cmp     edx, 0x106                  ; WM_SYSCHAR
    je      .syschar
.def:
    jmp     [DefWindowProcW]
.destroy:
    sub     rsp, 40
    xor     ecx, ecx
    call    [PostQuitMessage]
    add     rsp, 40
    xor     eax, eax
    ret
.size:
    cmp     r8d, 1                      ; SIZE_MINIMIZED — кадр не трогаем
    je      .zero
    movzx   eax, r9w
    mov     [cli_w], eax
    mov     eax, r9d
    shr     eax, 16
    mov     [cli_h], eax
    call    calc_view
.zero:
    xor     eax, eax
    ret
.key:
    cmp     r8d, 0x72                   ; F3 — показать хитбоксы
    je      .hitbox
    cmp     r8d, 0x7A                   ; F11
    jne     .def
    jmp     .toggle
.hitbox:
    bt      r9d, 30
    jc      .zero
    xor     dword [dbg_hit], 1
    xor     eax, eax
    ret
.syskey:
    cmp     r8d, 0x0D                   ; Alt+Enter
    jne     .def
.toggle:
    bt      r9d, 30                     ; автоповтор зажатой клавиши — не мигаем
    jc      .zero
    sub     rsp, 40
    call    toggle_fs
    add     rsp, 40
    xor     eax, eax
    ret
.syschar:
    cmp     r8d, 0x0D                   ; Alt+Enter без системного писка
    jne     .def
    xor     eax, eax
    ret

; окно <-> весь экран: без рамки поверх монитора, где стоит окно (как в играх
; «borderless»); прежнее место и размер окна запоминаются и возвращаются
toggle_fs:
    push    rbx
    sub     rsp, 0x40
    cmp     dword [fullscr], 0
    jne     .back
    mov     rcx, [hwnd]
    lea     rdx, [wplace]
    call    [GetWindowPlacement]
    mov     rcx, [hwnd]
    mov     edx, 2                      ; MONITOR_DEFAULTTONEAREST
    call    [MonitorFromWindow]
    mov     rcx, rax
    lea     rdx, [minfo]
    call    [GetMonitorInfoW]
    mov     dword [fullscr], 1
    mov     rcx, [hwnd]
    mov     edx, -16                    ; GWL_STYLE
    mov     r8d, 0x90000000             ; WS_POPUP | WS_VISIBLE
    call    [SetWindowLongPtrW]
    mov     rcx, [hwnd]
    xor     edx, edx                    ; HWND_TOP
    mov     r8d, [minfo + 4]            ; rcMonitor
    mov     r9d, [minfo + 8]
    mov     eax, [minfo + 12]
    sub     eax, r8d
    mov     [rsp + 32], rax
    mov     eax, [minfo + 16]
    sub     eax, r9d
    mov     [rsp + 40], rax
    mov     qword [rsp + 48], 0x0220    ; SWP_NOOWNERZORDER | SWP_FRAMECHANGED
    call    [SetWindowPos]
    jmp     .out
.back:
    mov     dword [fullscr], 0
    mov     rcx, [hwnd]
    mov     edx, -16
    mov     r8d, WSTYLE | 0x10000000
    call    [SetWindowLongPtrW]
    mov     rcx, [hwnd]
    lea     rdx, [wplace]
    call    [SetWindowPlacement]
    mov     rcx, [hwnd]
    xor     edx, edx
    xor     r8d, r8d
    xor     r9d, r9d
    mov     qword [rsp + 32], 0
    mov     qword [rsp + 40], 0
    mov     qword [rsp + 48], 0x0227    ; + NOSIZE | NOMOVE | NOZORDER
    call    [SetWindowPos]
.out:
    add     rsp, 0x40
    pop     rbx
    ret

; cli_w, cli_h -> vw_*: кадр 16:9 вписан по центру (1920x1080 — ровно x3)
calc_view:
    mov     eax, [cli_w]
    mov     ecx, [cli_h]
    imul    r8d, eax, 9
    imul    r9d, ecx, 16
    cmp     r8d, r9d
    jg      .hlim                       ; шире 16:9 — упираемся в высоту
    mov     r8d, eax
    imul    eax, eax, 9
    shr     eax, 4
    mov     r9d, eax
    jmp     .set
.hlim:
    mov     r9d, ecx
    mov     eax, ecx
    shl     eax, 4
    xor     edx, edx
    mov     r10d, 9
    div     r10d
    mov     r8d, eax
.set:
    mov     eax, 1
    cmp     r8d, eax
    cmovl   r8d, eax
    cmp     r9d, eax
    cmovl   r9d, eax
    mov     [vw_w], r8d
    mov     [vw_h], r9d
    mov     eax, [cli_w]
    sub     eax, r8d
    sar     eax, 1
    mov     [vw_x], eax
    mov     eax, [cli_h]
    sub     eax, r9d
    sar     eax, 1
    mov     [vw_y], eax
    ret

; ecx = x, edx = y, r8d = w, r9d = h — чёрная полоса поля (пустые пропускает)
black_bar:
    test    r8d, r8d
    jle     .no
    test    r9d, r9d
    jle     .no
    sub     rsp, 56
    mov     [rsp + 32], r9
    mov     qword [rsp + 40], 0x42      ; BLACKNESS
    mov     r9d, r8d
    mov     r8d, edx
    mov     edx, ecx
    mov     rcx, [hdc]
    call    [PatBlt]
    add     rsp, 56
.no:
    ret

present:
    sub     rsp, 0x58
    ; поля вокруг кадра (окно тянется, экран не 16:9)
    xor     ecx, ecx
    xor     edx, edx
    mov     r8d, [vw_x]
    mov     r9d, [cli_h]
    call    black_bar
    mov     ecx, [vw_x]
    add     ecx, [vw_w]
    xor     edx, edx
    mov     r8d, [cli_w]
    sub     r8d, ecx
    mov     r9d, [cli_h]
    call    black_bar
    xor     ecx, ecx
    xor     edx, edx
    mov     r8d, [cli_w]
    mov     r9d, [vw_y]
    call    black_bar
    xor     ecx, ecx
    mov     edx, [vw_y]
    add     edx, [vw_h]
    mov     r8d, [cli_w]
    mov     r9d, [cli_h]
    sub     r9d, edx
    call    black_bar
    mov     rcx, [hdc]
    mov     edx, [vw_x]
    mov     r8d, [vw_y]
    mov     r9d, [vw_w]
    mov     eax, [vw_h]
    mov     [rsp+32], rax
    mov     rax, [memdc]
    mov     [rsp+40], rax
    mov     qword [rsp+48], 0
    mov     qword [rsp+56], 0
    mov     qword [rsp+64], SCR_W
    mov     qword [rsp+72], SCR_H
    mov     qword [rsp+80], 0x00CC0020  ; SRCCOPY
    call    [StretchBlt]
    add     rsp, 0x58
    ret

; ---------------------------------------------------------------- ввод
read_input:
    push    rbx
    push    rsi
    sub     rsp, 40
    call    [GetForegroundWindow]
    cmp     rax, [hwnd]
    jne     .nofocus
    lea     rsi, [vk_list]
    lea     rbx, [keys]
.k:
    movzx   ecx, byte [rsi]
    test    ecx, ecx
    jz      .mouse
    call    [GetAsyncKeyState]
    movzx   eax, ax
    shr     eax, 15
    mov     [rbx], al
    inc     rsi
    inc     rbx
    jmp     .k
.mouse:
    lea     rcx, [pt]
    call    [GetCursorPos]
    mov     rcx, [hwnd]
    lea     rdx, [pt]
    call    [ScreenToClient]
    ; окно -> кадр 640x360 (с учётом полей и масштаба)
    mov     eax, [pt]
    sub     eax, [vw_x]
    imul    eax, eax, SCR_W
    cdq
    idiv    dword [vw_w]
    mov     [mx], eax
    mov     eax, [pt+4]
    sub     eax, [vw_y]
    imul    eax, eax, SCR_H
    cdq
    idiv    dword [vw_h]
    mov     [my], eax
    jmp     .edges
.nofocus:
    xor     eax, eax
    mov     [keys], rax
    mov     [keys+8], ax
    mov     [keys+10], al
.edges:
    mov     al, [k_m]
    mov     cl, [k_m_prev]
    mov     [k_m_prev], al
    not     cl
    and     al, cl
    mov     [k_m_edge], al
    mov     al, [k_f]
    mov     cl, [k_f_prev]
    mov     [k_f_prev], al
    not     cl
    and     al, cl
    mov     [k_f_edge], al
    mov     al, [k_r]
    mov     cl, [k_r_prev]
    mov     [k_r_prev], al
    not     cl
    and     al, cl
    mov     [k_r_edge], al
    mov     al, [k_esc]
    mov     cl, [k_esc_prev]
    mov     [k_esc_prev], al
    not     cl
    and     al, cl
    mov     [k_esc_edge], al
    add     rsp, 40
    pop     rsi
    pop     rbx
    ret

; ---------------------------------------------------------------- текст
; ecx = x, edx = y, r8 = строка UTF-16, r9d = COLORREF (0x00BBGGRR)
text_out:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    sub     rsp, 48
    mov     ebx, ecx
    mov     esi, edx
    mov     rdi, r8
    mov     r12d, r9d
    xor     eax, eax
.len:
    cmp     word [rdi + rax*2], 0
    je      .gl
    inc     eax
    jmp     .len
.gl:
    mov     r13d, eax
    mov     rcx, [memdc]
    mov     edx, 0x00100808
    call    [SetTextColor]
    mov     rcx, [memdc]
    lea     edx, [rbx+1]
    lea     r8d, [rsi+1]
    mov     r9, rdi
    mov     [rsp+32], r13
    call    [TextOutW]
    mov     rcx, [memdc]
    mov     edx, r12d
    call    [SetTextColor]
    mov     rcx, [memdc]
    mov     edx, ebx
    mov     r8d, esi
    mov     r9, rdi
    mov     [rsp+32], r13
    call    [TextOutW]
    add     rsp, 48
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret

; rcx = шрифт, edx = выравнивание (0 = TA_LEFT, 6 = TA_CENTER)
set_font:
    push    rbx
    sub     rsp, 32
    mov     ebx, edx
    mov     rdx, rcx
    mov     rcx, [memdc]
    call    [SelectObject]
    mov     rcx, [memdc]
    mov     edx, ebx
    call    [SetTextAlign]
    add     rsp, 32
    pop     rbx
    ret

; rdi = буфер, eax = число -> rdi продвинут
wput_num:
    push    rbx
    mov     ebx, 10
    xor     ecx, ecx
.d:
    xor     edx, edx
    div     ebx
    add     edx, '0'
    push    rdx
    inc     ecx
    test    eax, eax
    jnz     .d
.o:
    pop     rdx
    mov     [rdi], dx
    add     rdi, 2
    dec     ecx
    jnz     .o
    mov     word [rdi], 0
    pop     rbx
    ret

; rdi = буфер, rsi = строка -> rdi продвинут (на терминатор)
wput_str:
.c:
    mov     ax, [rsi]
    mov     [rdi], ax
    test    ax, ax
    jz      .e
    add     rsi, 2
    add     rdi, 2
    jmp     .c
.e:
    ret

; rdi = буфер, gtime -> "ЧЧ:ММ"
wput_clock:
    cvttss2si eax, [gtime]
    xor     edx, edx
    mov     ecx, 1440
    div     ecx
    mov     eax, edx
    xor     edx, edx
    mov     ecx, 60
    div     ecx                         ; eax = часы, edx = минуты
    push    rdx
    xor     edx, edx
    mov     ecx, 10
    div     ecx
    add     eax, '0'
    add     edx, '0'
    mov     [rdi], ax
    mov     [rdi+2], dx
    mov     word [rdi+4], ':'
    pop     rax
    xor     edx, edx
    div     ecx
    add     eax, '0'
    add     edx, '0'
    mov     [rdi+6], ax
    mov     [rdi+8], dx
    mov     word [rdi+10], 0
    add     rdi, 10
    ret

draw_text:
    push    rbx
    push    rsi
    push    rdi
    sub     rsp, 32
    ; --- левый верх: название и часы
    mov     rcx, [font_m]
    xor     edx, edx
    call    set_font
    mov     ecx, 10
    mov     edx, 6
    lea     r8, [s_place]
    mov     r9d, 0x00C8D8E8
    call    text_out
    mov     rcx, [font_s]
    xor     edx, edx
    call    set_font
    lea     rdi, [tbuf]
    call    wput_clock
    lea     rsi, [s_night]
    call    wput_str
    mov     ecx, 11
    mov     edx, 25
    lea     r8, [tbuf]
    mov     r9d, 0x00A0B0C0
    call    text_out
    ; --- правый верх: упокоено
    lea     rdi, [tbuf]
    lea     rsi, [s_kills]
    call    wput_str
    mov     eax, [kills]
    call    wput_num
    mov     ecx, 500
    mov     edx, 8
    lea     r8, [tbuf]
    mov     r9d, 0x00B0C0D0
    call    text_out
    ; время кадра: "кадр 4.2 мс"
    lea     rdi, [tbuf]
    lea     rsi, [s_frame]
    call    wput_str
    movss   xmm0, [frame_ms]
    mulss   xmm0, [f_10]
    cvtss2si eax, xmm0
    xor     edx, edx
    mov     ecx, 10
    div     ecx
    mov     [tmp_frac], edx
    call    wput_num
    mov     word [rdi], '.'
    add     rdi, 2
    mov     eax, [tmp_frac]
    call    wput_num
    lea     rsi, [s_ms]
    call    wput_str
    mov     ecx, 500
    mov     edx, 22
    lea     r8, [tbuf]
    mov     r9d, 0x00708090
    call    text_out
    ; --- ветер под часами: «ветер Ю 7 м/с · метель» (цифра — у игрока, с порывами)
    call    wind_text
    mov     ecx, 27
    mov     edx, 39
    lea     r8, [tbuf]
    mov     r9d, 0x00B0B8C0
    call    text_out
    ; --- подписи полосок
    mov     ecx, 116
    mov     edx, 301
    lea     r8, [s_hp]
    mov     r9d, 0x008080D0
    call    text_out
    lea     r8, [s_warm]
    mov     r9d, 0x0060A0E0
    movss   xmm0, [pl_warm]
    comiss  xmm0, [f_20]
    jae     .wok
    lea     r8, [s_freeze]
    mov     r9d, 0x00F0D8B0
.wok:
    mov     ecx, 116
    mov     edx, 313
    call    text_out
    mov     ecx, 116
    mov     edx, 325
    lea     r8, [s_sta]
    mov     r9d, 0x0060C0D8
    call    text_out
    lea     r8, [s_bat]
    mov     r9d, 0x00E0B070
    movss   xmm0, [pl_bat]
    comiss  xmm0, [f_0]
    ja      .batok
    lea     r8, [s_batdead]
    mov     r9d, 0x004040E0
.batok:
    mov     ecx, 116
    mov     edx, 337
    call    text_out
    ; --- оверлеи
    cmp     dword [gstate], 1
    je      .dead
    cmp     dword [gstate], 2
    je      .won
    movss   xmm0, [elapsed]
    comiss  xmm0, [f_7]
    ja      .out
    mov     rcx, [font_b]
    mov     edx, 6
    call    set_font
    mov     ecx, 320
    mov     edx, 96
    lea     r8, [s_title]
    mov     r9d, 0x00E8F0F8
    call    text_out
    mov     rcx, [font_m]
    mov     edx, 6
    call    set_font
    mov     ecx, 320
    mov     edx, 140
    lea     r8, [s_sub]
    mov     r9d, 0x00C0D0E0
    call    text_out
    mov     rcx, [font_s]
    mov     edx, 6
    call    set_font
    mov     ecx, 320
    mov     edx, 164
    lea     r8, [s_ctrl]
    mov     r9d, 0x0090A0B0
    call    text_out
    jmp     .out
.dead:
    mov     rcx, [font_b]
    mov     edx, 6
    call    set_font
    mov     ecx, 320
    mov     edx, 110
    lea     r8, [s_dead]
    mov     r9d, 0x003030D0
    call    text_out
    lea     rsi, [s_lasted]
    jmp     .stats
.won:
    mov     rcx, [font_b]
    mov     edx, 6
    call    set_font
    mov     ecx, 320
    mov     edx, 110
    lea     r8, [s_dawn]
    mov     r9d, 0x0080D0F8
    call    text_out
    lea     rsi, [s_survived]
.stats:
    lea     rdi, [tbuf]
    call    wput_str
    call    wput_clock
    lea     rsi, [s_sep_kills]
    call    wput_str
    mov     eax, [kills]
    call    wput_num
    mov     rcx, [font_m]
    mov     edx, 6
    call    set_font
    mov     ecx, 320
    mov     edx, 156
    lea     r8, [tbuf]
    mov     r9d, 0x00C0D0E0
    call    text_out
    mov     rcx, [font_s]
    mov     edx, 6
    call    set_font
    mov     ecx, 320
    mov     edx, 182
    lea     r8, [s_again]
    mov     r9d, 0x0090A0B0
    call    text_out
.out:
    add     rsp, 32
    pop     rdi
    pop     rsi
    pop     rbx
    ret

%include "world.inc"
%include "props.inc"
%include "game.inc"
%include "sky.inc"
%include "wind.inc"
%include "shadow.inc"
%include "fx.inc"
%include "render.inc"
%include "coll.inc"
%include "roof.inc"
%include "snow.inc"
%include "post.inc"
%include "audio.inc"
%include "data.inc"
