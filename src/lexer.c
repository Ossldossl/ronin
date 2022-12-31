#include "include/misc.h"

#define ADVANCE char c = advance(com); if ((c) == -1) { print_message(COLOR_RED, "ERROR: Unexpected EOF at %d:%d", com->line, com->col); panic("\n");}
#define no_eof(c) if ((c) == true) { print_message(COLOR_RED, "ERROR: Unexpected EOF at %d:%d", com->line, com->col); panic("\n"); }
#define ADVANCE_OR_RET_TRUE char c = advance(com); if ((c) == -1) { return true;}
#define ADVANCE_OR_RET char c = advance(com); if ((c) == -1) { return;}


static char advance(compiler_t* com)
{
    if (com->index == com->file_size + 1) {
        return -1; 
    }
    char result = com->file_content[com->index];
    com->index++;
    com->col++;
    return result;
}

static char peek(compiler_t* com) 
{
    if (com->index == com->file_size + 1) {
        return -1;
    }
    char result = com->file_content[com->index];
    return result;
}

static bool skip_until_newline(compiler_t* com) 
{
    char ch = 'h';
    ADVANCE_OR_RET_TRUE
    ch = c;
    while (true) {
        if (ch == 13) {
            com->index += 1;
            com->col = 0;
            com->line++;
            return false;
        }
        if (ch == '\n') {
            com->index++;
            com->line++;
            com->col = 0;
            printf("skip: %c", ch);
            return false;
        }
        ADVANCE_OR_RET_TRUE
        ch = c;
    }
}

static int search_whitespace(compiler_t* com) 
{
    int index = com->index;
    char c = com->file_content[index];
    while (c != '\n' && c != '\x20' && c != '\x09' && c != '\r' && index < com->file_size) {
        index++;
        c = com->file_content[index];
    }
    if (index == com->index) return com->index;
    return index - 1;
}

// returns true when eof reached
static bool skip_whitespace_or_comment(compiler_t* com) 
{
    ADVANCE
    while (true) {
        if (c == '\x20' || c == '\x09') {
            ADVANCE_OR_RET_TRUE
            continue;
        }
        if (c == '/') {
            ADVANCE_OR_RET_TRUE
            if (c == '/') {

                bool eof = skip_until_newline(com);
                no_eof(eof);
                continue;
            }
        }
        return false;
    }
}

#define TOKEN(len, token_type) token_t* tok = arena_alloc(com->token_t_allocator); \
                          tok->col    = com->col;                                  \
                          tok->line   = com->line;                                 \
                          tok->length = len;                                       \
                          tok->type   = token_type;                                \

#define dToken(next_char, tok1, tok2) c = peek(com);                               \
                                      if (c == next_char) {                        \
                                        TOKEN(2, tok2)                             \
                                        advance(com);                              \
                                        continue;                                  \
                                      }                                            \
                                      TOKEN(1, tok1)                               \
                                      continue;                                    \

static void parse_string_literal(compiler_t* com) 
{
    int len = 0;
    int index = com->index;
    while (true) {
        ADVANCE
        len++;
        if (c == '"') {
            TOKEN(len - 1, TOKEN_STRING_LITERAL)
            tok->index = com->index;
            return;
        }
    }
}

