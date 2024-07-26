// This is the first gcc header to be included

#include "gcc-plugin.h"
#include "plugin-version.h"

#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include "gimple_extractor.h"
#include "data_formatter.h"
#include "data_utils.h"
#include "cgraph.h"


// We must assert that this plugin is GPL compatible
int plugin_is_GPL_compatible;

std::string config_data_format = "msgpack";
std::string config_output_path = "__default_gimple_extract_output/";
std::string config_source_path = ".";


static struct plugin_info my_gcc_plugin_info = {
    "1.0",
    "This is a gimple extractor plugin to extract gimple instructions" 
};

namespace
{
const pass_data gimple_extractor_pass_data = {
    GIMPLE_PASS,
    "gimple_extractor_pass", /* name */
    OPTGROUP_NONE,         /* optinfo_flags */
    TV_NONE,               /* tv_id */
    PROP_gimple_any,       /* properties_required */
    0,                     /* properties_provided */
    0,                     /* properties_destroyed */
    0,                     /* todo_flags_start */
    0                      /* todo_flags_finish */
};

struct gimple_extractor_pass : gimple_opt_pass
{
    gimple_extractor_pass (gcc::context *ctx)
        : gimple_opt_pass (gimple_extractor_pass_data, ctx)
    {
    }

    virtual unsigned int
    execute (function *fun) override
    {
        // function *fun
        // `struct GTY(()) function` defined in gcc-10.1.0/gcc/function.h

        function_data_t fn_data;
        fn_data.fn_name = function_name (fun);

        {
            expanded_location xloc
                = expand_location (fun->function_start_locus);
            fn_data.fn_start_line_no = xloc.line;

            if (xloc.file) 
            {
                fn_data.fn_filename = xloc.file;
            } 
            else 
            {
                if (current_function_decl) 
                {
                    const char *source_filename = DECL_SOURCE_FILE(current_function_decl);
                    if (source_filename)
                        fn_data.fn_filename = source_filename;
                }
            }
        }

        if (fn_data.fn_filename.size () == 0)
            return 0;

        fn_data.fn_filename = get_full_path(fn_data.fn_filename, true);

        if (!starts_with(fn_data.fn_filename, config_source_path)) 
            return 0;

        std::cout << "[gimple-extractor] processing ... [" << fn_data.fn_filename << "] -- "
                  << fn_data.fn_name << std::endl;

        std::vector<std::string> source_lines
            = readFileToVector (std::string (fn_data.fn_filename));
        int source_lines_size = source_lines.size ();

        std::vector<int> start_end_range
            = getRangeVector (fn_data.fn_start_line_no, fn_data.fn_end_line_no);

        for (int num : start_end_range)
            {
                //  if file is empty, set to emtpy string
                if (source_lines_size < 1)
                    {
                        fn_data.fn_source_lines[std::to_string (num)] = "";
                    }
                else
                    {
                        int real_lineno = num - 1;
                        if (real_lineno < source_lines_size)
                            {
                                fn_data.fn_source_lines[std::to_string (num)]
                                    = source_lines.at (real_lineno);
                            }
                        else
                            {
                                fn_data.fn_source_lines[std::to_string (num)]
                                    = "";
                            }
                    }
            }

        // function tree data
        {
            tree_value_t tvalue;
            std::vector<data_value_t> dvalues;

            get_tree_data_values (fun->decl, dvalues);

            tvalue.values = dvalues;
            fn_data.fn_decl = tvalue;
        }

        // function args tree data
        if (DECL_ARGUMENTS (fun->decl))
            {
                tree arg;
                for (arg = DECL_ARGUMENTS (fun->decl); arg;
                     arg = DECL_CHAIN (arg))
                    {
                        fn_arg_variable_t var;

                        tree def = ssa_default_def (fun, arg);
                        if (def)
                            {
                                {
                                    tree_value_t tvalue;
                                    std::vector<data_value_t> dvalues;

                                    get_tree_data_values (TREE_TYPE (def),
                                                          dvalues);
                                    tvalue.values = dvalues;

                                    var.var_type = tvalue;
                                }

                                {
                                    tree_value_t tvalue;
                                    std::vector<data_value_t> dvalues;

                                    get_tree_data_values (def, dvalues);
                                    tvalue.values = dvalues;

                                    var.var_def = tvalue;
                                }

                                {
                                    tree_value_t tvalue;
                                    std::vector<data_value_t> dvalues;

                                    get_tree_data_values (SSA_NAME_VAR (def),
                                                          dvalues);
                                    tvalue.values = dvalues;

                                    var.var_ssa_name_var = tvalue;
                                }
                            }

                        {
                            tree_value_t tvalue;
                            std::vector<data_value_t> dvalues;

                            data_value_t dvalue;
                            get_basic_tree_node_info (arg, dvalue);

                            print_declaration (arg, dvalues, dvalue);
                            tvalue.values = dvalues;

                            var.var_declaration = tvalue;
                        }

                        tree_value_t tvalue;
                        std::vector<data_value_t> dvalues;

                        get_tree_data_values (arg, dvalues);
                        tvalue.values = dvalues;

                        var.arg = tvalue;
                        fn_data.fn_args.push_back (var);
                    }
            }

        if (fun->local_decls)
            {
                tree arg;
                unsigned i;
                // unsigned len = vec_safe_length (fun->local_decls);
                FOR_EACH_LOCAL_DECL (fun, i, arg)
                {
                    if (arg)
                        {
                            fn_local_variable_t var;

                            {
                                tree_value_t tvalue;
                                std::vector<data_value_t> dvalues;

                                get_tree_data_values (arg, dvalues);
                                tvalue.values = dvalues;

                                var.arg = tvalue;
                            }

                            {
                                tree_value_t tvalue;
                                std::vector<data_value_t> dvalues;

                                data_value_t dvalue;
                                get_basic_tree_node_info (arg, dvalue);

                                print_declaration (arg, dvalues, dvalue);
                                tvalue.values = dvalues;

                                var.var_declaration = tvalue;
                            }

                            fn_data.fn_local_variables.push_back (var);
                        }
                }
            }

        if (gimple_in_ssa_p (fun))
            {
                unsigned i;
                tree name;
                FOR_EACH_SSA_NAME (i, name, fun)
                {
                    if (!SSA_NAME_VAR (name))
                        {
                            fn_ssa_variable_t var;

                            {
                                tree_value_t tvalue;
                                std::vector<data_value_t> dvalues;

                                get_tree_data_values (TREE_TYPE (name),
                                                      dvalues);
                                tvalue.values = dvalues;

                                var.var_type = tvalue;
                            }

                            {
                                tree_value_t tvalue;
                                std::vector<data_value_t> dvalues;

                                get_tree_data_values (name, dvalues);
                                tvalue.values = dvalues;

                                var.arg = tvalue;
                            }

                            fn_data.fn_ssa_variables.push_back (var);
                        }
                }
            }

        for (unsigned i = 1; i < fun->gimple_df->ssa_names->length (); ++i)
            {
                tree name = ssa_name (i);
                if (name)
                    {
                        tree_value_t tvalue;
                        std::vector<data_value_t> dvalues;

                        get_tree_data_values (name, dvalues);

                        tvalue.values = dvalues;
                        fn_data.fn_ssa_names.push_back (tvalue);
                    }
            }

        edge e;
        edge_iterator ei;
        basic_block bb;

        std::vector<gimple_stmt_data> stmt_data_list;
        std::vector<basicblock_t> basic_block_list;

        FOR_EACH_BB_FN (bb, fun)
        {
            gimple_bb_info *bb_info = &bb->il.gimple;
            std::vector<int> bb_edges;

            FOR_EACH_EDGE (e, ei, bb->succs)
            {
                basic_block succ_bb = e->dest;
                bb_edges.push_back (succ_bb->index);
            }

            basicblock_t bb_data;
            bb_data.bb_index = bb->index;
            bb_data.bb_edges = bb_edges;

            dump_phi_nodes (bb, bb_data);

            gimple_stmt_iterator i;
            for (i = gsi_start (bb_info->seq); !gsi_end_p (i); gsi_next (&i))
                {
                    gimple *gs = gsi_stmt (i);
                    gimple_stmt_data stmt_data
                        = gimple_tuple_to_stmt_data (gs, bb->index, bb_edges);
                    stmt_data_list.push_back (stmt_data);
                }

            basic_block_list.push_back (bb_data);
        }

        std::string fn_extract_dump = function_to_string_dump (
            stmt_data_list, basic_block_list, fn_data, config_data_format);

        write_function_to_file (fn_data.fn_filename, fn_data.fn_name,
                                fn_extract_dump);

        std::cout << "[gimple-extractor] done ... [" << fn_data.fn_filename << "] -- "
                  << fn_data.fn_name << std::endl;

        return 0;
    }

    virtual gimple_extractor_pass *
    clone () override
    {
        // We do not clone ourselves
        return this;
    }
};
}

int
plugin_init (struct plugin_name_args *plugin_info,
             struct plugin_gcc_version *version)
{
    if (!plugin_default_version_check (version, &gcc_version))
        {
            std::cerr << "This GCC plugin is for version "
                      << GCCPLUGIN_VERSION_MAJOR << "."
                      << GCCPLUGIN_VERSION_MINOR << "\n";
            return 1;
        }

    register_callback (plugin_info->base_name,
                       /* event */ PLUGIN_INFO,
                       /* callback */ NULL,
                       /* user_data */ &my_gcc_plugin_info);

    struct register_pass_info pass_info;

    pass_info.pass = new gimple_extractor_pass (g);
    pass_info.reference_pass_name = "ssa";
    pass_info.ref_pass_instance_number = 1;
    pass_info.pos_op = PASS_POS_INSERT_AFTER;

    for (int i = 0; i < plugin_info->argc; i++)
        {
            std::string key = std::string(plugin_info->argv[i].key);
            std::string val = std::string(plugin_info->argv[i].value);

            if (key == "source_path") {
                config_source_path = val;
                config_source_path = get_full_path(config_source_path, true);
            }

            if (key == "output_path") {
                config_output_path = val;
                config_output_path = get_full_path(config_output_path, false);
            }

            if (key == "data_format") {
                if (val == "json")
                    config_data_format = "json";

                if (val == "msgpack")
                    config_data_format = "msgpack";
            }
        }

    register_callback (plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL,
                       &pass_info);

    return 0;
}


void
write_function_to_file (std::string filename, std::string function_name,
                        std::string function_extract_dump)
{
    std::string output_filename_without_source_path
        = filename.substr (config_source_path.size ());
    std::string output_function_name = function_name;
    std::replace (output_filename_without_source_path.begin (),
                  output_filename_without_source_path.end (), '.', '_');

    std::replace (output_function_name.begin (), output_function_name.end (),
                  ':', '_');
    std::replace (output_function_name.begin (), output_function_name.end (),
                  '~', '_');
    std::replace (output_function_name.begin (), output_function_name.end (),
                  '+', '_');

    if (ends_with_char (config_output_path, '/') == false)
        {
            if (starts_with_char (output_filename_without_source_path, '/')
                == false)
                {
                    config_output_path += "/";
                }
        }

    std::string output_dir_path
        = config_output_path + output_filename_without_source_path;

    if (!create_directories (output_dir_path))
        {
            throw std::runtime_error ("Error creating extract directory");
        }

    std::string output_full_path = output_dir_path + "/" + output_function_name
                                   + "." + config_data_format;

    // std::cout << output_full_path << std::endl;

    std::ofstream jsonfile;
    jsonfile.open (output_full_path);
    jsonfile << function_extract_dump;
    jsonfile.close ();
}

gimple_stmt_data
gimple_tuple_to_stmt_data (gimple *g, int bb_index, std::vector<int> &bb_edges)
{

    gimple_stmt_data stmt_data;
    // stmt_data.function_name = function_name;

    stmt_data.gimple_stmt_code = gimple_code (g);
    stmt_data.gimple_stmt_code_str
        = gimple_code_name[stmt_data.gimple_stmt_code];

    /*
        enum tree_code gimple_expr_code (gimple stmt)
        Return the tree code for the expression computed by STMT. This is only
       meaningful for GIMPLE_CALL, GIMPLE_ASSIGN and GIMPLE_COND. If STMT is
       GIMPLE_CALL, it will return CALL_EXPR. For GIMPLE_COND, it returns the
       code of the comparison predicate. For GIMPLE_ASSIGN it returns the code
       of the operation performed by the RHS of the assignment.

        Alternatively not accurate results
        stmt_data.gimple_stmt_expr_code = (enum tree_code)g->subcode;
        stmt_data.gimple_stmt_expr_code_str =
       get_tree_code_name(stmt_data.gimple_stmt_expr_code);

    */
    stmt_data.gimple_stmt_expr_code = gimple_expr_code (g);
    stmt_data.gimple_stmt_expr_code_str
        = get_tree_code_name (stmt_data.gimple_stmt_expr_code);

    stmt_data.has_substatements = bool_cast (gimple_has_substatements (g));

    if (gimple_has_location (g))
        {
            stmt_data.filename = gimple_filename (g);
            stmt_data.lineno = gimple_lineno (g);
        }

    stmt_data.has_register_or_memory_operands = bool_cast (gimple_has_ops (g));
    stmt_data.has_memory_operands = bool_cast (gimple_has_mem_ops (g));

    stmt_data.basic_block_index = bb_index;
    stmt_data.basic_block_edges = bb_edges;

    stmt_data.gimple_num_ops = gimple_num_ops (g);

    // gimple_tuple_args(g, stmt_data);

    get_gimple_mem_ops (g, stmt_data);
    gimple_tuple_arg_values (g, stmt_data);

    return stmt_data;
}

const std::string
bool_cast (const bool b)
{
    return b ? "true" : "false";
}

gimple_phi_t
dump_gimple_phi (const gphi *phi)
{
    gimple_phi_t phi_data;

    size_t i;
    tree lhs = gimple_phi_result (phi);

    {
        tree_value_t tvalue;
        std::vector<data_value_t> dvalues;

        get_tree_data_values (lhs, dvalues);

        tvalue.values = dvalues;
        phi_data.phi_lhs = tvalue;
    }

    for (i = 0; i < gimple_phi_num_args (phi); i++)
        {
            gimple_phi_rhs_t gimple_phi_rhs;

            if (gimple_phi_arg_has_location ((gphi*)phi, i))
                {

                    location_t loc = gimple_phi_arg_location ((gphi*)phi, i);
                    expanded_location xloc = expand_location (loc);
                    gimple_phi_rhs.line = xloc.line;
                    gimple_phi_rhs.column = xloc.column;
                }

            basic_block src = gimple_phi_arg_edge ((gphi*)phi, i)->src;
            gimple_phi_rhs.basic_block_src_index = src->index;

            tree_value_t tvalue;
            std::vector<data_value_t> dvalues;

            get_tree_data_values (gimple_phi_arg_def ((gphi*)phi, i), dvalues);

            tvalue.values = dvalues;
            gimple_phi_rhs.phi_rhs = tvalue;

            phi_data.gimple_phi_rhs_list.push_back (gimple_phi_rhs);
        }

    return phi_data;
}

void
dump_phi_nodes (basic_block bb, basicblock_t &bb_data)
{
    gphi_iterator i;

    for (i = gsi_start_phis (bb); !gsi_end_p (i); gsi_next (&i))
        {
            gphi *phi = i.phi ();
            if (!virtual_operand_p (gimple_phi_result (phi)))
                {
                    gimple_phi_t gimple_phi = dump_gimple_phi (phi);
                    bb_data.phis.push_back (gimple_phi);
                }
        }
}

void
gimple_tuple_args (gimple *g, gimple_stmt_data &stmt_data)
{
    if (stmt_data.gimple_num_ops < 1)
        return;

    std::vector<tree> ops;
    for (unsigned int i = 0; i < stmt_data.gimple_num_ops; ++i)
        {
            tree op = gimple_op (g, i);
            if (op == NULL_TREE)
                {
                    // printf("skipping op index %d\n", i);
                    continue;
                }
            ops.push_back (op);
        }

    for (const auto &op : ops)
        {
            const tree tree_node = static_cast<const tree> (op);

            std::vector<data_value_t> dvalues;
            get_tree_data_values (tree_node, dvalues);

            tree_value_t tvalue{ .values = dvalues };
            stmt_data.tree_values.push_back (tvalue);
        }
}

