#include "data_formatter_json.h"
#include "ext/json11.hpp"
using namespace json11;

Json
data_value_to_json_object (data_value_t &dvalue)
{
    Json stmt_data_json;

    if (dvalue.value_type == "simple")
        {
            stmt_data_json = Json::object{
                { "value_type", dvalue.value_type },
                { "code_class", dvalue.code_class },
                { "code_name", dvalue.code_name },
                { "is_expr", dvalue.is_expr },
                { "operand_length", int (dvalue.operand_length) },
                // { "has_inner_tree",  dvalue.has_inner_tree },
                // { "location_file",   dvalue.location_file },
                // { "location_line",   dvalue.location_line },
                // { "location_column", dvalue.location_column },
                { "value", dvalue.simple_data_value },
            };
        }

    else if (dvalue.value_type == "complex")
        {
            std::vector<Json> complex_dvalues;

            for (auto complex_data_value : dvalue.complex_data_values)
                {
                    complex_dvalues.push_back (
                        data_value_to_json_object (complex_data_value));
                }

            stmt_data_json = Json::object{
                { "value_type", dvalue.value_type },
                { "code_class", dvalue.code_class },
                { "code_name", dvalue.code_name },
                { "is_expr", dvalue.is_expr },
                { "operand_length", int (dvalue.operand_length) },
                // { "has_inner_tree",  dvalue.has_inner_tree },
                // { "location_file",   dvalue.location_file },
                // { "location_line",   dvalue.location_line },
                // { "location_column", dvalue.location_column },
                { "value", complex_dvalues },
            };
        }
    else
        {
            stmt_data_json = Json::object{};
        }

    return stmt_data_json;
}

Json
tree_value_to_json_object (tree_value_t &tvalue)
{
    std::vector<Json> dvalues;

    for (auto dvalue : tvalue.values)
        {
            dvalues.push_back (data_value_to_json_object (dvalue));
        }

    return Json::object{ { "values", dvalues } };
}

Json
tree_values_to_json_object (std::vector<tree_value_t> &tvalues)
{
    std::vector<Json> tvalues_json;

    for (auto tvalue : tvalues)
        {
            tvalues_json.push_back (tree_value_to_json_object (tvalue));
        }

    return tvalues_json;
}

Json
stmt_data_to_json_object (gimple_stmt_data &stmt_data)
{
    Json stmt_data_json = Json::object{
        // { "function_name",                   stmt_data.function_name },
        // { "filename",                        stmt_data.filename },
        { "gimple_code", stmt_data.gimple_stmt_code_str },
        { "gimple_expr_code", stmt_data.gimple_stmt_expr_code_str },
        { "lineno", stmt_data.lineno },
        { "has_substatements", stmt_data.has_substatements },
        { "has_register_or_memory_operands",
          stmt_data.has_register_or_memory_operands },
        { "has_memory_operands", stmt_data.has_memory_operands },
        { "gimple_num_ops", int (stmt_data.gimple_num_ops) },
        { "basic_block_index", stmt_data.basic_block_index },
        { "basic_block_edges", stmt_data.basic_block_edges },

        // { "vdef_value", tree_value_to_json_object(stmt_data.vdef_value) },
        // { "vuse_value", tree_value_to_json_object(stmt_data.vuse_value) },

        { "args", get_stmt_data_args_json (stmt_data) },
    };

    return stmt_data_json;
}

Json
stmts_data_to_json_object (std::vector<gimple_stmt_data> &stmts_data)
{
    std::vector<Json> stmt_data_lst;
    for (auto &stmt_data : stmts_data)
        {
            stmt_data_lst.push_back (stmt_data_to_json_object (stmt_data));
        }
    return stmt_data_lst;
}

