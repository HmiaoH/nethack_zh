/* NetHack 5.0	utf8map.h	*/
/* NetHack may be freely redistributed.  See license for details. */

#ifndef UTF8MAP_H
#define UTF8MAP_H

/*
 * UTF-8 helpers.
 *
 * These are used for the Chinese localization (ZHLANG): the tty port has
 * to measure and split text by display columns rather than by bytes, so
 * that multi-byte characters are neither cut in half nor miscounted.
 * They are harmless for the untranslated game and are therefore always
 * available.
 *
 * u8_seqlen(str): number of bytes in the UTF-8 sequence starting at
 *                 'str' (1..4); an invalid or incomplete sequence is
 *                 reported as 1 so that callers always make progress.
 *                 returns 0 for the terminating NUL byte.
 * u8_width(str):  number of terminal columns the string occupies;
 *                 ASCII takes 1 column, CJK/full-width characters take
 *                 2, combining marks take 0, and malformed bytes are
 *                 counted as 1 column each.
 */
extern int u8_seqlen(const char *);
extern int u8_width(const char *);

#endif /* UTF8MAP_H */