void
append_simple_value (std::vector<data_value_t> &dvalues, data_value_t &dvalue,
                     std::string value)
{
    data_value_t v = dvalue;
    v.value_type = "simple";
    v.simple_data_value = value;
    dvalues.push_back (v);
}

void
append_complex_value (std::vector<data_value_t> &dvalues, data_value_t &dvalue,
                      std::vector<data_value_t> &complex_dvalues)
{
    data_value_t v = dvalue;
    v.value_type = "complex";
    v.complex_data_values = complex_dvalues;
    dvalues.push_back (v);
}

void
get_basic_tree_node_info (tree tree_node, data_value_t &dvalue)
{
    dvalue.code = TREE_CODE (tree_node);
    dvalue.code_class = TREE_CODE_CLASS_STRING (TREE_CODE_CLASS (dvalue.code));
    dvalue.code_name = get_tree_code_name (dvalue.code);
    dvalue.is_expr = EXPR_P (tree_node);
    dvalue.operand_length = TREE_OPERAND_LENGTH (tree_node);

    if (EXPR_HAS_LOCATION (tree_node))
        {
            location_t loc = EXPR_LOCATION (tree_node);
            expanded_location xloc = expand_location (loc);

            if (xloc.file)
                dvalue.location_file = xloc.file;

            dvalue.location_line = xloc.line;
            dvalue.location_column = xloc.column;
        }
}

static void
dump_fancy_name (std::vector<data_value_t> &dvalues, data_value_t &dvalue,
                 tree name)
{
    int cnt = 0;
    int length = IDENTIFIER_LENGTH (name);
    const char *n = IDENTIFIER_POINTER (name);
    do
        {
            n = strchr (n, 'D');
            if (n == NULL)
                break;
            if (ISDIGIT (n[1])
                && (n == IDENTIFIER_POINTER (name) || n[-1] == '$'))
                {
                    int l = 2;
                    while (ISDIGIT (n[l]))
                        l++;
                    if (n[l] == '\0' || n[l] == '$')
                        {
                            cnt++;
                            length += 5 - l;
                        }
                    n += l;
                }
            else
                n++;
        }
    while (1);
    if (cnt == 0)
        {
            ppp_tree_identifier (dvalues, dvalue, name);
            return;
        }

    char *str = XNEWVEC (char, length + 1);
    char *p = str;
    const char *q;
    q = n = IDENTIFIER_POINTER (name);
    do
        {
            q = strchr (q, 'D');
            if (q == NULL)
                break;
            if (ISDIGIT (q[1])
                && (q == IDENTIFIER_POINTER (name) || q[-1] == '$'))
                {
                    int l = 2;
                    while (ISDIGIT (q[l]))
                        l++;
                    if (q[l] == '\0' || q[l] == '$')
                        {
                            memcpy (p, n, q - n);
                            memcpy (p + (q - n), "Dxxxx", 5);
                            p += (q - n) + 5;
                            n = q + l;
                        }
                    q += l;
                }
            else
                q++;
        }
    while (1);
    memcpy (p, n, IDENTIFIER_LENGTH (name) - (n - IDENTIFIER_POINTER (name)));
    str[length] = '\0';

    const char *text = identifier_to_locale (str);
    append_simple_value (dvalues, dvalue, text);

    XDELETEVEC (str);
}

void
ppp_tree_identifier (std::vector<data_value_t> &dvalues, data_value_t &dvalue,
                     tree id)
{
    // if (pp_translate_identifiers(pp)) {
    //     const char *text = identifier_to_locale(IDENTIFIER_POINTER(id));
    //     pp_append_text(dvalues, dvalue, text, text + strlen(text));
    // } else

    std::string text;
    text.assign (IDENTIFIER_POINTER (id), IDENTIFIER_LENGTH (id));
    append_simple_value (dvalues, dvalue, text);
}

void
dump_decl_name (tree node, std::vector<data_value_t> &dvalues,
                data_value_t &dvalue)
{
    tree name = DECL_NAME (node);
    if (name)
        {
            if (HAS_DECL_ASSEMBLER_NAME_P (node)
                && DECL_ASSEMBLER_NAME_SET_P (node))
                {
                    ppp_tree_identifier (dvalues, dvalue,
                                         DECL_ASSEMBLER_NAME_RAW (node));
                }

            /* For -fcompare-debug don't dump DECL_NAMELESS names at all,
               -g might have created more fancy names and their indexes
               could get out of sync.  Usually those should be DECL_IGNORED_P
               too, SRA can create even non-DECL_IGNORED_P DECL_NAMELESS fancy
               names, let's hope those never get out of sync after doing the
               dump_fancy_name sanitization.  */
            else if (DECL_NAMELESS (node) && DECL_IGNORED_P (node))
                {
                    name = NULL_TREE;
                }

            /* For DECL_NAMELESS names look for embedded uids in the
               names and sanitize them for TDF_NOUID.  */
            else if (DECL_NAMELESS (node))
                {
                    dump_fancy_name (dvalues, dvalue, name);
                }

            else
                {
                    ppp_tree_identifier (dvalues, dvalue, name);
                }
        }

    // char uid_sep = (flags & TDF_GIMPLE) ? '_' : '.';
    char uid_sep = '_';

    if (name == NULL_TREE)
        {
            if (TREE_CODE (node) == LABEL_DECL && LABEL_DECL_UID (node) != -1)
                {
                    append_simple_value (
                        dvalues, dvalue,
                        "L" + std::to_string (uid_sep)
                            + std::to_string ((int)LABEL_DECL_UID (node)));
                }

            else if (TREE_CODE (node) == DEBUG_EXPR_DECL)
                {
                    append_simple_value (
                        dvalues, dvalue,
                        "D#" + std::to_string (DEBUG_TEMP_UID (node)));
                }
            else
                {
                    char c = TREE_CODE (node) == CONST_DECL ? 'C' : 'D';
                    append_simple_value (
                        dvalues, dvalue,
                        std::to_string (c) + std::to_string (uid_sep)
                            + std::to_string (DECL_UID (node)));
                }
        }
    if (DECL_PT_UID (node) != DECL_UID (node))
        {
            append_simple_value (dvalues, dvalue,
                                 "ptD." + std::to_string (DECL_PT_UID (node)));
        }
}

/* Print X to PP in decimal.  */
template <unsigned int N, typename T>
void
ppp_wide_integer (std::vector<data_value_t> &dvalues, data_value_t &dvalue,
                  const poly_int_pod<N, T> &x)
{
    if (x.is_constant ())
        append_simple_value (dvalues, dvalue, std::to_string (x.coeffs[0]));
    else
        {
            ppp_left_bracket (dvalues, dvalue);
            for (unsigned int i = 0; i < N; ++i)
                {
                    if (i != 0)
                        ppp_comma (dvalues, dvalue);
                    append_simple_value (dvalues, dvalue,
                                         std::to_string (x.coeffs[i]));
                }
            ppp_right_bracket (dvalues, dvalue);
        }
}

static void
dump_function_declaration (tree node, std::vector<data_value_t> &dvalues,
                           data_value_t &dvalue)
{
    bool wrote_arg = false;
    tree arg;

    ppp_space (dvalues, dvalue);
    ppp_left_paren (dvalues, dvalue);

    /* Print the argument types.  */
    arg = TYPE_ARG_TYPES (node);
    while (arg && arg != void_list_node && arg != error_mark_node)
        {
            if (wrote_arg)
                {
                    ppp_comma (dvalues, dvalue);
                    ppp_space (dvalues, dvalue);
                }
            wrote_arg = true;
            get_tree_data_values (TREE_VALUE (arg), dvalues);
            arg = TREE_CHAIN (arg);
        }

    /* Drop the trailing void_type_node if we had any previous argument.  */
    if (arg == void_list_node && !wrote_arg)
        {
            ppp_string (dvalues, dvalue, "void");
        }
    /* Properly dump vararg function types.  */
    else if (!arg && wrote_arg)
        {
            ppp_string (dvalues, dvalue, ", ...");
        }
    /* Avoid printing any arg for unprototyped functions.  */

    ppp_right_paren (dvalues, dvalue);
}

static void
do_niy (const_tree node, std::vector<data_value_t> &dvalues,
        data_value_t &dvalue)
{
    int i, len;

    ppp_string (dvalues, dvalue, "<<< Unknown tree: ");
    ppp_string (dvalues, dvalue, get_tree_code_name (TREE_CODE (node)));

    if (EXPR_P (node))
        {
            len = TREE_OPERAND_LENGTH (node);
            for (i = 0; i < len; ++i)
                {
                    // newline_and_indent(dvalues, dvalue, 2);
                    get_tree_data_values (TREE_OPERAND (node, i), dvalues);
                }
        }

    ppp_string (dvalues, dvalue, " >>>");
}

static void
dump_mem_ref (tree node, std::vector<data_value_t> &dvalues,
              data_value_t &dvalue)
{
    ppp_string (dvalues, dvalue, "__MEM <");
    get_tree_data_values (TREE_TYPE (node), dvalues);

    if (TYPE_ALIGN (TREE_TYPE (node))
        != TYPE_ALIGN (TYPE_MAIN_VARIANT (TREE_TYPE (node))))
        {
            ppp_string (dvalues, dvalue, ", ");
            ppp_decimal_int (dvalues, dvalue, TYPE_ALIGN (TREE_TYPE (node)));
        }
    ppp_greater (dvalues, dvalue);
    ppp_string (dvalues, dvalue, " (");
    if (TREE_TYPE (TREE_OPERAND (node, 0))
        != TREE_TYPE (TREE_OPERAND (node, 1)))
        {
            ppp_left_paren (dvalues, dvalue);
            get_tree_data_values (TREE_TYPE (TREE_OPERAND (node, 1)), dvalues);
            ppp_right_paren (dvalues, dvalue);
        }
    get_tree_data_values (TREE_OPERAND (node, 0), dvalues);
    if (!integer_zerop (TREE_OPERAND (node, 1)))
        {
            ppp_string (dvalues, dvalue, " + ");
            get_tree_data_values (TREE_OPERAND (node, 1), dvalues);
        }
    ppp_right_paren (dvalues, dvalue);

    if (integer_zerop (TREE_OPERAND (node, 1))
        /* Dump the types of INTEGER_CSTs explicitly, for we can't
           infer them and MEM_ATTR caching will share MEM_REFs
           with differently-typed op0s.  */
        && TREE_CODE (TREE_OPERAND (node, 0)) != INTEGER_CST
        /* Released SSA_NAMES have no TREE_TYPE.  */
        && TREE_TYPE (TREE_OPERAND (node, 0)) != NULL_TREE
        /* Same pointer types, but ignoring POINTER_TYPE vs.
           REFERENCE_TYPE.  */
        && (TREE_TYPE (TREE_TYPE (TREE_OPERAND (node, 0)))
            == TREE_TYPE (TREE_TYPE (TREE_OPERAND (node, 1))))
        && (TYPE_MODE (TREE_TYPE (TREE_OPERAND (node, 0)))
            == TYPE_MODE (TREE_TYPE (TREE_OPERAND (node, 1))))
        && (TYPE_REF_CAN_ALIAS_ALL (TREE_TYPE (TREE_OPERAND (node, 0)))
            == TYPE_REF_CAN_ALIAS_ALL (TREE_TYPE (TREE_OPERAND (node, 1))))
        /* Same value types ignoring qualifiers.  */
        && (TYPE_MAIN_VARIANT (TREE_TYPE (node))
            == TYPE_MAIN_VARIANT (
                TREE_TYPE (TREE_TYPE (TREE_OPERAND (node, 1)))))
        && (MR_DEPENDENCE_CLIQUE (node) == 0))
        {
            if (TREE_CODE (TREE_OPERAND (node, 0)) != ADDR_EXPR)
                {
                    /* Enclose pointers to arrays in parentheses.  */
                    tree op0 = TREE_OPERAND (node, 0);
                    tree op0type = TREE_TYPE (op0);
                    if (POINTER_TYPE_P (op0type)
                        && TREE_CODE (TREE_TYPE (op0type)) == ARRAY_TYPE)
                        ppp_left_paren (dvalues, dvalue);
                    ppp_star (dvalues, dvalue);
                    get_tree_data_values (op0, dvalues);
                    if (POINTER_TYPE_P (op0type)
                        && TREE_CODE (TREE_TYPE (op0type)) == ARRAY_TYPE)
                        ppp_right_paren (dvalues, dvalue);
                }
            else
                get_tree_data_values (TREE_OPERAND (TREE_OPERAND (node, 0), 0),
                                      dvalues);
        }
    else
        {
            ppp_string (dvalues, dvalue, "MEM");

            tree nodetype = TREE_TYPE (node);
            tree op0 = TREE_OPERAND (node, 0);
            tree op1 = TREE_OPERAND (node, 1);
            tree op1type = TYPE_MAIN_VARIANT (TREE_TYPE (op1));

            tree op0size = TYPE_SIZE (nodetype);
            tree op1size = TYPE_SIZE (TREE_TYPE (op1type));

            if (!op0size || !op1size || !operand_equal_p (op0size, op1size, 0))
                {
                    ppp_string (dvalues, dvalue, " <");
                    /* If the size of the type of the operand is not the same
                       as the size of the MEM_REF expression include the type
                       of the latter similar to the TDF_GIMPLE output to make
                       it clear how many bytes of memory are being accessed.  */
                    get_tree_data_values (nodetype, dvalues);
                    ppp_string (dvalues, dvalue, "> ");
                }

            ppp_string (dvalues, dvalue, "[(");
            get_tree_data_values (op1type, dvalues);
            ppp_right_paren (dvalues, dvalue);
            get_tree_data_values (op0, dvalues);
            if (!integer_zerop (op1))
                if (!integer_zerop (TREE_OPERAND (node, 1)))
                    {
                        ppp_string (dvalues, dvalue, " + ");
                        get_tree_data_values (op1, dvalues);
                    }
            if (MR_DEPENDENCE_CLIQUE (node) != 0)
                {
                    ppp_string (dvalues, dvalue, " clique ");
                    append_simple_value (
                        dvalues, dvalue,
                        std::to_string (MR_DEPENDENCE_BASE (node)));

                    ppp_string (dvalues, dvalue, " base ");
                    append_simple_value (
                        dvalues, dvalue,
                        std::to_string (MR_DEPENDENCE_BASE (node)));
                }
            ppp_right_bracket (dvalues, dvalue);
        }
}

void
pretty_print_string (std::vector<data_value_t> &dvalues, data_value_t &dvalue,
                     const char *str, size_t n)
{
    if (str == NULL)
        return;

    for (; n; --n, ++str)
        {
            switch (str[0])
                {
                case '\b':
                    ppp_string (dvalues, dvalue, "\\b");
                    break;

                case '\f':
                    ppp_string (dvalues, dvalue, "\\f");
                    break;

                case '\n':
                    ppp_string (dvalues, dvalue, "\\n");
                    break;

                case '\r':
                    ppp_string (dvalues, dvalue, "\\r");
                    break;

                case '\t':
                    ppp_string (dvalues, dvalue, "\\t");
                    break;

                case '\v':
                    ppp_string (dvalues, dvalue, "\\v");
                    break;

                case '\\':
                    ppp_string (dvalues, dvalue, "\\\\");
                    break;

                case '\"':
                    ppp_string (dvalues, dvalue, "\\\"");
                    break;

                case '\'':
                    ppp_string (dvalues, dvalue, "\\'");
                    break;

                default:
                    if (str[0] || n > 1)
                        {
                            if (!ISPRINT (str[0]))
                                {
                                    char buf[5];
                                    sprintf (buf, "\\x%02x",
                                             (unsigned char)str[0]);
                                    ppp_string (dvalues, dvalue, buf);
                                }
                            else
                                {
                                    ppp_string (dvalues, dvalue,
                                                std::to_string (str[0]));
                                }
                            break;
                        }
                }
        }
}

static void
dump_array_domain (tree domain, std::vector<data_value_t> &dvalues,
                   data_value_t &dvalue)
{
    ppp_left_bracket (dvalues, dvalue);
    if (domain)
        {
            tree min = TYPE_MIN_VALUE (domain);
            tree max = TYPE_MAX_VALUE (domain);

            if (min && max && integer_zerop (min) && tree_fits_shwi_p (max))
                {
                    // ppp_wide_integer(dvalues, dvalue, tree_to_shwi(max) + 1);
                    ppp_string (dvalues, dvalue,
                                std::to_string (tree_to_shwi (max) + 1));
                }
            else
                {
                    if (min)
                        {
                            get_tree_data_values (min, dvalues);
                        }

                    ppp_colon (dvalues, dvalue);
                    if (max)
                        {
                            get_tree_data_values (max, dvalues);
                        }
                }
        }
    else
        ppp_string (dvalues, dvalue, "<unknown>");
    ppp_right_bracket (dvalues, dvalue);
}

