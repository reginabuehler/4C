// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef FOUR_C_UTILS_SYMBOLIC_EXPRESSION_DETAILS_HPP
#define FOUR_C_UTILS_SYMBOLIC_EXPRESSION_DETAILS_HPP

#include "4C_config.hpp"

#include "4C_utils_enum.hpp"
#include "4C_utils_exceptions.hpp"
#include "4C_utils_fad_meta.hpp"
#include "4C_utils_std23_unreachable.hpp"
#include "4C_utils_symbolic_expression.fwd.hpp"

#include <Sacado.hpp>

#include <bitset>
#include <cmath>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <utility>

FOUR_C_NAMESPACE_OPEN


namespace Core::Utils::SymbolicExpressionDetails
{
  // This type limits the maximum number of variables, AST nodes, instructions etc. to 2^32-1, which
  // is more than enough.
  using IndexType = std::uint16_t;
  constexpr IndexType invalid_index = std::numeric_limits<IndexType>::max();

  enum class NodeType : std::uint8_t
  {
    // numeric value
    number,
    // unary function e.g. 'sin', 'cos', 'exp', ...
    unary_function,
    // binary function e.g. '+', '-', '*', '/', ...
    binary_function,
    // independent variable: 't', 'x', ...
    variable,
  };

  enum class UnaryFunctionType : std::uint8_t
  {
    acos,
    asin,
    atan,
    cos,
    sin,
    tan,
    cosh,
    sinh,
    tanh,
    exp,
    log,
    log10,
    sqrt,
    heaviside,
    fabs,
    logical_not,
  };

  template <typename T>
  using UnaryFunction = T (*)(const T&);

  template <typename T>
  constexpr std::array<UnaryFunction<T>, 16> unary_function_table = {
      [](const T& arg) -> T { return acos(arg); },
      [](const T& arg) -> T { return asin(arg); },
      [](const T& arg) -> T { return atan(arg); },
      [](const T& arg) -> T { return cos(arg); },
      [](const T& arg) -> T { return sin(arg); },
      [](const T& arg) -> T { return tan(arg); },
      [](const T& arg) -> T { return cosh(arg); },
      [](const T& arg) -> T { return sinh(arg); },
      [](const T& arg) -> T { return tanh(arg); },
      [](const T& arg) -> T { return exp(arg); },
      [](const T& arg) -> T { return log(arg); },
      [](const T& arg) -> T { return log10(arg); },
      [](const T& arg) -> T { return sqrt(arg); },
      [](const T& arg) -> T { return arg > 0 ? 1.0 : 0.0; },
      [](const T& arg) -> T { return fabs(arg); },
      [](const T& arg) -> T { return arg == 0.0 ? 1.0 : 0.0; },
  };

  enum class BinaryFunctionType : std::uint8_t
  {
    add,
    sub,
    mul,
    div,
    atan2,
    pow,
    gt,
    ge,
    lt,
    le,
    eq,
    ne,
    logical_or,
    logical_and,
  };

  template <typename T>
  using BinaryFunction = T (*)(const T&, const T&);

  template <typename T>
  constexpr std::array<BinaryFunction<T>, EnumTools::enum_count<BinaryFunctionType>()>
      binary_function_table = {
          [](const T& lhs, const T& rhs) -> T { return lhs + rhs; },               // add
          [](const T& lhs, const T& rhs) -> T { return lhs - rhs; },               // sub
          [](const T& lhs, const T& rhs) -> T { return lhs * rhs; },               // mul
          [](const T& lhs, const T& rhs) -> T { return lhs / rhs; },               // div
          [](const T& lhs, const T& rhs) -> T { return atan2(lhs, rhs); },         // atan2
          [](const T& lhs, const T& rhs) -> T { return pow(lhs, rhs); },           // pow
          [](const T& lhs, const T& rhs) -> T { return lhs > rhs ? 1.0 : 0.0; },   // gt
          [](const T& lhs, const T& rhs) -> T { return lhs >= rhs ? 1.0 : 0.0; },  // ge
          [](const T& lhs, const T& rhs) -> T { return lhs < rhs ? 1.0 : 0.0; },   // lt
          [](const T& lhs, const T& rhs) -> T { return lhs <= rhs ? 1.0 : 0.0; },  // le
          [](const T& lhs, const T& rhs) -> T { return lhs == rhs ? 1.0 : 0.0; },  // eq
          [](const T& lhs, const T& rhs) -> T { return lhs != rhs ? 1.0 : 0.0; },  // ne
          [](const T& lhs, const T& rhs) -> T { return (lhs != 0.0) || (rhs != 0.0) ? 1.0 : 0.0; },
          [](const T& lhs, const T& rhs) -> T { return (lhs != 0.0) && (rhs != 0.0) ? 1.0 : 0.0; },
  };


