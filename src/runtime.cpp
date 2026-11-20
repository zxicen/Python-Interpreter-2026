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