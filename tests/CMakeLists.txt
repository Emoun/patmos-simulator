# macro to simplify test definition
macro (test_asm num expr)
  ADD_TEST(asm-test-${num} ${CMAKE_BINARY_DIR}/src/paasm ${PROJECT_SOURCE_DIR}/tests/test${num}.s ${CMAKE_CURRENT_BINARY_DIR}/test${num}.bin)
  SET_TESTS_PROPERTIES(asm-test-${num} PROPERTIES PASS_REGULAR_EXPRESSION ${expr})
  SET_PROPERTY(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES test${num}.bin)
endmacro (test_asm)

test_asm(01 "Emitted: 16 words
Errors : 0")

test_asm(02 "Emitted: 16 words
Errors : 0")

test_asm(03 "Emitted: 24 words
Errors : 0")

test_asm(04 "Emitted: 16 words
Errors : 0")

test_asm(05 "Emitted: 32 words
Errors : 0")

test_asm(06 "Emitted: 36 words
Errors : 0")

test_asm(07 "Emitted: 20 words
Errors : 0")

test_asm(08 "Emitted: 32 words
Errors : 0")

test_asm(09 "Emitted: 28 words
Errors : 0")

test_asm(10 "Emitted: 40 words
Errors : 0")

test_asm(11 "Emitted: 56 words
Errors : 0")

test_asm(12 "Emitted: 56 words
Errors : 0")

test_asm(13 "Emitted: 60 words
Errors : 0")

# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
# SIMULATOR TESTS
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

macro (test_sim num expr)
  ADD_TEST(sim-test-${num} ${CMAKE_BINARY_DIR}/src/pasim ${CMAKE_CURRENT_BINARY_DIR}/test${num}.bin -)
  SET_TESTS_PROPERTIES(sim-test-${num} PROPERTIES PASS_REGULAR_EXPRESSION ${expr} DEPENDS asm-test-${num})
endmacro (test_sim)

test_sim(01 "r0 : 00000005   r1 : 0000000a")

test_sim(02 "r0 : 00000005   r1 : 00000005")

test_sim(03 "r0 : 00a00000   r1 : 00000000")

test_sim(04 "r0 : 00000000   r1 : 00000005")

test_sim(05 "PRR: 00000011
 r0 : 00000005   r1 : 00000007   r2 : 0000000c   r3 : 00000000")

test_sim(06 "PRR: 00010011
 r0 : 00000005   r1 : 00000007   r2 : 0000000c")

test_sim(07 "s2 : 00000023   s3 : 00000000")

test_sim(08 "r0 : 00000005   r1 : 00000007   r2 : 00000000   r3 : 00000023   r4 : 00000023")

test_sim(09 "r0 : 00000000   r1 : 0283f180   r2 : fffff180   r3 : fffffff1   r4 : 0000f180   r5 : 000000f1")

test_sim(10 "r0 : 00000000   r1 : 0283f180   r2 : 000000f1   r3 : 0283f180   r4 : f1800000   r5 : 80000000")

test_sim(11 "BASE: 00000000   PC : 00000044.*
 r0 : 00000026")

test_sim(13 "BASE: 00000000   PC : 00000028.*
 r0 : 0000002d   r1 : 00000000   r2 : 0000000c")
