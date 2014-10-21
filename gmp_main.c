
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <gmp.h>

#include "ptypes.h"
#include "gmp_main.h"
#include "prime_iterator.h"
#include "bls75.h"
#include "ecpp.h"
#include "utility.h"

static mpz_t _bgcd;

void _GMP_init(void)
{
  /* We should  not use this random number system for crypto, so
   * using this lousy seed is ok.  We just would like something a
   * bit different every run. */
  unsigned long seed = time(NULL);
  init_randstate(seed);
  prime_iterator_global_startup();
  mpz_init(_bgcd);
  _GMP_pn_primorial(_bgcd, 168);   /* mpz_primorial_ui(_bgcd, 1000) */
}

void _GMP_destroy(void)
{
  prime_iterator_global_shutdown();
  clear_randstate();
  mpz_clear(_bgcd);
}


static const unsigned char next_wheel[30] =
  {1,7,7,7,7,7,7,11,11,11,11,13,13,17,17,17,17,19,19,23,23,23,23,29,29,29,29,29,29,1};
static const unsigned char prev_wheel[30] =
  {29,29,1,1,1,1,1,1,7,7,7,7,11,11,13,13,13,13,17,17,19,19,19,19,23,23,23,23,23,23};
static const unsigned char wheel_advance[30] =
  {0,6,0,0,0,0,0,4,0,0,0,2,0,4,0,0,0,2,0,4,0,0,0,6,0,0,0,0,0,2};


static INLINE int _GMP_miller_rabin_ui(mpz_t n, UV base)
{
  int rval;
  mpz_t a;
  mpz_init_set_ui(a, base);
  rval = _GMP_miller_rabin(n, a);
  mpz_clear(a);
  return rval;
}

int _GMP_miller_rabin_random(mpz_t n, UV numbases)
{
  gmp_randstate_t* p_randstate = get_randstate();
  mpz_t base;
  UV i;

  /* We could just use mpz_probab_prime_p and call it a day. */

  /* Make sure we can make proper random bases */
  if (mpz_cmp_ui(n, 2) < 0) return 0;  /* below 2 is composite */
  if (mpz_cmp_ui(n, 4) < 0) return 1;  /* 2 and 3 are prime */
  if (mpz_even_p(n))        return 0;  /* multiple of 2 is composite */

  mpz_init(base);
  for (i = 0; i < numbases; i++) {
    /* select a random base between 2 and n-2 (we're lazy and use n-1) */
    do {
      mpz_urandomm(base, *p_randstate, n);
    } while (mpz_cmp(base, n) >= 0 || mpz_cmp_ui(base, 1) <= 0);
    if (_GMP_miller_rabin(n, base) == 0)
      break;
  }
  mpz_clear(base);
  return (i >= numbases);
}

int _GMP_miller_rabin(mpz_t n, mpz_t a)
{
  mpz_t nminus1, d, x;
  UV s, r;
  int rval;

  {
    int cmpr = mpz_cmp_ui(n, 2);
    if (cmpr == 0)     return 1;  /* 2 is prime */
    if (cmpr < 0)      return 0;  /* below 2 is composite */
    if (mpz_even_p(n)) return 0;  /* multiple of 2 is composite */
  }
  if (mpz_cmp_ui(a, 1) <= 0)
    croak("Base %ld is invalid", mpz_get_si(a));
  mpz_init_set(nminus1, n);
  mpz_sub_ui(nminus1, nminus1, 1);
  mpz_init_set(x, a);

  /* Handle large and small bases.  Use x so we don't modify their input a. */
  if (mpz_cmp(x, n) >= 0)
    mpz_mod(x, x, n);
  if ( (mpz_cmp_ui(x, 1) <= 0) || (mpz_cmp(x, nminus1) >= 0) ) {
    mpz_clear(nminus1);
    mpz_clear(x);
    return 1;
  }

  mpz_init_set(d, nminus1);
  s = mpz_scan1(d, 0);
  mpz_tdiv_q_2exp(d, d, s);

  mpz_powm(x, x, d, n);
  mpz_clear(d); /* done with a and d */
  rval = 0;
  if (!mpz_cmp_ui(x, 1) || !mpz_cmp(x, nminus1)) {
    rval = 1;
  } else {
    for (r = 1; r < s; r++) {
      mpz_powm_ui(x, x, 2, n);
      if (!mpz_cmp_ui(x, 1)) {
        break;
      }
      if (!mpz_cmp(x, nminus1)) {
        rval = 1;
        break;
      }
    }
  }
  mpz_clear(nminus1); mpz_clear(x);
  return rval;
}

/* This code was verified against Feitsma's psps-below-2-to-64.txt file.
 * is_strong_pseudoprime reduced it from 118,968,378 to 31,894,014.
 * all three variations of the Lucas test reduce it to 0.
 * The test suite should check that they generate the correct pseudoprimes.
 *
 * The standard and strong versions use the method A (Selfridge) parameters,
 * while the extra strong version uses the parameters from Grantham (2000).
 *
 * Using the strong version, we can implement the strong BPSW test as
 * specified by Baillie and Wagstaff, 1980, page 1401.
 *
 * Testing on my x86_64 machine, the strong Lucas code is over 35% faster than
 * T.R. Nicely's implementation, and over 40% faster than David Cleaver's.
 */
