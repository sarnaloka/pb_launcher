    .section .rodata
    .balign 4
    .global amb_blob
    .global amb_blob_end
amb_blob:
    .incbin "amb.dat"
amb_blob_end:
    .section .note.GNU-stack,"",%progbits
