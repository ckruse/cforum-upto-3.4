/**
 * IDNA implementation, artistic license
 */

function IDNA(domn) {
  this.labels   = this.split_labels(domn);
  this.nameprep = new Nameprep();
  this.punycode = new Punycode();
}

IDNA.prototype.dots = [
  "\u002E", /* full stop */
  "\u3002", /* ideographic full stop */
  "\uFF0E", /* fullwidth full stop */
  "\uFF61"  /* halfwidth ideographic full stop */
];

IDNA.prototype.split_labels = function(str) {
  var ret = [],start = 0;

  for(var i=0;i<str.length;++i) {
    var c = str.charAt(i);

    for(var j=0;j<this.dots.length;++j) {
      if(c == this.dots[j]) { /* label delimiter */
        if(i == start) return false; /* if this happens, domain label is invalid (..) */

        ret.push(str.substring(start,i));
        start = i + 1;
      }
    }
  }

  ret.push(str.substring(start,i));

  return ret;
}

IDNA.prototype.toASCII = function() {
  var ret = [];

  for(var i=0;i<this.labels.length;++i) {
    var has_nonascii = false;
    var lbl = this.labels[i];

    // step 1: has this label non-ascii characters?
    for(var j=0;j<lbl.length;++j) {
      if(this.labels[i].charCodeAt(j) > 127) {
        has_nonascii = true;
        break;
      }
    }

    /* step 2: if has non-ascii characters */
    if(has_nonascii) {
      has_nonascii = false;
      lbl          = this.nameprep.prepare(lbl,1,1,1); /* use nameprep on them */

      if(lbl == false) return false;

      for(var j=0;j<lbl.length;++j) {
        var c = lbl.charCodeAt(j);
        // 0..2C, 2E..2F, 3A..40, 5B..60, and 7B..7F must be absent now
        if(c <= 0x2C || (c >= 0x2E && c <= 0x2F) || (c <= 0x3A && c >= 0x40) || (c >= 0x5B && c <= 0x60) || (c >= 0x7B && c <= 0x7F)) return false;

        if(c > 127) has_nonascii = true;
      }

      /* no leading or trailing slash */
      if(lbl.charAt(0) == '\u002D' || lbl.charAt(lbl.length-1) == '\u002D') return false;
    }

    if(has_nonascii) {
      lbl = this.punycode.encode(lbl);
      if(lbl == false) return false;
      lbl = "xn--" + lbl;
    }

    if(lbl.length < 1 || lbl.length > 63) return false;
    ret.push(lbl);
  }

  var dmn = ret.join('.');
  if(dmn.length > 255) return false;

  return dmn;
}

IDNA.prototype.toUnicode = function() {
  var ret = [];

  for(var i=0;i<this.labels.length;++i) {
    var has_nonascii = false;
    var lbl = this.labels[i];

    // step 1: has this label non-ascii characters?
    for(var j=0;j<lbl.length;++j) {
      if(this.labels[i].charCodeAt(j) > 127) {
        has_nonascii = true;
        break;
      }
    }

    /* step 2: nameprep */
    if(has_nonascii) {
      lbl = this.nameprep.prepare(lbl,1,1,1);
      if(!lbl) return false;
    }

    /* step 3: verify the ace prefix */
    if(lbl.substr(0,4) == "xn--") {
      /* step 4: remove ace */
      lbl = lbl.substring(4,lbl.length);

      lbl = this.punycode.decode(lbl);
      if(!lbl) return false;
    }

    ret.push(lbl);
  }

  return ret.join(".");
}

/* eof */
