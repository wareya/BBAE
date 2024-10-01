#ifndef BBEL_COMPILER
#define BBEL_COMPILER

#include <stdint.h>
#include <stddef.h>

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <optional>

#include "../src/bbae_builder.h"
#include "../src/buffers.h"

#include "grammar.hpp"

struct VarData
{
    Type type;
    std::shared_ptr<std::string> name;
    Value * var;
    bool is_arg = false;
    bool is_global = false;
    size_t scope_level = 0;
};

struct CompilerState
{
    Program * program = 0;
    Function * current_func = 0;
    Block * current_block = 0;
    byte_buffer * buffer = 0;
    
    std::vector<std::unordered_map<std::string, VarData>> vars = {{}};
    std::vector<Value *> stack;
    
    VarData add_var(std::shared_ptr<std::string> name, Type type, Value * backend_var)
    {
        assert(vars.size() > 0);
        auto var = VarData{type, name, backend_var, vars.size() == 2, vars.size() == 1, vars.size() - 1};
        vars.back().insert({*name, var});
        return var;
    }
    std::optional<VarData> get_var(std::shared_ptr<std::string> name)
    {
        if (vars.size() == 0)
            return {};
        
        for (ptrdiff_t i = vars.size() - 1; i >= 0; i--)
        {
            if (vars[i].count(*name) > 0)
                return {vars[i][*name]};
        }
        return {};
    }
};

Type parse_type(std::shared_ptr<ASTNode> ast)
{
    if (ast->children.size() == 1)
        return parse_type(ast->children[0]);
    assert(ast->children.size() == 0);
    if (*ast->text == "i64")
        return basic_type(TYPE_I64);
    else if (*ast->text == "f64")
        return basic_type(TYPE_F64);
    else
        assert(((void)"Invalid type!", 0));
}

bool std_starts_with(const std::string & a, const std::string & b)
{
    if (a.size() < b.size())
        return false;
    return strncmp(a.data(), b.data(), b.size()) == 0;
}

