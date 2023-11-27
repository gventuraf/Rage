#ifndef PARSER_HPP
#define PARSER_HPP

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <map> //llvm
#include <string>
#include <memory> //unique_ptr

#include "lexer.hpp"

[[noreturn]] inline void ERROR(const char* msg) {
    std::cout << "Error: " << msg << std::endl;
    exit(1);
}

namespace Semantic_Parser
{

// LLVM utils
extern std::unique_ptr<llvm::LLVMContext> TheContext;
extern std::unique_ptr<llvm::Module> TheModule;
extern std::unique_ptr<llvm::IRBuilder<>> Builder;
extern std::map<std::string, llvm::AllocaInst*> NamedValues;

enum class Math_Op :char {
    PLUS='+', MINUS='-', MULT='*', DIV='/'
};

 extern std::string math_ops;

extern std::unordered_map<Math_Op, int> op_precedence;

struct Ret_Val {
    bool yes{false};
    Lexer::Token_type data_type;
    llvm::Value *val;
};
extern Ret_Val ret_val;

class AST_Node {
public:
    virtual llvm::Value *codegen() =0;
    virtual ~AST_Node() =default; //why is this needed?
};

class Expr_AST {
public:
    virtual llvm::Value *codegen() =0;
    virtual ~Expr_AST() =default;
};


class Binary_Expr_AST : public Expr_AST {
public:
    Math_Op op;
    std::unique_ptr<Expr_AST> LHS;
    std::unique_ptr<Expr_AST> RHS;
    
    explicit Binary_Expr_AST() =default;
    
    explicit Binary_Expr_AST(Math_Op o,
        std::unique_ptr<Expr_AST> L, std::unique_ptr<Expr_AST> R)
        : op{o}, LHS{std::move(L)}, RHS{std::move(R)} {}
    
    llvm::Value *codegen();
};

class Number_Expr_AST : public Expr_AST {
    double val;
public:
    explicit Number_Expr_AST(double v) : val{v} {}
    llvm::Value *codegen();
};

class Var_Expr_AST : public Expr_AST {
    std::string name;
public:
    explicit Var_Expr_AST(const std::string& n) : name{n} {} // is move() worth it?
    
    llvm::Value *codegen();
};

class Var_Declaration_AST : public AST_Node {
public:
    std::string data_type;
    std::string var_name;
    std::unique_ptr<Expr_AST> expr;
    explicit Var_Declaration_AST(std::string dt, std::string vn, std::unique_ptr<Expr_AST> ex)
        : data_type{dt}, var_name{std::move(vn)}, expr{(std::move(ex))} {}
    
    llvm::Value *codegen();

    // helper function to ensure that 'alloca's are created
    // at the beginning of the function
    // TmpB is pointing at the first instruction of the entry block of the function
    // assumes varible type is double (for now) //TODO
    static llvm::AllocaInst *create_alloca_in_entryblock(llvm::Function *TheFunction, const std::string &var_name)
    {
        llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
        return TmpB.CreateAlloca(llvm::Type::getDoubleTy(*TheContext), nullptr, var_name);
    }
};

class Var_Assignment_AST : public AST_Node
{
    std::string id;
    std::unique_ptr<Expr_AST> expr;
public:
    Var_Assignment_AST(std::string i, std::unique_ptr<Expr_AST> e)
        : id{i}, expr{std::move(e)} {}
    
    llvm::Value* codegen();
};

class Return_AST : public AST_Node {
public:
    std::unique_ptr<Expr_AST> expr;
    explicit Return_AST(std::unique_ptr<Expr_AST> ex) : expr{std::move(ex)} {}

    llvm::Value *codegen();
};

class If_Else_AST : public AST_Node {
    std::unique_ptr<Expr_AST> cond;
    std::vector<std::unique_ptr<AST_Node>> if_body, else_body;
public:
    If_Else_AST(std::unique_ptr<Expr_AST> c, std::vector<std::unique_ptr<AST_Node>> i, std::vector<std::unique_ptr<AST_Node>> e)
        : cond{std::move(c)}, if_body{std::move(i)}, else_body{std::move(e)} {}
    
    llvm::Value* codegen();
};

class Function_AST : public AST_Node {
public:
    std::string ty; //type
    std::string name;
    // args
    std::vector<std::unique_ptr<AST_Node>> body;
    explicit Function_AST(std::string t, std::string n, std::vector<std::unique_ptr<AST_Node>> b)
        : ty{t}, name{n}, body{std::move(b)} {}
    
    llvm::Value* codegen();
};

class Stream_AST : public AST_Node {
    std::string id;
    std::string internal_func_name;

public:
    Stream_AST(std::string i, std::string f)
        : id{i}, internal_func_name{f} {}

    llvm::Value* codegen();
};

class AST
{
public:
    AST(const Lexer::Tokenizer& t) : toker{t} {}
    void parser();

private:
    using TT = Lexer::Token_type;
    //using Tk = Lexer::Token;
    const Lexer::Tokenizer& toker;
    const Lexer::Token* tok;

    // big problem: in all of my code im not checking if the value is nullptr before accessing it
    inline const Lexer::Token* next_token() { return tok = toker.token(); }
    inline void ignore_token(Lexer::Token_type tt) { while (TT::NL == toker.peek()->token_type) next_token(); }

    bool handle_function_def();
    std::unique_ptr<AST_Node> handle_statement();
    std::unique_ptr<Stream_AST> handle_stream();
    std::unique_ptr<Var_Assignment_AST> handle_assignment();
    std::unique_ptr<If_Else_AST> handle_if();
    std::unique_ptr<Var_Declaration_AST> handle_var_decl();
    std::unique_ptr<Return_AST> handle_return();
    std::unique_ptr<Expr_AST> handle_expr(std::unique_ptr<Binary_Expr_AST>* prev_exp=nullptr);

    inline bool is_math_op(TT tt)
    {
        return std::string::npos != math_ops.find(static_cast<char>(tt));
    }
};

} // end namespace Semantic_Parser

#endif