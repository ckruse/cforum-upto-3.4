/*
 * Code based on <http://www.macchiato.com/unicode/Nameprep.html> by Mark Davis
 *
 *
 * ICU License
 *
 * COPYRIGHT AND PERMISSION NOTICE
 *
 * Copyright (c) 1995-2005 International Business Machines Corporation and others
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons
 * to whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
 * OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
 * INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 * Except as contained in this notice, the name of a copyright holder
 * shall not be used in advertising or otherwise to promote the sale, use
 * or other dealings in this Software without prior written authorization
 * of the copyright holder.
 *
 * --------------------------------------------------------------------------------
 * All trademarks and registered trademarks mentioned herein are the property of their respective owners.
 */

function Nameprep() {
}

Nameprep.prototype.forbidden = new NameprepInversion([
  0x0, 0x2D, 0x2E, 0x30, 0x3A, 0x41,
  0x5B, 0x61, 0x7B, 0xA1, 0x1680, 0x1681,
  0x2000, 0x200C, 0x200E, 0x2010, 0x2028,
  0x2030, 0x206A, 0x2070, 0x2FF0, 0x3001,
  0xD800, 0xF900, 0xFFF9, 0x10000, 0x1FFFE,
  0x20000, 0x2FFFE, 0x30000, 0x3FFFE,
  0x40000, 0x4FFFE, 0x50000, 0x5FFFE,
  0x60000, 0x6FFFE, 0x70000, 0x7FFFE,
  0x80000, 0x8FFFE, 0x90000, 0x9FFFE,
  0xA0000, 0xAFFFE, 0xB0000, 0xBFFFE,
  0xC0000, 0xCFFFE, 0xD0000, 0xDFFFE,
  0xE0000, 0xEFFFE, 0x110000
]);

Nameprep.prototype.whitespace = new NameprepInversion([
  0x0009, 0x000E,
  0x0020,,        /* SPACE */
  0x0085,,        /* <control> */
  0x00A0,,        /* NO-BREAK SPACE */
  0x1680,,        /* OGHAM SPACE MARK */
  0x2000, 0x200C, /* EN QUAD.. */
  0x2028, 0x202A, /* PARAGRAPH SEPARATOR */
  0x202F,,        /* NARROW NO-BREAK SPACE */
  0x3000          /* IDEOGRAPHIC SPACE */
]);

Nameprep.prototype.dashes = new NameprepInversion([
  0x002D, 0x002E, /* HYPHEN-MINUS */
  0x00AD,0x00AE,  /* SOFT HYPHEN */
  0x058A, 0x058B, /* ARMENIAN HYPHEN */
  0x1806, 0x1807, /* MONGOLIAN TODO SOFT HYPHEN */
  0x2010, 0x2015, /* HYPHEN.. */
  0x207B, 0x207C, /* SUPERSCRIPT MINUS */
  0x208B, 0x208D, /* SUBSCRIPT MINUS */
  0x2212, 0x2213, /* MINUS SIGN */
  0x301C, 0x301d, /* WAVE DASH */
  0x3030, 0x3031, /* WAVY DASH */
  0xFE31, 0xFE32, /* PRESENTATION FORM FOR VERTICAL EM DASH.. */
  0xFE58, 0xFE59, /* SMALL EM DASH */
  0xFE63, 0xFE64, /* SMALL HYPHEN-MINUS */
  0xFF0D, 0xFF0E  /* FULLWIDTH HYPHEN-MINUS */
]);

Nameprep.prototype.noncharacter = new NameprepInversion([
  0xD800,0xE000, 0xFFFE,0x10000, 0x1FFFE,0x20000, 0x2FFFE,0x30000,
  0x3FFFE,0x40000, 0x4FFFE,0x50000, 0x5FFFE,0x60000, 0x6FFFE,0x70000,
  0x7FFFE,0x80000, 0x8FFFE,0x90000, 0x9FFFE,0xA0000, 0xAFFFE,0xB0000,
  0xBFFFE,0xC0000, 0xCFFFE,0xD0000, 0xDFFFE,0xE0000, 0xEFFFE,0xF0000,
  0xFFFFE,0x100000, 0x10FFFE,0x110000
]);

Nameprep.prototype.privateUse = new NameprepInversion([
  0xE000,  0xF900,
  0xF0000, 0xFFFFE,
  0x100000,0x10FFFE
]);

