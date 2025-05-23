/*	$OpenBSD: varmodifiers.c,v 1.50 2024/06/18 02:11:04 millert Exp $	*/
/*	$NetBSD: var.c,v 1.18 1997/03/18 19:24:46 christos Exp $	*/

/*
 * Copyright (c) 1999-2010 Marc Espie.
 *
 * Extensive code changes for the OpenBSD project.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* VarModifiers_Apply is mostly a constituent function of Var_Parse, it
 * is also called directly by Var_SubstVar.  */


#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defines.h"
#include "buf.h"
#include "var.h"
#include "varmodifiers.h"
#include "varname.h"
#include "targ.h"
#include "error.h"
#include "str.h"
#include "cmd_exec.h"
#include "memory.h"
#include "gnode.h"


/* Var*Pattern flags */
#define VAR_SUB_GLOBAL	0x01	/* Apply substitution globally */
#define VAR_SUB_ONE	0x02	/* Apply substitution to one word */
#define VAR_SUB_MATCHED 0x04	/* There was a match */
#define VAR_MATCH_START 0x08	/* Match at start of word */
#define VAR_MATCH_END	0x10	/* Match at end of word */

/* Modifiers flags */
#define VAR_EQUAL	0x20
#define VAR_MAY_EQUAL	0x40
#define VAR_ADD_EQUAL	0x80
#define VAR_BANG_EQUAL	0x100

typedef struct {
	char	  *lbuffer; /* Left string to free */
	char	  *lhs;     /* String to match */
	size_t	  leftLen;  /* Length of string */
	char	  *rhs;     /* Replacement string (w/ &'s removed) */
	size_t	  rightLen; /* Length of replacement */
	int 	  flags;
} VarPattern;

static bool VarHead(struct Name *, bool, Buffer, void *);
static bool VarTail(struct Name *, bool, Buffer, void *);
static bool VarSuffix(struct Name *, bool, Buffer, void *);
static bool VarRoot(struct Name *, bool, Buffer, void *);
static bool VarMatch(struct Name *, bool, Buffer, void *);
static bool VarSYSVMatch(struct Name *, bool, Buffer, void *);
static bool VarNoMatch(struct Name *, bool, Buffer, void *);


static void VarREError(int, regex_t *, const char *);
static bool VarRESubstitute(struct Name *, bool, Buffer, void *);
static char *do_regex(const char *, const struct Name *, void *);

typedef struct {
	regex_t	  re;
	int 	  nsub;
	regmatch_t	 *matches;
	char	 *replace;
	int 	  flags;
} VarREPattern;

static bool VarSubstitute(struct Name *, bool, Buffer, void *);
static char *VarGetPattern(SymTable *, int, const char **, int, int,
    size_t *, VarPattern *);
static char *VarQuote(const char *, const struct Name *, void *);
static char *VarModify(char *, bool (*)(struct Name *, bool, Buffer, void *), void *);

static void *check_empty(const char **, SymTable *, bool, int);
static void *check_quote(const char **, SymTable *, bool, int);
static char *do_upper(const char *, const struct Name *, void *);
static char *do_lower(const char *, const struct Name *, void *);
static void *check_shcmd(const char **, SymTable *, bool, int);
static char *do_shcmd(const char *, const struct Name *, void *);
static void *get_stringarg(const char **, SymTable *, bool, int);
static void free_stringarg(void *);
static void *get_patternarg(const char **, SymTable *, bool, int);
static void *get_spatternarg(const char **, SymTable *, bool, int);
static void *common_get_patternarg(const char **, SymTable *, bool, int, bool);
static void free_patternarg(void *);
static void *get_sysvpattern(const char **, SymTable *, bool, int);

static struct Name dummy;
static struct Name *dummy_arg = &dummy;

static struct modifier {
	    void * (*getarg)(const char **, SymTable *, bool, int);
	    char * (*apply)(const char *, const struct Name *, void *);
	    bool (*word_apply)(struct Name *, bool, Buffer, void *);
	    void   (*freearg)(void *);
} *choose_mod[256],
	match_mod = {get_stringarg, NULL, VarMatch, free_stringarg},
	nomatch_mod = {get_stringarg, NULL, VarNoMatch, free_stringarg},
	subst_mod = {get_spatternarg, NULL, VarSubstitute, free_patternarg},
	resubst_mod = {get_patternarg, do_regex, NULL, free_patternarg},
	quote_mod = {check_quote, VarQuote, NULL , free},
	tail_mod = {check_empty, NULL, VarTail, NULL},
	head_mod = {check_empty, NULL, VarHead, NULL},
	suffix_mod = {check_empty, NULL, VarSuffix, NULL},
	root_mod = {check_empty, NULL, VarRoot, NULL},
	upper_mod = {check_empty, do_upper, NULL, NULL},
	lower_mod = {check_empty, do_lower, NULL, NULL},
	shcmd_mod = {check_shcmd, do_shcmd, NULL, NULL},
	sysv_mod = {get_sysvpattern, NULL, VarSYSVMatch, free_patternarg}
