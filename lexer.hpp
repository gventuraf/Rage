#ifndef LEXER_HPP
#define LEXER_HPP

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <map> //llvm
#include <string>

namespace Lexer
{

enum class Token_type :char {
    // keywords
    IF='i', ELSE='e', RETURN='r',
    FOR='f', TO='o', TRUE='u', FALSE='a', NONE='o',
    STREAM='s', IN='n', OUT='u',
    //
    // single characters
    NL='\n',
    LBRACE='{', RBRACE='}', SC=';',
    LPAR='(', RPAR=')', ASS='=', COMMA=',',
    PLUS='+', MINUS='-', STAR='*', DIV='/',
    HASH='#', LESS='<', GREATER='>',
    DOT='.',
    //
    // literals
    TYPE='y', NUM_LIT='L', STRING='S',
    FLOAT='F',
    // identifier
    ID='d',
};

static std::string markers {"{}(),.;=+-*/#<>"};

static std::unordered_map<std::string, Token_type> keyword_mappings = {
    // working
    {"if", Token_type::IF},
    {"else", Token_type::ELSE},
    {"return", Token_type::RETURN},
    {"stream", Token_type::STREAM},
    {"in", Token_type::IN},
    {"out", Token_type::OUT},
    // integrals
    {"int32", Token_type::TYPE},
    {"int8", Token_type::TYPE},
    {"float", Token_type::TYPE},
    // bool
    {"true", Token_type::TRUE},
    {"false", Token_type::FALSE},
    // others
    {"for", Token_type::FOR},
    {"to", Token_type::TO},
    {"none", Token_type::NONE},
};

struct Token {
    Token_type token_type;
    std::string value; // make it optional?
    explicit Token(Token_type t, std::string v) : token_type{t}, value{std::move(v)} {}
    explicit Token(Token_type t) : token_type{t} {}
};

class Tokenizer
{
public:
    explicit Tokenizer(const char* s) : src_file{s, std::ios_base::in} {}
    ~Tokenizer() { src_file.close(); }

    const Token* token() const;
    const Token* peek() const;

    //DEBUG: delete later
    const std::vector<Token>& debug_get_tokens();

    void tokenize();

private:
    std::fstream src_file;
    std::vector<Token> tokens;
    mutable size_t curr_token {0};

    void deal_with_name(const std::string& s);
    std::string fulfil_name(char c);
    std::string fulfil_numberlit(char c);
    void handle_space(char c);
};

}

#endif