int _GMP_is_lucas_pseudoprime(mpz_t n, int do_strong)
{
  mpz_t d, U, V, Qk, T1, T2;
  IV D;
  UV P = 1;
  IV Q;
  UV s, b;
  int rval = 0;
  int _verbose = get_verbose_level();

  {
    int cmpr = mpz_cmp_ui(n, 2);
    if (cmpr == 0)     return 1;  /* 2 is prime */
    if (cmpr < 0)      return 0;  /* below 2 is composite */
    if (mpz_even_p(n)) return 0;  /* multiple of 2 is composite */
    if (mpz_perfect_square_p(n)) return 0;  /* n perfect square is composite */
  }
  mpz_init(T1);
  /* Determine Selfridge D, P, Q parameters */
  {
    UV D_ui = 5;
    IV sign = 1;
    while (1) {
      UV gcd = mpz_gcd_ui(NULL, n, D_ui);
      if ((gcd > 1) && mpz_cmp_ui(n, gcd) != 0) {
        mpz_clear(T1);
        return 0;
      }
      mpz_set_si(T1, (IV)D_ui * sign);
      if (mpz_jacobi(T1, n) == -1)  break;
      D_ui += 2;
      sign = -sign;
    }
    D = (IV)D_ui * sign;
  }
  Q = (1 - D) / 4;
  if (_verbose>3) gmp_printf("N: %Zd  D: %ld  P: %lu  Q: %ld\n", n, D, P, Q);
  if (D != ((IV)(P*P)) - 4*Q)  croak("incorrect DPQ\n");
  /* Now start on the Lucas sequence */
  mpz_init(T2);
  mpz_init_set_ui(U, 1);
  mpz_init_set_ui(V, P);
  mpz_init_set_si(Qk, Q);
  mpz_init_set(d, n);   mpz_add_ui(d, d, 1);

  s = mpz_scan1(d, 0);
  if (do_strong)
    mpz_tdiv_q_2exp(d, d, s);
  b = mpz_sizeinbase(d, 2);

  if (_verbose>3) gmp_printf("U=%Zd  V=%Zd  Q=%Zd\n", U, V, Qk);
  /* We assume P = 1, Q != 1. */
  while (b > 1) {
    mpz_mulmod(U, U, V, n, T1);     /* U2k = Uk * Vk */
    mpz_mul(T1, V, V);
    mpz_submul_ui(T1, Qk, 2);
    mpz_mod(V, T1, n);              /* V2k = Vk^2 - 2 Q^k */
    mpz_mul(Qk, Qk, Qk);            /* Q2k = Qk^2 */
    b--;
    if (_verbose>3) gmp_printf("U2k=%Zd  V2k=%Zd  Q2k=%Zd\n", U, V, Qk);
    if (mpz_tstbit(d, b-1)) { /* No mods in here */
      mpz_mul_si(T2, U, D);
                                  /* U:  U2k+1 = (P*U2k + V2k)/2 */
      mpz_add(T1, U, V);
      if (mpz_odd_p(T1)) mpz_add(T1, T1, n);
      mpz_fdiv_q_2exp(U, T1, 1);
                                  /* V:  V2k+1 = (D*U2k + P*V2k)/2 */
      mpz_add(T1, T2, V);
      if (mpz_odd_p(T1)) mpz_add(T1, T1, n);
      mpz_fdiv_q_2exp(V, T1, 1);
                                  /* Qk:  Qk+1 = Qk * Q */
      mpz_mul_si(Qk, Qk, Q);
    }
    mpz_mod(Qk, Qk, n);
    if (_verbose>3) gmp_printf("U=%Zd  V=%Zd  Q=%Zd\n", U, V, Qk);
  }
  mpz_mod(U, U, n);
  mpz_mod(V, V, n);

  if (!do_strong) {
    rval = (mpz_sgn(U) == 0);
  } else if ( (mpz_sgn(U) == 0) || (mpz_sgn(V) == 0) ) {
    rval = 1;
  } else {
    while (s--) {
      mpz_mul(T1, V, V);
      mpz_submul_ui(T1, Qk, 2);
      mpz_mod(V, T1, n);
      if (mpz_sgn(V) == 0) {
        rval = 1;
        break;
      }
      if (s)
        mpz_mulmod(Qk, Qk, Qk, n, T1);
    }
  }
  mpz_clear(d); mpz_clear(U); mpz_clear(V); mpz_clear(Qk);
  mpz_clear(T1); mpz_clear(T2);
  return rval;
}

int _GMP_is_extra_strong_lucas_pseudoprime(mpz_t n)
{
  mpz_t d, U, V, T1;
  UV D, P, Q, s, b;
  int rval = 0;
  int _verbose = get_verbose_level();

  {
    int cmpr = mpz_cmp_ui(n, 2);
    if (cmpr == 0)     return 1;  /* 2 is prime */
    if (cmpr < 0)      return 0;  /* below 2 is composite */
    if (mpz_even_p(n)) return 0;  /* multiple of 2 is composite */
    if (mpz_perfect_square_p(n)) return 0;  /* n perfect square is composite */
  }
  /* Determine parameters (Q=1) */
  mpz_init(T1);
  P = 3;
  Q = 1;
  while (1) {
    UV gcd;
    D = P*P - 4;
    mpz_set_ui(T1, D);
    gcd = mpz_gcd_ui(NULL, n, D);
    if (gcd > 1 && mpz_cmp_ui(n, gcd) != 0) {
      mpz_clear(T1);
      return 0;
    }
    if (mpz_jacobi(T1, n) == -1)  break;
    P++;
  }
  if (_verbose>3) gmp_printf("N: %Zd  D: %lu  P: %lu  Q: %ld\n", n, D, P, Q);
  if (D != (P*P - 4*Q))  croak("incorrect DPQ\n");
  /* Now start on the Lucas sequence */
  mpz_init_set_ui(U, 1);
  mpz_init_set_ui(V, P);
  mpz_init_set(d, n);   mpz_add_ui(d, d, 1);

  s = mpz_scan1(d, 0);
  mpz_tdiv_q_2exp(d, d, s);
  b = mpz_sizeinbase(d, 2);

  if (_verbose>3) gmp_printf("U=%Zd  V=%Zd\n", U, V);
  /* We assume P != 1, Q == 1. */
  while (b > 1) {
    mpz_mulmod(U, U, V, n, T1);     /* U2k = Uk * Vk */
    mpz_mul(V, V, V);
    mpz_sub_ui(V, V, 2);
    mpz_mod(V, V, n);               /* V2k = Vk^2 - 2 Q^k */
    b--;
    if (_verbose>3) gmp_printf("U2k=%Zd  V2k=%Zd\n", U, V);
    if (mpz_tstbit(d, b-1)) { /* No mods in here */
      mpz_mul_si(T1, U, D);
                                  /* U:  U2k+1 = (P*U2k + V2k)/2 */
      mpz_mul_ui(U, U, P);
      mpz_add(U, U, V);
      if (mpz_odd_p(U)) mpz_add(U, U, n);
      mpz_fdiv_q_2exp(U, U, 1);
                                  /* V:  V2k+1 = (D*U2k + P*V2k)/2 */
      mpz_mul_ui(V, V, P);
      mpz_add(V, V, T1);
      if (mpz_odd_p(V)) mpz_add(V, V, n);
      mpz_fdiv_q_2exp(V, V, 1);
    }
    if (_verbose>3) gmp_printf("U=%Zd  V=%Zd\n", U, V);
  }
  mpz_mod(U, U, n);
  mpz_mod(V, V, n);

  mpz_sub_ui(T1, n, 2);
  if (mpz_sgn(U) == 0 && (mpz_cmp_ui(V, 2) == 0 || mpz_cmp(V, T1) == 0)) {
    rval = 1;
  } else if (mpz_sgn(V) == 0) {
    rval = 1;
  } else {
    while (s--) {
      mpz_mul(V, V, V);
      mpz_sub_ui(V, V, 2);
      mpz_mod(V, V, n);
      if (mpz_sgn(V) == 0) {
        rval = 1;
        break;
      }
    }
  }
  mpz_clear(d); mpz_clear(U); mpz_clear(V); mpz_clear(T1);
  return rval;
}



