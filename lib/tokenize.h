/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2021 Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//---------------------------------------------------------------------------
#ifndef tokenizeH
#define tokenizeH
//---------------------------------------------------------------------------

#include "config.h"
#include "errortypes.h"
#include "tokenlist.h"

#include <list>
#include <map>
#include <string>
#include <stack>

class Settings;
class SymbolDatabase;
class TimerResults;
class Token;
class TemplateSimplifier;
class ErrorLogger;
class Preprocessor;

namespace simplecpp {
    class TokenList;
}

/// @addtogroup Core
/// @{

/** @brief The main purpose is to tokenize the source code. It also has functions that simplify the token list */
class CPPCHECKLIB Tokenizer {

    friend class TestSimplifyTokens;
    friend class TestSimplifyTypedef;
    friend class TestSimplifyUsing;
    friend class TestTokenizer;
    friend class SymbolDatabase;
    friend class TestSimplifyTemplate;
    friend class TemplateSimplifier;

    /** Class used in Tokenizer::setVarIdPass1 */
    class VariableMap {
    private:
        std::map<std::string, int> mVariableId;
        std::stack<std::list<std::pair<std::string,int>>> mScopeInfo;
        mutable nonneg int mVarId;
    public:
        VariableMap();
        void enterScope();
        bool leaveScope();
        void addVariable(const std::string &varname);
        bool hasVariable(const std::string &varname) const;
        std::map<std::string,int>::const_iterator find(const std::string &varname) const {
            return mVariableId.find(varname);
        }
        std::map<std::string,int>::const_iterator end() const {
            return mVariableId.end();
        }
        const std::map<std::string,int> &map() const {
            return mVariableId;
        }
        nonneg int *getVarId() const {
            return &mVarId;
        }
    };


public:
    Tokenizer();
    Tokenizer(const Settings * settings, ErrorLogger *errorLogger);
    ~Tokenizer();

    void setTimerResults(TimerResults *tr) {
        mTimerResults = tr;
    }

    /** Is the code C. Used for bailouts */
    bool isC() const {
        return list.isC();
    }

    /** Is the code CPP. Used for bailouts */
    bool isCPP() const {
        return list.isCPP();
    }

    /**
     * Check if inner scope ends with a call to a noreturn function
     * \param endScopeToken The '}' token
     * \param unknown set to true if it's unknown if the scope is noreturn
     * \return true if scope ends with a function call that might be 'noreturn'
     */
    bool isScopeNoReturn(const Token *endScopeToken, bool *unknown = nullptr) const;

    bool createTokens(std::istream &code, const std::string& FileName);
    void createTokens(simplecpp::TokenList&& tokenList);

    bool simplifyTokens1(const std::string &configuration);
    /**
     * Tokenize code
     * @param code input stream for code, e.g.
     * \code
     * #file "p.h"
     * class Foo
     * {
     * private:
     * void Bar();
     * };
     *
     * #endfile
     * void Foo::Bar()
     * {
     * }
     * \endcode
     *
     * @param FileName The filename
     * @param configuration E.g. "A" for code where "#ifdef A" is true
     * @return false if source code contains syntax errors
     */
    bool tokenize(std::istream &code,
                  const char FileName[],
                  const std::string &configuration = emptyString);

    /** Set variable id */
    void setVarId();
    void setVarIdPass1();
    void setVarIdPass2();

    /**
     * Basic simplification of tokenlist
     *
     * @param FileName The filename to run; used to do
     * markup checks.
     *
     * @return false if there is an error that requires aborting
     * the checking of this file.
     */
    bool simplifyTokenList1(const char FileName[]);

    /**
     * Most aggressive simplification of tokenlist
     *
     * @return false if there is an error that requires aborting
     * the checking of this file.
     */
    bool simplifyTokenList2();

    /**
     * If --check-headers=no has been given; then remove unneeded code in headers.
     * - All executable code.
     * - Unused types/variables/etc
     */
    void simplifyHeadersAndUnusedTemplates();