void compile(CompilerState & state, std::shared_ptr<ASTNode> ast)
{
    auto add_arg_prefix = [](std::shared_ptr<std::string> str) -> std::shared_ptr<std::string>
    {
        return std::make_shared<std::string>(std::string("_arg_") + *str);
    };
    
    if (*ast->text == "program")
    {
        for (auto & child : ast->children)
            compile(state, child);
    }
    else if (*ast->text == "funcdef")
    {
        assert(ast->children.size() == 4);
        Type return_type = parse_type(ast->children[0]);
        std::shared_ptr<std::string> name = ast->children[1]->children[0]->text;
        
        state.current_func = create_function(state.program, name->data(), return_type);
        state.current_block = function_get_entry_block(state.current_func);
        
        // args
        state.vars.push_back({});
        for (auto & a : ast->children[2]->children)
        {
            printf("%zu\n", a->children.size());
            Type arg_type = parse_type(a->children[0]);
            std::shared_ptr<std::string> raw_name = a->children[1]->children[0]->text;
            std::shared_ptr<std::string> name = add_arg_prefix(raw_name);
            
            auto arg = add_funcarg(state.current_func, name->data(), arg_type);
            auto var = add_stack_slot(state.current_func, raw_name->data(), type_size(arg_type));
            state.add_var(raw_name, arg_type, var);
            build_store(state.current_block, var, arg);
        }
        
        // statements
        state.vars.push_back({});
        for (auto & a : ast->children[3]->children)
            compile(state, a);
        
        // check for terminator
        if (!block_is_terminated(state.current_block))
        {
            if (types_same(return_type, basic_type(TYPE_I64)))
                build_return_1val(state.current_block, build_constant_i64(0));
            else if (types_same(return_type, basic_type(TYPE_IPTR)))
                build_return_1val(state.current_block, build_constant_iptr(0));
            else if (types_same(return_type, basic_type(TYPE_F64)))
                build_return_1val(state.current_block, build_constant_f64(0));
            else
                assert(0);
        }
        
        state.vars.pop_back();
        state.vars.pop_back();
    }
    else if (*ast->text == "statement")
    {
        for (auto & child : ast->children)
            compile(state, child);
    }
    else if (*ast->text == "expr" || *ast->text == "simple_expr")
    {
        assert(ast->children.size() == 1);
        compile(state, ast->children[0]);
    }
    else if (*ast->text == "int")
    {
        std::shared_ptr<std::string> text = ast->children[0]->text;
        
        uint64_t val;
        if ((*text)[0] == '-')
            val = std::stoll(*text);
        else
            val = std::stoull(*text);
        
        state.stack.push_back(build_constant_i64(val));
    }
    else if (*ast->text == "float")
    {
        std::shared_ptr<std::string> text = ast->children[0]->text;
        
        double val = std::stold(*text);
        
        state.stack.push_back(build_constant_f64(val));
    }
    else if (*ast->text == "vardec")
    {
        assert(ast->children.size() == 2 || ast->children.size() == 3);
        Type var_type = parse_type(ast->children[0]);
        std::shared_ptr<std::string> name = ast->children[1]->children[0]->text;
        
        auto var = add_stack_slot(state.current_func, name->data(), type_size(var_type));
        state.add_var(name, var_type, var);
        
        if (ast->children.size() == 3)
        {
            compile(state, ast->children[2]);
            auto val = state.stack.back();
            state.stack.pop_back();
            build_store(state.current_block, var, val);
        }
    }
    else if (*ast->text == "assign")
    {
        assert(ast->children.size() == 2);
        std::shared_ptr<std::string> name = ast->children[0]->children[0]->text;
        
        auto var_data = state.get_var(name);
        if (!var_data)
        {
            printf("Error: no such variable %s\n", name->data());
            assert(0);
        }
        
        compile(state, ast->children[1]);
        auto val = state.stack.back();
        state.stack.pop_back();
        
        build_store(state.current_block, var_data->var, val);
    }
    else if (*ast->text == "name")
    {
        assert(ast->children.size() == 1);
        std::shared_ptr<std::string> name = ast->children[0]->text;
        
        auto var_data = state.get_var(name);
        if (!var_data)
        {
            printf("Error: no such variable %s\n", name->data());
            assert(0);
        }
        
        auto statement = build_load(state.current_block, var_data->type, var_data->var);
        auto val = statement_get_output(statement);
        assert(val);
        state.stack.push_back(val);
    }
    else if (*ast->text == "return")
    {
        if (ast->children.size() == 0)
        {
            build_return_void(state.current_block);
        }
        else if (ast->children.size() == 1)
        {
            compile(state, ast->children[0]);
            auto a = state.stack.back();
            state.stack.pop_back();
            
            assert(a);
            
            build_return_1val(state.current_block, a);
        }
        else
            assert(0);
        
        state.current_block = create_block(state.program, 0);
    }
    else if (*ast->text == "base_binexp")
    {
        if (ast->children.size() == 1)
            compile(state, ast->children[0]);
        else if (ast->children.size() == 2)
        {
            compile(state, ast->children[1]);
            assert(((void)"TODO", 0));
        }
        else
            assert(0);
    }
    else if (*ast->text == "base_unexp")
    {
        if (ast->children.size() == 1)
            compile(state, ast->children[0]);
        else
            assert(((void)"TODO", 0));
    }
    else if (std_starts_with(*ast->text, std::string("binexp_")))
    {
        assert(ast->children.size() == 1 || ast->children.size() == 3);
        if (ast->children.size() == 1)
            compile(state, ast->children[0]);
        else
        {
            compile(state, ast->children[0]);
            auto a = state.stack.back();
            state.stack.pop_back();
            
            compile(state, ast->children[2]);
            auto b = state.stack.back();
            state.stack.pop_back();
            
            assert(a && b);
            
            auto op_text = ast->children[1]->children[0]->text;
            printf("op: %s\n", op_text->data());
            
            Statement * operation = 0;
            if (types_same(a->type, basic_type(TYPE_F64)))
            {
                if (!types_same(b->type, basic_type(TYPE_F64)))
                {
                    assert(((void)"(unmatched types. TODO add real error)", 0));
                }
                if (*op_text == "+")
                    operation = build_fadd(state.current_block, a, b);
                else if (*op_text == "-")
                    operation = build_fsub(state.current_block, a, b);
                else if (*op_text == "*")
                    operation = build_fmul(state.current_block, a, b);
                else if (*op_text == "/")
                    operation = build_fdiv(state.current_block, a, b);
                else if (*op_text == "==")
                    operation = build_fcmp(state.current_block, a, b, CMP_EQ);
                else if (*op_text == "!=")
                    operation = build_fcmp(state.current_block, a, b, CMP_NE);
                else if (*op_text == ">=")
                    operation = build_fcmp(state.current_block, a, b, CMP_GE);
                else if (*op_text == ">")
                    operation = build_fcmp(state.current_block, a, b, CMP_G);
                else if (*op_text == "<=")
                    operation = build_fcmp(state.current_block, a, b, CMP_LE);
                else if (*op_text == "<")
                    operation = build_fcmp(state.current_block, a, b, CMP_L);
                else
                {
                    printf("culprit: %s\n", op_text->data());
                    assert(((void)"unsupported floating-point infix operator", 0));
                }
            }
            else
            {
                if (types_same(b->type, basic_type(TYPE_F64)))
                {
                    assert(((void)"(unmatched types. TODO add real error)", 0));
                }
                // TODO: error if i64 <op> iptr
                // TODO: convert arg2 to iptr if iptr <op> i64
                if (*op_text == "+")
                    operation = build_add(state.current_block, a, b);
                else if (*op_text == "+")
                    operation = build_sub(state.current_block, a, b);
                else if (*op_text == "*")
                    operation = build_mul(state.current_block, a, b);
                else if (*op_text == "/")
                    operation = build_div(state.current_block, a, b, 1);
                else if (*op_text == "%")
                    operation = build_rem(state.current_block, a, b, 1);
                else if (*op_text == "+*")
                    operation = build_mul(state.current_block, a, b);
                else if (*op_text == "+/")
                    operation = build_div(state.current_block, a, b, 1);
                else if (*op_text == "+%")
                    operation = build_irem(state.current_block, a, b, 1);
                else if (*op_text == "<<")
                    operation = build_shl(state.current_block, a, b, 1);
                else if (*op_text == ">>")
                    operation = build_shr(state.current_block, a, b, 1);
                else if (*op_text == "+>>")
                    operation = build_sar(state.current_block, a, b, 1);
                else
                {
                    printf("culprit: %s\n", op_text->data());
                    assert(((void)"TODO", 0));
                }
            }
            
            auto val = statement_get_output(operation);
            state.stack.push_back(val);
        }
    }
    else
    {
        printf("%s\n", ast->text->data());
        assert(((void)"TODO", 0));
    }
}

Program * compile_root(std::shared_ptr<ASTNode> ast)
{
    Program * program = create_empty_program();
    assert(*ast->text == "program");
    if (ast->children.size() == 0)
        return program;
    
    CompilerState state;
    state.program = program;
    
    compile(state, ast);
    
    return program;
}

#endif // BBEL_COMPILER
