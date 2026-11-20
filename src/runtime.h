#pragma once

#include "int2048.h"
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
    void bindParameter(const std::string &name, const Value &value);
    
    void enterFunction();
    void leaveFunction();
    
    Value makeTuple(const std::vector<Value> &elements);
    
private:
    Scope globals_;
    std::vector<Scope> locals_;
};