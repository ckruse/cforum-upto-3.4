/**
 * punycode implementation, artistic license
 */

function Punycode() {
}

Punycode.prototype.base         = 36;
Punycode.prototype.tmin         = 1;
Punycode.prototype.tmax         = 26;
Punycode.prototype.skew         = 38;
Punycode.prototype.damp         = 700;
Punycode.prototype.initial_bias = 72;
Punycode.prototype.initial_n    = 0x80;
Punycode.prototype.delimiter    = 0x2D;
Punycode.prototype.maxint = 0xFFFFFFFF;

Punycode.prototype.basic = function(cp) {
  return toUint32(cp) < 0x80;
}

Punycode.prototype.delim = function(cp) {
  return cp == this.delimiter;
}

Punycode.prototype.decode_digit = function(cp) {
  return  cp - 48 < 10 ? cp - 22 :  cp - 65 < 26 ? cp - 65 :
          cp - 97 < 26 ? cp - 97 :  this.base;
}


Punycode.prototype.encode_digit = function(d,flag) {
  /*  0..25 map to ASCII a..z or A..Z */
  /* 26..35 map to ASCII 0..9         */
  return String.fromCharCode(d + 22 + 75 * (d < 26) - ((flag != 0) << 5));
}

Punycode.prototype.flagged = function(bcp) {
  return toUint32(bcp) - 65 < 26;
}

Punycode.prototype.encode_basic = function(bcp,flag) {
  bcp -= (bcp - 97 < 26) << 5;
  return String.fromCharCode(bcp + ((!flag && (bcp - 65 < 26)) << 5));
}

Punycode.prototype.adapt = function(delta,numpoints,firsttime) {
  var k;

  delta  = firsttime ? Math.floor(delta / this.damp) : delta >> 1;
  delta += Math.floor(delta / numpoints);

  for (k=0;delta > ((this.base - this.tmin) * this.tmax) / 2;k+=base) delta = Math.floor(delta / (this.base - this.tmin));

  return Math.floor(k + (this.base - this.tmin + 1) * delta / (delta + this.skew));
}



Punycode.prototype.encode = function(inpt,case_flags) {
  var n, delta, h, b, output = "", max_out, bias, j, m, q, k, t, out = 0;

  /* Initialize the state: */
  n       = this.initial_n;
  delta   = out = 0;
  bias    = this.initial_bias;

  /* Handle the basic code points: */
  for(j=0;j<inpt.length;++j) {
    var c = inpt.charCodeAt(j);
    if(this.basic(c)) {
      ++out;
      output += String.fromCharCode(case_flags ?  encode_basic(c,case_flags[j]) : c);
    }
  }

  h = b = out;

  /* h is the number of code points that have been handled, b is the
   * number of basic code points, and out is the number of characters
   * that have been output.
   */

  if(b > 0) output += String.fromCharCode(this.delimiter);

  /* Main encoding loop: */

  while(h < inpt.length) {
    /* All non-basic code points < n have been
     * handled already.  Find the next larger one:
     */
    for(m=this.maxint,j=0;j<inpt.length;++j) {
      var c = inpt.charCodeAt(j);
      if (c >= n && c < m) m = c;
    }

    /* Increase delta enough to advance the decoder's
     * <n,i> state to <m,0>, but guard against overflow:
     */
    if(m - n > (this.maxint - delta) / (h + 1)) return false;
    delta += (m - n) * (h + 1);
    n = m;

    for(j=0;j<inpt.length;++j) {
      var c = inpt.charCodeAt(j);
      if(c < n) {
        if(++delta == 0) return false;
      }

      if(c == n) {
        /* Represent delta as a generalized variable-length integer: */
        for(q=delta,k=this.base;;k+=this.base) {
          t = k <= bias ? this.tmin : k >= bias + this.tmax ? this.tmax : k - bias;
          if(q < t) break;

          output += this.encode_digit(t + (q - t) % (this.base - t), 0);
          ++out;
          q = Math.floor((q - t) / (this.base - t));
        }

        out++;
        output += this.encode_digit(q,case_flags?case_flags[j]?case_flags[j]:0:0);
        bias = this.adapt(delta, h + 1, h == b);
        delta = 0;
        ++h;
      }
    }

    ++delta, ++n;
  }

  return output;
}

Punycode.prototype.decode = function(inpt,case_flags) {
  var n, out, i, max_out, bias, b, j, inval, oldi, w, k, digit, t,output = "";

  /* Initialize the state: */
  n    = this.initial_n;
  out  = i = 0;
  bias = this.initial_bias;

  /* Handle the basic code points:  Let b be the number of input code
   * points before the last delimiter, or 0 if there is none, then
   * copy the first b code points to the output.
   */
  for(b=j=0;j<inpt.length;++j) if(this.delim(inpt.charCodeAt(j))) b = j;

  for(j=0,out=0;j<b;++j,out++) {
    var c = inpt.charCodeAt(j);

    if(case_flags) case_flags[out] = this.flagged(c);
    if(!this.basic(c)) return false;

    output += String.fromCharCode(c);
  }

  /* Main decoding loop:  Start just after the last delimiter if any
   * basic code points were copied; start at the beginning otherwise.
   */
  for (inval=b>0?b+1:0;inval<inpt.length;++out) {
    /* Decode a generalized variable-length integer into delta,
     * which gets added to i.  The overflow checking is easier
     * if we increase i as we go, then subtract off its starting
     * value at the end to obtain delta.
     */
    for(oldi=i,w=1,k=this.base;;k+=this.base) {
      if(inval >= inpt.length) return false;

      digit = this.decode_digit(inpt.charCodeAt(inval++));
      if(digit >= this.base) return false;
      if(digit > (this.maxint - i) / w) return false;

      i += digit * w;
      t  = k <= bias ? this.tmin : k >= bias + this.tmax ? this.tmax : k - bias;

      if(digit < t) break;
      if(w > this.maxint / (this.base - t)) return false;

      w *= this.base - t;
    }

    bias = this.adapt(i - oldi, out + 1, oldi == 0);

    /* i was supposed to wrap around from out+1 to 0,
     * incrementing n each time, so we'll fix that now: */
    if(i / (out + 1) > this.maxint - n) return false;
    n += Math.floor(i / (out + 1));
    i %= (out + 1);

    /* Insert n at position i of the output: */
    if (case_flags) {
      // memmove(case_flags + i + 1, case_flags + i, out - i);
      for(var z=i+1;z<out;++z) case_flags[z-1] = case_flags[z];

      /* Case of last character determines uppercase flag: */
      case_flags[i] = this.flagged(inpt.charAt(inval - 1));
    }

    // memmove(output + i + 1, output + i, (out - i) * sizeof *output);
    // output[i++] = n;
    //alert(output.substring(0,i) + ", " + String.fromCharCode(n) + ", " + output.substr(i,out-i));
    output = output.substring(0,i) + String.fromCharCode(n) + output.substr(i,out-i);
    ++i;
  }

  return output;
}

/* eof */