  /**
   * The value stored in a syntax tree node or a byte code instruction.
   */
  union Value
  {
    double number{};
    UnaryFunctionType unary_function;
    BinaryFunctionType binary_function;
    IndexType var_index;  // refer to a variable stored in the parser
  };

  /**
   * \brief Syntax tree node holding binary, unary operator, variables or literals.
   */
  struct SyntaxTreeNode
  {
    /**
     * The different options that could be stored in the node.
     */
    Value as;

    //! Lhs and rhs nodes
    IndexType lhs_index{invalid_index};
    IndexType rhs_index{invalid_index};

    //! type of the node, ie operator, literal, ...
    NodeType type;
  };

  /**
   * Opcode for the bytecode representation of the symbolic expression.
   */
  enum class OpCode : unsigned char
  {
    load_const,
    load_var,
    unary_function,
    binary_function,
  };

  /**
   * Bytecode instruction.
   */
  struct Instruction
  {  // true if this is a unary function, false if it is a binary function
    bool is_unary_function;
    // index of the function in the function table
    std::uint8_t function_index;
    // First operand
    IndexType src1;
    // Second operand
    IndexType src2{};
    // Destination register index
    IndexType dst_reg;
  };


  /**
   * A lexer to tokenize the input string.
   */
  class Lexer
  {
   public:
    //! constructor
    Lexer(std::string_view funct) : funct_(funct), pos_(0) {}

    //! delivers funct_ character at position pos_++
    int get_next();

    //! Advance to the next token.
    void advance();

    //! type of identifiable tokens in string funct_
    enum TokenType
    {
      tok_none,
      tok_done,
      tok_name,   // operator name, e.g. 'sin'
      tok_int,    // integer number
      tok_real,   // reals number
      tok_add,    // addition '+'
      tok_sub,    // subtraction and negation '-'
      tok_mul,    // multiplication '*'
      tok_div,    // division '/'
      tok_pow,    // power '^'
      tok_lpar,   // left parenthesis '('
      tok_rpar,   // right parenthesis ')'
      tok_comma,  // comma ',' (used to separate function arguments)
      tok_gt,     // greater than '>'
      tok_ge,     // greater than or equal '>='
      tok_lt,     // less than '<'
      tok_le,     // less than or equal '<='
      tok_eq,     // equal '=='
      tok_ne,     // not equal '!='
      tok_and,    // logical and '&&'
      tok_or,     // logical or '||'
      tok_bang,   // unary logical not '!'
    };

    std::string_view funct_;  // function description as string, i.e. "t^2", "sin(t)", etc
    unsigned pos_;            // current position in string funct_
    TokenType tok_;           // current token of string funct_
    const char* str_;         // pointer to begin of current string token in funct_
    int integer_;             // translated integer number or length of operator word
    double real_;             // translated real number
  };


  /**
   * Parse symbolic expressions into a syntax tree.
   * This is a handwritten recursive descent parser.
   */
  template <class T>
  class Parser
  {
   public:
    using NodePtr = SyntaxTreeNode*;

    //! Parse @p expression into a syntax tree.
    Parser(std::string expression, const std::vector<std::string_view>& allowed_variable_names)
        : allowed_variable_names_(allowed_variable_names), expression_(expression)
    {
      allow_any_variable_ = allowed_variable_names_.empty();
      parse();
    }

    /**
     * Actual storage for all parsed syntax tree nodes. The first element is the root node of
     * the tree.
     */
    std::vector<SyntaxTreeNode> node_arena_;

    //! root node of the syntax tree
    IndexType root_{invalid_index};

    //! All variables that were either allowed a priori or found in the expression.
    std::vector<std::string_view> allowed_variable_names_;