UV _GMP_trial_factor(mpz_t n, UV from_n, UV to_n)
{
  int small_n = 0;
  UV f;
  PRIME_ITERATOR(iter);

  if (mpz_cmp_ui(n, 4) < 0) {
    return (mpz_cmp_ui(n, 1) <= 0) ? 1 : 0;   /* 0,1 => 1   2,3 => 0 */
  }
  if ( (from_n <= 2) && mpz_even_p(n) )   return 2;

  if (from_n > to_n)
    croak("GMP_trial_factor from > to: %"UVuf" - %"UVuf, from_n, to_n);

  if (mpz_cmp_ui(n, to_n*to_n) < 0)
    small_n = 1;

  for (f = 2; f <= to_n; f = prime_iterator_next(&iter)) {
    if (small_n && mpz_cmp_ui(n, f*f) < 0) break;
    if (mpz_divisible_ui_p(n, f)) { prime_iterator_destroy(&iter); return f; }
  }
  prime_iterator_destroy(&iter);
  return 0;
}

/*
 * is_prob_prime      BPSW -- fast, no known counterexamples
 * is_prime           is_prob_prime + a little extra
 * is_provable_prime  really prove it, which could take a very long time
 *
 * They're all identical for numbers <= 2^64.
 *
 * The extra M-R tests in is_prime start actually costing something after
 * 1000 bits or so.  Primality proving will get quite a bit slower as the
 * number of bits increases.
 *
 * What are these extra M-R tests getting us?  From Rabin-Monier, with random
 * bases we should have p <= 4^-t.  So one extra test gives us p = 0.25, and
 * four tests gives us p = 0.00390625.  Hence a 0.4% chance that a composite
 * will pass the tests.  But for larger k (bits in n), this estimate is very
 * conservative.  See Damgård, Landrock, and Pomerance, 1993.  Also remember
 * that BPSW is correct for k <= 64, so k will always be > 64 for this case.
 * Given a k and t, show probabilities:
 *   perl -E 'my ($k,$t)=(256,2);  say 4**(-$t);
 *     my $p1 = $k*$k*4**(2.0-sqrt($k));  say $p1**$t;
 *     if (($t == 2 && $k >= 88) || ($t >= 3 && $k >= 21 && $t <= $k/9)) {
 *        my $p2 = $k**1.5 * 2**$t * $t**-.5 * 4**(2.0-sqrt($t*$k));  say $p2;
 *     }'
 * For a 256-bit input, we get:
 *   1 test:  p < 0.000244140625
 *   2 tests: p < 0.00000000441533
 *   3 tests: p < 0.0000000000062550875
 *   4 tests: p < 0.0000000000000035527137
 *
 * It's even more extreme as k goes higher.
 */

int _GMP_is_prob_prime(mpz_t n)
{
  /*  Step 1: Look for small divisors.  This is done purely for performance.
   *          It is *not* a requirement for the BPSW test. */

  /* If less than 1009, make trial factor handle it. */
  if (mpz_cmp_ui(n, 1008) <= 0)
    return _GMP_trial_factor(n, 2, 997) ? 0 : 2;
  /* Check for tiny divisors (GMP can do these really fast) */
  if ( mpz_even_p(n)
    || mpz_divisible_ui_p(n, 3)
    || mpz_divisible_ui_p(n, 5) ) return 0;
  /* Do a big GCD with all primes < 1009 */
  {
    mpz_t t;
    int r;
    mpz_init(t);
    mpz_gcd(t, n, _bgcd);
    r = mpz_cmp_ui(t, 1);
    mpz_clear(t);
    if (r != 0)
      return 0;
  }
  /* No divisors under 1009 */
  if (mpz_cmp_ui(n, 1009*1009) < 0)
    return 2;

  /*  Step 2: The BPSW test.  psp base 2 and slpsp. */

  /* Miller Rabin with base 2 */
  if (_GMP_miller_rabin_ui(n, 2) == 0)
    return 0;

  /* Strong Lucas-Selfridge */
  if (_GMP_is_lucas_pseudoprime(n, 1 /*strong*/) == 0)
    return 0;

  /* BPSW is deterministic below 2^64 */
  if (mpz_sizeinbase(n, 2) <= 64)
    return 2;

  return 1;
}

int _GMP_is_prime(mpz_t n)
{
  UV nbits = mpz_sizeinbase(n, 2);
  int prob_prime = _GMP_is_prob_prime(n);

  /* We're pretty sure n is a prime since it passed the BPSW test.  Try
   * a few random M-R bases to give some extra assurance.  See discussion
   * above about number of tests.  Assuming we are choosing uniformly random
   * bases, the caller cannot predict our random numbers (not guaranteed!),
   * and the caller is not choosing from a subset of worst case inputs,
   * then there is less than a 1 in 595,000 chance that a composite will pass
   * our extra tests. */

  if (prob_prime == 1) {
    UV ntests;
    if      (nbits <  80) ntests = 5;  /* p < .00000168 */
    else if (nbits < 115) ntests = 4;  /* p < .00000156 */
    else if (nbits < 200) ntests = 3;  /* p < .00000060 */
    else                  ntests = 2;  /* p < .00000012 */
    prob_prime = _GMP_miller_rabin_random(n, ntests);
  }

  /* For small numbers, try a quick BLS75 n-1 proof. */
  if (prob_prime == 1 && nbits <= 200)
    prob_prime = _GMP_primality_bls_nm1(n, 1 /* effort */, 0 /* proof */);

  return prob_prime;
}

