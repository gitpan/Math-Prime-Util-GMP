#!/usr/bin/env perl
use strict;
use warnings;

use Test::More;
use Math::Prime::Util::GMP qw/gcd lcm kronecker is_power valuation invmod
                              binomial gcdext vecsum/;
my $extra = defined $ENV{EXTENDED_TESTING} && $ENV{EXTENDED_TESTING};

my @gcds = (
  [ [], 0],
  [ [8], 8],
  [ [9,9], 9],
  [ [0,0], 0],
  [ [1, 0, 0], 1],
  [ [0, 0, 1], 1],
  [ [17,19], 1 ],
  [ [54,24], 6 ],
  [ [42,56], 14],
  [ [ 9,28], 1 ],
  [ [48,180], 12],
  [ [2705353758,2540073744,3512215098,2214052398], 18],
  [ [2301535282,3609610580,3261189640], 106],
  [ [694966514,510402262,195075284,609944479], 181],
  [ [294950648,651855678,263274296,493043500,581345426], 58 ],
  [ [-30,-90,90], 30],
  [ [-3,-9,-18], 3],
);

my @lcms = (
  [ [], 0],
  [ [8], 8],
  [ [9,9], 9],
  [ [0,0], 0],
  [ [1, 0, 0], 0],
  [ [0, 0, 1], 0],
  [ [17,19], 323 ],
  [ [54,24], 216 ],
  [ [42,56], 168],
  [ [ 9,28], 252 ],
  [ [48,180], 720],
  [ [36,45], 180],
  [ [-36,45], 180],
  [ [-36,-45], 180],
  [ [30,15,5], 30],
  [ [2,3,4,5], 60],
  [ [30245, 114552], 3464625240],
  [ [11926,78001,2211], 2790719778],
  [ [1426,26195,3289,8346], 4254749070],
);

my @kroneckers = (
  [ 109981, 737777,  1],
  [ 737779, 121080, -1],
  [-737779, 121080,  1],
  [ 737779,-121080, -1],
  [-737779,-121080, -1],
  [12345,331,-1],
  [1001,9907,-1],
  [19,45,1],
  [8,21,-1],
  [5,21,1],
  [5,1237,-1],
  [10, 49, 1],
  [123,4567,-1],
  [3,18,0], [3,-18,0],
  [-2, 0, 0],  [-1, 0, 1],  [ 0, 0, 0],  [ 1, 0, 1],  [ 2, 0, 0],
  [-2, 1, 1],  [-1, 1, 1],  [ 0, 1, 1],  [ 1, 1, 1],  [ 2, 1, 1],
  [-2,-1,-1],  [-1,-1,-1],  [ 0,-1, 1],  [ 1,-1, 1],  [ 2,-1, 1],
  # Some cases trying to make sure we're not turning UVs into IVs
  [ 3686556869,  428192857,  1],
  [-1453096827,  364435739, -1],
  [ 3527710253, -306243569, 1],
  [-1843526669, -332265377, 1],
  [  321781679, 4095783323, -1],
  [  454249403,  -79475159, -1],
);

my @valuations = (
  [-4,2, 2],
  [0,0, 0],
  [1,0, 0],
  [96552,6, 3],
  [1879048192,2, 28],
);
my @invmods = (
 [ 0, 0, undef],
 [ 1, 0, undef],
 [ 0, 1, undef],
 [ 1, 1, 0],
 [ 45, 59, 21],
 [  42,  2017, 1969],
 [  42, -2017, 1969],
 [ -42,  2017, 48],
 [ -42, -2017, 48],
 [ 14, 28474, undef],
);
my @binomials = (
 [ 0,0, 1 ],
 [ 0,1, 0 ],
 [ 1,0, 1 ],
 [ 1,1, 1 ],
 [ 1,2, 0 ],
 [ 13,13, 1 ],
 [ 13,14, 0 ],
 [ 35,16, 4059928950 ],             # We can do this natively even in 32-bit
 [ 40,19, "131282408400" ],         # We can do this in 64-bit
 [ 67,31, "11923179284862717872" ], # ...and this
 [ 228,12, "30689926618143230620" ],# But the result of this is too big.
 [ 177,78, "3314450882216440395106465322941753788648564665022000" ],
 [ -10,5, -2002 ],
 [ -11,22, 64512240 ],
 [ -12,23, -286097760 ],
 [ -23,-26, -2300 ],     # Kronenburg extension
 [ -12,-23, -705432 ],   # same
 [  12,-23, 0 ],
 [  12,-12, 0 ],
 [ -12,0, 1 ],
 [  0,-1, 0 ],
);

my @gcdexts = (
  [ [0, 28], [0, 1,28] ],
  [ [ 28,0], [ 1,0,28] ],
  [ [0,-28], [0,-1,28] ],
  [ [-28,0], [-1,0,28] ],
  [ [ 3706259912, 1223661804], [ 123862139,-375156991, 4] ],
  [ [ 3706259912,-1223661804], [ 123862139, 375156991, 4] ],
  [ [-3706259912, 1223661804], [-123862139,-375156991, 4] ],
  [ [-3706259912,-1223661804], [-123862139, 375156991, 4] ],
  [ [22,242], [1, 0, 22] ],
  [ [2731583792,3028241442], [-187089956, 168761937, 2] ],
  [ [42272720,12439910], [-21984, 74705, 70] ],
  [ ["10139483024654235947","8030280778952246347"], ["-2715309548282941287","3428502169395958570",1] ],
);