   private:
    //! Entry point to parse the expression.
    void parse();

    //! Any expression.
    IndexType parse_expr(Lexer& lexer);

    //! A logical OR expression.
    IndexType parse_logical_or(Lexer& lexer);
    //! A logical AND expression.
    IndexType parse_logical_and(Lexer& lexer);
    //! An equality expression.
    IndexType parse_equality(Lexer& lexer);
    //! A comparison expression.
    IndexType parse_comparison(Lexer& lexer);
    //! Addition and subtraction.
    IndexType parse_term(Lexer& lexer);
    //! Multiplication and division.
    IndexType parse_factor(Lexer& lexer);
    //! Power operator.
    IndexType parse_pow(Lexer& lexer);
    //! Number, variable, parentheses, unary operators, function call.
    IndexType parse_primary(Lexer& lexer);

    //! Create a new node in the node arena and return a pointer to it.
    //! Optionally, a left-hand side and right-hand side node index can be provided.
    IndexType create_node(
        NodeType type, Value value, IndexType lhs = invalid_index, IndexType rhs = invalid_index);

    //! Either return the index of @p var or give it a new one.
    IndexType create_variable(std::string_view var);

    //! given symbolic expression
    std::string expression_;

    bool allow_any_variable_{false};
  };

  /**
   * Implementation of the symbolic expression.
   *
   * This class does all the heavy lifting of parsing the expression, compiling it into bytecode,
   * and evaluating it for given variables.
   */
  template <typename Number, CompileTimeString... variables>
  class Impl
  {
   public:
    //! The variables that can be used in the expression. If empty, any variable can be used.
    static constexpr std::array<std::string_view, sizeof...(variables)> variable_names = {
        variables...};

    //! Parse and compile the expression.
    Impl(std::string expression) : expression_(std::move(expression)) { init(); }

    //! Evaluate for statically known variables.
    template <typename = void>
      requires(sizeof...(variables) > 0)
    Number evaluate(VarWrapper<variables, Number>&&... vars) const
    {
      // Move the variable values into a statically known slot in the memory.
      ((vm_memory_[index_of<variables, variables...>()] = std::move(vars.value)), ...);

      execute_bytecode(vm_memory_);

      return vm_memory_[vm_result_index_];
    }

    //! Evaluate for dynamically specified variables.
    template <typename = void>
      requires(sizeof...(variables) == 0)
    Number evaluate(const std::map<std::string, Number>& vars) const
    {
      CopyDynamicVariablesToMemoryHelper var_to_mem_helper{
          .vm_memory = vm_memory_,
          .identifiers = variable_names_,
      };

      var_to_mem_helper.fill_memory(vars);

      if (var_to_mem_helper.touch_count != variable_names_.size())
      {
        std::string missing_variables;
        for (const auto& var : variable_names_)
        {
          if (!vars.contains(var))
          {
            missing_variables += var + " ";
          }
        }

        FOUR_C_THROW(
            "Missing variables {}to evaluate expression '{}'", missing_variables, expression_);
      }

      execute_bytecode(vm_memory_);
      return vm_memory_[vm_result_index_];
    }

    /**
     * Print the bytecode instructions to the given output stream.
     *
     * This is useful for debugging and understanding how the expression is compiled.
     */
    void print_instructions(std::ostream& os) const;

   private:
    struct CopyDynamicVariablesToMemoryHelper
    {
      void fill_memory(const std::map<std::string, Number>& variable_values)
      {
        for (const auto& [key, val] : variable_values)
        {
          auto it = std::ranges::find(identifiers, key);
          if (it != identifiers.end())
          {
            auto index = std::distance(identifiers.begin(), it);
            vm_memory[index] = val;
            touch_count++;
          }
        }
      }

      std::vector<Number>& vm_memory;
      const std::vector<std::string>& identifiers;
      unsigned touch_count{};
    };

    void init();

    void compile(const Parser<Number>& parser);

    void execute_bytecode(std::vector<Number>& memory) const;

    /**
     * The bytecode instructions that represent the parsed expression.
     */
    std::vector<Instruction> instructions_;