void
print_declaration (tree t, std::vector<data_value_t> &dvalues,
                   data_value_t &dvalue)
{
    int spc = 1;
    INDENT (dvalues, dvalue, spc);

    if (TREE_CODE (t) == NAMELIST_DECL)
        {
            ppp_string (dvalues, dvalue, "namelist ");
            dump_decl_name (t, dvalues, dvalue);
            ppp_semicolon (dvalues, dvalue);
            return;
        }

    if (TREE_CODE (t) == TYPE_DECL)
        {
            ppp_string (dvalues, dvalue, "typedef ");
        }

    if (CODE_CONTAINS_STRUCT (TREE_CODE (t), TS_DECL_WRTL) && DECL_REGISTER (t))
        ppp_string (dvalues, dvalue, "register ");

    if (TREE_PUBLIC (t) && DECL_EXTERNAL (t))
        {
            ppp_string (dvalues, dvalue, "extern ");
        }
    else if (TREE_STATIC (t))
        {
            ppp_string (dvalues, dvalue, "static ");
        }

    /* Print the type and name.  */
    if (TREE_TYPE (t) && TREE_CODE (TREE_TYPE (t)) == ARRAY_TYPE)
        {
            tree tmp;

            /* Print array's type.  */
            tmp = TREE_TYPE (t);
            while (TREE_CODE (TREE_TYPE (tmp)) == ARRAY_TYPE)
                {
                    tmp = TREE_TYPE (tmp);
                }
            get_tree_data_values (TREE_TYPE (tmp), dvalues);

            /* Print variable's name.  */
            ppp_space (dvalues, dvalue);
            get_tree_data_values (t, dvalues);

            /* Print the dimensions.  */
            tmp = TREE_TYPE (t);
            while (TREE_CODE (tmp) == ARRAY_TYPE)
                {
                    dump_array_domain (TYPE_DOMAIN (tmp), dvalues, dvalue);
                    tmp = TREE_TYPE (tmp);
                }
        }
    else if (TREE_CODE (t) == FUNCTION_DECL)
        {
            get_tree_data_values (TREE_TYPE (TREE_TYPE (t)), dvalues);
            ppp_space (dvalues, dvalue);
            dump_decl_name (t, dvalues, dvalue);
            dump_function_declaration (TREE_TYPE (t), dvalues, dvalue);
        }
    else
        {
            /* Print type declaration.  */
            get_tree_data_values (TREE_TYPE (t), dvalues);

            /* Print variable's name.  */
            ppp_space (dvalues, dvalue);
            get_tree_data_values (t, dvalues);
        }

    if (VAR_P (t) && DECL_HARD_REGISTER (t))
        {
            ppp_string (dvalues, dvalue, " __asm__ ");
            ppp_left_paren (dvalues, dvalue);
            get_tree_data_values (DECL_ASSEMBLER_NAME (t), dvalues);
            ppp_right_paren (dvalues, dvalue);
        }

    /* The initial value of a function serves to determine whether the function
       is declared or defined.  So the following does not apply to function
       nodes.  */
    if (TREE_CODE (t) != FUNCTION_DECL)
        {
            /* Print the initial value.  */
            if (DECL_INITIAL (t))
                {
                    ppp_space (dvalues, dvalue);
                    ppp_equal (dvalues, dvalue);
                    ppp_space (dvalues, dvalue);
                    get_tree_data_values (DECL_INITIAL (t), dvalues);
                }
        }

    if (VAR_P (t) && DECL_HAS_VALUE_EXPR_P (t))
        {
            ppp_string (dvalues, dvalue, " [value-expr: ");
            get_tree_data_values (DECL_VALUE_EXPR (t), dvalues);
            ppp_right_bracket (dvalues, dvalue);
        }

    ppp_semicolon (dvalues, dvalue);
}

static void
dump_function_name (tree node, std::vector<data_value_t> &dvalues,
                    data_value_t &dvalue)
{
    if (CONVERT_EXPR_P (node))
        {
            node = TREE_OPERAND (node, 0);
        }
    if (DECL_NAME (node))
        {
            ppp_string (dvalues, dvalue,
                        lang_hooks.decl_printable_name (node, 1));
        }
    else
        {
            dump_decl_name (node, dvalues, dvalue);
        }
}

void
print_call_name (tree node, std::vector<data_value_t> &dvalues,
                 data_value_t &dvalue)
{
    tree op0 = node;

    if (TREE_CODE (op0) == NON_LVALUE_EXPR)
        {
            op0 = TREE_OPERAND (op0, 0);
        }

again:
    switch (TREE_CODE (op0))
        {
        case VAR_DECL:
        case PARM_DECL:
        case FUNCTION_DECL:
            {
                dump_function_name (op0, dvalues, dvalue);
                break;
            }

        case ADDR_EXPR:
        case INDIRECT_REF:
        CASE_CONVERT:
            {
                op0 = TREE_OPERAND (op0, 0);
                goto again;
            }

        case COND_EXPR:
            {
                ppp_left_paren (dvalues, dvalue);
                get_tree_data_values (TREE_OPERAND (op0, 0), dvalues);
                ppp_string (dvalues, dvalue, ") ? ");
                get_tree_data_values (TREE_OPERAND (op0, 1), dvalues);
                ppp_string (dvalues, dvalue, " : ");
                get_tree_data_values (TREE_OPERAND (op0, 2), dvalues);
                break;
            }

        case ARRAY_REF:
            {
                if (TREE_CODE (TREE_OPERAND (op0, 0)) == VAR_DECL)
                    {
                        get_tree_data_values (TREE_OPERAND (op0, 0), dvalues);
                    }
                else
                    {
                        get_tree_data_values (op0, dvalues);
                    }
                break;
            }

        case MEM_REF:
            if (integer_zerop (TREE_OPERAND (op0, 1)))
                {
                    op0 = TREE_OPERAND (op0, 0);
                    goto again;
                }
            /* Fallthru.  */
        case COMPONENT_REF:
        case SSA_NAME:
        case OBJ_TYPE_REF:
            get_tree_data_values (op0, dvalues);
            break;

        default:
            do_niy (node, dvalues, dvalue);
        }
}

static const char *
op_symbol (const_tree op)
{
    return op_symbol_code (TREE_CODE (op));
}

