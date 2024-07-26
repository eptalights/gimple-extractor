#ifndef H_DATA_FORMATTER_JSON_
#define H_DATA_FORMATTER_JSON_

#include "ext/json11.hpp"
#include "gimple_extractor.h"
using namespace json11;


Json function_data_to_json_object(function_data_t &fn_data);
Json stmts_data_to_json_object(std::vector<gimple_stmt_data> &stmts_data);
Json stmt_data_to_json_object(gimple_stmt_data &stmt_data);
Json tree_values_to_json_object(std::vector<tree_value_t> &tvalues);
Json tree_value_to_json_object(tree_value_t &tvalue);
Json data_value_to_json_object(data_value_t &dvalue);
Json get_stmt_data_args_json(gimple_stmt_data &stmt_data);
Json gimple_phi_data_to_json_object(gimple_phi_t &phis_data);
Json bb_data_to_json_object(basicblock_t &bb_data);


#endif