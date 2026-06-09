; api.asm - Системные вызовы ядра
section .text
global api_console_print
global api_user_input
global api_set_cursor_pos
global api_get_cursor_pos

section .data
cursor_x db 0
cursor_y db 0
temp_char db 0, 0

; ПОЛНАЯ карта сканкодов (256 байт для безопасности)
scancode_map:
    ; 0x00-0x0F
    db 0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0x08
    ; 0x10-0x1F
    db 0x09, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0x0D
    ; 0x20-0x2F
    db 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 0x27, '`', 0
    ; 0x30-0x3F
    db '\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 0, 0, ' '
    ; 0x40-0x7F (заполняем нулями)
    times 64 db 0
    ; 0x80-0xFF (игнорируются, так как сканкоды < 0x80)
    times 128 db 0

section .text

; void api_set_cursor_pos(int x, int y)
api_set_cursor_pos:
    push ebp
    mov ebp, esp
    push eax
    push ebx
    
    mov al, [ebp+8]     ; x
    mov [cursor_x], al
    mov al, [ebp+12]    ; y
    mov [cursor_y], al
    
    ; Устанавливаем аппаратный курсор
    mov al, [ebp+12]    ; y
    mov bl, 80
    mul bl
    add al, [ebp+8]     ; + x
    mov bx, ax
    
    mov dx, 0x03D4
    mov al, 0x0F
    out dx, al
    mov dx, 0x03D5
    mov al, bl
    out dx, al
    
    mov dx, 0x03D4
    mov al, 0x0E
    out dx, al
    mov dx, 0x03D5
    mov al, bh
    out dx, al
    
    pop ebx
    pop eax
    pop ebp
    ret

; void api_get_cursor_pos(int* x, int* y)
api_get_cursor_pos:
    push ebp
    mov ebp, esp
    push eax
    push ebx
    
    mov ebx, [ebp+8]    ; указатель на x
    mov al, [cursor_x]
    mov [ebx], al
    
    mov ebx, [ebp+12]   ; указатель на y
    mov al, [cursor_y]
    mov [ebx], al
    
    pop ebx
    pop eax
    pop ebp
    ret

; void api_console_print(const char* str)
api_console_print:
    push ebp
    mov ebp, esp
    pusha
    
    mov esi, [ebp+8]    ; строка
    xor eax, eax
    mov al, [cursor_y]
    mov ebx, 80
    mul ebx
    movzx ebx, byte [cursor_x]
    add eax, ebx
    shl eax, 1          ; *2 (символ + атрибут)
    add eax, 0xB8000
    mov edi, eax        ; edi = позиция в видеопамяти
    
.print_loop:
    lodsb
    test al, al
    jz .print_done
    
    cmp al, 0x0A        ; перевод строки
    je .newline
    
    cmp al, 0x08        ; backspace
    je .print_backspace
    
    ; Выводим символ
    mov [edi], al
    mov byte [edi+1], 0x0F
    add edi, 2
    
    ; Обновляем позицию курсора
    inc byte [cursor_x]
    cmp byte [cursor_x], 80
    jne .print_loop
    
    ; Перенос строки если x >= 80
    mov byte [cursor_x], 0
    inc byte [cursor_y]
    cmp byte [cursor_y], 25
    jne .print_loop
    
    ; Скролл если y >= 25
    call scroll_screen
    mov byte [cursor_y], 24
    jmp .print_loop
    
.newline:
    mov byte [cursor_x], 0
    inc byte [cursor_y]
    cmp byte [cursor_y], 25
    jne .recalc_position
    
    call scroll_screen
    mov byte [cursor_y], 24
    
.recalc_position:
    xor eax, eax
    mov al, [cursor_y]
    mov ebx, 80
    mul ebx
    movzx ebx, byte [cursor_x]
    add eax, ebx
    shl eax, 1
    add eax, 0xB8000
    mov edi, eax
    jmp .print_loop

.print_backspace:
    cmp byte [cursor_x], 0
    jne .do_backspace
    cmp byte [cursor_y], 0
    je .print_loop
    dec byte [cursor_y]
    mov byte [cursor_x], 79
    jmp .recalc_position
    
