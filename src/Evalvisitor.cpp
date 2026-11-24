#include "Evalvisitor.h"
#include "int2048.h"
#include <any>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <variant>
#include <optional>
#include <iostream>

bool EvalVisitor::isTruthy(const Value &value) {
    if (std::holds_alternative<NoneValue>(value)) {
        return false;
    }
    if (const auto *p = std::get_if<bool>(&value)) {
        return *p;
    }
    if (const auto *p = std::get_if<sjtu::int2048>(&value)) {
        return *p != 0;
    }
    if (const auto *p = std::get_if<double>(&value)) {
        return *p != 0.0;
    }
    return !std::get<std::string>(value).empty();
}

Value EvalVisitor::arithmetic(Value left, Value right, const std::string &op) {
    const sjtu::int2048 zero(0LL);
    const sjtu::int2048 one(1LL);

    if (const auto *p = std::get_if<bool>(&left)) {
        left = Value{*p ? one : zero};
    }
    if (const auto *p = std::get_if<bool>(&right)) {
        right = Value{*p ? one : zero};
    }

    const bool leftIsString = std::holds_alternative<std::string>(left);
    if (op == "+" && leftIsString) {
        return Value{std::get<std::string>(left) + std::get<std::string>(right)};
    }
    if (op == "*" && (leftIsString || std::holds_alternative<std::string>(right))) {
        const std::string text = std::get<std::string>(leftIsString ? left : right);
        sjtu::int2048 count = std::get<sjtu::int2048>(leftIsString ? right : left);
        std::string repeated;
        while (count > zero) {
            repeated += text;
            count -= one;
        }
        return Value{repeated};
    }

    if (std::holds_alternative<sjtu::int2048>(left) &&
        std::holds_alternative<sjtu::int2048>(right) && op != "/"
    ) {
        const sjtu::int2048 &a = std::get<sjtu::int2048>(left);
        const sjtu::int2048 &b = std::get<sjtu::int2048>(right);
        if (op == "+") {
            return Value{a + b};
        } else if (op == "-") {
            return Value{a - b};
        } else if (op == "*") {
            return Value{a * b};
        } else if (op == "//") {
            return Value{a / b};
        } else if (op == "%") {
            return Value{a % b};
        }
    }

    if (const auto *p = std::get_if<sjtu::int2048>(&left)) {
        left = Value{std::stod(p->to_string())};
    }
    if (const auto *p = std::get_if<sjtu::int2048>(&right)) {
        right = Value{std::stod(p->to_string())};
    }
    const double leftNumber = std::get<double>(left);
    const double rightNumber = std::get<double>(right);

    if (op == "+") {
        return Value{leftNumber + rightNumber};
    } else if (op == "-") {
        return Value{leftNumber - rightNumber};
    } else if (op == "*") {
        return Value{leftNumber * rightNumber};
    } else if (op == "/") {
        return Value{leftNumber / rightNumber};
    } else if (op == "//" || op == "%") {
        double mod = std::fmod(leftNumber, rightNumber);
        double div = (leftNumber - mod) / rightNumber;
        if (mod != 0.0) {
            if ((mod < 0.0) != (rightNumber < 0.0)) {
                mod += rightNumber;
                div -= 1.0;
            }
        } else {
            mod = std::copysign(0.0, rightNumber);
        }

        double floorDiv;
        if (div != 0.0) {
            floorDiv = std::floor(div);
            if (div - floorDiv > std::nextafter(0.5, 1.0)) {
                floorDiv += 1.0;
            }
        } else {
            floorDiv = std::copysign(0.0, leftNumber / rightNumber);
        }
        return Value{op == "//" ? floorDiv : mod};
    }
    throw std::invalid_argument("Unsupported arithmetic operator: " + op);
}

std::any EvalVisitor::visitFile_input(Python3Parser::File_inputContext *ctx) {
    for (auto *stmt : ctx->stmt()) {
        visit(stmt);
    }
    return {};
}

