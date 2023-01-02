#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _outgoing_bytes; }

void TCPSender::fill_window() {
    size_t win_size = _last_win_size == 0 ? 1 : _last_win_size;
    while (win_size > _outgoing_bytes) {
        TCPSegment seg;
        if (not _set_syn_flag) {
            seg.header().syn = true;
            _set_syn_flag = true;
        }
        seg.header().seqno = next_seqno();
        const size_t payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, win_size - _outgoing_bytes - seg.header().syn);
        string payload = _stream.read(payload_size);
        if (!_set_fin_flag && _stream.eof() && payload.size() + _outgoing_bytes < win_size) {
            _set_fin_flag = true;
            seg.header().fin = true;
        }
        seg.payload() = Buffer(move(payload));
        if (seg.length_in_sequence_space() == 0) {
            break;
        }
        if (_outgoing_queue.empty()) {
            _timeout = _initial_retransmission_timeout;
            _timecount = 0;
        }
        _segments_out.push(seg);
        _outgoing_bytes += seg.length_in_sequence_space();
        _outgoing_queue.push(make_pair(_next_seqno, seg));
        _next_seqno += seg.length_in_sequence_space();
        if (seg.header().fin) {
            break;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    size_t abseq = unwrap(ackno, _isn, _next_seqno);
    if (abseq > _next_seqno)
        return;
    while (not _outgoing_queue.empty()) {
        auto pair = _outgoing_queue.front();
        TCPSegment &seg = pair.second;
        size_t seq = pair.first;
        if (seq + seg.length_in_sequence_space() <= abseq) {
            _outgoing_bytes -= seg.length_in_sequence_space();
            _outgoing_queue.pop();
            _timeout = _initial_retransmission_timeout;
            _timecount = 0;
        } else {
            break;
        }
    }
    _consecutive_retrans_count = 0;
    _last_win_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timecount += ms_since_last_tick;
    if (not _outgoing_queue.empty() && _timecount >= _timeout) {
        auto pair = _outgoing_queue.front();
        if (_last_win_size > 0)
            _timeout *= 2;
        _timecount = 0;
        _segments_out.push(pair.second);
        _outgoing_queue.push(pair);
        _outgoing_queue.pop();
        _consecutive_retrans_count++;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retrans_count; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