void
get_tree_data_values (tree node, std::vector<data_value_t> &dvalues)
{
    data_value_t dvalue;
    get_basic_tree_node_info (node, dvalue);

    if (node == NULL_TREE)
        {
            append_simple_value (dvalues, dvalue, "NULL");
            return;
        }

    switch (dvalue.code)
        {
        case ERROR_MARK:
            {
                append_simple_value (dvalues, dvalue, "<<< error >>>");
                break;
            }

        case IDENTIFIER_NODE:
            {
                const char *text
                    = identifier_to_locale (IDENTIFIER_POINTER (node));
                append_simple_value (dvalues, dvalue, text);
                break;
            }

        case TREE_LIST:
            {
                std::vector<data_value_t> complex_dvalues;

                while (node && node != error_mark_node)
                    {
                        if (TREE_PURPOSE (node))
                            {
                                get_tree_data_values (TREE_PURPOSE (node),
                                                      complex_dvalues);

                                ppp_space (complex_dvalues, dvalue);
                            }
                        get_tree_data_values (TREE_VALUE (node),
                                              complex_dvalues);
                        node = TREE_CHAIN (node);
                        if (node && TREE_CODE (node) == TREE_LIST)
                            {
                                ppp_comma (complex_dvalues, dvalue);
                                ppp_space (complex_dvalues, dvalue);
                            }
                    }

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case TREE_BINFO:
            {
                std::vector<data_value_t> complex_dvalues;
                get_tree_data_values (BINFO_TYPE (node), complex_dvalues);
                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case TREE_VEC:
            {
                std::vector<data_value_t> complex_dvalues;

                size_t i;
                if (TREE_VEC_LENGTH (node) > 0)
                    {
                        size_t len = TREE_VEC_LENGTH (node);
                        for (i = 0; i < len - 1; i++)
                            {
                                get_tree_data_values (TREE_VEC_ELT (node, i),
                                                      complex_dvalues);
                                ppp_comma (complex_dvalues, dvalue);
                                ppp_space (complex_dvalues, dvalue);
                            }
                        get_tree_data_values (TREE_VEC_ELT (node, len - 1),
                                              complex_dvalues);
                    }

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case VOID_TYPE:
        case INTEGER_TYPE:
        case REAL_TYPE:
        case FIXED_POINT_TYPE:
        case COMPLEX_TYPE:
        case VECTOR_TYPE:
        case ENUMERAL_TYPE:
        case BOOLEAN_TYPE:
            {
                std::vector<data_value_t> complex_dvalues;

                unsigned int quals = TYPE_QUALS (node);
                enum tree_code_class tclass;

                if (quals & TYPE_QUAL_ATOMIC)
                    ppp_string (complex_dvalues, dvalue, "atomic");
                if (quals & TYPE_QUAL_CONST)
                    ppp_string (complex_dvalues, dvalue, "const");
                if (quals & TYPE_QUAL_VOLATILE)
                    ppp_string (complex_dvalues, dvalue, "volatile");
                if (quals & TYPE_QUAL_RESTRICT)
                    ppp_string (complex_dvalues, dvalue, "restrict");

                if (!ADDR_SPACE_GENERIC_P (TYPE_ADDR_SPACE (node)))
                    {
                        std::string addr_space
                            = "<address-space-"
                              + std::to_string (TYPE_ADDR_SPACE (node)) + ">";
                        append_simple_value (complex_dvalues, dvalue,
                                             addr_space);
                    }

                tclass = TREE_CODE_CLASS (TREE_CODE (node));

                if (tclass == tcc_declaration)
                    {
                        if (DECL_NAME (node))
                            dump_decl_name (node, complex_dvalues, dvalue);
                        else
                            ppp_string (complex_dvalues, dvalue,
                                        "<unnamed type decl>");
                    }
                else if (tclass == tcc_type)
                    {
                        if (TYPE_NAME (node))
                            {
                                if (TREE_CODE (TYPE_NAME (node))
                                    == IDENTIFIER_NODE)
                                    {
                                        ppp_tree_identifier (complex_dvalues,
                                                             dvalue,
                                                             TYPE_NAME (node));
                                    }
                                else if (TREE_CODE (TYPE_NAME (node))
                                             == TYPE_DECL
                                         && DECL_NAME (TYPE_NAME (node)))
                                    {
                                        dump_decl_name (TYPE_NAME (node),
                                                        complex_dvalues,
                                                        dvalue);
                                    }
                                else
                                    {
                                        ppp_string (complex_dvalues, dvalue,
                                                    "<unnamed type>");
                                    }
                            }
                        else if (TREE_CODE (node) == VECTOR_TYPE)
                            {
                                ppp_string (complex_dvalues, dvalue, "vector");
                                ppp_left_paren (complex_dvalues, dvalue);
                                ppp_wide_integer (complex_dvalues, dvalue,
                                                  TYPE_VECTOR_SUBPARTS (node));
                                ppp_string (complex_dvalues, dvalue, ") ");
                                get_tree_data_values (TREE_TYPE (node),
                                                      complex_dvalues);
                            }
                        else if (TREE_CODE (node) == INTEGER_TYPE)
                            {
                                if (TYPE_PRECISION (node) == CHAR_TYPE_SIZE)
                                    {
                                        ppp_string (complex_dvalues, dvalue,
                                                    (TYPE_UNSIGNED (node)
                                                         ? "unsigned char"
                                                         : "signed char"));
                                    }
                                else if (TYPE_PRECISION (node)
                                         == SHORT_TYPE_SIZE)
                                    {
                                        ppp_string (complex_dvalues, dvalue,
                                                    (TYPE_UNSIGNED (node)
                                                         ? "unsigned short"
                                                         : "signed short"));
                                    }
                                else if (TYPE_PRECISION (node) == INT_TYPE_SIZE)
                                    {
                                        ppp_string (complex_dvalues, dvalue,
                                                    (TYPE_UNSIGNED (node)
                                                         ? "unsigned int"
                                                         : "signed int"));
                                    }
                                else if (TYPE_PRECISION (node)
                                         == LONG_TYPE_SIZE)
                                    {
                                        ppp_string (complex_dvalues, dvalue,
                                                    (TYPE_UNSIGNED (node)
                                                         ? "unsigned long"
                                                         : "signed long"));
                                    }
                                else if (TYPE_PRECISION (node)
                                         == LONG_LONG_TYPE_SIZE)
                                    {
                                        ppp_string (complex_dvalues, dvalue,
                                                    (TYPE_UNSIGNED (node)
                                                         ? "unsigned long long"
                                                         : "signed long long"));
                                    }
                                else if (TYPE_PRECISION (node) >= CHAR_TYPE_SIZE
                                         && pow2p_hwi (TYPE_PRECISION (node)))
                                    {
                                        ppp_string (complex_dvalues, dvalue,
                                                    (TYPE_UNSIGNED (node)
                                                         ? "uint"
                                                         : "int"));
                                        ppp_decimal_int (complex_dvalues,
                                                         dvalue,
                                                         TYPE_PRECISION (node));
                                        ppp_string (complex_dvalues, dvalue,
                                                    "_t");
                                    }
                                else
                                    {
                                        ppp_string (complex_dvalues, dvalue,
                                                    (TYPE_UNSIGNED (node)
                                                         ? "<unnamed-unsigned:"
                                                         : "<unnamed-signed:"));
                                        ppp_decimal_int (complex_dvalues,
                                                         dvalue,
                                                         TYPE_PRECISION (node));
                                        ppp_greater (complex_dvalues, dvalue);
                                    }
                            }
                        else if (TREE_CODE (node) == COMPLEX_TYPE)
                            {
                                ppp_string (complex_dvalues, dvalue,
                                            "__complex__ ");
                                get_tree_data_values (TREE_TYPE (node),
                                                      complex_dvalues);
                            }
                        else if (TREE_CODE (node) == REAL_TYPE)
                            {
                                ppp_string (complex_dvalues, dvalue, "<float:");
                                ppp_decimal_int (complex_dvalues, dvalue,
                                                 TYPE_PRECISION (node));
                                ppp_greater (complex_dvalues, dvalue);
                            }
                        else if (TREE_CODE (node) == FIXED_POINT_TYPE)
                            {
                                ppp_string (complex_dvalues, dvalue,
                                            "<fixed-point-");
                                ppp_string (complex_dvalues, dvalue,
                                            TYPE_SATURATING (node) ? "sat:"
                                                                   : "nonsat:");
                                ppp_decimal_int (complex_dvalues, dvalue,
                                                 TYPE_PRECISION (node));
                                ppp_greater (complex_dvalues, dvalue);
                            }
                        else if (TREE_CODE (node) == BOOLEAN_TYPE)
                            {
                                ppp_string (complex_dvalues, dvalue,
                                            (TYPE_UNSIGNED (node)
                                                 ? "<unsigned-boolean:"
                                                 : "<signed-boolean:"));
                                ppp_decimal_int (complex_dvalues, dvalue,
                                                 TYPE_PRECISION (node));
                                ppp_greater (complex_dvalues, dvalue);
                            }
                        else if (TREE_CODE (node) == VOID_TYPE)
                            {
                                ppp_string (complex_dvalues, dvalue, "void");
                            }

                        else
                            {
                                ppp_string (complex_dvalues, dvalue,
                                            "<unnamed type>");
                            }
                    }
                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case POINTER_TYPE:
        case REFERENCE_TYPE:
            {
                std::vector<data_value_t> complex_dvalues;
                const char *str
                    = (TREE_CODE (node) == POINTER_TYPE ? "*" : "&");

                if (TREE_TYPE (node) == NULL)
                    {
                        ppp_string (complex_dvalues, dvalue, str);
                        ppp_string (complex_dvalues, dvalue, "<null type>");
                    }
                else if (TREE_CODE (TREE_TYPE (node)) == FUNCTION_TYPE)
                    {
                        tree fnode = TREE_TYPE (node);

                        get_tree_data_values (TREE_TYPE (fnode),
                                              complex_dvalues);
                        ppp_space (complex_dvalues, dvalue);
                        ppp_left_paren (complex_dvalues, dvalue);
                        ppp_string (complex_dvalues, dvalue, str);
                        if (TYPE_IDENTIFIER (node))
                            {
                                get_tree_data_values (TYPE_NAME (node),
                                                      complex_dvalues);
                            }
                        else
                            {
                                append_simple_value (
                                    complex_dvalues, dvalue,
                                    "<T" + std::to_string (TYPE_UID (node))
                                        + ">");
                            }

                        ppp_right_paren (complex_dvalues, dvalue);
                        dump_function_declaration (fnode, complex_dvalues,
                                                   dvalue);
                    }
                else
                    {
                        unsigned int quals = TYPE_QUALS (node);

                        get_tree_data_values (TREE_TYPE (node),
                                              complex_dvalues);
                        ppp_space (complex_dvalues, dvalue);
                        ppp_string (complex_dvalues, dvalue, str);

                        if (quals & TYPE_QUAL_CONST)
                            ppp_string (complex_dvalues, dvalue, " const");
                        if (quals & TYPE_QUAL_VOLATILE)
                            ppp_string (complex_dvalues, dvalue, " volatile");
                        if (quals & TYPE_QUAL_RESTRICT)
                            ppp_string (complex_dvalues, dvalue, " restrict");

                        if (!ADDR_SPACE_GENERIC_P (TYPE_ADDR_SPACE (node)))
                            {
                                append_simple_value (
                                    complex_dvalues, dvalue,
                                    "<address-space-"
                                        + std::to_string (
                                            TYPE_ADDR_SPACE (node))
                                        + ">");
                            }

                        if (TYPE_REF_CAN_ALIAS_ALL (node))
                            ppp_string (complex_dvalues, dvalue, " {ref-all}");
                    }
                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case OFFSET_TYPE:
            {
                std::vector<data_value_t> complex_dvalues;
                do_niy (node, complex_dvalues, dvalue);
                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case MEM_REF:
        case TARGET_MEM_REF:
            {
                std::vector<data_value_t> complex_dvalues;
                dump_mem_ref (node, complex_dvalues, dvalue);
                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case ARRAY_TYPE:
            {
                std::vector<data_value_t> complex_dvalues;
                unsigned int quals = TYPE_QUALS (node);
                tree tmp;

                if (quals & TYPE_QUAL_ATOMIC)
                    ppp_string (complex_dvalues, dvalue, "atomic ");
                if (quals & TYPE_QUAL_CONST)
                    ppp_string (complex_dvalues, dvalue, "const ");
                if (quals & TYPE_QUAL_VOLATILE)
                    ppp_string (complex_dvalues, dvalue, "volatile ");

                /* Print the innermost component type.  */
                for (tmp = TREE_TYPE (node); TREE_CODE (tmp) == ARRAY_TYPE;
                     tmp = TREE_TYPE (tmp))
                    ;

                /* Avoid to print recursively the array.  */
                /* FIXME : Not implemented correctly, see print_struct_decl.  */
                if (TREE_CODE (tmp) != POINTER_TYPE || TREE_TYPE (tmp) != node)
                    get_tree_data_values (tmp, complex_dvalues);

                /* Print the dimensions.  */
                for (tmp = node; TREE_CODE (tmp) == ARRAY_TYPE;
                     tmp = TREE_TYPE (tmp))
                    {
                        get_tree_data_values (TYPE_DOMAIN (tmp),
                                              complex_dvalues);
                    }

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case RECORD_TYPE:
        case UNION_TYPE:
        case QUAL_UNION_TYPE:
            {
                std::vector<data_value_t> complex_dvalues;
                unsigned int quals = TYPE_QUALS (node);

                if (quals & TYPE_QUAL_ATOMIC)
                    ppp_string (complex_dvalues, dvalue, "atomic ");
                if (quals & TYPE_QUAL_CONST)
                    ppp_string (complex_dvalues, dvalue, "const ");
                if (quals & TYPE_QUAL_VOLATILE)
                    ppp_string (complex_dvalues, dvalue, "volatile ");

                if (!ADDR_SPACE_GENERIC_P (TYPE_ADDR_SPACE (node)))
                    {
                        ppp_string (
                            complex_dvalues, dvalue,
                            "<address-space-"
                                + std::to_string (TYPE_ADDR_SPACE (node))
                                + "> ");
                    }

                /* Print the name of the structure.  */
                if (TREE_CODE (node) == RECORD_TYPE)
                    {
                        ppp_string (complex_dvalues, dvalue, "struct");
                    }
                else if (TREE_CODE (node) == UNION_TYPE)
                    {
                        ppp_string (complex_dvalues, dvalue, "union");
                    }

                if (TYPE_NAME (node))
                    get_tree_data_values (TYPE_NAME (node), complex_dvalues);
                // else if (!(flags & TDF_SLIM))
                //     /* FIXME: If we eliminate the 'else' above and attempt
                //        to show the fields for named types, we may get stuck
                //        following a cycle of pointers to structs.  The alleged
                //        self-reference check in print_struct_decl will not
                //        detect cycles involving more than one pointer or
                //        struct type.  */
                //     print_struct_decl(pp, node, spc, flags);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case LANG_TYPE:
            {
                std::vector<data_value_t> complex_dvalues;
                do_niy (node, complex_dvalues, dvalue);
                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case INTEGER_CST:
            {
                std::vector<data_value_t> complex_dvalues;

                if ((POINTER_TYPE_P (TREE_TYPE (node))
                     || (TYPE_PRECISION (TREE_TYPE (node))
                         < TYPE_PRECISION (integer_type_node))
                     || exact_log2 (TYPE_PRECISION (TREE_TYPE (node))) == -1
                     || tree_int_cst_sgn (node) < 0))
                    {
                        ppp_string (complex_dvalues, dvalue, "_Literal (");
                        get_tree_data_values (TREE_TYPE (node),
                                              complex_dvalues);
                        ppp_string (complex_dvalues, dvalue, ") ");
                    }
                if (TREE_CODE (TREE_TYPE (node)) == POINTER_TYPE)
                    {
                        /* In the case of a pointer, one may want to divide by
                           the size of the pointed-to type.  Unfortunately, this
                           not straightforward.  The C front-end maps
                           expressions

                           (int *) 5
                           int *p; (p + 5)

                           in such a way that the two INTEGER_CST nodes for "5"
                           have different values but identical types.  In the
                           latter case, the 5 is multiplied by sizeof (int) in
                           c-common.cc (pointer_int_sum) to convert it to a byte
                           address, and yet the type of the node is left
                           unchanged.  Argh.  What is consistent though is that
                           the number value corresponds to bytes (UNITS) offset.

                           NB: Neither of the following divisors can be
                           trivially used to recover the original literal:

                           TREE_INT_CST_LOW (TYPE_SIZE_UNIT (TREE_TYPE (node)))
                           TYPE_PRECISION (TREE_TYPE (TREE_TYPE (node)))  */
                        append_simple_value (
                            complex_dvalues, dvalue,
                            std::to_string (TREE_INT_CST_LOW (node)));
                        ppp_string (complex_dvalues, dvalue,
                                    "B"); /* pseudo-unit */
                    }
                else if (tree_fits_shwi_p (node))
                    {
                        append_simple_value (
                            complex_dvalues, dvalue,
                            std::to_string (tree_to_shwi (node)));
                    }
                else if (tree_fits_uhwi_p (node))
                    {
                        append_simple_value (
                            complex_dvalues, dvalue,
                            std::to_string (tree_to_uhwi (node)));
                    }
                else
                    {
                        wide_int val = wi::to_wide (node);

                        if (wi::neg_p (val, TYPE_SIGN (TREE_TYPE (node))))
                            {
                                ppp_minus (complex_dvalues, dvalue);
                                val = -val;
                            }
                        char *buf = NULL;
                        print_hex (val, buf);
                        ppp_string (complex_dvalues, dvalue, std::string (buf));
                    }
                if (!(POINTER_TYPE_P (TREE_TYPE (node))
                      || (TYPE_PRECISION (TREE_TYPE (node))
                          < TYPE_PRECISION (integer_type_node))
                      || exact_log2 (TYPE_PRECISION (TREE_TYPE (node))) == -1))
                    {
                        if (TYPE_UNSIGNED (TREE_TYPE (node)))
                            {
                                ppp_string (complex_dvalues, dvalue, "u");
                            }

                        if (TYPE_PRECISION (TREE_TYPE (node))
                            == TYPE_PRECISION (unsigned_type_node))
                            {
                                ;
                            }
                        else if (TYPE_PRECISION (TREE_TYPE (node))
                                 == TYPE_PRECISION (long_unsigned_type_node))
                            {
                                ppp_string (complex_dvalues, dvalue, "l");
                            }
                        else if (TYPE_PRECISION (TREE_TYPE (node))
                                 == TYPE_PRECISION (
                                     long_long_unsigned_type_node))
                            {
                                ppp_string (complex_dvalues, dvalue, "ll");
                            }
                    }
                if (TREE_OVERFLOW (node))
                    {
                        ppp_string (complex_dvalues, dvalue, "(OVF)");
                    }

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case POLY_INT_CST:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "POLY_INT_CST [");
                get_tree_data_values (POLY_INT_CST_COEFF (node, 0),
                                      complex_dvalues);

                for (unsigned int i = 1; i < NUM_POLY_INT_COEFFS; ++i)
                    {
                        ppp_string (complex_dvalues, dvalue, ", ");
                        get_tree_data_values (POLY_INT_CST_COEFF (node, i),
                                              complex_dvalues);
                    }
                ppp_string (complex_dvalues, dvalue, "]");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case REAL_CST:
            {
                std::vector<data_value_t> complex_dvalues;

                /* Code copied from print_node.  */

                REAL_VALUE_TYPE d;
                if (TREE_OVERFLOW (node))
                    ppp_string (complex_dvalues, dvalue, " overflow");

                d = TREE_REAL_CST (node);
                if (REAL_VALUE_ISINF (d))
                    {
                        ppp_string (complex_dvalues, dvalue,
                                    REAL_VALUE_NEGATIVE (d) ? " -Inf" : " Inf");
                    }

                else if (REAL_VALUE_ISNAN (d))
                    {
                        ppp_string (complex_dvalues, dvalue, " Nan");
                    }

                else
                    {
                        char string[100];
                        real_to_decimal (string, &d, sizeof (string), 0, 1);
                        ppp_string (complex_dvalues, dvalue, string);
                    }

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case FIXED_CST:
            {
                char string[100];
                fixed_to_decimal (string, TREE_FIXED_CST_PTR (node),
                                  sizeof (string));
                ppp_string (dvalues, dvalue, string);
                break;
            }

        case COMPLEX_CST:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "__complex__ (");
                get_tree_data_values (TREE_REALPART (node), complex_dvalues);

                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_IMAGPART (node), complex_dvalues);

                ppp_right_paren (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case STRING_CST:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "\"");
                if (unsigned nbytes = TREE_STRING_LENGTH (node))
                    {
                        pretty_print_string (complex_dvalues, dvalue,
                                             TREE_STRING_POINTER (node),
                                             nbytes);
                    }
                ppp_string (complex_dvalues, dvalue, "\"");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case VECTOR_CST:
            {
                std::vector<data_value_t> complex_dvalues;

                unsigned i;
                ppp_string (complex_dvalues, dvalue, "_Literal (");
                get_tree_data_values (TREE_TYPE (node), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ") ");

                ppp_string (complex_dvalues, dvalue, "{ ");
                unsigned HOST_WIDE_INT nunits;
                if (!VECTOR_CST_NELTS (node).is_constant (&nunits))
                    {
                        nunits = vector_cst_encoded_nelts (node);
                    }
                for (i = 0; i < nunits; ++i)
                    {
                        if (i != 0)
                            {
                                ppp_string (complex_dvalues, dvalue, ", ");
                            }
                        get_tree_data_values (VECTOR_CST_ELT (node, i),
                                              complex_dvalues);
                    }
                if (!VECTOR_CST_NELTS (node).is_constant ())
                    {
                        ppp_string (complex_dvalues, dvalue, ", ...");
                    }
                ppp_string (complex_dvalues, dvalue, " }");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case FUNCTION_TYPE:
        case METHOD_TYPE:
            {
                std::vector<data_value_t> complex_dvalues;

                get_tree_data_values (TREE_TYPE (node), complex_dvalues);
                ppp_space (complex_dvalues, dvalue);
                if (TREE_CODE (node) == METHOD_TYPE)
                    {
                        if (TYPE_METHOD_BASETYPE (node))
                            {
                                get_tree_data_values (
                                    TYPE_NAME (TYPE_METHOD_BASETYPE (node)),
                                    complex_dvalues);
                            }
                        else
                            {
                                ppp_string (complex_dvalues, dvalue,
                                            "<null method basetype>");
                            }
                        ppp_colon_colon (complex_dvalues, dvalue);
                    }
                if (TYPE_IDENTIFIER (node))
                    {
                        get_tree_data_values (TYPE_NAME (node),
                                              complex_dvalues);
                    }
                else if (TYPE_NAME (node) && DECL_NAME (TYPE_NAME (node)))
                    {
                        dump_decl_name (TYPE_NAME (node), complex_dvalues,
                                        dvalue);
                    }
                else
                    {
                        ppp_string (complex_dvalues, dvalue,
                                    "<T" + std::to_string (TYPE_UID (node))
                                        + ">");
                    }
                dump_function_declaration (node, complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case FUNCTION_DECL:
        case CONST_DECL:
            {
                std::vector<data_value_t> complex_dvalues;
                dump_decl_name (node, complex_dvalues, dvalue);
                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case LABEL_DECL:
            {
                std::vector<data_value_t> complex_dvalues;

                if (DECL_NAME (node))
                    {
                        dump_decl_name (node, complex_dvalues, dvalue);
                    }
                else if (LABEL_DECL_UID (node) != -1)
                    {
                        ppp_string (
                            complex_dvalues, dvalue,
                            "<L" + std::to_string ((int)LABEL_DECL_UID (node))
                                + ">");
                    }
                else
                    {
                        ppp_string (complex_dvalues, dvalue,
                                    "<D" + std::to_string (DECL_UID (node))
                                        + ">");
                    }
                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case TYPE_DECL:
            {
                std::vector<data_value_t> complex_dvalues;

                if (DECL_IS_UNDECLARED_BUILTIN (node))
                    {
                        /* Don't print the declaration of built-in types.  */
                        break;
                    }
                if (DECL_NAME (node))
                    dump_decl_name (node, complex_dvalues, dvalue);
                else if (TYPE_NAME (TREE_TYPE (node)) != node)
                    {
                        ppp_string (complex_dvalues, dvalue,
                                    (TREE_CODE (TREE_TYPE (node)) == UNION_TYPE
                                         ? "union"
                                         : "struct "));
                        get_tree_data_values (TREE_TYPE (node),
                                              complex_dvalues);
                    }
                else
                    ppp_string (complex_dvalues, dvalue, "<anon>");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case VAR_DECL:
        case PARM_DECL:
        case FIELD_DECL:
        case DEBUG_EXPR_DECL:
        case NAMESPACE_DECL:
        case NAMELIST_DECL:
            {
                std::vector<data_value_t> complex_dvalues;
                dump_decl_name (node, complex_dvalues, dvalue);
                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case RESULT_DECL:
            {
                std::vector<data_value_t> complex_dvalues;
                ppp_string (complex_dvalues, dvalue, "<retval>");
                dump_decl_name (node, complex_dvalues, dvalue);
                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case COMPONENT_REF:
            {
                std::vector<data_value_t> complex_dvalues;

                tree op0 = TREE_OPERAND (node, 0);
                std::string str = ".";
                if (op0
                    && (TREE_CODE (op0) == INDIRECT_REF
                        || (TREE_CODE (op0) == MEM_REF
                            && TREE_CODE (TREE_OPERAND (op0, 0)) != ADDR_EXPR
                            && integer_zerop (TREE_OPERAND (op0, 1))
                            /* Dump the types of INTEGER_CSTs explicitly, for we
                               can't infer them and MEM_ATTR caching will share
                               MEM_REFs with differently-typed op0s.  */
                            && TREE_CODE (TREE_OPERAND (op0, 0)) != INTEGER_CST
                            /* Released SSA_NAMES have no TREE_TYPE.  */
                            && TREE_TYPE (TREE_OPERAND (op0, 0)) != NULL_TREE
                            /* Same pointer types, but ignoring POINTER_TYPE vs.
                               REFERENCE_TYPE.  */
                            && (TREE_TYPE (TREE_TYPE (TREE_OPERAND (op0, 0)))
                                == TREE_TYPE (
                                    TREE_TYPE (TREE_OPERAND (op0, 1))))
                            && (TYPE_MODE (TREE_TYPE (TREE_OPERAND (op0, 0)))
                                == TYPE_MODE (
                                    TREE_TYPE (TREE_OPERAND (op0, 1))))
                            && (TYPE_REF_CAN_ALIAS_ALL (
                                    TREE_TYPE (TREE_OPERAND (op0, 0)))
                                == TYPE_REF_CAN_ALIAS_ALL (
                                    TREE_TYPE (TREE_OPERAND (op0, 1))))
                            /* Same value types ignoring qualifiers.  */
                            && (TYPE_MAIN_VARIANT (TREE_TYPE (op0))
                                == TYPE_MAIN_VARIANT (TREE_TYPE (
                                    TREE_TYPE (TREE_OPERAND (op0, 1)))))
                            && MR_DEPENDENCE_CLIQUE (op0) == 0)))
                    {
                        op0 = TREE_OPERAND (op0, 0);
                        str = "->";
                    }
                if (op_prio (op0) < op_prio (node))
                    {
                        ppp_left_paren (complex_dvalues, dvalue);
                    }

                get_tree_data_values (op0, complex_dvalues);

                if (op_prio (op0) < op_prio (node))
                    {
                        ppp_right_paren (complex_dvalues, dvalue);
                    }

                ppp_string (complex_dvalues, dvalue, str);

                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);

                op0 = component_ref_field_offset (node);

                if (op0 && TREE_CODE (op0) != INTEGER_CST)
                    {
                        ppp_string (complex_dvalues, dvalue, "{off: ");
                        get_tree_data_values (op0, complex_dvalues);
                        ppp_right_brace (complex_dvalues, dvalue);
                    }

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case BIT_FIELD_REF:
            {
                std::vector<data_value_t> complex_dvalues;
                ppp_string (complex_dvalues, dvalue, "BIT_FIELD_REF <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 2), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);
                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case BIT_INSERT_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "BIT_INSERT_EXPR <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 2), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, " (");
                if (INTEGRAL_TYPE_P (TREE_TYPE (TREE_OPERAND (node, 1))))
                    {
                        ppp_decimal_int (complex_dvalues, dvalue,
                                         TYPE_PRECISION (TREE_TYPE (
                                             TREE_OPERAND (node, 1))));
                    }
                else
                    {
                        get_tree_data_values (
                            TYPE_SIZE (TREE_TYPE (TREE_OPERAND (node, 1))),
                            complex_dvalues);
                    }
                ppp_string (complex_dvalues, dvalue, " bits)>");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case ARRAY_REF:
        case ARRAY_RANGE_REF:
            {
                std::vector<data_value_t> complex_dvalues;

                tree op0 = TREE_OPERAND (node, 0);
                if (op_prio (op0) < op_prio (node))
                    {
                        ppp_left_paren (complex_dvalues, dvalue);
                    }
                get_tree_data_values (op0, complex_dvalues);

                if (op_prio (op0) < op_prio (node))
                    {
                        ppp_right_paren (complex_dvalues, dvalue);
                    }
                ppp_left_bracket (complex_dvalues, dvalue);
                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);
                if (TREE_CODE (node) == ARRAY_RANGE_REF)
                    {
                        ppp_string (complex_dvalues, dvalue, " ...");
                    }
                ppp_right_bracket (complex_dvalues, dvalue);

                op0 = array_ref_low_bound (node);
                tree op1 = array_ref_element_size (node);

                if (!integer_zerop (op0) || TREE_OPERAND (node, 2)
                    || TREE_OPERAND (node, 3))
                    {
                        ppp_string (complex_dvalues, dvalue, "{lb: ");
                        get_tree_data_values (op0, complex_dvalues);
                        ppp_string (complex_dvalues, dvalue, " sz: ");
                        get_tree_data_values (op1, complex_dvalues);
                        ppp_right_brace (complex_dvalues, dvalue);
                    }

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case CONSTRUCTOR:
            {
                std::vector<data_value_t> complex_dvalues;

                unsigned HOST_WIDE_INT ix;
                tree field, val;
                bool is_struct_init = false;
                bool is_array_init = false;
                widest_int curidx;

                ppp_string (complex_dvalues, dvalue, "_Literal (");
                get_tree_data_values (TREE_TYPE (node), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ") ");

                ppp_left_brace (complex_dvalues, dvalue);
                if (TREE_CLOBBER_P (node))
                    {
                        ppp_string (complex_dvalues, dvalue, "CLOBBER");
                        if (CLOBBER_KIND (node) == CLOBBER_EOL)
                            {
                                ppp_string (complex_dvalues, dvalue, "(eol)");
                            }
                    }
                else if (TREE_CODE (TREE_TYPE (node)) == RECORD_TYPE
                         || TREE_CODE (TREE_TYPE (node)) == UNION_TYPE)
                    is_struct_init = true;
                else if (TREE_CODE (TREE_TYPE (node)) == ARRAY_TYPE
                         && TYPE_DOMAIN (TREE_TYPE (node))
                         && TYPE_MIN_VALUE (TYPE_DOMAIN (TREE_TYPE (node)))
                         && TREE_CODE (
                                TYPE_MIN_VALUE (TYPE_DOMAIN (TREE_TYPE (node))))
                                == INTEGER_CST)
                    {
                        tree minv
                            = TYPE_MIN_VALUE (TYPE_DOMAIN (TREE_TYPE (node)));
                        is_array_init = true;
                        curidx = wi::to_widest (minv);
                    }
                FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (node), ix, field,
                                          val)
                {
                    if (field)
                        {
                            if (is_struct_init)
                                {
                                    ppp_dot (complex_dvalues, dvalue);
                                    get_tree_data_values (field,
                                                          complex_dvalues);
                                    ppp_equal (complex_dvalues, dvalue);
                                }
                            else if (is_array_init
                                     && (TREE_CODE (field) != INTEGER_CST
                                         || curidx != wi::to_widest (field)))
                                {
                                    ppp_left_bracket (complex_dvalues, dvalue);
                                    if (TREE_CODE (field) == RANGE_EXPR)
                                        {
                                            get_tree_data_values (
                                                TREE_OPERAND (field, 0),
                                                complex_dvalues);
                                            ppp_string (complex_dvalues, dvalue,
                                                        " ... ");
                                            get_tree_data_values (
                                                TREE_OPERAND (field, 1),
                                                complex_dvalues);

                                            if (TREE_CODE (
                                                    TREE_OPERAND (field, 1))
                                                == INTEGER_CST)
                                                {
                                                    curidx = wi::to_widest (
                                                        TREE_OPERAND (field,
                                                                      1));
                                                }
                                        }
                                    else
                                        {
                                            get_tree_data_values (
                                                field, complex_dvalues);
                                        }
                                    if (TREE_CODE (field) == INTEGER_CST)
                                        {
                                            curidx = wi::to_widest (field);
                                        }
                                    ppp_string (complex_dvalues, dvalue, "]=");
                                }
                        }
                    if (is_array_init)
                        {
                            curidx += 1;
                        }
                    if (val && TREE_CODE (val) == ADDR_EXPR)
                        {
                            if (TREE_CODE (TREE_OPERAND (val, 0))
                                == FUNCTION_DECL)
                                {
                                    val = TREE_OPERAND (val, 0);
                                }
                        }

                    if (val && TREE_CODE (val) == FUNCTION_DECL)
                        {
                            dump_decl_name (val, complex_dvalues, dvalue);
                        }
                    else
                        {
                            get_tree_data_values (val, complex_dvalues);
                        }
                    if (ix != CONSTRUCTOR_NELTS (node) - 1)
                        {
                            ppp_comma (complex_dvalues, dvalue);
                            ppp_space (complex_dvalues, dvalue);
                        }
                }
                ppp_right_brace (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case COMPOUND_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                tree *tp;

                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);

                ppp_comma (complex_dvalues, dvalue);
                ppp_space (complex_dvalues, dvalue);

                for (tp = &TREE_OPERAND (node, 1);
                     TREE_CODE (*tp) == COMPOUND_EXPR;
                     tp = &TREE_OPERAND (*tp, 1))
                    {
                        get_tree_data_values (TREE_OPERAND (*tp, 0),
                                              complex_dvalues);

                        ppp_comma (complex_dvalues, dvalue);
                        ppp_space (complex_dvalues, dvalue);
                    }

                get_tree_data_values (*tp, complex_dvalues);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case STATEMENT_LIST:
            {
                std::vector<data_value_t> complex_dvalues;
                tree_stmt_iterator si;
                // bool first = true;

                for (si = tsi_start (node); !tsi_end_p (si); tsi_next (&si))
                    {
                        // if (!first)
                        //     newline_and_indent(pp, spc);
                        // else
                        //     first = false;
                        get_tree_data_values (tsi_stmt (si), complex_dvalues);
                    }

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case MODIFY_EXPR:
        case INIT_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_space (complex_dvalues, dvalue);
                ppp_equal (complex_dvalues, dvalue);
                ppp_space (complex_dvalues, dvalue);
                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case TARGET_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "TARGET_EXPR <");
                get_tree_data_values (TARGET_EXPR_SLOT (node), complex_dvalues);
                ppp_comma (complex_dvalues, dvalue);
                ppp_space (complex_dvalues, dvalue);
                get_tree_data_values (TARGET_EXPR_INITIAL (node),
                                      complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case DECL_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                print_declaration (DECL_EXPR_DECL (node), complex_dvalues,
                                   dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case COND_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                if (TREE_TYPE (node) == NULL
                    || TREE_TYPE (node) == void_type_node)
                    {
                        ppp_string (complex_dvalues, dvalue, "if (");
                        get_tree_data_values (COND_EXPR_COND (node),
                                              complex_dvalues);
                        ppp_right_paren (complex_dvalues, dvalue);
                        /* The lowered cond_exprs should always be printed in
                         * full.  */
                        if (COND_EXPR_THEN (node)
                            && (IS_EMPTY_STMT (COND_EXPR_THEN (node))
                                || TREE_CODE (COND_EXPR_THEN (node))
                                       == GOTO_EXPR)
                            && COND_EXPR_ELSE (node)
                            && (IS_EMPTY_STMT (COND_EXPR_ELSE (node))
                                || TREE_CODE (COND_EXPR_ELSE (node))
                                       == GOTO_EXPR))
                            {
                                ppp_space (complex_dvalues, dvalue);
                                get_tree_data_values (COND_EXPR_THEN (node),
                                                      complex_dvalues);
                                if (!IS_EMPTY_STMT (COND_EXPR_ELSE (node)))
                                    {
                                        ppp_string (complex_dvalues, dvalue,
                                                    " else ");
                                        get_tree_data_values (
                                            COND_EXPR_ELSE (node),
                                            complex_dvalues);
                                    }
                            }

                        /* Output COND_EXPR_THEN.  */
                        if (COND_EXPR_THEN (node))
                            {
                                // newline_and_indent(complex_dvalues, dvalue,
                                // spc + 2);
                                ppp_left_brace (complex_dvalues, dvalue);
                                // newline_and_indent(complex_dvalues, dvalue,
                                // spc + 4);
                                get_tree_data_values (COND_EXPR_THEN (node),
                                                      complex_dvalues);
                                // newline_and_indent(complex_dvalues, dvalue,
                                // spc + 2);
                                ppp_right_brace (complex_dvalues, dvalue);
                            }

                        /* Output COND_EXPR_ELSE.  */
                        if (COND_EXPR_ELSE (node)
                            && !IS_EMPTY_STMT (COND_EXPR_ELSE (node)))
                            {
                                // newline_and_indent(pp, spc);
                                ppp_string (complex_dvalues, dvalue, "else");
                                // newline_and_indent(pp, spc + 2);
                                ppp_left_brace (complex_dvalues, dvalue);
                                // newline_and_indent(complex_dvalues, dvalue,
                                // spc + 4);
                                get_tree_data_values (COND_EXPR_ELSE (node),
                                                      complex_dvalues);
                                // newline_and_indent(complex_dvalues, dvalue,
                                // spc + 2);
                                ppp_right_brace (complex_dvalues, dvalue);
                            }
                        // is_expr = false;
                    }
                else
                    {
                        get_tree_data_values (TREE_OPERAND (node, 0),
                                              complex_dvalues);
                        ppp_space (complex_dvalues, dvalue);
                        ppp_question (complex_dvalues, dvalue);
                        ppp_space (complex_dvalues, dvalue);
                        get_tree_data_values (TREE_OPERAND (node, 1),
                                              complex_dvalues);
                        ppp_space (complex_dvalues, dvalue);
                        ppp_colon (complex_dvalues, dvalue);
                        ppp_space (complex_dvalues, dvalue);
                        get_tree_data_values (TREE_OPERAND (node, 2),
                                              complex_dvalues);
                    }

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case BIND_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_left_brace (complex_dvalues, dvalue);

                if (BIND_EXPR_VARS (node))
                    {
                        // ppp_newline(complex_dvalues, dvalue);

                        for (tree op0 = BIND_EXPR_VARS (node); op0;
                             op0 = DECL_CHAIN (op0))
                            {
                                print_declaration (op0, complex_dvalues,
                                                   dvalue);
                                // ppp_newline(complex_dvalues, dvalue);
                            }
                    }

                // newline_and_indent(pp, spc + 2);
                get_tree_data_values (BIND_EXPR_BODY (node), complex_dvalues);
                // newline_and_indent(pp, spc);
                ppp_right_brace (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case CALL_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                if (CALL_EXPR_FN (node) != NULL_TREE)
                    print_call_name (CALL_EXPR_FN (node), complex_dvalues,
                                     dvalue);
                else
                    {
                        ppp_dot (complex_dvalues, dvalue);
                        ppp_string (complex_dvalues, dvalue,
                                    internal_fn_name (CALL_EXPR_IFN (node)));
                    }

                /* Print parameters.  */
                ppp_space (complex_dvalues, dvalue);
                ppp_left_paren (complex_dvalues, dvalue);
                {
                    tree arg;
                    call_expr_arg_iterator iter;
                    FOR_EACH_CALL_EXPR_ARG (arg, iter, node)
                    {
                        get_tree_data_values (arg, complex_dvalues);
                        if (more_call_expr_args_p (&iter))
                            {
                                ppp_comma (complex_dvalues, dvalue);
                                ppp_space (complex_dvalues, dvalue);
                            }
                    }
                }
                if (CALL_EXPR_VA_ARG_PACK (node))
                    {
                        if (call_expr_nargs (node) > 0)
                            {
                                ppp_comma (complex_dvalues, dvalue);
                                ppp_space (complex_dvalues, dvalue);
                            }
                        ppp_string (complex_dvalues, dvalue,
                                    "__builtin_va_arg_pack ()");
                    }
                ppp_right_paren (complex_dvalues, dvalue);

                tree op1 = CALL_EXPR_STATIC_CHAIN (node);
                if (op1)
                    {
                        ppp_string (complex_dvalues, dvalue,
                                    " [static-chain: ");
                        get_tree_data_values (op1, complex_dvalues);
                        ppp_right_bracket (complex_dvalues, dvalue);
                    }

                if (CALL_EXPR_RETURN_SLOT_OPT (node))
                    ppp_string (complex_dvalues, dvalue,
                                " [return slot optimization]");
                if (CALL_EXPR_TAILCALL (node))
                    ppp_string (complex_dvalues, dvalue, " [tail call]");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case WITH_CLEANUP_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;
                do_niy (node, complex_dvalues, dvalue);
                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case CLEANUP_POINT_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "<<cleanup_point ");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ">>");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case PLACEHOLDER_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "<PLACEHOLDER_EXPR ");
                get_tree_data_values (TREE_TYPE (node), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        /* Binary arithmetic and logic expressions.  */
        // case WIDEN_PLUS_EXPR:
        // case WIDEN_MINUS_EXPR:
        case WIDEN_SUM_EXPR:
        case WIDEN_MULT_EXPR:
        case MULT_EXPR:
        case MULT_HIGHPART_EXPR:
        case PLUS_EXPR:
        case POINTER_PLUS_EXPR:
        case POINTER_DIFF_EXPR:
        case MINUS_EXPR:
        case TRUNC_DIV_EXPR:
        case CEIL_DIV_EXPR:
        case FLOOR_DIV_EXPR:
        case ROUND_DIV_EXPR:
        case TRUNC_MOD_EXPR:
        case CEIL_MOD_EXPR:
        case FLOOR_MOD_EXPR:
        case ROUND_MOD_EXPR:
        case RDIV_EXPR:
        case EXACT_DIV_EXPR:
        case LSHIFT_EXPR:
        case RSHIFT_EXPR:
        case LROTATE_EXPR:
        case RROTATE_EXPR:
        case WIDEN_LSHIFT_EXPR:
        case BIT_IOR_EXPR:
        case BIT_XOR_EXPR:
        case BIT_AND_EXPR:
        case TRUTH_ANDIF_EXPR:
        case TRUTH_ORIF_EXPR:
        case TRUTH_AND_EXPR:
        case TRUTH_OR_EXPR:
        case TRUTH_XOR_EXPR:
        case LT_EXPR:
        case LE_EXPR:
        case GT_EXPR:
        case GE_EXPR:
        case EQ_EXPR:
        case NE_EXPR:
        case UNLT_EXPR:
        case UNLE_EXPR:
        case UNGT_EXPR:
        case UNGE_EXPR:
        case UNEQ_EXPR:
        case LTGT_EXPR:
        case ORDERED_EXPR:
        case UNORDERED_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                const char *op = op_symbol (node);
                tree op0 = TREE_OPERAND (node, 0);
                tree op1 = TREE_OPERAND (node, 1);

                /* When the operands are expressions with less priority,
                   keep semantics of the tree representation.  */
                if (op_prio (op0) <= op_prio (node))
                    {
                        ppp_left_paren (complex_dvalues, dvalue);
                        get_tree_data_values (op0, complex_dvalues);
                        ppp_right_paren (complex_dvalues, dvalue);
                    }
                else
                    {
                        get_tree_data_values (op0, complex_dvalues);
                    }

                ppp_space (complex_dvalues, dvalue);
                ppp_string (complex_dvalues, dvalue, op);
                ppp_space (complex_dvalues, dvalue);

                /* When the operands are expressions with less priority,
                   keep semantics of the tree representation.  */
                if (op_prio (op1) <= op_prio (node))
                    {
                        ppp_left_paren (complex_dvalues, dvalue);
                        get_tree_data_values (op1, complex_dvalues);
                        ppp_right_paren (complex_dvalues, dvalue);
                    }
                else
                    {
                        get_tree_data_values (op1, complex_dvalues);
                    }

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case NEGATE_EXPR:
        case BIT_NOT_EXPR:
        case TRUTH_NOT_EXPR:
        case ADDR_EXPR:
        case PREDECREMENT_EXPR:
        case PREINCREMENT_EXPR:
        case INDIRECT_REF:
            {
                std::vector<data_value_t> complex_dvalues;

                if (TREE_CODE (node) == ADDR_EXPR
                    && (TREE_CODE (TREE_OPERAND (node, 0)) == STRING_CST
                        || TREE_CODE (TREE_OPERAND (node, 0)) == FUNCTION_DECL))
                    ; /* Do not output '&' for strings and function pointers. */
                else
                    {
                        ppp_string (complex_dvalues, dvalue, op_symbol (node));
                    }

                if (op_prio (TREE_OPERAND (node, 0)) < op_prio (node))
                    {
                        ppp_left_paren (complex_dvalues, dvalue);
                        get_tree_data_values (TREE_OPERAND (node, 0),
                                              complex_dvalues);
                        ppp_right_paren (complex_dvalues, dvalue);
                    }
                else
                    {
                        get_tree_data_values (TREE_OPERAND (node, 0),
                                              complex_dvalues);
                    }

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case POSTDECREMENT_EXPR:
        case POSTINCREMENT_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                if (op_prio (TREE_OPERAND (node, 0)) < op_prio (node))
                    {
                        ppp_left_paren (complex_dvalues, dvalue);
                        get_tree_data_values (TREE_OPERAND (node, 0),
                                              complex_dvalues);
                        ppp_right_paren (complex_dvalues, dvalue);
                    }
                else
                    {
                        get_tree_data_values (TREE_OPERAND (node, 0),
                                              complex_dvalues);
                    }
                ppp_string (complex_dvalues, dvalue, op_symbol (node));

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case MIN_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "MIN_EXPR <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case MAX_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "MAX_EXPR <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case ABS_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "ABS_EXPR <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case ABSU_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "ABSU_EXPR <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case RANGE_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;
                do_niy (node, complex_dvalues, dvalue);
                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case ADDR_SPACE_CONVERT_EXPR:
        case FIXED_CONVERT_EXPR:
        case FIX_TRUNC_EXPR:
        case FLOAT_EXPR:
        CASE_CONVERT:
            {
                std::vector<data_value_t> complex_dvalues;

                tree type = TREE_TYPE (node);
                tree op0 = TREE_OPERAND (node, 0);
                if (type != TREE_TYPE (op0))
                    {
                        ppp_left_paren (complex_dvalues, dvalue);
                        get_tree_data_values (type, complex_dvalues);
                        ppp_string (complex_dvalues, dvalue, ") ");
                    }
                if (op_prio (op0) < op_prio (node))
                    {
                        ppp_left_paren (complex_dvalues, dvalue);
                    }
                get_tree_data_values (op0, complex_dvalues);
                if (op_prio (op0) < op_prio (node))
                    {
                        ppp_right_paren (complex_dvalues, dvalue);
                    }

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case VIEW_CONVERT_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "VIEW_CONVERT_EXPR<");

                get_tree_data_values (TREE_TYPE (node), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ">(");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_right_paren (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case PAREN_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "((");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, "))");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case NON_LVALUE_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "NON_LVALUE_EXPR <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case SAVE_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "SAVE_EXPR <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case COMPLEX_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "COMPLEX_EXPR <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case CONJ_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "CONJ_EXPR <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case REALPART_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "REALPART_EXPR <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case IMAGPART_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "IMAGPART_EXPR <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case VA_ARG_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "VA_ARG_EXPR <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case TRY_FINALLY_EXPR:
            
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "try");
                // newline_and_indent(pp, spc + 2);
                ppp_left_brace (complex_dvalues, dvalue);
                // newline_and_indent(pp, spc + 4);
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                // newline_and_indent(pp, spc + 2);
                ppp_right_brace (complex_dvalues, dvalue);
                // newline_and_indent(pp, spc);
                if (TREE_CODE (node) == TRY_CATCH_EXPR)
                    {
                        node = TREE_OPERAND (node, 1);
                        ppp_string (complex_dvalues, dvalue, "catch");
                    }
                else
                    {
                        gcc_assert (TREE_CODE (node) == TRY_FINALLY_EXPR);
                        node = TREE_OPERAND (node, 1);
                        ppp_string (complex_dvalues, dvalue, "finally");
                        if (TREE_CODE (node) == EH_ELSE_EXPR)
                            {
                                // newline_and_indent(pp, spc + 2);
                                ppp_left_brace (complex_dvalues, dvalue);
                                // newline_and_indent(pp, spc + 4);
                                get_tree_data_values (TREE_OPERAND (node, 0),
                                                      complex_dvalues);
                                // newline_and_indent(pp, spc + 2);
                                ppp_right_brace (complex_dvalues, dvalue);
                                // newline_and_indent(pp, spc);
                                node = TREE_OPERAND (node, 1);
                                ppp_string (complex_dvalues, dvalue, "else");
                            }
                    }
                // newline_and_indent(pp, spc + 2);
                ppp_left_brace (complex_dvalues, dvalue);
                // newline_and_indent(pp, spc + 4);
                get_tree_data_values (node, complex_dvalues);
                // newline_and_indent(pp, spc + 2);
                ppp_right_brace (complex_dvalues, dvalue);
                // is_expr = false;

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case CATCH_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "catch (");
                get_tree_data_values (CATCH_TYPES (node), complex_dvalues);
                ppp_right_paren (complex_dvalues, dvalue);
                // newline_and_indent(pp, spc + 2);
                ppp_left_brace (complex_dvalues, dvalue);
                // newline_and_indent(pp, spc + 4);
                get_tree_data_values (CATCH_BODY (node), complex_dvalues);
                // newline_and_indent(pp, spc + 2);
                ppp_right_brace (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case EH_FILTER_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "<<<eh_filter (");
                get_tree_data_values (EH_FILTER_TYPES (node), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ")>>>");
                // newline_and_indent(pp, spc + 2);
                ppp_left_brace (complex_dvalues, dvalue);
                // newline_and_indent(pp, spc + 4);
                get_tree_data_values (EH_FILTER_FAILURE (node),
                                      complex_dvalues);
                // newline_and_indent(pp, spc + 2);
                ppp_right_brace (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case LABEL_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                tree op0 = TREE_OPERAND (node, 0);
                /* If this is for break or continue, don't bother printing it.
                 */
                if (DECL_NAME (op0))
                    {
                        const char *name = IDENTIFIER_POINTER (DECL_NAME (op0));
                        if (strcmp (name, "break") == 0
                            || strcmp (name, "continue") == 0)
                            {
                                break;
                            }
                    }
                get_tree_data_values (op0, complex_dvalues);
                ppp_colon (complex_dvalues, dvalue);
                if (DECL_NONLOCAL (op0))
                    {
                        ppp_string (complex_dvalues, dvalue, " [non-local]");
                    }

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case LOOP_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "while (1)");

                // newline_and_indent(pp, spc + 2);
                ppp_left_brace (complex_dvalues, dvalue);
                // newline_and_indent(pp, spc + 4);
                get_tree_data_values (LOOP_EXPR_BODY (node), complex_dvalues);
                // newline_and_indent(pp, spc + 2);
                ppp_right_brace (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case PREDICT_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "// predicted ");
                if (PREDICT_EXPR_OUTCOME (node))
                    {
                        ppp_string (complex_dvalues, dvalue, "likely by ");
                    }
                else
                    {
                        ppp_string (complex_dvalues, dvalue, "unlikely by ");
                    }
                ppp_string (complex_dvalues, dvalue,
                            predictor_name (PREDICT_EXPR_PREDICTOR (node)));
                ppp_string (complex_dvalues, dvalue, " predictor.");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case ANNOTATE_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "ANNOTATE_EXPR <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                switch ((enum annot_expr_kind)TREE_INT_CST_LOW (
                    TREE_OPERAND (node, 1)))
                    {
                    case annot_expr_ivdep_kind:
                        ppp_string (complex_dvalues, dvalue, ", ivdep");
                        break;
                    case annot_expr_unroll_kind:
                        ppp_string (
                            complex_dvalues, dvalue,
                            ", unroll "
                                + std::to_string ((int)TREE_INT_CST_LOW (
                                    TREE_OPERAND (node, 2))));
                        break;
                    case annot_expr_no_vector_kind:
                        ppp_string (complex_dvalues, dvalue, ", no-vector");
                        break;
                    case annot_expr_vector_kind:
                        ppp_string (complex_dvalues, dvalue, ", vector");
                        break;
                    case annot_expr_parallel_kind:
                        ppp_string (complex_dvalues, dvalue, ", parallel");
                        break;
                    default:
                        gcc_unreachable ();
                    }
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case RETURN_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "return");
                tree op0 = TREE_OPERAND (node, 0);
                if (op0)
                    {
                        ppp_space (complex_dvalues, dvalue);
                        if (TREE_CODE (op0) == MODIFY_EXPR)
                            {
                                get_tree_data_values (TREE_OPERAND (op0, 1),
                                                      complex_dvalues);
                            }
                        else
                            {
                                get_tree_data_values (op0, complex_dvalues);
                            }
                    }

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case EXIT_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "if (");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ") break");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case SWITCH_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "switch (");
                get_tree_data_values (SWITCH_COND (node), complex_dvalues);
                ppp_right_paren (complex_dvalues, dvalue);

                // newline_and_indent(pp, spc + 2);
                ppp_left_brace (complex_dvalues, dvalue);
                if (SWITCH_BODY (node))
                    {
                        // newline_and_indent(pp, spc + 4);
                        get_tree_data_values (SWITCH_BODY (node),
                                              complex_dvalues);
                    }
                // newline_and_indent(pp, spc + 2);
                ppp_right_brace (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case GOTO_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                tree op0 = GOTO_DESTINATION (node);
                if (TREE_CODE (op0) != SSA_NAME && DECL_P (op0)
                    && DECL_NAME (op0))
                    {
                        const char *name = IDENTIFIER_POINTER (DECL_NAME (op0));
                        if (strcmp (name, "break") == 0
                            || strcmp (name, "continue") == 0)
                            {
                                ppp_string (complex_dvalues, dvalue, name);
                                break;
                            }
                    }
                ppp_string (complex_dvalues, dvalue, "goto ");
                get_tree_data_values (op0, complex_dvalues);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case ASM_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "__asm__");
                if (ASM_VOLATILE_P (node))
                    {
                        ppp_string (complex_dvalues, dvalue, " __volatile__");
                    }
                ppp_left_paren (complex_dvalues, dvalue);
                get_tree_data_values (ASM_STRING (node), complex_dvalues);
                ppp_colon (complex_dvalues, dvalue);
                get_tree_data_values (ASM_OUTPUTS (node), complex_dvalues);
                ppp_colon (complex_dvalues, dvalue);
                get_tree_data_values (ASM_INPUTS (node), complex_dvalues);
                if (ASM_CLOBBERS (node))
                    {
                        ppp_colon (complex_dvalues, dvalue);
                        get_tree_data_values (ASM_CLOBBERS (node),
                                              complex_dvalues);
                    }
                ppp_right_paren (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case CASE_LABEL_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                if (CASE_LOW (node) && CASE_HIGH (node))
                    {
                        ppp_string (complex_dvalues, dvalue, "case ");
                        get_tree_data_values (CASE_LOW (node), complex_dvalues);
                        ppp_string (complex_dvalues, dvalue, " ... ");
                        get_tree_data_values (CASE_HIGH (node),
                                              complex_dvalues);
                    }
                else if (CASE_LOW (node))
                    {
                        ppp_string (complex_dvalues, dvalue, "case ");
                        get_tree_data_values (CASE_LOW (node), complex_dvalues);
                    }
                else
                    {
                        ppp_string (complex_dvalues, dvalue, "default");
                    }
                ppp_colon (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case OBJ_TYPE_REF:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "OBJ_TYPE_REF(");
                get_tree_data_values (OBJ_TYPE_REF_EXPR (node),
                                      complex_dvalues);
                ppp_semicolon (complex_dvalues, dvalue);
                /* We omit the class type for -fcompare-debug because we may
                   drop TYPE_BINFO early depending on debug info, and then
                   virtual_method_call_p would return false, whereas when
                   TYPE_BINFO is preserved it may still return true and then
                   we'd print the class type.  Compare tree and rtl dumps for
                   libstdc++-prettyprinters/shared_ptr.cc with and without -g,
                   for example, at occurrences of OBJ_TYPE_REF.  */
                if (virtual_method_call_p (node))
                    {
                        ppp_string (complex_dvalues, dvalue, "(");
                        get_tree_data_values (obj_type_ref_class (node),
                                              complex_dvalues);
                        ppp_string (complex_dvalues, dvalue, ")");
                    }
                get_tree_data_values (OBJ_TYPE_REF_OBJECT (node),
                                      complex_dvalues);
                ppp_arrow (complex_dvalues, dvalue);
                get_tree_data_values (OBJ_TYPE_REF_TOKEN (node),
                                      complex_dvalues);
                ppp_right_paren (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case SSA_NAME:
            {
                std::vector<data_value_t> complex_dvalues;

                if (SSA_NAME_IDENTIFIER (node))
                    {
                        if (SSA_NAME_VAR (node)
                            && DECL_NAMELESS (SSA_NAME_VAR (node)))
                            {
                                dump_fancy_name (complex_dvalues, dvalue,
                                                 SSA_NAME_IDENTIFIER (node));
                            }

                        else if (SSA_NAME_VAR (node))
                            {
                                get_tree_data_values (
                                    SSA_NAME_IDENTIFIER (node),
                                    complex_dvalues);
                            }
                    }
                ppp_underscore (complex_dvalues, dvalue);
                ppp_decimal_int (complex_dvalues, dvalue,
                                 SSA_NAME_VERSION (node));

                if (SSA_NAME_IS_DEFAULT_DEF (node))
                    {
                        ppp_string (complex_dvalues, dvalue, "(D)");
                    }
                if (SSA_NAME_OCCURS_IN_ABNORMAL_PHI (node))
                    {
                        ppp_string (complex_dvalues, dvalue, "(ab)");
                    }

                // std::cout << complex_dvalues.size() << std::endl;
                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case WITH_SIZE_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "WITH_SIZE_EXPR <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        #ifdef GCC_VERSION_13_OR_LATER
        // ASSERT_EXPR_VAR, ASSERT_EXPR_COND not declare in 13.*
        #else
        case ASSERT_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "ASSERT_EXPR <");
                get_tree_data_values (ASSERT_EXPR_VAR (node), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (ASSERT_EXPR_COND (node), complex_dvalues);

                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }
        #endif

        case SCEV_KNOWN:
            {
                ppp_string (dvalues, dvalue, "scev_known");
                break;
            }

        case SCEV_NOT_KNOWN:
            {
                ppp_string (dvalues, dvalue, "scev_not_known");
                break;
            }

        case POLYNOMIAL_CHREC:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_left_brace (complex_dvalues, dvalue);
                get_tree_data_values (CHREC_LEFT (node), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", +, ");
                get_tree_data_values (CHREC_RIGHT (node), complex_dvalues);
                ppp_string (complex_dvalues, dvalue,
                            "}_" + std::to_string (CHREC_VARIABLE (node)));

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case REALIGN_LOAD_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, "REALIGN_LOAD <");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 2), complex_dvalues);
                ppp_greater (complex_dvalues, dvalue);

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case VEC_COND_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, " VEC_COND_EXPR < ");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, " , ");
                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, " , ");
                get_tree_data_values (TREE_OPERAND (node, 2), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, " > ");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case VEC_PERM_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, " VEC_PERM_EXPR < ");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, " , ");
                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, " , ");
                get_tree_data_values (TREE_OPERAND (node, 2), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, " > ");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case DOT_PROD_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue, " DOT_PROD_EXPR < ");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 2), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, " > ");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case WIDEN_MULT_PLUS_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue,
                            " WIDEN_MULT_PLUS_EXPR < ");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 2), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, " > ");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        case WIDEN_MULT_MINUS_EXPR:
            {
                std::vector<data_value_t> complex_dvalues;

                ppp_string (complex_dvalues, dvalue,
                            " WIDEN_MULT_MINUS_EXPR < ");
                get_tree_data_values (TREE_OPERAND (node, 0), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 1), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, ", ");
                get_tree_data_values (TREE_OPERAND (node, 2), complex_dvalues);
                ppp_string (complex_dvalues, dvalue, " > ");

                append_complex_value (dvalues, dvalue, complex_dvalues);
                break;
            }

        default:
            {
                append_simple_value (dvalues, dvalue,
                                     "<<< Unknown tree: " + dvalue.code_name
                                         + " >>");
                break;
            }
        }
}