;

void
VarModifiers_Init(void)
{
	choose_mod['M'] = &match_mod;
	choose_mod['N'] = &nomatch_mod;
	choose_mod['S'] = &subst_mod;
	choose_mod['C'] = &resubst_mod;
	choose_mod['Q'] = &quote_mod;
	choose_mod['T'] = &tail_mod;
	choose_mod['H'] = &head_mod;
	choose_mod['E'] = &suffix_mod;
	choose_mod['R'] = &root_mod;
	choose_mod['U'] = &upper_mod;
	choose_mod['L'] = &lower_mod;
	choose_mod['s'] = &shcmd_mod;
}

/* All modifiers handle addSpace (need to add a space before placing the
 * next word into the buffer) and propagate it when necessary.
 */

/*-
 *-----------------------------------------------------------------------
 * VarHead --
 *	Remove the tail of the given word and add the result to the given
 *	buffer.
 *-----------------------------------------------------------------------
 */
static bool
VarHead(struct Name *word, bool addSpace, Buffer buf, void *dummy UNUSED)
{
	const char	*slash;

	slash = Str_rchri(word->s, word->e, '/');
	if (slash != NULL) {
		if (addSpace)
			Buf_AddSpace(buf);
		Buf_Addi(buf, word->s, slash);
	} else {
		/* If no directory part, give . (q.v. the POSIX standard).  */
		if (addSpace)
			Buf_AddString(buf, " .");
		else
			Buf_AddChar(buf, '.');
	}
	return true;
}

/*-
 *-----------------------------------------------------------------------
 * VarTail --
 *	Remove the head of the given word add the result to the given
 *	buffer.
 *-----------------------------------------------------------------------
 */
static bool
VarTail(struct Name *word, bool addSpace, Buffer buf, void *dummy UNUSED)
{
	const char	*slash;

	if (addSpace)
		Buf_AddSpace(buf);
	slash = Str_rchri(word->s, word->e, '/');
	if (slash != NULL)
		Buf_Addi(buf, slash+1, word->e);
	else
		Buf_Addi(buf, word->s, word->e);
	return true;
}

/*-
 *-----------------------------------------------------------------------
 * VarSuffix --
 *	Add the suffix of the given word to the given buffer.
 *-----------------------------------------------------------------------
 */
static bool
VarSuffix(struct Name *word, bool addSpace, Buffer buf, void *dummy UNUSED)
{
	const char	*dot;

	dot = Str_rchri(word->s, word->e, '.');
	if (dot != NULL) {
		if (addSpace)
			Buf_AddSpace(buf);
		Buf_Addi(buf, dot+1, word->e);
		addSpace = true;
	}
	return addSpace;
}

/*-
 *-----------------------------------------------------------------------
 * VarRoot --
 *	Remove the suffix of the given word and add the result to the
 *	buffer.
 *-----------------------------------------------------------------------
 */
static bool
VarRoot(struct Name *word, bool addSpace, Buffer buf, void *dummy UNUSED)
{
	const char	*dot;

	if (addSpace)
		Buf_AddSpace(buf);
	dot = Str_rchri(word->s, word->e, '.');
	if (dot != NULL)
		Buf_Addi(buf, word->s, dot);
	else
		Buf_Addi(buf, word->s, word->e);
	return true;
}

/*-
 *-----------------------------------------------------------------------
 * VarMatch --
 *	Add the word to the buffer if it matches the given pattern.
 *-----------------------------------------------------------------------
 */
static bool
VarMatch(struct Name *word, bool addSpace, Buffer buf, void *pattern)
{
	const char *pat = pattern;

	if (Str_Matchi(word->s, word->e, pat, strchr(pat, '\0'))) {
		if (addSpace)
			Buf_AddSpace(buf);
		Buf_Addi(buf, word->s, word->e);
		return true;
	} else
		return addSpace;
}

