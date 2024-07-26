#ifndef H_GIMPLE_EXTRACTOR
#define H_GIMPLE_EXTRACTOR


#include <iostream>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>

#include "gcc-plugin.h"

#include "tree-pass.h"
#include "context.h"
#include "function.h"
#include "tree.h"
#include "tree-ssa-alias.h"
#include "internal-fn.h"
#include "is-a.h"
#include "predict.h"
#include "basic-block.h"
#include "gimple-expr.h"
#include "gimple.h"
#include "gimple-pretty-print.h"
#include "gimple-iterator.h"
#include "gimple-walk.h"
#include "ssa.h"
#include "tree-iterator.h"
#include "langhooks.h"
#include "trans-mem.h"
#include "cgraph.h"
#include "attribs.h"
#include "asan.h"
#include "tree-dfa.h"

#if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)

// Check for GCC 12.0.0 or later
#if (__GNUC__ > 12) || (__GNUC__ == 12 && __GNUC_MINOR__ >= 0)
    #define GCC_VERSION_12_OR_LATER
#endif

#if (__GNUC__ > 13) || (__GNUC__ == 13 && __GNUC_MINOR__ >= 0)
    #define GCC_VERSION_13_OR_LATER
#endif

#endif

typedef struct _data_value data_value_t;
typedef struct _gimple_stmt_data gimple_stmt_data;

typedef struct _tree_value
{
    std::vector<data_value_t> values;
} tree_value_t;

typedef struct fn_arg_variable
{
    // if var_def desn't hold data, check arg
    tree_value_t arg;
    tree_value_t var_def;
    tree_value_t var_ssa_name_var;

    // if var_type doesn't hold data, check var_declaration
    tree_value_t var_type;
    tree_value_t var_declaration;
} fn_arg_variable_t;

typedef struct fn_local_variable
{
    tree_value_t arg;
    tree_value_t var_declaration;
} fn_local_variable_t;

typedef struct fn_ssa_variable
{
    tree_value_t arg;
    tree_value_t var_type;
} fn_ssa_variable_t;

typedef struct _function_data
{
    std::string fn_name;
    std::string fn_filename;
    int fn_start_line_no = -1;
    int fn_end_line_no = -1;
    std::map<std::string, std::string> fn_source_lines;
    tree_value_t fn_decl;
    std::vector<fn_arg_variable_t> fn_args;
    std::vector<fn_local_variable_t> fn_local_variables;
    std::vector<fn_ssa_variable_t> fn_ssa_variables;
    std::vector<tree_value_t> fn_ssa_names;
} function_data_t;

typedef struct _data_value
{
    std::string value_type = "simple"; // simple|complex

    enum tree_code code;
    std::string code_class;
    std::string code_name;

    bool is_expr = false;
    unsigned int operand_length = 0;
    bool has_inner_tree = false;

    std::string location_file = "";
    int location_line = 0;
    int location_column = 0;

    std::string simple_data_value;
    std::vector<data_value_t> complex_data_values;
} data_value_t;

typedef struct _gimple_phi_rhs
{
    tree_value_t phi_rhs;
    int line = -1;
    int column = -1;
    int basic_block_src_index = -1;
} gimple_phi_rhs_t;

typedef struct _gimple_phi
{
    tree_value_t phi_lhs;
    std::vector<gimple_phi_rhs_t> gimple_phi_rhs_list;
} gimple_phi_t;

typedef struct _basicblock
{
    int bb_index = -1;
    std::vector<int> bb_edges;
    std::vector<gimple_phi_t> phis;
} basicblock_t;