void
get_gimple_mem_ops (const gimple *gs, gimple_stmt_data &stmt_data)
{
    tree vdef = gimple_vdef (gs);
    tree vuse = gimple_vuse (gs);

    if (vdef != NULL_TREE)
        {
            std::vector<data_value_t> dvalues;

            data_value_t dvalue;
            get_basic_tree_node_info (vdef, dvalue);

            ppp_string (dvalues, dvalue, "# ");

            std::vector<data_value_t> vdef_complex_dvalues;
            get_tree_data_values (vdef, vdef_complex_dvalues);
            append_complex_value (dvalues, dvalue, vdef_complex_dvalues);

            ppp_string (dvalues, dvalue, " = VDEF <");

            data_value_t dvalue_vuse;
            get_basic_tree_node_info (vdef, dvalue_vuse);

            std::vector<data_value_t> vuse_complex_dvalues;
            get_tree_data_values (vuse, vuse_complex_dvalues);
            append_complex_value (dvalues, dvalue_vuse, vuse_complex_dvalues);

            ppp_greater (dvalues, dvalue);

            tree_value_t tvalue{ .values = dvalues };
            stmt_data.vdef_value = tvalue;
        }

    else if (vuse != NULL_TREE)
        {
            std::vector<data_value_t> dvalues;

            data_value_t dvalue;
            get_basic_tree_node_info (vuse, dvalue);

            ppp_string (dvalues, dvalue, "# VUSE <");

            std::vector<data_value_t> vuse_complex_dvalues;
            get_tree_data_values (vuse, vuse_complex_dvalues);
            append_complex_value (dvalues, dvalue, vuse_complex_dvalues);

            ppp_greater (dvalues, dvalue);

            tree_value_t tvalue{ .values = dvalues };
            stmt_data.vuse_value = tvalue;
        }
}