    /**
     * Remove extra "template" keywords that are not used by Cppcheck
     */
    void removeExtraTemplateKeywords();


    /** Split up template right angle brackets.
     * foo < bar < >> => foo < bar < > >
     */
    void splitTemplateRightAngleBrackets(bool check);


    /**
     * Deletes dead code between 'begin' and 'end'.
     * In general not everything can be erased, such as:
     * - code after labels;
     * - code outside the scope where the function is called;
     * - code after a change of scope caused by 'switch(...);'
     *   instructions, like 'case %any%;' or 'default;'
     * Also, if the dead code contains a 'switch' block
     * and inside it there's a label, the function removes all
     * the 'switch(..)' tokens and every occurrence of 'case %any%; | default;'
     * expression, such as the 'switch' block is reduced to a simple block.
     *
     * @param begin Tokens after this have a possibility to be erased.
     * @param end Tokens before this have a possibility to be erased.
     */
    static void eraseDeadCode(Token *begin, const Token *end);

    /**
     * Simplify '* & ( %name% ) =' or any combination of '* &' and '()'
     * parentheses around '%name%' to '%name% ='
     */
    void simplifyMulAndParens();

    /**
     * Calculates sizeof value for given type.
     * @param type Token which will contain e.g. "int", "*", or string.
     * @return sizeof for given type, or 0 if it can't be calculated.
     */
    nonneg int sizeOfType(const Token *type) const;

    /**
     * Try to determine if function parameter is passed by value by looking
     * at the function declaration.
     * @param fpar token for function parameter in the function call
     * @return true if the parameter is passed by value. if unsure, false is returned
     */
    bool isFunctionParameterPassedByValue(const Token *fpar) const;

    /** Simplify assignment in function call "f(x=g());" => "x=g();f(x);"
     */
    void simplifyAssignmentInFunctionCall();

    /** Simplify assignment where rhs is a block : "x=({123;});" => "{x=123;}" */
    void simplifyAssignmentBlock();

    /**
     * Simplify constant calculations such as "1+2" => "3"
     * @return true if modifications to token-list are done.
     *         false if no modifications are done.
     */
    bool simplifyCalculations();

    /**
     * Simplify dereferencing a pointer offset by a number:
     *     "*(ptr + num)" => "ptr[num]"
     *     "*(ptr - num)" => "ptr[-num]"
     */
    void simplifyOffsetPointerDereference();

    /**
     * Simplify referencing a pointer offset:
     *     "Replace "&str[num]" => "(str + num)"
     */
    void simplifyOffsetPointerReference();

    /** Insert array size where it isn't given */
    void arraySize();

    /** Simplify labels and 'case|default' syntaxes.
     */
    void simplifyLabelsCaseDefault();

    /** simplify case ranges (gcc extension)
     */
    void simplifyCaseRange();

    /** Remove macros in global scope */
    void removeMacrosInGlobalScope();

    void addSemicolonAfterUnknownMacro();

    // Remove C99 and CPP11 _Pragma(str)
    void removePragma();

    /** Remove undefined macro in class definition:
     * class DLLEXPORT Fred { };
     * class Fred FINAL : Base { };
     */
    void removeMacroInClassDef();

    /** Remove unknown macro in variable declarations: PROGMEM char x; */
    void removeMacroInVarDecl();

    /** Remove redundant assignment */
    void removeRedundantAssignment();

    /** Simplifies some realloc usage like
     * 'x = realloc (0, n);' => 'x = malloc(n);'
     * 'x = realloc (y, 0);' => 'x = 0; free(y);'
     */
    void simplifyRealloc();

    /** Add parentheses for sizeof: sizeof x => sizeof(x) */
    void sizeofAddParentheses();

    /**
     * Replace sizeof() to appropriate size.
     * @return true if modifications to token-list are done.
     *         false if no modifications are done.
     */
    bool simplifySizeof();

