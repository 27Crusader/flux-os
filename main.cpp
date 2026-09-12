#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <fstream>
#include <sstream>
#include <memory>
#include <unordered_map>
#include <variant>

// Multi-type memory support (Integer, String, Array)
using ValueType = std::variant<int, std::string, std::vector<int>>;

// Global environment memory
std::unordered_map<std::string, ValueType> memory;

// --- 1. LEXER ---
enum TokenType { 
    NUMBER, STRING_LIT, PLUS, MINUS, STAR, SLASH, LPAREN, RPAREN, 
    LBRACE, RBRACE, LBRACK, RBRACK, COMMA, GREATER, LESS, EQUALS_EQUALS, 
    IDENTIFIER, EQUALS, SEMICOLON, PRINT, IF, ELSE, WHILE, INPUT, CLEAR, 
    FUNC, RETURN, END_OF_FILE 
};

struct Token { TokenType type; std::string value; };

std::vector<Token> tokenize(const std::string& source) {
    std::vector<Token> tokens;
    int i = 0;
    while (i < source.length()) {
        char c = source[i];
        if (isspace(c)) { i++; continue; }
        if (c == '+') { tokens.push_back({PLUS, "+"}); i++; continue; }
        if (c == '-') { tokens.push_back({MINUS, "-"}); i++; continue; }
        if (c == '*') { tokens.push_back({STAR, "*"}); i++; continue; }
        if (c == '/') { tokens.push_back({SLASH, "/"}); i++; continue; }
        if (c == '(') { tokens.push_back({LPAREN, "("}); i++; continue; }
        if (c == ')') { tokens.push_back({RPAREN, ")"}); i++; continue; }
        if (c == '{') { tokens.push_back({LBRACE, "{"}); i++; continue; }
        if (c == '}') { tokens.push_back({RBRACE, "}"}); i++; continue; }
        if (c == '[') { tokens.push_back({LBRACK, "["}); i++; continue; }
        if (c == ']') { tokens.push_back({RBRACK, "]"}); i++; continue; }
        if (c == ',') { tokens.push_back({COMMA, ","}); i++; continue; }
        if (c == '>') { tokens.push_back({GREATER, ">"}); i++; continue; }
        if (c == '<') { tokens.push_back({LESS, "<"}); i++; continue; }
        if (c == '=') {
            if (i + 1 < source.length() && source[i+1] == '=') {
                tokens.push_back({EQUALS_EQUALS, "=="}); i += 2; continue;
            }
            tokens.push_back({EQUALS, "="}); i++; continue;
        }
        if (c == ';') { tokens.push_back({SEMICOLON, ";"}); i++; continue; } 
        
        if (c == '"') {
            i++;
            std::string str = "";
            while (i < source.length() && source[i] != '"') { str += source[i]; i++; }
            i++;
            tokens.push_back({STRING_LIT, str});
            continue;
        }

        if (isdigit(c)) {
            std::string numStr = "";
            while (i < source.length() && isdigit(source[i])) { numStr += source[i]; i++; }
            tokens.push_back({NUMBER, numStr});
            continue;
        }
        
        if (isalpha(c)) {
            std::string word = "";
            while (i < source.length() && isalnum(source[i])) { word += source[i]; i++; }
            if (word == "print") tokens.push_back({PRINT, "print"});
            else if (word == "input") tokens.push_back({INPUT, "input"});
            else if (word == "clear") tokens.push_back({CLEAR, "clear"});
            else if (word == "if") tokens.push_back({IF, "if"});
            else if (word == "else") tokens.push_back({ELSE, "else"});
            else if (word == "while") tokens.push_back({WHILE, "while"});
            else if (word == "func") tokens.push_back({FUNC, "func"});
            else if (word == "return") tokens.push_back({RETURN, "return"});
            else tokens.push_back({IDENTIFIER, word});
            continue;
        }
        i++; 
    }
    tokens.push_back({END_OF_FILE, ""});
    return tokens;
}

// --- 2. AST INTERPRETER NODES ---
struct ASTNode {
    virtual ~ASTNode() = default;
    virtual ValueType evaluate() = 0; 
};

struct FunctionNode;
std::unordered_map<std::string, FunctionNode*> functions;

struct NumberNode : public ASTNode {
    int value;
    NumberNode(int val) : value(val) {}
    ValueType evaluate() override { return value; }
};