    /**
     * Temporary storage for the evaluation of the expression.
     * This is the memory that the bytecode instructions will operate on. The data consists of
     * the user variables and constants, the intermediate result registers and the numeric
     * constants. All sizes are fixed during compile().
     */
    mutable std::vector<Number> vm_memory_;
    IndexType vm_register_size_{};
    //! The index in vm_memory_ where the result of the expression is stored.
    IndexType vm_result_index_{};

    std::string expression_;

    std::vector<std::string> variable_names_;
  };



  template <class T>
  void Parser<T>::parse()
  {
    //! create Lexer which stores all token of funct
    Lexer lexer{expression_};

    //! retrieve first token of funct
    lexer.advance();

    //! create syntax tree equivalent to funct
    root_ = parse_expr(lexer);

    // check if parsing ended before processing the entire string
    if (lexer.tok_ != Lexer::tok_done)
    {
      FOUR_C_THROW("Invalid syntax: The remaining string '{}' is not parsed.",
          lexer.funct_.data() + lexer.pos_);
    }
  }

  template <class T>
  IndexType Parser<T>::parse_primary(Lexer& lexer)
  {
    IndexType lhs = invalid_index;
    switch (lexer.tok_)
    {
      case Lexer::tok_lpar:
        lexer.advance();
        lhs = parse_expr(lexer);
        if (lexer.tok_ != Lexer::tok_rpar) FOUR_C_THROW("')' expected");
        lexer.advance();
        break;
      case Lexer::tok_int:
        lhs = create_node(NodeType::number, {.number = static_cast<double>(lexer.integer_)});
        lexer.advance();
        break;
      case Lexer::tok_real:
        lhs = create_node(NodeType::number, {.number = lexer.real_});
        lexer.advance();
        break;
      case Lexer::tok_bang:
        lexer.advance();
        lhs = create_node(NodeType::unary_function,
            {.unary_function = UnaryFunctionType::logical_not}, parse_primary(lexer));
        break;
      case Lexer::tok_sub:
      {
        // This is a unary minus operator.
        lexer.advance();
        IndexType rhs = parse_pow(lexer);
        auto& rhs_node = node_arena_[rhs];
        if (rhs_node.type == NodeType::number)
        {
          rhs_node.as.number *= -1;
          lhs = rhs;
        }
        else
        {
          lhs = create_node(NodeType::number, {.number = -1});
          lhs = create_node(
              NodeType::binary_function, {.binary_function = BinaryFunctionType::mul}, lhs, rhs);
        }
        break;
      }
      case Lexer::tok_name:
      {
        // get substring starting from str_ with length of lexer.integer_
        std::string_view name(lexer.str_, lexer.integer_);
        if (name == "pi")
        {
          lhs = create_node(NodeType::number, {.number = M_PI});
          lexer.advance();
          break;
        }
        else
        {
          auto maybe_unary_function = EnumTools::enum_cast<UnaryFunctionType>(name);
          if (maybe_unary_function)
          {
            // Consume opening parenthesis
            lexer.advance();
            if (lexer.tok_ != Lexer::tok_lpar)
              FOUR_C_THROW("'(' expected after function name '{}'", name);

            lexer.advance();
            lhs = create_node(NodeType::unary_function, {.unary_function = *maybe_unary_function},
                parse_expr(lexer));

            // Consume closing parenthesis
            if (lexer.tok_ != Lexer::tok_rpar) FOUR_C_THROW("')' expected");
            lexer.advance();
            break;
          }

          auto maybe_binary_function = EnumTools::enum_cast<BinaryFunctionType>(name);
          if (maybe_binary_function)
          {
            // Consume opening parenthesis
            lexer.advance();
            if (lexer.tok_ != Lexer::tok_lpar)
              FOUR_C_THROW("'(' expected after function name '{}'", name);

            lexer.advance();
            auto first_arg = parse_expr(lexer);

            // Consume comma separating the two arguments
            if (lexer.tok_ != Lexer::tok_comma) FOUR_C_THROW("',' expected");
            lexer.advance();
            auto second_arg = parse_expr(lexer);

            lhs = create_node(NodeType::binary_function,
                {.binary_function = *maybe_binary_function}, first_arg, second_arg);

            // Consume closing parenthesis
            if (lexer.tok_ != Lexer::tok_rpar)
              FOUR_C_THROW("')' expected for function name '{}'", name);
            lexer.advance();
            break;
          }

          // Some other identifier that is treated as a variable.
          else
          {
            lhs = create_node(NodeType::variable, {.var_index = create_variable(name)});
            lexer.advance();
          }
        }
        break;
      }
      default:
        FOUR_C_THROW("unexpected token {}", lexer.tok_);
        break;
    }

    return lhs;
  }


