.global my_strlen
.intel_syntax noprefix

my_strlen:
    vpxor ymm0, ymm0, ymm0       
    
    vpcmpeqb ymm1, ymm0, [rdi]
    vpmovmskb eax, ymm1
    test eax, eax
    jnz .found_first

    vzeroupper
    ret

.found_first:
    bsf eax, eax
    vzeroupper
    ret