struct StringNode : public ASTNode {
    std::string value;
    StringNode(std::string val) : value(val) {}
    ValueType evaluate() override { return value; }
};

struct VarNode : public ASTNode {
    std::string name;
    VarNode(std::string n) : name(n) {}
    ValueType evaluate() override { return memory[name]; } 
};

struct AssignNode : public ASTNode {
    std::string name;
    std::unique_ptr<ASTNode> value;
    AssignNode(std::string n, std::unique_ptr<ASTNode> v) : name(n), value(std::move(v)) {}
    ValueType evaluate() override {
        ValueType val = value->evaluate();
        memory[name] = val;
        return val;
    }
};

struct ArrayNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> elements;
    ArrayNode(std::vector<std::unique_ptr<ASTNode>> el) : elements(std::move(el)) {}
    ValueType evaluate() override {
        std::vector<int> arr;
        for (const auto& el : elements) {
            arr.push_back(std::get<int>(el->evaluate()));
        }
        return arr;
    }
};

struct ArrayAccessNode : public ASTNode {
    std::string name;
    std::unique_ptr<ASTNode> index;
    ArrayAccessNode(std::string n, std::unique_ptr<ASTNode> idx) : name(n), index(std::move(idx)) {}
    ValueType evaluate() override {
        auto arr = std::get<std::vector<int>>(memory[name]);
        int idx = std::get<int>(index->evaluate());
        return arr[idx];
    }
};

struct ArrayAssignNode : public ASTNode {
    std::string name;
    std::unique_ptr<ASTNode> index;
    std::unique_ptr<ASTNode> value;
    ArrayAssignNode(std::string n, std::unique_ptr<ASTNode> idx, std::unique_ptr<ASTNode> v) 
        : name(n), index(std::move(idx)), value(std::move(v)) {}
    ValueType evaluate() override {
        int idx = std::get<int>(index->evaluate());
        int val = std::get<int>(value->evaluate());
        auto& arr = std::get<std::vector<int>>(memory[name]);
        arr[idx] = val;
        return val;
    }
};

struct InputNode : public ASTNode {
    ValueType evaluate() override {
        int val;
        std::cout << "? ";
        std::cin >> val;
        return val;
    }
};

struct ClearNode : public ASTNode {
    ValueType evaluate() override {
        std::cout << "\033[2J\033[1;1H";
        return 0;
    }
};

struct PrintNode : public ASTNode {
    std::unique_ptr<ASTNode> expr;
    PrintNode(std::unique_ptr<ASTNode> e) : expr(std::move(e)) {}
    ValueType evaluate() override {
        ValueType val = expr->evaluate();
        if (std::holds_alternative<int>(val)) {
            std::cout << ">> " << std::get<int>(val) << "\n";
        } else if (std::holds_alternative<std::string>(val)) {
            std::cout << std::get<std::string>(val) << "\n";
        } else if (std::holds_alternative<std::vector<int>>(val)) {
            std::cout << "[ ";
            for (int i : std::get<std::vector<int>>(val)) std::cout << i << " ";
            std::cout << "]\n";
        }
        return val;
    }
};

struct BlockNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;
    ValueType evaluate() override {
        ValueType last = 0;
        for (const auto& stmt : statements) last = stmt->evaluate();
        return last;
    }
};

struct IfNode : public ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> thenBranch;
    std::unique_ptr<ASTNode> elseBranch;
    IfNode(std::unique_ptr<ASTNode> cond, std::unique_ptr<ASTNode> thenB, std::unique_ptr<ASTNode> elseB = nullptr) 
        : condition(std::move(cond)), thenBranch(std::move(thenB)), elseBranch(std::move(elseB)) {}
    ValueType evaluate() override {
        if (std::get<int>(condition->evaluate()) != 0) {
            return thenBranch->evaluate();
        } else if (elseBranch) {
            return elseBranch->evaluate();
        }
        return 0;
    }
};

struct WhileNode : public ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> body;
    WhileNode(std::unique_ptr<ASTNode> cond, std::unique_ptr<ASTNode> b) 
        : condition(std::move(cond)), body(std::move(b)) {}
    ValueType evaluate() override {
        while (std::get<int>(condition->evaluate()) != 0) {
            body->evaluate();
        }
        return 0;
    }
};