  template <class T>
  IndexType Parser<T>::parse_pow(Lexer& lexer)
  {
    IndexType lhs;
    IndexType rhs;

    lhs = parse_primary(lexer);
    for (;;)
    {
      if (lexer.tok_ == Lexer::tok_pow)
      {
        lexer.advance();
        rhs = parse_primary(lexer);
        lhs = create_node(
            NodeType::binary_function, {.binary_function = BinaryFunctionType::pow}, lhs, rhs);
      }
      else
      {
        break;
      }
    }

    return lhs;
  }


  template <class T>
  IndexType Parser<T>::parse_factor(Lexer& lexer)
  {
    IndexType lhs;
    IndexType rhs;

    lhs = parse_pow(lexer);
    for (;;)
    {
      if (lexer.tok_ == Lexer::tok_mul)
      {
        lexer.advance();
        rhs = parse_pow(lexer);
        lhs = create_node(
            NodeType::binary_function, {.binary_function = BinaryFunctionType::mul}, lhs, rhs);
      }
      else if (lexer.tok_ == Lexer::tok_div)
      {
        lexer.advance();
        rhs = parse_pow(lexer);
        lhs = create_node(
            NodeType::binary_function, {.binary_function = BinaryFunctionType::div}, lhs, rhs);
      }
      else
      {
        break;
      }
    }

    return lhs;
  }

  template <class T>
  IndexType Parser<T>::parse_comparison(Lexer& lexer)
  {
    IndexType lhs = parse_term(lexer);
    for (;;)
    {
      if (lexer.tok_ == Lexer::tok_gt)
      {
        lexer.advance();
        IndexType rhs = parse_term(lexer);
        lhs = create_node(
            NodeType::binary_function, {.binary_function = BinaryFunctionType::gt}, lhs, rhs);
      }
      else if (lexer.tok_ == Lexer::tok_lt)
      {
        lexer.advance();
        IndexType rhs = parse_term(lexer);
        lhs = create_node(
            NodeType::binary_function, {.binary_function = BinaryFunctionType::lt}, lhs, rhs);
      }
      else if (lexer.tok_ == Lexer::tok_ge)
      {
        lexer.advance();
        IndexType rhs = parse_term(lexer);
        lhs = create_node(
            NodeType::binary_function, {.binary_function = BinaryFunctionType::ge}, lhs, rhs);
      }
      else if (lexer.tok_ == Lexer::tok_le)
      {
        lexer.advance();
        IndexType rhs = parse_term(lexer);
        lhs = create_node(
            NodeType::binary_function, {.binary_function = BinaryFunctionType::le}, lhs, rhs);
      }
      else
      {
        break;
      }
    }

    return lhs;
  }

  template <class T>
  IndexType Parser<T>::parse_equality(Lexer& lexer)
  {
    IndexType lhs = parse_comparison(lexer);
    for (;;)
    {
      if (lexer.tok_ == Lexer::tok_eq)
      {
        lexer.advance();
        IndexType rhs = parse_comparison(lexer);
        lhs = create_node(
            NodeType::binary_function, {.binary_function = BinaryFunctionType::eq}, lhs, rhs);
      }
      else if (lexer.tok_ == Lexer::tok_ne)
      {
        lexer.advance();
        IndexType rhs = parse_comparison(lexer);
        lhs = create_node(
            NodeType::binary_function, {.binary_function = BinaryFunctionType::ne}, lhs, rhs);
      }
      else
      {
        break;
      }
    }
    return lhs;
  }


  template <class T>
  IndexType Parser<T>::parse_logical_and(Lexer& lexer)
  {
    IndexType lhs = parse_equality(lexer);
    for (;;)
    {
      if (lexer.tok_ == Lexer::tok_and)
      {
        lexer.advance();
        IndexType rhs = parse_equality(lexer);
        lhs = create_node(NodeType::binary_function,
            {.binary_function = BinaryFunctionType::logical_and}, lhs, rhs);
      }
      else
      {
        break;
      }
    }
    return lhs;
  }

