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


/**
 * Functions for normalization. Not particularly optimized. Requires data from Normalization_data.js
 */

/**
 * Normalizes to form NFKD
 * @parameter source source string
 * @return normalized version
 */
Nameprep.prototype.nfkd = function(source) {
  var result = "";
  var k      = 0;

  for(var i=0;i<source.length;++i) {
    var buffer = this.rawDecompose(source.charCodeAt(i));

    // add all of the characters in the decomposition.
    // (may be just the original character, if there was
    // no decomposition mapping)

    for(var j=0;j<buffer.length;++j) {
      var ch      = buffer.charCodeAt(j);
      var chClass = this.canonicalClass(ch);

      if(chClass == 0) {
        result += String.fromCharCode(ch);
        continue;
      }

      // bubble-sort combining marks as necessary
      for(k=result.length-1;k>=0;--k) {
        if(this.canonicalClass(result.charCodeAt(k)) <= chClass) break;
      }

      result = this.replace(result,k+1,k+1,String.fromCharCode(ch));
    }
  }

  return result;
}

/**
 * Normalizes to form NFKC
 * @parameter source source string
 * @return normalized version
 */
Nameprep.prototype.nfkc = function(str) {
  return this.compose(this.nfkd(str));
}

/**
 * Internal function for NFKC
 * @parameter source source string
 * @return normalized version
 */
Nameprep.prototype.compose = function(source) {
  if(source.length == 0) return result;

  var result    = "";
  var buffer    = "";
  var starterCh = source.charCodeAt(0);
  var lastClass = this.canonicalClass(starterCh);

  if(lastClass != 0) lastClass = 256; // fix for strings starting with a combining mark

  // Loop on the decomposed characters, combining where possible
  for(var decompPos=1;decompPos<source.length;++decompPos) {
    var ch        = source.charCodeAt(decompPos);
    var chClass   = this.canonicalClass(ch);
    var composite = this.rawCompose(starterCh, ch);

    if(composite != null && (lastClass < chClass || lastClass == 0)) starterCh = composite;
    else {
      if(chClass == 0) {
        result += String.fromCharCode(starterCh) + buffer;
        buffer.length = 0;
        starterCh  = ch;
      }
      else buffer += String.fromCharCode(ch);

      lastClass = chClass;
    }
  }

  return result + String.fromCharCode(starterCh) + buffer;
}

/**
 * Internal utility for decomposing. Uses Javascript object as simple lookup.
 * @parameter ch source character
 * @return raw NFKD decomposition.
 */
Nameprep.prototype.rawDecompose = function(ch) {
  if(this.SBase <= ch && ch < this.SLimit) return this.decomposeHangul(ch);
  return this.KD_Map[ch] ? this.KD_Map[ch] : String.fromCharCode(ch);
}

/**
 * Internal function for composing. Uses Javascript object as simple lookup.
 * WARNING: DOESN'T DO HANGUL YET
 * @parameter char1 first character to check
 * @parameter char1 second character to check
 * @return composed character, or null if there is none.
 */
Nameprep.prototype.rawCompose = function(char1, char2) {
  var temp = this.composeHangul(char1, char2);

  if(temp != null) return temp;

  return this.KC_Map[(char1 << 16) | char2];
}

/**
 * Internal function for NFKC
 * Returns canonical class, using Javascript object for simple lookup.
 * @parameter ch character to check
 * @return canonical class, number from 0 to 255
 */
Nameprep.prototype.canonicalClass = function(ch) {
  return this.CC_Map[ch] ? this.CC_Map[ch] : 0;
}

/**
 * Utility, since Javascript doesn't have it
 * @parameter sourceString String to replace extent in
 * @parameter startPos starting position of text to delete and replace
 * @parameter endPos ending position (as with substring, index of 1 past last char to replace)
 * @parameter insertionString string to put in
 * @return string with replacement done
 */
Nameprep.prototype.replace = function(sourceString, startPos, endPos, insertionString) {
  return sourceString.substring(0, startPos) + insertionString + sourceString.substring(endPos,sourceString.length);
}

// constants for Hangul
Nameprep.prototype.SBase  = 0xAC00;
Nameprep.prototype.LBase  = 0x1100;
Nameprep.prototype.VBase  = 0x1161;
Nameprep.prototype.TBase  = 0x11A7;
Nameprep.prototype.LCount = 19;
Nameprep.prototype.VCount = 21;
Nameprep.prototype.TCount = 28;
Nameprep.prototype.NCount = Nameprep.prototype.VCount * Nameprep.prototype.TCount; // 588
Nameprep.prototype.SCount = Nameprep.prototype.LCount * Nameprep.prototype.NCount; // 11,172
Nameprep.prototype.LLimit = Nameprep.prototype.LBase + Nameprep.prototype.LCount;  // 1113
Nameprep.prototype.VLimit = Nameprep.prototype.VBase + Nameprep.prototype.VCount;  // 1176
Nameprep.prototype.TLimit = Nameprep.prototype.TBase + Nameprep.prototype.TCount;  // 11C3
Nameprep.prototype.SLimit = Nameprep.prototype.SBase + Nameprep.prototype.SCount;  // D7A4

/**
 * Internal utility for decomposing.
 * @parameter ch source character
 * @return raw decomposition.
 */
Nameprep.prototype.decomposeHangul = function(s) {
  var SIndex = s - this.SBase;
  var L      = this.LBase + SIndex / this.NCount;
  var V      = this.VBase + (SIndex % this.NCount) / this.TCount;
  var T      = this.TBase + SIndex % this.TCount;
  var result = String.fromCharCode(L) + String.fromCharCode(V);

  if(T != this.TBase) result += String.fromCharCode(T);
  return result;
}

/**
 * Internal function for composing.
 * @parameter char1 first character to check
 * @parameter char1 second character to check
 * @return composed character, or null if there is none.
 */
Nameprep.prototype.composeHangul = function(char1, char2) {
  if(this.LBase <= char1 && char1 < this.LLimit && this.VBase <= char2 && char2 < this.VLimit)
    return this.SBase + ((char1 - this.LBase) * this.VCount + (char2 - this.VBase)) * this.TCount;

  if(this.SBase <= char1 && char1 < this.SLimit && this.TBase <= char2 && char2 < this.TLimit && ((char1 - this.SBase) % this.TCount) == 0)
    return char1 + (char2 - this.TBase);

  return null; // no composition
}

/* eof */
