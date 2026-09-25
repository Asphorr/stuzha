; ----------------------------------------------------------------------------
;  vegdraw.inc — растение тайла в G-буфер (код полосы, как objects.inc)
; ----------------------------------------------------------------------------
; vd_rs -> картинка варианта (veg.inc), vd_fl — слово vmap (зеркало, сдвиг основания),
; bk_tx/bk_ty; img_sway — качание у макушки (px·16), vd_dith — просвечивать перед
; игроком. У пикселя своя точка в мире: пишется, только если она не дальше от глаза,
; чем то, что уже лежит в G-буфере (на одном луче ближе тот, у кого x + y больше), —
; кроны заходят на соседние тайлы, за избы и друг за друга как надо
draw_veg:
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
    push    rbp
    sub     rsp, 40
    mov     rbx, [vd_rs]
    mov     ecx, [bk_tx]
    mov     edx, [bk_ty]
    call    veg_basei
    movzx   r12d, ax                    ; основание: x, y (1/256 тайла)
    shr     eax, 16
    mov     r13d, eax
    ; основание на экране
    mov     eax, r12d
    sub     eax, [camx]
    mov     ecx, r13d
    sub     ecx, [camy]
    mov     edx, eax
    sub     edx, ecx
    sar     edx, 4
    add     edx, CX0
    add     eax, ecx
    sar     eax, 5
    add     eax, CY0
    ; левый верхний угол картинки (у зеркальной — от другого края)
    mov     ecx, [rbx + RS_AX]
    test    dword [vd_fl], 0x8000
    jz      .nm
    mov     ecx, [rbx + RS_W]
    dec     ecx
    sub     ecx, [rbx + RS_AX]
.nm:
    sub     edx, ecx
    mov     [vd_x0], edx
    sub     eax, [rbx + RS_AY]
    mov     [vd_y0], eax
    ; мимо экрана по x (с запасом на качание)
    mov     ecx, edx
    add     ecx, [rbx + RS_W]
    add     ecx, 16
    js      .out
    cmp     edx, SCR_W + 16
    jg      .out
    ; строки картинки в полосе: от max(0, y0' − y0) до min(h, y1' − y0)
    mov     r14d, [band_y0]
    sub     r14d, eax
    xor     ecx, ecx
    cmp     r14d, ecx
    cmovl   r14d, ecx
    mov     r15d, [band_y1]
    sub     r15d, eax
    mov     ecx, [rbx + RS_H]
    cmp     r15d, ecx
    cmovg   r15d, ecx
    cmp     r14d, r15d
    jge     .out
    mov     [vd_r1], r15d
.r:
    ; сдвиг строки: качание ∝ (высота над основанием)²
    xor     r9d, r9d
    mov     eax, [img_sway]
    test    eax, eax
    jz      .noff
    mov     ecx, [rbx + RS_AY]
    mov     edx, ecx
    sub     edx, r14d
    jle     .noff
    imul    edx, edx
    imul    eax, edx
    imul    ecx, ecx
    cdq
    idiv    ecx
    add     eax, 8
    sar     eax, 4
    mov     r9d, eax
.noff:
    mov     eax, [vd_y0]
    add     eax, r14d
    mov     [vd_y], eax
    imul    r11d, eax, SCR_W
    ; пиксель картинки в начале строки и шаг по ней (у зеркальной — справа налево)
    mov     edi, r14d
    imul    edi, [rbx + RS_W]
    mov     r15d, 1
    test    dword [vd_fl], 0x8000
    jz      .fw
    add     edi, [rbx + RS_W]
    dec     edi
    mov     r15d, -1
.fw:
    add     r9d, [vd_x0]                ; экранный x первого столбца
    xor     esi, esi
.c:
    cmp     esi, [rbx + RS_W]
    jge     .rn
    mov     rax, [rbx + RS_PX]
    mov     r10d, [rax + rdi*4]
    test    r10d, r10d
    jz      .cn
    lea     edx, [r9d + esi]
    cmp     edx, SCR_W
    jae     .cn
    cmp     dword [vd_dith], 0
    je      .nd
    mov     eax, edx
    add     eax, [vd_y]
    test    eax, 1
    jnz     .cn
.nd:
    add     edx, r11d                   ; пиксель кадра
    ; точка в мире
    mov     rax, [rbx + RS_PO]
    mov     eax, [rax + rdi*4]
    movsx   ecx, ax
    sar     eax, 16
    test    r15d, r15d
    jns     .c2
    xchg    eax, ecx
.c2:
    add     ecx, r12d                   ; x
    add     eax, r13d                   ; y
    ; то, что уже есть, ближе к глазу?
    lea     r8, [albedo]
    cmp     dword [r8 + rdx*4], 0
    je      .w
    mov     r8d, [r8 + rdx*4 + (gposb - albedo)]
    movzx   ebp, r8w
    shr     r8d, 16
    add     ebp, r8d
    lea     r8d, [ecx + eax + 8]
    cmp     ebp, r8d
    jg      .cn
.w:
    ; лампа не горит — стекло тёмное
    test    r10d, r10d
    jns     .rime
    cmp     dword [vd_off], 0
    je      .w1
    mov     r10d, 0x01383C44
    jmp     .w1
.rime:
    ; иней на ветках растёт за мороз (rime_t): по краю силуэта (сосед слева, справа
    ; или сверху пуст) — вдвое гуще, в глубине — вчетверо реже; постройки двора — без.
    ; Какие пиксели — по хэшу пикселя картинки (не мерцает)
    cmp     dword [rime_t], 0
    je      .w1
    lea     rbp, [vrs + NVAR*RS_SZ]
    cmp     rbx, rbp
    jae     .w1
    push    rax
    push    rcx
    mov     r8d, [rime_t]
    lea     ecx, [edi + ebx]
    imul    ecx, ecx, 0x2545F491
    mov     eax, ecx
    shr     eax, 15
    xor     ecx, eax
    imul    ecx, ecx, 0x2C1B3C6D
    shr     ecx, 24
    mov     rbp, [rbx + RS_PX]
    cmp     dword [rbp + rdi*4 - 4], 0
    je      .rme
    cmp     dword [rbp + rdi*4 + 4], 0
    je      .rme
    mov     eax, edi
    sub     eax, [rbx + RS_W]
    js      .rme
    cmp     dword [rbp + rax*4], 0
    je      .rme
    mov     eax, r8d
    shr     eax, 2
    jmp     .rmt
.rme:
    lea     eax, [r8d + r8d]
.rmt:
    cmp     ecx, eax
    jae     .rm9
    mov     ebp, r10d
    shr     ebp, 2
    and     ebp, 0x003F3F3F
    add     ebp, 0x00B1B5B7             ; (c + 3·иней)/4, иней ≈ (236, 242, 244)
    and     r10d, 0xFF000000
    or      r10d, ebp
.rm9:
    pop     rcx
    pop     rax
.w1:
    lea     r8, [albedo]
    mov     [r8 + rdx*4], r10d
    shl     eax, 16
    and     ecx, 0xFFFF
    or      eax, ecx
    mov     [r8 + rdx*4 + (gposb - albedo)], eax
    mov     rax, [rbx + RS_PN]
    movzx   eax, byte [rax + rdi]
    test    r15d, r15d
    jns     .c3
    lea     r8, [nq_mir]
    movzx   eax, byte [r8 + rax]
.c3:
    lea     r8, [nbuf]
    mov     [r8 + rdx], al
.cn:
    inc     esi
    add     edi, r15d
    jmp     .c
.rn:
    inc     r14d
    cmp     r14d, [vd_r1]
    jl      .r
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
    ret