    /**
     * Simplify variable declarations (split up)
     * \param only_k_r_fpar Only simplify K&R function parameters
     */
    void simplifyVarDecl(const bool only_k_r_fpar);
    void simplifyVarDecl(Token * tokBegin, const Token * const tokEnd, const bool only_k_r_fpar);

    /**
     * Simplify variable initialization
     * '; int *p(0);' => '; int *p = 0;'
     */
    void simplifyInitVar();
    Token * initVar(Token * tok);

    /**
     * Simplify easy constant '?:' operation
     * Example: 0 ? (2/0) : 0 => 0
     * @return true if something is modified
     *         false if nothing is done.
     */
    bool simplifyConstTernaryOp();

    /**
     * Simplify compound assignments
     * Example: ";a+=b;" => ";a=a+b;"
     */
    void simplifyCompoundAssignment();

    /**
     * Simplify the location of "static" and "const" qualifiers in
     * a variable declaration or definition.
     * Example: "int static const a;" => "static const a;"
     * Example: "long long const static b;" => "static const long long b;"
     */
    void simplifyStaticConst();

    /**
     * Simplify assignments in "if" and "while" conditions
     * Example: "if(a=b);" => "a=b;if(a);"
     * Example: "while(a=b) { f(a); }" => "a = b; while(a){ f(a); a = b; }"
     * Example: "do { f(a); } while(a=b);" => "do { f(a); a = b; } while(a);"
     */
    void simplifyIfAndWhileAssign();

    /**
     * Simplify multiple assignments.
     * Example: "a = b = c = 0;" => "a = 0; b = 0; c = 0;"
     */
    void simplifyVariableMultipleAssign();

    /**
     * Simplify the 'C Alternative Tokens'
     * Examples:
     * "if(s and t)" => "if(s && t)"
     * "while((r bitand s) and not t)" => while((r & s) && !t)"
     * "a and_eq b;" => "a &= b;"
     */
    bool simplifyCAlternativeTokens();

    /**
     * Simplify comma into a semicolon when possible:
     * - "delete a, delete b" => "delete a; delete b;"
     * - "a = 0, b = 0;" => "a = 0; b = 0;"
     * - "return a(), b;" => "a(); return b;"
     */
    void simplifyComma();

    /** Add braces to an if-block, for-block, etc.
     * @return true if no syntax errors
     */
    bool simplifyAddBraces();

    /** Add braces to an if-block, for-block, etc.
     * for command starting at token including else-block
     * @return last token of command
     *         or input token in case of an error where no braces are added
     *         or NULL when syntaxError is called
     */
    Token * simplifyAddBracesToCommand(Token * tok);

    /** Add pair of braces to an single if-block, else-block, for-block, etc.
     * for command starting at token
     * @return last token of command
     *         or input token in case of an error where no braces are added
     *         or NULL when syntaxError is called
     */
    Token * simplifyAddBracesPair(Token *tok, bool commandWithCondition);

    // Convert "using ...;" to corresponding typedef
    void simplifyUsingToTypedef();

    /**
     * typedef A mytype;
     * mytype c;
     *
     * Becomes:
     * typedef A mytype;
     * A c;
     */
    void simplifyTypedef();

    /**
     */
    bool isMemberFunction(const Token *openParen) const;

    /**
     */
    bool simplifyUsing();

    /**
     * Simplify casts
     */
    void simplifyCasts();

    /**
     * Change (multiple) arrays to (multiple) pointers.
     */
    void simplifyUndefinedSizeArray();

    /**
     * A simplify function that replaces a variable with its value in cases
     * when the value is known. e.g. "x=10; if(x)" => "x=10;if(10)"
     *
     * @return true if modifications to token-list are done.
     *         false if no modifications are done.
     */
    bool simplifyKnownVariables();