std::any EvalVisitor::visitFuncdef(Python3Parser::FuncdefContext *ctx) {
    std::vector<std::optional<Value>> defaults;
    if (ctx->parameters()->typedargslist()) {
        for (auto *child : ctx->parameters()->typedargslist()->children) {
            if (dynamic_cast<Python3Parser::TfpdefContext *>(child)) {
                defaults.emplace_back(std::nullopt);
            } else if (auto *test = dynamic_cast<Python3Parser::TestContext *>(child)) {
                defaults.back() = std::any_cast<Value>(visit(test));
            }
        }
    }
    runtime_.defineFunction(ctx->NAME()->getText(), ctx, std::move(defaults));
    return Value{NoneValue{}};
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

std::any EvalVisitor::visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) {
    const auto lists = ctx->testlist();
    
    if (lists.size() == 2) {
        if (ctx->augassign()) {
            std::string op = ctx->augassign()->getText();
            std::vector<Value> temporaryLeft, temporaryRight;
            temporaryLeft.reserve(lists[0]->test().size());
            temporaryRight.reserve(lists[1]->test().size());
            for (size_t i = 0; i < lists[0]->test().size(); ++i) {
                temporaryLeft.emplace_back(runtime_.get(lists[0]->test(i)->getText()));
                temporaryRight.emplace_back(std::any_cast<Value>(visit(lists[1]->test(i))));
            }
            op.pop_back();
            for (size_t i = 0; i < lists[0]->test().size(); ++i) {
                runtime_.assign(lists[0]->test(i)->getText(), arithmetic(temporaryLeft[i], temporaryRight[i], op));
            }
        } else {
            const Value temporaryRight = std::any_cast<Value>(visit(lists[1]));
            const auto targets = lists[0]->test();
            if (targets.size() == 1) {
                runtime_.assign(targets[0]->getText(), temporaryRight);
            } else {
                const auto &tuple = *std::get_if<std::shared_ptr<const TupleValue>>(&temporaryRight);
                for (size_t i = 0; i < targets.size(); ++i) {
                    runtime_.assign(targets[i]->getText(), tuple->elements[i]);
                }
            }
        }
        return Value{NoneValue{}};
    }
    
    Value value = std::any_cast<Value>(visit(lists.back()->test(0)));
    for (size_t i = lists.size() - 1; i-- > 0;) {
        runtime_.assign(lists[i]->test(0)->getText(), value);
    }
    return value;
}

std::any EvalVisitor::visitBreak_stmt(Python3Parser::Break_stmtContext *ctx) {
    throw BreakValue{};
}

std::any EvalVisitor::visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx) {
    throw ContinueValue{};
}

std::any EvalVisitor::visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) {
    if (ctx->testlist()) {
        throw std::any_cast<Value>(visit(ctx->testlist()));
    } else {
        throw Value{NoneValue{}};
    }
}

std::any EvalVisitor::visitIf_stmt(Python3Parser::If_stmtContext *ctx) {
    const auto test = ctx->test();
    const auto suite = ctx->suite();
    const auto elif = ctx->ELIF();
    
    if (isTruthy(std::any_cast<Value>(visit(test[0])))) {
        return visit(suite[0]);
    }
    for (size_t i = 0; i < elif.size(); ++i) {
        if (isTruthy(std::any_cast<Value>(visit(test[i + 1])))) {
            return visit(suite[i + 1]);
        }
    }
    if (ctx->ELSE()) {
        return visit(suite.back());
    }
    return Value{NoneValue{}};
}

std::any EvalVisitor::visitWhile_stmt(Python3Parser::While_stmtContext *ctx) {
    while (isTruthy(std::any_cast<Value>(visit(ctx->test())))) {
        try {
            visit(ctx->suite());
        } catch (const BreakValue &) {
            break;
        } catch (const ContinueValue &) {
            continue;
        }
    }
    return Value{NoneValue{}};
}