typedef struct _gimple_stmt_data
{
    // std::string              function_name;

    std::string gimple_stmt_code_str;
    enum gimple_code gimple_stmt_code;

    std::string gimple_stmt_expr_code_str;
    enum tree_code gimple_stmt_expr_code;

    std::string filename;
    int lineno = 0;

    std::string has_substatements;

    std::string has_register_or_memory_operands;
    std::string has_memory_operands;
    unsigned int gimple_num_ops = 0;

    int basic_block_index = 0;
    std::vector<int> basic_block_edges;

    std::vector<tree_value_t> tree_values;

    tree_value_t vdef_value;
    tree_value_t vuse_value;

    /**********************************************
     * GIMPLE_ASM
     *
     * STRING is the assembly code.
     * INPUTS, OUTPUTS, CLOBBERS and LABELS are the inputs, outputs, clobbered
     * registers and labels.
     *
     * *******************************************/
    std::string gasm_string_code;
    std::vector<tree_value_t> gasm_input_operands;
    std::vector<tree_value_t> gasm_output_operands;
    std::vector<tree_value_t> gasm_clobber_operands;
    std::vector<tree_value_t> gasm_labels;

    bool gasm_volatile = false;
    bool gasm_inline = false;

    /**********************************************
     * GIMPLE_ASSIGN
     *
     * The left-hand side is an lvalue passed in lhs
     * The right-hand side can be either a unary or binary tree expression
     * Subcode is the tree_code for the right-hand side of the assignment.
     * Op1, op2 and op3 are the operands.
     *
     * "%G <%s, %T, %T, %T, %T>", gs,
     * get_tree_code_name(gimple_assign_rhs_code(gs)), gimple_assign_lhs(gs),
     * arg1, arg2, arg3
     *
     * *******************************************/
    std::string gassign_subcode;

    bool gassign_has_rhs_arg1 = false;
    bool gassign_has_rhs_arg2 = false;
    bool gassign_has_rhs_arg3 = false;

    tree_value_t gassign_lhs_arg;
    tree_value_t gassign_rhs_arg1;
    tree_value_t gassign_rhs_arg2;
    tree_value_t gassign_rhs_arg3;

    /**********************************************
     * GIMPLE_BIND
     *
     * The left-hand side is an lvalue passed in lhs
     * The right-hand side can be either a unary or binary tree expression
     * Subcode is the tree_code for the right-hand side of the assignment.
     * Op1, op2 and op3 are the operands.
     *
     * "%G <", gs
     *
     * *******************************************/
    std::vector<tree_value_t> gbind_bind_vars;

    std::vector<gimple_stmt_data> gbind_bind_body;

    /**********************************************
     * GIMPLE_CALL
     *
     * Build a GIMPLE_CALL statement to function FN.
     * The argument FN must be either a FUNCTION_DECL or a gimple call address
     * as determined by is_gimple_call_addr. NARGS are the number of arguments.
     * The rest of the arguments follow the argument NARGS, and must be trees
     * that are valid as rvalues in gimple (i.e., each operand is validated with
     * is_gimple_operand).
     *
     *
     *
     * *******************************************/
    bool gcall_isinternal_only_function = false;
    std::string gcall_internal_function_name;

    int gcall_call_num_of_args = 0;

    bool gcall_has_lhs = false;
    tree_value_t gcall_lhs_arg;

    bool gcall_is_tm_clone = false;
    std::vector<std::string> gcall_transaction_code_properties;
    bool gcall_is_marked_for_return_slot_optimization = false;
    bool gcall_is_marked_as_a_tail_call = false;
    bool gcall_is_marked_as_requiring_tail_call_optimization = false;

    bool gcall_has_static_chain_for_call_statement = false;
    tree_value_t gcall_static_chain_for_call_statement;

    tree_value_t gcall_fn;
    std::vector<tree_value_t> gcall_args;

    /**********************************************
     * GIMPLE_COND
     *
     * A GIMPLE_COND statement compares LHS and RHS and if the condition in
     * PRED_CODE is true, jump to the label in t_label, otherwise jump to the
     * label in f_label. PRED_CODE are relational operator tree codes like
     * EQ_EXPR, LT_EXPR, LE_EXPR, NE_EXPR, etc.
     *
     *
     * *******************************************/
    bool gcond_has_true_goto_label = false;
    bool gcond_has_false_else_goto_label = false;

    tree_value_t gcond_rhs;
    tree_value_t gcond_lhs;
    tree_value_t gcond_true_goto_label;
    tree_value_t gcond_false_else_goto_label;
    std::string gcond_tree_code_name;

    bool gcond_has_goto_true_edge = false;
    bool gcond_has_else_goto_false_edge = false;

    int goto_true_edge = 0;
    int else_goto_false_edge = 0;

    /**********************************************
     * GIMPLE_LABEL
     *
     * *******************************************/
    tree_value_t glabel_label;
    bool glabel_is_non_local = false;

    /**********************************************
     * GIMPLE_GOTO
     * GIMPLE_GOTO statement to label destination (DEST) of the unconditional
     * jump G
     *
     * *******************************************/
    tree_value_t ggoto_dest_goto_label;

    /**********************************************
     * GIMPLE_NOP
     *
     * *******************************************/
    std::string gnop_nop_str;

    /**********************************************
     * GIMPLE_RETURN
     *
     * *******************************************/
    bool greturn_has_greturn_return_value = false;
    tree_value_t greturn_return_value;

    /**********************************************
     * GIMPLE_SWITCH
     * INDEX is the index variable to switch on, and DEFAULT_LABEL represents
     * the default label ARGS is a vector of CASE_LABEL_EXPR trees that contain
     * the non-default case labels. Each label is a tree of code CASE_
     * LABEL_EXPR.
     *
     * *******************************************/
    tree_value_t gswitch_switch_index;
    std::vector<tree_value_t> gswitch_switch_case_labels;
    std::vector<tree_value_t> gswitch_switch_labels;

    /**********************************************
     * GIMPLE_TRY
     * KIND is the enumeration value GIMPLE_TRY_CATCH if this statement denotes
     * a try/catch construct or GIMPLE_ TRY_FINALLY if this statement denotes a
     * try/finally construct. EVAL is a sequence with the expression to
     * evaluate. CLEANUP is a sequence of statements to run at clean-up time.
     *
     * *******************************************/
    std::string gtry_try_type_kind = "UNKNOWN_GIMPLE_TRY";

    bool gtry_has_try_cleanup = false;

    std::vector<gimple_stmt_data> gtry_try_cleanup;
    std::vector<gimple_stmt_data> gtry_try_eval;

    /**********************************************
     * GIMPLE_PHI
     *
     * *******************************************/
    tree_value_t gphi_lhs;

    std::vector<tree_value_t> gphi_phi_args;
    std::vector<int> gphi_phi_args_basicblock_src_index;
    std::vector<std::string> gphi_phi_args_locations;

} gimple_stmt_data;