    /**
     * Utility function for simplifyKnownVariables. Get data about an
     * assigned variable.
     */
    static bool simplifyKnownVariablesGetData(nonneg int varid, Token **_tok2, Token **_tok3, std::string &value, nonneg int &valueVarId, bool &valueIsPointer, bool floatvar);

    /**
     * utility function for simplifyKnownVariables. Perform simplification
     * of a given variable
     */
    bool simplifyKnownVariablesSimplify(Token **tok2, Token *tok3, nonneg int varid, const std::string &structname, std::string &value, nonneg int valueVarId, bool valueIsPointer, const Token * const valueToken, int indentlevel) const;

    /** Simplify useless C++ empty namespaces, like: 'namespace %name% { }'*/
    void simplifyEmptyNamespaces();

    /** Simplify redundant code placed after control flow statements :
     * 'return', 'throw', 'goto', 'break' and 'continue'
     */
    void simplifyFlowControl();

    /** Expand nested strcat() calls. */
    void simplifyNestedStrcat();

    /** Simplify "if else" */
    void elseif();

    /** Simplify C++17/C++20 if/switch/for initialization expression */
    void simplifyIfSwitchForInit();

    /** Simplify conditions
     * @return true if something is modified
     *         false if nothing is done.
     */
    bool simplifyConditions();

    /** Remove redundant code, e.g. if( false ) { int a; } should be
     * removed, because it is never executed.
     * @return true if something is modified
     *         false if nothing is done.
     */
    bool removeRedundantConditions();

    /**
     * Remove redundant for:
     * "for (x=0;x<1;x++) { }" => "{ x = 1; }"
     */
    void removeRedundantFor();


    /**
     * Reduces "; ;" to ";", except in "( ; ; )"
     */
    void removeRedundantSemicolons();

    /** Simplify function calls - constant return value
     * @return true if something is modified
     *         false if nothing is done.
     */
    bool simplifyFunctionReturn();

    /** Struct simplification
     * "struct S { } s;" => "struct S { }; S s;"
     */

    void simplifyStructDecl();

    /**
     * Remove redundant parentheses:
     * - "((x))" => "(x)"
     * - "(function())" => "function()"
     * - "(delete x)" => "delete x"
     * - "(delete [] x)" => "delete [] x"
     * @return true if modifications to token-list are done.
     *         false if no modifications are done.
     */
    bool simplifyRedundantParentheses();

    void simplifyCharAt();

    /** Simplify references */
    void simplifyReference();

    /**
     * Simplify functions like "void f(x) int x; {"
     * into "void f(int x) {"
     */
    void simplifyFunctionParameters();

    /** Simplify function level try blocks:
     *  Convert "void f() try {} catch (int) {}"
     *  to "void f() { try {} catch (int) {} }"
     */
    void simplifyFunctionTryCatch();

    /**
     * Simplify templates
     */
    void simplifyTemplates();

    void simplifyDoublePlusAndDoubleMinus();

    void simplifyRedundantConsecutiveBraces();

    void simplifyArrayAccessSyntax();

    void simplifyParameterVoid();

    void fillTypeSizes();

    void combineOperators();

    void combineStringAndCharLiterals();

    void concatenateNegativeNumberAndAnyPositive();

    void simplifyExternC();

    void simplifyRoundCurlyParentheses();

    void simplifyTypeIntrinsics();

    void simplifySQL();

    void checkForEnumsWithTypedef();

    void findComplicatedSyntaxErrorsInTemplates();

    /**
     * Simplify e.g. 'atol("0")' into '0'
     */
    void simplifyMathFunctions();

    /**
     * Simplify e.g. 'sin(0)' into '0'
     */
    void simplifyMathExpressions();

    /**
     * Modify strings in the token list by replacing hex and oct
     * values. E.g. "\x61" -> "a" and "\000" -> "\0"
     * @param source The string to be modified, e.g. "\x61"
     * @return Modified string, e.g. "a"
     */
    static std::string simplifyString(const std::string &source);

