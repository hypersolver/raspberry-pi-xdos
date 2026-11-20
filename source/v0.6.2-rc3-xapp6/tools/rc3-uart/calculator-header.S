.section .xapp_header,"a",%progbits
.balign 4
.global xapp_image_start
xapp_image_start:
    .ascii "XDAP"
    .word 1, 32
    .word xapp_image_end-xapp_image_start
    .word xapp_entry-xapp_image_start
    .word 0, 0, 0
