; ----------------------------------------------------------------------------
;  pool.inc — пул потоков для проходов рендера, где строки (или клетки) независимы
; ----------------------------------------------------------------------------
; par_for: rcx = функция, edx = число элементов, r8d = порция. Функция зовётся
; как fn(ecx = начало, edx = конец) с разных потоков; порции не пересекаются, так
; что проход, где элементы независимы, даёт тот же кадр, что и один поток. Главный
; поток работает наравне с остальными и возвращается, когда сделаны все порции.
;
; Порции раздаёт pl_ticket: старшие 32 бита — поколение задания, младшие — первый
; ещё не взятый элемент. Берут cmpxchg по всему слову: поток, проснувшийся к уже
; закрытому заданию, не отнимет элемент у следующего. Между заданиями рабочие
; недолго крутятся на pause (кадр зовёт задания пачкой), потом спят в WaitOnAddress.
POOL_MAX  equ 16
POOL_SPIN equ 2000                      ; pause перед сном, ~70 мкс

section .text

; --threads N или все логические ядра, не больше POOL_MAX
pool_init:
    push    rbx
    sub     rsp, 48
    mov     eax, [pl_want]
    test    eax, eax
    jnz     .n
    mov     ecx, 0xFFFF                 ; ALL_PROCESSOR_GROUPS
    call    [GetActiveProcessorCount]
.n:
    mov     ecx, POOL_MAX
    cmp     eax, ecx
    cmova   eax, ecx
    mov     ecx, 1
    cmp     eax, ecx
    cmovb   eax, ecx
    mov     [pl_n], eax
    lea     ebx, [rax - 1]
    test    ebx, ebx
    jz      .out
.cr:
    xor     ecx, ecx
    xor     edx, edx
    lea     r8, [pool_worker]
    xor     r9d, r9d
    mov     qword [rsp+32], 0
    mov     qword [rsp+40], 0
    call    [CreateThread]
    dec     ebx
    jnz     .cr
.out:
    add     rsp, 48
    pop     rbx
    ret

; поток пула: ждёт нового поколения, разбирает его порции
pool_worker:
    push    rbx
    push    rsi
    sub     rsp, 40
    xor     esi, esi                    ; своё поколение (первое задание — 1)
.wait:
    mov     ebx, POOL_SPIN
.spin:
    mov     eax, [pl_ticket + 4]
    cmp     eax, esi
    jne     .go
    pause
    dec     ebx
    jnz     .spin
    mov     [rsp+32], esi               ; спим, пока поколение то же
    lea     rcx, [pl_ticket + 4]
    lea     rdx, [rsp+32]
    mov     r8d, 4
    mov     r9d, -1                     ; INFINITE
    call    [WaitOnAddress]
    jmp     .wait
.go:
    mov     esi, eax
    call    pool_grab
    jmp     .wait

; esi = поколение: брать порции, пока оно не закрыто
pool_grab:
    push    rbx
    push    r12
    push    r13
    sub     rsp, 32
.grab:
    mov     rax, [pl_ticket]
.retry:
    mov     rdx, rax
    shr     rdx, 32
    cmp     edx, esi
    jne     .out
    mov     r12d, eax                   ; начало порции
    cmp     r12d, [pl_count]
    jae     .out
    mov     ecx, [pl_chunk]
    lea     rbx, [rax + rcx]
    lock cmpxchg [pl_ticket], rbx
    jne     .retry                      ; rax — новое значение
    lea     r13d, [r12 + rcx]
    mov     ecx, [pl_count]
    cmp     r13d, ecx
    cmova   r13d, ecx
    mov     ecx, r12d
    mov     edx, r13d
    call    [pl_func]
    sub     r13d, r12d
    lock add [pl_fin], r13d
    jmp     .grab
.out:
    add     rsp, 32
    pop     r13
    pop     r12
    pop     rbx
    ret

; rcx = функция, edx = число элементов, r8d = порция
par_for:
    push    rsi
    sub     rsp, 32
    cmp     dword [pl_n], 1
    ja      .pool
    mov     rax, rcx                    ; один поток — всё сразу
    xor     ecx, ecx
    call    rax
    jmp     .out
.pool:
    mov     [pl_func], rcx
    mov     [pl_count], edx
    mov     [pl_chunk], r8d
    mov     dword [pl_fin], 0
    mov     esi, [pl_gen]
    inc     esi
    mov     [pl_gen], esi
    mov     rax, rsi
    shl     rax, 32
    xchg    [pl_ticket], rax            ; задание видно всем (xchg — полный барьер)
    lea     rcx, [pl_ticket + 4]
    call    [WakeByAddressAll]
    call    pool_grab
.wt:
    mov     eax, [pl_fin]
    cmp     eax, [pl_count]
    jae     .out
    pause
    jmp     .wt
.out:
    add     rsp, 32
    pop     rsi
    ret

section .data
align 64, db 0
pl_ticket:  dq 0                        ; поколение << 32 | следующий элемент
align 64, db 0
pl_fin:     dd 0                        ; сделано элементов в текущем задании
align 64, db 0
pl_func:    dq 0
pl_count:   dd 0
pl_chunk:   dd 0
pl_gen:     dd 0
pl_n:       dd 1                        ; потоков вместе с главным
pl_want:    dd 0                        ; --threads N

section .text