    /**
     * is token pointing at function head?
     * @param tok         A '(' or ')' token in a possible function head
     * @param endsWith    string after function head
     * @return token matching with endsWith if syntax seems to be a function head else nullptr
     */
    const Token * isFunctionHead(const Token *tok, const std::string &endsWith) const;

    /**
     * is token pointing at function head?
     * @param tok         A '(' or ')' token in a possible function head
     * @param endsWith    string after function head
     * @param cpp         c++ code
     * @return token matching with endsWith if syntax seems to be a function head else nullptr
     */
    static const Token * isFunctionHead(const Token *tok, const std::string &endsWith, bool cpp);

    void setPreprocessor(const Preprocessor *preprocessor) {
        mPreprocessor = preprocessor;
    }
    const Preprocessor *getPreprocessor() const {
        return mPreprocessor;
    }

    bool hasIfdef(const Token *start, const Token *end) const;

private:

    /**
     * simplify "while (0)"
     */
    void simplifyWhile0();

    /**
     * Simplify while(func() && errno==EINTR)
     */
    void simplifyErrNoInWhile();

    /**
     * Simplify while(func(f))
     */
    void simplifyFuncInWhile();

    /**
     * Remove "std::" before some function names
     */
    void simplifyStd();

    /** Simplify pointer to standard type (C only) */
    void simplifyPointerToStandardType();

    /** Simplify function pointers */
    void simplifyFunctionPointers();

    /**
     * Send error message to error logger about internal bug.
     * @param tok the token that this bug concerns.
     */
    NORETURN void cppcheckError(const Token *tok) const;

    /**
     * Setup links for tokens so that one can call Token::link().
     */
    void createLinks();

    /**
     * Setup links between < and >.
     */
    void createLinks2();

public:

    /** Syntax error */
    NORETURN void syntaxError(const Token *tok, const std::string &code = "") const;

    /** Syntax error. Unmatched character. */
    NORETURN void unmatchedToken(const Token *tok) const;

    /** Syntax error. C++ code in C file. */
    NORETURN void syntaxErrorC(const Token *tok, const std::string &what) const;

    /** Warn about unknown macro(s), configuration is recommended */
    NORETURN void unknownMacroError(const Token *tok1) const;

    void unhandledCharLiteral(const Token *tok, const std::string& msg) const;

private:

    /** Report that there is an unhandled "class x y {" code */
    void unhandled_macro_class_x_y(const Token *tok) const;

    /** Check configuration (unknown macros etc) */
    void checkConfiguration() const;
    void macroWithSemicolonError(const Token *tok, const std::string &macroName) const;

    /**
     * Is there C++ code in C file?
     */
    void validateC() const;

    /**
     * assert that tokens are ok - used during debugging for example
     * to catch problems in simplifyTokenList1/2.
     */
    void validate() const;

    /** Detect unknown macros and throw unknownMacro */
    void reportUnknownMacros() const;

    /** Detect garbage code and call syntaxError() if found. */
    void findGarbageCode() const;

    /** Detect garbage expression */
    static bool isGarbageExpr(const Token *start, const Token *end, bool allowSemicolon);

    /**
     * Remove __declspec()
     */
    void simplifyDeclspec();

    /**
     * Remove calling convention
     */
    void simplifyCallingConvention();

    /**
     * Remove \__attribute\__ ((?))
     */
    void simplifyAttribute();

    /**
     * Remove \__cppcheck\__ ((?))
     */
    void simplifyCppcheckAttribute();

    /** Remove alignas */
    void removeAlignas();

    /** Simplify c++20 spaceship operator */
    void simplifySpaceshipOperator();

    /**
     * Remove keywords "volatile", "inline", "register", and "restrict"
     */
    void simplifyKeyword();

    /**
     * Remove __asm
     */
    void simplifyAsm();

