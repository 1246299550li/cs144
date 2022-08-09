#include "byte_stream.hh"

#include <algorithm>
#include <cmath>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _buf(), _capacity(capacity), _size(0), _write_size(0), _read_size(0), _is_end(false), _error(false) {}

size_t ByteStream::write(const string &data) {
    size_t len = min(remaining_capacity(), data.size());
    for (size_t i = 0; i < len; i++) {
        _buf.emplace_back(data[i]);
    }
    _size += len;
    _write_size += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const { return string(_buf.begin(), _buf.begin() + min(len, _size)); }

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t length = min(len, _size);
    for (size_t i = 0; i < length; i++) {
        _buf.pop_front();
    }
    _size -= length;
    _read_size += length;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    const auto ret = peek_output(len);
    pop_output(len);
    return ret;
}

void ByteStream::end_input() { _is_end = true; }

bool ByteStream::input_ended() const { return _is_end; }

size_t ByteStream::buffer_size() const { return _size; }

bool ByteStream::buffer_empty() const { return _size == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _write_size; }

size_t ByteStream::bytes_read() const { return _read_size; }

size_t ByteStream::remaining_capacity() const { return _capacity - _size; }