  template <class T>
  IndexType Parser<T>::parse_logical_or(Lexer& lexer)
  {
    IndexType lhs = parse_logical_and(lexer);
    for (;;)
    {
      if (lexer.tok_ == Lexer::tok_or)
      {
        lexer.advance();
        IndexType rhs = parse_logical_and(lexer);
        lhs = create_node(NodeType::binary_function,
            {.binary_function = BinaryFunctionType::logical_or}, lhs, rhs);
      }
      else
      {
        break;
      }
    }
    return lhs;
  }


  template <class T>
  IndexType Parser<T>::parse_expr(Lexer& lexer)
  {
    return parse_logical_or(lexer);
  }


  template <class T>
  IndexType Parser<T>::create_node(NodeType type, Value value, IndexType lhs, IndexType rhs)
  {
    FOUR_C_ASSERT_ALWAYS(node_arena_.size() < invalid_index,
        "Could not parse expression '{}'\nThis expression is too complicated and the number of AST "
        "nodes exceeds the maximum allowed size of {}.",
        expression_, invalid_index);

    SyntaxTreeNode& node = node_arena_.emplace_back();
    node.as = value;
    node.type = type;
    node.lhs_index = lhs;
    node.rhs_index = rhs;
    // Index of the node that was just created.
    return node_arena_.size() - 1;
  }

  template <class T>
  IndexType Parser<T>::create_variable(std::string_view var)
  {
    auto it = std::ranges::find(allowed_variable_names_, var);
    // Variable is already known and allowed.
    if (it != allowed_variable_names_.end())
    {
      return std::distance(allowed_variable_names_.begin(), it);
    }
    else
    {
      if (allow_any_variable_)
      {
        // If we allow any variable, we just add it to the list of parsed variables.
        allowed_variable_names_.emplace_back(var);
        return allowed_variable_names_.size() - 1;
      }
      else
      {
        FOUR_C_THROW("While parsing '{}': variable '{}' is not allowed.", expression_, var);
      }
    }
  }

  template <class T>
  IndexType Parser<T>::parse_term(Lexer& lexer)
  {
    IndexType lhs = parse_factor(lexer);
    for (;;)
    {
      if (lexer.tok_ == Lexer::tok_add)
      {
        lexer.advance();
        IndexType rhs = parse_factor(lexer);
        lhs = create_node(
            NodeType::binary_function, {.binary_function = BinaryFunctionType::add}, lhs, rhs);
      }
      else if (lexer.tok_ == Lexer::tok_sub)
      {
        lexer.advance();
        IndexType rhs = parse_factor(lexer);
        lhs = create_node(
            NodeType::binary_function, {.binary_function = BinaryFunctionType::sub}, lhs, rhs);
      }
      else
      {
        break;
      }
    }

    return lhs;
  }


  template <typename T, CompileTimeString... variables>
  void Impl<T, variables...>::init()
  {
    // Use the variable names provided in the template parameters to initialize the parser.
    // If no variable names are provided, we allow any variable name.
    std::vector<std::string_view> allowed_variable_names(
        variable_names.begin(), variable_names.end());
    Parser<T> parser{expression_, allowed_variable_names};
    compile(parser);
  }