struct FunctionNode : public ASTNode {
    std::string name;
    std::vector<std::string> params;
    std::unique_ptr<ASTNode> body;
    FunctionNode(std::string n, std::vector<std::string> p, std::unique_ptr<ASTNode> b)
        : name(n), params(p), body(std::move(b)) {}
    ValueType evaluate() override {
        functions[name] = this;
        return 0;
    }
};

struct CallNode : public ASTNode {
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> args;
    CallNode(std::string n, std::vector<std::unique_ptr<ASTNode>> a) : name(n), args(std::move(a)) {}
    ValueType evaluate() override {
        FunctionNode* func = functions[name];
        auto oldMemory = memory;
        for (size_t i = 0; i < args.size(); ++i) {
            memory[func->params[i]] = args[i]->evaluate();
        }
        ValueType result = func->body->evaluate();
        memory = oldMemory;
        return result;
    }
};

struct OpNode : public ASTNode {
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    TokenType op;
    OpNode(std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> r, TokenType o) : left(std::move(l)), right(std::move(r)), op(o) {}
    ValueType evaluate() override {
        int l = std::get<int>(left->evaluate());
        int r = std::get<int>(right->evaluate());
        if (op == PLUS) return l + r;
        if (op == MINUS) return l - r;
        if (op == STAR) return l * r;
        if (op == SLASH) return l / r;
        if (op == GREATER) return (l > r) ? 1 : 0;
        if (op == LESS) return (l < r) ? 1 : 0;
        if (op == EQUALS_EQUALS) return (l == r) ? 1 : 0;
        return 0;
    }
};

struct ProgramNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;
    ValueType evaluate() override {
        for (const auto& stmt : statements) stmt->evaluate();
        return 0;
    }
};

// --- 3. PARSER ---
class Parser {
    std::vector<Token> tokens;
    int pos = 0;

    Token current() { return pos < tokens.size() ? tokens[pos] : tokens.back(); }
    Token peek() { return pos + 1 < tokens.size() ? tokens[pos + 1] : tokens.back(); }
    void advance() { pos++; }

public:
    Parser(std::vector<Token> t) : tokens(t) {}

    std::unique_ptr<ASTNode> parseProgram() {
        auto program = std::make_unique<ProgramNode>();
        while (current().type != END_OF_FILE) program->statements.push_back(parseStatement());
        return program;
    }

private:
    std::unique_ptr<ASTNode> parseStatement() {
        if (current().type == CLEAR) {
            advance();
            if (current().type == SEMICOLON) advance();
            return std::make_unique<ClearNode>();
        }

        if (current().type == FUNC) {
            advance();
            std::string name = current().value; advance();
            advance(); // skip '('
            std::vector<std::string> params;
            while (current().type != RPAREN) {
                params.push_back(current().value); advance();
                if (current().type == COMMA) advance();
            }
            advance(); // skip ')'
            advance(); // skip '{'
            auto block = std::make_unique<BlockNode>();
            while (current().type != RBRACE && current().type != END_OF_FILE) block->statements.push_back(parseStatement());
            advance();
            return std::make_unique<FunctionNode>(name, params, std::move(block));
        }

        if (current().type == IF) {
            advance(); advance(); 
            auto condition = parseComparison();
            advance(); advance(); 
            auto thenBlock = std::make_unique<BlockNode>();
            while (current().type != RBRACE && current().type != END_OF_FILE) thenBlock->statements.push_back(parseStatement());
            advance();
            
            std::unique_ptr<ASTNode> elseBlock = nullptr;
            if (current().type == ELSE) {
                advance(); advance(); // skip else {
                elseBlock = std::make_unique<BlockNode>();
                while (current().type != RBRACE && current().type != END_OF_FILE) {
                    static_cast<BlockNode*>(elseBlock.get())->statements.push_back(parseStatement());
                }
                advance();
            }
            return std::make_unique<IfNode>(std::move(condition), std::move(thenBlock), std::move(elseBlock));
        }

        if (current().type == WHILE) {
            advance(); advance(); 
            auto condition = parseComparison();
            advance(); advance(); 
            auto block = std::make_unique<BlockNode>();
            while (current().type != RBRACE && current().type != END_OF_FILE) block->statements.push_back(parseStatement());
            advance();
            return std::make_unique<WhileNode>(std::move(condition), std::move(block));
        }
        
        if (current().type == PRINT) {
            advance();
            auto expr = parseComparison();
            if (current().type == SEMICOLON) advance();
            return std::make_unique<PrintNode>(std::move(expr));
        }

        if (current().type == IDENTIFIER && peek().type == EQUALS) {
            std::string varName = current().value;
            advance(); advance(); 
            auto expr = std::make_unique<AssignNode>(varName, parseComparison());
            if (current().type == SEMICOLON) advance();
            return expr;
        }

        if (current().type == IDENTIFIER && peek().type == LBRACK) {
            std::string name = current().value; advance(); advance();
            auto idx = parseComparison(); advance(); advance(); // skip ']' and '='
            auto val = parseComparison();
            if (current().type == SEMICOLON) advance();
            return std::make_unique<ArrayAssignNode>(name, std::move(idx), std::move(val));
        }
        
        auto expr = parseComparison();
        if (current().type == SEMICOLON) advance();
        return expr;
    }