static void
dump_gimple_call_args (const gcall *gs, gimple_stmt_data &stmt_data)
{
    size_t i = 0;

    /* Pretty print first arg to certain internal fns.  */
    if (gimple_call_internal_p (gs))
        {
            const char *const *enums = NULL;
            unsigned limit = 0;

            switch (gimple_call_internal_fn (gs))
                {
                case IFN_UNIQUE:
#define DEF(X) #X
                    static const char *const unique_args[]
                        = { IFN_UNIQUE_CODES };
#undef DEF
                    enums = unique_args;

                    limit = ARRAY_SIZE (unique_args);
                    break;

                case IFN_GOACC_LOOP:
#define DEF(X) #X
                    static const char *const loop_args[]
                        = { IFN_GOACC_LOOP_CODES };
#undef DEF
                    enums = loop_args;
                    limit = ARRAY_SIZE (loop_args);
                    break;

                case IFN_GOACC_REDUCTION:
#define DEF(X) #X
                    static const char *const reduction_args[]
                        = { IFN_GOACC_REDUCTION_CODES };
#undef DEF
                    enums = reduction_args;
                    limit = ARRAY_SIZE (reduction_args);
                    break;

                case IFN_ASAN_MARK:
#define DEF(X) #X
                    static const char *const asan_mark_args[]
                        = { IFN_ASAN_MARK_FLAGS };
#undef DEF
                    enums = asan_mark_args;
                    limit = ARRAY_SIZE (asan_mark_args);
                    break;

                default:
                    break;
                }
            if (limit)
                {
                    tree arg0 = gimple_call_arg (gs, 0);
                    HOST_WIDE_INT v;

                    if (TREE_CODE (arg0) == INTEGER_CST
                        && tree_fits_shwi_p (arg0)
                        && (v = tree_to_shwi (arg0)) >= 0 && v < limit)
                        {
                            i++;

                            tree_value_t tvalue;
                            data_value_t dvalue;
                            std::vector<data_value_t> dvalues;

                            get_basic_tree_node_info (arg0, dvalue);
                            append_simple_value (dvalues, dvalue, enums[v]);

                            dvalues.push_back (dvalue);
                            tvalue.values = dvalues;
                            stmt_data.gcall_args.push_back (tvalue);

                            // pp_string(buffer, enums[v]);
                        }
                }
        }

    for (; i < gimple_call_num_args (gs); i++)
        {
            // if (i) pp_string(buffer, ", ");

            tree_value_t tvalue;
            std::vector<data_value_t> dvalues;

            get_tree_data_values (gimple_call_arg (gs, i), dvalues);

            tvalue.values = dvalues;
            stmt_data.gcall_args.push_back (tvalue);
        }

    if (gimple_call_va_arg_pack_p ((gcall*)gs))
        {
            // pp_string(buffer, "__builtin_va_arg_pack ()");
        }
}

