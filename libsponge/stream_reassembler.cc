#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _unassemble_strs()
    , _next_assembled_idx(0)
    , _unassembled_bytes_num(0)
    , _eof_idx(-1)
    , _output(capacity)
    , _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t new_idx, new_end;
    new_idx = index;
    new_end = new_idx + data.size();
    auto iter = _unassemble_strs.lower_bound(index);
    // lower_bound: new_id<=next_idx
    while (iter != _unassemble_strs.end()) {
        size_t next_idx = iter->first;
        size_t next_end = next_idx + iter->second.size();
        if (new_end >= next_end) {
            // 新数据完全覆盖了这一数据段
            _unassembled_bytes_num -= iter->second.size();
            iter = _unassemble_strs.erase(iter);
            continue;
        } else if (new_end > next_idx) {
            // 新数据的后半段被这一数据段覆盖，将其截去
            new_end = next_idx;
        }
        // case: new_end<=next_idx
        break;
    }
    // case: new_idx>last_idx
    if (iter != _unassemble_strs.begin()) {
        iter--;
        size_t last_end = iter->first + iter->second.size();
        if (new_idx < last_end) {
            // 新数据的前半段被这一数据段覆盖，将其截去
            new_idx = last_end;
        }
    }
    // 新数据的前半段已经重组完成，将其截去
    if (_next_assembled_idx > new_idx) {
        new_idx = _next_assembled_idx;
    }
    std::string new_str;
    if (new_idx >= _next_assembled_idx + _capacity - _output.buffer_size()) {
        // 超出窗口
        return;
    }
    if (new_idx < new_end) {
        // 防止之前截没了
        new_str = data.substr(new_idx - index, new_end - new_idx);
        _unassemble_strs.insert(make_pair(new_idx, new_str));
        _unassembled_bytes_num += new_end - new_idx;
    }
    // 从头开始将连续部分输出
    for (iter = _unassemble_strs.begin(); iter != _unassemble_strs.end();) {
        if (iter->first == _next_assembled_idx) {
            std::string str = iter->second;
            size_t wirtten_size = _output.write(str);
            _next_assembled_idx += wirtten_size;
            _unassembled_bytes_num -= wirtten_size;
            if (wirtten_size < str.size()) {
                size_t i = iter->first + wirtten_size;
                std::string s = str.substr(wirtten_size);
                _unassemble_strs.insert(make_pair(i, s));
                iter = _unassemble_strs.erase(iter);
                break;
            } else {
                iter = _unassemble_strs.erase(iter);
            }
        } else {
            break;
        }
    }
    if (eof) {
        // 标记eof，代表是结束数据段
        _eof_idx = index + data.size();
    }
    if (_eof_idx <= _next_assembled_idx) {
        // 结束数据段整流完毕
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes_num; }

bool StreamReassembler::empty() const { return _unassembled_bytes_num == 0; }
