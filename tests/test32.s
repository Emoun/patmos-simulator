#
# Expected Result: r2 = 1
#

                nop     0;
                lwc     r1 = [r0 + 10];
                nop     0;
                brr     r1;
                nop     0;
                nop     0;
                halt;
                nop     0;
                nop     0;
                .word   8;
                add     r2 = r0, 1;
                halt;
                nop     0;
                nop     0;
