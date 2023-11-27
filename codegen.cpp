#include "parser.hpp"

namespace Semantic_Parser
{

llvm::Value* Binary_Expr_AST::codegen()
{
    llvm::Value *L = LHS->codegen();
    llvm::Value *R = RHS->codegen();
    if (!L || !R) return nullptr;

    switch (op) {
        case Math_Op::PLUS:
            return Builder->CreateFAdd(L, R, "addtmp_name");
        case Math_Op::MINUS:
            return Builder->CreateFSub(L, R, "subtmp_name");
        case Math_Op::MULT:
            return Builder->CreateFMul(L, R, "multmp_name");
        case Math_Op::DIV:
            return Builder->CreateFDiv(L, R, "divtmp_name");
        default:
            ERROR("BinaryExprAST codegen(): invalid operator.");
    }
}

llvm::Value* Number_Expr_AST::codegen()
{
    return llvm::ConstantFP::get(*TheContext, llvm::APFloat(val));
}

llvm::Value* Var_Expr_AST::codegen()
{
    llvm::AllocaInst *A = NamedValues[name];
    if (!A)
        ERROR("VarExprAST codegen(): variable not defined earlier");
    return Builder->CreateLoad(A->getAllocatedType(), A, name.c_str());
}

llvm::Value* Var_Declaration_AST::codegen()
{
    //!
    //TODO check if variable was already defined
    
    llvm::Value *v_expr = expr->codegen();
    if (!v_expr)
        ERROR("In VarDeclaration_AST::codegen(): invalid expression.");

    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
    llvm::AllocaInst *alloca_space = Var_Declaration_AST::create_alloca_in_entryblock(TheFunction, var_name);
    Builder->CreateStore(v_expr, alloca_space);
    NamedValues[var_name] = alloca_space;
    
    return v_expr;
}

llvm::Value* Var_Assignment_AST::codegen()
{
    llvm::AllocaInst *aloc = NamedValues[id];
    if (!aloc) ERROR(std::string{"In VarAssignment_AST::codegen(): var name " + id + " not recognized"}.c_str());
    llvm::Value *val {std::move(expr->codegen())};
    if (!val) ERROR("In VarAssignment_AST::codegen(): invalid expression");
    Builder->CreateStore(val, aloc);
    return val;
}

//TODO for now it converts value to int32
llvm::Value* Return_AST::codegen()
{
    //llvm::Value *as_int = Builder->CreateFPToSI(std::move(expr->codegen()), llvm::Type::getInt32Ty(*TheContext), "float_to_i32");
    //return Builder->CreateRet(as_int);
    ret_val.yes = true;
    ret_val.data_type = Lexer::Token_type::FLOAT; //TODO assume float for now
    return ret_val.val = expr->codegen();
    
}

llvm::Value* If_Else_AST::codegen()
{
    //* Step 1: condition
    llvm::Value *v_cond = cond->codegen();
    if (!v_cond)
        ERROR("In IfElse_AST::codegen(): condition is NULL");
    // convert condition's value from float to bool
    v_cond = Builder->CreateFCmpONE(v_cond, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "ifcond");

    llvm::Function *current_function = Builder->GetInsertBlock()->getParent();
    
    llvm::BasicBlock *if_bb = llvm::BasicBlock::Create(*TheContext, "if_block", current_function);
    llvm::BasicBlock *else_bb = llvm::BasicBlock::Create(*TheContext, "else_body");
    llvm::BasicBlock *merge_bb = llvm::BasicBlock::Create(*TheContext, "after_ifelse");
    Builder->CreateCondBr(v_cond, if_bb, else_bb);

    //* Step 2: if

    Builder->SetInsertPoint(if_bb);
    llvm::Value *if_ret_val {nullptr};
    for (const auto& stm : if_body) {
        stm->codegen();
        if (ret_val.yes) {
            if_ret_val = ret_val.val;
            ret_val.yes = false;
            //TODO temporary: convert to int
            llvm::Value *as_int = Builder->CreateFPToSI(if_ret_val, llvm::Type::getInt32Ty(*TheContext), "float_to_i32");
            Builder->CreateRet(as_int);
        }
    }

    // if no 'return' was seen, branch to merge_bb
    if (!if_ret_val) {
        Builder->CreateBr(merge_bb);
        if_bb = Builder->GetInsertBlock();
    }

    //* Step 3: else

    current_function->insert(current_function->end(), else_bb);
    Builder->SetInsertPoint(else_bb);
    llvm::Value *else_ret_val {nullptr};
    for (const auto& stm : else_body) {
        stm->codegen();
        if (ret_val.yes) {
            else_ret_val = ret_val.val;
            ret_val.yes = false;
            //TODO temporary: convert to int
            llvm::Value *as_int = Builder->CreateFPToSI(else_ret_val, llvm::Type::getInt32Ty(*TheContext), "float_to_i32");
            Builder->CreateRet(as_int);
        }
    }

    // if no 'return' was seen, branch to merge_bb
    if (!else_ret_val) {
        Builder->CreateBr(merge_bb);
        else_bb = Builder->GetInsertBlock();
    }

    current_function->insert(current_function->end(), merge_bb);
    Builder->SetInsertPoint(merge_bb);
    
    //* Step 4

    if (if_ret_val && else_ret_val) {
        // put some instruction after so merge_bb is ntot empty and crashes
        llvm::Value *as_int = Builder->CreateFPToSI(else_ret_val, llvm::Type::getInt32Ty(*TheContext), "float_to_i32");
        Builder->CreateRet(as_int);
    }

    /*
    if (if_ret_val && else_ret_val) {
        // using PHI node
        llvm::PHINode *PN = Builder->CreatePHI(llvm::Type::getDoubleTy(*TheContext), 2, "iftmp");
        PN->addIncoming(if_ret_val, if_bb);
        PN->addIncoming(else_ret_val, else_bb);
        /return PN;
    }
    */

    return merge_bb; // return something...
}

llvm::Value* Function_AST::codegen() 
{
    llvm::FunctionType *funcType = llvm::FunctionType::get(llvm::Type::getInt32Ty(*TheContext), false); //TODO assume int32: 
    llvm::Function *func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, name, TheModule.get());
    llvm::BasicBlock *entryBlock = llvm::BasicBlock::Create(*TheContext, "entry", func);
    Builder->SetInsertPoint(entryBlock);
    
