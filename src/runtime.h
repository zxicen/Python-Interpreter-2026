#pragma once

#include "Python3Parser.h"
#include "int2048.h"
#include <optional>
#include <unordered_map>
#include <string>
#include <variant>
#include <vector>
#include <memory>

struct NoneValue {};

struct TupleValue;

using Value = std::variant<
    NoneValue,
    bool,
    sjtu::int2048,
    double,
    std::string,
    std::shared_ptr<const TupleValue>
>;

struct TupleValue {
    std::vector<Value> elements;
};

class Runtime {
    using Scope = std::unordered_map<std::string, Value>;

public:
    Value get(const std::string &name) const;
    void assign(const std::string &name, const Value &value);
    void defineFunction(const std::string &name, Python3Parser::FuncdefContext *ctx,
                        std::vector<std::optional<Value>> defaults);
    Python3Parser::FuncdefContext *getFunction(const std::string &name) const;
    const std::vector<std::optional<Value>> &getDefaults(const std::string &name) const;
    void bindParameter(const std::string &name, const Value &value);
    void enterFunction();
    void leaveFunction();
    Value makeTuple(const std::vector<Value> &elements);
    std::string toString(const Value &value);
    Value toInt(const Value &value);
    Value toFloat(const Value &value);
    Value toStr(const Value &value);
    Value toBool(const Value &value);
    
private:
    Scope globals_;
    std::unordered_map<std::string, Python3Parser::FuncdefContext *> functions_;
    std::unordered_map<std::string, std::vector<std::optional<Value>>> defaults_;
    std::vector<Scope> locals_;
};