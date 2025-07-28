#include "parser.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <umalloc.h>

#include "command_type.h"
#include "token_type.h"

static Token    advance(Parser *parser);
static bool     consume(Parser *parser, TokenType type);
static bool     is_at_end(Parser *parser);
static void     skip_nls(Parser *parser);
static bool     consume_newline(Parser *parser);
static Command *create_command(CommandType type);
static bool     is_variable(Token token);
static bool     parse_variable(Token token, int64_t *var_num);
static bool     parse_number(Token token, int64_t *result);
static bool     parse_variable_operand(Parser *parser, Operand *op);
static bool     parse_var_or_imm(Parser *parser, Operand *op, bool *is_immediate);
static Command *parse_cmd(Parser *parser);

void parser_init(Parser *parser, Lexer *lexer, LabelMap *map) {
    if (!parser) {
        return;
    }

    parser->lexer     = lexer;
    parser->had_error = false;
    parser->label_map = map;
    parser->current   = lexer_next_token(parser->lexer);
    parser->next      = lexer_next_token(parser->lexer);
}

/**
 * @brief Advances the parser in the token stream.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return The token that was just consumed.
 */
static Token advance(Parser *parser) {
    Token ret_token = parser->current;
    if (!is_at_end(parser)) {
        parser->current = parser->next;
        parser->next    = lexer_next_token(parser->lexer);
    }
    return ret_token;
}

/**
 * @brief Determines if the parser reached the end of the token stream.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return True if the parser is at the end of the token stream, false
 * otherwise.
 */
static bool is_at_end(Parser *parser) {
    return parser->current.type == TOK_EOF;
}

/**
 * @brief Consumes the token if it matches the specified token type.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param type The type of the token to match.
 * @return True if the token was consumed, false otherwise.
 */
static bool consume(Parser *parser, TokenType type) {
    if (parser->current.type == type) {
        advance(parser);
        return true;
    }

    return false;
}

/**
 * @brief Creates a command of the given type.
 *
 * @param type The type of the command to create.
 * @return A pointer to a command with the requested type.
 *
 * @note It is the responsibility of the caller to free the memory associated
 * with the returned command.
 */
static Command *create_command(CommandType type) {
    Command *cmd = (Command *) umalloc(sizeof(Command));
    if (!cmd) {
        return NULL;
    }
    memset(cmd, 0, sizeof(Command));

    cmd->type             = type;
    cmd->next             = NULL;
    cmd->is_a_immediate   = false;
    cmd->is_a_string      = false;
    cmd->is_b_immediate   = false;
    cmd->is_b_string      = false;
    cmd->branch_condition = BRANCH_NONE;
    return cmd;
}

/**
 * @brief Determines if the given token is a valid variable.
 *
 * A valid (potential) variable is a token that begins with the prefix "x",
 * followed by any other character(s).
 *
 * @param token The token to check.
 * @return True if this token could be a variable, false otherwise.
 */
static bool is_variable(Token token) {
    return token.length >= 2 && token.lexeme[0] == 'x';
}

/**
 * @brief Determines if the given token is a valid base signifier.
 *
 * A valid base signifier is one of d (decimal), x (hex), b (binary) or s (string).
 *
 * @param token The token to check.
 * @return True if this token is a base signifier, false otherwise
 */
static bool is_base(Token token) {
    return token.length == 1 && (token.lexeme[0] == 'd' || token.lexeme[0] == 'x' ||
                                 token.lexeme[0] == 's' || token.lexeme[0] == 'b');
}

/**
 * @brief Parses the given token as a base signifier
 *
 * A base is a single character, either d, s, x, or b.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @return True if the current token was parsed as a base, false otherwise.
 */
static bool parse_base(Parser *parser, Operand *op) {
    // STUDENT TODO: Parse the current token as a base
    if (is_base(parser->current)) {
        op->base = parser->current.lexeme[0];
        return true;
    }
    return false;
}

/**
 * @brief Parses the given token as a variable.
 *
 * @param token The token to parse.
 * @param var_num a pointer to modify on success.
 * @return True if `var_num` was successfully modified, false otherwise.
 *
 * @note It is assumed that the token already was verified to begin with a valid
 * prefix, "x".
 */
static bool parse_variable(Token token, int64_t *var_num) {
    char   *endptr;
    int64_t tempnum = strtol(token.lexeme + 1, &endptr, 10);

    if ((token.lexeme + token.length) != endptr || tempnum < 0 || tempnum > 31) {
        return false;
    }

    *var_num = tempnum;
    return true;
}