std::vector<int> getRangeVector(int start, int end);
std::vector<std::string> readFileToVector(const std::string& filename);

void write_function_to_file(std::string filename, std::string function_name, std::string function_extract_dump);

gimple_stmt_data gimple_tuple_to_stmt_data(gimple *g, int bb_index, std::vector<int> &bb_edges);
const std::string bool_cast(const bool b);
void gimple_tuple_args(gimple *g, gimple_stmt_data &stmt_data);
void get_tree_data_values(tree node, std::vector<data_value_t> &dvalues);
void append_simple_value(std::vector<data_value_t> &dvalues, data_value_t &dvalue, std::string value);
void append_complex_value(std::vector<data_value_t> &dvalues, data_value_t &dvalue, std::vector<data_value_t> &complex_dvalues);
void get_basic_tree_node_info(tree tree_node, data_value_t &dvalue);
void ppp_tree_identifier(std::vector<data_value_t> &dvalues, data_value_t &dvalue, tree id);
void get_gimple_mem_ops (const gimple *gs, gimple_stmt_data &stmt_data);
void gimple_tuple_arg_values(gimple *g, gimple_stmt_data &stmt_data);
void print_declaration(tree t, std::vector<data_value_t> &dvalues, data_value_t &dvalue);

gimple_phi_t dump_gimple_phi(const gphi *phi, basicblock_t &bb_data);
void dump_phi_nodes(basic_block bb, basicblock_t &bb_data);

void dump_gimple_asm(const gasm *gs, gimple_stmt_data &stmt_data);
void dump_gimple_assign(const gassign *gs, gimple_stmt_data &stmt_data);
void dump_gimple_bind(const gbind *gs, gimple_stmt_data &stmt_data);
void dump_gimple_call(const gcall *gs, gimple_stmt_data &stmt_data);
void dump_gimple_cond(const gcond *gs, gimple_stmt_data &stmt_data);
void dump_gimple_label(const glabel *gs, gimple_stmt_data &stmt_data);
void dump_gimple_goto(const ggoto *gs, gimple_stmt_data &stmt_data);
void dump_gimple_return(const greturn *gs, gimple_stmt_data &stmt_data);
void dump_gimple_switch(const gswitch *gs, gimple_stmt_data &stmt_data);
void dump_gimple_try(const gtry *gs, gimple_stmt_data &stmt_data);
void dump_gimple_phi(const gphi *phi, gimple_stmt_data &stmt_data);