int _GMP_is_provable_prime(mpz_t n, char** prooftext)
{
  int prob_prime = _GMP_is_prob_prime(n);

  /* The primality proving algorithms tend to be VERY slow for composites,
   * so run a few more MR tests. */
  if (prob_prime == 1)
    prob_prime = _GMP_miller_rabin_random(n, 2);

  /* We can choose a primality proving algorithm:
   *   AKS    _GMP_is_aks_prime       really slow, don't bother
   *   N-1    _GMP_primality_bls_nm1  small or special numbers
   *   ECPP   _GMP_ecpp               fastest in general
   */

  /* Give n-1 a small go */
  if (prob_prime == 1)
    prob_prime = _GMP_primality_bls_nm1(n, 2, prooftext);

  /* ECPP */
  if (prob_prime == 1)
    prob_prime = _GMP_ecpp(n, prooftext);

  return prob_prime;
}

#if 0
  /* This would be useful for a Bernstein AKS variant */
static UV largest_factor(UV n) {
  UV p = 2;
  PRIME_ITERATOR(iter);
  while (n >= p*p && !prime_iterator_isprime(&iter, n)) {
    while ( (n % p) == 0  &&  n >= p*p ) { n /= p; }
    p = prime_iterator_next(&iter);
  }
  prime_iterator_destroy(&iter);
  return n;
}
#endif

/*****************************************************************************/
/*          AKS.    This implementation is quite slow, but useful to have.   */

static int test_anr(UV a, mpz_t n, UV r, mpz_t* px, mpz_t* py)
{
  int retval = 1;
  UV i, n_mod_r;
  mpz_t t;

  for (i = 0; i < r; i++)
    mpz_set_ui(px[i], 0);

  a %= r;
  mpz_set_ui(px[0], a);
  mpz_set_ui(px[1], 1);

  poly_mod_pow(py, px, n, r, n);

  mpz_init(t);
  n_mod_r = mpz_mod_ui(t, n, r);
  if (n_mod_r >= r)  croak("n mod r >= r ?!");
  mpz_sub_ui(t, py[n_mod_r], 1);
  mpz_mod(py[n_mod_r], t, n);
  mpz_sub_ui(t, py[0], a);
  mpz_mod(py[0], t, n);
  mpz_clear(t);

  for (i = 0; i < r; i++)
    if (mpz_sgn(py[i]))
      retval = 0;
  return retval;
}

int _GMP_is_aks_prime(mpz_t n)
{
  mpz_t sqrtn;
  mpz_t *px, *py;
  int retval;
  UV i, limit, rlimit, r, a;
  double log2n;
  int _verbose = get_verbose_level();
  /* PRIME_ITERATOR(iter); */

  if (mpz_cmp_ui(n, 4) < 0) {
    return (mpz_cmp_ui(n, 1) <= 0) ? 0 : 1;
  }

  if (mpz_perfect_power_p(n)) {
    return 0;
  }

  mpz_init(sqrtn);
  mpz_sqrt(sqrtn, n);
  /* limit should be floor( log2(n) ** 2 ).  The simple GMP solution is
   * to get ceil(log2(n)) viz mpz_sizeinbase(n,2) and square, but that
   * overcalculates by a fair amount.  We'll calculate float log2n as:
   *   ceil(log2(n**k)) / k  [mpz_sizeinbase(n,2) <=> ceil(log2(n))]
   * which gives us a value that slightly overestimates log2(n).
   */
  {
    mpz_t t;
    mpz_init(t);
    mpz_pow_ui(t, n, 32);
    log2n = ((double) mpz_sizeinbase(t, 2) + 0.000001) / 32.0;
    limit = (UV) floor( log2n * log2n );
    mpz_clear(t);
  }

  if (_verbose>1) gmp_printf("# AKS checking order_r(%Zd) to %lu\n", n, (unsigned long) limit);

  /* Using a native r limits us to ~2000 digits in the worst case (r ~ log^5n)
   * but would typically work for 100,000+ digits (r ~ log^3n).  This code is
   * far too slow to matter either way. */

  /* Note, since order_r(n) can never be > r, we should start r at limit+1 */
  for (r = limit+1; mpz_cmp_ui(n, r) >= 0; r++) {
    if (mpz_divisible_ui_p(n, r)) {   /* r divides n.  composite. */
      /* prime_iterator_destroy(&iter); */
      mpz_clear(sqrtn);
      return 0;
    }
    if (mpz_cmp_ui(sqrtn, r) < 0) {  /* no r <= sqrtn divides n.  prime. */
      /* prime_iterator_destroy(&iter); */
      mpz_clear(sqrtn);
      return 1;
    }
    if (mpz_order_ui(r, n, limit) > limit)
      break;
  }
  /* prime_iterator_destroy(&iter); */
  mpz_clear(sqrtn);

  if (mpz_cmp_ui(n, r) <= 0) {
    return 1;
  }

  rlimit = (UV) floor( sqrt(r-1) * log2n );

  if (_verbose) gmp_printf("# AKS %Zd.  r = %lu rlimit = %lu\n", n, (unsigned long) r, (unsigned long) rlimit);

  /* Create the three polynomials we will use */
  New(0, px, r, mpz_t);
  New(0, py, r, mpz_t);
  if ( !px || !py )
    croak("allocation failure\n");
  for (i = 0; i < r; i++) {
    mpz_init(px[i]);
    mpz_init(py[i]);
  }

  retval = 1;
  for (a = 1; a <= rlimit; a++) {
    if (! test_anr(a, n, r, px, py) ) {
      retval = 0;
      break;
    }
    if (_verbose>1) { printf("."); fflush(stdout); }
  }
  if (_verbose>1) { printf("\n"); fflush(stdout); };

  /* Free the polynomials */
  for (i = 0; i < r; i++) {
    mpz_clear(px[i]);
    mpz_clear(py[i]);
  }
  Safefree(px);
  Safefree(py);

  return retval;
}

/*****************************************************************************/