.do_backspace:
    dec byte [cursor_x]
    sub edi, 2
    mov word [edi], 0x0F20
    jmp .print_loop
    
.print_done:
    ; Обновляем аппаратный курсор
    call update_hardware_cursor
    popa
    pop ebp
    ret

; char* api_user_input(char* buffer, int max_len)
api_user_input:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    
    mov edi, [ebp+8]    ; buffer
    mov ecx, [ebp+12]   ; max_len
    xor ebx, ebx        ; счетчик
    dec ecx             ; место для нуль-терминатора
    
.input_loop:
    ; Ждем нажатия
    in al, 0x64
    test al, 1
    jz .input_loop
    
    in al, 0x60
    test al, 0x80       ; отпускание
    jnz .input_loop
    
    ; Конвертируем сканкод
    and eax, 0x7F
    cmp eax, 128
    jae .input_loop
    
    ; Загружаем символ из карты
    mov esi, scancode_map
    mov al, [esi+eax]
    
    test al, al
    jz .input_loop
    
    cmp al, 0x0D        ; Enter
    je .enter_pressed
    
    cmp al, 0x08        ; Backspace
    je .backspace_pressed
    
    ; Отладочный вывод - покажет код клавиши
    ; push eax
    ; call debug_print_keycode
    ; add esp, 4
    ; pop eax
    
    ; Обычный символ
    cmp ebx, ecx
    jge .input_loop
    
    mov [edi+ebx], al
    inc ebx
    
    ; Выводим символ
    mov [temp_char], al
    mov byte [temp_char+1], 0
    push eax
    push temp_char
    call api_console_print
    add esp, 4
    pop eax
    
    jmp .input_loop
    
.backspace_pressed:
    test ebx, ebx
    jz .input_loop
    
    dec ebx
    mov byte [edi+ebx], 0
    
    ; Выводим backspace
    mov byte [temp_char], 0x08
    mov byte [temp_char+1], 0
    push temp_char
    call api_console_print
    add esp, 4
    
    jmp .input_loop
    
.enter_pressed:
    mov byte [edi+ebx], 0
    
    ; Новая строка
    mov byte [temp_char], 0x0A
    mov byte [temp_char+1], 0
    push temp_char
    call api_console_print
    add esp, 4
    
    mov eax, edi
    
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

; Отладочная функция для вывода кода клавиши
debug_print_keycode:
    push ebp
    mov ebp, esp
    pusha
    
    mov eax, [ebp+8]
    
    ; Конвертируем в HEX
    mov ecx, 8
    mov esi, hex_buffer
.hex_loop:
    rol eax, 4
    mov bl, al
    and bl, 0x0F
    cmp bl, 10
    jl .digit
    add bl, 'A' - 10
    jmp .store
.digit:
    add bl, '0'
.store:
    mov [esi], bl
    inc esi
    loop .hex_loop
    
    mov byte [esi], ':'
    mov byte [esi+1], ' '
    mov byte [esi+2], 0
    
    push hex_buffer
    call api_console_print
    add esp, 4
    
    popa
    pop ebp
    ret

; Вспомогательные функции
scroll_screen:
    pusha
    mov esi, 0xB8000 + 160    ; вторая строка
    mov edi, 0xB8000           ; первая строка
    mov ecx, 80*24             ; 24 строки
    rep movsw
    
    ; Очищаем последнюю строку
    mov edi, 0xB8000 + 80*24*2
    mov ecx, 80
    mov ax, 0x0F20
    rep stosw
    popa
    ret

update_hardware_cursor:
    pusha
    xor eax, eax
    mov al, [cursor_y]
    mov bl, 80
    mul bl
    add al, [cursor_x]
    mov bx, ax
    
    mov dx, 0x03D4
    mov al, 0x0F
    out dx, al
    mov dx, 0x03D5
    mov al, bl
    out dx, al
    
    mov dx, 0x03D4
    mov al, 0x0E
    out dx, al
    mov dx, 0x03D5
    mov al, bh
    out dx, al
    popa
    ret

section .data
hex_buffer: db '00000000: ', 0