#define ppp_string(dvalues, dvalue, str) 	    append_simple_value(dvalues, dvalue, str);
#define ppp_decimal_int(dvalues, dvalue, val) append_simple_value(dvalues, dvalue, std::to_string(val));
#define ppp_space(dvalues, dvalue)            append_simple_value(dvalues, dvalue, " ");
#define ppp_left_paren(dvalues, dvalue)       append_simple_value(dvalues, dvalue, "(");
#define ppp_right_paren(dvalues, dvalue)      append_simple_value(dvalues, dvalue, ")");
#define ppp_left_bracket(dvalues, dvalue)     append_simple_value(dvalues, dvalue, "[");
#define ppp_right_bracket(dvalues, dvalue)    append_simple_value(dvalues, dvalue, "]");
#define ppp_left_brace(dvalues, dvalue)       append_simple_value(dvalues, dvalue, "{");
#define ppp_right_brace(dvalues, dvalue)      append_simple_value(dvalues, dvalue, "}");
#define ppp_semicolon(dvalues, dvalue)        append_simple_value(dvalues, dvalue, ";");
#define ppp_comma(dvalues, dvalue)            append_simple_value(dvalues, dvalue, ",");
#define ppp_dot(dvalues, dvalue)              append_simple_value(dvalues, dvalue, ".");
#define ppp_colon(dvalues, dvalue)            append_simple_value(dvalues, dvalue, ":");
#define ppp_colon_colon(dvalues, dvalue)      append_simple_value(dvalues, dvalue, "::");
#define ppp_arrow(dvalues, dvalue)            append_simple_value(dvalues, dvalue, "->");
#define ppp_equal(dvalues, dvalue)            append_simple_value(dvalues, dvalue, "=");
#define ppp_question(dvalues, dvalue)         append_simple_value(dvalues, dvalue, "?");
#define ppp_bar(dvalues, dvalue)              append_simple_value(dvalues, dvalue, "|");
#define ppp_bar_bar(dvalues, dvalue)          append_simple_value(dvalues, dvalue, "||");
#define ppp_carret(dvalues, dvalue)           append_simple_value(dvalues, dvalue, "^");
#define ppp_ampersand(dvalues, dvalue)        append_simple_value(dvalues, dvalue, "&");
#define ppp_ampersand_ampersand(dvalues, dvalue) append_simple_value(dvalues, dvalue, "&&");
#define ppp_less(dvalues, dvalue)             append_simple_value(dvalues, dvalue, "<");
#define ppp_less_equal(dvalues, dvalue)       append_simple_value(dvalues, dvalue, "<=");
#define ppp_greater(dvalues, dvalue)          append_simple_value(dvalues, dvalue, ">");
#define ppp_greater_equal(dvalues, dvalue)    append_simple_value(dvalues, dvalue, ">=");
#define ppp_plus(dvalues, dvalue)             append_simple_value(dvalues, dvalue, "+");
#define ppp_minus(dvalues, dvalue)            append_simple_value(dvalues, dvalue, "-");
#define ppp_star(dvalues, dvalue)             append_simple_value(dvalues, dvalue, "*");
#define ppp_slash(dvalues, dvalue)            append_simple_value(dvalues, dvalue, "/");
#define ppp_modulo(dvalues, dvalue)           append_simple_value(dvalues, dvalue, "%");
#define ppp_exclamation(dvalues, dvalue)      append_simple_value(dvalues, dvalue, "!");
#define ppp_complement(dvalues, dvalue)       append_simple_value(dvalues, dvalue, "~");
#define ppp_quote(dvalues, dvalue)            append_simple_value(dvalues, dvalue, "\'");
#define ppp_backquote(dvalues, dvalue)        append_simple_value(dvalues, dvalue, "`");
#define ppp_doublequote(dvalues, dvalue)      append_simple_value(dvalues, dvalue, "\"");
#define ppp_underscore(dvalues, dvalue)       append_simple_value(dvalues, dvalue, "_");


#define DECL_IS_UNDECLARED_BUILTIN(DECL) \
  (DECL_SOURCE_LOCATION (DECL) <= BUILTINS_LOCATION)

#define TREE_CLOBBER_P(NODE) \
  (TREE_CODE (NODE) == CONSTRUCTOR && TREE_THIS_VOLATILE (NODE))

/* Return the clobber_kind of a CLOBBER CONSTRUCTOR.  */
#define CLOBBER_KIND(NODE) \
  (CONSTRUCTOR_CHECK (NODE)->base.u.bits.address_space)

#ifdef GCC_VERSION_12_OR_LATER
  enum clobber_kind;
#else
/* The kind of a TREE_CLOBBER_P CONSTRUCTOR node.  */
enum clobber_kind {
  /* Unspecified, this clobber acts as a store of an undefined value.  */
  CLOBBER_UNDEF,
  /* This clobber ends the lifetime of the storage.  */
  CLOBBER_EOL,
  CLOBBER_LAST
};
#endif


#define INDENT(dvalues, dvalue, SPACE) do { \
  int i; for (i = 0; i<SPACE; i++) ppp_space (dvalues, dvalue); } while (0)

#endif