/* Modifies argument */
void _GMP_next_prime(mpz_t n)
{
  mpz_t d;
  UV m;

  /* small inputs */
  if (mpz_cmp_ui(n, 7) < 0) {
    if      (mpz_cmp_ui(n, 2) < 0) { mpz_set_ui(n, 2); }
    else if (mpz_cmp_ui(n, 3) < 0) { mpz_set_ui(n, 3); }
    else if (mpz_cmp_ui(n, 5) < 0) { mpz_set_ui(n, 5); }
    else                           { mpz_set_ui(n, 7); }
    return;
  }

  mpz_init(d);
  m = mpz_fdiv_q_ui(d, n, 30);

  if (m == 29) {
    mpz_add_ui(d, d, 1);
    m = 1;
  } else {
    m = next_wheel[m];
  }
  mpz_mul_ui(n, d, 30);
  mpz_add_ui(n, n, m);
  while (1) {
    if (_GMP_is_prob_prime(n))
      break;
    mpz_add_ui(n, n, wheel_advance[m]);
    m = next_wheel[m];
  }
  mpz_clear(d);
}

/* Modifies argument */
void _GMP_prev_prime(mpz_t n)
{
  mpz_t d;
  UV m;

  /* small inputs */
  if (mpz_cmp_ui(n, 2) <= 0) { mpz_set_ui(n, 0); return; }
  if (mpz_cmp_ui(n, 3) <= 0) { mpz_set_ui(n, 2); return; }
  if (mpz_cmp_ui(n, 5) <= 0) { mpz_set_ui(n, 3); return; }
  if (mpz_cmp_ui(n, 7) <= 0) { mpz_set_ui(n, 5); return; }

  mpz_init(d);
  m = mpz_fdiv_q_ui(d, n, 30);

  while (1) {
    m = prev_wheel[m];
    if (m == 29)
      mpz_sub_ui(d, d, 1);
    mpz_mul_ui(n, d, 30);
    mpz_add_ui(n, n, m);
    if (_GMP_is_prob_prime(n))
      break;
  }
  mpz_clear(d);
}

void _GMP_pn_primorial(mpz_t prim, UV n)
{
  UV p = 2;
  PRIME_ITERATOR(iter);

  mpz_set_ui(prim, 1);
  while (n--) {
    mpz_mul_ui(prim, prim, p);
    p = prime_iterator_next(&iter);
  }
  prime_iterator_destroy(&iter);
}
void _GMP_primorial(mpz_t prim, mpz_t n)
{
  UV p = 2;
  PRIME_ITERATOR(iter);

  mpz_set_ui(prim, 1);
  while (mpz_cmp_ui(n, p) >= 0) {
    mpz_mul_ui(prim, prim, p);
    p = prime_iterator_next(&iter);
  }
  prime_iterator_destroy(&iter);
}

#define TEST_FOR_2357(n, f) \
  { \
    if (mpz_divisible_ui_p(n, 2)) { mpz_set_ui(f, 2); return 1; } \
    if (mpz_divisible_ui_p(n, 3)) { mpz_set_ui(f, 3); return 1; } \
    if (mpz_divisible_ui_p(n, 5)) { mpz_set_ui(f, 5); return 1; } \
    if (mpz_divisible_ui_p(n, 7)) { mpz_set_ui(f, 7); return 1; } \
    if (mpz_cmp_ui(n, 121) < 0) { return 0; } \
  }



int _GMP_prho_factor(mpz_t n, mpz_t f, UV a, UV rounds)
{
  mpz_t U, V, oldU, oldV, m;
  int i;
  const UV inner = 256;

  TEST_FOR_2357(n, f);
  rounds = (rounds + inner - 1) / inner;
  mpz_init_set_ui(U, 7);
  mpz_init_set_ui(V, 7);
  mpz_init(m);
  mpz_init(oldU);
  mpz_init(oldV);
  while (rounds-- > 0) {
    mpz_set_ui(m, 1); mpz_set(oldU, U);  mpz_set(oldV, V);
    for (i = 0; i < (int)inner; i++) {
      mpz_mul(U, U, U);  mpz_add_ui(U, U, a);  mpz_tdiv_r(U, U, n);
      mpz_mul(V, V, V);  mpz_add_ui(V, V, a);  mpz_tdiv_r(V, V, n);
      mpz_mul(V, V, V);  mpz_add_ui(V, V, a);  mpz_tdiv_r(V, V, n);
      if (mpz_cmp(U, V) >= 0)  mpz_sub(f, U, V);
      else                     mpz_sub(f, V, U);
      mpz_mul(m, m, f);
      mpz_tdiv_r(m, m, n);
    }
    mpz_gcd(f, m, n);
    if (!mpz_cmp_ui(f, 1))
      continue;
    if (!mpz_cmp(f, n)) {
      /* f == n, so we have to back up to see what factor got found */
      mpz_set(U, oldU); mpz_set(V, oldV);
      i = inner;
      do {
        mpz_mul(U, U, U);  mpz_add_ui(U, U, a);  mpz_tdiv_r(U, U, n);
        mpz_mul(V, V, V);  mpz_add_ui(V, V, a);  mpz_tdiv_r(V, V, n);
        mpz_mul(V, V, V);  mpz_add_ui(V, V, a);  mpz_tdiv_r(V, V, n);
        if (mpz_cmp(U, V) >= 0)  mpz_sub(f, U, V);
        else                     mpz_sub(f, V, U);
        mpz_gcd(f, f, n);
      } while (!mpz_cmp_ui(f, 1) && i-- != 0);
      if ( (!mpz_cmp_ui(f, 1)) || (!mpz_cmp(f, n)) )  break;
    }
    mpz_clear(U); mpz_clear(V); mpz_clear(m); mpz_clear(oldU); mpz_clear(oldV);
    return 1;
  }
  mpz_clear(U); mpz_clear(V); mpz_clear(m); mpz_clear(oldU); mpz_clear(oldV);
  mpz_set(f, n);
  return 0;
}

