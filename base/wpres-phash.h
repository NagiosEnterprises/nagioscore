/* C code produced by gperf version 3.0.3 */
/* Command-line: gperf -S 1 -t -H wpres_key_phash -N wpres_get_key wpres.gperf  */
/* Computed positions: -k'4-5,7' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "wpres.gperf"

enum {
	WPRES_job_id,
	WPRES_type,
	WPRES_command,
	WPRES_timeout,
	WPRES_wait_status,
	WPRES_start,
	WPRES_stop,
	WPRES_outstd,
	WPRES_outerr,
	WPRES_exited_ok,
	WPRES_error_msg,
	WPRES_error_code,
	WPRES_runtime,
	WPRES_ru_utime,
	WPRES_ru_stime,
	WPRES_ru_maxrss,
	WPRES_ru_ixrss,
	WPRES_ru_idrss,
	WPRES_ru_isrss,
	WPRES_ru_minflt,
	WPRES_ru_majflt,
	WPRES_ru_nswap,
	WPRES_ru_inblock,
	WPRES_ru_oublock,
	WPRES_ru_msgsnd,
	WPRES_ru_msgrcv,
	WPRES_ru_nsignals,
	WPRES_ru_nvcsw,
	WPRES_ru_nivcsw,
};
#include <string.h> /* for strcmp() */
#line 35 "wpres.gperf"
struct wpres_key {
	const char *name;
	int code;
};

#define TOTAL_KEYWORDS 29
#define MIN_WORD_LENGTH 4
#define MAX_WORD_LENGTH 11
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 64
/* maximum key range = 61, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
wpres_key_phash (str, len)
     register const char *str;
     register unsigned int len;
{
  static unsigned char asso_values[] =
    {
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 15, 65, 25, 65,  3,
      10,  0, 30,  0, 65,  0, 65, 65,  0,  0,
       0, 20,  5, 65,  0,  5,  0,  0, 30, 65,
      15, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65, 65, 65, 65, 65,
      65, 65, 65, 65, 65, 65
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[6]];
      /*FALLTHROUGH*/
      case 6:
      case 5:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
        hval += asso_values[(unsigned char)str[3]];
        break;
    }
  return hval;
}

#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
struct wpres_key *
wpres_get_key (str, len)
     register const char *str;
     register unsigned int len;
{
  static struct wpres_key wordlist[] =
    {
#line 41 "wpres.gperf"
      {"type", WPRES_type},
#line 45 "wpres.gperf"
      {"start", WPRES_start},
#line 48 "wpres.gperf"
      {"outerr", WPRES_outerr},
#line 52 "wpres.gperf"
      {"runtime", WPRES_runtime},
#line 53 "wpres.gperf"
      {"ru_utime", WPRES_ru_utime},
#line 46 "wpres.gperf"
      {"stop", WPRES_stop},
#line 62 "wpres.gperf"
      {"ru_inblock", WPRES_ru_inblock},
#line 47 "wpres.gperf"
      {"outstd", WPRES_outstd},
#line 68 "wpres.gperf"
      {"ru_nivcsw", WPRES_ru_nivcsw},
#line 54 "wpres.gperf"
      {"ru_stime", WPRES_ru_stime},
#line 65 "wpres.gperf"
      {"ru_msgrcv", WPRES_ru_msgrcv},
#line 66 "wpres.gperf"
      {"ru_nsignals", WPRES_ru_nsignals},
#line 58 "wpres.gperf"
      {"ru_isrss", WPRES_ru_isrss},
#line 64 "wpres.gperf"
      {"ru_msgsnd", WPRES_ru_msgsnd},
#line 40 "wpres.gperf"
      {"job_id", WPRES_job_id},
#line 57 "wpres.gperf"
      {"ru_idrss", WPRES_ru_idrss},
#line 49 "wpres.gperf"
      {"exited_ok", WPRES_exited_ok},
#line 44 "wpres.gperf"
      {"wait_status", WPRES_wait_status},
#line 43 "wpres.gperf"
      {"timeout", WPRES_timeout},
#line 56 "wpres.gperf"
      {"ru_ixrss", WPRES_ru_ixrss},
#line 50 "wpres.gperf"
      {"error_msg", WPRES_error_msg},
#line 63 "wpres.gperf"
      {"ru_oublock", WPRES_ru_oublock},
#line 51 "wpres.gperf"
      {"error_code", WPRES_error_code},
#line 55 "wpres.gperf"
      {"ru_maxrss", WPRES_ru_maxrss},
#line 61 "wpres.gperf"
      {"ru_nswap", WPRES_ru_nswap},
#line 59 "wpres.gperf"
      {"ru_minflt", WPRES_ru_minflt},
#line 42 "wpres.gperf"
      {"command", WPRES_command},
#line 67 "wpres.gperf"
      {"ru_nvcsw", WPRES_ru_nvcsw},
#line 60 "wpres.gperf"
      {"ru_majflt", WPRES_ru_majflt}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = wpres_key_phash (str, len);

      if (key <= MAX_HASH_VALUE && key >= MIN_HASH_VALUE)
        {
          register struct wpres_key *resword;

          switch (key - 4)
            {
              case 0:
                resword = &wordlist[0];
                goto compare;
              case 1:
                resword = &wordlist[1];
                goto compare;
              case 2:
                resword = &wordlist[2];
                goto compare;
              case 3:
                resword = &wordlist[3];
                goto compare;
              case 4:
                resword = &wordlist[4];
                goto compare;
              case 5:
                resword = &wordlist[5];
                goto compare;
              case 6:
                resword = &wordlist[6];
                goto compare;
              case 7:
                resword = &wordlist[7];
                goto compare;
              case 8:
                resword = &wordlist[8];
                goto compare;
              case 9:
                resword = &wordlist[9];
                goto compare;
              case 10:
                resword = &wordlist[10];
                goto compare;
              case 12:
                resword = &wordlist[11];
                goto compare;
              case 14:
                resword = &wordlist[12];
                goto compare;
              case 15:
                resword = &wordlist[13];
                goto compare;
              case 17:
                resword = &wordlist[14];
                goto compare;
              case 19:
                resword = &wordlist[15];
                goto compare;
              case 20:
                resword = &wordlist[16];
                goto compare;
              case 22:
                resword = &wordlist[17];
                goto compare;
              case 23:
                resword = &wordlist[18];
                goto compare;
              case 24:
                resword = &wordlist[19];
                goto compare;
              case 25:
                resword = &wordlist[20];
                goto compare;
              case 26:
                resword = &wordlist[21];
                goto compare;
              case 29:
                resword = &wordlist[22];
                goto compare;
              case 30:
                resword = &wordlist[23];
                goto compare;
              case 34:
                resword = &wordlist[24];
                goto compare;
              case 35:
                resword = &wordlist[25];
                goto compare;
              case 38:
                resword = &wordlist[26];
                goto compare;
              case 39:
                resword = &wordlist[27];
                goto compare;
              case 60:
                resword = &wordlist[28];
                goto compare;
            }
          return 0;
        compare:
          {
            register const char *s = resword->name;

            if (*str == *s && !strcmp (str + 1, s + 1))
              return resword;
          }
        }
    }
  return 0;
}
