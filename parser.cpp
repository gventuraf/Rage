#include "parser.hpp"

namespace Semantic_Parser
{
    std::string math_ops {"+-*/"};
    
    std::unordered_map<Math_Op, int> op_precedence = {
    {Math_Op::PLUS, 50},
    {Math_Op::MINUS, 50},
    {Math_Op::MULT, 60},
    {Math_Op::DIV, 60}, 
};
    
    Ret_Val ret_val;

void AST::parser()
{
    while (handle_function_def()) {
    }
}

bool AST::handle_function_def()
{
    if (!next_token())
        return false;
    
    if (TT::TYPE != tok->token_type) {
        std::cout << "Got " << static_cast<char>(tok->token_type) << '\n';
        ERROR("In handle_function_def(): expected TYPE");
    }
    std::string func_type {tok->value};

    ignore_token(TT::NL);
    
    if (TT::ID != next_token()->token_type)
        ERROR("In handle_function_def(): expected ID");
    std::string func_name {tok->value};
    
    if (TT::LPAR != next_token()->token_type
        || TT::RPAR != next_token()->token_type) {
        ERROR("In handle_function_def(): expected '()'");
    }

    ignore_token(TT::NL);
    if (TT::LBRACE != next_token()->token_type)
        ERROR("In handle_function_def(): expected '{'");

    std::vector<std::unique_ptr<AST_Node>> body;
    while (true) {
        auto s = handle_statement();
        if (!s) break;
        body.push_back(std::move(s));
    }

    if (!ret_val.yes)
        ERROR("In handle_function(): function must return a value");  // asume all functions return int32
    else
        ret_val.yes = false;
    // in void functions i should add the Builder->CreateRetVoid() myself if not present

    if (TT::RBRACE != next_token()->token_type)
        ERROR("In handle_function_def(): expected '}'");

    Function_AST func {func_type, func_name, std::move(body)};
    func.codegen();

    return true;
}

//TODO maybe Statement_AST instead of AST_Node
// rn both instructions end with an expression
// which ends when a TT::NL is reached
std::unique_ptr<AST_Node> AST::handle_statement()
{
    ignore_token(TT::NL);

    switch (toker.peek()->token_type)
    {
    case TT::STREAM:
        return handle_stream();
    case TT::TYPE:
        return handle_var_decl(); // can pass as parameter what token_type to end on, e.g TT::NL
    case TT::RETURN: {
        std::unique_ptr<Return_AST> ret {handle_return()};
        ignore_token(TT::NL);
        if (TT::RBRACE != toker.peek()->token_type)
            ERROR("After 'return' expect '}'");
        ret_val.yes = true;
        return std::move(ret);
    }
    case TT::IF:
        return handle_if();
    case TT::ID:
        return handle_assignment();
    case TT::RBRACE:
        //!
        //TODO
        // if function does not return, i should catch the error here
        return nullptr;
    default:
        std::cout << "Token read is " << static_cast<char>(tok->token_type) << " and value is " << tok->value << '\n';
        ERROR("Unexpected statement");
    }
}


std::unique_ptr<Stream_AST> AST::handle_stream()
{
    next_token(); // eat 'stream'
    
    if (TT::DOT != next_token()->token_type)
        ERROR("In handle_stream: expected '.'.");
    
    TT op = next_token()->token_type;
    if (TT::IN != op && TT::OUT != op)
        ERROR("In handle_stream: expected 'in' or 'out'.");
    
    if (TT::ID != next_token()->token_type)
        ERROR("In handle_stream: expected an ID.");
    
    return std::make_unique<Stream_AST>(tok->value, op==TT::IN ? "scanf" : "printf");
}

std::unique_ptr<Var_Assignment_AST> AST::handle_assignment()
{
    //TODO ignore TT::NL (?)
    std::string id {next_token()->value};
    if (TT::ASS != next_token()->token_type)
        ERROR("In handle_assignment(): expected '='");
    std::unique_ptr<Expr_AST> expr {handle_expr()};
    if (!expr) ERROR("In handle_assignment(): invalid expression");
    return std::make_unique<Var_Assignment_AST>(id, std::move(expr));
}

std::unique_ptr<If_Else_AST> AST::handle_if()
{
    next_token(); // eat 'if'
    
    std::unique_ptr<Expr_AST> cond = handle_expr();
    if (!cond)
        ERROR("In handle_if(): invalid condition.");
    
    ignore_token(TT::NL);
    
    if (TT::LBRACE != next_token()->token_type)
        ERROR("In handle_if(): expected '{' afer condition");
    
    std::vector<std::unique_ptr<AST_Node>> if_body;
    while (true) {
        auto s = handle_statement();
        if (!s) break;
        if_body.push_back(std::move(s));
    }

    if (TT::RBRACE != next_token()->token_type)
        ERROR("In handle_if(): expected '}' afer 'if's body");

    ignore_token(TT::NL);

    if (TT::ELSE != next_token()->token_type)
        ERROR("in handle_if(): expected 'else'");

    ignore_token(TT::NL);

    if (TT::LBRACE != next_token()->token_type)
        ERROR("in handle_if(): expected '{' afer else");

    std::vector<std::unique_ptr<AST_Node>> else_body;
    while (true) {
        auto s = handle_statement();
        if (!s) break;
        else_body.push_back(std::move(s));
    }

    if (TT::RBRACE != next_token()->token_type)
        ERROR("In handle_if(): expected '}' afer 'else's body");

    return std::make_unique<If_Else_AST>(
        std::move(cond), std::move(if_body), std::move(else_body)
    );
}

std::unique_ptr<Var_Declaration_AST> AST::handle_var_decl()
{
    std::string type0 = next_token()->value;

    ignore_token(TT::NL);

    if (TT::ID != next_token()->token_type)
        ERROR("In handle_var_decl: expected ID");
    
    std::string name0 = tok->value;

    if (TT::ASS != next_token()->token_type)
        ERROR("In handle_error(): expected =");
    
    ignore_token(TT::NL);

    if (std::unique_ptr<Expr_AST> expr {handle_expr()})
        return std::make_unique<Var_Declaration_AST>(type0, name0, std::move(expr));
    
    return nullptr;
}

std::unique_ptr<Return_AST> AST::handle_return()
{
    next_token(); //eat 'return'
    ignore_token(TT::NL);
    
    if (std::unique_ptr<Expr_AST> expr{handle_expr()})
        return std::make_unique<Return_AST>(std::move(expr));
    
    return nullptr;
}

std::unique_ptr<Expr_AST> AST::handle_expr(std::unique_ptr<Binary_Expr_AST>* prev_exp)
{
    if (TT::NL == next_token()->token_type) // signals the end of expr (for now)
        return nullptr;

    std::unique_ptr<Expr_AST> LHS;

    switch (tok->token_type) {
        case TT::NUM_LIT:
            LHS = std::make_unique<Number_Expr_AST>(std::stod(tok->value));
            break;
        case TT::ID:
            LHS = std::make_unique<Var_Expr_AST>(tok->value);
            break;
            // might also be a function call
        default:
            std::cout << "Token " << static_cast<char>(tok->token_type) << '\n';
            ERROR("Semantic Parser: in handle_expr()");
    }

    if (!is_math_op( toker.peek()->token_type ))
        return LHS;
    
    next_token();
    
    std::unique_ptr<Binary_Expr_AST> expr = std::make_unique<Binary_Expr_AST>();
    expr->op = static_cast<Math_Op>(static_cast<char>(tok->token_type));
    expr->LHS = std::move(LHS);

    if (!prev_exp || op_precedence[expr->op] >= op_precedence[prev_exp->get()->op]) {
        expr->RHS = handle_expr(&expr);
        if (expr->RHS==nullptr) ERROR("Semantic Parser: in handle_expr()");
        return std::move(expr);
    }

    // prev_expr exists and has higher operator precedence
    std::unique_ptr<Expr_AST> prev_LHS = std::move(prev_exp->get()->LHS);
    prev_exp->get()->LHS = std::make_unique<Binary_Expr_AST>(
        prev_exp->get()->op,
        std::move(prev_LHS),
        std::move(expr->LHS)
    );
    prev_exp->get()->op = expr->op;
    
    return handle_expr(prev_exp);
}

} // namespace Semantic_Parser