/*-
 *-----------------------------------------------------------------------
 * VarNoMatch --
 *	Add the word to the buffer if it doesn't match the given pattern.
 *-----------------------------------------------------------------------
 */
static bool
VarNoMatch(struct Name *word, bool addSpace, Buffer buf, void *pattern)
{
	const char *pat = pattern;

	if (!Str_Matchi(word->s, word->e, pat, strchr(pat, '\0'))) {
		if (addSpace)
			Buf_AddSpace(buf);
		Buf_Addi(buf, word->s, word->e);
		return true;
	} else
		return addSpace;
}

/*-
 *-----------------------------------------------------------------------
 * VarSYSVMatch --
 *	Add the word to the buffer if it matches the given pattern.
 *	Used to implement the System V % modifiers.
 *-----------------------------------------------------------------------
 */
static bool
VarSYSVMatch(struct Name *word, bool addSpace, Buffer buf, void *patp)
{
	size_t	len;
	const char	*ptr;
	VarPattern	*pat = patp;

	if (*word->s != '\0') {
		if (addSpace)
			Buf_AddSpace(buf);
		if ((ptr = Str_SYSVMatch(word->s, pat->lhs, &len)) != NULL)
			Str_SYSVSubst(buf, pat->rhs, ptr, len);
		else
			Buf_Addi(buf, word->s, word->e);
		return true;
	} else
		return addSpace;
}

void *
get_sysvpattern(const char **p, SymTable *ctxt UNUSED, bool err, int endc)
{
	VarPattern		*pattern;
	const char		*cp, *cp2;
	BUFFER buf, buf2;
	int cnt = 0;
	char startc = endc == ')' ? '(' : '{';

	Buf_Init(&buf, 0);
	for (cp = *p;; cp++) {
		if (*cp == '=' && cnt == 0)
			break;
		if (*cp == '\0') {
			Buf_Destroy(&buf);
			return NULL;
		}
		if (*cp == startc)
			cnt++;
		else if (*cp == endc) {
			cnt--;
			if (cnt < 0) {
				Buf_Destroy(&buf);
				return NULL;
			}
		} else if (*cp == '$') {
			if (cp[1] == '$')
				cp++;
			else {
				size_t len;
				(void)Var_ParseBuffer(&buf, cp, ctxt, err, 
				    &len);
				cp += len - 1;
				continue;
			}
		}
		Buf_AddChar(&buf, *cp);
	}

	Buf_Init(&buf2, 0);
	for (cp2 = cp+1;; cp2++) {
		if (((*cp2 == ':' && cp2[1] != endc) || *cp2 == endc) && 
		    cnt == 0)
			break;
		if (*cp2 == '\0') {
			Buf_Destroy(&buf);
			Buf_Destroy(&buf2);
			return NULL;
		}
		if (*cp2 == startc)
			cnt++;
		else if (*cp2 == endc) {
			cnt--;
			if (cnt < 0) {
				Buf_Destroy(&buf);
				Buf_Destroy(&buf2);
				return NULL;
			}
		} else if (*cp2 == '$') {
			if (cp2[1] == '$')
				cp2++;
			else {
				size_t len;
				(void)Var_ParseBuffer(&buf2, cp2, ctxt, err, 
				    &len);
				cp2 += len - 1;
				continue;
			}
		}
		Buf_AddChar(&buf2, *cp2);
	}

	pattern = emalloc(sizeof(VarPattern));
	pattern->lbuffer = pattern->lhs = Buf_Retrieve(&buf);
	pattern->leftLen = Buf_Size(&buf);
	pattern->rhs = Buf_Retrieve(&buf2);
	pattern->rightLen = Buf_Size(&buf2);
	pattern->flags = 0;
	*p = cp2;
	return pattern;
}


/*-
 *-----------------------------------------------------------------------
 * VarSubstitute --
 *	Perform a string-substitution on the given word, Adding the
 *	result to the given buffer.
 *-----------------------------------------------------------------------
 */
