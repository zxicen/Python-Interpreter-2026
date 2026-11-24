#include "runtime.h"
#include <stdexcept>

Value Runtime::get(const std::string &name) const {
    if (!locals_.empty()) {
        const auto &local = locals_.back();
        auto it = local.find(name);
        if (it != local.end()) {
            return it->second;
        }
    }
    auto it = globals_.find(name);
    if (it != globals_.end()) {
        return it->second;
    }
    throw std::runtime_error("Variable not found: " + name);
}

void Runtime::assign(const std::string &name, const Value &value) {
    if (!locals_.empty() && locals_.back().find(name) != locals_.back().end()) {
        locals_.back()[name] = value;
    } else if (globals_.find(name) != globals_.end()) {
        globals_[name] = value;
    } else if (!locals_.empty()) {
        locals_.back()[name] = value;
    } else {
        globals_[name] = value;
    }
}

void Runtime::defineFunction(const std::string &name, Python3Parser::FuncdefContext *ctx,
                             std::vector<std::optional<Value>> defaults) {
    functions_[name] = ctx;
    defaults_[name] = std::move(defaults);
}

const std::vector<std::optional<Value>> &Runtime::getDefaults(const std::string &name) const {
    return defaults_.at(name);
}

Python3Parser::FuncdefContext *Runtime::getFunction(const std::string &name) const {
    const auto it = functions_.find(name);
    if (it == functions_.end()) {
        throw std::runtime_error("Function not found: " + name);
    }
    return it->second;
}

void Runtime::bindParameter(const std::string &name, const Value &value) {
    if (!locals_.empty()) {
        locals_.back()[name] = value;
    } else {
        throw std::runtime_error("No local scope to bind parameter: " + name);
    }
}

void Runtime::enterFunction() {
    locals_.emplace_back();
}

void Runtime::leaveFunction() {
    if (!locals_.empty()) {
        locals_.pop_back();
    } else {
        throw std::runtime_error("No local scope to leave");
    }
}

Value Runtime::makeTuple(const std::vector<Value> &elements) {
    return std::make_shared<const TupleValue>(TupleValue{elements});
}

std::string Runtime::toString(const Value &value) {
    if (const auto *p = std::get_if<NoneValue>(&value)) {
        return "None";
    }
    if (const auto *p = std::get_if<bool>(&value)) {
        return *p ? "True" : "False";
    }
    if (const auto *p = std::get_if<sjtu::int2048>(&value)) {
        return p->to_string();
    }
    if (const auto *p = std::get_if<double>(&value)) {
        return std::to_string(*p);
    }
    if (const auto *p = std::get_if<std::string>(&value)) {
        return *p;
    }
}

Value Runtime::toInt(const Value &value) {
    if (const auto *p = std::get_if<sjtu::int2048>(&value)) {
        return *p;
    }
    if (const auto *p = std::get_if<double>(&value)) {
        return static_cast<sjtu::int2048>(*p);
    }
    if (const auto *p = std::get_if<bool>(&value)) {
        return *p ? sjtu::int2048(1LL) : sjtu::int2048(0LL);
    }
    if (const auto *p = std::get_if<std::string>(&value)) {
        sjtu::int2048 result;
        result.read(*p);
        return result;
    }
}

Value Runtime::toFloat(const Value &value) {
    if (const auto *p = std::get_if<double>(&value)) {
        return *p;
    }
    if (const auto *p = std::get_if<sjtu::int2048>(&value)) {
        return std::stod((*p).to_string());
    }
    if (const auto *p = std::get_if<bool>(&value)) {
        return *p ? 1.0 : 0.0;
    }
    if (const auto *p = std::get_if<std::string>(&value)) {
        return std::stod(*p);
    }
}

Value Runtime::toStr(const Value &value) {
    return Value{toString(value)};
}

Value Runtime::toBool(const Value &value) {
    if (const auto *p = std::get_if<bool>(&value)) {
        return *p;
    }
    if (const auto *p = std::get_if<sjtu::int2048>(&value)) {
        return *p != sjtu::int2048(0LL);
    }
    if (const auto *p = std::get_if<double>(&value)) {
        return static_cast<bool>(*p);
    }
    if (const auto *p = std::get_if<std::string>(&value)) {
        return !p->empty();
    }
    if (const auto *p = std::get_if<NoneValue>(&value)) {
        return false;
    }
}