Json
get_stmt_data_args_json (gimple_stmt_data &stmt_data)
{
    Json data_args;

    switch (stmt_data.gimple_stmt_code)
        {
        case GIMPLE_ASM:
            {
                data_args = Json::object{
                    { "gasm_string_code", stmt_data.gasm_string_code },
                    { "gasm_input_operands",
                      tree_values_to_json_object (
                          stmt_data.gasm_input_operands) },
                    { "gasm_output_operands",
                      tree_values_to_json_object (
                          stmt_data.gasm_output_operands) },
                    { "gasm_clobber_operands",
                      tree_values_to_json_object (
                          stmt_data.gasm_clobber_operands) },
                    { "gasm_labels",
                      tree_values_to_json_object (stmt_data.gasm_labels) },
                    { "gasm_volatile", stmt_data.gasm_volatile },
                    { "gasm_inline", stmt_data.gasm_inline },
                };
                break;
            }

        case GIMPLE_ASSIGN:
            {
                Json gassign_rhs_arg1 = Json::object{};
                Json gassign_rhs_arg2 = Json::object{};
                Json gassign_rhs_arg3 = Json::object{};

                if (stmt_data.gassign_has_rhs_arg1)
                    gassign_rhs_arg1 = tree_value_to_json_object (
                        stmt_data.gassign_rhs_arg1);

                if (stmt_data.gassign_has_rhs_arg2)
                    gassign_rhs_arg2 = tree_value_to_json_object (
                        stmt_data.gassign_rhs_arg2);

                if (stmt_data.gassign_has_rhs_arg3)
                    gassign_rhs_arg3 = tree_value_to_json_object (
                        stmt_data.gassign_rhs_arg3);

                data_args = Json::object{
                    { "gassign_subcode", stmt_data.gassign_subcode },
                    { "gassign_lhs_arg",
                      tree_value_to_json_object (stmt_data.gassign_lhs_arg) },

                    { "gassign_has_rhs_arg1", stmt_data.gassign_has_rhs_arg1 },
                    { "gassign_has_rhs_arg2", stmt_data.gassign_has_rhs_arg2 },
                    { "gassign_has_rhs_arg3", stmt_data.gassign_has_rhs_arg3 },

                    { "gassign_rhs_arg1", gassign_rhs_arg1 },
                    { "gassign_rhs_arg2", gassign_rhs_arg2 },
                    { "gassign_rhs_arg3", gassign_rhs_arg3 },
                };
                break;
            }

        case GIMPLE_BIND:
            {
                data_args = Json::object{
                    { "gbind_bind_vars",
                      tree_values_to_json_object (stmt_data.gbind_bind_vars) },
                    // { "stmt_data_to_json_object",
                    // tree_values_to_json_object(stmt_data.stmt_data_to_json_object)
                    // },
                };
                break;
            }

        case GIMPLE_CALL:
            {
                Json gcall_lhs_arg = Json::object{};
                Json gcall_static_chain_for_call_statement = Json::object{};

                if (stmt_data.gcall_has_lhs)
                    gcall_lhs_arg
                        = tree_value_to_json_object (stmt_data.gcall_lhs_arg);

                if (stmt_data.gcall_has_static_chain_for_call_statement)
                    gcall_static_chain_for_call_statement
                        = tree_value_to_json_object (
                            stmt_data.gcall_static_chain_for_call_statement);

                data_args = Json::object{
                    { "gcall_isinternal_only_function",
                      stmt_data.gcall_isinternal_only_function },
                    { "gcall_internal_function_name",
                      stmt_data.gcall_internal_function_name },

                    { "gcall_call_num_of_args",
                      stmt_data.gcall_call_num_of_args },
                    { "gcall_has_lhs", stmt_data.gcall_has_lhs },
                    { "gcall_lhs_arg", gcall_lhs_arg },

                    { "gcall_is_tm_clone", stmt_data.gcall_is_tm_clone },
                    { "gcall_transaction_code_properties",
                      stmt_data.gcall_transaction_code_properties },
                    { "gcall_is_marked_for_return_slot_optimization",
                      stmt_data.gcall_is_marked_for_return_slot_optimization },
                    { "gcall_is_marked_as_a_tail_call",
                      stmt_data.gcall_is_marked_as_a_tail_call },
                    { "gcall_is_marked_as_requiring_tail_call_optimization",
                      stmt_data
                          .gcall_is_marked_as_requiring_tail_call_optimization },
                    { "gcall_has_static_chain_for_call_statement",
                      stmt_data.gcall_has_static_chain_for_call_statement },
                    { "gcall_static_chain_for_call_statement",
                      gcall_static_chain_for_call_statement },

                    { "gcall_fn",
                      tree_value_to_json_object (stmt_data.gcall_fn) },
                    { "gcall_args",
                      tree_values_to_json_object (stmt_data.gcall_args) },
                };
                break;
            }

        case GIMPLE_COND:
            {
                Json gcond_true_goto_label = Json::object{};
                Json gcond_false_else_goto_label = Json::object{};

                int goto_true_edge = -1;
                int else_goto_false_edge = -1;

                if (stmt_data.gcond_has_true_goto_label)
                    gcond_true_goto_label = tree_value_to_json_object (
                        stmt_data.gcond_true_goto_label);

                if (stmt_data.gcond_has_false_else_goto_label)
                    gcond_false_else_goto_label = tree_value_to_json_object (
                        stmt_data.gcond_false_else_goto_label);

                if (stmt_data.gcond_has_goto_true_edge)
                    goto_true_edge = stmt_data.goto_true_edge;

                if (stmt_data.gcond_has_else_goto_false_edge)
                    else_goto_false_edge = stmt_data.else_goto_false_edge;

                data_args = Json::object{
                    { "gcond_rhs",
                      tree_value_to_json_object (stmt_data.gcond_rhs) },
                    { "gcond_lhs",
                      tree_value_to_json_object (stmt_data.gcond_lhs) },

                    { "gcond_has_true_goto_label",
                      stmt_data.gcond_has_true_goto_label },
                    { "gcond_true_goto_label", gcond_true_goto_label },
                    { "gcond_has_false_else_goto_label",
                      stmt_data.gcond_has_false_else_goto_label },
                    { "gcond_false_else_goto_label",
                      gcond_false_else_goto_label },

                    { "gcond_tree_code_name", stmt_data.gcond_tree_code_name },

                    { "gcond_has_goto_true_edge",
                      stmt_data.gcond_has_goto_true_edge },
                    { "goto_true_edge", goto_true_edge },
                    { "gcond_has_else_goto_false_edge",
                      stmt_data.gcond_has_else_goto_false_edge },
                    { "else_goto_false_edge", else_goto_false_edge },
                };
                break;
            }

        case GIMPLE_LABEL:
            {
                data_args = Json::object{
                    { "glabel_label",
                      tree_value_to_json_object (stmt_data.glabel_label) },
                    { "glabel_is_non_local", stmt_data.glabel_is_non_local },
                };
                break;
            }

        case GIMPLE_GOTO:
            {
                data_args = Json::object{
                    { "ggoto_dest_goto_label",
                      tree_value_to_json_object (
                          stmt_data.ggoto_dest_goto_label) },
                };
                break;
            }

        case GIMPLE_NOP:
            {
                data_args = Json::object{
                    { "gnop_nop_str", stmt_data.gnop_nop_str },
                };
                break;
            }

        case GIMPLE_RETURN:
            {
                Json greturn_return_value = Json::object{};

                if (stmt_data.greturn_has_greturn_return_value)
                    greturn_return_value = tree_value_to_json_object (
                        stmt_data.greturn_return_value);

                data_args = Json::object{
                    { "greturn_return_value", greturn_return_value },
                    { "greturn_has_greturn_return_value",
                      stmt_data.greturn_has_greturn_return_value },
                };
                break;
            }

        case GIMPLE_SWITCH:
            {
                data_args = Json::object{
                    { "gswitch_switch_index",
                      tree_value_to_json_object (
                          stmt_data.gswitch_switch_index) },
                    { "gswitch_switch_case_labels",
                      tree_values_to_json_object (
                          stmt_data.gswitch_switch_case_labels) },
                    { "gswitch_switch_labels",
                      tree_values_to_json_object (
                          stmt_data.gswitch_switch_labels) },
                };
                break;
            }

        case GIMPLE_TRY:
            {
                Json gtry_try_cleanup = Json::object{};

                if (stmt_data.gtry_has_try_cleanup)
                    gtry_try_cleanup = stmts_data_to_json_object (
                        stmt_data.gtry_try_cleanup);

                data_args = Json::object{
                    { "gtry_try_cleanup", gtry_try_cleanup },
                    { "gtry_try_eval",
                      stmts_data_to_json_object (stmt_data.gtry_try_eval) },

                    { "gtry_try_type_kind", stmt_data.gtry_try_type_kind },
                    { "gtry_has_try_cleanup", stmt_data.gtry_has_try_cleanup },
                };
                break;
            }

        case GIMPLE_PHI:
            {
                data_args = Json::object{
                    { "gphi_lhs",
                      tree_value_to_json_object (stmt_data.gphi_lhs) },
                    { "gphi_phi_args",
                      tree_values_to_json_object (stmt_data.gphi_phi_args) },

                    { "gphi_phi_args_basicblock_src_index",
                      stmt_data.gphi_phi_args_basicblock_src_index },
                    { "gphi_phi_args_locations",
                      stmt_data.gphi_phi_args_locations },
                };
                break;
            }

        default:
            data_args = Json::object{};
            break;
        }

    return data_args;
}

