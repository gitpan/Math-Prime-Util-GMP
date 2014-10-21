#!/usr/bin/env perl
use strict;
use warnings;

use Test::More;
use Math::Prime::Util::GMP qw/is_strong_pseudoprime is_strong_lucas_pseudoprime is_prime/;
my $extra = defined $ENV{EXTENDED_TESTING} && $ENV{EXTENDED_TESTING};

# pseudoprimes from 2-100k for many bases

my %pseudoprimes = (
   2 => [ qw/2047 3277 4033 4681 8321 15841 29341 42799 49141 52633 65281 74665 80581 85489 88357 90751 1194649/ ],
   3 => [ qw/121 703 1891 3281 8401 8911 10585 12403 16531 18721 19345 23521 31621 44287 47197 55969 63139 74593 79003 82513 87913 88573 97567/ ],
   5 => [ qw/781 1541 5461 5611 7813 13021 14981 15751 24211 25351 29539 38081 40501 44801 53971 79381/ ],
   7 => [ qw/25 325 703 2101 2353 4525 11041 14089 20197 29857 29891 39331 49241 58825 64681 76627 78937 79381 87673 88399 88831/ ],
  11 => [ qw/133 793 2047 4577 5041 12403 13333 14521 17711 23377 43213 43739 47611 48283 49601 50737 50997 56057 58969 68137 74089 85879 86347 87913 88831/ ],
  13 => [ qw/85 1099 5149 7107 8911 9637 13019 14491 17803 19757 20881 22177 23521 26521 35371 44173 45629 54097 56033 57205 75241 83333 85285 86347/ ],
  17 => [ qw/9 91 145 781 1111 2821 4033 4187 5365 5833 6697 7171 15805 19729 21781 22791 24211 26245 31621 33001 33227 34441 35371 38081 42127 49771 71071 74665 77293 78881 88831 96433 97921 98671/ ],
  19 => [ qw/9 49 169 343 1849 2353 2701 4033 4681 6541 6697 7957 9997 12403 13213 13747 15251 16531 18769 19729 24761 30589 31621 31861 32477 41003 49771 63139 64681 65161 66421 68257 73555 96049/ ],
  23 => [ qw/169 265 553 1271 2701 4033 4371 4681 6533 6541 7957 8321 8651 8911 9805 14981 18721 25201 31861 34133 44173 47611 47783 50737 57401 62849 82513 96049/ ],
  29 => [ qw/15 91 341 469 871 2257 4371 4411 5149 6097 8401 11581 12431 15577 16471 19093 25681 28009 29539 31417 33001 48133 49141 54913 79003/ ],
  31 => [ qw/15 49 133 481 931 6241 8911 9131 10963 11041 14191 17767 29341 56033 58969 68251 79003 83333 87061 88183/ ],
  37 => [ qw/9 451 469 589 685 817 1333 3781 8905 9271 18631 19517 20591 25327 34237 45551 46981 47587 48133 59563 61337 68101 68251 73633 79381 79501 83333 84151 96727/ ],
         61 => [ qw/217 341 1261 2701 3661 6541 6697 7613 13213 16213 22177 23653 23959 31417 50117 61777 63139 67721 76301 77421 79381 80041/ ],
         73 => [ qw/205 259 533 1441 1921 2665 3439 5257 15457 23281 24617 26797 27787 28939 34219 39481 44671 45629 64681 67069 76429 79501 93521/ ],
        325 => [ qw/341 343 697 1141 2059 2149 3097 3537 4033 4681 4941 5833 6517 7987 8911 12403 12913 15043 16021 20017 22261 23221 24649 24929 31841 35371 38503 43213 44173 47197 50041 55909 56033 58969 59089 61337 65441 68823 72641 76793 78409 85879/ ],
       9375 => [ qw/11521 14689 17893 18361 20591 28093 32809 37969 44287 60701 70801 79957 88357 88831 94249 96247 99547/ ],
      28178 => [ qw/28179 29381 30353 34441 35371 37051 38503 43387 50557 51491 57553 79003 82801 83333 87249 88507 97921 99811/ ],
      75088 => [ qw/75089 79381 81317 91001 100101 111361 114211 136927 148289 169641 176661 191407 195649/ ],
     450775 => [ qw/465991 468931 485357 505441 536851 556421 578771 585631 586249 606361 631651 638731 641683 645679/ ],
     642735 => [ qw/653251 653333 663181 676651 714653 759277 794683 805141 844097 872191 874171 894671/ ],
    9780504 => [ qw/9780505 9784915 9826489 9882457 9974791 10017517 10018081 10084177 10188481 10247357 10267951 10392241 10427209 10511201/ ],
  203659041 => [ qw/204172939 204456793 206407057 206976001 207373483 209301121 210339397 211867969 212146507 212337217 212355793 214400629 214539841 215161459/ ],
  553174392 => [ qw/553174393 553945231 554494951 554892787 555429169 557058133 557163157 557165209 558966793 559407061 560291719 561008251 563947141/ ],
 1005905886 => [ qw/1005905887 1007713171 1008793699 1010415421 1010487061 1010836369 1012732873 1015269391 1016250247 1018405741 1020182041/ ],
 1340600841 => [ qw/1345289261 1345582981 1347743101 1348964401 1350371821 1353332417 1355646961 1357500901 1361675929 1364378203 1366346521 1367104639/ ],
 1795265022 => [ qw/1795265023 1797174457 1797741901 1804469753 1807751977 1808043283 1808205701 1813675681 1816462201 1817936371 1819050257/ ],
 3046413974 => [ qw/3046413975 3048698683 3051199817 3068572849 3069705673 3070556233 3079010071 3089940811 3090723901 3109299161 3110951251 3113625601/ ],
 3613982119 => [ qw/3626488471 3630467017 3643480501 3651840727 3653628247 3654142177 3672033223 3672036061 3675774019 3687246109 3690036017 3720856369/ ],
 lucas      => [ qw/5459 5777 10877 16109 18971 22499 24569 25199 40309 58519 75077 97439/ ],
);
my $num_pseudoprimes = 0;
foreach my $ppref (values %pseudoprimes) {
  $num_pseudoprimes += scalar @$ppref;
}
my @small_lucas_trials = (2, 9, 16, 100, 102, 2047, 2048, 5781, 9000, 14381);

