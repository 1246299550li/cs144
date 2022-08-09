#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _ms_remaining_time{retx_timeout}
    , _current_retransmission_timeout{retx_timeout} {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    size_t current_window_size = _current_window_size ? _current_window_size : 1;
    while (current_window_size > _bytes_in_flight && !_is_fin_set) {
        TCPSegment segment;
        if (!_is_syn_set) {
            _is_syn_set = true;
            segment.header().syn = true;
        }
        segment.header().seqno = next_seqno();
        size_t max_payload_size =
            min(TCPConfig::MAX_PAYLOAD_SIZE, current_window_size - _bytes_in_flight - segment.header().syn);
        segment.payload() = Buffer(_stream.read(max_payload_size));
        if (!_is_fin_set && _stream.eof() &&
            segment.length_in_sequence_space() + _bytes_in_flight < current_window_size) {
            _is_fin_set = true;
            segment.header().fin = true;
        }
        if (!segment.length_in_sequence_space()) {
            return;
        }
        _segments_out.push(segment);
        _segments_not_ack.push(segment);
        _bytes_in_flight += segment.length_in_sequence_space();
        _next_seqno += segment.length_in_sequence_space();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t absolute_ackno = unwrap(ackno, _isn, _next_seqno);
    if (absolute_ackno > _next_seqno) {
        return;
    }
    _current_window_size = window_size;

    bool is_acked_new_data = false;
    while (!_segments_not_ack.empty()) {
        uint64_t seqno = unwrap(_segments_not_ack.front().header().seqno, _isn, _next_seqno);
        const auto segment_len = _segments_not_ack.front().length_in_sequence_space();
        if (seqno + segment_len > absolute_ackno) {
            break;
        }
        _segments_not_ack.pop();
        _bytes_in_flight -= segment_len;
        is_acked_new_data = true;
    }
    if (is_acked_new_data) {
        _current_retransmission_timeout = _initial_retransmission_timeout;
        _ms_remaining_time = _current_retransmission_timeout;
        _consecutive_retransmission_cnt = 0;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    bool is_need_retransmitted = false;
    if (ms_since_last_tick >= _ms_remaining_time) {
        is_need_retransmitted = true;
    } else {
        _ms_remaining_time -= ms_since_last_tick;
    }
    if (is_need_retransmitted && !_segments_not_ack.empty()) {
        if (_current_window_size > 0) {
            _current_retransmission_timeout <<= 1;
            _consecutive_retransmission_cnt++;
        }
        if (_consecutive_retransmission_cnt <= TCPConfig::MAX_RETX_ATTEMPTS) {
            _segments_out.push(_segments_not_ack.front());
        }
        _ms_remaining_time = _current_retransmission_timeout;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmission_cnt; }

void TCPSender::send_empty_segment(bool isRst) {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    segment.header().rst = isRst;
    _segments_out.push(segment);
}
