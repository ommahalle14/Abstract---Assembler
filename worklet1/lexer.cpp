// =====================================================================
//  pooja_lexer.cpp  -  POOJA'S MODULE : Lexer / Tokenizer
//  ---------------------------------------------------------------------
//  '-' is ALWAYS its own MINUS token (never glued to a following digit)
//  so "mov eax, -5" and "[ebx-8]" and "[ebx - 8]" all tokenize the same
//  way; Om's parser is what turns MINUS+IMMEDIATE into a negative value.
// =====================================================================
#include "pooja_lexer.h"

vector<Token> tokenizeLine(const string& line, int lineNumber, vector<AsmError>& errors) {
    vector<Token> tokens;
    size_t i = 0;

    while (i < line.size()) {
        char c = line[i];
        int column = (int)i + 1;

        if (isspace((unsigned char)c)) { i++; continue; }      // whitespace
        if (c == ';') break;                                   // comment -> rest of line ignored

        if (c == ',') { tokens.push_back({TokenType::COMMA,    ",", lineNumber, column}); i++; continue; }
        if (c == '[') { tokens.push_back({TokenType::LBRACKET, "[", lineNumber, column}); i++; continue; }
        if (c == ']') { tokens.push_back({TokenType::RBRACKET, "]", lineNumber, column}); i++; continue; }
        if (c == '+') { tokens.push_back({TokenType::PLUS,     "+", lineNumber, column}); i++; continue; }
        if (c == '-') { tokens.push_back({TokenType::MINUS,    "-", lineNumber, column}); i++; continue; }
        if (c == '*') { tokens.push_back({TokenType::STAR,     "*", lineNumber, column}); i++; continue; }

        // number: decimal or 0x hex
        if (isdigit((unsigned char)c)) {
            size_t start = i;
            i++;
            while (i < line.size() && (isalnum((unsigned char)line[i]) || line[i] == '_')) i++;
            string text = line.substr(start, i - start);

            long long ignored;
            if (parseNumber(text, ignored)) {
                tokens.push_back({TokenType::IMMEDIATE, text, lineNumber, column});
            } else {
                addError(errors, "Lexical Error", "invalid immediate '" + text + "'",
                         lineNumber, column);
            }
            continue;
        }

        // word: mnemonic, register, keyword (BYTE/WORD/DWORD/PTR), label or identifier
        if (isalpha((unsigned char)c) || c == '_') {
            size_t start = i;
            while (i < line.size() && (isalnum((unsigned char)line[i]) || line[i] == '_')) i++;
            string word  = line.substr(start, i - start);
            string upper = toUpper(word);

            bool followedByColon = (i < line.size() && line[i] == ':');

            if (followedByColon) {
                i++;   // consume ':'
                if (isValidMnemonic(upper) || isValidRegister(upper) ||
                    upper == "BYTE" || upper == "WORD" || upper == "DWORD" || upper == "PTR") {
                    addError(errors, "Lexical Error",
                             "label name '" + word + "' is a reserved word", lineNumber, column);
                } else {
                    tokens.push_back({TokenType::LABEL, word, lineNumber, column});
                }
            } else if (isValidMnemonic(upper)) {
                tokens.push_back({TokenType::MNEMONIC, upper, lineNumber, column});
            } else if (isValidRegister(upper)) {
                tokens.push_back({TokenType::REGISTER, upper, lineNumber, column});
            } else {
                // IDENTIFIER covers label references AND the size keywords
                // BYTE/WORD/DWORD/PTR. Kept in ORIGINAL case (unlike
                // mnemonics/registers) so a label reference's spelling
                // matches its definition exactly as the symbol table key;
                // Om's parser upper-cases only when checking for a keyword.
                tokens.push_back({TokenType::IDENTIFIER, word, lineNumber, column});
            }
            continue;
        }

        addError(errors, "Lexical Error", string("invalid character '") + c + "'",
                 lineNumber, column);
        i++;
    }
    return tokens;
}