Json
gimple_phi_data_to_json_object (gimple_phi_t &phis_data)
{
    std::vector<Json> gimple_phi_rhs_list;

    for (auto &gimple_phi_rhs : phis_data.gimple_phi_rhs_list)
        {
            Json gimple_phi_rhs_json = Json::object{
                { "line", gimple_phi_rhs.line },
                { "column", gimple_phi_rhs.column },
                { "basic_block_src_index",
                  gimple_phi_rhs.basic_block_src_index },
                { "phi_rhs",
                  tree_value_to_json_object (gimple_phi_rhs.phi_rhs) },
            };
            gimple_phi_rhs_list.push_back (gimple_phi_rhs_json);
        }

    Json gimple_phi_json = Json::object{
        { "phi_lhs", tree_value_to_json_object (phis_data.phi_lhs) },
        { "gimple_phi_rhs_list", gimple_phi_rhs_list },
    };

    return gimple_phi_json;
}

Json
bb_data_to_json_object (basicblock_t &bb_data)
{
    std::vector<Json> phis;

    for (auto &phis_data : bb_data.phis)
        {
            phis.push_back (gimple_phi_data_to_json_object (phis_data));
        }

    Json data_json = Json::object{ { "bb_index", bb_data.bb_index },
                                   { "bb_edges", bb_data.bb_edges },
                                   { "phis", phis } };

    return data_json;
}

