// MODULE: calc
// Purpose : evaluate arithmetic expressions (+ - * / % ^)
// Exports : calc()
// Depends : common.h

#include <cmath>

struct calc_parser {
    const std::string& s;
    size_t i;
    calc_parser(const std::string& s) : s(s), i(0) {}

    void skip() { while (i < s.size() && s[i] == ' ') i++; }

    double primary() {
        skip();
        if (i < s.size() && s[i] == '(') {
            i++;
            double v = expr();
            skip();
            if (i < s.size() && s[i] == ')') i++;
            return v;
        }
        size_t j = i;
        if (i < s.size() && (s[i] == '-' || s[i] == '+')) i++;
        while (i < s.size() && (isdigit((unsigned char)s[i]) || s[i] == '.')) i++;
        if (j == i) throw std::runtime_error("expected number");
        return std::stod(s.substr(j, i - j));
    }

    double power() {
        double base = primary();
        skip();
        if (i < s.size() && s[i] == '^') { i++; return std::pow(base, power()); }
        return base;
    }

    double term() {
        double v = power();
        while (true) {
            skip();
            if (i >= s.size()) break;
            char op = s[i];
            if (op != '*' && op != '/' && op != '%') break;
            i++;
            double r = power();
            if      (op == '*') v *= r;
            else if (op == '/') v /= r;
            else                v  = std::fmod(v, r);
        }
        return v;
    }

    double expr() {
        double v = term();
        while (true) {
            skip();
            if (i >= s.size()) break;
            char op = s[i];
            if (op != '+' && op != '-') break;
            i++;
            double r = term();
            if (op == '+') v += r;
            else           v -= r;
        }
        return v;
    }
};

static int calc(const std::string& expression) {
    if (expression.empty()) { err("Usage: calc <expression>\r\n"); return 1; }
    try {
        calc_parser p(expression);
        double result = p.expr();
        char buf[64];
        if (result == (long long)result)
            snprintf(buf, sizeof(buf), "%lld\r\n", (long long)result);
        else
            snprintf(buf, sizeof(buf), "%g\r\n", result);
        out(buf);
        return 0;
    } catch (...) {
        err("Invalid expression\r\n");
        return 1;
    }
}