    for (auto& s : body)
        s->codegen();

    if (ret_val.yes) {
        ret_val.yes = false;
        llvm::Value *v_int = Builder->CreateFPToSI(ret_val.val, llvm::Type::getInt32Ty(*TheContext), "float_to_i32");
        Builder->CreateRet(v_int);
    }
    
    llvm::verifyFunction(*func);
    return func;
}

llvm::Value* Stream_AST::codegen()
{    
    llvm::FunctionType *stream_func_type = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*TheContext),
        llvm::PointerType::get(llvm::Type::getInt8Ty(*TheContext), 0),
        true
    );
    
    llvm::Function *stream_func = llvm::Function::Create(
        stream_func_type,
        llvm::Function::ExternalLinkage,
        internal_func_name,
        TheModule.get()
    );

    llvm::AllocaInst *aloc = NamedValues[id];
    llvm::LoadInst *var = nullptr;
    if (!aloc)
        ERROR(std::string{"In Stream_AST::codegen(): var name " + id + " not recognized"}.c_str());
    if (internal_func_name == "printf")
        var = Builder->CreateLoad(aloc->getAllocatedType(), aloc, id.c_str());

    std::vector<llvm::Value*> stream_func_args;
    if (internal_func_name=="scanf") {
        llvm::Constant *formatStr = Builder->CreateGlobalStringPtr("%lf");
        stream_func_args.push_back(formatStr);
        stream_func_args.push_back(aloc);
    } else {
        llvm::Constant *formatStr = Builder->CreateGlobalStringPtr("%lf\n");
        stream_func_args.push_back(formatStr);
        stream_func_args.push_back(var);
    }
    
    Builder->CreateCall(stream_func, stream_func_args, internal_func_name);

    return stream_func;
}


}
