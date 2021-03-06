/**
 * \file charconvert.h
 * \author Christian Kruse, <cjk@wwwtech.de>
 * \brief Character converting functions
 *
 * This file contains general character converting functions.
 * Its a wrapper around the libiconv, which provides conversion between many
 * charsets.
 */

/* {{{ Initial headers */
/*
 * $LastChangedDate$
 * $LastChangedRevision$
 * $LastChangedBy$
 *
 */
/* }}} */

#ifndef _CF_CHARCONVERT_H
#define _CF_CHARCONVERT_H

#define CF_UNI_CLS_SC  1
#define CF_UNI_CLS_CC  2
#define CF_UNI_CLS_NL  3
#define CF_UNI_CLS_PF  4
#define CF_UNI_CLS_PO  5
#define CF_UNI_CLS_ND  6
#define CF_UNI_CLS_NO  7
#define CF_UNI_CLS_PD  8
#define CF_UNI_CLS_ZP  9
#define CF_UNI_CLS_LL  10
#define CF_UNI_CLS_CN  11
#define CF_UNI_CLS_CO  12
#define CF_UNI_CLS_LU  13
#define CF_UNI_CLS_LO  14
#define CF_UNI_CLS_ZL  15
#define CF_UNI_CLS_LT  16
#define CF_UNI_CLS_MN  17
#define CF_UNI_CLS_PC  18
#define CF_UNI_CLS_CS  19
#define CF_UNI_CLS_ME  20
#define CF_UNI_CLS_LM  21
#define CF_UNI_CLS_ZS  22
#define CF_UNI_CLS_CF  23
#define CF_UNI_CLS_MC  24
#define CF_UNI_CLS_SM  25
#define CF_UNI_CLS_SO  26
#define CF_UNI_CLS_PE  27
#define CF_UNI_CLS_PI  28
#define CF_UNI_CLS_SK  29
#define CF_UNI_CLS_PS  30

#define CF_UNI_PROP_IDEOGRAPHIC  1
#define CF_UNI_PROP_OTHER_MATH  2
#define CF_UNI_PROP_RADICAL  3
#define CF_UNI_PROP_VARIATION_SELECTOR  4
#define CF_UNI_PROP_BIDI_CONTROL  5
#define CF_UNI_PROP_IDS_BINARY_OPERATOR  6
#define CF_UNI_PROP_OTHER_ID_CONTINUE  7
#define CF_UNI_PROP_NONCHARACTER_CODE_POINT  8
#define CF_UNI_PROP_STERM  9
#define CF_UNI_PROP_PATTERN_SYNTAX  10
#define CF_UNI_PROP_OTHER_ALPHABETIC  11
#define CF_UNI_PROP_TERMINAL_PUNCTUATION  12
#define CF_UNI_PROP_IDS_TRINARY_OPERATOR  13
#define CF_UNI_PROP_DEPRECATED  14
#define CF_UNI_PROP_OTHER_DEFAULT_IGNORABLE_CODE_POINT  15
#define CF_UNI_PROP_UNIFIED_IDEOGRAPH  16
#define CF_UNI_PROP_PATTERN_WHITE_SPACE  17
#define CF_UNI_PROP_ASCII_HEX_DIGIT  18
#define CF_UNI_PROP_QUOTATION_MARK  19
#define CF_UNI_PROP_OTHER_LOWERCASE  20
#define CF_UNI_PROP_WHITE_SPACE  21
#define CF_UNI_PROP_DIACRITIC  22
#define CF_UNI_PROP_JOIN_CONTROL  23
#define CF_UNI_PROP_EXTENDER  24
#define CF_UNI_PROP_HEX_DIGIT  25
#define CF_UNI_PROP_LOGICAL_ORDER_EXCEPTION  26
#define CF_UNI_PROP_DASH  27
#define CF_UNI_PROP_OTHER_ID_START  28
#define CF_UNI_PROP_SOFT_DOTTED  29
#define CF_UNI_PROP_HYPHEN  30
#define CF_UNI_PROP_OTHER_UPPERCASE  31
#define CF_UNI_PROP_OTHER_GRAPHEME_EXTEND  32

/**
 * This function tries to get the property of a unicode character as described in
 * <http://www.unicode.org/Public/UNIDATA/PropList.txt>
 * \param c The unicode character to get the properties of
 * \return The unicode property
 */
