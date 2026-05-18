.global HashTableContains
.intel_syntax noprefix

# -----------------------------------------------------------------------------
# int HashTableContains(const HashTable *table [rdi], const char *key_data [rsi])
# -----------------------------------------------------------------------------
HashTableContains:
    mov eax, 0xFFFFFFFF                  # uint64_t crc = 0xFFFFFFFF
    crc32 rax, qword ptr [rsi]           # crc = _mm_crc32_u64(crc, buf[0])
    crc32 rax, qword ptr [rsi + 8]       # crc = _mm_crc32_u64(crc, buf[1])
    crc32 rax, qword ptr [rsi + 16]      # crc = _mm_crc32_u64(crc, buf[2])
    crc32 rax, qword ptr [rsi + 24]      # crc = _mm_crc32_u64(crc, buf[3])

    # ---------------------------------------------------------
    # idx = crc % capacity
    # смещение table->capacity - 16 байт
    # ---------------------------------------------------------
    xor edx, edx                         
    div qword ptr [rdi + 16]             # деление rax на table->capacity, остаток в rdx

    # ---------------------------------------------------------
    # Получение первого индекса узла из buckets
    # Смещение table->buckets - 0 байт
    # ---------------------------------------------------------
    mov rcx, [rdi]                       # rcx = table->buckets
    mov eax, [rcx + rdx * 4]             # eax = current_node_idx (int, 4 байта)

    # ---------------------------------------------------------
    # Перед поиском
    # ---------------------------------------------------------
    mov rcx, [rdi + 8]                   # rcx = table->list
    mov r8,  [rcx]                       # r8  = table->list->nodes
    
    vmovdqu ymm0, [rsi]

.loop:
    cmp eax, -1                          # ? current_node_idx == CF_NULL_INDEX
    je .not_found                        # если -1, то слова нет

    # ---------------------------------------------------------
    # Адрес текущего узла: &list->nodes[current_node_idx]
    # ---------------------------------------------------------
    movsxd r9, eax                       # расширил индекс до 64 бит
    imul r9, r9, 36                      # умножил на размер структуры CFNode (36 байт)
                                         
    lea r10, [r8 + r9]                   # r10 = адрес текущего CFNode

    # ---------------------------------------------------------
    # Сравнение строк через AVX2 (StringEquals)
    # Смещение node->key_data - 0 байт
    # ---------------------------------------------------------
    vpcmpeqb ymm1, ymm0, [r10]           # сравнил искомое (ymm0) с текущим узлом в памяти
    vpmovmskb r11d, ymm1                 # собирал маску совпадений
    cmp r11d, -1                         # проверка на совпадение
    je .found                            # нашли

    # ---------------------------------------------------------
    # Переход к следующему узлу
    # смещение node->next -  32 байта
    # ---------------------------------------------------------
    mov eax, [r10 + 32]                  # current_node_idx = node->next
    jmp .loop                            # цикл

.found:
    mov eax, 1                           # return 1
    vzeroupper                           # очистил верхнюю половину YMM для совместимости
    ret

.not_found:
    xor eax, eax                         # return 0
    vzeroupper
    ret