my %large_pseudoprimes = (
   '75792980677' => [ qw/2/ ],
   '21652684502221' => [ qw/2 7 37 61 9375/ ],
   '3825123056546413051' => [ qw/2 3 5 7 11 13 17 19 23 29 31 325 9375/ ],
   '318665857834031151167461' => [ qw/2 3 5 7 11 13 17 19 23 29 31 37 325 9375/ ],
   '3317044064679887385961981' => [ qw/2 3 5 7 11 13 17 19 23 29 31 37 73 325 9375/ ],
   '6003094289670105800312596501' => [ qw/2 3 5 7 11 13 17 19 23 29 31 37 61 325 9375/ ],
   '59276361075595573263446330101' => [ qw/2 3 5 7 11 13 17 19 23 29 31 37 325 9375/ ],
   '564132928021909221014087501701' => [ qw/2 3 5 7 11 13 17 19 23 29 31 37 325 9375/ ],
);
my $num_large_pseudoprime_tests = 0;
foreach my $psrp (keys %large_pseudoprimes) {
  $num_large_pseudoprime_tests += scalar @{$large_pseudoprimes{$psrp}};
}


plan tests => 0 + 9
                + 3
                + 6
                + $num_pseudoprimes
                + 1  # mr base 2    2-4k
                + 9  # mr with large bases
                + scalar @small_lucas_trials
              # + $num_large_pseudoprime_tests
                + 6*$extra  # Large Carmichael numbers
                + 0;

eval { is_strong_pseudoprime(2047); };
like($@, qr/no base/i, "is_strong_pseudoprime with no base fails");
eval { is_strong_pseudoprime(2047, undef); };
like($@, qr/defined/i, "is_strong_pseudoprime with base undef fails");
eval { is_strong_pseudoprime(2047, ''); };
like($@, qr/positive/i, "is_strong_pseudoprime with base '' fails");
eval { is_strong_pseudoprime(2047,0); };
like($@, qr/invalid/i, "is_strong_pseudoprime with base 0 fails");
eval { is_strong_pseudoprime(2047,1); };
like($@, qr/invalid/i, "is_strong_pseudoprime with base 1 fails");
eval { is_strong_pseudoprime(2047,-7); };
like($@, qr/positive/i, "is_strong_pseudoprime with base -7 fails");
eval { is_strong_pseudoprime(undef, 2); };
like($@, qr/defined/i, "is_strong_pseudoprime(undef,2) is invalid");
eval { is_strong_pseudoprime('', 2); };
like($@, qr/positive/i, "is_strong_pseudoprime('',2) is invalid");
eval { is_strong_pseudoprime(-7, 2); };
like($@, qr/positive/i, "is_strong_pseudoprime(-7,2) is invalid");

eval { no warnings; is_strong_lucas_pseudoprime(undef); };
like($@, qr/empty string/i, "is_strong_lucas_pseudoprime(undef) is invalid");
eval { is_strong_lucas_pseudoprime(''); };
like($@, qr/empty string/i, "is_strong_lucas_pseudoprime('') is invalid");
eval { is_strong_lucas_pseudoprime(-7); };
like($@, qr/integer/i, "is_strong_lucas_pseudoprime(-7) is invalid");