static bool
VarSubstitute(struct Name *word, bool addSpace, Buffer buf,
    void *patternp) /* Pattern for substitution */
{
    size_t	wordLen;    /* Length of word */
    const char	*cp;	    /* General pointer */
    VarPattern	*pattern = patternp;

    wordLen = word->e - word->s;
    if ((pattern->flags & (VAR_SUB_ONE|VAR_SUB_MATCHED)) !=
	(VAR_SUB_ONE|VAR_SUB_MATCHED)) {
	/* Still substituting -- break it down into simple anchored cases
	 * and if none of them fits, perform the general substitution case.  */
	if ((pattern->flags & VAR_MATCH_START) &&
	    (strncmp(word->s, pattern->lhs, pattern->leftLen) == 0)) {
		/* Anchored at start and beginning of word matches pattern.  */
		if ((pattern->flags & VAR_MATCH_END) &&
		    (wordLen == pattern->leftLen)) {
			/* Also anchored at end and matches to the end (word
			 * is same length as pattern) add space and rhs only
			 * if rhs is non-null.	*/
			if (pattern->rightLen != 0) {
			    if (addSpace)
				Buf_AddSpace(buf);
			    addSpace = true;
			    Buf_AddChars(buf, pattern->rightLen,
					 pattern->rhs);
			}
			pattern->flags |= VAR_SUB_MATCHED;
		} else if (pattern->flags & VAR_MATCH_END) {
		    /* Doesn't match to end -- copy word wholesale.  */
		    goto nosub;
		} else {
		    /* Matches at start but need to copy in
		     * trailing characters.  */
		    if ((pattern->rightLen + wordLen - pattern->leftLen) != 0){
			if (addSpace)
			    Buf_AddSpace(buf);
			addSpace = true;
		    }
		    Buf_AddChars(buf, pattern->rightLen, pattern->rhs);
		    Buf_AddChars(buf, wordLen - pattern->leftLen,
				 word->s + pattern->leftLen);
		    pattern->flags |= VAR_SUB_MATCHED;
		}
	} else if (pattern->flags & VAR_MATCH_START) {
	    /* Had to match at start of word and didn't -- copy whole word.  */
	    goto nosub;
	} else if (pattern->flags & VAR_MATCH_END) {
	    /* Anchored at end, Find only place match could occur (leftLen
	     * characters from the end of the word) and see if it does. Note
	     * that because the $ will be left at the end of the lhs, we have
	     * to use strncmp.	*/
	    cp = word->s + (wordLen - pattern->leftLen);
	    if (cp >= word->s &&
		strncmp(cp, pattern->lhs, pattern->leftLen) == 0) {
		/* Match found. If we will place characters in the buffer,
		 * add a space before hand as indicated by addSpace, then
		 * stuff in the initial, unmatched part of the word followed
		 * by the right-hand-side.  */
		if (((cp - word->s) + pattern->rightLen) != 0) {
		    if (addSpace)
			Buf_AddSpace(buf);
		    addSpace = true;
		}
		Buf_Addi(buf, word->s, cp);
		Buf_AddChars(buf, pattern->rightLen, pattern->rhs);
		pattern->flags |= VAR_SUB_MATCHED;
	    } else {
		/* Had to match at end and didn't. Copy entire word.  */
		goto nosub;
	    }
	} else {
	    /* Pattern is unanchored: search for the pattern in the word using
	     * strstr, copying unmatched portions and the
	     * right-hand-side for each match found, handling non-global
	     * substitutions correctly, etc. When the loop is done, any
	     * remaining part of the word (word and wordLen are adjusted
	     * accordingly through the loop) is copied straight into the
	     * buffer.
	     * addSpace is set to false as soon as a space is added to the
	     * buffer.	*/
	    bool done;
	    size_t origSize;

	    done = false;
	    origSize = Buf_Size(buf);
	    while (!done) {
		cp = strstr(word->s, pattern->lhs);
		if (cp != NULL) {
		    if (addSpace && (cp - word->s) + pattern->rightLen != 0){
			Buf_AddSpace(buf);
			addSpace = false;
		    }
		    Buf_Addi(buf, word->s, cp);
		    Buf_AddChars(buf, pattern->rightLen, pattern->rhs);
		    wordLen -= (cp - word->s) + pattern->leftLen;
		    word->s = cp + pattern->leftLen;
		    if (wordLen == 0 || (pattern->flags & VAR_SUB_GLOBAL) == 0)
			done = true;
		    pattern->flags |= VAR_SUB_MATCHED;
		} else
		    done = true;
	    }
	    if (wordLen != 0) {
		if (addSpace)
		    Buf_AddSpace(buf);
		Buf_AddChars(buf, wordLen, word->s);
	    }
	    /* If added characters to the buffer, need to add a space
	     * before we add any more. If we didn't add any, just return
	     * the previous value of addSpace.	*/
	    return Buf_Size(buf) != origSize || addSpace;
	}
	return addSpace;
    }
 nosub:
    if (addSpace)
	Buf_AddSpace(buf);
    Buf_AddChars(buf, wordLen, word->s);
    return true;
}

