#ifndef H_DATA_FORMATTER_MSGPACK_
#define H_DATA_FORMATTER_MSGPACK_

#include "ext/msgpack11.hpp"
#include "gimple_extractor.h"
using namespace msgpack11;


MsgPack function_data_to_msgpack_object(function_data_t &fn_data);
MsgPack stmts_data_to_msgpack_object(std::vector<gimple_stmt_data> &stmts_data);
MsgPack stmt_data_to_msgpack_object(gimple_stmt_data &stmt_data); 
MsgPack tree_values_to_msgpack_object(std::vector<tree_value_t> &tvalues);
MsgPack tree_value_to_msgpack_object(tree_value_t &tvalue);
MsgPack data_value_to_msgpack_object(data_value_t &dvalue);
MsgPack get_stmt_data_args_msgpack(gimple_stmt_data &stmt_data);
MsgPack gimple_phi_data_to_msgpack_object(gimple_phi_t &phis_data);
MsgPack bb_data_to_msgpack_object(basicblock_t &bb_data);


#endif