is( is_strong_pseudoprime(0, 2), 0, "spsp(0, 2) shortcut composite");
is( is_strong_pseudoprime(1, 2), 0, "spsp(1, 2) shortcut composite");
is( is_strong_pseudoprime(2, 2), 1, "spsp(2, 2) shortcut prime");
is( is_strong_pseudoprime(3, 2), 1, "spsp(2, 2) shortcut prime");
is( is_strong_lucas_pseudoprime(1), 0, "slpsp(1) shortcut composite");
is( is_strong_lucas_pseudoprime(3), 1, "slpsp(3) shortcut prime");

# Check that each strong pseudoprime base b makes it through MR with that base
while (my($base, $ppref) = each (%pseudoprimes)) {
  foreach my $p (@$ppref) {
    if ($base eq 'lucas') {
      ok(is_strong_lucas_pseudoprime($p), "$p is a strong Lucas-Selfridge pseudoprime");
    } else {
      ok(is_strong_pseudoprime($p, $base), "Pseudoprime (base $base) $p passes MR");
    }
  }
}

# Verify MR base 2 for all small numbers
{
  my $mr2fail = 0;
  for (2 .. 4032) {
    next if $_ == 2047 || $_ == 3277;
    if (is_prime($_)) {
      if (!is_strong_pseudoprime($_,2)) { $mr2fail = $_; last; }
    } else {
      if (is_strong_pseudoprime($_,2))  { $mr2fail = $_; last; }
    }
  }
  is($mr2fail, 0, "MR base 2 matches is_prime for 2-4032 (excl 2047,3277)");
}

# Verify MR for bases >= n
is( is_strong_pseudoprime(  3,    3), 1, "spsp(  3,    3)");
is( is_strong_pseudoprime( 11,   11), 1, "spsp( 11,   11)");
is( is_strong_pseudoprime( 89, 5785), 1, "spsp( 89, 5785)");
is( is_strong_pseudoprime(257, 6168), 1, "spsp(257, 6168)");
is( is_strong_pseudoprime(367,  367), 1, "spsp(367,  367)");
is( is_strong_pseudoprime(367, 1101), 1, "spsp(367, 1101)");
is( is_strong_pseudoprime(49001, 921211727), 0, "spsp(49001, 921211727)");
is( is_strong_pseudoprime(  331, 921211727), 1, "spsp(  331, 921211727)");
is( is_strong_pseudoprime(49117, 921211727), 1, "spsp(49117, 921211727)");

# Verify Lucas for some small numbers
for my $n (@small_lucas_trials) {
  next if $n == 5459 || $n == 5777 || $n == 10877 || $n == 16109 || $n == 18971;
  if (is_prime($n)) {
    # Technically if it is a prime it isn't a pseudoprime.
    ok(is_strong_lucas_pseudoprime($n), "$n is a prime and a strong Lucas-Selfridge pseudoprime");
  } else {
    ok(!is_strong_lucas_pseudoprime($n), "$n is not a prime and not a strong Lucas-Selfridge pseudoprime");
  }
}

if ($extra) {
  my $n;
  # Zhang 2004
  $n = "9688312712744590973050578123260748216127001625571";
  is( is_strong_pseudoprime($n, 2..70), 1, "968..571 is spsp(1..70)" );
  is( is_strong_pseudoprime($n, 71),    0, "968..571 is found composite by base 71" );
  # Bleichenbacher
  $n = "68528663395046912244223605902738356719751082784386681071";
  is( is_strong_pseudoprime($n, 2..100), 1, "685..071 is spsp(1..100)" );
  is( is_strong_pseudoprime($n, 101),    0, "685..071 is found composite by base 101" );
  # Arnault 1994, strong pseudoprime to all bases up to 306
  #my $p1 = Math::BigInt->new("29674495668685510550154174642905332730771991799853043350995075531276838753171770199594238596428121188033664754218345562493168782883");
  #$n = $p1* (313 * ($p1-1) + 1) * (353 * ($p1-1) + 1);
  $n = "2887148238050771212671429597130393991977609459279722700926516024197432303799152733116328983144639225941977803110929349655578418949441740933805615113979999421542416933972905423711002751042080134966731755152859226962916775325475044445856101949404200039904432116776619949629539250452698719329070373564032273701278453899126120309244841494728976885406024976768122077071687938121709811322297802059565867";
  is( is_strong_pseudoprime($n, 2..306), 1, "Arnault-397 Carmichael is spsp(1..306)" );
  is( is_strong_pseudoprime($n, 307), 0, "Arnault-397 Carmichael is found composite by base 307" );
}