Nameprep.prototype.assigned = new NameprepInversion([
  0,544, 546,564, 592,686, 688,751, 768,847, 864,867, 884,886, 890,891, 894,895, 900,907, 908,909, 910,930, 931,975, 976,984, 986,1012, 1024,1159,
  1160,1162, 1164,1221, 1223,1225, 1227,1229, 1232,1270, 1272,1274, 1329,1367, 1369,1376, 1377,1416, 1417,1419, 1425,1442, 1443,1466, 1467,1477,
  1488,1515, 1520,1525, 1548,1549, 1563,1564, 1567,1568, 1569,1595, 1600,1622, 1632,1646, 1648,1774, 1776,1791, 1792,1806, 1807,1837, 1840,1867,
  1920,1969, 2305,2308, 2309,2362, 2364,2382, 2384,2389, 2392,2417, 2433,2436, 2437,2445, 2447,2449, 2451,2473, 2474,2481, 2482,2483, 2486,2490,
  2492,2493, 2494,2501, 2503,2505, 2507,2510, 2519,2520, 2524,2526, 2527,2532, 2534,2555, 2562,2563, 2565,2571, 2575,2577, 2579,2601, 2602,2609,
  2610,2612, 2613,2615, 2616,2618, 2620,2621, 2622,2627, 2631,2633, 2635,2638, 2649,2653, 2654,2655, 2662,2677, 2689,2692, 2693,2700, 2701,2702,
  2703,2706, 2707,2729, 2730,2737, 2738,2740, 2741,2746, 2748,2758, 2759,2762, 2763,2766, 2768,2769, 2784,2785, 2790,2800, 2817,2820, 2821,2829,
  2831,2833, 2835,2857, 2858,2865, 2866,2868, 2870,2874, 2876,2884, 2887,2889, 2891,2894, 2902,2904, 2908,2910, 2911,2914, 2918,2929, 2946,2948,
  2949,2955, 2958,2961, 2962,2966, 2969,2971, 2972,2973, 2974,2976, 2979,2981, 2984,2987, 2990,2998, 2999,3002, 3006,3011, 3014,3017, 3018,3022,
  3031,3032, 3047,3059, 3073,3076, 3077,3085, 3086,3089, 3090,3113, 3114,3124, 3125,3130, 3134,3141, 3142,3145, 3146,3150, 3157,3159, 3168,3170,
  3174,3184, 3202,3204, 3205,3213, 3214,3217, 3218,3241, 3242,3252, 3253,3258, 3262,3269, 3270,3273, 3274,3278, 3285,3287, 3294,3295, 3296,3298,
  3302,3312, 3330,3332, 3333,3341, 3342,3345, 3346,3369, 3370,3386, 3390,3396, 3398,3401, 3402,3406, 3415,3416, 3424,3426, 3430,3440, 3458,3460,
  3461,3479, 3482,3506, 3507,3516, 3517,3518, 3520,3527, 3530,3531, 3535,3541, 3542,3543, 3544,3552, 3570,3573, 3585,3643, 3647,3676, 3713,3715,
  3716,3717, 3719,3721, 3722,3723, 3725,3726, 3732,3736, 3737,3744, 3745,3748, 3749,3750, 3751,3752, 3754,3756, 3757,3770, 3771,3774, 3776,3781,
  3782,3783, 3784,3790, 3792,3802, 3804,3806, 3840,3912, 3913,3947, 3953,3980, 3984,3992, 3993,4029, 4030,4045, 4047,4048, 4096,4130, 4131,4136,
  4137,4139, 4140,4147, 4150,4154, 4160,4186, 4256,4294, 4304,4343, 4347,4348, 4352,4442, 4447,4515, 4520,4602, 4608,4615, 4616,4679, 4680,4681,
  4682,4686, 4688,4695, 4696,4697, 4698,4702, 4704,4743, 4744,4745, 4746,4750, 4752,4783, 4784,4785, 4786,4790, 4792,4799, 4800,4801, 4802,4806,
  4808,4815, 4816,4823, 4824,4847, 4848,4879, 4880,4881, 4882,4886, 4888,4895, 4896,4935, 4936,4955, 4961,4989, 5024,5109, 5121,5751, 5760,5789,
  5792,5873, 6016,6109, 6112,6122, 6144,6159, 6160,6170, 6176,6264, 6272,6314, 7680,7836, 7840,7930, 7936,7958, 7960,7966, 7968,8006, 8008,8014,
  8016,8024, 8025,8026, 8027,8028, 8029,8030, 8031,8062, 8064,8117, 8118,8133, 8134,8148, 8150,8156, 8157,8176, 8178,8181, 8182,8191, 8192,8263,
  8264,8270, 8298,8305, 8308,8335, 8352,8368, 8400,8420, 8448,8507, 8531,8580, 8592,8692, 8704,8946, 8960,9084, 9085,9115, 9216,9255, 9280,9291,
  9312,9451, 9472,9622, 9632,9720, 9728,9748, 9753,9842, 9985,9989, 9990,9994, 9996,10024, 10025,10060, 10061,10062, 10063,10067, 10070,10071,
  10072,10079, 10081,10088, 10102,10133, 10136,10160, 10161,10175, 10240,10496, 11904,11930, 11931,12020, 12032,12246, 12272,12284, 12288,12347,
  12350,12352, 12353,12437, 12441,12447, 12449,12543, 12549,12589, 12593,12687, 12688,12728, 12800,12829, 12832,12868, 12896,12924, 12927,12977,
  12992,13004, 13008,13055, 13056,13175, 13179,13278, 13280,13311, 13312,19894, 19968,40870, 40960,42125, 42128,42146, 42148,42164, 42165,42177,
  42178,42181, 42182,42183, 44032,55204, 55296,64046, 64256,64263, 64275,64280, 64285,64311, 64312,64317, 64318,64319, 64320,64322, 64323,64325,
  64326,64434, 64467,64832, 64848,64912, 64914,64968, 65008,65020, 65056,65060, 65072,65093, 65097,65107, 65108,65127, 65128,65132, 65136,65139,
  65140,65141, 65142,65277, 65279,65280, 65281,65375, 65377,65471, 65474,65480, 65482,65488, 65490,65496, 65498,65501, 65504,65511, 65512,65519,
  65529,65534, 0xF0000
]);

Nameprep.prototype.unassigned = Nameprep.prototype.assigned.makeOpposite();

Nameprep.prototype.prepare = function(str,cf,n,f) {
  if(cf) str = this.fold(str);
  if(n)  str = this.nfkc(str);

  if(f) {
    if(this.filter(str) == -1) return str;
    else return false;
  }

  return str;
}

/* eof */