/*-
 *-----------------------------------------------------------------------
 * VarREError --
 *	Print the error caused by a regcomp or regexec call.
 *-----------------------------------------------------------------------
 */
static void
VarREError(int err, regex_t *pat, const char *str)
{
	char	*errbuf;
	int 	errlen;

	errlen = regerror(err, pat, 0, 0);
	errbuf = emalloc(errlen);
	regerror(err, pat, errbuf, errlen);
	Error("%s: %s", str, errbuf);
	free(errbuf);
}

/*-
 *-----------------------------------------------------------------------
 * VarRESubstitute --
 *	Perform a regex substitution on the given word, placing the
 *	result in the passed buffer.
 *-----------------------------------------------------------------------
 */
static bool
VarRESubstitute(struct Name *word, bool addSpace, Buffer buf, void *patternp)
{
	VarREPattern	*pat;
	int 		xrv;
	const char		*wp;
	char		*rp;
	int 		added;

#define MAYBE_ADD_SPACE()		\
	if (addSpace && !added) 	\
		Buf_AddSpace(buf);	\
	added = 1

	added = 0;
	wp = word->s;
	pat = patternp;

	if ((pat->flags & (VAR_SUB_ONE|VAR_SUB_MATCHED)) ==
	    (VAR_SUB_ONE|VAR_SUB_MATCHED))
		xrv = REG_NOMATCH;
	else {
	tryagain:
		xrv = regexec(&pat->re, wp, pat->nsub, pat->matches, 0);
	}

	switch (xrv) {
	case 0:
		pat->flags |= VAR_SUB_MATCHED;
		if (pat->matches[0].rm_so > 0) {
			MAYBE_ADD_SPACE();
			Buf_AddChars(buf, pat->matches[0].rm_so, wp);
		}

		for (rp = pat->replace; *rp; rp++) {
			if (*rp == '\\' && (rp[1] == '&' || rp[1] == '\\')) {
				MAYBE_ADD_SPACE();
				Buf_AddChar(buf,rp[1]);
				rp++;
			}
			else if (*rp == '&' ||
			    (*rp == '\\' && ISDIGIT(rp[1]))) {
				int n;
				const char *subbuf;
				int sublen;
				char errstr[3];

				if (*rp == '&') {
					n = 0;
					errstr[0] = '&';
					errstr[1] = '\0';
				} else {
					n = rp[1] - '0';
					errstr[0] = '\\';
					errstr[1] = rp[1];
					errstr[2] = '\0';
					rp++;
				}

				if (n >= pat->nsub) {
					Error("No subexpression %s",
					    &errstr[0]);
					subbuf = "";
					sublen = 0;
				} else if (pat->matches[n].rm_so == -1 &&
				    pat->matches[n].rm_eo == -1) {
					Error("No match for subexpression %s",
					    &errstr[0]);
					subbuf = "";
					sublen = 0;
				} else {
					subbuf = wp + pat->matches[n].rm_so;
					sublen = pat->matches[n].rm_eo -
					    pat->matches[n].rm_so;
				}

				if (sublen > 0) {
					MAYBE_ADD_SPACE();
					Buf_AddChars(buf, sublen, subbuf);
				}
			} else {
				MAYBE_ADD_SPACE();
				Buf_AddChar(buf, *rp);
			}
		}
		wp += pat->matches[0].rm_eo;
		if (pat->flags & VAR_SUB_GLOBAL) {
			/* like most modern tools, empty string matches
			 * should advance one char at a time...
			 */
			if (pat->matches[0].rm_eo == 0)  {
				if (*wp) {
					MAYBE_ADD_SPACE();
					Buf_AddChar(buf, *wp++);
				} else
					break;
			}
			goto tryagain;
		}
		if (*wp) {
			MAYBE_ADD_SPACE();
			Buf_AddString(buf, wp);
		}
		break;
	default:
		VarREError(xrv, &pat->re, "Unexpected regex error");
	       /* FALLTHROUGH */
	case REG_NOMATCH:
		if (*wp) {
			MAYBE_ADD_SPACE();
			Buf_AddString(buf, wp);
		}
		break;
	}
	return addSpace||added;
}

