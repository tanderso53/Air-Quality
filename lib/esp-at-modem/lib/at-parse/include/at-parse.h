/*
* Copyright (c) 2022 Tyler J. Anderson.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in
*    the documentation and/or other materials provided with the
*    distribution.
*
* 3. Neither the name of the copyright holder nor the names of its
*    contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
* ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file at-parse.h
 * @brief Public header for the at command and response parsing
 * library
 */

#ifndef AT_PARSE_H
#define AT_PARSE_H

/**
 * @defgroup atparseapi AT Command and Response Parsing Library
 * @{
 */

#define AT_RESPONSE_MAX_LINES	10 /**< Max number of response lines */
#define AT_RESPONSE_MAX_TOKENS	15 /**< Max tokens per line */
#define AT_RESPONSE_STR_LEN	24 /**< String buffer sizes */

/** @brief Possible AT command types
 *
 * @note Not currently used by the library
 */
typedef enum {
	AT_CMD_TYPE_TEST	= 0x01, /**< AT+<COMMANDNAME>=? */
	AT_CMD_TYPE_QUERY	= 0x02, /**< AT+<COMMANDNAME>? */
	AT_CMD_TYPE_SET		= 0x04, /**< AT+<COMMANDNAME>=<...> */
	AT_CMD_TYPE_EXEC	= 0x08  /**< AT+<COMMANDNAME> */
} at_cmd_type;

/** @brief Token content data types
 *
 * Data types, such as integer and string, that a token can be
 * converted to
 */
typedef enum {
	AT_RSP_TK_TYPE_INT	= 0x01, /**< Convertable to integer */
	AT_RSP_TK_TYPE_STR	= 0x02  /**< Convertable to string */
} at_rsp_tk_type;

/** @brief Token object
 *
 * An AT command response line may contain tokens. If the tokens are
 * surrounded by quotes, then it is of @em string type. Otherwise the
 * token is of @em integer type.
 */
typedef struct {
	char content[AT_RESPONSE_STR_LEN]; /**< Buffer containing token */
	at_rsp_tk_type type; /**< Type the token is convertable to */
} at_rsp_tk;

/** @brief Parsed line object
 *
 * An AT command response will consist of multiple lines. The library
 * will ignore any lines with no useful data and parse the lines of
 * the format:
 *
 * @verbatim
 * <preamble>:<token1>,<token2>,...
 * @endverbatim
 *
 * Each parsed lines will be stored in this structure.
 */
typedef struct {
	/**
	 * The preamble may be used to find the line from a list
	 * of lines with the @ref at_rsp_get_property function.
	 */
	char preamble[AT_RESPONSE_STR_LEN];

	/**
	 * List of tokens of type @ref at_rsp_tk found on
	 * the line
	 */
	at_rsp_tk tokenlist[AT_RESPONSE_MAX_TOKENS];

	/**
	 * Number of tokens found on the line
	 */
	unsigned int ntokens;
} at_rsp_line_tokens;

/** @brief Parsed AT response
 *
 * The object produced by parsing a full response to an AT command.
 *
 * @note Only lines with useful data are parsed
 */
typedef struct {
	/** @brief Command string
	 *
	 * Command the response was from
	 *
	 * @warning The cmd buffer is not used by the library and may
	 * be removed in future versions
	 */
	char cmd[AT_RESPONSE_STR_LEN];

	/** @brief Command Type
	 *
	 * AT command classification the command was in response to
	 *
	 * @warning cmdtype is not used by the library and may be
	 * removed in future versions
	 */
	at_cmd_type cmdtype;

	/** @brief List of parsed lines
	 *
	 * List of parsed lines that contained preambles and tokens
	 */
	at_rsp_line_tokens tokenlists[AT_RESPONSE_MAX_LINES];

	/** @brief Number of parsed lines
	 *
	 * The number of lines in the tokenlists property
	 */
	unsigned int nlines;
} at_rsp_lines;

/** @brief Return the content of a token as a c-string */
const char *at_rsp_token_as_str(const at_rsp_tk *tk);

/** @brief Return the content of a token as an int */
int at_rsp_token_as_int(const at_rsp_tk *tk);

/** @brief Assign the value of a token
 *
 * The string passed as @em content will be parsed to determine
 * whether the token is @em string type or @em integer type
 *
 * @param content C-string with raw content to be assigned to the token
 *
 * @param tk the @ref at_rsp_tk token object to assign the value to
 *
 * @return Number of char written on success, <0 on failure
 */
int at_rsp_assign_token(const char *content, at_rsp_tk *tk);

/** @brief Parse a line to obtain a list of tokens
 *
 * Creates a list of parsed lines that can be used to build an
 * @ref at_rsp_lines object's tokenlists property.
 *
 * @param line Raw line from AT command response with '\r''\n' replaced
 * with '\0'
 *
 * @param tok Token buffer to fill with parsed lines
 *
 * @return number of parsed lines placed in tok
 */
int at_rsp_tokenize_line(const char *line,
			 at_rsp_line_tokens *tok);

/** @brief Parse a full AT command response
 *
 * Parses a raw response in the null-terminated string @em line into
 * the provided @ref at_rsp_lines object
 *
 * @param rsp The raw null-terminated response to the AT command
 *
 * @param lines Structure to store parsed lines
 *
 * @return number of parsed lines
 */
int at_rsp_get_lines(const char *rsp,
		     at_rsp_lines *lines);

/** @brief Find a list of tokens associated with a given preamble
 *
 * @param prop Null-terminated string exactly matching the preamble of
 * the token list to match
 *
 * @param lines List of parse AT command response to search for a line
 * with the matching preamble
 *
 * @return A pointer to the @ref at_rsp_line_tokens object with the
 * matching preamble if found, NULL if not found
 */
at_rsp_line_tokens *at_rsp_get_property(const char *prop,
					at_rsp_lines *lines);

/**
 * @}
 */
/* @defgroup atparseapi AT Command and Response Parsing Library */

#endif /* #define AT_PARSE_H */