    /**
     * asm heuristics, Put ^{} statements in asm()
     */
    void simplifyAsm2();

    /**
     * Simplify \@&hellip;  (compiler extension)
     */
    void simplifyAt();

    /**
     * Simplify bitfields - the field width is removed as we don't use it.
     */
    void simplifyBitfields();

    /**
     * Remove unnecessary member qualification
     */
    void removeUnnecessaryQualification();

    /**
     * Add std:: in front of std classes, when using namespace std; was given
     */
    void simplifyNamespaceStd();

    /**
     * Convert Microsoft memory functions
     * CopyMemory(dst, src, len) -> memcpy(dst, src, len)
     * FillMemory(dst, len, val) -> memset(dst, val, len)
     * MoveMemory(dst, src, len) -> memmove(dst, src, len)
     * ZeroMemory(dst, len) -> memset(dst, 0, len)
     */
    void simplifyMicrosoftMemoryFunctions();

    /**
     * Convert Microsoft string functions
     * _tcscpy -> strcpy
     */
    void simplifyMicrosoftStringFunctions();

    /**
     * Remove Borland code
     */
    void simplifyBorland();

    /**
     * Remove Qt signals and slots
     */
    void simplifyQtSignalsSlots();

    /**
     * Collapse operator name tokens into single token
     * operator = => operator=
     */
    void simplifyOperatorName();

    /** simplify overloaded operators: 'obj(123)' => 'obj . operator() ( 123 )' */
    void simplifyOverloadedOperators();

    /**
     * Remove [[attribute]] (C++11 and later) from TokenList
     */
    void simplifyCPPAttribute();

    /**
     * Replace strlen(str)
     * @return true if any replacement took place, false else
     * */
    bool simplifyStrlen();

    /**
     * Convert namespace aliases
     */
    void simplifyNamespaceAliases();

    /**
     * Convert C++17 style nested namespace to older style
     */
    void simplifyNestedNamespace();

    /**
     * Simplify coroutines - just put parentheses around arguments for
     * co_* keywords so they can be handled like function calls in data
     * flow.
     */
    void simplifyCoroutines();

    /**
     * Prepare ternary operators with parentheses so that the AST can be created
     * */
    void prepareTernaryOpForAST();

    /**
     * report error message
     */
    void reportError(const Token* tok, const Severity::SeverityType severity, const std::string& id, const std::string& msg, bool inconclusive = false) const;
    void reportError(const std::list<const Token*>& callstack, Severity::SeverityType severity, const std::string& id, const std::string& msg, bool inconclusive = false) const;

    bool duplicateTypedef(Token **tokPtr, const Token *name, const Token *typeDef) const;

    void unsupportedTypedef(const Token *tok) const;

    void setVarIdClassDeclaration(const Token * const startToken,
                                  const VariableMap &variableMap,
                                  const nonneg int scopeStartVarId,
                                  std::map<int, std::map<std::string,int>>& structMembers);

    void setVarIdStructMembers(Token **tok1,
                               std::map<int, std::map<std::string, int>>& structMembers,
                               nonneg int *varId) const;

    void setVarIdClassFunction(const std::string &classname,
                               Token * const startToken,
                               const Token * const endToken,
                               const std::map<std::string,int> &varlist,
                               std::map<int, std::map<std::string,int>>& structMembers,
                               nonneg int *varId_);

    /**
     * Simplify e.g. 'return(strncat(temp,"a",1));' into
     * strncat(temp,"a",1); return temp;
     */
    void simplifyReturnStrncat();

    /**
     * Output list of unknown types.
     */
    void printUnknownTypes() const;

    /** Find end of SQL (or PL/SQL) block */
    static const Token *findSQLBlockEnd(const Token *tokSQLStart);

    bool operatorEnd(const Token * tok) const;

public:

    /** Was there templates in the code? */
    bool codeWithTemplates() const {
        return mCodeWithTemplates;
    }


