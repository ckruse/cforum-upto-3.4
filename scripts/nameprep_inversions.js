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

NameprepInversion.prototype.rangeArray	 = [];
NameprepInversion.prototype.opposite     = 0;
NameprepInversion.prototype.getLeast     = NameprepInversion_getLeast;
NameprepInversion.prototype.getLeast2    = NameprepInversion_getLeast2;
NameprepInversion.prototype.contains     = NameprepInversion_contains;
NameprepInversion.prototype.previous     = NameprepInversion_previousDifferent;
NameprepInversion.prototype.next         = NameprepInversion_nextDifferent;
NameprepInversion.prototype.makeOpposite = NameprepInversion_makeOpposite;

NameprepInversionMap.prototype.inversion = null;
NameprepInversionMap.prototype.values    = null;
NameprepInversionMap.prototype.at        = NameprepInversionMap_at;

/**
 * Maps integers to a range (half-open).
 * When used as a set, even indices are IN, and odd are OUT.
 * @parameter rangeArray must be an array of monotonically increasing integer values, with at least one instance.
 */
function NameprepInversion(rangeArray) {
  this.rangeArray = rangeArray;

  for(var i=1;i<rangeArray.length;++i) {
    if(rangeArray[i] == null) rangeArray[i] = rangeArray[i-1] + 1;
    else if(rangeArray[i-1] >= rangeArray[i]) return; // Array must be monotonically increasing!
  }
}

/**
 * Creates opposite of this, that is: result.contains(c) if !this.contains(c)
 * @return reversal of this
 */
function NameprepInversion_makeOpposite () {
  var result = new NameprepInversion(this.rangeArray);
  result.opposite = 1 ^ this.opposite;
  return result;
}

/**
 * @intValue probe value
 * @return true if probe is in the list, false otherwise.
 * Uses the fact than an inversion list
 * contains half-open ranges. An element is
 * in the list iff the smallest index is even
 */
function NameprepInversion_contains(intValue) {
  return (this.getLeast(intValue) & 1) == this.opposite;
}

/**
 * @intValue probe value
 * @return the largest index such that rangeArray[index] <= intValue.
 * If intValue < rangeArray[0], returns -1.
 */
function NameprepInversion_getLeast (intValue) {
  var arr  = this.rangeArray;
  var low  = 0;
  var high = arr.length;

  while(high - low > 8) {
    var mid = (high + low) >> 1;

    if(arr[mid] <= intValue) low = mid;
    else high = mid;
  }

  for(;low < high;++low) {
    if(intValue < arr[low]) break;
  }

  return low - 1;
}

/**
 * @intValue probe value
 * @return the largest index such that rangeArray[index] <= intValue.
 * If intValue < rangeArray[0], returns -1.
 */
function NameprepInversion_getLeast2(intValue) {
  var arr  = this.rangeArray;
  var low  = 0;
  var high = arr.length;

  for(; low < high; ++low) {
    if(intValue < arr[low]) break;
  }

  return low - 1;
}

/**
 * @intValue probe value
 * @return next greater probe value that would be different.
 * or null if it would be out of range
 */
function NameprepInversion_nextDifferent(intValue, delta) {
  return this.rangeArray[this.getLeast(intValue) + delta];
}

/**
 * @intValue probe value
 * @return previous lesser probe value that would be different.
 * or null if it would be out of range
 */
function NameprepInversion_previousDifferent(intValue, delta) {
  return this.rangeArray[this.getLeast(intValue) - delta];
}

/**
 * Maps ranges to values.
 * @parameter rangeArray must be suitable for an Inversion.
 * @parameter valueArray is the list of corresponding values.
 * Length must be the same as rangeArray.
 */
function NameprepInversionMap(rangeArray, valueArray) {
  if(rangeArray.length != valueArray.length) return; // error

  this.inversion = new NameprepInversion(rangeArray);
  this.values    = valueArray;
}

/**
 * Gets value at range
 * @parameter intValue. Any integer value.
 * @return the value associated with that integer. null if before the first
 * item in the range.
 */
function NameprepInversionMap_at(intValue) {
  var index = this.inversion.getLeast(intValue);

  if(index < 0) return null;

  return this.values[index];
}

/* eof */