void
gimple_tuple_arg_values (gimple *g, gimple_stmt_data &stmt_data)
{
    switch (gimple_code (g))
        {
        case GIMPLE_ASM:
            {
                const gasm *gs = reinterpret_cast<const gasm *> (g);
                dump_gimple_asm (gs, stmt_data);
                break;
            }

        case GIMPLE_ASSIGN:
            {
                const gassign *gs = reinterpret_cast<const gassign *> (g);
                dump_gimple_assign (gs, stmt_data);
                break;
            }

        case GIMPLE_BIND:
            {
                const gbind *gs = reinterpret_cast<const gbind *> (g);
                dump_gimple_bind (gs, stmt_data);
                break;
            }

        case GIMPLE_CALL:
            {
                const gcall *gs = reinterpret_cast<const gcall *> (g);
                dump_gimple_call (gs, stmt_data);
                break;
            }

        case GIMPLE_COND:
            {
                const gcond *gs = reinterpret_cast<const gcond *> (g);
                dump_gimple_cond (gs, stmt_data);
                break;
            }

        case GIMPLE_LABEL:
            {
                const glabel *gs = reinterpret_cast<const glabel *> (g);
                dump_gimple_label (gs, stmt_data);
                break;
            }

        case GIMPLE_GOTO:
            {
                const ggoto *gs = reinterpret_cast<const ggoto *> (g);
                dump_gimple_goto (gs, stmt_data);
                break;
            }

        case GIMPLE_NOP:
            {
                stmt_data.gnop_nop_str = "GIMPLE_NOP";
                break;
            }

        case GIMPLE_RETURN:
            {
                const greturn *gs = reinterpret_cast<const greturn *> (g);
                dump_gimple_return (gs, stmt_data);
                break;
            }

        case GIMPLE_SWITCH:
            {
                const gswitch *gs = reinterpret_cast<const gswitch *> (g);
                dump_gimple_switch (gs, stmt_data);
                break;
            }

        case GIMPLE_TRY:
            {
                const gtry *gs = reinterpret_cast<const gtry *> (g);
                dump_gimple_try (gs, stmt_data);
                break;
            }

        case GIMPLE_PHI:
            {
                const gphi *gs = reinterpret_cast<const gphi *> (g);
                dump_gimple_phi (gs, stmt_data);
                break;
            }

        default:
            break;
            // GIMPLE_NIY;
        }
}

void
dump_gimple_asm (const gasm *gs, gimple_stmt_data &stmt_data)
{
    unsigned int i, n;
    stmt_data.gasm_string_code = gimple_asm_string (gs);

    if (gimple_asm_volatile_p (gs))
        {
            // pp_string("__volatile__");
            stmt_data.gasm_volatile = true;
        }

    if (gimple_asm_inline_p (gs))
        {
            // pp_string(" __inline__");
            stmt_data.gasm_inline = true;
        }

    // Return the number of output operands for GIMPLE_ASM G
    n = gimple_asm_noutputs (gs);
    if (n)
        {
            for (i = 0; i < n; i++)
                {
                    tree_value_t tvalue;
                    std::vector<data_value_t> dvalues;

                    get_tree_data_values (gimple_asm_output_op (gs, i),
                                          dvalues);

                    tvalue.values = dvalues;
                    stmt_data.gasm_output_operands.push_back (tvalue);
                }
        }

    // Return the number of input operands for GIMPLE_ASM G.
    n = gimple_asm_ninputs (gs);
    if (n)
        {
            for (i = 0; i < n; i++)
                {
                    tree_value_t tvalue;
                    std::vector<data_value_t> dvalues;

                    get_tree_data_values (gimple_asm_input_op (gs, i), dvalues);

                    tvalue.values = dvalues;
                    stmt_data.gasm_input_operands.push_back (tvalue);
                }
        }

    // Return the number of clobber operands for GIMPLE_ASM G.
    n = gimple_asm_nclobbers (gs);
    if (n)
        {
            for (i = 0; i < n; i++)
                {
                    tree_value_t tvalue;
                    std::vector<data_value_t> dvalues;

                    get_tree_data_values (gimple_asm_clobber_op (gs, i),
                                          dvalues);

                    tvalue.values = dvalues;
                    stmt_data.gasm_clobber_operands.push_back (tvalue);
                }
        }

    n = gimple_asm_nlabels (gs);
    if (n)
        {
            for (i = 0; i < n; i++)
                {
                    tree_value_t tvalue;
                    std::vector<data_value_t> dvalues;

                    get_tree_data_values (gimple_asm_label_op (gs, i), dvalues);

                    tvalue.values = dvalues;
                    stmt_data.gasm_labels.push_back (tvalue);
                }
        }
}

void
dump_gimple_assign (const gassign *gs, gimple_stmt_data &stmt_data)
{
    tree arg1 = NULL;
    tree arg2 = NULL;
    tree arg3 = NULL;

    switch (gimple_num_ops (gs))
        {
        case 4:
            arg3 = gimple_assign_rhs3 (gs);
            /* FALLTHRU */
        case 3:
            arg2 = gimple_assign_rhs2 (gs);
            /* FALLTHRU */
        case 2:
            arg1 = gimple_assign_rhs1 (gs);
            break;
        default:
            gcc_unreachable ();
        }

    stmt_data.gassign_subcode
        = get_tree_code_name (gimple_assign_rhs_code (gs));

    if (arg1 != NULL)
        {
            stmt_data.gassign_has_rhs_arg1 = true;

            tree_value_t tvalue;
            std::vector<data_value_t> dvalues;

            get_tree_data_values (arg1, dvalues);

            tvalue.values = dvalues;
            stmt_data.gassign_rhs_arg1 = tvalue;
        }

    if (arg2 != NULL)
        {
            stmt_data.gassign_has_rhs_arg2 = true;

            tree_value_t tvalue;
            std::vector<data_value_t> dvalues;

            get_tree_data_values (arg2, dvalues);

            tvalue.values = dvalues;
            stmt_data.gassign_rhs_arg2 = tvalue;
        }

    if (arg3 != NULL)
        {
            stmt_data.gassign_has_rhs_arg2 = true;

            tree_value_t tvalue;
            std::vector<data_value_t> dvalues;

            get_tree_data_values (arg3, dvalues);

            tvalue.values = dvalues;
            stmt_data.gassign_rhs_arg3 = tvalue;
        }

    {
        tree_value_t tvalue;
        std::vector<data_value_t> dvalues;

        get_tree_data_values (gimple_assign_lhs (gs), dvalues);

        tvalue.values = dvalues;
        stmt_data.gassign_lhs_arg = tvalue;
    }
}

void
dump_gimple_bind (const gbind *gs, gimple_stmt_data &stmt_data)
{
    tree var;

    for (var = gimple_bind_vars (gs); var; var = DECL_CHAIN (var))
        {
            tree_value_t tvalue;
            data_value_t dvalue;
            std::vector<data_value_t> dvalues;

            get_basic_tree_node_info (var, dvalue);
            print_declaration (var, dvalues, dvalue);

            tvalue.values = dvalues;
            stmt_data.gbind_bind_vars.push_back (tvalue);
        }

    gimple_seq seq = gimple_bind_body ((gbind*)gs);
    gimple_stmt_iterator i;

    for (i = gsi_start (seq); !gsi_end_p (i); gsi_next (&i))
        {

            gimple_stmt_data stmt_data_tmp = gimple_tuple_to_stmt_data (
                (gimple *)gs, stmt_data.basic_block_index,
                stmt_data.basic_block_edges);
            stmt_data.gbind_bind_body.push_back (stmt_data_tmp);
        }
}

