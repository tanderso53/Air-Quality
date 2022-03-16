#include "at-parse.h"

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define ARRAY_LEN(array) sizeof(array)/sizeof(array[0])

static int _at_replace_cr(char *result, const char *str,
			  unsigned int len);

const char *at_rsp_token_as_str(const at_rsp_tk *tk)
{
	return tk->content;
}

int at_rsp_token_as_int(const at_rsp_tk *tk)
{
	return strtol(tk->content, NULL, 10);
}

int at_rsp_assign_token(const char *content, at_rsp_tk *tk)
{
	bool in_para = 0;
	bool is_esc = 0;
	unsigned int wi = 0;

	tk->type = AT_RSP_TK_TYPE_INT;

	for (unsigned int i = 0; i < AT_RESPONSE_STR_LEN; ++i) {
		char c = content[i];

		switch (c) {
		case '\\':
			if (is_esc) {
				is_esc = true;
				continue;
			}
			break;
		case '"':
			if (is_esc) {
				in_para = in_para ? false : true;
				tk->type = AT_RSP_TK_TYPE_STR;
				break;
			}
			continue;
		case '\0':
			tk->content[i] = c;

			if (in_para) {
				return -1;
			}

			return wi;
		default:
			tk->content[i] = c;
			++wi;
			break;
		}
	}

	tk->content[AT_RESPONSE_STR_LEN - 1] = '\0';

	return wi;
}

int at_rsp_tokenize_line(const char *line,
			 at_rsp_line_tokens *tok)
{
	char str[1028];
	char *lastp = NULL;
	char *starttk = NULL;
	unsigned int n_tok = 0;

	strncpy(str, line, sizeof(str) - 1);
	str[ARRAY_LEN(str) - 1] = '\0';

	for (char *tk = strtok_r(str, ":", &lastp); tk;
	     tk = strtok_r(NULL, ":", &lastp)) {
		if (lastp && tk != lastp) {
			strncpy(tok->preamble, tk,
				sizeof(tok->preamble) - 1);
			tok->preamble[ARRAY_LEN(tok->preamble) - 1] = '\0';
			starttk = lastp;
		}
	}

	if (!starttk) {
		return 0;
	}

	for (char *tk = strtok_r(starttk, ",", &lastp); tk;
	     tk = strtok_r(NULL, ",", &lastp)) {
		at_rsp_assign_token(tk, &tok->tokenlist[n_tok]);

		++n_tok;
	}

	tok->ntokens = n_tok;

	return n_tok;
}

int at_rsp_get_lines(const char *rsp,
		     at_rsp_lines *lines)
{
	char buf[4096];
	char *last;
	unsigned int n_tok = 0;

	_at_replace_cr(buf, rsp, ARRAY_LEN(buf));

	for (char *tk = strtok_r(buf, "\n", &last); tk;
	     tk = strtok_r(NULL, "\n", &last)) {
		at_rsp_line_tokens *line = &lines->tokenlists[n_tok];

		if (at_rsp_tokenize_line(tk, line) > 0) {
			++n_tok;
		}
	}

	lines->nlines = n_tok;

	return n_tok;
}

at_rsp_line_tokens *at_rsp_get_property(const char * prop,
					    at_rsp_lines *lines)
{
	for (unsigned int i = 0; i < lines->nlines; ++i) {
		if (strcmp(lines->tokenlists[i].preamble, prop) == 0) {
			return &lines->tokenlists[i];
		}
	}

	return NULL;
}

static int _at_replace_cr(char *result, const char *str,
			   unsigned int len)
{
	unsigned int wi = 0;

	for (unsigned int i = 0; i < len - 1; ++i) {
		char c = str[i];

		switch (c) {
		case '\0':
			return wi;
		case '\r':
			if (str[i + 1] != '\n') {
				result[wi] = '\n';
				++wi;
			}
			break;
		default:
			result[wi] = c;
			++wi;
			break;	
		}
	}

	result[len - 1] = '\0';

	return len - 1;
}