int cf_char_property(u_int32_t c);

/**
 * This function tries to classify a unicode character as described in
 * <http://www.unicode.org/Public/UNIDATA/UCD.html#General_Category_Values>
 * \param c The unicode character to classify
 * \return The unicode class
 */
int cf_classify_char(u_int32_t c);

/**
 * This function tries to convert a string from charset from_charset to charset to_charset.
 * \param toencode     The string to convert
 * \param in_len       The length of the string to convert
 * \param from_charset The starting charset
 * \param to_charset   The target charset
 * \param out_len_p    Pointer to a size_t variable. Will be filled with the length of the converted string
 * \return The converted string on success or NULL on failure
 * \attention You have to free() the returned string!
 */
u_char *charset_convert(const u_char *toencode,size_t in_len,const u_char *from_charset,const u_char *to_charset,size_t *out_len_p);

/**
 * This function converts HTML special characters to their entities. These
 * are < to &lt;, > to &gt;, & to &amp; and " to &quot;. If you set the
 * parameter sq to 1, ' will be encoded to &#39;, too.
 * \param string The string to convert
 * \param sq Switch which defines if ' shall be converted, too
 * \return The converted string on success or NULL on failure
 * \attention You have to free() the returned string!
 */
u_char *htmlentities(const u_char *string,int sq);

/**
 * This function does the same as htmlentities(), but it writes
 * the encoded string to a file handle
 * \param string The string to encode and write
 * \param sq Switch which defines if ' shall be converted, too
 * \param handle The file handle to write in
 * \return The number of characters written
 */
size_t print_htmlentities_encoded(const u_char *string,int sq,FILE *handle);

/**
 * This function converts a utf8 u_char string sequence to the unicode
 * number.
 *
 * \param s The sequence string
 * \param n The length of the sequence string (used to check if sequence can be legal)
 * \param num Reference to a u_int32_t. The unicode number will be stored in it
 * \return EILSEQ on failure, length of byte sequence on success
 */
int utf8_to_unicode(const u_char *s,size_t n,u_int32_t *num);

/**
 * This function converts a unicode char number to a utf8 u_char string sequence
 *
 * \param num The unicode char number
 * \param c The character buffer
 * \param n The size of the character buffer (more than 7 bytes will never be used)
 * \return EINVAL on failure, length of byte sequence on success
 */
int unicode_to_utf8(u_int32_t num, u_char *c,size_t n);

/**
 * This function converts a string from a given charset to a given charset
 * and encodes all html active chars (<, >, &, " and optional '). If a character
 * doesn't exist in the target charset, the character will be replaced by an named
 * entity (if exists) or a unicode entity.
 *
 * \param toencode The string to encode
 * \param from The source character set
 * \param to The target character set
 * \param outlen A reference to a size_t variable to store the length of the new string in it
 * \param sq Boolean; if true, single quotes will be encoded to &apos;
 * \return NULL on failure, the new string on success
 */
u_char *htmlentities_charset_convert(const u_char *toencode, const u_char *from, const u_char *to,size_t *outlen,int sq);

/**
 * This function converts a string from a given charset to a given charset. If a
 * character cannot be shown in the target charset it would be replaced by an HTML
 * entity. Everything else will not be touched (e.g. > will be > after calling, too).
 *
 * \param toencode The string to encode
 * \param in_len The length of the string to encode
 * \param from The source character set
 * \param to The target character set
 * \param outlen A reference to a size_t variable to store the length of the new string in it
 * \return NULL on failure, the new string on success
 */
u_char *charset_convert_entities(const u_char *toencode, size_t in_len,const u_char *from, const u_char *to,size_t *outlen);

/**
 * This function checks if the given string is a valid UTF-8 string
 * \param str The string to check
 * \param len The length of the string
 * \return Returns 0 if string is valid and -1 if it is not
 */
int is_valid_utf8_string(const u_char *str,size_t len);

/**
 * This function decodes the entities in a string
 * \param string The string to decode
 * \param len A pointer to store the new length in; may be NULL
 * \return NULL on failure, the new string on success
 */
u_char *htmlentities_decode(const u_char *string,size_t *len);


#endif

/* eof */
