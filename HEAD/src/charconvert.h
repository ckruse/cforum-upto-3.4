/**
 * \file charconvert.h
 * \author Christian Kruse, <ckruse@wwwtech.de>
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

#ifndef __CHARCONVERT_H
#define __CHARCONVERT_H

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

#endif

/* eof */