std::any EvalVisitor::visitSuite(Python3Parser::SuiteContext *ctx) {
    if (ctx->simple_stmt()) {
        return visit(ctx->simple_stmt());
    } else {
        const auto stmts = ctx->stmt();
        for (const auto stmt : stmts) {
            visit(stmt);
        }
        return Value{NoneValue{}};
    }
}

std::any EvalVisitor::visitOr_test(Python3Parser::Or_testContext *ctx) {
    const auto and_test = ctx->and_test();
    
    if (ctx->OR().empty()) {
        return visit(and_test[0]);
    }
    
    for (size_t i = 0; i < and_test.size(); ++i) {
        const auto value = std::any_cast<Value>(visit(and_test[i]));
        if (isTruthy(value)) {
            return Value{true};
        }
    }
    return Value{false};
}

std::any EvalVisitor::visitAnd_test(Python3Parser::And_testContext *ctx) {
    const auto not_test = ctx->not_test();
    
    if (ctx->AND().empty()) {
        return visit(not_test[0]);
    }
    
    for (size_t i = 0; i < not_test.size(); ++i) {
        const auto value = std::any_cast<Value>(visit(not_test[i]));
        if (!isTruthy(value)) {
            return Value{false};
        }
    }
    return Value{true};
}

std::any EvalVisitor::visitNot_test(Python3Parser::Not_testContext *ctx) {
    if (ctx->NOT()) {
        const auto value = std::any_cast<Value>(visit(ctx->not_test()));
        return Value{!isTruthy(value)};
        
    } else {
        return visit(ctx->comparison());
    }
}

std::any EvalVisitor::visitComparison(Python3Parser::ComparisonContext *ctx) {
    const auto expressions = ctx->arith_expr();
    const auto comparators = ctx->comp_op();
    const sjtu::int2048 zero(0LL);
    const sjtu::int2048 one(1LL);
    
    if (comparators.empty()) {
        return visit(expressions[0]);
    }
    
    Value left = std::any_cast<Value>(visit(expressions[0]));
    if (const auto *p = std::get_if<bool>(&left)) {
        left = Value{(*p) ? one : zero};
    }
    for (size_t i = 0; i < comparators.size(); ++i) {
        Value right = std::any_cast<Value>(visit(expressions[i + 1]));
        if (const auto *p = std::get_if<bool>(&right)) {
            right = Value{(*p) ? one : zero};
        }
        
        bool comparisonResult = false;
        const bool leftIsString = std::holds_alternative<std::string>(left);
        const bool rightIsString = std::holds_alternative<std::string>(right);
        if (leftIsString || rightIsString) {
            if (!leftIsString || !rightIsString) {
                comparisonResult = false;
            } else {
                const auto &leftString = std::get<std::string>(left);
                const auto &rightString = std::get<std::string>(right);
                if (comparators[i]->LESS_THAN()) comparisonResult = leftString < rightString;
                else if (comparators[i]->GREATER_THAN()) comparisonResult = leftString > rightString;
                else if (comparators[i]->LT_EQ()) comparisonResult = leftString <= rightString;
                else if (comparators[i]->GT_EQ()) comparisonResult = leftString >= rightString;
                else if (comparators[i]->EQUALS()) comparisonResult = leftString == rightString;
                else if (comparators[i]->NOT_EQ_2()) comparisonResult = leftString != rightString;
            }
        } else {
            const bool leftIsDouble = std::holds_alternative<double>(left);
            const bool rightIsDouble = std::holds_alternative<double>(right);
            if (leftIsDouble || rightIsDouble) {
                const double leftNumber = leftIsDouble ? std::get<double>(left) : std::stod(std::get<sjtu::int2048>(left).to_string());
                const double rightNumber = rightIsDouble ? std::get<double>(right) : std::stod(std::get<sjtu::int2048>(right).to_string());
                
                if (comparators[i]->LESS_THAN()) comparisonResult = leftNumber < rightNumber;
                else if (comparators[i]->GREATER_THAN()) comparisonResult = leftNumber > rightNumber;
                else if (comparators[i]->LT_EQ()) comparisonResult = leftNumber <= rightNumber;
                else if (comparators[i]->GT_EQ()) comparisonResult = leftNumber >= rightNumber;
                else if (comparators[i]->EQUALS()) comparisonResult = leftNumber == rightNumber;
                else if (comparators[i]->NOT_EQ_2()) comparisonResult = leftNumber != rightNumber;
            } else {
                const auto &leftNumber = std::get<sjtu::int2048>(left);
                const auto &rightNumber = std::get<sjtu::int2048>(right);
                
                if (comparators[i]->LESS_THAN()) comparisonResult = leftNumber < rightNumber;
                else if (comparators[i]->GREATER_THAN()) comparisonResult = leftNumber > rightNumber;
                else if (comparators[i]->LT_EQ()) comparisonResult = leftNumber <= rightNumber;
                else if (comparators[i]->GT_EQ()) comparisonResult = leftNumber >= rightNumber;
                else if (comparators[i]->EQUALS()) comparisonResult = leftNumber == rightNumber;
                else if (comparators[i]->NOT_EQ_2()) comparisonResult = leftNumber != rightNumber;
            }
        }
        if (!comparisonResult) {
            return Value{false};
        }
        left = right;
    }
    return Value{true};
}

