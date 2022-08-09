#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _eof(false), _eof_pos(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t first_unassembled = _output.bytes_written();
    size_t first_unaccept = _output.bytes_read() + _capacity;

    if (!_eof && eof) {
        _eof = true;
        _eof_pos = index + data.size();
    }

    if (index + data.size() <= first_unassembled || index > first_unaccept) {
        if (_eof && _output.bytes_written() == _eof_pos) {
            _output.end_input();
        }
        return;
    }

    size_t useful_len = data.size();

    if (index + data.size() > first_unaccept) {
        useful_len = first_unaccept - index;
    }

    // 缓存
    for (size_t i = max(index, first_unassembled); i < index + useful_len; i++) {
        _buf[i] = data[i - index];
    }
    // 写入
    if (index <= first_unassembled) {
        size_t end = first_unassembled;
        string str;
        while (_buf.count(end) == 1) {
            str.append(1, _buf[end]);
            _buf.erase(end);
            end++;
        }
        _output.write(str);
    }

    if (_eof && _output.bytes_written() == _eof_pos) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _buf.size(); }

bool StreamReassembler::empty() const { return unassembled_bytes() == 0 && _output.buffer_empty(); }
