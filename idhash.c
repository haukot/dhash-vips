#include <ruby.h>
// to skip include numeric.h from bignum.c
/* #define INTERNAL_NUMERIC_H */

/* #include <internal/numeric.h> */

#include <bignum.c>

// does ((a ^ b) & (a | b) >> 128)
static VALUE idhash_distance_bdigit(VALUE self, VALUE a, VALUE b){
  /* printf("bdigit a: %lu, b: %lu\n", a, b); */
    BDIGIT* tempd;
    long i, an = BIGNUM_LEN(a), bn = BIGNUM_LEN(b), templ, acc = 0;
    BDIGIT* as = BDIGITS(a);
    BDIGIT* bs = BDIGITS(b);

    /* for (size_t i = 0; i < an; i++) { */
    /*     printf("bdigit a[%lu]: %u\n", i, as[i]); */
    /* } */
    /* for (size_t i = 0; i < bn; i++) { */
    /*     printf("bdigit b[%lu]: %u\n", i, bs[i]); */
    /* } */

    while (0 < an && as[an-1] == 0) an--; // for (i = an; --i;) printf("%u\n", as[i]);
    while (0 < bn && bs[bn-1] == 0) bn--; // for (i = bn; --i;) printf("%u\n", bs[i]);
    // printf("%lu %lu\n", an, bn);
    if (an < bn) {
      tempd = as; as = bs; bs = tempd;
      templ = an; an = bn; bn = templ;
    }

    /* printf("bdigit an: %lu, bn: %lu\n", an, bn); */

    for (i = an; i-- > 4;) {
      // printf("%ld : (%u | %u) & (%u ^ %u)\n", i, as[i], (i >= bn ? 0 : bs[i]), as[i-4], bs[i-4]);

      /* printf("bdigit %ld : (%lu | %lu) & (%lu ^ %lu)\n", i, as[i], (i >= bn ? 0 : bs[i]), as[i-4], bs[i-4]); */
      acc += __builtin_popcountl((as[i] | (i >= bn ? 0 : bs[i])) & (as[i-4] ^ bs[i-4]));
      // printf("%ld : %ld\n", i, acc);
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
  /* printf("cpack  a: %lu, b: %lu\n", a, b); */
    size_t an;
    // TODO: could be long?
    unsigned int *as = bignum_to_buf(a, &an);

    size_t bn;
    unsigned int *bs = bignum_to_buf(b, &bn);

    /* for (size_t i = 0; i < an; i++) { */
    /*     printf("cpack  a[%lu]: %u\n", i, as[i]); */
    /* } */
    /* for (size_t i = 0; i < bn; i++) { */
    /*     printf("cpack  b[%lu]: %u\n", i, bs[i]); */
    /* } */

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

    /* printf("cpack  an: %lu, bn: %lu, cycles: %lu\n", an, bn, cycles); */
    for (i = an; i-- > cycles;) {
      /* printf("cpack  %ld : (%lu | %lu) & (%lu ^ %lu)\n", i, as[i], (i >= bn ? 0 : bs[i]), as[i-4], bs[i-4]); */
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
  /* // This print could fail */
  /* printf("my_and a %lu, b %lu\n", NUM2LONG(a), NUM2LONG(b)); */
    if (FIXNUM_P(a) && FIXNUM_P(b)) {
      return LONG2FIX(NUM2LONG(a) & NUM2LONG(b));
    } else if (FIXNUM_P(a)) {
      return rb_big_and(b, a);
    }
    else {
      return rb_big_and(a, b);
    }
    // TODO if both are fixnum
}

static VALUE my_rshift(VALUE a, VALUE b) {
  /* printf("A is a type: %d\n, T_BIGMUN: %d\n, T_FIXNUM: %d\n", TYPE(a), T_BIGNUM, T_FIXNUM); */
    if (TYPE(a) == T_BIGNUM) {
      return rb_big_rshift(a, b);
    }
    else {
      /* printf("A: %lu, B: %lu\n", a, b); */
      /* printf("FIX2LONG(a): %lu, FIX2LONG(b): %lu\n", FIX2LONG(a), FIX2LONG(b)); */
      /* printf("FIX2LONG(a) >> FIX2LONG(b): %lu\n", FIX2LONG(a) >> FIX2LONG(b)); */
      /* printf("rb_int2big(a): %lu\n", rb_big2int(rb_int2big(a))); */
      return rb_big_rshift(rb_uint2big(a), b);
      /* return LONG2NUM(FIX2LONG(a) >> FIX2LONG(b)); */
    }
}

static VALUE idhash_distance_pack_bit_logic(VALUE self, VALUE a, VALUE b) {
  /* printf("a: %lu, b: %lu\n", a, b); */
    // Perform XOR operation: a ^ b
    VALUE xor_result = rb_big_xor(a, b);

    /* if (FIXNUM_P(xor_result)) { */
    /*   printf("xor_result: %lu\n", NUM2LONG(xor_result)); */
    /* } */

    // Perform OR operation: a | b
    VALUE or_result = rb_big_or(a, b);

    // Perform shift operation ((a ^ b) & (a | b) >> 128)
    VALUE shift_amount = INT2FIX(128);
    VALUE shifted = my_rshift(or_result, shift_amount);

    /* if (FIXNUM_P(shifted)) { */
    /*   printf("shifted: %lu\n", NUM2LONG(shifted)); */
    /* } */

    // Perform AND operation: (a ^ b) & (a | b)
    VALUE and_result = my_and(shifted, xor_result);

    /* if (FIXNUM_P(and_result)) { */
    /*   printf("and_result: %lu\n", NUM2LONG(and_result)); */
    /* } */

    VALUE to_count = and_result;
    /* VALUE to_count = a; */

    /* printf("C\n"); */
    /* printf("xor_result: %lu, or_result: %lu, and_result: %lu, shifted: %lu\n", xor_result, or_result, and_result, shifted); */

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
      /* printf("buf[%lu]: %lu\n", i, buf[i]); */
      /* printf("__builtin_popcountl(buf[%lu]): %lu\n", i, __builtin_popcountl(buf[i])); */
        count += __builtin_popcountl(buf[i]);
    }

    /* printf("count: %lu\n", count); */

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
