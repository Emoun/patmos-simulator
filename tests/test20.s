#
# Expected Result: r1 = 0x0281fa81 & r2 = 0x8281fa82
#
                .word   28;
                dlwm    sm  = [r31 + 1];
                dlwm    sm  = [r31 + 2] ||      mfs    r1  = s1;
                waitm                   ||      mfs    r2  = s1;
                halt;
