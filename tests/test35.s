#
# Expected Result: r2 = 1
#

                addi    r1 = r0, 1;
                cmpeq   p1 = r0, r1;
          (!p1) bc      x;
                addi    r2 = r0, 0;
                nop     0;
                addi    r2 = r2, 1;
x:              addi    r2 = r2, 1;
                halt;
