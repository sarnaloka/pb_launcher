/* Dades del Lector dins del paquet: el CSS per defecte del CREngine i els diccionaris de partició de mots.
   Els fitxers els prepara tools/cre/build_deps.sh a build/cre-<destí>/data (es compila amb -Wa,-I<aquella carpeta>). */
    .section .rodata
    .balign 4
    .global lec_css
    .hidden lec_css
    .global lec_css_end
    .hidden lec_css_end
lec_css:
    .incbin "epub.css"
lec_css_end:
    .byte 0
    .balign 4
    .global lec_h_Catalan
    .hidden lec_h_Catalan
    .global lec_h_Catalan_end
    .hidden lec_h_Catalan_end
lec_h_Catalan:
    .incbin "hyph/Catalan.pattern"
lec_h_Catalan_end:
    .byte 0
    .balign 4
    .global lec_h_Spanish
    .hidden lec_h_Spanish
    .global lec_h_Spanish_end
    .hidden lec_h_Spanish_end
lec_h_Spanish:
    .incbin "hyph/Spanish.pattern"
lec_h_Spanish_end:
    .byte 0
    .balign 4
    .global lec_h_English_US
    .hidden lec_h_English_US
    .global lec_h_English_US_end
    .hidden lec_h_English_US_end
lec_h_English_US:
    .incbin "hyph/English_US.pattern"
lec_h_English_US_end:
    .byte 0
    .balign 4
    .global lec_h_English_GB
    .hidden lec_h_English_GB
    .global lec_h_English_GB_end
    .hidden lec_h_English_GB_end
lec_h_English_GB:
    .incbin "hyph/English_GB.pattern"
lec_h_English_GB_end:
    .byte 0
    .balign 4
    .global lec_h_French
    .hidden lec_h_French
    .global lec_h_French_end
    .hidden lec_h_French_end
lec_h_French:
    .incbin "hyph/French.pattern"
lec_h_French_end:
    .byte 0
    .balign 4
    .global lec_h_Italian
    .hidden lec_h_Italian
    .global lec_h_Italian_end
    .hidden lec_h_Italian_end
lec_h_Italian:
    .incbin "hyph/Italian.pattern"
lec_h_Italian_end:
    .byte 0
    .balign 4
    .global lec_h_Portuguese
    .hidden lec_h_Portuguese
    .global lec_h_Portuguese_end
    .hidden lec_h_Portuguese_end
lec_h_Portuguese:
    .incbin "hyph/Portuguese.pattern"
lec_h_Portuguese_end:
    .byte 0
    .balign 4
    .global lec_h_Occitan
    .hidden lec_h_Occitan
    .global lec_h_Occitan_end
    .hidden lec_h_Occitan_end
lec_h_Occitan:
    .incbin "hyph/Occitan.pattern"
lec_h_Occitan_end:
    .byte 0
    .balign 4
    .global lec_h_Basque
    .hidden lec_h_Basque
    .global lec_h_Basque_end
    .hidden lec_h_Basque_end
lec_h_Basque:
    .incbin "hyph/Basque.pattern"
lec_h_Basque_end:
    .byte 0
    .balign 4
    .global lec_h_Galician
    .hidden lec_h_Galician
    .global lec_h_Galician_end
    .hidden lec_h_Galician_end
lec_h_Galician:
    .incbin "hyph/Galician.pattern"
lec_h_Galician_end:
    .byte 0
    .section .note.GNU-stack,"",%progbits
