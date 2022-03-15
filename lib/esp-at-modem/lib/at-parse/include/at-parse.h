#ifndef AT_PARSE_H
#define AT_PARSE_H

#define AT_RESPONSE_MAX_LINES 10
#define AT_RESPONSE_MAX_TOKENS 15
#define AT_RESPONSE_STR_LEN 24

typedef enum {
	AT_CMD_TYPE_TEST = 0x01,
	AT_CMD_TYPE_QUERY = 0x02,
	AT_CMD_TYPE_SET = 0x04,
	AT_CMD_TYPE_EXEC = 0x08
} at_cmd_type;

typedef enum {
	AT_RSP_TK_TYPE_INT = 0x01,
	AT_RSP_TK_TYPE_STR = 0x02
} at_rsp_tk_type;

typedef struct {
	char content[AT_RESPONSE_STR_LEN];
	at_rsp_tk_type type;
} at_rsp_tk;

typedef struct {
	char preamble[AT_RESPONSE_STR_LEN];
	at_rsp_tk tokenlist[AT_RESPONSE_MAX_TOKENS];
	unsigned int ntokens;
} at_rsp_line_tokens;

typedef struct {
	char cmd[AT_RESPONSE_STR_LEN];
	at_cmd_type cmdtype;
	at_rsp_line_tokens tokenlists[AT_RESPONSE_MAX_LINES];
	unsigned int nlines;
} at_rsp_lines;

const char *at_rsp_token_as_str(const at_rsp_tk *tk);
int at_rsp_token_as_int(const at_rsp_tk *tk);
int at_rsp_assign_token(const char *content, at_rsp_tk *tk);
int at_rsp_tokenize_line(const char *line,
			 at_rsp_line_tokens *tok);
int at_rsp_get_lines(const char *rsp,
		     at_rsp_lines *lines);
at_rsp_line_tokens *at_rsp_get_property(const char * prop,
					at_rsp_lines *lines);

#endif /* #define AT_PARSE_H */