  template <class T, CompileTimeString... variables>
  void Impl<T, variables...>::compile(const Parser<T>& parser)
  {
    for (const auto& name : parser.allowed_variable_names_) variable_names_.emplace_back(name);

    // Order the data we operate on in one contiguous memory block.
    // First, we store the variables, then the registers, and finally the constants.
    // We know the number of variables from the parser, but we do not know the number of
    // constants and registers that will be used. These may change due to optimization.
    // While compiling, we use a large offset to distinguish between registers and constants and
    // fixup these cases in a second pass.
    const IndexType registers_offset = variable_names_.size();
    const IndexType tmp_constants_offset = std::numeric_limits<IndexType>::max() >> 1;

    // Track the current register index and remember the maximum index used.
    // This is later used to determine the number of registers in the VM memory.
    struct RegisterCount
    {
      IndexType current{};
      IndexType max{};

      IndexType& operator--()
      {
        FOUR_C_ASSERT(current > 0, "Internal error: Register stack underflow.");
        return --current;
      }

      IndexType& operator++()
      {
        ++current;
        max = std::max(current, max);
        return current;
      }

      operator const IndexType&() const { return current; }
    } register_count;

    std::vector<T> constants;

    struct StackEntry
    {
      enum class Type : std::uint8_t
      {
        var_index,
        reg_index,
        number,
      } type;

      IndexType index;
      // Only used for number.
      T value{};
    };
    std::vector<StackEntry> stack_simulator;


    // For compilation, we need a post-order traversal of the syntax tree. Since this order is also
    // exactly the order in which the AST was built, we can just take the nodes in the order they
    // are stored in the node_arena_ vector.
    for (const auto& node : parser.node_arena_)
    {
      switch (node.type)
      {
        case NodeType::number:
        {
          IndexType index = tmp_constants_offset + constants.size();
          stack_simulator.emplace_back(StackEntry{
              .type = StackEntry::Type::number,
              .index = index,
              .value = node.as.number,
          });
          constants.push_back(node.as.number);
          break;
        }
        case NodeType::variable:
        {
          stack_simulator.emplace_back(StackEntry{
              .type = StackEntry::Type::var_index,
              .index = node.as.var_index,
          });
          break;
        }
        case NodeType::unary_function:
        {
          FOUR_C_ASSERT(stack_simulator.size() >= 1,
              "Internal error: A valid expression should contain at least one value on the stack "
              "for a unary function.");

          const auto& operand = stack_simulator.back();
          stack_simulator.pop_back();

          const auto function_index =
              static_cast<std::underlying_type_t<UnaryFunctionType>>(node.as.unary_function);
          switch (operand.type)
          {
            // Apply the function to the value. No instruction is generated for this case.
            case StackEntry::Type::number:
            {
              T value = unary_function_table<T>[function_index](operand.value);
              constants.back() = value;
              stack_simulator.emplace_back(StackEntry{
                  .type = StackEntry::Type::number,
                  .index = operand.index,
                  .value = value,
              });
              break;
            }
            case StackEntry::Type::reg_index:
              --register_count;
              [[fallthrough]];
            case StackEntry::Type::var_index:
            {
              IndexType dst_reg = registers_offset + register_count;
              instructions_.emplace_back(Instruction{
                  .is_unary_function = true,
                  .function_index = function_index,
                  .src1 = operand.index,
                  .dst_reg = dst_reg,
              });
              stack_simulator.emplace_back(StackEntry{
                  .type = StackEntry::Type::reg_index,
                  .index = dst_reg,
              });
              ++register_count;
              break;
            }
              std23::unreachable();
          }
          break;
        }
        case NodeType::binary_function:
        {
          const auto& rhs_operand = stack_simulator.back();
          stack_simulator.pop_back();
          const auto& lhs_operand = stack_simulator.back();
          stack_simulator.pop_back();

          const auto function_index =
              static_cast<std::underlying_type_t<BinaryFunctionType>>(node.as.binary_function);

          // Check for constant folding. If both operands are constants, we can compute the result
          // directly and do not need to generate an instruction.
          if (lhs_operand.type == StackEntry::Type::number &&
              rhs_operand.type == StackEntry::Type::number)
          {
            T value =
                binary_function_table<T>[function_index](lhs_operand.value, rhs_operand.value);
            // Pop off one value and overwrite the other one.
            constants.pop_back();
            constants.back() = value;

            IndexType index = tmp_constants_offset + constants.size() - 1;
            stack_simulator.emplace_back(StackEntry{
                .type = StackEntry::Type::number,
                .index = index,
                .value = value,
            });
            break;
          }

          if (lhs_operand.type == StackEntry::Type::reg_index) --register_count;
          if (rhs_operand.type == StackEntry::Type::reg_index) --register_count;

          // Otherwise, generate an instruction for the binary function.
          IndexType dst_reg = registers_offset + register_count;
          instructions_.emplace_back(Instruction{
              .is_unary_function = false,
              .function_index = function_index,
              .src1 = lhs_operand.index,
              .src2 = rhs_operand.index,
              .dst_reg = dst_reg,
          });
          stack_simulator.emplace_back(StackEntry{
              .type = StackEntry::Type::reg_index,
              .index = dst_reg,
          });

          ++register_count;

          break;
        }
      }
    }

    // The result will always be the single value left on the stack.
    FOUR_C_ASSERT((stack_simulator.size() == 1),
        "Internal error: while compiling '{}'.\nStack contains {} values and {} instructions "
        "were compiled.",
        expression_, stack_simulator.size(), instructions_.size());


    // Now we can compute the correct offsets for the constants.
    IndexType actual_constants_offset = registers_offset + register_count.max;
    for (auto& instr : instructions_)
    {
      if (instr.src1 >= tmp_constants_offset)
      {
        instr.src1 = actual_constants_offset + (instr.src1 - tmp_constants_offset);
      }
      if (instr.src2 >= tmp_constants_offset)
      {
        instr.src2 = actual_constants_offset + (instr.src2 - tmp_constants_offset);
      }
    }

    // Finally, we can resize the data vector to hold all variables, registers and constants.
    vm_memory_.resize(actual_constants_offset + constants.size());
    // Copy the constants into the data vector.
    std::copy(constants.begin(), constants.end(), vm_memory_.begin() + actual_constants_offset);
    vm_register_size_ = register_count.max;
    // Special case: only loading a variable means we have no registers and the result is in the
    // first memory slot (corresponding to the first variable).
    vm_result_index_ = (vm_memory_.size() == 1) ? 0 : registers_offset;
  }