/*-
 *-----------------------------------------------------------------------
 * VarModify --
 *	Modify each of the words of the passed string using the given
 *	function. Used to implement most modifiers.
 *
 * Results:
 *	A string of all the words modified appropriately.
 *-----------------------------------------------------------------------
 */
static char *
VarModify(char *str, 		/* String whose words should be trimmed */
				/* Function to use to modify them */
    bool (*modProc)(struct Name *, bool, Buffer, void *),
    void *datum)		/* Datum to pass it */
{
	BUFFER	  buf;		/* Buffer for the new string */
	bool	  addSpace;	/* true if need to add a space to the
				     * buffer before adding the trimmed
				     * word */
	struct Name	  word;

	Buf_Init(&buf, 0);
	addSpace = false;

	word.e = str;

	while ((word.s = iterate_words(&word.e)) != NULL) {
		char termc;

		termc = *word.e;
		*((char *)(word.e)) = '\0';
		addSpace = (*modProc)(&word, addSpace, &buf, datum);
		*((char *)(word.e)) = termc;
	}
	return Buf_Retrieve(&buf);
}

/*-
 *-----------------------------------------------------------------------
 * VarGetPattern --
 *	Pass through the tstr looking for 1) escaped delimiters,
 *	'$'s and backslashes (place the escaped character in
 *	uninterpreted) and 2) unescaped $'s that aren't before
 *	the delimiter (expand the variable substitution).
 *	Return the expanded string or NULL if the delimiter was missing
 *	If pattern is specified, handle escaped ampersands, and replace
 *	unescaped ampersands with the lhs of the pattern.
 *
 * Results:
 *	A string of all the words modified appropriately.
 *	If length is specified, return the string length of the buffer
 *-----------------------------------------------------------------------
 */
static char *
VarGetPattern(SymTable *ctxt, int err, const char **tstr, int delim1,
    int delim2, size_t *length, VarPattern *pattern)
{
	const char	*cp;
	char	*result;
	BUFFER	buf;
	size_t	junk;

	Buf_Init(&buf, 0);
	if (length == NULL)
		length = &junk;

#define IS_A_MATCH(cp, delim1, delim2) \
	(cp[0] == '\\' && (cp[1] == delim1 || cp[1] == delim2 || \
	 cp[1] == '\\' || cp[1] == '$' || (pattern && cp[1] == '&')))

	/*
	 * Skim through until the matching delimiter is found;
	 * pick up variable substitutions on the way. Also allow
	 * backslashes to quote the delimiter, $, and \, but don't
	 * touch other backslashes.
	 */
	for (cp = *tstr; *cp != '\0' && *cp != delim1 && *cp != delim2; cp++) {
		if (IS_A_MATCH(cp, delim1, delim2)) {
			Buf_AddChar(&buf, cp[1]);
			cp++;
		} else if (*cp == '$') {
			/* Allowed at end of pattern */
			if (cp[1] == delim1 || cp[1] == delim2)
				Buf_AddChar(&buf, *cp);
			else {
				size_t len;

				/* If unescaped dollar sign not before the
				 * delimiter, assume it's a variable
				 * substitution and recurse.  */
				(void)Var_ParseBuffer(&buf, cp, ctxt, err,
				    &len);
				cp += len - 1;
			}
		} else if (pattern && *cp == '&')
			Buf_AddChars(&buf, pattern->leftLen, pattern->lhs);
		else
			Buf_AddChar(&buf, *cp);
	}

	*length = Buf_Size(&buf);
	result = Buf_Retrieve(&buf);

	if (*cp != delim1 && *cp != delim2) {
		*tstr = cp;
		*length = 0;
		free(result);
		return NULL;
	}
	else {
		*tstr = ++cp;
		return result;
	}
}

/*-
 *-----------------------------------------------------------------------
 * VarQuote --
 *	Quote shell meta-characters in the string
 *
 * Results:
 *	The quoted string
 *-----------------------------------------------------------------------
 */
static char *
VarQuote(const char *str, const struct Name *n UNUSED, void *islistp)
{
	int *p = islistp;
	int islist = *p;

	BUFFER	  buf;
	/* This should cover most shells :-( */
	static char meta[] = "\n \t'`\";&<>()|*?{}[]\\$!#^~";
	char *rep = meta;
	if (islist)
		rep += 3;

	Buf_Init(&buf, MAKE_BSIZE);
	for (; *str; str++) {
		if (strchr(rep, *str) != NULL)
			Buf_AddChar(&buf, '\\');
		Buf_AddChar(&buf, *str);
	}
	return Buf_Retrieve(&buf);
}

