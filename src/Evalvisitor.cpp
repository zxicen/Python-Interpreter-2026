#include "Evalvisitor.h"
#include <any>
#include <stdexcept>

std::any EvalVisitor::visitFile_input(Python3Parser::File_inputContext *ctx) {
    for (auto *stmt : ctx->stmt()) {
        visit(stmt);
    }
    return {};
}

std::any EvalVisitor::visitStmt(Python3Parser::StmtContext *ctx) {
    if (ctx->simple_stmt()) {
        return visit(ctx->simple_stmt());
    }
    if (ctx->compound_stmt()) {
        return visit(ctx->compound_stmt());
    }
    return {};
}

std::any EvalVisitor::visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) {
    return visit(ctx->small_stmt());
}

std::any EvalVisitor::visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) {
    if (ctx->expr_stmt()) {
        return visit(ctx->expr_stmt());
    }
    if (ctx->flow_stmt()) {
        return visit(ctx->flow_stmt());
    }
    return {};
}

std::any EvalVisitor::visitAtom(Python3Parser::AtomContext *ctx) {
    if (ctx->NONE()) {
        return  Value{NoneValue{}};
    }
    if (ctx->TRUE()) {
        return Value{bool(true)};
    }
    if (ctx->FALSE()) {
        return Value{bool(false)};
    }
    if (ctx->NAME()) {
        return runtime_.get(ctx->NAME()->getText());
    }
    if (ctx->NUMBER()) {
        const std::string text = ctx->NUMBER()->getText();
        
        const auto isDigits = [](const std::string &s) {
            return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
        };
        if (isDigits(text)) {
            return Value{sjtu::int2048(text)};
        }
        
        const auto dot = text.find('.');
        if (dot != std::string::npos) {
            const std::string whole = text.substr(0, dot);
            const std::string fraction = text.substr(dot + 1);
            
            if (isDigits(whole) &&
                (fraction.empty() || isDigits(fraction)) &&
                !(whole.size() > 1 && whole[0] == '0')) {
                
                return Value{std::stod(text)};
            }
        }
        throw std::runtime_error("Unsupported numeric literal: " + text);
    }
    const auto stringTokens = ctx->STRING();
    if (!stringTokens.empty()) {
        if(stringTokens.size() > 1) {
            throw std::runtime_error("String concatenation is not supported");
        }
        
        const std::string text = stringTokens[0]->getText();
        
        const char quote = text.front();
        const bool tripleQuoted = 
            text.size() >= 6 &&
            text[1] == quote &&
            text[2] == quote;
        
        if((quote != '\'' && quote != '"') ||
           text.back() != quote ||
           tripleQuoted) {
            throw std::runtime_error("Unsupported string literal: " + text);
        }
        
        std::string result;
        
        for(size_t i = 1; i + 1 < text.size(); ++i) {
            if(text[i] != '\\') {
                result.push_back(text[i]);
                continue;
            }
            
            if(i + 2 >= text.size()) {
                throw std::runtime_error("Incomplete string escape");
            }
            
            const char next = text[++i];
            
            switch(next) {
                case 'n':
                    result.push_back('\n');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                case '\\':
                    result.push_back('\\');
                    break;
                case '\'':
                    result.push_back('\'');
                    break;
                case '"':
                    result.push_back('"');
                    break;
                default:
                    throw std::runtime_error("Unsupported string escape");
            }
        }
        return Value{result};
    }
    throw std::runtime_error("Unknown atom type");
}