  template <class T, CompileTimeString... variables>
  void Impl<T, variables...>::execute_bytecode(std::vector<T>& vm_memory) const
  {
    for (const auto& instr : instructions_)
    {
      const auto& lhs = vm_memory[instr.src1];
      auto& dst = vm_memory[instr.dst_reg];
      if (instr.is_unary_function)
      {
        dst = unary_function_table<T>[instr.function_index](lhs);
      }
      else
      {
        auto binary_function = static_cast<BinaryFunctionType>(instr.function_index);
        const auto& rhs = vm_memory[instr.src2];
        switch (binary_function)
        {
          case BinaryFunctionType::add:
            dst = lhs + rhs;
            break;
          case BinaryFunctionType::sub:
            dst = lhs - rhs;
            break;
          case BinaryFunctionType::mul:
            dst = lhs * rhs;
            break;
          case BinaryFunctionType::div:
            dst = lhs / rhs;
            break;
          default:
            dst = binary_function_table<T>[instr.function_index](lhs, rhs);
            break;
        }
      }
    }
  }


  template <class T, CompileTimeString... variables>
  void Impl<T, variables...>::print_instructions(std::ostream& os) const
  {
    const auto operand = [&](IndexType index) -> std::string
    {
      if (index < variable_names_.size())
      {
        return std::string(variable_names_[index]);
      }

      IndexType offset = variable_names_.size();
      if (index < offset + vm_register_size_)
      {
        return "%" + std::to_string(index - offset);
      }

      if (index < vm_memory_.size())
      {
        if constexpr (FADUtils::SacadoFadType<T>)
        {
          if constexpr (FADUtils::SacadoFadType<typename T::value_type>)
            return std::to_string(vm_memory_[index].val().val());
          else
            return std::to_string(vm_memory_[index].val());
        }
        else
          return std::to_string(vm_memory_[index]);
      }

      return "(invalid index)";
    };

    for (const auto& instr : instructions_)
    {
      if (instr.is_unary_function)
      {
        os << EnumTools::enum_name(EnumTools::enum_value<UnaryFunctionType>(instr.function_index))
           << " " << operand(instr.src1) << ", "
           << "%" << (instr.dst_reg - variable_names_.size());
      }
      else
      {
        os << EnumTools::enum_name(EnumTools::enum_value<BinaryFunctionType>(instr.function_index))
           << " " << operand(instr.src1) << ", " << operand(instr.src2) << ", " << "%"
           << (instr.dst_reg - variable_names_.size());
      }
      os << '\n';
    }
    os << std::flush;
  }

}  // namespace Core::Utils::SymbolicExpressionDetails

FOUR_C_NAMESPACE_CLOSE

#endif