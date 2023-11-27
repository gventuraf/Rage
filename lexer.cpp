#include "lexer.hpp"

namespace Lexer
{

// returns a pointer to be able to have a null value
const Token* Tokenizer::token() const { return curr_token<tokens.size() ? &tokens[curr_token++] : nullptr; }
const Token* Tokenizer::peek() const { return curr_token<tokens.size() ? &tokens[curr_token] : nullptr; }

void Tokenizer::tokenize()
{
    char c;
    while (src_file.get(c)) {
        if (std::isalpha(c) || '_'==c) {
            std::string s = fulfil_name(c);
            deal_with_name(s);
            //TODO should expect some sort of space or bracket after
        } else if (std::isdigit(c)) {
            std::string s = fulfil_numberlit(c);
            tokens.push_back(Token{Token_type::NUM_LIT, s});
            //TODO should expect some sort of space or bracket after
        } else if (std::isspace(c)) {
            handle_space(c);
        } else if (std::string::npos != markers.find(c)) {
            // single characters
            tokens.push_back(Token{static_cast<Token_type>(c)});
        } else {
            std::cout << "Error: Lexer: Invalid character '" << c << '\'' << std::endl;
            exit(1);
        }
    }
}

//DEBUG: delete later
const std::vector<Token>& Tokenizer::debug_get_tokens()
{
    return tokens;
}

void Tokenizer::deal_with_name(const std::string& s)
{
    auto v = keyword_mappings.find(s);
    if (keyword_mappings.end() != v) {
        if (Token_type::TYPE == v->second)
            tokens.push_back(Token{v->second, s});
        else
            tokens.push_back(Token{v->second});
    } else {
        tokens.push_back(Token{Token_type::ID, s});
    }
}

std::string Tokenizer::fulfil_name(char c)
{
    std::string s {c};
    while(std::isalnum(src_file.peek()) || src_file.peek()=='_')
        s.push_back(src_file.get());
    return s;
}

// numbers could be written as eg. 10_000 too (just bc of readibility)
std::string Tokenizer::fulfil_numberlit(char c)
{
    bool saw_dot = false;
    std::string s {c};
    char q = static_cast<char>(src_file.peek());
    
    while(std::isdigit(q) || (!saw_dot && '.'==q && (saw_dot=true))) {
        s.push_back(src_file.get());
        q=src_file.peek();
    }

    
    return s;
}

void Tokenizer::handle_space(char c)
{
    switch(c) {
    case '\t': case ' ':
        break;
    case '\n':
        tokens.push_back(Token{Token_type::NL});
        break;
    case '\r':
        src_file.get(c);
        if ('\n' == c) {
            tokens.push_back(Token{Token_type::NL});
            break;
        } else {
            std::cout << "Error: Lexer: handle_space(): Invalid character after '\\r'." << std::endl;
            exit(1);
        }
    default:
        std::cout << "Error: Lexer: handle_space(): unexpected space char." << std::endl;
        exit(1);

    }
}

}