int _GMP_pbrent_factor(mpz_t n, mpz_t f, UV a, UV rounds)
{
  mpz_t Xi, Xm, saveXi, m, t;
  UV i, r;
  const UV inner = 256;

  TEST_FOR_2357(n, f);
  mpz_init_set_ui(Xi, 2);
  mpz_init_set_ui(Xm, 2);
  mpz_init(m);
  mpz_init(t);
  mpz_init(saveXi);

  r = 1;
  while (rounds > 0) {
    UV rleft = (r > rounds) ? rounds : r;
    while (rleft > 0) {   /* Do rleft rounds, inner at a time */
      UV dorounds = (rleft > inner) ? inner : rleft;
      mpz_set_ui(m, 1);
      mpz_set(saveXi, Xi);
      for (i = 0; i < dorounds; i++) {
        mpz_mul(t, Xi, Xi);  mpz_add_ui(t, t, a);  mpz_tdiv_r(Xi, t, n);
        if (mpz_cmp(Xi, Xm) >= 0)  mpz_sub(f, Xi, Xm);
        else                       mpz_sub(f, Xm, Xi);
        mpz_mul(t, m, f);
        mpz_tdiv_r(m, t, n);
      }
      rleft -= dorounds;
      rounds -= dorounds;
      mpz_gcd(f, m, n);
      if (mpz_cmp_ui(f, 1) != 0)
        break;
    }
    if (!mpz_cmp_ui(f, 1)) {
      r *= 2;
      mpz_set(Xm, Xi);
      continue;
    }
    if (!mpz_cmp(f, n)) {
      /* f == n, so we have to back up to see what factor got found */
      mpz_set(Xi, saveXi);
      do {
        mpz_mul(t, Xi, Xi);  mpz_add_ui(t, t, a);  mpz_tdiv_r(Xi, t, n);
        if (mpz_cmp(Xi, Xm) >= 0)  mpz_sub(f, Xi, Xm);
        else                       mpz_sub(f, Xm, Xi);
        mpz_gcd(f, f, n);
      } while (!mpz_cmp_ui(f, 1) && r-- != 0);
      if ( (!mpz_cmp_ui(f, 1)) || (!mpz_cmp(f, n)) )  break;
    }
    mpz_clear(Xi); mpz_clear(Xm); mpz_clear(m); mpz_clear(saveXi); mpz_clear(t);
    return 1;
  }
  mpz_clear(Xi); mpz_clear(Xm); mpz_clear(m); mpz_clear(saveXi); mpz_clear(t);
  mpz_set(f, n);
  return 0;
}


void _GMP_lcm_of_consecutive_integers(UV B, mpz_t m)
{
  UV p, p_power, pmin;
  PRIME_ITERATOR(iter);

  /* For each prime, multiply m by p^floor(log B / log p), which means
   * raise p to the largest power e such that p^e <= B.
   */
  mpz_set_ui(m, 1);
  if (B >= 2) {
    p_power = 2;
    while (p_power <= B/2)
      p_power *= 2;
    mpz_mul_ui(m, m, p_power);
  }
  p = prime_iterator_next(&iter);
  while (p <= B) {
    pmin = B/p;
    if (p > pmin)
      break;
    p_power = p*p;
    while (p_power <= pmin)
      p_power *= p;
    mpz_mul_ui(m, m, p_power);
    p = prime_iterator_next(&iter);
  }
  while (p <= B) {
    mpz_mul_ui(m, m, p);
    p = prime_iterator_next(&iter);
  }
  prime_iterator_destroy(&iter);
}


