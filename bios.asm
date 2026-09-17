_start:
call main
hlt

; void putc(c: color);
putc:
    loda 0x1000
    lodb 0x2000
    outc 0x20
    inca
    stra 0x1000

    pushb
    setb 51
    sbaab
    popb
    jaz wrap
    ret

wrap:
    seta 0
    stra 0x1000
    incb
    strb 0x2000
    ret

main:
    out 0x20 0x81
    strimmimm 0x1000 0x00 ; CURSOR X
    strimmimm 0x2000 0x00 ; CUSROR Y

    setc 0xFF
    call putc

    ret