/**
 * @brief Parses the given value as a number.
 *
 * @param token The token to parse.
 * @param result A pointer to the value to modify on success.
 * @return True if `result` was successfully modified, false otherwise.
 */
static bool parse_number(Token token, int64_t *result) {
    const char *parse_start = token.lexeme;
    int         base        = 10;

    if (token.length > 2 && token.lexeme[0] == '0') {
        if (token.lexeme[1] == 'x') {
            parse_start += 2;
            base = 16;
        } else if (token.lexeme[1] == 'b') {
            parse_start += 2;
            base = 2;
        }
    }

    char *endptr;
    *result = strtoll(parse_start, &endptr, base);

    return (token.lexeme + token.length) == endptr;
}

/**
 * @brief Conditionally parses the current token as a number.
 *
 * Note that this won't advance the parser if the token cannot be converted to
 * an integer.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @return True if this token is a number and was was converted successfully,
 * false otherwise.
 */
static bool parse_im(Parser *parser, Operand *op) {
    // STUDENT TODO: Parse current token as an immediate
    if (parser->current.type == TOK_NUM) {
        return parse_number(parser->current, &op->num_val);
    }
    return false;
}

/**
 * @brief Parses the next token as a variable.
 *
 * A variable is anything starting with the prefix x and will be of type
 * TOK_IDENT.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @return True if this was parsed as a variable, false otherwise.
 */
static bool parse_variable_operand(Parser *parser, Operand *op) {
    // STUDENT TODO: Parse the current token as a variable
    if (parser->current.type == TOK_IDENT && is_variable(parser->current)) {
        return parse_variable(parser->current, &op->num_val);
    }
    return false;
}

/**
 * @brief Parses the next token as either a variable or an immediate.
 *
 * A number is considered to be anything beginning with a decimal digit or the
 * prefixes 0b or 0x and will be of type TOK_NUM. A variable is anything
 * starting with the prefix x and will be of type TOK_IDENT.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @param op A pointer to the operand to modify.
 * @param is_immediate A pointer to a boolean to modify upon determining whether
 * the given value is an immediate.
 * @return True if this was parsed as an immediate or a variable, false
 * otherwise.
 */
static bool parse_var_or_imm(Parser *parser, Operand *op, bool *is_immediate) {
    // STUDENT TODO: Parse the current token as a variable or an immediate
    if (parse_im(parser, op)) {
        *is_immediate = true;
        return true;
    } else {
        *is_immediate = false;
        return parse_variable_operand(parser, op);
    }
}

/**
 * @brief Skips past tokens that signal the start of a new line
 *
 * Consumes newlines until the end of file is reached.
 * An EOF is not considered to be a "new line" in this context because it is a
 * sentinel token, I.e, there is nothing after it.
 *
 * @param parser A pointer to the parser to read tokens from.
 */
static void skip_nls(Parser *parser) {
    while (consume(parser, TOK_NL))
        ;
}

/**
 * @brief Consumes a single newline
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return True whether a "new line" was consumed, false otherwise.
 *
 * @note An encounter of TOK_EOF should not be considered a failure, as this
 * procedure is designed to "reset" the grammar. In other words, it should be
 * used to ensure that we have a valid ending token after encountering an
 * instruction. Since TOK_EOF signals no more possible instructions, it should
 * effectively play the role of a new line when checking for a valid ending
 * sequence for a command.
 */
static bool consume_newline(Parser *parser) {
    return consume(parser, TOK_NL) || consume(parser, TOK_EOF);
}

static void error_occured(Parser *parser, Command *cmd) {
    parser->had_error = true;
    free_command(cmd);
}

/**
 * @brief Parses a singular command.
 *
 * Reads in the token(s) from the lexer that the parser owns and determines the
 * appropriate matching command. Updates the parser->had_error if an error
 * occurs.
 *
 * @param parser A pointer to the parser to read tokens from.
 * @return A pointer to the appropriate command.
 * Returns null if an error occurred or there are no commands to parse.
 *
 * @note The caller is responsible for freeing the memory associated with the
 * returned command.
 */