int _GMP_pminus1_factor(mpz_t n, mpz_t f, UV B1, UV B2)
{
  mpz_t a, savea, t;
  UV q, saveq, j;
  int _verbose = get_verbose_level();
  PRIME_ITERATOR(iter);

  TEST_FOR_2357(n, f);
  if (B1 < 7) return 0;

  mpz_init(a);
  mpz_init(savea);
  mpz_init(t);

  if (_verbose>2) gmp_printf("# p-1 trying %Zd (B1=%lu B2=%lu)\n", n, (unsigned long)B1, (unsigned long)B2);

  /* STAGE 1
   * Montgomery 1987 p249-250 and Brent 1990 p5 both indicate we can calculate
   * a^m mod n where m is the lcm of the integers to B1.  This can be done
   * using either
   *    m = calc_lcm(B), b = a^m mod n
   * or
   *    calculate_b_lcm(b, B1, a, n);
   *
   * The first means raising a to a huge power then doing the mod, which is
   * inefficient and can be _very_ slow on some machines.  The latter does
   * one powmod for each prime power, which works pretty well.  Yet another
   * way to handle this is to loop over each prime p below B1, calculating
   * a = a^(p^e) mod n, where e is the largest e such that p^e <= B1.
   * My experience with GMP is that this last method is faster with large B1,
   * sometimes a lot faster.
   *
   * One thing that can speed things up quite a bit is not running the GCD
   * on every step.  However with small factors this means we can easily end
   * up with multiple factors between GCDs, so we allow backtracking.  This
   * could also be added to stage 2, but it's far less likely to happen there.
   */
  j = 1;
  mpz_set_ui(a, 2);
  mpz_set_ui(savea, 2);
  saveq = 2;
  /* We could wrap this in a loop trying a few different a values, in case
   * the current one ended up going to 0. */
  q = 2;
  mpz_set_ui(t, 1);
  while (q <= B1) {
    UV k, kmin;
    k = q;
    kmin = B1/q;
    while (k <= kmin)
      k *= q;
    mpz_mul_ui(t, t, k);        /* Accumulate powers for a */
    if ( (j++ % 32) == 0) {
      mpz_powm(a, a, t, n);     /* a=a^(k1*k2*k3*...) mod n */
      if (mpz_sgn(a))  mpz_sub_ui(t, a, 1);
      else             mpz_sub_ui(t, n, 1);
      mpz_gcd(f, t, n);         /* f = gcd(a-1, n) */
      mpz_set_ui(t, 1);
      if (mpz_cmp(f, n) == 0)
        break;
      if (mpz_cmp_ui(f, 1) != 0)
        goto end_success;
      saveq = q;
      mpz_set(savea, a);
    }
    q = prime_iterator_next(&iter);
  }
  mpz_powm(a, a, t, n);
  if (mpz_sgn(a))  mpz_sub_ui(t, a, 1);
  else             mpz_sub_ui(t, n, 1);
  mpz_gcd(f, t, n);
  if (mpz_cmp(f, n) == 0) {
    /* We found multiple factors.  Loop one at a time. */
    prime_iterator_setprime(&iter, saveq);
    mpz_set(a, savea);
    for (q = saveq; q <= B1; q = prime_iterator_next(&iter)) {
      UV k = q;
      UV kmin = B1/q;
      while (k <= kmin)
        k *= q;
      mpz_powm_ui(a, a, k, n );
      mpz_sub_ui(t, a, 1);
      mpz_gcd(f, t, n);
      if (mpz_cmp(f, n) == 0)
        goto end_fail;
      if (mpz_cmp_ui(f, 1) != 0)
        goto end_success;
    }
  }
  if ( (mpz_cmp_ui(f, 1) != 0) && (mpz_cmp(f, n) != 0) )
    goto end_success;

  /* STAGE 2
   * This is the standard continuation which replaces the powmods in stage 1
   * with two mulmods, with a GCD every 64 primes (no backtracking).
   * This is quite a bit faster than stage 1.
   * See Montgomery 1987, p250-253 for possible optimizations.
   * We quickly precalculate a few of the prime gaps, and lazily cache others
   * up to a gap of 222.  That's enough for a B2 value of 189 million.  We
   * still work above that, we just won't cache the value for big gaps.
   */
  if (B2 > B1) {
    mpz_t b, bm, bmdiff;
    mpz_t precomp_bm[111];
    int   is_precomp[111] = {0};

    mpz_init(bmdiff);
    mpz_init_set(bm, a);
    mpz_init_set_ui(b, 1);

    /* Set the first 20 differences */
    mpz_powm_ui(bmdiff, bm, 2, n);
    mpz_init_set(precomp_bm[0], bmdiff);
    is_precomp[0] = 1;
    for (j = 1; j < 22; j++) {
      mpz_mul(bmdiff, bmdiff, bm);
      mpz_mul(bmdiff, bmdiff, bm);
      mpz_tdiv_r(bmdiff, bmdiff, n);
      mpz_init_set(precomp_bm[j], bmdiff);
      is_precomp[j] = 1;
    }

    mpz_powm_ui(a, a, q, n );

    j = 31;
    while (q <= B2) {
      UV lastq, qdiff;

      lastq = q;
      q = prime_iterator_next(&iter);
      qdiff = (q - lastq) / 2 - 1;

      if (qdiff < 111 && is_precomp[qdiff]) {
        mpz_mul(t, a, precomp_bm[qdiff]);
      } else if (qdiff < 111) {
        mpz_powm_ui(bmdiff, bm, q-lastq, n);
        mpz_init_set(precomp_bm[qdiff], bmdiff);
        is_precomp[qdiff] = 1;
        mpz_mul(t, a, bmdiff);
      } else {
        mpz_powm_ui(bmdiff, bm, q-lastq, n);  /* Big gap */
        mpz_mul(t, a, bmdiff);
      }
      mpz_tdiv_r(a, t, n);
      if (mpz_sgn(a))  mpz_sub_ui(t, a, 1);
      else             mpz_sub_ui(t, n, 1);
      mpz_mul(b, b, t);
      if ((j % 2) == 0)           /* put off mods a little */
        mpz_tdiv_r(b, b, n);
      if ( (j++ % 64) == 0) {     /* GCD every so often */
        mpz_gcd(f, b, n);
        if ( (mpz_cmp_ui(f, 1) != 0) && (mpz_cmp(f, n) != 0) )
          break;
      }
    }
    mpz_gcd(f, b, n);
    mpz_clear(b);
    mpz_clear(bm);
    mpz_clear(bmdiff);
    for (j = 0; j < 111; j++) {
      if (is_precomp[j])
        mpz_clear(precomp_bm[j]);
    }
    if ( (mpz_cmp_ui(f, 1) != 0) && (mpz_cmp(f, n) != 0) )
      goto end_success;
  }

  end_fail:
    mpz_set(f,n);
  end_success:
    prime_iterator_destroy(&iter);
    mpz_clear(a);
    mpz_clear(savea);
    mpz_clear(t);
    if ( (mpz_cmp_ui(f, 1) != 0) && (mpz_cmp(f, n) != 0) ) {
      if (_verbose>2) gmp_printf("# p-1: %Zd\n", f);
      return 1;
    }
    if (_verbose>2) gmp_printf("# p-1: no factor\n");
    mpz_set(f, n);
    return 0;
}

int _GMP_holf_factor(mpz_t n, mpz_t f, UV rounds)
{
  mpz_t s, m;
  UV i;

#define PREMULT 480   /* 1  2  6  12  480  151200 */

  TEST_FOR_2357(n, f);
  if (mpz_perfect_square_p(n)) {
    mpz_sqrt(f, n);
    return 1;
  }

  mpz_mul_ui(n, n, PREMULT);
  mpz_init(s);
  mpz_init(m);
  for (i = 1; i <= rounds; i++) {
    mpz_mul_ui(f, n, i);    /* f = n*i */
    if (mpz_perfect_square_p(f)) {
      /* s^2 = n*i, so m = s^2 mod n = 0.  Hence f = GCD(n, s) = GCD(n, n*i) */
      mpz_divexact_ui(n, n, PREMULT);
      mpz_gcd(f, f, n);
      mpz_clear(s); mpz_clear(m);
      if (mpz_cmp(f, n) == 0)  return 0;
      return 1;
    }
    mpz_sqrt(s, f);
    mpz_add_ui(s, s, 1);    /* s = ceil(sqrt(n*i)) */
    mpz_mul(m, s, s);
    mpz_sub(m, m, f);       /* m = s^2 mod n = s^2 - n*i */
    if (mpz_perfect_square_p(m)) {
      mpz_divexact_ui(n, n, PREMULT);
      mpz_sqrt(f, m);
      mpz_sub(s, s, f);
      mpz_gcd(f, s, n);
      mpz_clear(s); mpz_clear(m); return 1;
    }
  }
  mpz_divexact_ui(n, n, PREMULT);
  mpz_set(f, n);
  mpz_clear(s); mpz_clear(m);
  return 0;
}


/*----------------------------------------------------------------------
 * GMP version of Ben Buhrow's public domain 9/24/09 implementation.
 * It uses ideas and code from Jason Papadopoulos, Scott Contini, and
 * Tom St. Denis.  Also see the papers of Stephen McMath, Daniel Shanks,
 * and Jason Gower.  Gower and Wagstaff is particularly useful:
 *    http://homes.cerias.purdue.edu/~ssw/squfof.pdf
 *--------------------------------------------------------------------*/

