#include <ruby.h>

// this only for idhash_distance_bdigit
#include <bignum.c>

// ```
// $ rake compare_speed
//
// measure the distance (32*32*2000 times):
//
//                                        user     system      total        real
// hamming                            0.949597   0.000000   0.949597 (  0.949625)
// distance                           1.398479   0.000000   1.398479 (  1.398506)
// distance3_bdigit                   0.198673   0.000000   0.198673 (  0.198672)
// distance3_pack_bit_logic           0.868502   0.000000   0.868502 (  0.868512)
// distance3_pack_algo                0.373779   0.000000   0.373779 (  0.373777)
// distance3_popcount_c               0.961717   0.000000   0.961717 (  0.961730)
// distance3_popcount_twiddle         0.859967   0.000000   0.859967 (  0.859983)
// distance3_ruby                     1.824285   0.000000   1.824285 (  1.824315)
// distance 4                         4.441611   0.000000   4.441611 (  4.441687)
// ```

// does ((a ^ b) & (a | b) >> 128)
static VALUE idhash_distance_bdigit(VALUE self, VALUE a, VALUE b){
    BDIGIT* tempd;
    long i, an = BIGNUM_LEN(a), bn = BIGNUM_LEN(b), templ, acc = 0;
    BDIGIT* as = BDIGITS(a);
    BDIGIT* bs = BDIGITS(b);

    while (0 < an && as[an-1] == 0) an--;
    while (0 < bn && bs[bn-1] == 0) bn--;
    if (an < bn) {
      tempd = as; as = bs; bs = tempd;
      templ = an; an = bn; bn = templ;
    }

    for (i = an; i-- > 4;) {
      acc += __builtin_popcountl((as[i] | (i >= bn ? 0 : bs[i])) & (as[i-4] ^ bs[i-4]));
    }
    RB_GC_GUARD(a);
    RB_GC_GUARD(b);
    return INT2FIX(acc);
}

static unsigned int * bignum_to_buf(VALUE a, size_t *num_longs) {
    size_t word_numbits = sizeof(unsigned int) * CHAR_BIT;
    size_t nlz_bits = 0;
    *num_longs = rb_absint_numwords(a, word_numbits, &nlz_bits);

    if (*num_longs == (size_t)-1) {
        rb_raise(rb_eRuntimeError, "Number too large to represent and overflow occured");
    }

    unsigned int *buf = ALLOC_N(unsigned int, *num_longs);

    rb_integer_pack(a, buf, *num_longs, sizeof(unsigned int), 0,
                    INTEGER_PACK_LSWORD_FIRST|INTEGER_PACK_NATIVE_BYTE_ORDER|
                    INTEGER_PACK_2COMP);

    return buf;
}

static VALUE idhash_distance_pack_algo(VALUE self, VALUE a, VALUE b) {
    size_t an;
    // TODO: could be long?
    unsigned int *as = bignum_to_buf(a, &an);

    size_t bn;
    unsigned int *bs = bignum_to_buf(b, &bn);

    while (an > 0 && as[an-1] == 0) an--;
    while (bn > 0 && bs[bn-1] == 0) bn--;

    if (an < bn) {
      unsigned int *tempd; size_t templ;
      tempd = as; as = bs; bs = tempd;
      templ = an; an = bn; bn = templ;
    }

    size_t i;
    long acc = 0;
    size_t cycles = 128 / (sizeof(unsigned int) * CHAR_BIT);

    for (i = an; i-- > cycles;) {
      acc += __builtin_popcountl((as[i] | (i >= bn ? 0 : bs[i])) & (as[i-4] ^ bs[i-4]));
    }

    xfree(as);
    xfree(bs);

    return INT2FIX(acc);
}

static VALUE idhash_popcount(VALUE self, VALUE a) {
    VALUE to_count = a;

    size_t word_numbits = sizeof(long) * CHAR_BIT;
    size_t nlz_bits = 0;
    size_t num_longs = rb_absint_numwords(to_count, word_numbits, &nlz_bits);

    if (num_longs == (size_t)-1) {
        rb_raise(rb_eRuntimeError, "Number too large to represent and overflow occured");
    }

    unsigned long *buf = ALLOC_N(unsigned long, num_longs);

    // TODO: in new versions could use rb_big_pack(to_count, buf, num_logs)
    rb_integer_pack(to_count, buf, num_longs, sizeof(long), 0,
                    INTEGER_PACK_LSWORD_FIRST|INTEGER_PACK_NATIVE_BYTE_ORDER|
                    INTEGER_PACK_2COMP);

    long count = 0;
    // Count set bits using __builtin_popcountl
    for (size_t i = 0; i < num_longs; i++) {
        count += __builtin_popcountl(buf[i]);
    }

    xfree(buf);

    return INT2FIX(count);
}

static VALUE my_and(VALUE a, VALUE b) {
    if (FIXNUM_P(a) && FIXNUM_P(b)) {
      return LONG2FIX(NUM2LONG(a) & NUM2LONG(b));
    } else if (FIXNUM_P(a)) {
      return rb_big_and(b, a);
    }
    else {
      return rb_big_and(a, b);
    }
}

static VALUE my_rshift(VALUE a, VALUE b) {
    if (TYPE(a) == T_BIGNUM) {
      return rb_big_rshift(a, b);
    }
    else {
      return rb_big_rshift(rb_uint2big(a), b);
    }
}

static VALUE idhash_distance_pack_bit_logic(VALUE self, VALUE a, VALUE b) {
    // Perform XOR operation: a ^ b
    VALUE xor_result = rb_big_xor(a, b);

    // Perform OR operation: a | b
    VALUE or_result = rb_big_or(a, b);

    // Perform shift operation ((a ^ b) & (a | b) >> 128)
    VALUE shift_amount = INT2FIX(128);
    VALUE shifted = my_rshift(or_result, shift_amount);

    // Perform AND operation: (a ^ b) & (a | b)
    VALUE and_result = my_and(shifted, xor_result);

    VALUE to_count = and_result;

    size_t word_numbits = sizeof(long) * CHAR_BIT;
    size_t nlz_bits = 0;
    size_t num_longs = rb_absint_numwords(to_count, word_numbits, &nlz_bits);

    if (num_longs == (size_t)-1) {
        rb_raise(rb_eRuntimeError, "Number too large to represent and overflow occured");
    }

    unsigned long *buf = ALLOC_N(unsigned long, num_longs);

    // TODO: in new versions could use rb_big_pack(to_count, buf, num_logs)
    rb_integer_pack(to_count, buf, num_longs, sizeof(long), 0,
                    INTEGER_PACK_LSWORD_FIRST|INTEGER_PACK_NATIVE_BYTE_ORDER|
                    INTEGER_PACK_2COMP);

    long count = 0;
    // Count set bits using __builtin_popcountl
    for (size_t i = 0; i < num_longs; i++) {
        count += __builtin_popcountl(buf[i]);
    }

    xfree(buf);

    return LONG2FIX(count);
}

void Init_idhash(void) {
    VALUE m = rb_define_module("DHashVips");
    VALUE mm = rb_define_module_under(m, "IDHash");
    rb_define_module_function(mm, "distance_bdigit_c", idhash_distance_bdigit, 2);
    rb_define_module_function(mm, "distance_pack_algo_c", idhash_distance_pack_algo, 2);
    rb_define_module_function(mm, "distance_pack_bit_logic_c", idhash_distance_pack_bit_logic, 2);
    rb_define_module_function(mm, "popcount_c", idhash_popcount, 1);
}