void
dump_gimple_call (const gcall *gs, gimple_stmt_data &stmt_data)
{
    tree lhs = gimple_call_lhs (gs);
    tree fn = gimple_call_fn (gs);

    if (gimple_call_internal_p (gs))
        {
            stmt_data.gcall_isinternal_only_function = true;
            stmt_data.gcall_internal_function_name
                = internal_fn_name (gimple_call_internal_fn (gs));
        }

    if (fn)
        {
            tree_value_t tvalue;
            std::vector<data_value_t> dvalues;

            get_tree_data_values (fn, dvalues);

            tvalue.values = dvalues;
            stmt_data.gcall_fn = tvalue;
        }

    if (lhs)
        {
            stmt_data.gcall_has_lhs = true;

            tree_value_t tvalue;
            std::vector<data_value_t> dvalues;

            get_tree_data_values (lhs, dvalues);

            tvalue.values = dvalues;
            stmt_data.gcall_lhs_arg = tvalue;
        }

    stmt_data.gcall_call_num_of_args = gimple_call_num_args (gs);
    if (stmt_data.gcall_call_num_of_args > 0)
        {
            dump_gimple_call_args (gs, stmt_data);
        }

    if (gimple_call_chain (gs))
        {
            stmt_data.gcall_has_static_chain_for_call_statement = true;

            tree_value_t tvalue;
            std::vector<data_value_t> dvalues;

            get_tree_data_values (gimple_call_chain (gs), dvalues);

            tvalue.values = dvalues;
            stmt_data.gcall_static_chain_for_call_statement = tvalue;
        }

    if (gimple_call_return_slot_opt_p ((gcall*)gs))
        {
            stmt_data.gcall_is_marked_for_return_slot_optimization = true;
        }

    if (gimple_call_tail_p ((gcall*)gs))
        {
            stmt_data.gcall_is_marked_as_a_tail_call = true;
        }

    if (gimple_call_must_tail_p (gs))
        {
            stmt_data.gcall_is_marked_as_requiring_tail_call_optimization
                = true;
        }

    if (fn == NULL)
        return;

    /* Dump the arguments of _ITM_beginTransaction sanely.  */
    if (TREE_CODE (fn) == ADDR_EXPR)
        {
            fn = TREE_OPERAND (fn, 0);
        }

    if (TREE_CODE (fn) == FUNCTION_DECL && decl_is_tm_clone (fn))
        {
            // pp_string(buffer, " [tm-clone]");
            stmt_data.gcall_is_tm_clone = true;
        }

    if (TREE_CODE (fn) == FUNCTION_DECL
        && fndecl_built_in_p (fn, BUILT_IN_TM_START)
        && gimple_call_num_args (gs) > 0)
        {
            tree t = gimple_call_arg (gs, 0);
            unsigned HOST_WIDE_INT props;
            gcc_assert (TREE_CODE (t) == INTEGER_CST);

            /* Get the transaction code properties.  */
            props = TREE_INT_CST_LOW (t);

            if (props & PR_INSTRUMENTEDCODE)
                {
                    stmt_data.gcall_transaction_code_properties.push_back (
                        "instrumentedCode");
                }
            if (props & PR_UNINSTRUMENTEDCODE)
                {
                    stmt_data.gcall_transaction_code_properties.push_back (
                        "uninstrumentedCode");
                }
            if (props & PR_HASNOXMMUPDATE)
                {
                    stmt_data.gcall_transaction_code_properties.push_back (
                        "hasNoXMMUpdate");
                }
            if (props & PR_HASNOABORT)
                {
                    stmt_data.gcall_transaction_code_properties.push_back (
                        "hasNoAbort");
                }
            if (props & PR_HASNOIRREVOCABLE)
                {
                    stmt_data.gcall_transaction_code_properties.push_back (
                        "hasNoIrrevocable");
                }
            if (props & PR_DOESGOIRREVOCABLE)
                {
                    stmt_data.gcall_transaction_code_properties.push_back (
                        "doesGoIrrevocable");
                }
            if (props & PR_HASNOSIMPLEREADS)
                {
                    stmt_data.gcall_transaction_code_properties.push_back (
                        "hasNoSimpleReads");
                }
            if (props & PR_AWBARRIERSOMITTED)
                {
                    stmt_data.gcall_transaction_code_properties.push_back (
                        "awBarriersOmitted");
                }
            if (props & PR_RARBARRIERSOMITTED)
                {
                    stmt_data.gcall_transaction_code_properties.push_back (
                        "RaRBarriersOmitted");
                }
            if (props & PR_UNDOLOGCODE)
                {
                    stmt_data.gcall_transaction_code_properties.push_back (
                        "undoLogCode");
                }
            if (props & PR_PREFERUNINSTRUMENTED)
                {
                    stmt_data.gcall_transaction_code_properties.push_back (
                        "preferUninstrumented");
                }
            if (props & PR_EXCEPTIONBLOCK)
                {
                    stmt_data.gcall_transaction_code_properties.push_back (
                        "exceptionBlock");
                }
            if (props & PR_HASELSE)
                {
                    stmt_data.gcall_transaction_code_properties.push_back (
                        "hasElse");
                }
            if (props & PR_READONLY)
                {
                    stmt_data.gcall_transaction_code_properties.push_back (
                        "readOnly");
                }
        }

    // if (flags & TDF_ALIAS) {
    //     const pt_solution *pt;
    //     pt = gimple_call_use_set(gs);
    //     if (!pt_solution_empty_p(pt)) {
    //         pp_string(buffer, "# USE = ");
    //         pp_points_to_solution(buffer, pt);
    //         newline_and_indent(buffer, spc);
    //     }
    //     pt = gimple_call_clobber_set(gs);
    //     if (!pt_solution_empty_p(pt)) {
    //         pp_string(buffer, "# CLB = ");
    //         pp_points_to_solution(buffer, pt);
    //         newline_and_indent(buffer, spc);
    //     }
    // }

    // if (flags & TDF_RAW) {
    //     if (gimple_call_internal_p(gs)) {
    //         dump_gimple_fmt(buffer, spc, flags, "%G <.%s, %T", gs,
    //                         internal_fn_name(gimple_call_internal_fn(gs)),
    //                         lhs);
    //     }
    //     else {
    //         dump_gimple_fmt(buffer, spc, flags, "%G <%T, %T", gs, fn, lhs);
    //     }

    //     if (gimple_call_num_args(gs) > 0) {
    //         pp_string(buffer, ", ");
    //         dump_gimple_call_args(buffer, gs, flags);
    //     }
    //     pp_greater(buffer);
    // } else {
    //     if (lhs && !(flags & TDF_RHS_ONLY)) {
    //         dump_generic_node(buffer, lhs, spc, flags, false);
    //         pp_string(buffer, " =");

    //         if (gimple_has_volatile_ops(gs)) pp_string(buffer, "{v}");

    //         pp_space(buffer);
    //     }
    //     if (gimple_call_internal_p(gs)) {
    //         pp_dot(buffer);
    //         pp_string(buffer, internal_fn_name(gimple_call_internal_fn(gs)));
    //     } else
    //         print_call_name(buffer, fn, flags);
    //     pp_string(buffer, " (");
    //     dump_gimple_call_args(buffer, gs, flags);
    //     pp_right_paren(buffer);
    //     if (!(flags & TDF_RHS_ONLY)) pp_semicolon(buffer);
    // }

    // if (gimple_call_chain(gs)) {
    //     pp_string(buffer, " [static-chain: ");
    //     dump_generic_node(buffer, gimple_call_chain(gs), spc, flags, false);
    //     pp_right_bracket(buffer);
    // }

    // if (gimple_call_return_slot_opt_p(gs))
    //     pp_string(buffer, " [return slot optimization]");
    // if (gimple_call_tail_p(gs)) pp_string(buffer, " [tail call]");
    // if (gimple_call_must_tail_p(gs)) pp_string(buffer, " [must tail call]");

    // if (fn == NULL) return;

    // /* Dump the arguments of _ITM_beginTransaction sanely.  */
    // if (TREE_CODE(fn) == ADDR_EXPR) {
    //  fn = TREE_OPERAND(fn, 0);
    // }

    // if (TREE_CODE(fn) == FUNCTION_DECL && decl_is_tm_clone(fn)) {
    //     pp_string(buffer, " [tm-clone]");
    // }

    // if (TREE_CODE(fn) == FUNCTION_DECL && fndecl_built_in_p(fn,
    // BUILT_IN_TM_START) && gimple_call_num_args(gs) > 0) {
    //     tree t = gimple_call_arg(gs, 0);
    //     unsigned HOST_WIDE_INT props;
    //     gcc_assert(TREE_CODE(t) == INTEGER_CST);

    //     pp_string(buffer, " [ ");

    //     /* Get the transaction code properties.  */
    //     props = TREE_INT_CST_LOW(t);

    //     if (props & PR_INSTRUMENTEDCODE) pp_string(buffer, "instrumentedCode
    //     "); if (props & PR_UNINSTRUMENTEDCODE)
    //         pp_string(buffer, "uninstrumentedCode ");
    //     if (props & PR_HASNOXMMUPDATE) pp_string(buffer, "hasNoXMMUpdate ");
    //     if (props & PR_HASNOABORT) pp_string(buffer, "hasNoAbort ");
    //     if (props & PR_HASNOIRREVOCABLE) pp_string(buffer, "hasNoIrrevocable
    //     "); if (props & PR_DOESGOIRREVOCABLE)
    //         pp_string(buffer, "doesGoIrrevocable ");
    //     if (props & PR_HASNOSIMPLEREADS) pp_string(buffer, "hasNoSimpleReads
    //     "); if (props & PR_AWBARRIERSOMITTED)
    //         pp_string(buffer, "awBarriersOmitted ");
    //     if (props & PR_RARBARRIERSOMITTED)
    //         pp_string(buffer, "RaRBarriersOmitted ");
    //     if (props & PR_UNDOLOGCODE) pp_string(buffer, "undoLogCode ");
    //     if (props & PR_PREFERUNINSTRUMENTED)
    //         pp_string(buffer, "preferUninstrumented ");
    //     if (props & PR_EXCEPTIONBLOCK) pp_string(buffer, "exceptionBlock ");
    //     if (props & PR_HASELSE) pp_string(buffer, "hasElse ");
    //     if (props & PR_READONLY) pp_string(buffer, "readOnly ");

    //     pp_right_bracket(buffer);
}

void
dump_gimple_cond (const gcond *gs, gimple_stmt_data &stmt_data)
{
    stmt_data.gcond_tree_code_name = get_tree_code_name (gimple_cond_code (gs));

    {
        tree_value_t tvalue;
        std::vector<data_value_t> dvalues;

        get_tree_data_values (gimple_cond_lhs (gs), dvalues);

        tvalue.values = dvalues;
        stmt_data.gcond_lhs = tvalue;
    }

    {
        tree_value_t tvalue;
        std::vector<data_value_t> dvalues;

        get_tree_data_values (gimple_cond_rhs (gs), dvalues);

        tvalue.values = dvalues;
        stmt_data.gcond_rhs = tvalue;
    }

    if (gimple_cond_true_label (gs))
        {
            stmt_data.gcond_has_true_goto_label = true;

            tree_value_t tvalue;
            std::vector<data_value_t> dvalues;

            get_tree_data_values (gimple_cond_true_label (gs), dvalues);

            tvalue.values = dvalues;
            stmt_data.gcond_true_goto_label = tvalue;
        }

    if (gimple_cond_false_label (gs))
        {
            stmt_data.gcond_has_false_else_goto_label = true;

            tree_value_t tvalue;
            std::vector<data_value_t> dvalues;

            get_tree_data_values (gimple_cond_false_label (gs), dvalues);

            tvalue.values = dvalues;
            stmt_data.gcond_false_else_goto_label = tvalue;
        }

    edge_iterator ei;
    edge e, true_edge = NULL, false_edge = NULL;
    basic_block bb = gimple_bb (gs);

    if (bb)
        {
            FOR_EACH_EDGE (e, ei, bb->succs)
            {
                if (e->flags & EDGE_TRUE_VALUE)
                    true_edge = e;
                else if (e->flags & EDGE_FALSE_VALUE)
                    false_edge = e;
            }

            if (true_edge != NULL)
                {
                    stmt_data.gcond_has_goto_true_edge = true;
                    basic_block tmp_bb = true_edge->dest;
                    stmt_data.goto_true_edge = tmp_bb->index;
                }

            if (false_edge != NULL)
                {
                    stmt_data.gcond_has_else_goto_false_edge = true;
                    basic_block tmp_bb = false_edge->dest;
                    stmt_data.else_goto_false_edge = tmp_bb->index;
                }
        }
}

void
dump_gimple_label (const glabel *gs, gimple_stmt_data &stmt_data)
{
    tree label = gimple_label_label (gs);

    {
        tree_value_t tvalue;
        std::vector<data_value_t> dvalues;

        get_tree_data_values (label, dvalues);

        tvalue.values = dvalues;
        stmt_data.glabel_label = tvalue;
    }

    if (DECL_NONLOCAL (label))
        {
            stmt_data.glabel_is_non_local = true;
        }
}

void
dump_gimple_goto (const ggoto *gs, gimple_stmt_data &stmt_data)
{
    tree label = gimple_goto_dest (gs);

    {
        tree_value_t tvalue;
        std::vector<data_value_t> dvalues;

        get_tree_data_values (label, dvalues);

        tvalue.values = dvalues;
        stmt_data.ggoto_dest_goto_label = tvalue;
    }
}

void
dump_gimple_return (const greturn *gs, gimple_stmt_data &stmt_data)
{
    tree t;
    t = gimple_return_retval (gs);

    if (t)
        {
            stmt_data.greturn_has_greturn_return_value = true;

            tree_value_t tvalue;
            std::vector<data_value_t> dvalues;

            get_tree_data_values (t, dvalues);

            tvalue.values = dvalues;
            stmt_data.greturn_return_value = tvalue;
        }
}

void
dump_gimple_switch (const gswitch *gs, gimple_stmt_data &stmt_data)
{
    unsigned int i;
    GIMPLE_CHECK (gs, GIMPLE_SWITCH);

    {
        tree_value_t tvalue;
        std::vector<data_value_t> dvalues;

        get_tree_data_values (gimple_switch_index (gs), dvalues);

        tvalue.values = dvalues;
        stmt_data.gswitch_switch_index = tvalue;
    }

    for (i = 0; i < gimple_switch_num_labels (gs); i++)
        {
            tree case_label = gimple_switch_label (gs, i);
            {
                tree_value_t tvalue;
                std::vector<data_value_t> dvalues;

                get_tree_data_values (case_label, dvalues);

                tvalue.values = dvalues;
                stmt_data.gswitch_switch_case_labels.push_back (tvalue);
            }

            tree label = CASE_LABEL (case_label);
            {
                tree_value_t tvalue;
                std::vector<data_value_t> dvalues;

                get_tree_data_values (label, dvalues);

                tvalue.values = dvalues;
                stmt_data.gswitch_switch_labels.push_back (tvalue);
            }
        }
}

void
dump_gimple_try (const gtry *gs, gimple_stmt_data &stmt_data)
{
    if (gimple_try_kind (gs) == GIMPLE_TRY_CATCH)
        {
            stmt_data.gtry_try_type_kind = "GIMPLE_TRY_CATCH";
        }

    if (gimple_try_kind (gs) == GIMPLE_TRY_FINALLY)
        {
            stmt_data.gtry_try_type_kind = "GIMPLE_TRY_FINALLY";
        }

    {
        gimple_seq seq = gimple_try_eval ((gimple*)gs);
        gimple_stmt_iterator i;

        for (i = gsi_start (seq); !gsi_end_p (i); gsi_next (&i))
            {
                gimple *gs = gsi_stmt (i);
                gimple_stmt_data stmt_data_tmp = gimple_tuple_to_stmt_data (
                    gs, stmt_data.basic_block_index,
                    stmt_data.basic_block_edges);
                stmt_data.gtry_try_eval.push_back (stmt_data_tmp);
            }
    }

    gimple_seq seq = gimple_try_cleanup ((gimple*)gs);
    if (seq)
        {
            stmt_data.gtry_has_try_cleanup = true;
            gimple_stmt_iterator i;

            for (i = gsi_start (seq); !gsi_end_p (i); gsi_next (&i))
                {
                    gimple *gs = gsi_stmt (i);
                    gimple_stmt_data stmt_data_tmp = gimple_tuple_to_stmt_data (
                        gs, stmt_data.basic_block_index,
                        stmt_data.basic_block_edges);
                    stmt_data.gtry_try_cleanup.push_back (stmt_data_tmp);
                }
        }
}

void
dump_gimple_phi (const gphi *phi, gimple_stmt_data &stmt_data)
{
    size_t i;
    tree lhs = gimple_phi_result (phi);

    {
        tree_value_t tvalue;
        std::vector<data_value_t> dvalues;

        get_tree_data_values (lhs, dvalues);

        tvalue.values = dvalues;
        stmt_data.gphi_lhs = tvalue;
    }

    for (i = 0; i < gimple_phi_num_args (phi); i++)
        {
            tree_value_t tvalue;
            std::vector<data_value_t> dvalues;

            get_tree_data_values (gimple_phi_arg_def ((gphi*)phi, i), dvalues);

            tvalue.values = dvalues;
            stmt_data.gphi_phi_args.push_back (tvalue);

            basic_block src = gimple_phi_arg_edge ((gphi*)phi, i)->src;
            stmt_data.gphi_phi_args_basicblock_src_index.push_back (src->index);

            if (gimple_phi_arg_has_location ((gphi*)phi, i))
                {
                    expanded_location xloc
                        = expand_location (gimple_phi_arg_location ((gphi*)phi, i));
                    std::string loc_str = "";

                    if (xloc.file)
                        loc_str = std::string (xloc.file) + ":"
                                  + std::to_string (xloc.line);

                    stmt_data.gphi_phi_args_locations.push_back (loc_str);
                }
            else
                {
                    stmt_data.gphi_phi_args_locations.push_back ("");
                }
        }
}