static void *
check_empty(const char **p, SymTable *ctxt UNUSED, bool b UNUSED, int endc)
{
	dummy_arg->s = NULL;
	if ((*p)[1] == endc || (*p)[1] == ':') {
		(*p)++;
		return dummy_arg;
	} else
		return NULL;
}

static void *
check_quote(const char **p, SymTable *ctxt UNUSED, bool b UNUSED, int endc)
{
	int *qargs = emalloc(sizeof(int));
	*qargs = 0;
	if ((*p)[1] == 'L') {
		*qargs = 1;
		(*p)++;
	}
	if ((*p)[1] == endc || (*p)[1] == ':') {
		(*p)++;
		return qargs;
	} else  {
		free(qargs);
		return NULL;
	}
}

static void *
check_shcmd(const char **p, SymTable *ctxt UNUSED, bool b UNUSED, int endc)
{
	if ((*p)[1] == 'h' && ((*p)[2] == endc || (*p)[2] == ':')) {
		(*p)+=2;
		return dummy_arg;
	} else
		return NULL;
}


static char *
do_shcmd(const char *s, const struct Name *n UNUSED, void *arg UNUSED)
{
	char *err;
	char *t;

	t = Cmd_Exec(s, &err);
	if (err)
		Error(err, s);
	return t;
}

static void *
get_stringarg(const char **p, SymTable *ctxt UNUSED, bool b UNUSED, int endc)
{
	const char *cp;
	char *s;

	for (cp = *p + 1; *cp != ':' && *cp != endc; cp++) {
		if (*cp == '\\') {
			if (cp[1] == ':' || cp[1] == endc || cp[1] == '\\')
				cp++;
		} else if (*cp == '\0')
			return NULL;
	}
	s = escape_dupi(*p+1, cp, ":)}");
	*p = cp;
	return s;
}

static void
free_stringarg(void *arg)
{
	free(arg);
}

static char *
do_upper(const char *s, const struct Name *n UNUSED, void *arg UNUSED)
{
	size_t len, i;
	char *t;

	len = strlen(s);
	t = emalloc(len+1);
	for (i = 0; i < len; i++)
		t[i] = TOUPPER(s[i]);
	t[len] = '\0';
	return t;
}

static char *
do_lower(const char *s, const struct Name *n UNUSED, void *arg UNUSED)
{
	size_t	len, i;
	char	*t;

	len = strlen(s);
	t = emalloc(len+1);
	for (i = 0; i < len; i++)
		t[i] = TOLOWER(s[i]);
	t[len] = '\0';
	return t;
}

static void *
get_patternarg(const char **p, SymTable *ctxt, bool err, int endc)
{
	return common_get_patternarg(p, ctxt, err, endc, false);
}

/* Extract anchors */
static void *
get_spatternarg(const char **p, SymTable *ctxt, bool err, int endc)
{
	return common_get_patternarg(p, ctxt, err, endc, true);
}

static void *
common_get_patternarg(const char **p, SymTable *ctxt, bool err, int endc,
    bool dosubst)
{
	VarPattern *pattern;
	char delim;
	const char *s;

	pattern = emalloc(sizeof(VarPattern));
	pattern->flags = 0;
	s = *p;

	delim = s[1];
	if (delim == '\0')
		return NULL;
	s += 2;

	pattern->rhs = NULL;
	pattern->lhs = VarGetPattern(ctxt, err, &s, delim, delim,
	    &pattern->leftLen, NULL);
	pattern->lbuffer = pattern->lhs;
	if (pattern->lhs != NULL) {
		if (dosubst && pattern->leftLen > 0) {
			if (pattern->lhs[pattern->leftLen-1] == '$') {
				    pattern->leftLen--;
				    pattern->flags |= VAR_MATCH_END;
			}
			if (pattern->lhs[0] == '^') {
				    pattern->lhs++;
				    pattern->leftLen--;
				    pattern->flags |= VAR_MATCH_START;
			}
		}
		pattern->rhs = VarGetPattern(ctxt, err, &s, delim, delim,
		    &pattern->rightLen, dosubst ? pattern: NULL);
		if (pattern->rhs != NULL) {
			/* Check for global substitution. If 'g' after the
			 * final delimiter, substitution is global and is
			 * marked that way.  */
			for (;; s++) {
				switch (*s) {
				case 'g':
					pattern->flags |= VAR_SUB_GLOBAL;
					continue;
				case '1':
					pattern->flags |= VAR_SUB_ONE;
					continue;
				}
				break;
			}
			if (*s == endc || *s == ':') {
				*p = s;
				return pattern;
			}
		}
	}
	free_patternarg(pattern);
	return NULL;
}