    std::unique_ptr<ASTNode> parseComparison() {
        auto left = parseExpression();
        while (current().type == GREATER || current().type == LESS || current().type == EQUALS_EQUALS) {
            TokenType op = current().type; advance();
            left = std::make_unique<OpNode>(std::move(left), parseExpression(), op);
        }
        return left;
    }

    std::unique_ptr<ASTNode> parsePrimary() {
        Token t = current();
        if (t.type == INPUT) { advance(); return std::make_unique<InputNode>(); }
        if (t.type == NUMBER) { advance(); return std::make_unique<NumberNode>(std::stoi(t.value)); }
        if (t.type == STRING_LIT) { advance(); return std::make_unique<StringNode>(t.value); }
        
        if (t.type == LBRACK) {
            advance();
            std::vector<std::unique_ptr<ASTNode>> elements;
            while (current().type != RBRACK) {
                elements.push_back(parseComparison());
                if (current().type == COMMA) advance();
            }
            advance();
            return std::make_unique<ArrayNode>(std::move(elements));
        }

        if (t.type == IDENTIFIER && peek().type == LPAREN) {
            std::string name = t.value; advance(); advance();
            std::vector<std::unique_ptr<ASTNode>> args;
            while (current().type != RPAREN) {
                args.push_back(parseComparison());
                if (current().type == COMMA) advance();
            }
            advance();
            return std::make_unique<CallNode>(name, std::move(args));
        }

        if (t.type == IDENTIFIER && peek().type == LBRACK) {
            std::string name = t.value; advance(); advance();
            auto idx = parseComparison(); advance();
            return std::make_unique<ArrayAccessNode>(name, std::move(idx));
        }

        if (t.type == IDENTIFIER) { advance(); return std::make_unique<VarNode>(t.value); }
        if (t.type == LPAREN) {
            advance(); 
            auto expr = parseComparison(); 
            if (current().type == RPAREN) advance();
            return expr;
        }
        return nullptr;
    }

    std::unique_ptr<ASTNode> parseTerm() {
        auto left = parsePrimary();
        while (current().type == STAR || current().type == SLASH) {
            TokenType op = current().type; advance();
            left = std::make_unique<OpNode>(std::move(left), parsePrimary(), op);
        }
        return left;
    }

    std::unique_ptr<ASTNode> parseExpression() {
        auto left = parseTerm();
        while (current().type == PLUS || current().type == MINUS) {
            TokenType op = current().type; advance();
            left = std::make_unique<OpNode>(std::move(left), parseTerm(), op);
        }
        return left;
    }
};

// --- 4. ENTRY POINT ---
int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string filename = argv[1];
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error opening file: " << filename << "\n";
            return 1;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        
        std::vector<Token> tokens = tokenize(buffer.str());
        Parser parser(tokens);
        std::unique_ptr<ASTNode> tree = parser.parseProgram();

        if (tree) {
            tree->evaluate();
        }
        return 0;
    }

    std::cout << "=== Flux v0.2 Complete Language Engine ===\n";
    std::cout << "Type 'exit' to quit.\n\n";

    std::string line;
    while (true) {
        std::cout << "flux> " << std::flush;
        if (!std::getline(std::cin, line) || line == "exit") {
            std::cout << "Goodbye!\n";
            break;
        }

        if (line.empty()) continue;

        if (line.back() != ';' && line.back() != '}') {
            line += ";";
        }

        std::vector<Token> tokens = tokenize(line);
        Parser parser(tokens);
        std::unique_ptr<ASTNode> tree = parser.parseProgram();

        if (tree) {
            tree->evaluate();
        }
    }

    return 0;
}