std::any EvalVisitor::visitComp_op(Python3Parser::Comp_opContext *ctx) {
    return ctx->getText();
}

std::any EvalVisitor::visitArith_expr(Python3Parser::Arith_exprContext *ctx) {
    const auto terms = ctx->term();
    const auto operators = ctx->addorsub_op();
    Value result = std::any_cast<Value>(visit(terms[0]));

    for (size_t i = 0; i < operators.size(); ++i) {
        Value right = std::any_cast<Value>(visit(terms[i + 1]));
        result = arithmetic(result, right, operators[i]->getText());
    }
    return result;
}

std::any EvalVisitor::visitTerm(Python3Parser::TermContext *ctx) {
    const auto factors = ctx->factor();
    const auto operators = ctx->muldivmod_op();
    Value result = std::any_cast<Value>(visit(factors[0]));

    for (size_t i = 0; i < operators.size(); ++i) {
        Value right = std::any_cast<Value>(visit(factors[i + 1]));
        result = arithmetic(result, right, operators[i]->getText());
    }
    return result;
}

std::any EvalVisitor::visitFactor(Python3Parser::FactorContext *ctx) {
    if (ctx->atom_expr()) {
        return visit(ctx->atom_expr());
    }
    
    const Value value = std::any_cast<Value>(visit(ctx->factor()));
    const bool isMinus = ctx->MINUS() != nullptr;
    
    if (const auto *p = std::get_if<sjtu::int2048>(&value)) {
        return Value{isMinus ? -(*p) : *p};
    }
    if (const auto *p = std::get_if<double>(&value)) {
        return Value{isMinus ? -(*p) : *p};
    }
    if (const auto *p = std::get_if<bool>(&value)) {
        return Value{(*p) ? (isMinus ? -1 : 1) : 0};
    }
}

