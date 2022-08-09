#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    const auto &header = seg.header();
    const auto &data = seg.payload();
    if (!_syn) {
        if (!header.syn) {
            return;
        } else {
            _syn = true;
            _isn = header.seqno;
        }
    }

    if (header.fin) {
        _fin = true;
    }
    // checkpoint
    size_t ckpt = stream_out().bytes_written() + 1;
    uint64_t absolute_seqno = unwrap(header.seqno, _isn, ckpt);
    uint64_t stream_idx = absolute_seqno + static_cast<uint64_t>(header.syn) - 1;
    _reassembler.push_substring(data.copy(), stream_idx, _fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn) {
        return nullopt;
    }
    uint64_t absolute_ackno = stream_out().bytes_written() + 1;
    absolute_ackno += (_fin && _reassembler.unassembled_bytes() == 0) ? 1 : 0;
    return wrap(absolute_ackno, _isn);
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
