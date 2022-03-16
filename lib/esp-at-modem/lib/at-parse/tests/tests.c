#include "at-parse.h"

#include "munit.h"

#include "string.h"

typedef struct {
	const char *name;
	const char *msg;
	const at_rsp_lines expected;
} test_rsp_param;

static test_rsp_param rsp_params[] = {
	{
		.name = "AT+CIPSTATUS",
		.msg =
		"AT+CIPSTATUS\r\n"
		"STATUS:3\r\n"
		"+CIPSTATUS:0,\"TCP\",\"192.168.5.114\",48706,333,1\r\n"
		"+CIPSTATUS:1,\"UDP\",\"192.168.5.211\",48740,333,1\r\n"
		"\r\n"
		"OK\r\n",
		.expected = {
			.cmd = {'\0'},
			.tokenlists = {
				{
					.preamble = "STATUS",
					.tokenlist = {
						{
							.content = "3",
							.type = AT_RSP_TK_TYPE_INT
						}
					},
					.ntokens = 1
				},
				{
					.preamble = "+CIPSTATUS",
					.tokenlist = {
						{
							.content = "0",
							.type = AT_RSP_TK_TYPE_INT
						},
						{
							.content = "TCP",
							.type = AT_RSP_TK_TYPE_STR
						},
						{
							.content = "192.168.5.114",
							.type = AT_RSP_TK_TYPE_STR
						},
						{
							.content = "48706",
							.type = AT_RSP_TK_TYPE_INT
						},
						{
							.content = "333",
							.type = AT_RSP_TK_TYPE_INT
						},
						{
							.content = "1",
							.type = AT_RSP_TK_TYPE_INT
						}
					}
				},
				{
					.preamble = "+CIPSTATUS",
					.tokenlist = {
						{
							.content = "1",
							.type = AT_RSP_TK_TYPE_INT
						},
						{
							.content = "UDP",
							.type = AT_RSP_TK_TYPE_STR
						},
						{
							.content = "192.168.5.211",
							.type = AT_RSP_TK_TYPE_STR
						},
						{
							.content = "48740",
							.type = AT_RSP_TK_TYPE_INT
						},
						{
							.content = "333",
							.type = AT_RSP_TK_TYPE_INT
						},
						{
							.content = "1",
							.type = AT_RSP_TK_TYPE_INT
						}
					}
				}
			},
			.nlines = 3
		}
	},
	{
		.name = "AT+CIPSTA?",
		.msg =
		"AT+CIPSTA?\r\n"
		"+CIPSTA:ip:\"192.168.5.105\"\r\n"
		"+CIPSTA:gateway:\"192.168.5.1\"\r\n"
		"+CIPSTA:netmask:\"255.255.255.0\"\r\n"
		"\r\n"
		"OK\r\n",
		.expected = {
			.cmd = {'\0'},
			.tokenlists = {
				{
					.preamble = "ip",
					.tokenlist = {
						{
							.content = "192.168.5.105",
							.type = AT_RSP_TK_TYPE_STR
						}
					}
				},
				{
					.preamble = "gateway",
					.tokenlist = {
						{
							.content = "192.168.5.1",
							.type = AT_RSP_TK_TYPE_STR
						}
					}
				},
				{
					.preamble = "netmask",
					.tokenlist = {
						{
							.content = "255.255.255.0",
							.type = AT_RSP_TK_TYPE_STR
						}
					}
				}
			},
			.nlines = 3
		}
	},
	{
		.name = "AT+CIPMUX?",
		.msg =
		"AT+CIPMUX?\r\n"
		"+CIPMUX:1\r\n"
		"\r\n"
		"OK\r\n",
		.expected = {
			.cmd = {'\0'},
			.tokenlists = {
				{
					.preamble = "+CIPMUX",
					.tokenlist = {
						{
							.content = "1",
							.type = AT_RSP_TK_TYPE_INT
						}
					},
					.ntokens = 1
				}
			},
			.nlines = 1
		}
	}
};

static char *cmd_list[] = {
	"AT+CIPSTATUS",
	"AT+CIPSTA?",
	"AT+CIPMUX?",
	NULL
};

static MunitParameterEnum test_param_list[] = {
	{
		.name = "cmd",
		.values = cmd_list
	},
	{
		.name = NULL,
		.values = NULL
	}
};

test_rsp_param *get_at_test_param(void *fixture, const char *cmd)
{
	test_rsp_param *par = (test_rsp_param*) fixture;

	for (unsigned int i = 0; &par[i]; ++i) {
		if (strcmp(par[i].name, cmd) == 0) {
			return &par[i];
		}
	}

	return NULL;
}

static MunitResult test_checkparse(const MunitParameter params[],
				   void *fixture)
{
	int ret;
	const char *cmd = munit_parameters_get(params, "cmd");
	test_rsp_param *par = get_at_test_param(fixture, cmd);
	at_rsp_lines parsed;

	munit_assert_not_null(par);

	ret = at_rsp_get_lines(par->msg, &parsed);
	munit_assert_int(ret, >=, 0);

	munit_assert_uint(parsed.nlines, ==, par->expected.nlines);

	for (unsigned int i = 0; i < parsed.nlines; ++i) {
		at_rsp_line_tokens *tk_p = &parsed.tokenlists[i];
		at_rsp_line_tokens *tk_x = &parsed.tokenlists[i];

		munit_assert_uint(tk_p->ntokens, ==, tk_x->ntokens);
		munit_assert_memory_equal(sizeof(tk_p->tokenlist),
					  tk_p->tokenlist,
					  tk_x->tokenlist);
	}

	return MUNIT_OK;
}

static MunitTest at_parse_tests[] = {
	{
		.name = "/parse-structure-test",
		.test = test_checkparse,
		.setup = NULL,
		.tear_down = NULL,
		.options = MUNIT_TEST_OPTION_NONE,
		.parameters = test_param_list
	},
	{
		.name = NULL,
		.test = NULL,
		.setup = NULL,
		.tear_down = NULL,
		.options = MUNIT_TEST_OPTION_NONE,
		.parameters = NULL
	}
};

static const MunitSuite at_parse_test_suite = {
	"/at-parse-suite",
	at_parse_tests,
	NULL,
	1,
	MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char *const argv[])
{
	return munit_suite_main(&at_parse_test_suite,
				(void*) &rsp_params,
				argc, argv);
}