    void setSettings(const Settings *settings) {
        mSettings = settings;
        list.setSettings(settings);
    }

    const SymbolDatabase *getSymbolDatabase() const {
        return mSymbolDatabase;
    }
    void createSymbolDatabase();
    void deleteSymbolDatabase();

    /** print --debug output if debug flags match the simplification:
     * 0=unknown/both simplifications
     * 1=1st simplifications
     * 2=2nd simplifications
     */
    void printDebugOutput(int simplification) const;

    void dump(std::ostream &out) const;

    Token *deleteInvalidTypedef(Token *typeDef);

    /**
     * Get variable count.
     * @return number of variables
     */
    nonneg int varIdCount() const {
        return mVarId;
    }

    /**
     * Token list: stores all tokens.
     */
    TokenList list;
    // Implement tokens() as a wrapper for convenience when using the TokenList
    const Token* tokens() const {
        return list.front();
    }

    /**
     * Helper function to check whether number is zero (0 or 0.0 or 0E+0) or not?
     * @param s the string to check
     * @return true in case is is zero and false otherwise.
     */
    static bool isZeroNumber(const std::string &s);

    /**
     * Helper function to check whether number is one (1 or 0.1E+1 or 1E+0) or not?
     * @param s the string to check
     * @return true in case is is one and false otherwise.
     */
    static bool isOneNumber(const std::string &s);

    /**
     * Helper function to check whether number is two (2 or 0.2E+1 or 2E+0) or not?
     * @param s the string to check
     * @return true in case is is two and false otherwise.
     */
    static bool isTwoNumber(const std::string &s);

    /**
     * Helper function to check for start of function execution scope.
     * Do not use this in checks.  Use the symbol database.
     * @param tok pointer to end parentheses of parameter list
     * @return pointer to start brace of function scope or nullptr if not start.
     */
    static const Token * startOfExecutableScope(const Token * tok);

#ifdef MAXTIME
    bool isMaxTime() const {
        return (std::time(0) > mMaxTime);
#else
    static bool isMaxTime() {
        return false;
#endif
    }

    const Settings *getSettings() const {
        return mSettings;
    }

    void calculateScopes();

    /** Disable copy constructor */
    Tokenizer(const Tokenizer &) = delete;

    /** Disable assignment operator */
    Tokenizer &operator=(const Tokenizer &) = delete;

private:
    Token *processFunc(Token *tok2, bool inOperator) const;

    /**
     * Get new variable id.
     * @return new variable id
     */
    nonneg int newVarId() {
        return ++mVarId;
    }

    /** Set pod types */
    void setPodTypes();

    /** settings */
    const Settings * mSettings;

    /** errorlogger */
    ErrorLogger* const mErrorLogger;

    /** Symbol database that all checks etc can use */
    SymbolDatabase *mSymbolDatabase;

    TemplateSimplifier *mTemplateSimplifier;

    /** E.g. "A" for code where "#ifdef A" is true. This is used to
        print additional information in error situations. */
    std::string mConfiguration;

    /** sizeof information for known types */
    std::map<std::string, int> mTypeSize;

    struct TypedefInfo {
        std::string name;
        std::string filename;
        int lineNumber;
        int column;
        bool used;
    };
    std::vector<TypedefInfo> mTypedefInfo;

    /** variable count */
    nonneg int mVarId;

    /** unnamed count "Unnamed0", "Unnamed1", "Unnamed2", ... */
    nonneg int mUnnamedCount;

    /**
     * was there any templates? templates that are "unused" are
     * removed from the token list
     */
    bool mCodeWithTemplates;

    /**
     * TimerResults
     */
    TimerResults *mTimerResults;

#ifdef MAXTIME
    /** Tokenizer maxtime */
    const std::time_t mMaxTime;
#endif

    const Preprocessor *mPreprocessor;
};

/// @}

//---------------------------------------------------------------------------
#endif // tokenizeH
