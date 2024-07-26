#ifndef H_DATA_FORMATTER_
#define H_DATA_FORMATTER_

#include "gimple_extractor.h"
#include <string>
#include <vector>

std::string
function_to_string_dump (std::vector<gimple_stmt_data> &stmt_data_list,
                         std::vector<basicblock_t> &basic_block_list,
                         function_data_t &fn_data, std::string data_format);

std::string
function_to_string_dump_json (std::vector<gimple_stmt_data> &stmt_data_list,
                              std::vector<basicblock_t> &basic_block_list,
                              function_data_t &fn_data);

std::string
function_to_string_dump_msgpack (std::vector<gimple_stmt_data> &stmt_data_list,
                                 std::vector<basicblock_t> &basic_block_list,
                                 function_data_t &fn_data);

#endif