Json
fn_arg_variable_to_json_object (fn_arg_variable_t var)
{
    return Json::object{
        { "arg", tree_value_to_json_object (var.arg) },
        { "var_type", tree_value_to_json_object (var.var_type) },
        { "var_declaration", tree_value_to_json_object (var.var_declaration) },
        { "var_def", tree_value_to_json_object (var.var_def) },
        { "var_ssa_name_var", tree_value_to_json_object (var.var_ssa_name_var) }
    };
}

Json
fn_arg_variables_to_json_object (std::vector<fn_arg_variable_t> vars)
{
    std::vector<Json> vars_json;

    for (auto var : vars)
        {
            vars_json.push_back (fn_arg_variable_to_json_object (var));
        }

    return vars_json;
}

Json
local_variable_to_json_object (fn_local_variable_t var)
{
    return Json::object{ { "arg", tree_value_to_json_object (var.arg) },
                         { "var_declaration",
                           tree_value_to_json_object (var.var_declaration) } };
}

Json
local_variables_to_json_object (std::vector<fn_local_variable_t> vars)
{
    std::vector<Json> vars_json;

    for (auto var : vars)
        {
            vars_json.push_back (local_variable_to_json_object (var));
        }

    return vars_json;
}

Json
fn_ssa_variable_to_json_object (fn_ssa_variable_t var)
{
    return Json::object{ { "arg", tree_value_to_json_object (var.arg) },
                         { "var_type",
                           tree_value_to_json_object (var.var_type) } };
}

Json
fn_ssa_variables_to_json_object (std::vector<fn_ssa_variable_t> vars)
{
    std::vector<Json> vars_json;

    for (auto var : vars)
        {
            vars_json.push_back (fn_ssa_variable_to_json_object (var));
        }

    return vars_json;
}

// Json source_lines_to_json_object(std::map<int, std::string> fn_source_lines)
// {
//    Json lines_json = Json::object {fn_source_lines};

//     // for (const auto& pair : fn_source_lines) {
//     //     lines_json[pair.first] = pair.second;
//     //     // std::cout << "Key: " << pair.first << ", Value: " <<
//     pair.second << std::endl;
//     // }

//     return lines_json;
// }

Json
function_data_to_json_object (function_data_t &fn_data)
{
    Json data_json = Json::object{
        { "fn_name", fn_data.fn_name },
        { "fn_filename", fn_data.fn_filename },
        { "fn_start_line_no", fn_data.fn_start_line_no },
        { "fn_end_line_no", fn_data.fn_end_line_no },
        { "fn_source_lines", fn_data.fn_source_lines },
        { "fn_decl", tree_value_to_json_object (fn_data.fn_decl) },
        { "fn_ssa_names", tree_values_to_json_object (fn_data.fn_ssa_names) },
        { "fn_args", fn_arg_variables_to_json_object (fn_data.fn_args) },
        { "fn_local_variables",
          local_variables_to_json_object (fn_data.fn_local_variables) },
        { "fn_ssa_variables",
          fn_ssa_variables_to_json_object (fn_data.fn_ssa_variables) },

    };

    return data_json;
}