static bool parse_hex_literal(compiler_t* com) 
{
    int next_whitespace_index = search_whitespace(com);
    int result = 0;
    int index = 0;
    for (int i = next_whitespace_index - com->index; i > 0; i--) {
        char c = com->file_content[com->index + i];
        if (c == '_') continue;
        if (c == '0') {index++; continue;}

        int digit = -1;
        if      (c >= '1' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else {
            MAKE_ERROR(false, com->line, com->col + next_whitespace_index - com->index, 1, "error: Invalid character in Hex number literal")
            TOKEN(next_whitespace_index - com->index, TOKEN_ERROR);
            return false;
        }
        result += digit * pow(16, index);
        index++;
    }
    if (index == 0) {
        MAKE_ERROR(false, com->line, com->col - 1, 2, "error: Hex number literal cannot be empty")
        TOKEN(next_whitespace_index - com->index, TOKEN_ERROR);
        return false;
    }
    TOKEN(next_whitespace_index - com->index + 2, TOKEN_I_NUMBER_LITERAL)
    tok->value = result;
    com->col  += next_whitespace_index - com->index;
    com->index = next_whitespace_index;
    return false;
}

static bool parse_bin_literal(compiler_t* com) 
{
    int len = 0;
    return false;
}

static bool parse_oct_literal(compiler_t* com) 
{
    int len = 0;
    return false;
}

static bool parse_number_literal(compiler_t* com) 
{
    char c = peek(com);
    if (c == -1) { panic("EOF!");}

    switch (c) {
        case 'x': return parse_hex_literal(com);
        case 'b': return parse_bin_literal(com);
        case 'o': return parse_oct_literal(com);
    }   
    int len = 0;
    return false;
}

void lexer_lex_file(compiler_t* com)
{
    // skip utf-8 sequence
    if (com->file_content[0] == '\xEF' && com->file_content[1] == '\xBB' && com->file_content[2] == '\xBF')
	{
		com->index += 3;
	}

    com->token_t_allocator = arena_new(sizeof(token_t), 5);

    while (com->index < com->file_size + 1) {
        ADVANCE_OR_RET
        if (c == '\r') {
            com->index++;
            com->line++;
            com->col = 0;
            continue;
        }
        if (c == '\n') {
            com->index++;
            com->line++;
            com->col = 0;
            continue;
        }
        #pragma region single_ops
        if (c == '+') {
            TOKEN(1, TOKEN_PLUS)
            continue;
        }
        if (c == '-') {
            TOKEN(1, TOKEN_MINUS)
            continue;
        }
        if (c == '*') {
            TOKEN(1, TOKEN_MUL)
            continue;
        }
        if (c == '%') {
            TOKEN(1, TOKEN_MOD)
            continue;
        }
        if (c == ';') {
            TOKEN(1, TOKEN_SEMI)
            continue;
        }
        if (c == '(') {
            TOKEN(1, TOKEN_LPAREN)
            continue;
        }
        if (c == ')') {
            TOKEN(1, TOKEN_RPAREN)
            continue;
        }
        if (c == '^') {
            TOKEN(1, TOKEN_XOR)
            continue;
        }
        #pragma endregion single_ops
        #pragma region two_char_ops
        if (c == '/') {
            c = peek(com);
            if (c == '/') {
                token_t *tok = arena_alloc(com->token_t_allocator);
                tok->col = com->col;
                tok->line = com->line;
                tok->length = 2;
                tok->type = TOKEN_COMMENT;
                advance(com);
                bool eof = skip_until_newline(com);
                if (eof) break;
                continue;
            }
            token_t *tok = arena_alloc(com->token_t_allocator);
            tok->col = com->col;
            tok->line = com->line;
            tok->length = 1;
            tok->type = TOKEN_DIV;
            continue;
        }
        if (c == '=') {
            dToken('=', TOKEN_SEQ, TOKEN_EQ)
        }
        if (c == '!') {
            dToken('=', TOKEN_NOT, TOKEN_NEQ)
        }
        if (c == '<') {
            dToken('=', TOKEN_LT, TOKEN_LEQ)
        }
        if (c == '>') {
            dToken('=', TOKEN_GT, TOKEN_GEQ)
        }
        if (c == '&') {
            dToken('&', TOKEN_BAND, TOKEN_AND)
        }
        if (c == '|') {
            dToken('|', TOKEN_BOR, TOKEN_OR)
        }
        #pragma endregion two_char_ops
        if (c == '\"') {
            parse_string_literal(com);
            continue;
        }
        if (c == '0') {        
            parse_number_literal(com);
        }
    }

    for (int i = 0; i <= com->token_t_allocator->index; i++) {
        token_t* tok = arena_get(com->token_t_allocator, i);
        if (tok == null) break;
        const char* token_name = token_names[tok->type];
        printf("%d: type: %s => %d\n", i, token_name, tok->value);
    }
}