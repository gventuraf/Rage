#include <iostream>
#include <string>
#include <memory> //unique_ptr

#include "lexer.hpp"
#include "parser.hpp"

namespace Semantic_Parser
{
std::unique_ptr<llvm::LLVMContext> TheContext = std::make_unique<llvm::LLVMContext>();
std::unique_ptr<llvm::Module> TheModule = std::make_unique<llvm::Module>("Rage Language", *Semantic_Parser::TheContext);
std::unique_ptr<llvm::IRBuilder<>> Builder = std::make_unique<llvm::IRBuilder<>>(*Semantic_Parser::TheContext);
std::map<std::string, llvm::AllocaInst*> NamedValues = {};
}

int main(int argc, char* argv[])
{
    Lexer::Tokenizer tokenizer {argv[1]};
    tokenizer.tokenize();

    if (3 == argc) {
        for (auto t : tokenizer.debug_get_tokens()) {
            std::cout << static_cast<char>(t.token_type) << ' ';
        }
        std::cout << '\n';
    }

    Semantic_Parser::AST parser {tokenizer};
    parser.parser();

    // print the IR
    Semantic_Parser::TheModule->print(llvm::outs(), nullptr);

    return 0;
}