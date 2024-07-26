#include "ext/json11.hpp"
#include "ext/msgpack11.hpp"
#include "data_formatter.h"
#include "data_formatter_json.h"
#include "data_formatter_msgpack.h"
using namespace json11;
using namespace msgpack11;


std::string
function_to_string_dump (std::vector<gimple_stmt_data> &stmt_data_list,
                         std::vector<basicblock_t> &basic_block_list,
                         function_data_t &fn_data, std::string data_format)
{
    if (data_format == "json")
        return function_to_string_dump_json (stmt_data_list, basic_block_list,
                                             fn_data);

    if (data_format == "msgpack")
        return function_to_string_dump_msgpack (stmt_data_list, basic_block_list,
                                                fn_data);

    return std::string ("");
}

std::string
function_to_string_dump_json (std::vector<gimple_stmt_data> &stmt_data_list,
                              std::vector<basicblock_t> &basic_block_list,
                              function_data_t &fn_data)
{
    std::vector<Json> gimple_json_list;
    std::vector<Json> basic_block_json_list;

    for (auto &stmt_data : stmt_data_list)
        {
            Json stmt_data_object = stmt_data_to_json_object (stmt_data);
            gimple_json_list.push_back (stmt_data_object);
        }

    for (auto &bb_data : basic_block_list)
        {
            Json bb_data_object = bb_data_to_json_object (bb_data);
            basic_block_json_list.push_back (bb_data_object);
        }

    Json data = Json::object{
        { "function_info", function_data_to_json_object (fn_data) },
        { "gimples", gimple_json_list },
        { "basicblocks", basic_block_json_list },
    };
    return data.dump ();
}

std::string
function_to_string_dump_msgpack (std::vector<gimple_stmt_data> &stmt_data_list,
                                 std::vector<basicblock_t> &basic_block_list,
                                 function_data_t &fn_data)
{
    std::vector<MsgPack> gimple_json_list;
    std::vector<MsgPack> basic_block_json_list;

    for (auto &stmt_data : stmt_data_list)
        {
            MsgPack stmt_data_object = stmt_data_to_msgpack_object (stmt_data);
            gimple_json_list.push_back (stmt_data_object);
        }

    for (auto &bb_data : basic_block_list)
        {
            MsgPack bb_data_object = bb_data_to_msgpack_object (bb_data);
            basic_block_json_list.push_back (bb_data_object);
        }

    MsgPack data = MsgPack::object {
	    { "function_info", function_data_to_msgpack_object (fn_data) },
        { "gimples", gimple_json_list },
        { "basicblocks", basic_block_json_list },
	};
    return data.dump ();
}