static int shanks_mult(mpz_t n, mpz_t f)
{
   /*
    * use shanks SQUFOF to factor N.
    *
    * return 0 if no factor found, 1 if found with factor in f1.
    *
    * Input should have gone through trial division to 5.
    */

   int result = 0;
   unsigned long j=0;
   mpz_t b0, bn, imax, tmp, Q0, Qn, P, i, t1, t2, S, Ro, So, bbn;

   if (mpz_cmp_ui(n, 3) <= 0)
     return 0;

   if (mpz_perfect_square_p(n)) {
     mpz_sqrt(f, n);
     return 1;
   }

   mpz_init(b0);
   mpz_init(bn);
   mpz_init(imax);
   mpz_init(tmp);
   mpz_init(Q0);
   mpz_init(Qn);
   mpz_init(P);
   mpz_init(i);
   mpz_init(t1);
   mpz_init(t2);
   mpz_init(S);
   mpz_init(Ro);
   mpz_init(So);
   mpz_init(bbn);

   mpz_mod_ui(t1, n, 4);
   if (mpz_cmp_ui(t1, 3))
     croak("Incorrect call to shanks\n");

   mpz_sqrt(b0, n);
   mpz_sqrt(tmp, b0);
   mpz_mul_ui(imax, tmp, 3);

   /* set up recurrence */
   mpz_set_ui(Q0, 1);
   mpz_set(P, b0);
   mpz_mul(tmp, b0, b0);
   mpz_sub(Qn, n, tmp);

   mpz_add(tmp, b0, P);
   mpz_tdiv_q(bn, tmp, Qn);

   mpz_set_ui(i, 0);
   while (1) {
      j=0;
      while (1) {
         mpz_set(t1, P);   /* hold Pn for this iteration */
         mpz_mul(tmp, bn, Qn);
         mpz_sub(P, tmp, P);
         mpz_set(t2, Qn);  /* hold Qn for this iteration */
         mpz_sub(tmp, t1, P);
         mpz_mul(tmp, tmp, bn);
         mpz_add(Qn, Q0, tmp);
         mpz_set(Q0, t2);  /* remember last Q */
         mpz_add(tmp, b0, P);
         mpz_tdiv_q(bn, tmp, Qn);

         if (mpz_even_p(i)) {
           if (mpz_perfect_square_p(Qn)) {
             mpz_add_ui(i, i, 1);
             break;
           }
         }
         mpz_add_ui(i, i, 1);

         if (mpz_cmp(i, imax) >= 0) {
           result = 0;
           goto end;
         }
      }

      /* reduce to G0 */
      mpz_sqrt(S, Qn);
      mpz_sub(tmp, b0, P);
      mpz_tdiv_q(tmp, tmp, S);
      mpz_mul(tmp, S, tmp);
      mpz_add(Ro, P, tmp);
      mpz_mul(tmp, Ro, Ro);
      mpz_sub(tmp, n, tmp);
      mpz_tdiv_q(So, tmp, S);
      mpz_add(tmp, b0, Ro);
      mpz_tdiv_q(bbn, tmp, So);

      /* search for symmetry point */
      while (1) {
         mpz_set(t1, Ro);  /* hold Ro for this iteration */
         mpz_mul(tmp, bbn, So);
         mpz_sub(Ro, tmp, Ro);
         mpz_set(t2, So);  /* hold So for this iteration */
         mpz_sub(tmp, t1, Ro);
         mpz_mul(tmp, bbn, tmp);
         mpz_add(So, S, tmp);
         mpz_set(S, t2);   /* remember last S */
         mpz_add(tmp, b0, Ro);
         mpz_tdiv_q(bbn, tmp, So);

         /* check for symmetry point */
         if (mpz_cmp(Ro, t1) == 0)
            break;

         /* this gets stuck very rarely, but it does happen. */
         if (++j > 1000000000)
         {
            result = -1;
            goto end;
         }
      }

      mpz_gcd(t1, Ro, n);
      if (mpz_cmp_ui(t1, 1) > 0) {
         mpz_set(f, t1);
         /* gmp_printf("GMP SQUFOF found factor after %Zd/%lu rounds: %Zd\n", i, j, f); */
         result = 1;
         goto end;
      }
   }

   end:
   mpz_clear(b0);
   mpz_clear(bn);
   mpz_clear(imax);
   mpz_clear(tmp);
   mpz_clear(Q0);
   mpz_clear(Qn);
   mpz_clear(P);
   mpz_clear(i);
   mpz_clear(t1);
   mpz_clear(t2);
   mpz_clear(S);
   mpz_clear(Ro);
   mpz_clear(So);
   mpz_clear(bbn);
   return result;
}

int _GMP_squfof_factor(mpz_t n, mpz_t f, UV rounds)
{
   const UV multipliers[] = {
      3*5*7*11, 3*5*7, 3*5*11, 3*5, 3*7*11, 3*7, 5*7*11, 5*7,
      3*11,     3,     5*11,   5,   7*11,   7,   11,     1   };
   const size_t sz_mul = sizeof(multipliers)/sizeof(multipliers[0]);
   size_t i;
   int result;
   UV nmod4;
   mpz_t nm, t;

   TEST_FOR_2357(n, f);
   mpz_init(nm);
   mpz_init(t);
   mpz_set_ui(f, 1);
   nmod4 = mpz_tdiv_r_ui(nm, n, 4);

   for (i = 0; i < sz_mul; i++) {
      UV mult = multipliers[i];
      /* Only use multipliers where n*m = 3 mod 4 */
      if (nmod4 == (mult % 4))
        continue;
      /* Only run when 64*m^3 < n */
      mpz_set_ui(t, mult);
      mpz_pow_ui(t, t, 3);
      mpz_mul_ui(t, t, 64);
      if (mpz_cmp(t, n) >= 0)
        continue;
      /* Run with this multiplier */
      mpz_mul_ui(nm, n, mult);
      result = shanks_mult(nm, f);
      if (result == -1)
        break;
      if ( (result == 1) && (mpz_cmp_ui(f, mult) != 0) ) {
        unsigned long gcdf = mpz_gcd_ui(NULL, f, mult);
        mpz_divexact_ui(f, f, gcdf);
        if (mpz_cmp_ui(f, 1) > 0)
          break;
      }
   }
   mpz_clear(t);
   mpz_clear(nm);
   return (mpz_cmp_ui(f, 1) > 0);
}

/* See if n is a perfect power */
int _GMP_power_factor(mpz_t n, mpz_t f)
{
  if (mpz_perfect_power_p(n)) {
    unsigned long k;

    mpz_set_ui(f, 1);
    for (k = 2; mpz_sgn(f); k++) {
      if (mpz_root(f, n, k))
        return 1;
    }
    /* GMP says it's a perfect power, but we couldn't find an integer root? */
  }
  return 0;
}