static Command *parse_cmd(Parser *parser) {
    // STUDENT TODO: Parse an individual command by looking at the current token's type
    // TODO: Skip newlines before anything else
    skip_nls(parser);
    // You will need to modify this later
    // However, this is fine for getting going
    Token token = parser->current;
    char *label = NULL;

    if (token.type == TOK_IDENT) {
        // TODO Week 4: Handle labels
        // be careful of edge cases!
        size_t labelLength = parser->current.length;
        label              = umalloc(sizeof(char *) * (labelLength + 1));
        if (label == NULL) {
            parser->had_error = true;
            ufree(label);
            return NULL;
        }
        strncpy(label, token.lexeme, labelLength);
        label[labelLength] = '\0';
        advance(parser);
        if (parser->current.type != TOK_COLON) {
            parser->had_error = true;
            ufree(label);
            return NULL;
        }
        advance(parser);
        skip_nls(parser);
        token = parser->current;
    }

    if (token.type == TOK_EOF) {
        // Week 4 TODO: If there is a label, put it there with a null command
        if (label != NULL) {
            put_label(parser->label_map, label, NULL);
        }
        ufree(label);
        // No commands to parse; we are done
        return NULL;
    }

    Command *cmd = NULL;
    switch (token.type) {
        // STUDENT TODO: Add cases handling different commands
        case TOK_MOV:
            cmd = create_command(CMD_MOV);
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->destination)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_im(parser, &cmd->val_a)) {
                error_occured(parser, cmd);
                return NULL;
            }
            cmd->is_a_immediate = true;
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_ADD:
            cmd = create_command(CMD_ADD);
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->destination)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->val_a)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_var_or_imm(parser, &cmd->val_b, &cmd->is_b_immediate)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_SUB:
            cmd = create_command(CMD_SUB);
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->destination)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->val_a)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_var_or_imm(parser, &cmd->val_b, &cmd->is_b_immediate)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_CMP:
            cmd = create_command(CMD_CMP);
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->val_a)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_var_or_imm(parser, &cmd->val_b, &cmd->is_b_immediate)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_CMP_U:
            cmd = create_command(CMD_CMP_U);
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->val_a)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_var_or_imm(parser, &cmd->val_b, &cmd->is_b_immediate)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_PRINT:
            cmd = create_command(CMD_PRINT);
            advance(parser);
            if (!parse_var_or_imm(parser, &cmd->val_a, &cmd->is_a_immediate)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_base(parser, &cmd->val_b)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_AND:
            cmd = create_command(CMD_AND);
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->destination)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->val_a)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->val_b)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_ORR:
            cmd = create_command(CMD_ORR);
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->destination)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->val_a)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->val_b)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_EOR:
            cmd = create_command(CMD_EOR);
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->destination)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->val_a)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->val_b)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_LSL:
            cmd = create_command(CMD_LSL);
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->destination)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->val_a)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_im(parser, &cmd->val_b)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_LSR:
            cmd = create_command(CMD_LSR);
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->destination)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->val_a)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_im(parser, &cmd->val_b)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_ASR:
            cmd = create_command(CMD_ASR);
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->destination)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->val_a)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_im(parser, &cmd->val_b)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_LOAD:
            cmd = create_command(CMD_LOAD);
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->destination)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_im(parser, &cmd->val_a)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_var_or_imm(parser, &cmd->val_b, &cmd->is_b_immediate)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_STORE:
            cmd = create_command(CMD_STORE);
            advance(parser);
            if (!parse_variable_operand(parser, &cmd->destination)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_var_or_imm(parser, &cmd->val_a, &cmd->is_a_immediate)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!parse_im(parser, &cmd->val_b)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_PUT:
            cmd = create_command(CMD_PUT);
            advance(parser);
            cmd->destination.str_val = umalloc(parser->current.length + 1);
            if (cmd->destination.str_val == NULL) {
                error_occured(parser, cmd);
                return NULL;
            }
            strncpy(cmd->destination.str_val, parser->current.lexeme, parser->current.length);
            cmd->destination.str_val[parser->current.length] = '\0';
            advance(parser);
            if (!parse_var_or_imm(parser, &cmd->val_a, &cmd->is_a_immediate)) {
                error_occured(parser, cmd);
                return NULL;
            }
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_BRANCH:
            cmd                   = create_command(CMD_BRANCH);
            cmd->branch_condition = BRANCH_ALWAYS;
            advance(parser);
            cmd->destination.str_val = umalloc(sizeof(char *) * ((parser->current.length) + 1));
            cmd->is_b_string         = true;
            if (cmd->destination.str_val == NULL) {
                error_occured(parser, cmd);
                return NULL;
            }
            strncpy(cmd->destination.str_val, parser->current.lexeme, parser->current.length);
            cmd->destination.str_val[parser->current.length] = '\0';
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_BRANCH_EQ:
            cmd                   = create_command(CMD_BRANCH);
            cmd->branch_condition = BRANCH_EQUAL;
            advance(parser);
            cmd->destination.str_val = umalloc(sizeof(char *) * ((parser->current.length) + 1));
            cmd->is_b_string         = true;
            if (cmd->destination.str_val == NULL) {
                error_occured(parser, cmd);
                return NULL;
            }
            strncpy(cmd->destination.str_val, parser->current.lexeme, parser->current.length);
            cmd->destination.str_val[parser->current.length] = '\0';
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_BRANCH_GE:
            cmd                   = create_command(CMD_BRANCH);
            cmd->branch_condition = BRANCH_GREATER_EQUAL;
            advance(parser);
            cmd->destination.str_val = umalloc(sizeof(char *) * ((parser->current.length) + 1));
            cmd->is_b_string         = true;
            if (cmd->destination.str_val == NULL) {
                error_occured(parser, cmd);
                return NULL;
            }
            strncpy(cmd->destination.str_val, parser->current.lexeme, parser->current.length);
            cmd->destination.str_val[parser->current.length] = '\0';
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_BRANCH_GT:
            cmd                   = create_command(CMD_BRANCH);
            cmd->branch_condition = BRANCH_GREATER;
            advance(parser);
            cmd->destination.str_val = umalloc(sizeof(char *) * ((parser->current.length) + 1));
            cmd->is_b_string         = true;
            if (cmd->destination.str_val == NULL) {
                error_occured(parser, cmd);
                return NULL;
            }
            strncpy(cmd->destination.str_val, parser->current.lexeme, parser->current.length);
            cmd->destination.str_val[parser->current.length] = '\0';
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_BRANCH_LE:
            cmd                   = create_command(CMD_BRANCH);
            cmd->branch_condition = BRANCH_LESS_EQUAL;
            advance(parser);
            cmd->destination.str_val = umalloc(sizeof(char *) * ((parser->current.length) + 1));
            cmd->is_b_string         = true;
            if (cmd->destination.str_val == NULL) {
                error_occured(parser, cmd);
                return NULL;
            }
            strncpy(cmd->destination.str_val, parser->current.lexeme, parser->current.length);
            cmd->destination.str_val[parser->current.length] = '\0';
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_BRANCH_LT:
            cmd                   = create_command(CMD_BRANCH);
            cmd->branch_condition = BRANCH_LESS;
            advance(parser);
            cmd->destination.str_val = umalloc(sizeof(char *) * ((parser->current.length) + 1));
            cmd->is_b_string         = true;
            if (cmd->destination.str_val == NULL) {
                error_occured(parser, cmd);
                return NULL;
            }
            strncpy(cmd->destination.str_val, parser->current.lexeme, parser->current.length);
            cmd->destination.str_val[parser->current.length] = '\0';
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_BRANCH_NEQ:
            cmd                   = create_command(CMD_BRANCH);
            cmd->branch_condition = BRANCH_NOT_EQUAL;
            advance(parser);
            cmd->destination.str_val = umalloc(sizeof(char *) * ((parser->current.length) + 1));
            cmd->is_b_string         = true;
            if (cmd->destination.str_val == NULL) {
                error_occured(parser, cmd);
                return NULL;
            }
            strncpy(cmd->destination.str_val, parser->current.lexeme, parser->current.length);
            cmd->destination.str_val[parser->current.length] = '\0';
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_RET:
            cmd = create_command(CMD_RET);
            advance(parser);
            if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        case TOK_CALL:
            cmd = create_command(CMD_CALL);
            advance(parser);
            if (parser->current.type == TOK_IDENT) {
                cmd->destination.str_val = umalloc(sizeof(char *) * ((parser->current.length) + 1));
                cmd->is_b_string = true;
                if (cmd->destination.str_val == NULL) {
                    error_occured(parser, cmd);
                    return NULL;
                }
                strncpy(cmd->destination.str_val, parser->current.lexeme, parser->current.length);
                cmd->destination.str_val[parser->current.length] = '\0';
                advance(parser);
                if (!consume_newline(parser) && parser->current.type != TOK_EOF) {
                    error_occured(parser, cmd);
                    return NULL;
                }
            } else {
                parser->had_error = true;
                error_occured(parser, cmd);
                return NULL;
            }
            break;
        default:
            parser->had_error = true;
            break;
    }
    if (label != NULL) {
        put_label(parser->label_map, label, cmd);
    }
    ufree(label);
    // TODO: Check for errors and consume newlines
    return cmd;
}

Command *parse_commands(Parser *parser) {
    // STUDENT TODO: Create a linked list of commands using parse_cmd as described in the handout
    // Change this!
    Command *head     = parse_cmd(parser);
    Command *headCopy = head;
    while (parser->current.type != TOK_EOF && parser->had_error == false) {
        Command *cmd = parse_cmd(parser);
        if (cmd == NULL) {
            break;
        }
        head->next = cmd;
        head       = head->next;
    }
    return headCopy;
}
