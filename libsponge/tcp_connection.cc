#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _curr_time - _last_segment_received_time; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _last_segment_received_time = _curr_time;
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return;
    }
    _receiver.segment_received(seg);
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    if (_receiver.ackno().has_value()) {
        _sender.fill_window();
        if (seg.length_in_sequence_space() && _sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
        send_segment();
    }
    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }
}

bool TCPConnection::active() const {
    if (_sender.stream_in().error() || _receiver.stream_out().error()) {
        return false;
    }
    if (_receiver.stream_out().input_ended() && _sender.stream_in().eof() &&
        _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 && _sender.bytes_in_flight() == 0) {
        if (_linger_after_streams_finish && time_since_last_segment_received() < 10 * _cfg.rt_timeout) {
            return true;
        } else {
            return false;
        }
    }
    return true;
}

size_t TCPConnection::write(const string &data) {
    size_t bytes_written = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segment();
    return bytes_written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    _curr_time += ms_since_last_tick;
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        _sender.send_empty_segment(true);
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
    } else if (_receiver.ackno().has_value()) {
        _sender.fill_window();
    }
    send_segment();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segment();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_segment();
}

void TCPConnection::send_segment() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        seg.header().win = _receiver.window_size();
        _segments_out.push(move(seg));
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            _sender.send_empty_segment(true);
            send_segment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