std::any EvalVisitor::visitAtom_expr(Python3Parser::Atom_exprContext *ctx) {
    if (ctx->trailer()) {
        const std::string funcName = ctx->atom()->getText();
        if (funcName == "print") {
            if (ctx->trailer()->arglist()) {
                const auto arguments = ctx->trailer()->arglist()->argument();
                for (size_t i = 0; i < arguments.size(); ++i) {
                    if (i > 0) {
                        std::cout << ' ';
                    }
                    std::cout << runtime_.toString(std::any_cast<Value>(visit(arguments[i]->test(0))));
                }
            }
            std::cout << '\n';
            return Value{NoneValue{}};
        }
        if (funcName == "int") {
            return runtime_.toInt(std::any_cast<Value>(visit(ctx->trailer()->arglist()->argument(0)->test(0))));
        }
        if (funcName == "float") {
            return runtime_.toFloat(std::any_cast<Value>(visit(ctx->trailer()->arglist()->argument(0)->test(0))));
        }
        if (funcName == "str") {
            return runtime_.toStr(std::any_cast<Value>(visit(ctx->trailer()->arglist()->argument(0)->test(0))));
        }
        if (funcName == "bool") {
            return runtime_.toBool(std::any_cast<Value>(visit(ctx->trailer()->arglist()->argument(0)->test(0))));
        }
        
        const auto ctxFunc = runtime_.getFunction(funcName);
        std::vector<Python3Parser::TfpdefContext *> parameters;
        if (ctxFunc->parameters()->typedargslist()) {
            parameters = ctxFunc->parameters()->typedargslist()->tfpdef();
        }
        std::vector<std::optional<Value>> boundValues(parameters.size());
        
        if (ctx->trailer()->arglist()) {
            const auto arguments = ctx->trailer()->arglist()->argument();
            for (size_t i = 0; i < arguments.size(); ++i) {
                auto *argument = arguments[i];
                if (argument->ASSIGN()) {
                    const std::string name = argument->test(0)->getText();
                    size_t parameterIndex = 0;
                    while (parameterIndex < parameters.size() &&
                           parameters[parameterIndex]->NAME()->getText() != name) {
                        ++parameterIndex;
                    }
                    boundValues[parameterIndex] = std::any_cast<Value>(visit(argument->test(1)));
                }
            }
            for (size_t i = 0, j = 0; i < arguments.size(); ++i) {
                auto *argument = arguments[i];
                if (!argument->ASSIGN()) {
                    while (j < boundValues.size() && boundValues[j].has_value()) {
                        ++j;
                    }
                    boundValues[j] = std::any_cast<Value>(visit(argument->test(0)));
                    ++j;
                }
            }
        }

        const auto &defaults = runtime_.getDefaults(ctx->atom()->getText());
        for (size_t i = 0; i < boundValues.size(); ++i) {
            if (!boundValues[i].has_value()) {
                boundValues[i] = defaults[i];
            }
        }
        
        try {
            runtime_.enterFunction();
            for (size_t i = 0; i < parameters.size(); ++i) {
                if (boundValues[i].has_value()) {
                    runtime_.bindParameter(parameters[i]->NAME()->getText(), *boundValues[i]);
                }
            }
            visit(ctxFunc->suite());
            runtime_.leaveFunction();
            return Value{NoneValue{}};
        } catch (const Value &returnValue) {
            runtime_.leaveFunction();
            return returnValue;
        }
    }
    if (ctx->atom()) {
        return visit(ctx->atom());
    }
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
        
        if (text.find('.') != std::string::npos) {
            return Value{std::stod(text)};
        }
    }
    const auto stringTokens = ctx->STRING();
    if (!stringTokens.empty()) {
        std::string result;
        for (size_t i = 0; i < stringTokens.size(); ++i) {
            const std::string text = stringTokens[i]->getText();
            
            for (size_t j = 1; j + 1 < text.size(); ++j) {
                if (text[j] != '\\') {
                    result.push_back(text[j]);
                    continue;
                }
                
                const char next = text[++j];
                
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
                }
            }
        }
        return Value{result};
    }
    if (ctx->test()) {
        return visit(ctx->test());
    }
}

std::any EvalVisitor::visitTestlist(Python3Parser::TestlistContext *ctx) {
    if (ctx->COMMA().empty()) {
        return visit(ctx->test(0));
    }
    
    std::vector<Value> vec(ctx->test().size());
    for (size_t i = 0; i < vec.size(); ++i) {
        vec[i] = std::any_cast<Value>(visit(ctx->test(i)));
    }
    return runtime_.makeTuple(vec);
}