my @vecsums = (
  [ 0 ],
  [ -1, -1 ],
  [ 0, 1,-1 ],
  [ 0, -1,1 ],
  [ 0, -1,1 ],
  [ 0, -2147483648,2147483648 ],
  [ 0, "-4294967296","4294967296" ],
  [ 0, "-9223372036854775808","9223372036854775808" ],
  [ "18446744073709551615", "18446744073709551615","-18446744073709551615","18446744073709551615" ],
  [ "55340232221128654848", "18446744073709551616","18446744073709551616","18446744073709551616" ],
  [ "18446744073709620400", 18446744073709540400, (1000) x 80 ],
  [ "22229615424432722482764646042115836201380906995100292325888852211992579", "8940560560432415123818915720822415267807123681474252424566821897853531", "7778547618243732438765515250718016989143156212607337512983395245244477", "2189527014402679437989261998352299668199802723705390949617417617818071", "-3503124593441728232550096334002786135023", "3320980231353895482190953072226607400824266105545861535055220237211523" ],
);

plan tests => scalar(@gcds)
            + scalar(@lcms)
            + scalar(@kroneckers)
            + scalar(@valuations)
            + scalar(@invmods)
            + 2 + scalar(@binomials)
            + scalar(@gcdexts)
            + scalar(@vecsums)
            + 3 + 3 + 1 + 5;

foreach my $garg (@gcds) {
  my($aref, $exp) = @$garg;
  my $gcd = gcd(@$aref);
  is( $gcd, $exp, "gcd(".join(",",@$aref).") = $exp" );
}

foreach my $garg (@lcms) {
  my($aref, $exp) = @$garg;
  my $lcm = lcm(@$aref);
  is( $lcm, $exp, "lcm(".join(",",@$aref).") = $exp" );
}

foreach my $karg (@kroneckers) {
  my($a, $n, $exp) = @$karg;
  my $k = kronecker($a, $n);
  is( $k, $exp, "kronecker($a, $n) = $exp" );
}

foreach my $r (@valuations) {
  my($n, $k, $exp) = @$r;
  is( valuation($n, $k), $exp, "valuation($n,$k) = $exp" );
}

foreach my $r (@invmods) {
  my($a, $n, $exp) = @$r;
  is( invmod($a,$n), $exp, "invmod($a,$n) = ".((defined $exp)?$exp:"<undef>") );
}

foreach my $r (@binomials) {
  my($n, $k, $exp) = @$r;
  is( binomial($n,$k), $exp, "binomial($n,$k)) = $exp" );
}
is_deeply( [map { binomial(10, $_) } -15 .. 15],
           [qw/0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 10 45 120 210 252 210 120 45 10 1 0 0 0 0 0/],
           "binomial(10,n) for n in -15 .. 15" );
is_deeply( [map { binomial(-10, $_) } -15 .. 15],
           [qw/-2002 715 -220 55 -10 1 0 0 0 0 0 0 0 0 0 1 -10 55 -220 715 -2002 5005 -11440 24310 -48620 92378 -167960 293930 -497420 817190 -1307504/],
           "binomial(-10,n) for n in -15 .. 15" );

foreach my $garg (@gcdexts) {
  my($aref, $eref) = @$garg;
  my($x,$y) = @$aref;
  is_deeply( [gcdext($x,$y)], $eref, "gcdext($x,$y) = [@$eref]" );
}

foreach my $r (@vecsums) {
  my($exp, @vals) = @$r;
  is( vecsum(@vals), $exp, "vecsum(@vals) = $exp" );
}

is( gcd("921166566073002915606255698642","1168315374100658224561074758384","951943731056111403092536868444"), 14, "gcd(a,b,c)" );
is( gcd("1214969109355385138343690512057521757303400673155500334102084","1112036111724848964580068879654799564977409491290450115714228"), 42996, "gcd(a,b)" );
is( gcd("745845206184162095041321","61540282492897317017092677682588744425929751009997907259657808323805386381007"), 1, "gcd of two primes = 1" );

is( lcm("9999999998987","10000000001011"), "99999999999979999998975857", "lcm(p1,p2)" );
is( lcm("892478777297173184633","892478777297173184633"), "892478777297173184633", "lcm(p1,p1)" );
is( lcm("23498324","32497832409432","328732487324","328973248732","3487234897324"), "1124956497899814324967019145509298020838481660295598696", "lcm(a,b,c,d,e)" );

is( kronecker("878944444444444447324234","216539985579699669610468715172511426009"), -1, "kronecker(..., ...)" );

is( is_power("18475335773296164196"), "0", "is_power(18475335773296164196) == 0" );
is( is_power("1415842012524355766113858481287417543594447475938337587864641453047142843853822559252126433860162253504357722982805134804808530350591698526668732807053601"), "18", "is_power(322396049^18) == 18" );
is( is_power("195820481042341245090221890868767224469265867337457650976172728836917821923718632978263135461761"), "16", "is_power(903111^16) == 16" );
ok( is_power("195820481042341245090221890868767224469265867337457650976172728836917821923718632978263135461761",4), "is_power(903111^16,4) is true" );
is( is_power("894311843364148115560351871258324837202590615410044436950984649"), "2", "is_power(29905047121918201644964877983907^2) == 2" );
