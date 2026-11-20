#pragma once
#include <any>
#ifndef PYTHON_INTERPRETER_EVALVISITOR_H
#define PYTHON_INTERPRETER_EVALVISITOR_H


#include "Python3ParserBaseVisitor.h"
#include "runtime.h"

class EvalVisitor : public Python3ParserBaseVisitor {
public:
	std::any visitFile_input(Python3Parser::File_inputContext *ctx) override;
	std::any visitStmt(Python3Parser::StmtContext *ctx) override;
	std::any visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) override;
	std::any visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) override;
	std::any visitAtom(Python3Parser::AtomContext *ctx) override;

private:
	Runtime runtime_;
};


#endif//PYTHON_INTERPRETER_EVALVISITOR_H