static void
free_patternarg(void *p)
{
	VarPattern *vp = p;

	free(vp->lbuffer);
	free(vp->rhs);
	free(vp);
}

static char *
do_regex(const char *s, const struct Name *n UNUSED, void *arg)
{
	VarREPattern p2;
	VarPattern *p = arg;
	int error;
	char *result;

	error = regcomp(&p2.re, p->lhs, REG_EXTENDED);
	if (error) {
		VarREError(error, &p2.re, "RE substitution error");
		return var_Error;
	}
	p2.nsub = p2.re.re_nsub + 1;
	p2.replace = p->rhs;
	p2.flags = p->flags;
	if (p2.nsub < 1)
		p2.nsub = 1;
	if (p2.nsub > 10)
		p2.nsub = 10;
	p2.matches = ereallocarray(NULL, p2.nsub, sizeof(regmatch_t));
	result = VarModify((char *)s, VarRESubstitute, &p2);
	regfree(&p2.re);
	free(p2.matches);
	return result;
}

char *
VarModifiers_Apply(char *str, const struct Name *name, SymTable *ctxt,
    bool err, bool *freePtr, const char **pscan, int paren)
{
	const char *tstr;
	char endc = paren == '(' ? ')' : '}';
	const char *start = *pscan;

	tstr = start;
	/*
	 * Now we need to apply any modifiers the user wants applied.
	 * These are:
	 *		  :M<pattern>	words which match the given <pattern>.
	 *				<pattern> is of the standard file
	 *				wildcarding form.
	 *		  :S<d><pat1><d><pat2><d>[g]
	 *				Substitute <pat2> for <pat1> in the
	 *				value
	 *		  :C<d><pat1><d><pat2><d>[g]
	 *				Substitute <pat2> for regex <pat1> in
	 *				the value
	 *		  :H		Substitute the head of each word
	 *		  :T		Substitute the tail of each word
	 *		  :E		Substitute the extension (minus '.') of
	 *				each word
	 *		  :R		Substitute the root of each word
	 *				(pathname minus the suffix).
	 *		  :lhs=rhs	Like :S, but the rhs goes to the end of
	 *				the invocation.
	 */

	while (*tstr != endc && *tstr != '\0') {
		struct modifier *mod;
		void *arg;
		char *newStr;

		tstr++;
		if (DEBUG(VAR)) {
			if (str != NULL)
				printf("Applying :%c to \"%s\"\n", *tstr, str);
			else
				printf("Applying :%c\n", *tstr);
		}

		mod = choose_mod[(unsigned char)*tstr];
		arg = NULL;

		if (mod != NULL)
			arg = mod->getarg(&tstr, ctxt, err, endc);
		if (arg == NULL) {
			mod = &sysv_mod;
			arg = mod->getarg(&tstr, ctxt, err, endc);
		}
		if (arg != NULL) {
			if (str != NULL) {
				if (mod->word_apply != NULL) {
					newStr = VarModify(str,
					    mod->word_apply, arg);
					assert(mod->apply == NULL);
				} else
					newStr = mod->apply(str, name, arg);
				if (*freePtr)
					free(str);
				str = newStr;
				if (str != var_Error)
					*freePtr = true;
				else
					*freePtr = false;
			}
			if (mod->freearg != NULL)
				mod->freearg(arg);
		} else {
			Error("Bad modifier: %s", tstr);
			/* Try skipping to end of var... */
			while (*tstr != endc && *tstr != '\0')
				tstr++;
			if (str != NULL && *freePtr)
				free(str);
			str = var_Error;
			*freePtr = false;
			break;
		}
		if (DEBUG(VAR) && str != NULL)
			printf("Result is \"%s\"\n", str);
	}
	if (*tstr == '\0')
		Parse_Error(PARSE_FATAL, "Unclosed variable specification");
	else
		tstr++;

	*pscan = tstr;
	return str;
}

char *
Var_GetHead(char *s)
{
	return VarModify(s, VarHead, NULL);
}

char *
Var_GetTail(char *s)
{
	return VarModify(s, VarTail, NULL);
}
